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

#include "util/TestWsServer.hpp"

#include "util/requests/Types.hpp"

#include <boost/asio/buffer.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/socket_base.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/role.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/websocket/error.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <gtest/gtest.h>

#include <algorithm>
#include <expected>
#include <functional>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace asio = boost::asio;
namespace websocket = boost::beast::websocket;

TestWsConnection::TestWsConnection(
    websocket::stream<boost::beast::tcp_stream> wsStream,
    std::vector<util::requests::HttpHeader> headers
)
    : ws_(std::move(wsStream)), headers_(std::move(headers))
{
}

TestWsConnection::TestWsConnection(TestWsConnection&& other)
    : ws_(std::move(other.ws_)), headers_(std::move(other.headers_))
{
}

std::optional<std::string>
TestWsConnection::send(std::string const& message, boost::asio::yield_context yield)
{
    boost::beast::error_code errorCode;
    ws_.async_write(asio::buffer(message), yield[errorCode]);
    if (errorCode)
        return errorCode.message();
    return std::nullopt;
}

void
TestWsConnection::sendPing(boost::beast::websocket::ping_data const& data, boost::asio::yield_context yield)
{
    boost::beast::error_code errorCode;
    ws_.async_ping(data, yield[errorCode]);
    [&]() { ASSERT_FALSE(errorCode) << errorCode.message(); }();
}

std::optional<std::string>
TestWsConnection::receive(boost::asio::yield_context yield)
{
    boost::beast::error_code errorCode;
    boost::beast::flat_buffer buffer;

    ws_.async_read(buffer, yield[errorCode]);
    if (errorCode == websocket::error::closed)
        return std::nullopt;

    [&]() { ASSERT_FALSE(errorCode) << errorCode.message(); }();
    return boost::beast::buffers_to_string(buffer.data());
}

std::optional<std::string>
TestWsConnection::close(boost::asio::yield_context yield)
{
    boost::beast::error_code errorCode;
    ws_.async_close(websocket::close_code::normal, yield[errorCode]);
    if (errorCode)
        return errorCode.message();
    return std::nullopt;
}

std::vector<util::requests::HttpHeader> const&
TestWsConnection::headers() const
{
    return headers_;
}

void
TestWsConnection::setControlFrameCallback(
    std::function<void(boost::beast::websocket::frame_type, std::string_view)> callback
)
{
    ws_.control_callback(std::move(callback));
}

void
TestWsConnection::resetControlFrameCallback()
{
    ws_.control_callback();
}

TestWsServer::TestWsServer(asio::io_context& context, std::string const& host) : acceptor_(context)
{
    auto endpoint = asio::ip::tcp::endpoint(boost::asio::ip::make_address(host), 0);
    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
}

std::string
TestWsServer::port() const
{
    return std::to_string(this->acceptor_.local_endpoint().port());
}

std::expected<TestWsConnection, util::requests::RequestError>
TestWsServer::acceptConnection(asio::yield_context yield)
{
    acceptor_.listen(asio::socket_base::max_listen_connections);

    boost::beast::error_code errorCode;
    asio::ip::tcp::socket socket(acceptor_.get_executor());
    acceptor_.async_accept(socket, yield[errorCode]);
    if (errorCode)
        return std::unexpected{util::requests::RequestError{"Accept error", errorCode}};

    boost::beast::flat_buffer buffer;
    boost::beast::http::request<boost::beast::http::string_body> request;
    boost::beast::http::async_read(socket, buffer, request, yield[errorCode]);
    if (errorCode)
        return std::unexpected{util::requests::RequestError{"Read error", errorCode}};
    std::vector<util::requests::HttpHeader> headers;
    std::ranges::transform(request, std::back_inserter(headers), [](auto const& header) {
        if (header.name() == boost::beast::http::field::unknown)
            return util::requests::HttpHeader{header.name_string(), header.value()};

        return util::requests::HttpHeader{header.name(), header.value()};
    });
    if (not boost::beast::websocket::is_upgrade(request))
        return std::unexpected{util::requests::RequestError{"Not a websocket request"}};

    boost::beast::websocket::stream<boost::beast::tcp_stream> ws(std::move(socket));
    ws.set_option(websocket::stream_base::timeout::suggested(boost::beast::role_type::server));
    ws.async_accept(request, yield[errorCode]);
    if (errorCode)
        return std::unexpected{util::requests::RequestError{"Handshake error", errorCode}};

    return TestWsConnection(std::move(ws), std::move(headers));
}

void
TestWsServer::acceptConnectionAndDropIt(asio::yield_context yield)
{
    acceptConnectionWithoutHandshake(yield);
}

boost::asio::ip::tcp::socket
TestWsServer::acceptConnectionWithoutHandshake(boost::asio::yield_context yield)
{
    acceptor_.listen(asio::socket_base::max_listen_connections);
    boost::beast::error_code errorCode;
    asio::ip::tcp::socket socket(acceptor_.get_executor());
    acceptor_.async_accept(socket, yield[errorCode]);
    [&]() { ASSERT_FALSE(errorCode) << errorCode.message(); }();
    return socket;
}
