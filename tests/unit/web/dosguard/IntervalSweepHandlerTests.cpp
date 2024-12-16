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

#include "util/AsioContextTestFixture.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/dosguard/DOSGuardMock.hpp"
#include "web/dosguard/IntervalSweepHandler.hpp"

#include <boost/json/parse.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <chrono>

using namespace web::dosguard;
using namespace util::config;

struct IntervalSweepHandlerTest : SyncAsioContextTest {
protected:
    constexpr static auto JSONData = R"JSON(
    {
        "dos_guard": {
            "sweep_interval": 0
        }
    }
)JSON";

    DOSGuardStrictMock guardMock;

    ClioConfigDefinition cfg{{"dos_guard.sweep_interval", ConfigValue{ConfigType::Integer}.defaultValue(0)}};
    IntervalSweepHandler sweepHandler{cfg, ctx, guardMock};
};

TEST_F(IntervalSweepHandlerTest, SweepAfterInterval)
{
    EXPECT_CALL(guardMock, clear()).Times(testing::AtLeast(10));
    runContextFor(std::chrono::milliseconds{20});
}
