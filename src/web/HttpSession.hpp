//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2023, the clio developers.

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

#pragma once

#include "util/Taggable.hpp"
#include "web/AdminVerificationStrategy.hpp"
#include "web/PlainWsSession.hpp"
#include "web/dosguard/DOSGuardInterface.hpp"
#include "web/impl/HttpBase.hpp"
#include "web/interface/Concepts.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace web {

using tcp = boost::asio::ip::tcp;

/**
 * @brief Represents a HTTP connection established by a client.
 *
 * It will handle the upgrade to websocket, pass the ownership of the socket to the upgrade session.
 * Otherwise, it will pass control to the base class.
 *
 * @tparam HandlerType The type of the server handler to use
 */
template <SomeServerHandler HandlerType>
class HttpSession : public impl::HttpBase<HttpSession, HandlerType>,
                    public std::enable_shared_from_this<HttpSession<HandlerType>> {
    boost::beast::tcp_stream stream_;
    std::reference_wrapper<util::TagDecoratorFactory const> tagFactory_;
    std::uint32_t maxWsSendingQueueSize_;

public:
    /**
     * @brief Create a new session.
     *
     * @param socket The socket. Ownership is transferred to HttpSession
     * @param ip Client's IP address
     * @param adminVerification The admin verification strategy to use
     * @param tagFactory A factory that is used to generate tags to track requests and sessions
     * @param dosGuard The denial of service guard to use
     * @param handler The server handler to use
     * @param buffer Buffer with initial data received from the peer
     * @param maxWsSendingQueueSize The maximum size of the sending queue for websocket
     */
    explicit HttpSession(
        tcp::socket&& socket,
        std::string const& ip,
        std::shared_ptr<AdminVerificationStrategy> const& adminVerification,
        std::reference_wrapper<util::TagDecoratorFactory const> tagFactory,
        std::reference_wrapper<dosguard::DOSGuardInterface> dosGuard,
        std::shared_ptr<HandlerType> const& handler,
        boost::beast::flat_buffer buffer,
        std::uint32_t maxWsSendingQueueSize
    )
        : impl::HttpBase<HttpSession, HandlerType>(
              ip,
              tagFactory,
              adminVerification,
              dosGuard,
              handler,
              std::move(buffer)
          )
        , stream_(std::move(socket))
        , tagFactory_(tagFactory)
        , maxWsSendingQueueSize_(maxWsSendingQueueSize)
    {
    }

    ~HttpSession() override = default;

    /** @return The TCP stream */
    boost::beast::tcp_stream&
    stream()
    {
        return stream_;
    }

    /** @brief Starts reading from the stream. */
    void
    run()
    {
        boost::asio::dispatch(
            stream_.get_executor(),
            boost::beast::bind_front_handler(
                &impl::HttpBase<HttpSession, HandlerType>::doRead, this->shared_from_this()
            )
        );
    }

    /** @brief Closes the underlying socket. */
    void
    doClose()
    {
        boost::beast::error_code ec;
        stream_.socket().shutdown(tcp::socket::shutdown_send, ec);
    }

    /** @brief Upgrade to WebSocket connection. */
    void
    upgrade()
    {
        std::make_shared<WsUpgrader<HandlerType>>(
            std::move(stream_),
            this->clientIp,
            tagFactory_,
            this->dosGuard_,
            this->handler_,
            std::move(this->buffer_),
            std::move(this->req_),
            ConnectionBase::isAdmin(),
            maxWsSendingQueueSize_
        )
            ->run();
    }
};
}  // namespace web
