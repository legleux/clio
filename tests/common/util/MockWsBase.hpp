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
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/SubscriptionContextInterface.hpp"
#include "web/interface/ConnectionBase.hpp"

#include <boost/beast/http/status.hpp>
#include <gmock/gmock.h>

#include <cstdint>
#include <memory>
#include <string>

struct MockSession : public web::SubscriptionContextInterface {
    MOCK_METHOD(void, send, (std::shared_ptr<std::string>), (override));
    MOCK_METHOD(void, onDisconnect, (OnDisconnectSlot const&), (override));
    MOCK_METHOD(void, setApiSubversion, (uint32_t), (override));
    MOCK_METHOD(uint32_t, apiSubversion, (), (const, override));

    util::TagDecoratorFactory tagDecoratorFactory{util::config::ClioConfigDefinition{
        {"log_tag_style", util::config::ConfigValue{util::config::ConfigType::String}.defaultValue("none")}
    }};

    MockSession() : web::SubscriptionContextInterface(tagDecoratorFactory)
    {
    }
};

struct MockDeadSession : public web::ConnectionBase {
    void
    send(std::shared_ptr<std::string>) override
    {
        // err happen, the session should remove from subscribers
        ec_.assign(2, boost::system::system_category());
    }

    void
    // NOLINTNEXTLINE(cppcoreguidelines-rvalue-reference-param-not-moved)
    send(std::string&&, boost::beast::http::status = boost::beast::http::status::ok) override
    {
    }

    MockDeadSession(util::TagDecoratorFactory const& factory) : web::ConnectionBase(factory, "")
    {
    }
};
