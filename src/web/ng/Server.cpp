//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2024, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "web/ng/Server.hpp"

#include "util/Assert.hpp"
#include "util/Taggable.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ObjectView.hpp"
#include "web/ng/Connection.hpp"
#include "web/ng/MessageHandler.hpp"
#include "web/ng/ProcessingPolicy.hpp"
#include "web/ng/Response.hpp"
#include "web/ng/impl/HttpConnection.hpp"
#include "web/ng/impl/ServerSslContext.hpp"

#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/error.hpp>
#include <boost/beast/core/detect_ssl.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/system/system_error.hpp>
#include <fmt/core.h>

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace web::ng {

namespace {

std::expected<boost::asio::ip::tcp::endpoint, std::string>
makeEndpoint(util::config::ObjectView const& serverConfig)
{
    auto const ip = serverConfig.get<std::string>("ip");

    boost::system::error_code error;
    auto const address = boost::asio::ip::make_address(ip, error);
    if (error)
        return std::unexpected{fmt::format("Error parsing provided IP: {}", error.message())};

    auto const port = serverConfig.get<unsigned short>("port");
    return boost::asio::ip::tcp::endpoint{address, port};
}

std::expected<boost::asio::ip::tcp::acceptor, std::string>
makeAcceptor(boost::asio::io_context& context, boost::asio::ip::tcp::endpoint const& endpoint)
{
    boost::asio::ip::tcp::acceptor acceptor{context};
    try {
        acceptor.open(endpoint.protocol());
        acceptor.set_option(boost::asio::socket_base::reuse_address(true));
        acceptor.bind(endpoint);
        acceptor.listen(boost::asio::socket_base::max_listen_connections);
    } catch (boost::system::system_error const& error) {
        return std::unexpected{fmt::format("Error creating TCP acceptor: {}", error.what())};
    }
    return acceptor;
}

std::expected<std::string, boost::system::system_error>
extractIp(boost::asio::ip::tcp::socket const& socket)
{
    std::string ip;
    try {
        ip = socket.remote_endpoint().address().to_string();
    } catch (boost::system::system_error const& error) {
        return std::unexpected{error};
    }
    return ip;
}

struct SslDetectionResult {
    boost::asio::ip::tcp::socket socket;
    bool isSsl;
    boost::beast::flat_buffer buffer;
};

std::expected<std::optional<SslDetectionResult>, std::string>
detectSsl(boost::asio::ip::tcp::socket socket, boost::asio::yield_context yield)
{
    boost::beast::tcp_stream tcpStream{std::move(socket)};
    boost::beast::flat_buffer buffer;
    boost::beast::error_code errorCode;
    bool const isSsl = boost::beast::async_detect_ssl(tcpStream, buffer, yield[errorCode]);

    if (errorCode == boost::asio::ssl::error::stream_truncated)
        return std::nullopt;

    if (errorCode)
        return std::unexpected{fmt::format("Detector failed (detect): {}", errorCode.message())};

    return SslDetectionResult{.socket = tcpStream.release_socket(), .isSsl = isSsl, .buffer = std::move(buffer)};
}

std::expected<impl::UpgradableConnectionPtr, std::optional<std::string>>
makeConnection(
    SslDetectionResult sslDetectionResult,
    std::optional<boost::asio::ssl::context>& sslContext,
    std::string ip,
    util::TagDecoratorFactory& tagDecoratorFactory,
    Server::OnConnectCheck onConnectCheck,
    boost::asio::yield_context yield
)
{
    impl::UpgradableConnectionPtr connection;
    if (sslDetectionResult.isSsl) {
        if (not sslContext.has_value())
            return std::unexpected{"Error creating a connection: SSL is not supported by this server"};

        connection = std::make_unique<impl::SslHttpConnection>(
            std::move(sslDetectionResult.socket),
            std::move(ip),
            std::move(sslDetectionResult.buffer),
            *sslContext,
            tagDecoratorFactory
        );
    } else {
        connection = std::make_unique<impl::PlainHttpConnection>(
            std::move(sslDetectionResult.socket),
            std::move(ip),
            std::move(sslDetectionResult.buffer),
            tagDecoratorFactory
        );
    }

    auto expectedSuccess = onConnectCheck(*connection);
    if (not expectedSuccess.has_value()) {
        connection->send(std::move(expectedSuccess).error(), yield);
        connection->close(yield);
        return std::unexpected{std::nullopt};
    }
    return connection;
}

std::expected<ConnectionPtr, std::string>
tryUpgradeConnection(
    impl::UpgradableConnectionPtr connection,
    std::optional<boost::asio::ssl::context>& sslContext,
    util::TagDecoratorFactory& tagDecoratorFactory,
    boost::asio::yield_context yield
)
{
    auto const expectedIsUpgrade = connection->isUpgradeRequested(yield);
    if (not expectedIsUpgrade.has_value()) {
        return std::unexpected{
            fmt::format("Error checking whether upgrade requested: {}", expectedIsUpgrade.error().message())
        };
    }

    if (*expectedIsUpgrade) {
        auto expectedUpgradedConnection = connection->upgrade(sslContext, tagDecoratorFactory, yield);
        if (expectedUpgradedConnection.has_value())
            return std::move(expectedUpgradedConnection).value();

        return std::unexpected{fmt::format("Error upgrading connection: {}", expectedUpgradedConnection.error().what())
        };
    }

    return connection;
}

}  // namespace

Server::Server(
    boost::asio::io_context& ctx,
    boost::asio::ip::tcp::endpoint endpoint,
    std::optional<boost::asio::ssl::context> sslContext,
    ProcessingPolicy processingPolicy,
    std::optional<size_t> parallelRequestLimit,
    util::TagDecoratorFactory tagDecoratorFactory,
    std::optional<size_t> maxSubscriptionSendQueueSize,
    OnConnectCheck onConnectCheck,
    OnDisconnectHook onDisconnectHook
)
    : ctx_{ctx}
    , sslContext_{std::move(sslContext)}
    , tagDecoratorFactory_{tagDecoratorFactory}
    , connectionHandler_{processingPolicy, parallelRequestLimit, tagDecoratorFactory_, maxSubscriptionSendQueueSize, std::move(onDisconnectHook)}
    , endpoint_{std::move(endpoint)}
    , onConnectCheck_{std::move(onConnectCheck)}
{
}

void
Server::onGet(std::string const& target, MessageHandler handler)
{
    ASSERT(not running_, "Adding a GET handler is not allowed when Server is running.");
    connectionHandler_.onGet(target, std::move(handler));
}

void
Server::onPost(std::string const& target, MessageHandler handler)
{
    ASSERT(not running_, "Adding a POST handler is not allowed when Server is running.");
    connectionHandler_.onPost(target, std::move(handler));
}

void
Server::onWs(MessageHandler handler)
{
    ASSERT(not running_, "Adding a Websocket handler is not allowed when Server is running.");
    connectionHandler_.onWs(std::move(handler));
}

std::optional<std::string>
Server::run()
{
    LOG(log_.info()) << "Starting ng::Server";
    auto acceptor = makeAcceptor(ctx_.get(), endpoint_);
    if (not acceptor.has_value())
        return std::move(acceptor).error();

    running_ = true;
    boost::asio::spawn(
        ctx_.get(),
        [this, acceptor = std::move(acceptor).value()](boost::asio::yield_context yield) mutable {
            while (true) {
                boost::beast::error_code errorCode;
                boost::asio::ip::tcp::socket socket{ctx_.get().get_executor()};

                acceptor.async_accept(socket, yield[errorCode]);
                LOG(log_.trace()) << "Accepted a new connection";
                if (errorCode) {
                    LOG(log_.debug()) << "Error accepting a connection: " << errorCode.what();
                    continue;
                }
                boost::asio::spawn(
                    ctx_.get(),
                    [this, socket = std::move(socket)](boost::asio::yield_context yield) mutable {
                        handleConnection(std::move(socket), yield);
                    },
                    boost::asio::detached
                );
            }
        }
    );
    return std::nullopt;
}

void
Server::stop(boost::asio::yield_context yield)
{
    connectionHandler_.stop(yield);
}

void
Server::handleConnection(boost::asio::ip::tcp::socket socket, boost::asio::yield_context yield)
{
    auto sslDetectionResultExpected = detectSsl(std::move(socket), yield);
    if (not sslDetectionResultExpected) {
        LOG(log_.info()) << sslDetectionResultExpected.error();
        return;
    }
    auto sslDetectionResult = std::move(sslDetectionResultExpected).value();
    if (not sslDetectionResult)
        return;  // stream truncated, probably user disconnected

    auto ip = extractIp(sslDetectionResult->socket);
    if (not ip.has_value()) {
        LOG(log_.info()) << "Cannot get remote endpoint: " << ip.error().what();
        return;
    }

    auto connectionExpected = makeConnection(
        std::move(sslDetectionResult).value(),
        sslContext_,
        std::move(ip).value(),
        tagDecoratorFactory_,
        onConnectCheck_,
        yield
    );
    if (not connectionExpected.has_value()) {
        if (connectionExpected.error().has_value()) {
            LOG(log_.info()) << *connectionExpected.error();
        }
        return;
    }
    LOG(log_.trace()) << connectionExpected.value()->tag() << "Connection created";

    if (connectionHandler_.isStopping()) {
        boost::asio::spawn(
            ctx_.get(),
            [connection = std::move(connectionExpected).value()](boost::asio::yield_context yield) {
                web::ng::impl::ConnectionHandler::stopConnection(*connection, yield);
            }
        );
        return;
    }

    auto connection =
        tryUpgradeConnection(std::move(connectionExpected).value(), sslContext_, tagDecoratorFactory_, yield);
    if (not connection.has_value()) {
        LOG(log_.info()) << connection.error();
        return;
    }

    boost::asio::spawn(
        ctx_.get(),
        [this, connection = std::move(connection).value()](boost::asio::yield_context yield) mutable {
            connectionHandler_.processConnection(std::move(connection), yield);
        }
    );
}

std::expected<Server, std::string>
makeServer(
    util::config::ClioConfigDefinition const& config,
    Server::OnConnectCheck onConnectCheck,
    Server::OnDisconnectHook onDisconnectHook,
    boost::asio::io_context& context
)
{
    auto const serverConfig = config.getObject("server");

    auto endpoint = makeEndpoint(serverConfig);
    if (not endpoint.has_value())
        return std::unexpected{std::move(endpoint).error()};

    auto expectedSslContext = impl::makeServerSslContext(config);
    if (not expectedSslContext)
        return std::unexpected{std::move(expectedSslContext).error()};

    ProcessingPolicy processingPolicy{ProcessingPolicy::Parallel};
    std::optional<size_t> parallelRequestLimit;

    auto const processingStrategyStr = serverConfig.get<std::string>("processing_policy");
    if (processingStrategyStr == "sequent") {
        processingPolicy = ProcessingPolicy::Sequential;
    } else if (processingStrategyStr == "parallel") {
        parallelRequestLimit = serverConfig.maybeValue<size_t>("parallel_requests_limit");
    } else {
        return std::unexpected{fmt::format("Invalid 'server.processing_strategy': {}", processingStrategyStr)};
    }

    auto const maxSubscriptionSendQueueSize = serverConfig.get<size_t>("ws_max_sending_queue_size");

    return Server{
        context,
        std::move(endpoint).value(),
        std::move(expectedSslContext).value(),
        processingPolicy,
        parallelRequestLimit,
        util::TagDecoratorFactory(config),
        maxSubscriptionSendQueueSize,
        std::move(onConnectCheck),
        std::move(onDisconnectHook)
    };
}

}  // namespace web::ng
