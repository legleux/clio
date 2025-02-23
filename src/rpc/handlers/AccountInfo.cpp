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

#include "rpc/handlers/AccountInfo.hpp"

#include "data/AmendmentCenter.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/JsonBool.hpp"
#include "rpc/common/Types.hpp"
#include "util/Assert.hpp"

#include <boost/json/array.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/strHex.h>
#include <xrpl/protocol/ErrorCodes.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace rpc {
AccountInfoHandler::Result
AccountInfoHandler::process(AccountInfoHandler::Input input, Context const& ctx) const
{
    using namespace data;

    if (!input.account && !input.ident)
        return Error{Status{RippledError::rpcINVALID_PARAMS, ripple::RPC::missing_field_message(JS(account))}};

    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "AccountInfo's ledger range must be available");
    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto const accountStr = input.account.value_or(input.ident.value_or(""));
    auto const accountID = accountFromStringStrict(accountStr);
    auto const accountKeylet = ripple::keylet::account(*accountID);
    auto const accountLedgerObject = sharedPtrBackend_->fetchLedgerObject(accountKeylet.key, lgrInfo.seq, ctx.yield);

    if (!accountLedgerObject)
        return Error{Status{RippledError::rpcACT_NOT_FOUND}};

    ripple::STLedgerEntry const sle{
        ripple::SerialIter{accountLedgerObject->data(), accountLedgerObject->size()}, accountKeylet.key
    };

    if (!accountKeylet.check(sle))
        return Error{Status{RippledError::rpcDB_DESERIALIZATION}};

    auto isEnabled = [this, &ctx, seq = lgrInfo.seq](auto key) {
        return amendmentCenter_->isEnabled(ctx.yield, key, seq);
    };

    auto const isDisallowIncomingEnabled = isEnabled(Amendments::DisallowIncoming);
    auto const isClawbackEnabled = isEnabled(Amendments::Clawback);

    // Return SignerList(s) if that is requested.
    if (input.signerLists) {
        // We put the SignerList in an array because of an anticipated
        // future when we support multiple signer lists on one account.
        auto const signersKey = ripple::keylet::signers(*accountID);

        // This code will need to be revisited if in the future we
        // support multiple SignerLists on one account.
        auto const signers = sharedPtrBackend_->fetchLedgerObject(signersKey.key, lgrInfo.seq, ctx.yield);
        std::vector<ripple::STLedgerEntry> signerList;

        if (signers) {
            ripple::STLedgerEntry const sleSigners{
                ripple::SerialIter{signers->data(), signers->size()}, signersKey.key
            };

            if (!signersKey.check(sleSigners))
                return Error{Status{RippledError::rpcDB_DESERIALIZATION}};

            signerList.push_back(sleSigners);
        }

        return Output(
            lgrInfo.seq,
            ripple::strHex(lgrInfo.hash),
            sle,
            isDisallowIncomingEnabled,
            isClawbackEnabled,
            ctx.apiVersion,
            signerList
        );
    }

    return Output(
        lgrInfo.seq, ripple::strHex(lgrInfo.hash), sle, isDisallowIncomingEnabled, isClawbackEnabled, ctx.apiVersion
    );
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, AccountInfoHandler::Output const& output)
{
    jv = boost::json::object{
        {JS(account_data), toJson(output.accountData)},
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
    };

    std::vector<std::pair<std::string_view, ripple::LedgerSpecificFlags>> lsFlags{{
        {"defaultRipple", ripple::lsfDefaultRipple},
        {"depositAuth", ripple::lsfDepositAuth},
        {"disableMasterKey", ripple::lsfDisableMaster},
        {"disallowIncomingXRP", ripple::lsfDisallowXRP},
        {"globalFreeze", ripple::lsfGlobalFreeze},
        {"noFreeze", ripple::lsfNoFreeze},
        {"passwordSpent", ripple::lsfPasswordSpent},
        {"requireAuthorization", ripple::lsfRequireAuth},
        {"requireDestinationTag", ripple::lsfRequireDestTag},
    }};

    if (output.isDisallowIncomingEnabled) {
        std::vector<std::pair<std::string_view, ripple::LedgerSpecificFlags>> const disallowIncomingFlags = {
            {"disallowIncomingNFTokenOffer", ripple::lsfDisallowIncomingNFTokenOffer},
            {"disallowIncomingCheck", ripple::lsfDisallowIncomingCheck},
            {"disallowIncomingPayChan", ripple::lsfDisallowIncomingPayChan},
            {"disallowIncomingTrustline", ripple::lsfDisallowIncomingTrustline},
        };
        lsFlags.insert(lsFlags.end(), disallowIncomingFlags.begin(), disallowIncomingFlags.end());
    }

    if (output.isClawbackEnabled) {
        lsFlags.emplace_back("allowTrustLineClawback", ripple::lsfAllowTrustLineClawback);
    }

    boost::json::object acctFlags;
    for (auto const& lsf : lsFlags)
        acctFlags[lsf.first] = output.accountData.isFlag(lsf.second);

    jv.as_object()[JS(account_flags)] = std::move(acctFlags);

    if (output.signerLists) {
        auto signers = boost::json::array();
        std::transform(
            std::cbegin(output.signerLists.value()),
            std::cend(output.signerLists.value()),
            std::back_inserter(signers),
            [](auto const& signerList) { return toJson(signerList); }
        );
        if (output.apiVersion == 1) {
            jv.as_object()[JS(account_data)].as_object()[JS(signer_lists)] = std::move(signers);
        } else {
            jv.as_object()[JS(signer_lists)] = signers;
        }
    }
}

AccountInfoHandler::Input
tag_invoke(boost::json::value_to_tag<AccountInfoHandler::Input>, boost::json::value const& jv)
{
    auto input = AccountInfoHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ident)))
        input.ident = boost::json::value_to<std::string>(jsonObject.at(JS(ident)));

    if (jsonObject.contains(JS(account)))
        input.account = boost::json::value_to<std::string>(jsonObject.at(JS(account)));

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jsonObject.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jsonObject.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jsonObject.at(JS(ledger_index))));
        }
    }

    if (jsonObject.contains(JS(signer_lists)))
        input.signerLists = boost::json::value_to<JsonBool>(jsonObject.at(JS(signer_lists)));

    return input;
}

}  // namespace rpc
