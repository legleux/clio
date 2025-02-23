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

#include "util/LoggerFixtures.hpp"
#include "util/NameGenerator.hpp"
#include "util/newconfig/ConfigDefinition.hpp"
#include "util/newconfig/ConfigFileJson.hpp"
#include "util/newconfig/ConfigValue.hpp"
#include "util/newconfig/Types.hpp"
#include "web/AdminVerificationStrategy.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/value.hpp>
#include <gtest/gtest.h>

#include <optional>
#include <string>

namespace http = boost::beast::http;
using namespace util::config;

class IPAdminVerificationStrategyTest : public NoLoggerFixture {
protected:
    web::IPAdminVerificationStrategy strat_;
    http::request<http::string_body> request_;
};

TEST_F(IPAdminVerificationStrategyTest, IsAdminOnlyForIP_127_0_0_1)
{
    EXPECT_TRUE(strat_.isAdmin(request_, "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(request_, "127.0.0.2"));
    EXPECT_FALSE(strat_.isAdmin(request_, "127"));
    EXPECT_FALSE(strat_.isAdmin(request_, ""));
    EXPECT_FALSE(strat_.isAdmin(request_, "localhost"));
}

class PasswordAdminVerificationStrategyTest : public NoLoggerFixture {
protected:
    std::string const password_ = "secret";
    std::string const passwordHash_ = "2bb80d537b1da3e38bd30361aa855686bde0eacd7162fef6a25fe97bf527a25b";

    web::PasswordAdminVerificationStrategy strat_{password_};

    static http::request<http::string_body>
    makeRequest(std::string const& password, http::field const field = http::field::authorization)
    {
        http::request<http::string_body> request = {};
        request.set(field, "Password " + password);
        return request;
    }
};

TEST_F(PasswordAdminVerificationStrategyTest, IsAdminReturnsTrueOnlyForValidPasswordInAuthHeader)
{
    EXPECT_TRUE(strat_.isAdmin(makeRequest(passwordHash_), ""));
    EXPECT_TRUE(strat_.isAdmin(makeRequest(passwordHash_), "123"));

    // Wrong password
    EXPECT_FALSE(strat_.isAdmin(makeRequest("SECRET"), ""));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("SECRET"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("S"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("SeCret"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("secre"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("s"), "127.0.0.1"));
    EXPECT_FALSE(strat_.isAdmin(makeRequest("a"), "127.0.0.1"));

    // Wrong header
    EXPECT_FALSE(strat_.isAdmin(makeRequest(passwordHash_, http::field::authentication_info), ""));
}

struct MakeAdminVerificationStrategyTestParams {
    std::string testName;
    std::optional<std::string> passwordOpt;
    bool expectIpStrategy;
    bool expectPasswordStrategy;
};

class MakeAdminVerificationStrategyTest : public testing::TestWithParam<MakeAdminVerificationStrategyTestParams> {};

TEST_P(MakeAdminVerificationStrategyTest, ChoosesStrategyCorrectly)
{
    auto strat = web::makeAdminVerificationStrategy(GetParam().passwordOpt);
    auto ipStrat = dynamic_cast<web::IPAdminVerificationStrategy*>(strat.get());
    EXPECT_EQ(ipStrat != nullptr, GetParam().expectIpStrategy);
    auto passwordStrat = dynamic_cast<web::PasswordAdminVerificationStrategy*>(strat.get());
    EXPECT_EQ(passwordStrat != nullptr, GetParam().expectPasswordStrategy);
}

INSTANTIATE_TEST_CASE_P(
    MakeAdminVerificationStrategyTest,
    MakeAdminVerificationStrategyTest,
    testing::Values(
        MakeAdminVerificationStrategyTestParams{
            .testName = "NoPassword",
            .passwordOpt = std::nullopt,
            .expectIpStrategy = true,
            .expectPasswordStrategy = false
        },
        MakeAdminVerificationStrategyTestParams{
            .testName = "HasPassword",
            .passwordOpt = "p",
            .expectIpStrategy = false,
            .expectPasswordStrategy = true
        },
        MakeAdminVerificationStrategyTestParams{
            .testName = "EmptyPassword",
            .passwordOpt = "",
            .expectIpStrategy = false,
            .expectPasswordStrategy = true
        }
    )
);

struct MakeAdminVerificationStrategyFromConfigTestParams {
    std::string testName;
    std::string config;
    bool expectedError;
};

struct MakeAdminVerificationStrategyFromConfigTest
    : public testing::TestWithParam<MakeAdminVerificationStrategyFromConfigTestParams> {};

inline static ClioConfigDefinition
generateDefaultAdminConfig()
{
    return ClioConfigDefinition{
        {{"server.local_admin", ConfigValue{ConfigType::Boolean}.optional()},
         {"server.admin_password", ConfigValue{ConfigType::String}.optional()}}
    };
}

TEST_P(MakeAdminVerificationStrategyFromConfigTest, ChecksConfig)
{
    ConfigFileJson const js{boost::json::parse(GetParam().config).as_object()};
    ClioConfigDefinition serverConfig{generateDefaultAdminConfig()};
    auto const errors = serverConfig.parse(js);
    ASSERT_TRUE(!errors.has_value());
    auto const result = web::makeAdminVerificationStrategy(serverConfig);
    EXPECT_EQ(not result.has_value(), GetParam().expectedError);
}

INSTANTIATE_TEST_SUITE_P(
    MakeAdminVerificationStrategyFromConfigTest,
    MakeAdminVerificationStrategyFromConfigTest,
    testing::Values(
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "NoPasswordNoLocalAdmin",
            .config = "{}",
            .expectedError = false
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "OnlyPassword",
            .config = R"({"server": {"admin_password": "password"}})",
            .expectedError = false
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "OnlyLocalAdmin",
            .config = R"({"server": {"local_admin": true}})",
            .expectedError = false
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "OnlyLocalAdminDisabled",
            .config = R"({"server": {"local_admin": false}})",
            .expectedError = true
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "LocalAdminAndPassword",
            .config = R"({"server": {"local_admin": true, "admin_password": "password"}})",
            .expectedError = true
        },
        MakeAdminVerificationStrategyFromConfigTestParams{
            .testName = "LocalAdminDisabledAndPassword",
            .config = R"({"server": {"local_admin": false, "admin_password": "password"}})",
            .expectedError = false
        }
    ),
    tests::util::kNAME_GENERATOR
);
