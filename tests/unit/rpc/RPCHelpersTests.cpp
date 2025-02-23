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
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/AsioContextTestFixture.hpp"
#include "util/MockBackendTestFixture.hpp"
#include "util/MockPrometheus.hpp"
#include "util/NameGenerator.hpp"
#include "util/TestObject.hpp"

#include <boost/asio/impl/spawn.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/json/array.hpp>
#include <boost/json/parse.hpp>
#include <fmt/core.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STObject.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <variant>
#include <vector>

using namespace rpc;
using namespace testing;

namespace {

constexpr auto kACCOUNT = "rf1BiGeXwwQoi8Z2ueFYTEXSwuJYfV2Jpn";
constexpr auto kACCOUNT2 = "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun";
constexpr auto kINDEX1 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";
constexpr auto kINDEX2 = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC322";
constexpr auto kTXN_ID = "E6DBAFC99223B42257915A63DFC6B0C032D4070F9A574B255AD97466726FC321";

}  // namespace

class RPCHelpersTest : public util::prometheus::WithPrometheus, public MockBackendTest, public SyncAsioContextTest {
    void
    SetUp() override
    {
        SyncAsioContextTest::SetUp();
    }
    void
    TearDown() override
    {
        SyncAsioContextTest::TearDown();
    }
};

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidIndexNotHex)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto account = getAccountIdWithString(kACCOUNT);
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, "nothex,10", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Malformed cursor.");
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, TraverseOwnedNodesMarkerInvalidPageNotInt)
{
    boost::asio::spawn(ctx_, [this](boost::asio::yield_context yield) {
        auto account = getAccountIdWithString(kACCOUNT);
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, "nothex,abc", yield, [](auto) {

        });
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Malformed cursor.");
    });
    ctx_.run();
}

// limit = 10, return 2 objects
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    // return owner index
    ripple::STObject const ownerDir =
        createOwnerDirLedgerObject({ripple::uint256{kINDEX1}, ripple::uint256{kINDEX2}}, kINDEX1);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    // return two payment channel objects
    std::vector<Blob> bbs;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    bbs.push_back(channel1.getSerializer().peekData());
    bbs.push_back(channel1.getSerializer().peekData());
    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx_, [this, &account](boost::asio::yield_context yield) {
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, {}, yield, [](auto) {

        });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(
            cursor->toString(),
            "0000000000000000000000000000000000000000000000000000000000000000,"
            "0"
        );
    });
    ctx_.run();
}

// limit = 10, return 10 objects and marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnSamePageMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto owneDirKk = ripple::keylet::ownerDir(account).key;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    std::vector<Blob> bbs;

    int objectsCount = 11;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 99);
    ON_CALL(*backend_, doFetchLedgerObject(owneDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx_, [this, &account](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*backend_, account, 9, 10, {}, yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, 10);
        EXPECT_EQ(cursor->toString(), fmt::format("{},0", kINDEX1));
    });
    ctx_.run();
}

// 10 objects per page, limit is 15, return the second page as marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesNoInputMarkerReturnOtherPageMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto ownerDirKk = ripple::keylet::ownerDir(account).key;
    static constexpr auto kNEXT_PAGE = 99;
    static constexpr auto kLIMIT = 15;
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), kNEXT_PAGE).key;

    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        objectsCount--;
    }
    objectsCount = 15;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, kNEXT_PAGE);
    // first page 's next page is 99
    ON_CALL(*backend_, doFetchLedgerObject(ownerDirKk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));
    ripple::STObject ownerDir2 = createOwnerDirLedgerObject(indexes, kINDEX1);
    // second page's next page is 0
    ownerDir2.setFieldU64(ripple::sfIndexNext, 0);
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir2.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(*backend_, account, 9, kLIMIT, {}, yield, [&](auto) { count++; });
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, kLIMIT);
        EXPECT_EQ(cursor->toString(), fmt::format("{},{}", kINDEX1, kNEXT_PAGE));
    });
    ctx_.run();
}

// Send a valid marker
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithMarkerReturnSamePageMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), 99).key;
    static constexpr auto kLIMIT = 8;
    static constexpr auto kPAGE_NUM = 99;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(2);

    std::vector<Blob> bbs;

    int objectsCount = 10;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        objectsCount--;
    }
    objectsCount = 10;
    while (objectsCount != 0) {
        bbs.push_back(channel1.getSerializer().peekData());
        objectsCount--;
    }

    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    ON_CALL(*backend_, doFetchLedgerObjects).WillByDefault(Return(bbs));
    EXPECT_CALL(*backend_, doFetchLedgerObjects).Times(1);

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *backend_, account, 9, kLIMIT, fmt::format("{},{}", kINDEX1, kPAGE_NUM), yield, [&](auto) { count++; }
        );
        auto cursor = std::get_if<AccountCursor>(&ret);
        EXPECT_TRUE(cursor != nullptr);
        EXPECT_EQ(count, kLIMIT);
        EXPECT_EQ(cursor->toString(), fmt::format("{},{}", kINDEX1, kPAGE_NUM));
    });
    ctx_.run();
}

// Send a valid marker, but marker contain an unexisting index
// return invalid params error
TEST_F(RPCHelpersTest, TraverseOwnedNodesWithUnexistingIndexMarker)
{
    auto account = getAccountIdWithString(kACCOUNT);
    auto ownerDir2Kk = ripple::keylet::page(ripple::keylet::ownerDir(account), 99).key;
    static constexpr auto kLIMIT = 8;
    static constexpr auto kPAGE_NUM = 99;
    EXPECT_CALL(*backend_, doFetchLedgerObject).Times(1);

    int objectsCount = 10;
    ripple::STObject const channel1 = createPaymentChannelLedgerObject(kACCOUNT, kACCOUNT2, 100, 10, 32, kTXN_ID, 28);
    std::vector<ripple::uint256> indexes;
    while (objectsCount != 0) {
        // return owner index
        indexes.emplace_back(kINDEX1);
        objectsCount--;
    }
    ripple::STObject ownerDir = createOwnerDirLedgerObject(indexes, kINDEX1);
    ownerDir.setFieldU64(ripple::sfIndexNext, 0);
    // return ownerdir when search by marker
    ON_CALL(*backend_, doFetchLedgerObject(ownerDir2Kk, testing::_, testing::_))
        .WillByDefault(Return(ownerDir.getSerializer().peekData()));

    boost::asio::spawn(ctx_, [&, this](boost::asio::yield_context yield) {
        auto count = 0;
        auto ret = traverseOwnedNodes(
            *backend_, account, 9, kLIMIT, fmt::format("{},{}", kINDEX2, kPAGE_NUM), yield, [&](auto) { count++; }
        );
        auto status = std::get_if<Status>(&ret);
        EXPECT_TRUE(status != nullptr);
        EXPECT_EQ(*status, ripple::rpcINVALID_PARAMS);
        EXPECT_EQ(status->message, "Invalid marker.");
    });
    ctx_.run();
}

TEST_F(RPCHelpersTest, EncodeCTID)
{
    auto const ctid = encodeCTID(0x1234, 0x67, 0x89);
    ASSERT_TRUE(ctid);
    EXPECT_EQ(*ctid, "C000123400670089");
    EXPECT_FALSE(encodeCTID(0x1FFFFFFF, 0x67, 0x89));
}

TEST_F(RPCHelpersTest, DecodeCTIDString)
{
    auto const ctid = decodeCTID("C000123400670089");
    ASSERT_TRUE(ctid);
    EXPECT_EQ(*ctid, std::make_tuple(0x1234, 0x67, 0x89));
    EXPECT_FALSE(decodeCTID("F000123400670089"));
    EXPECT_FALSE(decodeCTID("F0001234006700"));
    EXPECT_FALSE(decodeCTID("F000123400*700"));
}

TEST_F(RPCHelpersTest, DecodeCTIDInt)
{
    uint64_t ctidStr = 0xC000123400670089;
    auto const ctid = decodeCTID(ctidStr);
    ASSERT_TRUE(ctid);
    EXPECT_EQ(*ctid, std::make_tuple(0x1234, 0x67, 0x89));
    ctidStr = 0xF000123400670089;
    EXPECT_FALSE(decodeCTID(ctidStr));
}

TEST_F(RPCHelpersTest, DecodeInvalidCTID)
{
    EXPECT_FALSE(decodeCTID('c'));
    EXPECT_FALSE(decodeCTID(true));
}

TEST_F(RPCHelpersTest, DeliverMaxAliasV1)
{
    std::array<std::string, 3> const inputArray = {
        R"({
            "TransactionType": "Payment",
            "Amount": {
                "test": "test"
            }
        })",
        R"({
            "TransactionType": "OfferCreate",
            "Amount": {
                "test": "test"
            }
        })",
        R"({
            "TransactionType": "Payment",
            "Amount1": {
                "test": "test"
            }
        })"
    };

    std::array<std::string, 3> outputArray = {
        R"({
            "TransactionType": "Payment",
            "Amount": {
                "test": "test"
            },
            "DeliverMax": {
                "test": "test"
            }
        })",
        R"({
            "TransactionType": "OfferCreate",
            "Amount": {
                "test": "test"
            }
        })",
        R"({
            "TransactionType": "Payment",
            "Amount1": {
                "test": "test"
            }
        })"
    };

    for (size_t i = 0; i < inputArray.size(); i++) {
        auto req = boost::json::parse(inputArray[i]).as_object();
        insertDeliverMaxAlias(req, 1);
        EXPECT_EQ(req, boost::json::parse(outputArray[i]).as_object());
    }
}

TEST_F(RPCHelpersTest, DeliverMaxAliasV2)
{
    auto req = boost::json::parse(
                   R"({
                        "TransactionType": "Payment",
                        "Amount": {
                            "test": "test"
                        }
                    })"
    )
                   .as_object();

    insertDeliverMaxAlias(req, 2);
    EXPECT_EQ(
        req,
        boost::json::parse(
            R"({
                "TransactionType": "Payment",
                "DeliverMax": {
                    "test": "test"
                }
            })"
        )
    );
}

TEST_F(RPCHelpersTest, LedgerHeaderJson)
{
    auto const ledgerHeader = createLedgerHeader(kINDEX1, 30);
    auto const binJson = toJson(ledgerHeader, true, 1u);

    constexpr auto kEXPECT_BIN = R"({
                                    "ledger_data": "0000001E000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000",
                                    "closed": true
                                })";
    EXPECT_EQ(binJson, boost::json::parse(kEXPECT_BIN));

    auto const expectJson = fmt::format(
        R"({{
            "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "close_flags": 0,
            "close_time": 0,
            "close_time_resolution": 0,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "ledger_hash": "{}",
            "ledger_index": "{}",
            "parent_close_time": 0,
            "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "total_coins": "0",
            "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "closed": true
        }})",
        kINDEX1,
        30
    );
    auto json = toJson(ledgerHeader, false, 1u);
    // remove platform-related close_time_human field
    json.erase(JS(close_time_human));
    EXPECT_EQ(json, boost::json::parse(expectJson));
}

TEST_F(RPCHelpersTest, LedgerHeaderJsonV2)
{
    auto const ledgerHeader = createLedgerHeader(kINDEX1, 30);

    auto const expectJson = fmt::format(
        R"({{
            "account_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "close_flags": 0,
            "close_time": 0,
            "close_time_resolution": 0,
            "close_time_iso": "2000-01-01T00:00:00Z",
            "ledger_hash": "{}",
            "ledger_index": {},
            "parent_close_time": 0,
            "parent_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "total_coins": "0",
            "transaction_hash": "0000000000000000000000000000000000000000000000000000000000000000",
            "closed": true
        }})",
        kINDEX1,
        30
    );
    auto json = toJson(ledgerHeader, false, 2u);
    // remove platform-related close_time_human field
    json.erase(JS(close_time_human));
    EXPECT_EQ(json, boost::json::parse(expectJson));
}

TEST_F(RPCHelpersTest, TransactionAndMetadataBinaryJsonV1)
{
    auto const txMeta = createAcceptNftBuyerOfferTxWithMetadata(kACCOUNT, 30, 1, kINDEX1, kINDEX2);
    auto const json = toJsonWithBinaryTx(txMeta, 1);
    EXPECT_TRUE(json.contains(JS(tx_blob)));
    EXPECT_TRUE(json.contains(JS(meta)));
}

TEST_F(RPCHelpersTest, TransactionAndMetadataBinaryJsonV2)
{
    auto const txMeta = createAcceptNftBuyerOfferTxWithMetadata(kACCOUNT, 30, 1, kINDEX1, kINDEX2);
    auto const json = toJsonWithBinaryTx(txMeta, 2);
    EXPECT_TRUE(json.contains(JS(tx_blob)));
    EXPECT_TRUE(json.contains(JS(meta_blob)));
}

TEST_F(RPCHelpersTest, ParseIssue)
{
    auto issue = parseIssue(boost::json::parse(
                                R"({
                                        "issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun",
                                        "currency": "JPY"
                                    })"
    )
                                .as_object());
    EXPECT_TRUE(issue.account == getAccountIdWithString(kACCOUNT2));

    issue = parseIssue(boost::json::parse(R"({"currency": "XRP"})").as_object());
    EXPECT_TRUE(ripple::isXRP(issue.currency));

    EXPECT_THROW(parseIssue(boost::json::parse(R"({"currency": 2})").as_object()), std::runtime_error);

    EXPECT_THROW(parseIssue(boost::json::parse(R"({"currency": "XRP2"})").as_object()), std::runtime_error);

    EXPECT_THROW(
        parseIssue(boost::json::parse(
                       R"({
                                "issuer": "abcd",
                                "currency": "JPY"
                            })"
        )
                       .as_object()),
        std::runtime_error
    );

    EXPECT_THROW(
        parseIssue(boost::json::parse(R"({"issuer": "rLEsXccBGNR3UPuPu2hUXPjziKC3qKSBun"})").as_object()),
        std::runtime_error
    );
}

struct IsAdminCmdParamTestCaseBundle {
    std::string testName;
    std::string method;
    std::string testJson;
    bool expected;
};

struct IsAdminCmdParameterTest : public TestWithParam<IsAdminCmdParamTestCaseBundle> {};

static auto
generateTestValuesForParametersTest()
{
    return std::vector<IsAdminCmdParamTestCaseBundle>{
        {.testName = "ledgerEntry", .method = "ledger_entry", .testJson = R"({"type": false})", .expected = false},

        {.testName = "featureVetoedTrue",
         .method = "feature",
         .testJson = R"({"vetoed": true, "feature": "foo"})",
         .expected = true},
        {.testName = "featureVetoedFalse",
         .method = "feature",
         .testJson = R"({"vetoed": false, "feature": "foo"})",
         .expected = true},
        {.testName = "featureVetoedIsStr", .method = "feature", .testJson = R"({"vetoed": "String"})", .expected = true
        },

        {.testName = "ledger", .method = "ledger", .testJson = R"({})", .expected = false},
        {.testName = "ledgerWithType", .method = "ledger", .testJson = R"({"type": "fee"})", .expected = false},
        {.testName = "ledgerFullTrue", .method = "ledger", .testJson = R"({"full": true})", .expected = true},
        {.testName = "ledgerFullFalse", .method = "ledger", .testJson = R"({"full": false})", .expected = false},
        {.testName = "ledgerFullIsStr", .method = "ledger", .testJson = R"({"full": "String"})", .expected = true},
        {.testName = "ledgerFullIsEmptyStr", .method = "ledger", .testJson = R"({"full": ""})", .expected = false},
        {.testName = "ledgerFullIsNumber1", .method = "ledger", .testJson = R"({"full": 1})", .expected = true},
        {.testName = "ledgerFullIsNumber0", .method = "ledger", .testJson = R"({"full": 0})", .expected = false},
        {.testName = "ledgerFullIsNull", .method = "ledger", .testJson = R"({"full": null})", .expected = false},
        {.testName = "ledgerFullIsFloat0", .method = "ledger", .testJson = R"({"full": 0.0})", .expected = false},
        {.testName = "ledgerFullIsFloat1", .method = "ledger", .testJson = R"({"full": 0.1})", .expected = true},
        {.testName = "ledgerFullIsArray", .method = "ledger", .testJson = R"({"full": [1]})", .expected = true},
        {.testName = "ledgerFullIsEmptyArray", .method = "ledger", .testJson = R"({"full": []})", .expected = false},
        {.testName = "ledgerFullIsObject", .method = "ledger", .testJson = R"({"full": {"key": 1}})", .expected = true},
        {.testName = "ledgerFullIsEmptyObject", .method = "ledger", .testJson = R"({"full": {}})", .expected = false},

        {.testName = "ledgerAccountsTrue", .method = "ledger", .testJson = R"({"accounts": true})", .expected = true},
        {.testName = "ledgerAccountsFalse", .method = "ledger", .testJson = R"({"accounts": false})", .expected = false
        },
        {.testName = "ledgerAccountsIsStr",
         .method = "ledger",
         .testJson = R"({"accounts": "String"})",
         .expected = true},
        {.testName = "ledgerAccountsIsEmptyStr",
         .method = "ledger",
         .testJson = R"({"accounts": ""})",
         .expected = false},
        {.testName = "ledgerAccountsIsNumber1", .method = "ledger", .testJson = R"({"accounts": 1})", .expected = true},
        {.testName = "ledgerAccountsIsNumber0", .method = "ledger", .testJson = R"({"accounts": 0})", .expected = false
        },
        {.testName = "ledgerAccountsIsNull", .method = "ledger", .testJson = R"({"accounts": null})", .expected = false
        },
        {.testName = "ledgerAccountsIsFloat0", .method = "ledger", .testJson = R"({"accounts": 0.0})", .expected = false
        },
        {.testName = "ledgerAccountsIsFloat1", .method = "ledger", .testJson = R"({"accounts": 0.1})", .expected = true
        },
        {.testName = "ledgerAccountsIsArray", .method = "ledger", .testJson = R"({"accounts": [1]})", .expected = true},
        {.testName = "ledgerAccountsIsEmptyArray",
         .method = "ledger",
         .testJson = R"({"accounts": []})",
         .expected = false},
        {.testName = "ledgerAccountsIsObject",
         .method = "ledger",
         .testJson = R"({"accounts": {"key": 1}})",
         .expected = true},
        {.testName = "ledgerAccountsIsEmptyObject",
         .method = "ledger",
         .testJson = R"({"accounts": {}})",
         .expected = false},
    };
}

INSTANTIATE_TEST_CASE_P(
    IsAdminCmdTest,
    IsAdminCmdParameterTest,
    ValuesIn(generateTestValuesForParametersTest()),
    tests::util::kNAME_GENERATOR
);

TEST_P(IsAdminCmdParameterTest, Test)
{
    auto const testBundle = GetParam();
    EXPECT_EQ(isAdminCmd(testBundle.method, boost::json::parse(testBundle.testJson).as_object()), testBundle.expected);
}
