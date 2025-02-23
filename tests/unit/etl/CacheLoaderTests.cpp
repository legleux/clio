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

#include "data/Types.hpp"
#include "etl/CacheLoader.hpp"
#include "etl/CacheLoaderSettings.hpp"
#include "etl/FakeDiffProvider.hpp"
#include "etl/impl/CacheLoader.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockCache.hpp"
#include "util/MockPrometheus.hpp"
#include "util/async/context/BasicExecutionContext.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <vector>

namespace json = boost::json;
using namespace etl;
using namespace util;
using namespace data;
using namespace testing;
using namespace util::config;

namespace {

inline ClioConfigDefinition
generateDefaultCacheConfig()
{
    return ClioConfigDefinition{
        {{"io_threads", ConfigValue{ConfigType::Integer}.defaultValue(2)},
         {"cache.num_diffs", ConfigValue{ConfigType::Integer}.defaultValue(32)},
         {"cache.num_markers", ConfigValue{ConfigType::Integer}.defaultValue(48)},
         {"cache.num_cursors_from_diff", ConfigValue{ConfigType::Integer}.defaultValue(0)},
         {"cache.num_cursors_from_account", ConfigValue{ConfigType::Integer}.defaultValue(0)},
         {"cache.page_fetch_size", ConfigValue{ConfigType::Integer}.defaultValue(512)},
         {"cache.load", ConfigValue{ConfigType::String}.defaultValue("async")}}
    };
}

inline ClioConfigDefinition
getParseCacheConfig(boost::json::value val)
{
    ConfigFileJson const jsonVal{val.as_object()};
    auto config = generateDefaultCacheConfig();
    auto const errors = config.parse(jsonVal);
    [&]() { ASSERT_FALSE(errors.has_value()); }();
    return config;
}

constexpr auto kSEQ = 30;

struct CacheLoaderTest : util::prometheus::WithPrometheus, MockBackendTest {
    DiffProvider diffProvider;
    MockCache cache;
};

using Settings = etl::CacheLoaderSettings;
struct ParametrizedCacheLoaderTest : CacheLoaderTest, WithParamInterface<Settings> {};

};  // namespace

//
// Tests of implementation
//
INSTANTIATE_TEST_CASE_P(
    CacheLoaderTest,
    ParametrizedCacheLoaderTest,
    Values(
        Settings{.numCacheDiffs = 32, .numCacheMarkers = 48, .cachePageFetchSize = 512, .numThreads = 2},
        Settings{.numCacheDiffs = 32, .numCacheMarkers = 48, .cachePageFetchSize = 512, .numThreads = 4},
        Settings{.numCacheDiffs = 32, .numCacheMarkers = 48, .cachePageFetchSize = 512, .numThreads = 8},
        Settings{.numCacheDiffs = 32, .numCacheMarkers = 48, .cachePageFetchSize = 512, .numThreads = 16},
        Settings{.numCacheDiffs = 32, .numCacheMarkers = 128, .cachePageFetchSize = 24, .numThreads = 2},
        Settings{.numCacheDiffs = 32, .numCacheMarkers = 64, .cachePageFetchSize = 48, .numThreads = 4},
        Settings{.numCacheDiffs = 32, .numCacheMarkers = 48, .cachePageFetchSize = 64, .numThreads = 8},
        Settings{.numCacheDiffs = 32, .numCacheMarkers = 24, .cachePageFetchSize = 128, .numThreads = 16},
        Settings{.numCacheDiffs = 128, .numCacheMarkers = 128, .cachePageFetchSize = 24, .numThreads = 2},
        Settings{.numCacheDiffs = 1024, .numCacheMarkers = 64, .cachePageFetchSize = 48, .numThreads = 4},
        Settings{.numCacheDiffs = 512, .numCacheMarkers = 48, .cachePageFetchSize = 64, .numThreads = 8},
        Settings{.numCacheDiffs = 64, .numCacheMarkers = 24, .cachePageFetchSize = 128, .numThreads = 16}
    ),
    [](auto const& info) {
        auto const settings = info.param;
        return fmt::format(
            "diffs_{}__markers_{}__fetchSize_{}__threads_{}",
            settings.numCacheDiffs,
            settings.numCacheMarkers,
            settings.cachePageFetchSize,
            settings.numThreads
        );
    }
);

TEST_P(ParametrizedCacheLoaderTest, LoadCacheWithDifferentSettings)
{
    auto const& settings = GetParam();
    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 14;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, kSEQ, _)).Times(keysSize * loops).WillRepeatedly([this]() {
        return diffProvider.nextKey(keysSize);
    });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .WillRepeatedly(Return(std::vector<Blob>(keysSize - 1, Blob{'s'})));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImp).Times(loops);
    EXPECT_CALL(cache, setFull).Times(1);

    async::CoroExecutionContext ctx{settings.numThreads};
    etl::impl::CursorFromFixDiffNumProvider const provider{backend_, settings.numCacheDiffs};

    etl::impl::CacheLoaderImpl<MockCache> loader{
        ctx, backend_, cache, kSEQ, settings.numCacheMarkers, settings.cachePageFetchSize, provider.getCursors(kSEQ)
    };

    loader.wait();
}

TEST_P(ParametrizedCacheLoaderTest, AutomaticallyCancelledAndAwaitedInDestructor)
{
    auto const& settings = GetParam();
    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 1024;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, kSEQ, _)).Times(AtMost(keysSize * loops)).WillRepeatedly([this]() {
        return diffProvider.nextKey(keysSize);
    });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .WillRepeatedly(Return(std::vector<Blob>(keysSize - 1, Blob{'s'})));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImp).Times(AtMost(loops));
    EXPECT_CALL(cache, setFull).Times(AtMost(1));

    async::CoroExecutionContext ctx{settings.numThreads};
    etl::impl::CursorFromFixDiffNumProvider const provider{backend_, settings.numCacheDiffs};

    etl::impl::CacheLoaderImpl<MockCache> const loader{
        ctx, backend_, cache, kSEQ, settings.numCacheMarkers, settings.cachePageFetchSize, provider.getCursors(kSEQ)
    };

    // no loader.wait(): loader is immediately stopped and awaited in destructor
}

TEST_P(ParametrizedCacheLoaderTest, CacheDisabledLeadsToCancellation)
{
    auto const& settings = GetParam();
    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 1024;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey(_, kSEQ, _)).Times(AtMost(keysSize * loops)).WillRepeatedly([this]() {
        return diffProvider.nextKey(keysSize);
    });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .WillRepeatedly(Return(std::vector<Blob>(keysSize - 1, Blob{'s'})));

    EXPECT_CALL(cache, isDisabled).WillOnce(Return(false)).WillRepeatedly(Return(true));
    EXPECT_CALL(cache, updateImp).Times(AtMost(1));
    EXPECT_CALL(cache, setFull).Times(0);

    async::CoroExecutionContext ctx{settings.numThreads};
    etl::impl::CursorFromFixDiffNumProvider const provider{backend_, settings.numCacheDiffs};

    etl::impl::CacheLoaderImpl<MockCache> loader{
        ctx, backend_, cache, kSEQ, settings.numCacheMarkers, settings.cachePageFetchSize, provider.getCursors(kSEQ)
    };

    loader.wait();
}

//
// Tests of public CacheLoader interface
//
TEST_F(CacheLoaderTest, SyncCacheLoaderWaitsTillFullyLoaded)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"load": "sync"}})"));
    CacheLoader loader{cfg, backend_, cache};

    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 14;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(32).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(keysSize * loops).WillRepeatedly([this]() {
        return diffProvider.nextKey(keysSize);
    });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .Times(loops)
        .WillRepeatedly(Return(std::vector<Blob>{keysSize - 1, Blob{'s'}}));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImp).Times(loops);
    EXPECT_CALL(cache, isFull).WillOnce(Return(false)).WillRepeatedly(Return(true));
    EXPECT_CALL(cache, setFull).Times(1);

    loader.load(kSEQ);
}

TEST_F(CacheLoaderTest, AsyncCacheLoaderCanBeStopped)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"load": "async"}})"));
    CacheLoader loader{cfg, backend_, cache};

    auto const diffs = diffProvider.getLatestDiff();
    auto const loops = diffs.size() + 1;
    auto const keysSize = 14;

    EXPECT_CALL(*backend_, fetchLedgerDiff(_, _)).Times(AtMost(32)).WillRepeatedly(Return(diffs));
    EXPECT_CALL(*backend_, doFetchSuccessorKey).Times(AtMost(keysSize * loops)).WillRepeatedly([this]() {
        return diffProvider.nextKey(keysSize);
    });

    EXPECT_CALL(*backend_, doFetchLedgerObjects(_, kSEQ, _))
        .Times(AtMost(loops))
        .WillRepeatedly(Return(std::vector<Blob>{keysSize - 1, Blob{'s'}}));

    EXPECT_CALL(cache, isDisabled).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, updateImp).Times(AtMost(loops));
    EXPECT_CALL(cache, isFull).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, setFull).Times(AtMost(1));

    loader.load(kSEQ);
    loader.stop();
    loader.wait();
}

TEST_F(CacheLoaderTest, DisabledCacheLoaderDoesNotLoadCache)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"load": "none"}})"));
    CacheLoader loader{cfg, backend_, cache};

    EXPECT_CALL(cache, updateImp).Times(0);
    EXPECT_CALL(cache, isFull).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, setDisabled).Times(1);

    loader.load(kSEQ);
}

TEST_F(CacheLoaderTest, DisabledCacheLoaderCanCallStopAndWait)
{
    auto const cfg = getParseCacheConfig(json::parse(R"({"cache": {"load": "none"}})"));
    CacheLoader loader{cfg, backend_, cache};

    EXPECT_CALL(cache, updateImp).Times(0);
    EXPECT_CALL(cache, isFull).WillRepeatedly(Return(false));
    EXPECT_CALL(cache, setDisabled).Times(1);

    loader.load(kSEQ);

    EXPECT_NO_THROW(loader.stop());
    EXPECT_NO_THROW(loader.wait());
}
