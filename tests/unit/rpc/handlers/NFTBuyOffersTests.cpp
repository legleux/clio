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
#include "rpc/Errors.hpp"
#include "rpc/common/AnyHandler.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/handlers/NFTBuyOffers.hpp"
#include "util/HandlerBaseTestFixture.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/spawn.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STObject.h>

#include <optional>
#include <string>
#include <vector>

using namespace rpc;
namespace json = boost::json;
using namespace testing;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kLEDGER_HASH = "4BC50C9B0D8515D3EAAE1E74B29A95804346C491EE1A95BF25E4AAB854A6A652";
constexpr auto kNFT_ID = "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004";
constexpr auto kINDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";

}  // namespace

struct RPCNFTBuyOffersHandlerTest : HandlerBaseTest {
    RPCNFTBuyOffersHandlerTest()
    {
        backend_->setRange(10, 30);
    }
};

TEST_F(RPCNFTBuyOffersHandlerTest, NonHexLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "ledger_hash": "xxx"
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashMalformed");
    });
}

TEST_F(RPCNFTBuyOffersHandlerTest, LimitNotInt)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": "xxx"
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCNFTBuyOffersHandlerTest, LimitNegative)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": -1
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCNFTBuyOffersHandlerTest, LimitZero)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": 0
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

TEST_F(RPCNFTBuyOffersHandlerTest, NonStringLedgerHash)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{
                "nft_id": "{}", 
                "ledger_hash": 123
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledger_hashNotString");
    });
}

TEST_F(RPCNFTBuyOffersHandlerTest, InvalidLedgerIndexString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "ledger_index": "notvalidated"
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerIndexMalformed");
    });
}

// error case: nft_id invalid format, length is incorrect
TEST_F(RPCNFTBuyOffersHandlerTest, NFTIDInvalidFormat)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(R"({ 
            "nft_id": "00080000B4F4AFC5FBCBD76873F18006173D2193467D3EE7"
        })");
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "nft_idMalformed");
    });
}

// error case: nft_id invalid format
TEST_F(RPCNFTBuyOffersHandlerTest, NFTIDNotString)
{
    runSpawn([this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(R"({ 
            "nft_id": 12
        })");
        auto const output = handler.process(input, Context{.yield = yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "nft_idNotString");
    });
}

// error case ledger non exist via hash
TEST_F(RPCNFTBuyOffersHandlerTest, NonExistLedgerViaLedgerHash)
{
    // mock fetchLedgerByHash return empty
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _))
        .WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "ledger_hash": "{}"
        }})",
        kNFT_ID,
        kLEDGER_HASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield=yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger non exist via index
TEST_F(RPCNFTBuyOffersHandlerTest, NonExistLedgerViaLedgerIndex)
{
    // mock fetchLedgerBySequence return empty
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(std::optional<ripple::LedgerHeader>{}));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_id": "{}",
            "ledger_index": "4"
        }})",
        kNFT_ID
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield=yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via hash
// idk why this case will happen in reality
TEST_F(RPCNFTBuyOffersHandlerTest, NonExistLedgerViaLedgerHash2)
{
    // mock fetchLedgerByHash return ledger but seq is 31 > 30
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 31);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_id": "{}",
            "ledger_hash": "{}"
        }})",
        kNFT_ID,
        kLEDGER_HASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield=yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case ledger > max seq via index
TEST_F(RPCNFTBuyOffersHandlerTest, NonExistLedgerViaLedgerIndex2)
{
    // no need to check from db, call fetchLedgerBySequence 0 time
    // differ from previous logic
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(0);
    auto const input = json::parse(fmt::format(
        R"({{ 
            "nft_id": "{}",
            "ledger_index": "31"
        }})",
        kNFT_ID
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield=yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "lgrNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "ledgerNotFound");
    });
}

// error case when nft is not found
TEST_F(RPCNFTBuyOffersHandlerTest, NoNFT)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerByHash(ripple::uint256{kLEDGER_HASH}, _)).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerByHash).Times(1);
    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(std::nullopt));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);
    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "ledger_hash": "{}"
        }})",
        kNFT_ID,
        kLEDGER_HASH
    ));
    runSpawn([&, this](boost::asio::yield_context yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const output = handler.process(input, Context{.yield=yield});
        ASSERT_FALSE(output);
        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "objectNotFound");
        EXPECT_EQ(err.at("error_message").as_string(), "notFound");
    });
}

TEST_F(RPCNFTBuyOffersHandlerTest, MarkerNotString)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "marker": 9
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerNotString");
    });
}

// error case : invalid marker
// marker format in this RPC is a hex-string of a ripple::uint256.
TEST_F(RPCNFTBuyOffersHandlerTest, InvalidMarker)
{
    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}",
                "marker": "123invalid"
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
        EXPECT_EQ(err.at("error_message").as_string(), "markerMalformed");
    });
    runSpawn([&, this](auto yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "marker": 250
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_FALSE(output);

        auto const err = rpc::makeError(output.result.error());
        EXPECT_EQ(err.at("error").as_string(), "invalidParams");
    });
}

// normal case when only provide nft_id
TEST_F(RPCNFTBuyOffersHandlerTest, DefaultParameters)
{
    static constexpr auto kCORRECT_OUTPUT = R"({
        "nft_id": "00010000A7CAD27B688D14BA1A9FA5366554D6ADCF9CE0875B974D9F00000004",
        "validated": true,
        "offers": [
            {
                "nft_offer_index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321",
                "flags": 0,
                "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "amount": "123"
            },
            {
                "nft_offer_index": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322",
                "flags": 0,
                "owner": "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn",
                "amount": "123"
            }
        ]
    })";

    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // return owner index containing 2 indexes
    auto const directory = ripple::keylet::nft_buys(ripple::uint256{kNFT_ID});
    auto const ownerDir = createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject(directory.key, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(directory.key, testing::_, testing::_)).Times(2);

    // return two nft buy offers
    std::vector<Blob> bbs;
    auto const offer = createNftBuyOffer(kNFT_ID, kACCOUNT);
    bbs.push_back(offer.getSerializer().peekData());
    bbs.push_back(offer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}"
        }})",
        kNFT_ID
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTBuyOffersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(json::parse(kCORRECT_OUTPUT), *output.result);
    });
}

// normal case when provided with nft_id and limit
TEST_F(RPCNFTBuyOffersHandlerTest, MultipleResultsWithMarkerAndLimitOutput)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;
    auto repetitions = 500;
    auto const offer = createNftBuyOffer(kNFT_ID, kACCOUNT);
    auto idx = ripple::uint256{kINDEX1};
    while ((repetitions--) != 0) {
        indexes.push_back(idx++);
        bbs.push_back(offer.getSerializer().peekData());
    }
    ripple::STObject const ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject).WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "limit": 50
        }})",
        kNFT_ID
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTBuyOffersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), 50);
        EXPECT_EQ(output.result->at("limit").as_uint64(), 50);
        EXPECT_EQ(
            boost::json::value_to<std::string>(output.result->at("marker")),
            "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353"
        );
    });
}

// normal case when provided with nft_id, limit and marker
TEST_F(RPCNFTBuyOffersHandlerTest, ResultsForInputWithMarkerAndLimit)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;
    auto repetitions = 500;
    auto const offer = createNftBuyOffer(kNFT_ID, kACCOUNT);
    auto idx = ripple::uint256{kINDEX1};
    while ((repetitions--) != 0) {
        indexes.push_back(idx++);
        bbs.push_back(offer.getSerializer().peekData());
    }
    ripple::STObject const ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    auto const cursorBuyOffer = createNftBuyOffer(kNFT_ID, kACCOUNT);

    // first is nft offer object
    auto const cursor = ripple::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353"};
    auto const first = ripple::keylet::nftoffer(cursor);
    ON_CALL(*backend_, doFetchLedgerObject(first.key, testing::_, testing::_))
        .WillByDefault(Return(cursorBuyOffer.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(first.key, testing::_, testing::_)).Times(1);

    auto const directory = ripple::keylet::nft_buys(ripple::uint256{kNFT_ID});
    auto const startHint = 0ul;  // offer node is hardcoded to 0ul
    auto const secondKey = ripple::keylet::page(directory, startHint).key;
    ON_CALL(*backend_, doFetchLedgerObject(secondKey, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(secondKey, testing::_, testing::_)).Times(3);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "marker": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353",
            "limit": 50
        }})",
        kNFT_ID
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTBuyOffersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), 50);
        EXPECT_EQ(output.result->at("limit").as_uint64(), 50);
        // marker also progressed by 50
        EXPECT_EQ(
            boost::json::value_to<std::string>(output.result->at("marker")),
            "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC385"
        );
    });
}

// normal case when provided with nft_id, limit and marker
// nothing left after reading remaining 50 entries
TEST_F(RPCNFTBuyOffersHandlerTest, ResultsWithoutMarkerForInputWithMarkerAndLimit)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(3);

    // return owner index
    std::vector<ripple::uint256> indexes;
    std::vector<Blob> bbs;
    auto repetitions = 100;
    auto const offer = createNftBuyOffer(kNFT_ID, kACCOUNT);
    auto idx = ripple::uint256{kINDEX1};
    while ((repetitions--) != 0) {
        indexes.push_back(idx++);
        bbs.push_back(offer.getSerializer().peekData());
    }
    ripple::STObject const ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    auto const cursorBuyOffer = createNftBuyOffer(kNFT_ID, kACCOUNT);

    // first is nft offer object
    auto const cursor = ripple::uint256{"E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353"};
    auto const first = ripple::keylet::nftoffer(cursor);
    ON_CALL(*backend_, doFetchLedgerObject(first.key, testing::_, testing::_))
        .WillByDefault(Return(cursorBuyOffer.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(first.key, testing::_, testing::_)).Times(1);

    auto const directory = ripple::keylet::nft_buys(ripple::uint256{kNFT_ID});
    auto const startHint = 0ul;  // offer node is hardcoded to 0ul
    auto const secondKey = ripple::keylet::page(directory, startHint).key;
    ON_CALL(*backend_, doFetchLedgerObject(secondKey, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(secondKey, testing::_, testing::_)).Times(7);

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(3);

    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTBuyOffersHandler{this->backend_}};
        auto const input = json::parse(fmt::format(
            R"({{
                "nft_id": "{}",
                "marker": "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC353",
                "limit": 50
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), 50);
        // no marker/limit to output - we read all items already
        EXPECT_FALSE(output.result->as_object().contains("limit"));
        EXPECT_FALSE(output.result->as_object().contains("marker"));
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": 49
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit somehow?
    });

    runSpawn([this](auto yield) {
        auto const handler = AnyHandler{NFTBuyOffersHandler{backend_}};
        auto const input = json::parse(fmt::format(
            R"({{ 
                "nft_id": "{}", 
                "limit": 501
            }})",
            kNFT_ID
        ));
        auto const output = handler.process(input, Context{yield});
        ASSERT_TRUE(output);  // todo: check limit somehow?
    });
}

TEST_F(RPCNFTBuyOffersHandlerTest, LimitLessThanMin)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // return owner index containing 2 indexes
    auto const directory = ripple::keylet::nft_buys(ripple::uint256{kNFT_ID});
    auto const ownerDir =
        createOwnerDirLedgerObject(std::vector{NFTBuyOffersHandler::kLIMIT_MIN + 1, ripple::uint256{kINDEX1}}, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject(directory.key, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(directory.key, testing::_, testing::_)).Times(2);

    // return two nft buy offers
    std::vector<Blob> bbs;
    auto const offer = createNftBuyOffer(kNFT_ID, kACCOUNT);
    bbs.reserve(NFTBuyOffersHandler::kLIMIT_MIN + 1);
    for (auto i = 0; i < NFTBuyOffersHandler::kLIMIT_MIN + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "limit": {}
        }})",
        kNFT_ID,
        NFTBuyOffersHandler::kLIMIT_MIN - 1
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTBuyOffersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), NFTBuyOffersHandler::kLIMIT_MIN);
        EXPECT_EQ(output.result->at("limit").as_uint64(), NFTBuyOffersHandler::kLIMIT_MIN);
    });
}

TEST_F(RPCNFTBuyOffersHandlerTest, LimitMoreThanMax)
{
    auto ledgerHeader = createLedgerHeader(kLEDGER_HASH, 30);
    ON_CALL(*backend_, fetchLedgerBySequence).WillByDefault(Return(ledgerHeader));
    EXPECT_CALL(*backend_, fetchLedgerBySequence).Times(1);

    // return owner index containing 2 indexes
    auto const directory = ripple::keylet::nft_buys(ripple::uint256{kNFT_ID});
    auto const ownerDir =
        createOwnerDirLedgerObject(std::vector{NFTBuyOffersHandler::kLIMIT_MAX + 1, ripple::uint256{kINDEX1}}, kINDEX1);

    ON_CALL(*backend_, doFetchLedgerObject(directory.key, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    EXPECT_CALL(*backend_, doFetchLedgerObject(directory.key, testing::_, testing::_)).Times(2);

    // return two nft buy offers
    std::vector<Blob> bbs;
    auto const offer = createNftBuyOffer(kNFT_ID, kACCOUNT);
    bbs.reserve(NFTBuyOffersHandler::kLIMIT_MAX + 1);
    for (auto i = 0; i < NFTBuyOffersHandler::kLIMIT_MAX + 1; i++)
        bbs.push_back(offer.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    auto const input = json::parse(fmt::format(
        R"({{
            "nft_id": "{}",
            "limit": {}
        }})",
        kNFT_ID,
        NFTBuyOffersHandler::kLIMIT_MAX + 1
    ));
    runSpawn([&, this](auto yield) {
        auto handler = AnyHandler{NFTBuyOffersHandler{this->backend_}};
        auto const output = handler.process(input, Context{yield});

        ASSERT_TRUE(output);
        EXPECT_EQ(output.result->at("offers").as_array().size(), NFTBuyOffersHandler::kLIMIT_MAX);
        EXPECT_EQ(output.result->at("limit").as_uint64(), NFTBuyOffersHandler::kLIMIT_MAX);
    });
}
