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

#include "feed/FeedTestUtil.hpp"
#include "feed/impl/SingleFeedBase.hpp"
#include "util/MockPrometheus.hpp"
#include "util/MockWsBase.hpp"
#include "util/SyncExecutionCtxFixture.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/prometheus/Gauge.hpp"
#include "web/SubscriptionContextInterface.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>

namespace {
constexpr auto kFEED = R"({"test":"test"})";
}  // namespace

using namespace feed::impl;
using namespace util::prometheus;

struct FeedBaseMockPrometheusTest : WithMockPrometheus, SyncExecutionCtxFixture {
protected:
    web::SubscriptionContextPtr sessionPtr_ = std::make_shared<MockSession>();
    std::shared_ptr<SingleFeedBase> testFeedPtr_ = std::make_shared<SingleFeedBase>(ctx_, "testFeed");
    MockSession* mockSessionPtr_ = dynamic_cast<MockSession*>(sessionPtr_.get());
};

TEST_F(FeedBaseMockPrometheusTest, subUnsub)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"testFeed\"}");
    EXPECT_CALL(counter, add(1));
    EXPECT_CALL(counter, add(-1));

    EXPECT_CALL(*mockSessionPtr_, onDisconnect);
    testFeedPtr_->sub(sessionPtr_);
    testFeedPtr_->unsub(sessionPtr_);
}

TEST_F(FeedBaseMockPrometheusTest, AutoUnsub)
{
    auto& counter = makeMock<GaugeInt>("subscriptions_current_number", "{stream=\"testFeed\"}");
    EXPECT_CALL(counter, add(1));
    EXPECT_CALL(counter, add(-1));

    web::SubscriptionContextInterface::OnDisconnectSlot slot;
    EXPECT_CALL(*mockSessionPtr_, onDisconnect).WillOnce(testing::SaveArg<0>(&slot));
    testFeedPtr_->sub(sessionPtr_);
    slot(sessionPtr_.get());
    sessionPtr_.reset();
}

class NamedSingleFeedTest : public SingleFeedBase {
public:
    NamedSingleFeedTest(util::async::AnyExecutionContext& executionCtx) : SingleFeedBase(executionCtx, "forTest")
    {
    }
};

using SingleFeedBaseTest = FeedBaseTest<NamedSingleFeedTest>;

TEST_F(SingleFeedBaseTest, Test)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kFEED)));
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->pub(kFEED);

    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->pub(kFEED);
}

TEST_F(SingleFeedBaseTest, TestAutoDisconnect)
{
    web::SubscriptionContextInterface::OnDisconnectSlot slot;
    EXPECT_CALL(*mockSessionPtr, onDisconnect).WillOnce(testing::SaveArg<0>(&slot));
    EXPECT_CALL(*mockSessionPtr, send(sharedStringJsonEq(kFEED)));
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->pub(kFEED);

    slot(sessionPtr.get());
    sessionPtr.reset();
    EXPECT_EQ(testFeedPtr->count(), 0);
}

TEST_F(SingleFeedBaseTest, RepeatSub)
{
    EXPECT_CALL(*mockSessionPtr, onDisconnect);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->sub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 1);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
    testFeedPtr->unsub(sessionPtr);
    EXPECT_EQ(testFeedPtr->count(), 0);
}
