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

#include "util/MockOperation.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/Error.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <expected>
#include <string>
#include <utility>

using namespace util::async;
using namespace ::testing;

struct AnyOperationTests : Test {
    using OperationType = MockOperation<std::expected<std::any, ExecutionError>>;
    using StoppableOperationType = MockStoppableOperation<std::expected<std::any, ExecutionError>>;
    using ScheduledOperationType = MockScheduledOperation<std::expected<std::any, ExecutionError>>;
    using RepeatingOperationType = MockRepeatingOperation<std::expected<std::any, ExecutionError>>;

    NaggyMock<OperationType> mockOp;
    NaggyMock<StoppableOperationType> mockStoppableOp;
    NaggyMock<ScheduledOperationType> mockScheduledOp;
    NaggyMock<RepeatingOperationType> mockRepeatingOp;

    AnyOperation<void> voidOp{impl::ErasedOperation(static_cast<OperationType&>(mockOp))};
    AnyOperation<void> voidStoppableOp{impl::ErasedOperation(static_cast<StoppableOperationType&>(mockStoppableOp))};
    AnyOperation<int> intOp{impl::ErasedOperation(static_cast<OperationType&>(mockOp))};
    AnyOperation<void> scheduledVoidOp{impl::ErasedOperation(static_cast<ScheduledOperationType&>(mockScheduledOp))};
    AnyOperation<void> repeatingOp{impl::ErasedOperation(static_cast<RepeatingOperationType&>(mockRepeatingOp))};
};
using AnyOperationDeathTest = AnyOperationTests;

TEST_F(AnyOperationTests, Move)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::any{}));
    auto yoink = std::move(voidOp);
    auto res = yoink.get();
    ASSERT_TRUE(res);
}

TEST_F(AnyOperationTests, VoidDataYieldsNoError)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::any{}));
    auto res = voidOp.get();
    ASSERT_TRUE(res);
}

TEST_F(AnyOperationTests, GetIntData)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    auto res = intOp.get();
    EXPECT_EQ(res.value(), 42);
}

TEST_F(AnyOperationTests, WaitCallPropagated)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call());
    EXPECT_CALL(mockOp, wait()).WillOnce([&] { callback.Call(); });
    voidOp.wait();
}

TEST_F(AnyOperationTests, CancelAndRequestStopCallPropagated)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call()).Times(2);
    EXPECT_CALL(mockScheduledOp, cancel()).WillOnce([&] { callback.Call(); });
    EXPECT_CALL(mockScheduledOp, requestStop()).WillOnce([&] { callback.Call(); });
    scheduledVoidOp.abort();
}

TEST_F(AnyOperationTests, RequestStopCallPropagatedOnStoppableOperation)
{
    StrictMock<MockFunction<void()>> callback;
    EXPECT_CALL(callback, Call());
    EXPECT_CALL(mockStoppableOp, requestStop()).WillOnce([&] { callback.Call(); });
    voidStoppableOp.abort();
}

TEST_F(AnyOperationTests, GetPropagatesError)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::unexpected(ExecutionError{"tid", "Not good"})));
    auto res = intOp.get();
    ASSERT_FALSE(res);
    EXPECT_TRUE(res.error().message.ends_with("Not good"));
}

TEST_F(AnyOperationTests, GetIncorrectDataReturnsError)
{
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<double>(4.2)));
    auto res = intOp.get();

    ASSERT_FALSE(res);
    EXPECT_TRUE(res.error().message.ends_with("Bad any cast"));
    EXPECT_TRUE(std::string{res.error()}.ends_with("Bad any cast"));
}

TEST_F(AnyOperationTests, RepeatingOpWaitPropagated)
{
    EXPECT_CALL(mockRepeatingOp, wait());
    repeatingOp.wait();
}

TEST_F(AnyOperationTests, RepeatingOpRequestStopCallPropagated)
{
    EXPECT_CALL(mockRepeatingOp, requestStop());
    repeatingOp.abort();
}

TEST_F(AnyOperationDeathTest, CallAbortOnNonStoppableOrCancellableOperation)
{
    EXPECT_DEATH(voidOp.abort(), ".*");
}
