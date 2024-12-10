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

#pragma once

#include "web/dosguard/DOSGuardInterface.hpp"

#include <gmock/gmock.h>

#include <cstdint>
#include <string>
#include <string_view>

struct DOSGuardMockImpl : web::dosguard::DOSGuardInterface {
    MOCK_METHOD(bool, isWhiteListed, (std::string_view const ip), (const, noexcept, override));
    MOCK_METHOD(bool, isOk, (std::string const& ip), (const, noexcept, override));
    MOCK_METHOD(void, increment, (std::string const& ip), (noexcept, override));
    MOCK_METHOD(void, decrement, (std::string const& ip), (noexcept, override));
    MOCK_METHOD(bool, add, (std::string const& ip, uint32_t size), (noexcept, override));
    MOCK_METHOD(bool, request, (std::string const& ip), (noexcept, override));
    MOCK_METHOD(void, clear, (), (noexcept, override));
};

using DOSGuardMock = testing::NiceMock<DOSGuardMockImpl>;
using DOSGuardStrictMock = testing::StrictMock<DOSGuardMockImpl>;
