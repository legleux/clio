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

#pragma once

#include "util/WithTimeout.hpp"
#include "util/requests/Types.hpp"
#include "util/requests/WsConnection.hpp"

#include <boost/asio/associated_executor.hpp>
#include <boost/asio/bind_cancellation_slot.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/cancellation_signal.hpp>
#include <boost/asio/cancellation_type.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/core/flat_buffer.hpp>
#include <boost/beast/core/tcp_stream.hpp>
#include <boost/beast/ssl/ssl_stream.hpp>
#include <boost/beast/websocket/rfc6455.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket/stream_base.hpp>
#include <boost/system/errc.hpp>

#include <atomic>
#include <chrono>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <utility>

namespace util::requests::impl {

template <typename StreamType>
class WsConnectionImpl : public WsConnection {
    StreamType ws_;

public:
    explicit WsConnectionImpl(StreamType ws) : ws_(std::move(ws))
    {
    }

    std::expected<std::string, RequestError>
    read(boost::asio::yield_context yield, std::optional<std::chrono::steady_clock::duration> timeout = std::nullopt)
        override
    {
        boost::beast::error_code errorCode;
        boost::beast::flat_buffer buffer;

        auto operation = [&](auto&& token) { ws_.async_read(buffer, token); };
        if (timeout) {
            errorCode = util::withTimeout(operation, yield[errorCode], *timeout);
        } else {
            operation(yield[errorCode]);
        }

        if (errorCode)
            return std::unexpected{RequestError{"Read error", errorCode}};

        return boost::beast::buffers_to_string(std::move(buffer).data());
    }

    std::optional<RequestError>
    write(
        std::string const& message,
        boost::asio::yield_context yield,
        std::optional<std::chrono::steady_clock::duration> timeout = std::nullopt
    ) override
    {
        boost::beast::error_code errorCode;
        auto operation = [&](auto&& token) { ws_.async_write(boost::asio::buffer(message), token); };
        if (timeout) {
            errorCode = util::withTimeout(operation, yield, *timeout);
        } else {
            operation(yield[errorCode]);
        }

        if (errorCode)
            return RequestError{"Write error", errorCode};

        return std::nullopt;
    }

    std::optional<RequestError>
    close(boost::asio::yield_context yield, std::chrono::steady_clock::duration const timeout = kDEFAULT_TIMEOUT)
        override
    {
        // Set the timeout for closing the connection
        boost::beast::websocket::stream_base::timeout wsTimeout{};
        ws_.get_option(wsTimeout);
        wsTimeout.handshake_timeout = timeout;
        ws_.set_option(wsTimeout);

        boost::beast::error_code errorCode;
        ws_.async_close(boost::beast::websocket::close_code::normal, yield[errorCode]);
        if (errorCode)
            return RequestError{"Close error", errorCode};
        return std::nullopt;
    }
};

using PlainWsConnection = WsConnectionImpl<boost::beast::websocket::stream<boost::beast::tcp_stream>>;
using SslWsConnection =
    WsConnectionImpl<boost::beast::websocket::stream<boost::beast::ssl_stream<boost::beast::tcp_stream>>>;

}  // namespace util::requests::impl
