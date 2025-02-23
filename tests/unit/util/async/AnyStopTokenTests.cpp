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

#include "util/async/AnyStopToken.hpp"

#include <boost/asio/spawn.hpp>
#include <gtest/gtest.h>

using namespace util::async;
using namespace ::testing;

namespace {
struct FakeStopToken {
    bool stopRequested = false;

    bool
    isStopRequested() const
    {
        return stopRequested;
    }
};
}  // namespace

struct AnyStopTokenTests : public TestWithParam<bool> {};
using AnyStopTokenDeathTest = AnyStopTokenTests;

INSTANTIATE_TEST_CASE_P(AnyStopTokenGroup, AnyStopTokenTests, ValuesIn({true, false}), [](auto const& info) {
    return info.param ? "true" : "false";
});

TEST_P(AnyStopTokenTests, CanCopy)
{
    AnyStopToken const stopToken{FakeStopToken{GetParam()}};
    AnyStopToken const token = stopToken;

    EXPECT_EQ(token, stopToken);
}

TEST_P(AnyStopTokenTests, IsStopRequestedCallPropagated)
{
    auto const flag = GetParam();
    AnyStopToken const stopToken{FakeStopToken{flag}};

    EXPECT_EQ(stopToken.isStopRequested(), flag);
    EXPECT_EQ(stopToken, flag);
}

TEST_F(AnyStopTokenDeathTest, ConversionToYieldContextAssertsIfUnsupported)
{
    EXPECT_DEATH(
        [[maybe_unused]] auto unused = static_cast<boost::asio::yield_context>(AnyStopToken{FakeStopToken{}}), ".*"
    );
}
