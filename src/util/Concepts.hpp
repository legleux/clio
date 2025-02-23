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

#include <algorithm>
#include <array>
#include <cstddef>
#include <string_view>
#include <type_traits>

namespace util {

/**
 * @brief Specifies a number type
 */
template <typename T>
concept SomeNumberType = std::is_arithmetic_v<T> && !std::is_same_v<T, bool> && !std::is_const_v<T>;

/**
 * @brief Checks that the list of given values contains no duplicates
 *
 * @param values The list of values to check
 * @returns true if no duplicates exist; false otherwise
 */
static consteval auto
hasNoDuplicates(auto&&... values)
{
    auto store = std::array{values...};
    auto end = store.end();
    std::ranges::sort(store);
    return (std::unique(std::begin(store), end) == end);
}

/**
 * @brief Checks that the list of given type contains no duplicates
 *
 * @tparam Types The types to check
 * @returns true if no duplicates exist; false otherwise
 */
template <typename... Types>
constexpr bool
hasNoDuplicateNames()
{
    constexpr std::array<std::string_view, sizeof...(Types)> kNAMES = {Types::kNAME...};
    return !std::ranges::any_of(kNAMES, [&](std::string_view const& name1) {
        return std::ranges::any_of(kNAMES, [&](std::string_view const& name2) {
            return &name1 != &name2 && name1 == name2;  // Ensure different elements are compared
        });
    });
}

}  // namespace util
