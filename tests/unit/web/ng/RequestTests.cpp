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

#include "util/NameGenerator.hpp"
#include "web/ng/Request.hpp"

#include <boost/beast/http/field.hpp>
#include <boost/beast/http/message.hpp>
#include <boost/beast/http/string_body.hpp>
#include <boost/beast/http/verb.hpp>
#include <gtest/gtest.h>

#include <iterator>
#include <optional>
#include <string>
#include <utility>

using namespace web::ng;
namespace http = boost::beast::http;

struct RequestTest : public ::testing::Test {
    static Request::HttpHeaders const kHEADERS;
};
Request::HttpHeaders const RequestTest::kHEADERS = {};

struct RequestMethodTestBundle {
    std::string testName;
    Request request;
    Request::Method expectedMethod;
};

struct RequestMethodTest : RequestTest, ::testing::WithParamInterface<RequestMethodTestBundle> {};

TEST_P(RequestMethodTest, method)
{
    EXPECT_EQ(GetParam().request.method(), GetParam().expectedMethod);
}

INSTANTIATE_TEST_SUITE_P(
    RequestMethodTest,
    RequestMethodTest,
    testing::Values(
        RequestMethodTestBundle{
            .testName = "HttpGet",
            .request = Request{http::request<http::string_body>{http::verb::get, "/", 11}},
            .expectedMethod = Request::Method::Get,
        },
        RequestMethodTestBundle{
            .testName = "HttpPost",
            .request = Request{http::request<http::string_body>{http::verb::post, "/", 11}},
            .expectedMethod = Request::Method::Post,
        },
        RequestMethodTestBundle{
            .testName = "WebSocket",
            .request = Request{"websocket message", RequestTest::kHEADERS},
            .expectedMethod = Request::Method::Websocket,
        },
        RequestMethodTestBundle{
            .testName = "Unsupported",
            .request = Request{http::request<http::string_body>{http::verb::acl, "/", 11}},
            .expectedMethod = Request::Method::Unsupported,
        }
    ),
    tests::util::kNAME_GENERATOR
);

struct RequestIsHttpTestBundle {
    std::string testName;
    Request request;
    bool expectedIsHttp;
};

struct RequestIsHttpTest : RequestTest, testing::WithParamInterface<RequestIsHttpTestBundle> {};

TEST_P(RequestIsHttpTest, isHttp)
{
    EXPECT_EQ(GetParam().request.isHttp(), GetParam().expectedIsHttp);
}

INSTANTIATE_TEST_SUITE_P(
    RequestIsHttpTest,
    RequestIsHttpTest,
    testing::Values(
        RequestIsHttpTestBundle{
            .testName = "HttpRequest",
            .request = Request{http::request<http::string_body>{http::verb::get, "/", 11}},
            .expectedIsHttp = true,
        },
        RequestIsHttpTestBundle{
            .testName = "WebSocketRequest",
            .request = Request{"websocket message", RequestTest::kHEADERS},
            .expectedIsHttp = false,
        }
    ),
    tests::util::kNAME_GENERATOR
);

struct RequestAsHttpRequestTest : RequestTest {};

TEST_F(RequestAsHttpRequestTest, HttpRequest)
{
    http::request<http::string_body> const httpRequest{http::verb::get, "/some", 11};
    Request const request{httpRequest};
    auto const maybeHttpRequest = request.asHttpRequest();
    ASSERT_TRUE(maybeHttpRequest.has_value());
    auto const& actualHttpRequest = maybeHttpRequest->get();
    EXPECT_EQ(actualHttpRequest.method(), httpRequest.method());
    EXPECT_EQ(actualHttpRequest.target(), httpRequest.target());
    EXPECT_EQ(actualHttpRequest.version(), httpRequest.version());
}

TEST_F(RequestAsHttpRequestTest, WebSocketRequest)
{
    Request const request{"websocket message", RequestTest::kHEADERS};
    auto const maybeHttpRequest = request.asHttpRequest();
    EXPECT_FALSE(maybeHttpRequest.has_value());
}

struct RequestMessageTest : RequestTest {};

TEST_F(RequestMessageTest, HttpRequest)
{
    std::string const body = "some body";
    http::request<http::string_body> const httpRequest{http::verb::post, "/some", 11, body};
    Request const request{httpRequest};
    EXPECT_EQ(request.message(), httpRequest.body());
}

TEST_F(RequestMessageTest, WebSocketRequest)
{
    std::string const message = "websocket message";
    Request const request{message, RequestTest::kHEADERS};
    EXPECT_EQ(request.message(), message);
}

struct RequestTargetTestBundle {
    std::string testName;
    Request request;
    std::optional<std::string> expectedTarget;
};

struct RequestTargetTest : RequestTest, ::testing::WithParamInterface<RequestTargetTestBundle> {};

TEST_P(RequestTargetTest, target)
{
    auto const maybeTarget = GetParam().request.target();
    EXPECT_EQ(maybeTarget, GetParam().expectedTarget);
}

INSTANTIATE_TEST_SUITE_P(
    RequestTargetTest,
    RequestTargetTest,
    testing::Values(
        RequestTargetTestBundle{
            .testName = "HttpRequest",
            .request = Request{http::request<http::string_body>{http::verb::get, "/some", 11}},
            .expectedTarget = "/some",
        },
        RequestTargetTestBundle{
            .testName = "WebSocketRequest",
            .request = Request{"websocket message", RequestTest::kHEADERS},
            .expectedTarget = std::nullopt,
        }
    ),
    tests::util::kNAME_GENERATOR
);

struct RequestHttpHeadersTest : RequestTest {
protected:
    http::field const headerName_ = http::field::user_agent;
    std::string const headerValue_ = "clio";
};

TEST_F(RequestHttpHeadersTest, httpHeaders_HttpRequest)
{
    auto httpRequest = http::request<http::string_body>{http::verb::get, "/", 11};
    httpRequest.set(headerName_, headerValue_);
    Request const request{std::move(httpRequest)};

    auto const& headersFromRequest = request.httpHeaders();
    ASSERT_EQ(headersFromRequest.count(headerName_), 1);
    ASSERT_EQ(std::distance(headersFromRequest.cbegin(), headersFromRequest.cend()), 1);
    EXPECT_EQ(headersFromRequest.at(headerName_), headerValue_);
}

TEST_F(RequestHttpHeadersTest, httpHeaders_WsRequest)
{
    Request::HttpHeaders headers;
    headers.set(headerName_, headerValue_);
    Request const request{"websocket message", headers};

    auto const& headersFromRequest = request.httpHeaders();
    ASSERT_EQ(std::distance(headersFromRequest.cbegin(), headersFromRequest.cend()), 1);
    ASSERT_EQ(headersFromRequest.count(headerName_), 1);
    EXPECT_EQ(headersFromRequest.at(headerName_), headerValue_);
}

struct RequestHeaderValueTest : RequestTest {};

TEST_F(RequestHeaderValueTest, headerValue)
{
    http::request<http::string_body> httpRequest{http::verb::get, "/some", 11};
    http::field const headerName = http::field::user_agent;
    std::string const headerValue = "clio";
    httpRequest.set(headerName, headerValue);

    Request const request{httpRequest};
    auto const maybeHeaderValue = request.headerValue(headerName);
    ASSERT_TRUE(maybeHeaderValue.has_value());
    EXPECT_EQ(maybeHeaderValue.value(), headerValue);
}

TEST_F(RequestHeaderValueTest, headerValueString)
{
    http::request<http::string_body> httpRequest{http::verb::get, "/some", 11};
    std::string const headerName = "Custom";
    std::string const headerValue = "some value";
    httpRequest.set(headerName, headerValue);
    Request const request{httpRequest};
    auto const maybeHeaderValue = request.headerValue(headerName);
    ASSERT_TRUE(maybeHeaderValue.has_value());
    EXPECT_EQ(maybeHeaderValue.value(), headerValue);
}

TEST_F(RequestHeaderValueTest, headerValueNotFound)
{
    http::request<http::string_body> const httpRequest{http::verb::get, "/some", 11};
    Request const request{httpRequest};
    auto const maybeHeaderValue = request.headerValue(http::field::user_agent);
    EXPECT_FALSE(maybeHeaderValue.has_value());
}

TEST_F(RequestHeaderValueTest, headerValueWebsocketRequest)
{
    Request::HttpHeaders headers;
    http::field const headerName = http::field::user_agent;
    std::string const headerValue = "clio";
    headers.set(headerName, headerValue);
    Request const request{"websocket message", headers};
    auto const maybeHeaderValue = request.headerValue(headerName);
    ASSERT_TRUE(maybeHeaderValue.has_value());
    EXPECT_EQ(maybeHeaderValue.value(), headerValue);
}
