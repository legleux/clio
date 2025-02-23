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

#include "rpc/WorkQueue.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/MockPrometheus.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "util/prometheus/Counter.hpp"
#include "util/prometheus/Gauge.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <semaphore>

using namespace util;
using namespace util::config;
using namespace rpc;
using namespace util::prometheus;

struct RPCWorkQueueTestBase : NoLoggerFixture {
    ClioConfigDefinition cfg = {
        {"server.max_queue_size", ConfigValue{ConfigType::Integer}.defaultValue(2)},
        {"workers", ConfigValue{ConfigType::Integer}.defaultValue(4)}
    };

    WorkQueue queue = WorkQueue::makeWorkQueue(cfg);
};

struct WorkQueueTest : WithPrometheus, RPCWorkQueueTestBase {};

TEST_F(WorkQueueTest, WhitelistedExecutionCountAddsUp)
{
    static constexpr auto kTOTAL = 512u;
    std::atomic_uint32_t executeCount = 0u;

    for (auto i = 0u; i < kTOTAL; ++i) {
        queue.postCoro([&executeCount](auto /* yield */) { ++executeCount; }, true);
    }

    queue.join();

    auto const report = queue.report();

    EXPECT_EQ(executeCount, kTOTAL);
    EXPECT_EQ(report.at("queued"), kTOTAL);
    EXPECT_EQ(report.at("current_queue_size"), 0);
    EXPECT_EQ(report.at("max_queue_size"), 2);
}

TEST_F(WorkQueueTest, NonWhitelistedPreventSchedulingAtQueueLimitExceeded)
{
    static constexpr auto kTOTAL = 3u;
    auto expectedCount = 2u;
    auto unblocked = false;

    std::mutex mtx;
    std::condition_variable cv;

    for (auto i = 0u; i < kTOTAL; ++i) {
        auto res = queue.postCoro(
            [&](auto /* yield */) {
                std::unique_lock lk{mtx};
                cv.wait(lk, [&] { return unblocked; });

                --expectedCount;
            },
            false
        );

        if (i == kTOTAL - 1) {
            EXPECT_FALSE(res);

            std::unique_lock const lk{mtx};
            unblocked = true;
            cv.notify_all();
        } else {
            EXPECT_TRUE(res);
        }
    }

    queue.join();
    EXPECT_TRUE(unblocked);
}

struct WorkQueueStopTest : WorkQueueTest {
    testing::StrictMock<testing::MockFunction<void()>> onTasksComplete;
    testing::StrictMock<testing::MockFunction<void()>> taskMock;
};

TEST_F(WorkQueueStopTest, RejectsNewTasksWhenStopping)
{
    EXPECT_CALL(taskMock, Call());
    EXPECT_TRUE(queue.postCoro([this](auto /* yield */) { taskMock.Call(); }, false));

    queue.stop([]() {});
    EXPECT_FALSE(queue.postCoro([this](auto /* yield */) { taskMock.Call(); }, false));

    queue.join();
}

TEST_F(WorkQueueStopTest, CallsOnTasksCompleteWhenStoppingAndQueueIsEmpty)
{
    EXPECT_CALL(taskMock, Call());
    EXPECT_TRUE(queue.postCoro([this](auto /* yield */) { taskMock.Call(); }, false));

    EXPECT_CALL(onTasksComplete, Call()).WillOnce([&]() { EXPECT_EQ(queue.size(), 0u); });
    queue.stop(onTasksComplete.AsStdFunction());
    queue.join();
}
TEST_F(WorkQueueStopTest, CallsOnTasksCompleteWhenStoppingOnLastTask)
{
    std::binary_semaphore semaphore{0};

    EXPECT_CALL(taskMock, Call());
    EXPECT_TRUE(queue.postCoro(
        [&](auto /* yield */) {
            taskMock.Call();
            semaphore.acquire();
        },
        false
    ));

    EXPECT_CALL(onTasksComplete, Call()).WillOnce([&]() { EXPECT_EQ(queue.size(), 0u); });
    queue.stop(onTasksComplete.AsStdFunction());
    semaphore.release();

    queue.join();
}

struct WorkQueueMockPrometheusTest : WithMockPrometheus, RPCWorkQueueTestBase {};

TEST_F(WorkQueueMockPrometheusTest, postCoroCouhters)
{
    auto& queuedMock = makeMock<CounterInt>("work_queue_queued_total_number", "");
    auto& durationMock = makeMock<CounterInt>("work_queue_cumulitive_tasks_duration_us", "");
    auto& curSizeMock = makeMock<GaugeInt>("work_queue_current_size", "");

    std::binary_semaphore semaphore{0};

    EXPECT_CALL(curSizeMock, value()).Times(2).WillRepeatedly(::testing::Return(0));
    EXPECT_CALL(curSizeMock, add(1));
    EXPECT_CALL(queuedMock, add(1));
    EXPECT_CALL(durationMock, add(::testing::Gt(0))).WillOnce([&](auto) {
        EXPECT_CALL(curSizeMock, add(-1));
        semaphore.release();
    });

    auto const res = queue.postCoro([&](auto /* yield */) { semaphore.acquire(); }, false);

    ASSERT_TRUE(res);
    queue.join();
}
