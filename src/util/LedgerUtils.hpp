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

#include "rpc/JS.hpp"

#include <fmt/core.h>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <array>
#include <string>
#include <unordered_set>
#include <vector>

namespace util {

class LedgerTypes;

namespace impl {
class LedgerTypeAttribute {
    enum class LedgerCategory {
        Invalid,
        AccountOwned,    // The ledger object is owned by account
        Chain,           // The ledger object is shared across the chain
        DeletionBlocker  // The ledger object is owned by account and it blocks deletion
    };

    ripple::LedgerEntryType type_ = ripple::ltANY;
    char const* name_ = nullptr;
    LedgerCategory category_ = LedgerCategory::Invalid;

    constexpr LedgerTypeAttribute(char const* name, ripple::LedgerEntryType type, LedgerCategory category)
        : type_(type), name_(name), category_(category)
    {
    }

public:
    static constexpr LedgerTypeAttribute
    chainLedgerType(char const* name, ripple::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, type, LedgerCategory::Chain);
    }

    static constexpr LedgerTypeAttribute
    accountOwnedLedgerType(char const* name, ripple::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, type, LedgerCategory::AccountOwned);
    }

    static constexpr LedgerTypeAttribute
    deletionBlockerLedgerType(char const* name, ripple::LedgerEntryType type)
    {
        return LedgerTypeAttribute(name, type, LedgerCategory::DeletionBlocker);
    }
    friend class util::LedgerTypes;
};
}  // namespace impl

/**
 * @brief A helper class that provides lists of different ledger type catagory.
 *
 */
class LedgerTypes {
    using LedgerTypeAttribute = impl::LedgerTypeAttribute;
    using LedgerTypeAttributeList = LedgerTypeAttribute[];

    static constexpr LedgerTypeAttributeList const kLEDGER_TYPES{
        LedgerTypeAttribute::accountOwnedLedgerType(JS(account), ripple::ltACCOUNT_ROOT),
        LedgerTypeAttribute::chainLedgerType(JS(amendments), ripple::ltAMENDMENTS),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(check), ripple::ltCHECK),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(deposit_preauth), ripple::ltDEPOSIT_PREAUTH),
        // dir node belongs to account, but can not be filtered from account_objects
        LedgerTypeAttribute::chainLedgerType(JS(directory), ripple::ltDIR_NODE),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(escrow), ripple::ltESCROW),
        LedgerTypeAttribute::chainLedgerType(JS(fee), ripple::ltFEE_SETTINGS),
        LedgerTypeAttribute::chainLedgerType(JS(hashes), ripple::ltLEDGER_HASHES),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(offer), ripple::ltOFFER),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(payment_channel), ripple::ltPAYCHAN),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(signer_list), ripple::ltSIGNER_LIST),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(state), ripple::ltRIPPLE_STATE),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(ticket), ripple::ltTICKET),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(nft_offer), ripple::ltNFTOKEN_OFFER),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(nft_page), ripple::ltNFTOKEN_PAGE),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(amm), ripple::ltAMM),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(bridge), ripple::ltBRIDGE),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(xchain_owned_claim_id), ripple::ltXCHAIN_OWNED_CLAIM_ID),
        LedgerTypeAttribute::deletionBlockerLedgerType(
            JS(xchain_owned_create_account_claim_id),
            ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID
        ),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(did), ripple::ltDID),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(oracle), ripple::ltORACLE),
        LedgerTypeAttribute::accountOwnedLedgerType(JS(credential), ripple::ltCREDENTIAL),
        LedgerTypeAttribute::chainLedgerType(JS(nunl), ripple::ltNEGATIVE_UNL),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(mpt_issuance), ripple::ltMPTOKEN_ISSUANCE),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(mptoken), ripple::ltMPTOKEN),
        LedgerTypeAttribute::deletionBlockerLedgerType(JS(permissioned_domain), ripple::ltPERMISSIONED_DOMAIN),
    };

public:
    /**
     * @brief Returns a list of all ledger entry type as string.
     * @return A list of all ledger entry type as string.
     */
    static constexpr auto
    getLedgerEntryTypeStrList()
    {
        std::array<char const*, std::size(kLEDGER_TYPES)> res{};
        std::ranges::transform(kLEDGER_TYPES, std::begin(res), [](auto const& item) { return item.name_; });
        return res;
    }

    /**
     * @brief Returns a list of all account owned ledger entry type as string.
     *
     * @return A list of all account owned ledger entry type as string.
     */
    static constexpr auto
    getAccountOwnedLedgerTypeStrList()
    {
        constexpr auto kFILTER = [](auto const& item) {
            return item.category_ != LedgerTypeAttribute::LedgerCategory::Chain;
        };

        constexpr auto kACCOUNT_OWNED_COUNT =
            std::count_if(std::begin(kLEDGER_TYPES), std::end(kLEDGER_TYPES), kFILTER);
        std::array<char const*, kACCOUNT_OWNED_COUNT> res{};
        auto it = std::begin(res);
        std::ranges::for_each(kLEDGER_TYPES, [&](auto const& item) {
            if (kFILTER(item)) {
                *it = item.name_;
                ++it;
            }
        });
        return res;
    }

    /**
     * @brief Returns a list of all account deletion blocker's type as string.
     *
     * @return A list of all account deletion blocker's type as string.
     */
    static constexpr auto
    getDeletionBlockerLedgerTypes()
    {
        constexpr auto kFILTER = [](auto const& item) {
            return item.category_ == LedgerTypeAttribute::LedgerCategory::DeletionBlocker;
        };

        constexpr auto kDELETION_BLOCKERS_COUNT =
            std::count_if(std::begin(kLEDGER_TYPES), std::end(kLEDGER_TYPES), kFILTER);
        std::array<ripple::LedgerEntryType, kDELETION_BLOCKERS_COUNT> res{};
        auto it = std::begin(res);
        std::ranges::for_each(kLEDGER_TYPES, [&](auto const& item) {
            if (kFILTER(item)) {
                *it = item.type_;
                ++it;
            }
        });
        return res;
    }

    /**
     * @brief Returns the ripple::LedgerEntryType from the given string.
     *
     * @param entryName The name of the ledger entry type
     * @return The ripple::LedgerEntryType of the given string, returns ltANY if not found.
     */
    static ripple::LedgerEntryType
    getLedgerEntryTypeFromStr(std::string const& entryName);
};

/**
 * @brief Deserializes a ripple::LedgerHeader from ripple::Slice of data.
 *
 * @param data The slice to deserialize
 * @return The deserialized ripple::LedgerHeader
 */
inline ripple::LedgerHeader
deserializeHeader(ripple::Slice data)
{
    return ripple::deserializeHeader(data, /* hasHash = */ true);
}

/**
 * @brief A helper function that converts a ripple::LedgerHeader to a string representation.
 *
 * @param info The ledger header
 * @return The string representation of the supplied ledger header
 */
inline std::string
toString(ripple::LedgerHeader const& info)
{
    return fmt::format(
        "LedgerHeader {{Sequence: {}, Hash: {}, TxHash: {}, AccountHash: {}, ParentHash: {}}}",
        info.seq,
        ripple::strHex(info.hash),
        strHex(info.txHash),
        ripple::strHex(info.accountHash),
        strHex(info.parentHash)
    );
}

}  // namespace util
