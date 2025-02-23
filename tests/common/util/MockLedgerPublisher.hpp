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

#pragma once

#include <gmock/gmock.h>
#include <xrpl/protocol/LedgerHeader.h>

#include <chrono>
#include <cstdint>
#include <optional>

struct MockLedgerPublisher {
    MOCK_METHOD(bool, publish, (uint32_t, std::optional<uint32_t>), ());
    MOCK_METHOD(void, publish, (ripple::LedgerHeader const&), ());
    MOCK_METHOD(std::uint32_t, lastPublishAgeSeconds, (), (const));
    MOCK_METHOD(std::chrono::time_point<std::chrono::system_clock>, getLastPublish, (), (const));
    MOCK_METHOD(std::uint32_t, lastCloseAgeSeconds, (), (const));
    MOCK_METHOD(std::optional<uint32_t>, getLastPublishedSequence, (), (const));
};
