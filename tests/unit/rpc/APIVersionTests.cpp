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

#include "rpc/common/impl/APIVersionParser.hpp"
#include "util/LoggerFixtures.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"

#include <boost/json/parse.hpp>
#include <gtest/gtest.h>

namespace {

constexpr auto kDEFAULT_API_VERSION = 5u;
constexpr auto kMIN_API_VERSION = 2u;
constexpr auto kMAX_API_VERSION = 10u;

}  // namespace

using namespace util::config;
using namespace rpc::impl;
namespace json = boost::json;

class RPCAPIVersionTest : public NoLoggerFixture {
protected:
    ProductionAPIVersionParser parser_{kDEFAULT_API_VERSION, kMIN_API_VERSION, kMAX_API_VERSION};
};

TEST_F(RPCAPIVersionTest, ReturnsDefaultVersionIfNotSpecified)
{
    auto ver = parser_.parse(json::parse("{}").as_object());
    EXPECT_TRUE(ver);
    EXPECT_EQ(ver.value(), kDEFAULT_API_VERSION);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorIfVersionHigherThanMaxSupported)
{
    auto ver = parser_.parse(json::parse(R"({"api_version": 11})").as_object());
    EXPECT_FALSE(ver);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorIfVersionLowerThanMinSupported)
{
    auto ver = parser_.parse(json::parse(R"({"api_version": 1})").as_object());
    EXPECT_FALSE(ver);
}

TEST_F(RPCAPIVersionTest, ReturnsErrorOnWrongType)
{
    {
        auto ver = parser_.parse(json::parse(R"({"api_version": null})").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = parser_.parse(json::parse(R"({"api_version": "5"})").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = parser_.parse(json::parse(R"({"api_version": "wrong"})").as_object());
        EXPECT_FALSE(ver);
    }
}

TEST_F(RPCAPIVersionTest, ReturnsParsedVersionIfAllPreconditionsAreMet)
{
    {
        auto ver = parser_.parse(json::parse(R"({"api_version": 2})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 2u);
    }
    {
        auto ver = parser_.parse(json::parse(R"({"api_version": 10})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 10u);
    }
    {
        auto ver = parser_.parse(json::parse(R"({"api_version": 5})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 5u);
    }
}

TEST_F(RPCAPIVersionTest, GetsValuesFromConfigCorrectly)
{
    ClioConfigDefinition const cfg{
        {"api_version.min", ConfigValue{ConfigType::Integer}.defaultValue(kMIN_API_VERSION)},
        {"api_version.max", ConfigValue{ConfigType::Integer}.defaultValue(kMAX_API_VERSION)},
        {"api_version.default", ConfigValue{ConfigType::Integer}.defaultValue(kDEFAULT_API_VERSION)}
    };

    ProductionAPIVersionParser const configuredParser{cfg.getObject("api_version")};

    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 2})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 2u);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 10})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 10u);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 5})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), 5u);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({})").as_object());
        EXPECT_TRUE(ver);
        EXPECT_EQ(ver.value(), kDEFAULT_API_VERSION);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 11})").as_object());
        EXPECT_FALSE(ver);
    }
    {
        auto ver = configuredParser.parse(json::parse(R"({"api_version": 1})").as_object());
        EXPECT_FALSE(ver);
    }
}
