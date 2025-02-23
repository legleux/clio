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

#include "rpc/common/APIVersion.hpp"
#include "util/log/Logger.hpp"
#include "util/newconfig/Error.hpp"
#include "util/newconfig/Types.hpp"

#include <fmt/core.h>
#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <variant>

namespace util::config {
class ValueView;
class ConfigValue;

/**
 * @brief specific values that are accepted for logger levels in config.
 */
static constexpr std::array<char const*, 7> kLOG_LEVELS = {
    "trace",
    "debug",
    "info",
    "warning",
    "error",
    "fatal",
    "count",
};

/**
 * @brief specific values that are accepted for logger tag style in config.
 */
static constexpr std::array<char const*, 5> kLOG_TAGS = {
    "int",
    "uint",
    "null",
    "none",
    "uuid",
};

/**
 * @brief specific values that are accepted for cache loading in config.
 */
static constexpr std::array<char const*, 3> kLOAD_CACHE_MODE = {
    "sync",
    "async",
    "none",
};

/**
 * @brief specific values that are accepted for database type in config.
 */
static constexpr std::array<char const*, 1> kDATABASE_TYPE = {"cassandra"};

/**
 * @brief specific values that are accepted for server's processing_policy in config.
 */
static constexpr std::array<char const*, 2> kPROCESSING_POLICY = {"parallel", "sequent"};

/**
 * @brief An interface to enforce constraints on certain values within ClioConfigDefinition.
 */
class Constraint {
public:
    constexpr virtual ~Constraint() noexcept = default;

    /**
     * @brief Check if the value meets the specific constraint.
     *
     * @param val The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]]
    std::optional<Error>
    checkConstraint(Value const& val) const
    {
        if (auto const maybeError = checkTypeImpl(val); maybeError.has_value())
            return maybeError;
        return checkValueImpl(val);
    }

protected:
    /**
     * @brief Creates an error message for all constraints that must satisfy certain hard-coded values.
     *
     * @tparam arrSize, the size of the array of hardcoded values
     * @param key The key to the value
     * @param value The value the user provided
     * @param arr The array with hard-coded values to add to error message
     * @return The error message specifying what the value of key must be
     */
    template <std::size_t ArrSize>
    constexpr std::string
    makeErrorMsg(std::string_view key, Value const& value, std::array<char const*, ArrSize> arr) const
    {
        // Extract the value from the variant
        auto const valueStr = std::visit([](auto const& v) { return fmt::format("{}", v); }, value);

        // Create the error message
        return fmt::format(
            R"(You provided value "{}". Key "{}"'s value must be one of the following: {})",
            valueStr,
            key,
            fmt::join(arr, ", ")
        );
    }

    /**
     * @brief Check if the value is of a correct type for the constraint.
     *
     * @param val The value type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    virtual std::optional<Error>
    checkTypeImpl(Value const& val) const = 0;

    /**
     * @brief Check if the value is within the constraint.
     *
     * @param val The value type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    virtual std::optional<Error>
    checkValueImpl(Value const& val) const = 0;
};

/**
 * @brief A constraint to ensure the port number is within a valid range.
 */
class PortConstraint final : public Constraint {
public:
    constexpr ~PortConstraint() noexcept override = default;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param port The type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& port) const override;

    /**
     * @brief Check if the value is within the constraint.
     *
     * @param port The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& port) const override;

    static constexpr uint32_t kPORT_MIN = 1;
    static constexpr uint32_t kPORT_MAX = 65535;
};

/**
 * @brief A constraint to ensure the IP address is valid.
 */
class ValidIPConstraint final : public Constraint {
public:
    constexpr ~ValidIPConstraint() noexcept override = default;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param ip The type to be checked.
     * @return An optional Error if the constraint is not met, std::nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& ip) const override;

    /**
     * @brief Check if the value is within the constraint.
     *
     * @param ip The value to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& ip) const override;
};

/**
 * @brief A constraint class to ensure the provided value is one of the specified values in an array.
 *
 * @tparam arrSize The size of the array containing the valid values for the constraint
 */
template <std::size_t ArrSize>
class OneOf final : public Constraint {
public:
    /**
     * @brief Constructs a constraint where the value must be one of the values in the provided array.
     *
     * @param key The key of the ConfigValue that has this constraint
     * @param arr The value that has this constraint must be of the values in arr
     */
    constexpr OneOf(std::string_view key, std::array<char const*, ArrSize> arr) : key_{key}, arr_{arr}
    {
    }

    constexpr ~OneOf() noexcept override = default;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param val The type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& val) const override
    {
        if (!std::holds_alternative<std::string>(val))
            return Error{fmt::format(R"(Key "{}"'s value must be a string)", key_)};
        return std::nullopt;
    }

    /**
     * @brief Check if the value matches one of the value in the provided array
     *
     * @param val The value to check
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& val) const override
    {
        namespace rg = std::ranges;
        auto const check = [&val](std::string_view name) { return std::get<std::string>(val) == name; };
        if (rg::any_of(arr_, check))
            return std::nullopt;

        return Error{makeErrorMsg(key_, val, arr_)};
    }

    std::string_view key_;
    std::array<char const*, ArrSize> arr_;
};

/**
 * @brief A constraint class to ensure an integer value is between two numbers (inclusive)
 */
template <typename NumType>
class NumberValueConstraint final : public Constraint {
public:
    /**
     * @brief Constructs a constraint where the number must be between min_ and max_.
     *
     * @param min the minimum number it can be to satisfy this constraint
     * @param max the maximum number it can be to satisfy this constraint
     */
    constexpr NumberValueConstraint(NumType min, NumType max) : min_{min}, max_{max}
    {
    }

    constexpr ~NumberValueConstraint() noexcept override = default;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param num The type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& num) const override
    {
        if (!std::holds_alternative<int64_t>(num))
            return Error{"Number must be of type integer"};
        return std::nullopt;
    }

    /**
     * @brief Check if the number is positive.
     *
     * @param num The number to check
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& num) const override
    {
        auto const numValue = std::get<int64_t>(num);
        if (numValue >= static_cast<int64_t>(min_) && numValue <= static_cast<int64_t>(max_))
            return std::nullopt;
        return Error{fmt::format("Number must be between {} and {}", min_, max_)};
    }

    NumType min_;
    NumType max_;
};

/**
 * @brief A constraint to ensure a double number is positive
 */
class PositiveDouble final : public Constraint {
public:
    constexpr ~PositiveDouble() noexcept override = default;

private:
    /**
     * @brief Check if the type of the value is correct for this specific constraint.
     *
     * @param num The type to be checked
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkTypeImpl(Value const& num) const override;

    /**
     * @brief Check if the number is positive.
     *
     * @param num The number to check
     * @return An Error object if the constraint is not met, nullopt otherwise
     */
    [[nodiscard]] std::optional<Error>
    checkValueImpl(Value const& num) const override;
};

static constinit PortConstraint gValidatePort{};
static constinit ValidIPConstraint gValidateIp{};

static constinit OneOf gValidateChannelName{"channel", Logger::kCHANNELS};
static constinit OneOf gValidateLogLevelName{"log_level", kLOG_LEVELS};
static constinit OneOf gValidateCassandraName{"database.type", kDATABASE_TYPE};
static constinit OneOf gValidateLoadMode{"cache.load", kLOAD_CACHE_MODE};
static constinit OneOf gValidateLogTag{"log_tag_style", kLOG_TAGS};
static constinit OneOf gValidateProcessingPolicy{"server.processing_policy", kPROCESSING_POLICY};

static constinit PositiveDouble gValidatePositiveDouble{};

static constinit NumberValueConstraint<uint32_t> gValidateNumMarkers{1, 256};
static constinit NumberValueConstraint<uint32_t> gValidateIOThreads{1, std::numeric_limits<uint16_t>::max()};

static constinit NumberValueConstraint<uint16_t> gValidateUint16{
    std::numeric_limits<uint16_t>::min(),
    std::numeric_limits<uint16_t>::max()
};

// log file size minimum is 1mb, log rotation time minimum is 1hr
static constinit NumberValueConstraint<uint32_t> gValidateLogSize{1, std::numeric_limits<uint32_t>::max()};
static constinit NumberValueConstraint<uint32_t> gValidateLogRotationTime{1, std::numeric_limits<uint32_t>::max()};
static constinit NumberValueConstraint<uint32_t> gValidateUint32{
    std::numeric_limits<uint32_t>::min(),
    std::numeric_limits<uint32_t>::max()
};
static constinit NumberValueConstraint<uint32_t> gValidateApiVersion{rpc::kAPI_VERSION_MIN, rpc::kAPI_VERSION_MAX};

}  // namespace util::config
