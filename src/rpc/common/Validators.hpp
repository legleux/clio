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

#include "rpc/Errors.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/ValidationHelpers.hpp"

#include <boost/json/array.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <fmt/core.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/ErrorCodes.h>

#include <concepts>
#include <ctime>
#include <functional>
#include <initializer_list>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rpc::validation {

/**
 * @brief A validator that simply requires a field to be present.
 */
struct Required final {
    /**
     * @brief Verify that the JSON value is present and not null.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return An error if validation failed; otherwise no error is returned
     */
    [[nodiscard]] static MaybeError
    verify(boost::json::value const& value, std::string_view key);
};

/**
 * @brief A validator that forbids a field to be present.
 *
 * If there is a value provided, it will forbid the field only when the value equals.
 * If there is no value provided, it will forbid the field when the field shows up.
 */
template <typename... T>
class NotSupported;

/**
 * @brief A specialized NotSupported validator that forbids a field to be present when the value equals the given value.
 */
template <typename T>
class NotSupported<T> final {
    T value_;

public:
    /**
     * @brief Constructs a new NotSupported validator.
     *
     * @param val The value to store and verify against
     */
    NotSupported(T val) : value_(val)
    {
    }

    /**
     * @brief Verify whether the field is supported or not.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcNOT_SUPPORTED` if the value matched; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (value.is_object() and value.as_object().contains(key)) {
            using boost::json::value_to;
            auto const res = value_to<T>(value.as_object().at(key));
            if (value_ == res) {
                return Error{Status{
                    RippledError::rpcNOT_SUPPORTED,
                    fmt::format("Not supported field '{}'s value '{}'", std::string{key}, res)
                }};
            }
        }
        return {};
    }
};

/**
 * @brief A specialized NotSupported validator that forbids a field to be present.
 */
template <>
class NotSupported<> final {
public:
    /**
     * @brief Verify whether the field is supported or not.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcNOT_SUPPORTED` if the field is found; otherwise no error is returned
     */
    [[nodiscard]] static MaybeError
    verify(boost::json::value const& value, std::string_view key)
    {
        if (value.is_object() and value.as_object().contains(key))
            return Error{Status{RippledError::rpcNOT_SUPPORTED, "Not supported field '" + std::string{key} + '\''}};

        return {};
    }
};

/**
 * @brief Deduction guide to avoid having to specify the template arguments.
 */
template <typename... T>
NotSupported(T&&... t) -> NotSupported<T...>;

/**
 * @brief Validates that the type of the value is one of the given types.
 */
template <typename... Types>
struct Type final {
    /**
     * @brief Verify that the JSON value is (one) of specified type(s).
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. If field is supposed to exist, let 'required' fail instead

        auto const& res = value.as_object().at(key);
        auto const convertible = (checkType<Types>(res) || ...);

        if (not convertible)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validate that value is between specified min and max.
 */
template <typename Type>
class Between final {
    Type min_;
    Type max_;

public:
    /**
     * @brief Construct the validator storing min and max values.
     *
     * @param min
     * @param max
     */
    explicit Between(Type min, Type max) : min_{min}, max_{max}
    {
    }

    /**
     * @brief Verify that the JSON value is within a certain range.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));

        // TODO: may want a way to make this code more generic (e.g. use a free
        // function that can be overridden for this comparison)
        if (res < min_ || res > max_)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validate that value is equal or greater than the specified min.
 */
template <typename Type>
class Min final {
    Type min_;

public:
    /**
     * @brief Construct the validator storing min value.
     *
     * @param min
     */
    explicit Min(Type min) : min_{min}
    {
    }

    /**
     * @brief Verify that the JSON value is not smaller than min
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));

        if (res < min_)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validate that value is not greater than max.
 */
template <typename Type>
class Max final {
    Type max_;

public:
    /**
     * @brief Construct the validator storing max value.
     *
     * @param max
     */
    explicit Max(Type max) : max_{max}
    {
    }

    /**
     * @brief Verify that the JSON value is not greater than max.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));

        if (res > max_)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Validate that value can be converted to time according to the given format.
 */
class TimeFormatValidator final {
    std::string format_;

public:
    /**
     * @brief Construct the validator storing format value.
     *
     * @param format The format to use for time conversion
     */
    explicit TimeFormatValidator(std::string format) : format_{std::move(format)}
    {
    }

    /**
     * @brief Verify that the JSON value is valid formatted time.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief Validates that the value is equal to the one passed in.
 */
template <typename Type>
class EqualTo final {
    Type original_;

public:
    /**
     * @brief Construct the validator with stored original value.
     *
     * @param original The original value to store
     */
    explicit EqualTo(Type original) : original_{std::move(original)}
    {
    }

    /**
     * @brief Verify that the JSON value is equal to the stored original.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));
        if (res != original_)
            return Error{Status{RippledError::rpcINVALID_PARAMS}};

        return {};
    }
};

/**
 * @brief Deduction guide to help disambiguate what it means to EqualTo a "string" without specifying the type.
 */
EqualTo(char const*) -> EqualTo<std::string>;

/**
 * @brief Validates that the value is one of the values passed in.
 */
template <typename Type>
class OneOf final {
    std::vector<Type> options_;

public:
    /**
     * @brief Construct the validator with stored options of initializer list.
     *
     * @param options The list of allowed options
     */
    explicit OneOf(std::initializer_list<Type> options) : options_{options}
    {
    }

    /**
     * @brief Construct the validator with stored options of other container.
     *
     * @param begin,end the range to copy the elements from
     */
    template <typename InputIt>
    explicit OneOf(InputIt begin, InputIt end) : options_{begin, end}
    {
    }

    /**
     * @brief Verify that the JSON value is one of the stored options.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const
    {
        using boost::json::value_to;

        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. field does not exist, let 'required' fail instead

        auto const res = value_to<Type>(value.as_object().at(key));
        if (std::find(std::begin(options_), std::end(options_), res) == std::end(options_))
            return Error{Status{RippledError::rpcINVALID_PARAMS, fmt::format("Invalid field '{}'.", key)}};

        return {};
    }
};

/**
 * @brief Deduction guide to help disambiguate what it means to OneOf a few "strings" without specifying the type.
 */
OneOf(std::initializer_list<char const*>) -> OneOf<std::string>;

/**
 * @brief A meta-validator that allows to specify a custom validation function.
 */
class CustomValidator final {
    std::function<MaybeError(boost::json::value const&, std::string_view)> validator_;

public:
    /**
     * @brief Constructs a custom validator from any supported callable.
     *
     * @tparam Fn The type of callable
     * @param fn The callable/function object
     */
    template <typename Fn>
        requires std::invocable<Fn, boost::json::value const&, std::string_view>
    explicit CustomValidator(Fn&& fn) : validator_{std::forward<Fn>(fn)}
    {
    }

    /**
     * @brief Verify that the JSON value is valid according to the custom validation function stored.
     *
     * @param value The JSON value representing the outer object
     * @param key The key used to retrieve the tested value from the outer object
     * @return Any compatible user-provided error if validation failed; otherwise no error is returned
     */
    [[nodiscard]] MaybeError
    verify(boost::json::value const& value, std::string_view key) const;
};

/**
 * @brief Helper function to check if input value is an uint32 number or not.
 *
 * @param sv The input value as a string_view
 * @return true if the string can be converted to a uint32; false otherwise
 */
[[nodiscard]] bool
checkIsU32Numeric(std::string_view sv);

template <class HexType>
    requires(std::is_same_v<HexType, ripple::uint160> || std::is_same_v<HexType, ripple::uint192> || std::is_same_v<HexType, ripple::uint256>)
MaybeError
makeHexStringValidator(boost::json::value const& value, std::string_view key)
{
    if (!value.is_string())
        return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "NotString"}};

    HexType parsedInt;
    if (!parsedInt.parseHex(value.as_string().c_str()))
        return Error{Status{RippledError::rpcINVALID_PARAMS, std::string(key) + "Malformed"}};

    return MaybeError{};
}

/**
 * @brief A group of custom validation functions
 */
struct CustomValidators final {
    /**
     * @brief Provides a commonly used validator for ledger index.
     *
     * LedgerIndex must be a string or an int. If the specified LedgerIndex is a string, its value must be either
     * "validated" or a valid integer value represented as a string.
     */
    static CustomValidator ledgerIndexValidator;

    /**
     * @brief Provides a commonly used validator for accounts.
     *
     * Account must be a string and the converted public key is valid.
     */
    static CustomValidator accountValidator;

    /**
     * @brief Provides a commonly used validator for accounts.
     *
     * Account must be a string and can convert to base58.
     */
    static CustomValidator accountBase58Validator;

    /**
     * @brief Provides a commonly used validator for markers.
     *
     * A marker is composed of a comma-separated index and a start hint.
     * The former will be read as hex, and the latter can be cast to uint64.
     */
    static CustomValidator accountMarkerValidator;

    /**
     * @brief Provides a commonly used validator for uint160(AccountID) hex string.
     *
     * It must be a string and also a decodable hex.
     * AccountID uses this validator.
     */
    static CustomValidator uint160HexStringValidator;

    /**
     * @brief Provides a commonly used validator for uint192 hex string.
     *
     * It must be a string and also a decodable hex.
     * MPTIssuanceID uses this validator.
     */
    static CustomValidator uint192HexStringValidator;

    /**
     * @brief Provides a commonly used validator for uint256 hex string.
     *
     * It must be a string and also a decodable hex.
     * Transaction index, ledger hash all use this validator.
     */
    static CustomValidator uint256HexStringValidator;

    /**
     * @brief Provides a commonly used validator for currency, including standard currency code and token code.
     */
    static CustomValidator currencyValidator;

    /**
     * @brief Provides a commonly used validator for issuer type.
     *
     * It must be a hex string or base58 string.
     */
    static CustomValidator issuerValidator;

    /**
     * @brief Provides a validator for validating streams used in subscribe/unsubscribe.
     */
    static CustomValidator subscribeStreamValidator;

    /**
     * @brief Provides a validator for validating accounts used in subscribe/unsubscribe.
     */
    static CustomValidator subscribeAccountsValidator;

    /**
     * @brief Validates an asset (ripple::Issue).
     *
     * Used by amm_info.
     */
    static CustomValidator currencyIssueValidator;

    /**
     * @brief Provides a validator for validating authorized_credentials json array.
     *
     * Used by deposit_preauth.
     */
    static CustomValidator authorizeCredentialValidator;

    /**
     * @brief Provides a validator for validating credential_type.
     *
     * Used by AuthorizeCredentialValidator in deposit_preauth.
     */
    static CustomValidator credentialTypeValidator;
};

/**
 * @brief Validates that the elements of the array is of type Hex256 uint
 */
struct Hex256ItemType final {
    /**
     * @brief Validates given the prerequisite that the type of the json value is an array,
     * verifies all values within the array is of uint256 hash
     *
     * @param value the value to verify
     * @param key The key used to retrieve the tested value from the outer object
     * @return `RippledError::rpcINVALID_PARAMS` if validation failed; otherwise no error is returned
     */
    [[nodiscard]] static MaybeError
    verify(boost::json::value const& value, std::string_view key)
    {
        if (not value.is_object() or not value.as_object().contains(key))
            return {};  // ignore. If field is supposed to exist, let 'required' fail instead

        auto const& res = value.as_object().at(key);

        // loop through each item in the array and make sure it is uint256 hex string
        for (auto const& elem : res.as_array()) {
            ripple::uint256 num;
            if (!elem.is_string() || !num.parseHex(elem.as_string())) {
                return Error{Status{RippledError::rpcINVALID_PARAMS, "Item is not a valid uint256 type."}};
            }
        }
        return {};
    }
};

}  // namespace rpc::validation
