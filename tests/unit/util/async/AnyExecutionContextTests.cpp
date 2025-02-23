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

#include "util/MockExecutionContext.hpp"
#include "util/MockOperation.hpp"
#include "util/MockStrand.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/async/AnyOperation.hpp"
#include "util/async/AnyStopToken.hpp"
#include "util/async/AnyStrand.hpp"
#include "util/async/Concepts.hpp"
#include "util/async/Operation.hpp"
#include "util/async/Outcome.hpp"
#include "util/async/context/SyncExecutionContext.hpp"
#include "util/async/context/impl/Cancellation.hpp"
#include "util/async/impl/ErasedOperation.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <any>
#include <chrono>
#include <functional>
#include <optional>
#include <type_traits>
#include <utility>

using namespace util::async;
using namespace ::testing;

static_assert(SomeStoppable<StoppableOperation<int, impl::BasicStopSource>>);
static_assert(not SomeStoppable<Operation<int>>);

static_assert(SomeCancellable<ScheduledOperation<SyncExecutionContext, int>>);
static_assert(not SomeCancellable<int>);

static_assert(SomeOperation<Operation<int>>);
static_assert(not SomeOperation<int>);

static_assert(SomeStoppableOperation<MockStoppableOperation<int>>);
static_assert(not SomeStoppableOperation<MockOperation<int>>);

static_assert(SomeCancellableOperation<MockScheduledOperation<int>>);
static_assert(not SomeCancellableOperation<MockOperation<int>>);

static_assert(SomeOutcome<Outcome<int>>);
static_assert(not SomeOutcome<int>);

static_assert(SomeStopToken<impl::BasicStopSource::Token>);
static_assert(not SomeStopToken<int>);

static_assert(SomeYieldStopSource<impl::YieldContextStopSource>);
static_assert(not SomeYieldStopSource<int>);
static_assert(not SomeYieldStopSource<impl::BasicStopSource>);

static_assert(SomeSimpleStopSource<impl::BasicStopSource>);
static_assert(not SomeSimpleStopSource<int>);

static_assert(SomeStopSource<impl::BasicStopSource>);
static_assert(not SomeStopSource<int>);

static_assert(SomeStopSourceProvider<StoppableOutcome<int, impl::BasicStopSource>>);
static_assert(not SomeStopSourceProvider<Outcome<int>>);

static_assert(SomeStoppableOutcome<StoppableOutcome<int, impl::BasicStopSource>>);
static_assert(not SomeStoppableOutcome<Outcome<int>>);

static_assert(SomeHandlerWithoutStopToken<decltype([]() {})>);
static_assert(not SomeHandlerWithoutStopToken<decltype([](int) {})>);

static_assert(SomeHandlerWith<decltype([](int) {}), int>);
static_assert(not SomeHandlerWith<decltype([](int) {}), std::optional<float>>);

static_assert(SomeStdDuration<std::chrono::steady_clock::duration>);
static_assert(not SomeStdDuration<std::chrono::steady_clock::time_point>);

static_assert(SomeStdOptional<std::optional<int>>);
static_assert(not SomeStdOptional<int>);

static_assert(SomeOptStdDuration<std::optional<std::chrono::steady_clock::duration>>);
static_assert(not SomeOptStdDuration<std::chrono::duration<double>>);
static_assert(not SomeOptStdDuration<std::optional<int>>);

static_assert(NotSameAs<int, impl::ErasedOperation>);
static_assert(not NotSameAs<impl::ErasedOperation, impl::ErasedOperation>);

static_assert(RValueNotSameAs<int&&, impl::ErasedOperation>);
static_assert(not RValueNotSameAs<int&, impl::ErasedOperation>);

struct AnyExecutionContextTests : Test {
    using StrandType = NiceMock<MockStrand>;

    template <typename T>
    using OperationType = NiceMock<MockOperation<T>>;

    template <typename T>
    using StoppableOperationType = NiceMock<MockStoppableOperation<T>>;

    template <typename T>
    using ScheduledOperationType = NiceMock<MockScheduledOperation<T>>;

    template <typename T>
    using RepeatingOperationType = NiceMock<MockRepeatingOperation<T>>;

    NiceMock<MockExecutionContext> mockExecutionContext;
    AnyExecutionContext ctx{static_cast<MockExecutionContext&>(mockExecutionContext)};
};

TEST_F(AnyExecutionContextTests, Move)
{
    auto mockOp = OperationType<std::any>{};
    EXPECT_CALL(mockExecutionContext, execute(A<std::function<std::any()>>())).WillOnce(ReturnRef(mockOp));
    EXPECT_CALL(mockOp, get());

    auto mineNow = std::move(ctx);
    ASSERT_TRUE(mineNow.execute([] { throw 0; }).get());
}

TEST_F(AnyExecutionContextTests, CopyIsRefCounted)
{
    auto mockOp = OperationType<std::any>{};
    EXPECT_CALL(mockExecutionContext, execute(A<std::function<std::any()>>())).WillOnce(ReturnRef(mockOp));
    EXPECT_CALL(mockOp, get());

    auto yoink = ctx;
    ASSERT_TRUE(yoink.execute([] { throw 0; }).get());
}

TEST_F(AnyExecutionContextTests, ExecuteWithoutTokenAndVoid)
{
    auto mockOp = OperationType<std::any>{};
    EXPECT_CALL(mockExecutionContext, execute(A<std::function<std::any()>>())).WillOnce(ReturnRef(mockOp));
    EXPECT_CALL(mockOp, get());

    auto op = ctx.execute([] { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, ExecuteWithoutTokenAndVoidThrowsException)
{
    auto mockOp = OperationType<std::any>{};
    EXPECT_CALL(mockExecutionContext, execute(A<std::function<std::any()>>()))
        .WillOnce([](auto&&) -> OperationType<std::any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = ctx.execute([] { throw 0; }));
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<std::any>{};
    EXPECT_CALL(mockExecutionContext, execute(A<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));
    EXPECT_CALL(mockOp, get());

    auto op = ctx.execute([](auto) { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndVoidThrowsException)
{
    EXPECT_CALL(mockExecutionContext, execute(A<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<std::any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = ctx.execute([](auto) { throw 0; }));
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<std::any>{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, execute(A<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce(ReturnRef(mockOp));

    auto op = ctx.execute([](auto) -> int { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    ASSERT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, ExecuteWithStopTokenAndReturnValueThrowsException)
{
    EXPECT_CALL(mockExecutionContext, execute(A<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<std::any> const& { throw 0; });

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = ctx.execute([](auto) -> int { throw 0; }));
}

TEST_F(AnyExecutionContextTests, TimerCancellation)
{
    auto mockScheduledOp = ScheduledOperationType<std::any>{};
    EXPECT_CALL(mockScheduledOp, cancel());
    EXPECT_CALL(
        mockExecutionContext, scheduleAfter(std::chrono::milliseconds{12}, A<std::function<std::any(AnyStopToken)>>())
    )
        .WillOnce(ReturnRef(mockScheduledOp));

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{12}, [](auto) { throw 0; });
    static_assert(std::is_same_v<decltype(timer), AnyOperation<void>>);

    timer.abort();
}

TEST_F(AnyExecutionContextTests, TimerExecuted)
{
    auto mockScheduledOp = ScheduledOperationType<std::any>{};
    EXPECT_CALL(mockScheduledOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(
        mockExecutionContext, scheduleAfter(std::chrono::milliseconds{12}, A<std::function<std::any(AnyStopToken)>>())
    )
        .WillOnce([&mockScheduledOp](auto, auto&&) -> ScheduledOperationType<std::any> const& {
            return mockScheduledOp;
        });

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{12}, [](auto) -> int { throw 0; });

    static_assert(std::is_same_v<decltype(timer), AnyOperation<int>>);
    EXPECT_EQ(timer.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, TimerWithBoolHandlerCancellation)
{
    auto mockScheduledOp = ScheduledOperationType<std::any>{};
    EXPECT_CALL(mockScheduledOp, cancel());
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(std::chrono::milliseconds{12}, A<std::function<std::any(AnyStopToken, bool)>>())
    )
        .WillOnce(ReturnRef(mockScheduledOp));

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{12}, [](auto, bool) { throw 0; });
    static_assert(std::is_same_v<decltype(timer), AnyOperation<void>>);

    timer.abort();
}

TEST_F(AnyExecutionContextTests, TimerWithBoolHandlerExecuted)
{
    auto mockScheduledOp = ScheduledOperationType<std::any>{};
    EXPECT_CALL(mockScheduledOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(
        mockExecutionContext,
        scheduleAfter(std::chrono::milliseconds{12}, A<std::function<std::any(AnyStopToken, bool)>>())
    )
        .WillOnce([&mockScheduledOp](auto, auto&&) -> ScheduledOperationType<std::any> const& {
            return mockScheduledOp;
        });

    auto timer = ctx.scheduleAfter(std::chrono::milliseconds{12}, [](auto, bool) -> int { throw 0; });

    static_assert(std::is_same_v<decltype(timer), AnyOperation<int>>);
    EXPECT_EQ(timer.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, RepeatingOperation)
{
    auto mockRepeatingOp = RepeatingOperationType<std::any>{};
    EXPECT_CALL(mockRepeatingOp, wait());
    EXPECT_CALL(mockExecutionContext, executeRepeatedly(std::chrono::milliseconds{1}, A<std::function<std::any()>>()))
        .WillOnce([&mockRepeatingOp] -> RepeatingOperationType<std::any> const& { return mockRepeatingOp; });

    auto res = ctx.executeRepeatedly(std::chrono::milliseconds{1}, [] -> void { throw 0; });
    static_assert(std::is_same_v<decltype(res), AnyOperation<void>>);
    res.wait();
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithVoid)
{
    auto mockOp = OperationType<std::any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get());
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(A<std::function<std::any()>>())).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([] { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithVoidThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(A<std::function<std::any()>>()))
        .WillOnce([](auto&&) -> OperationType<std::any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([] {}));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithReturnValue)
{
    auto mockOp = OperationType<std::any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(A<std::function<std::any()>>())).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([]() -> int { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    EXPECT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithReturnValueThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(A<std::function<std::any()>>()))
        .WillOnce([](auto&&) -> OperationType<std::any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([]() -> int { throw 0; }));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndVoid)
{
    auto mockOp = StoppableOperationType<std::any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get());
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(A<std::function<std::any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([](auto) { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<void>>);

    ASSERT_TRUE(op.get());
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndVoidThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(A<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<std::any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) { throw 0; }));
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndReturnValue)
{
    auto mockOp = StoppableOperationType<std::any>{};
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockOp, get()).WillOnce(Return(std::make_any<int>(42)));
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(A<std::function<std::any(AnyStopToken)>>(), _)).WillOnce(ReturnRef(mockOp));

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    auto op = strand.execute([](auto) -> int { throw 0; });
    static_assert(std::is_same_v<decltype(op), AnyOperation<int>>);

    EXPECT_EQ(op.get().value(), 42);
}

TEST_F(AnyExecutionContextTests, StrandExecuteWithStopTokenAndReturnValueThrowsException)
{
    auto mockStrand = StrandType{};
    EXPECT_CALL(mockExecutionContext, makeStrand()).WillOnce(ReturnRef(mockStrand));
    EXPECT_CALL(mockStrand, execute(A<std::function<std::any(AnyStopToken)>>(), _))
        .WillOnce([](auto&&, auto) -> StoppableOperationType<std::any> const& { throw 0; });

    auto strand = ctx.makeStrand();
    static_assert(std::is_same_v<decltype(strand), AnyStrand>);

    EXPECT_ANY_THROW([[maybe_unused]] auto unused = strand.execute([](auto) -> int { throw 0; }));
}
