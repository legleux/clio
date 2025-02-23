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

#include "rpc/handlers/LedgerEntry.hpp"

#include "rpc/CredentialHelpers.hpp"
#include "rpc/Errors.hpp"
#include "rpc/JS.hpp"
#include "rpc/RPCHelpers.hpp"
#include "rpc/common/Types.hpp"
#include "util/AccountUtils.hpp"
#include "util/Assert.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_to.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/StringUtilities.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/basics/strHex.h>
#include <xrpl/json/json_value.h>
#include <xrpl/protocol/AccountID.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/Issue.h>
#include <xrpl/protocol/LedgerFormats.h>
#include <xrpl/protocol/LedgerHeader.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/STXChainBridge.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/UintTypes.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <variant>

namespace rpc {

LedgerEntryHandler::Result
LedgerEntryHandler::process(LedgerEntryHandler::Input input, Context const& ctx) const
{
    ripple::uint256 key;

    if (input.index) {
        key = ripple::uint256{std::string_view(*(input.index))};
    } else if (input.accountRoot) {
        key = ripple::keylet::account(*util::parseBase58Wrapper<ripple::AccountID>(*(input.accountRoot))).key;
    } else if (input.did) {
        key = ripple::keylet::did(*util::parseBase58Wrapper<ripple::AccountID>(*(input.did))).key;
    } else if (input.directory) {
        auto const keyOrStatus = composeKeyFromDirectory(*input.directory);
        if (auto const status = std::get_if<Status>(&keyOrStatus))
            return Error{*status};

        key = std::get<ripple::uint256>(keyOrStatus);
    } else if (input.offer) {
        auto const id =
            util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(input.offer->at(JS(account)))
            );
        key = ripple::keylet::offer(*id, boost::json::value_to<std::uint32_t>(input.offer->at(JS(seq)))).key;
    } else if (input.rippleStateAccount) {
        auto const id1 = util::parseBase58Wrapper<ripple::AccountID>(
            boost::json::value_to<std::string>(input.rippleStateAccount->at(JS(accounts)).as_array().at(0))
        );
        auto const id2 = util::parseBase58Wrapper<ripple::AccountID>(
            boost::json::value_to<std::string>(input.rippleStateAccount->at(JS(accounts)).as_array().at(1))
        );
        auto const currency =
            ripple::to_currency(boost::json::value_to<std::string>(input.rippleStateAccount->at(JS(currency))));

        key = ripple::keylet::line(*id1, *id2, currency).key;
    } else if (input.escrow) {
        auto const id =
            util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(input.escrow->at(JS(owner)))
            );
        key = ripple::keylet::escrow(*id, input.escrow->at(JS(seq)).as_int64()).key;
    } else if (input.depositPreauth) {
        auto const owner = util::parseBase58Wrapper<ripple::AccountID>(
            boost::json::value_to<std::string>(input.depositPreauth->at(JS(owner)))
        );
        // Only one of authorize or authorized_credentials MUST exist;
        if (input.depositPreauth->contains(JS(authorized)) ==
            input.depositPreauth->contains(JS(authorized_credentials))) {
            return Error{
                Status{ClioError::RpcMalformedRequest, "Must have one of authorized or authorized_credentials."}
            };
        }

        if (input.depositPreauth->contains(JS(authorized))) {
            auto const authorized = util::parseBase58Wrapper<ripple::AccountID>(
                boost::json::value_to<std::string>(input.depositPreauth->at(JS(authorized)))
            );
            key = ripple::keylet::depositPreauth(*owner, *authorized).key;
        } else {
            auto const authorizedCredentials = rpc::credentials::parseAuthorizeCredentials(
                input.depositPreauth->at(JS(authorized_credentials)).as_array()
            );

            auto const authCreds = credentials::createAuthCredentials(authorizedCredentials);
            if (authCreds.size() != authorizedCredentials.size())
                return Error{Status{ClioError::RpcMalformedAuthorizedCredentials, "duplicates in credentials."}};

            key = ripple::keylet::depositPreauth(owner.value(), authCreds).key;
        }
    } else if (input.ticket) {
        auto const id =
            util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(input.ticket->at(JS(account))
            ));

        key = ripple::getTicketIndex(*id, input.ticket->at(JS(ticket_seq)).as_int64());
    } else if (input.amm) {
        auto const getIssuerFromJson = [](auto const& assetJson) {
            // the field check has been done in validator
            auto const currency = ripple::to_currency(boost::json::value_to<std::string>(assetJson.at(JS(currency))));
            if (ripple::isXRP(currency)) {
                return ripple::xrpIssue();
            }
            auto const issuer =
                util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(assetJson.at(JS(issuer)))
                );
            return ripple::Issue{currency, *issuer};
        };

        key = ripple::keylet::amm(
                  getIssuerFromJson(input.amm->at(JS(asset))), getIssuerFromJson(input.amm->at(JS(asset2)))
        )
                  .key;
    } else if (input.bridge) {
        if (!input.bridgeAccount && !input.chainClaimId && !input.createAccountClaimId)
            return Error{Status{ClioError::RpcMalformedRequest}};

        if (input.bridgeAccount) {
            auto const bridgeAccount = util::parseBase58Wrapper<ripple::AccountID>(*(input.bridgeAccount));
            auto const chainType = ripple::STXChainBridge::srcChain(bridgeAccount == input.bridge->lockingChainDoor());

            if (bridgeAccount != input.bridge->door(chainType))
                return Error{Status{ClioError::RpcMalformedRequest}};

            key = ripple::keylet::bridge(input.bridge->value(), chainType).key;
        } else if (input.chainClaimId) {
            key = ripple::keylet::xChainClaimID(input.bridge->value(), input.chainClaimId.value()).key;
        } else {
            key = ripple::keylet::xChainCreateAccountClaimID(input.bridge->value(), input.createAccountClaimId.value())
                      .key;
        }
    } else if (input.oracleNode) {
        key = input.oracleNode.value();
    } else if (input.credential) {
        key = input.credential.value();
    } else if (input.mptIssuance) {
        auto const mptIssuanceID = ripple::uint192{std::string_view(*(input.mptIssuance))};
        key = ripple::keylet::mptIssuance(mptIssuanceID).key;
    } else if (input.mptoken) {
        auto const holder =
            ripple::parseBase58<ripple::AccountID>(boost::json::value_to<std::string>(input.mptoken->at(JS(account))));
        auto const mptIssuanceID =
            ripple::uint192{std::string_view(boost::json::value_to<std::string>(input.mptoken->at(JS(mpt_issuance_id))))
            };
        key = ripple::keylet::mptoken(mptIssuanceID, *holder).key;
    } else if (input.permissionedDomain) {
        auto const account = ripple::parseBase58<ripple::AccountID>(
            boost::json::value_to<std::string>(input.permissionedDomain->at(JS(account)))
        );
        auto const seq = input.permissionedDomain->at(JS(seq)).as_int64();
        key = ripple::keylet::permissionedDomain(*account, seq).key;
    } else {
        // Must specify 1 of the following fields to indicate what type
        if (ctx.apiVersion == 1)
            return Error{Status{ClioError::RpcUnknownOption}};
        return Error{Status{RippledError::rpcINVALID_PARAMS}};
    }

    // check ledger exists
    auto const range = sharedPtrBackend_->fetchLedgerRange();
    ASSERT(range.has_value(), "LedgerEntry's ledger range must be available");
    auto const lgrInfoOrStatus = getLedgerHeaderFromHashOrSeq(
        *sharedPtrBackend_, ctx.yield, input.ledgerHash, input.ledgerIndex, range->maxSequence
    );

    if (auto const status = std::get_if<Status>(&lgrInfoOrStatus))
        return Error{*status};

    auto const lgrInfo = std::get<ripple::LedgerHeader>(lgrInfoOrStatus);
    auto output = LedgerEntryHandler::Output{};
    auto ledgerObject = sharedPtrBackend_->fetchLedgerObject(key, lgrInfo.seq, ctx.yield);

    if (!ledgerObject || ledgerObject->empty()) {
        if (not input.includeDeleted)
            return Error{Status{"entryNotFound"}};
        auto const deletedSeq = sharedPtrBackend_->fetchLedgerObjectSeq(key, lgrInfo.seq, ctx.yield);
        if (!deletedSeq)
            return Error{Status{"entryNotFound"}};
        ledgerObject = sharedPtrBackend_->fetchLedgerObject(key, deletedSeq.value() - 1, ctx.yield);
        if (!ledgerObject || ledgerObject->empty())
            return Error{Status{"entryNotFound"}};
        output.deletedLedgerIndex = deletedSeq;
    }

    ripple::STLedgerEntry const sle{ripple::SerialIter{ledgerObject->data(), ledgerObject->size()}, key};

    if (input.expectedType != ripple::ltANY && sle.getType() != input.expectedType)
        return Error{Status{"unexpectedLedgerType"}};

    output.index = ripple::strHex(key);
    output.ledgerIndex = lgrInfo.seq;
    output.ledgerHash = ripple::strHex(lgrInfo.hash);

    if (input.binary) {
        output.nodeBinary = ripple::strHex(*ledgerObject);
    } else {
        output.node = toJson(sle);
    }

    return output;
}

std::variant<ripple::uint256, Status>
LedgerEntryHandler::composeKeyFromDirectory(boost::json::object const& directory) noexcept
{
    // can not specify both dir_root and owner.
    if (directory.contains(JS(dir_root)) && directory.contains(JS(owner)))
        return Status{RippledError::rpcINVALID_PARAMS, "mayNotSpecifyBothDirRootAndOwner"};

    // at least one should availiable
    if (!(directory.contains(JS(dir_root)) || directory.contains(JS(owner))))
        return Status{RippledError::rpcINVALID_PARAMS, "missingOwnerOrDirRoot"};

    uint64_t const subIndex =
        directory.contains(JS(sub_index)) ? boost::json::value_to<uint64_t>(directory.at(JS(sub_index))) : 0;

    if (directory.contains(JS(dir_root))) {
        ripple::uint256 const uDirRoot{boost::json::value_to<std::string>(directory.at(JS(dir_root))).data()};
        return ripple::keylet::page(uDirRoot, subIndex).key;
    }

    auto const ownerID =
        util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(directory.at(JS(owner))));
    return ripple::keylet::page(ripple::keylet::ownerDir(*ownerID), subIndex).key;
}

void
tag_invoke(boost::json::value_from_tag, boost::json::value& jv, LedgerEntryHandler::Output const& output)
{
    auto object = boost::json::object{
        {JS(ledger_hash), output.ledgerHash},
        {JS(ledger_index), output.ledgerIndex},
        {JS(validated), output.validated},
        {JS(index), output.index},
    };

    if (output.deletedLedgerIndex)
        object["deleted_ledger_index"] = *(output.deletedLedgerIndex);

    if (output.nodeBinary) {
        object[JS(node_binary)] = *(output.nodeBinary);
    } else {
        object[JS(node)] = *(output.node);
    }

    jv = std::move(object);
}

LedgerEntryHandler::Input
tag_invoke(boost::json::value_to_tag<LedgerEntryHandler::Input>, boost::json::value const& jv)
{
    auto input = LedgerEntryHandler::Input{};
    auto const& jsonObject = jv.as_object();

    if (jsonObject.contains(JS(ledger_hash)))
        input.ledgerHash = boost::json::value_to<std::string>(jv.at(JS(ledger_hash)));

    if (jsonObject.contains(JS(ledger_index))) {
        if (!jsonObject.at(JS(ledger_index)).is_string()) {
            input.ledgerIndex = jv.at(JS(ledger_index)).as_int64();
        } else if (jsonObject.at(JS(ledger_index)).as_string() != "validated") {
            input.ledgerIndex = std::stoi(boost::json::value_to<std::string>(jv.at(JS(ledger_index))));
        }
    }

    if (jsonObject.contains(JS(binary)))
        input.binary = jv.at(JS(binary)).as_bool();

    // check all the protential index
    static auto const kINDEX_FIELD_TYPE_MAP = std::unordered_map<std::string, ripple::LedgerEntryType>{
        {JS(index), ripple::ltANY},
        {JS(directory), ripple::ltDIR_NODE},
        {JS(offer), ripple::ltOFFER},
        {JS(check), ripple::ltCHECK},
        {JS(escrow), ripple::ltESCROW},
        {JS(payment_channel), ripple::ltPAYCHAN},
        {JS(deposit_preauth), ripple::ltDEPOSIT_PREAUTH},
        {JS(ticket), ripple::ltTICKET},
        {JS(nft_page), ripple::ltNFTOKEN_PAGE},
        {JS(amm), ripple::ltAMM},
        {JS(xchain_owned_create_account_claim_id), ripple::ltXCHAIN_OWNED_CREATE_ACCOUNT_CLAIM_ID},
        {JS(xchain_owned_claim_id), ripple::ltXCHAIN_OWNED_CLAIM_ID},
        {JS(oracle), ripple::ltORACLE},
        {JS(credential), ripple::ltCREDENTIAL},
        {JS(mptoken), ripple::ltMPTOKEN},
        {JS(permissioned_domain), ripple::ltPERMISSIONED_DOMAIN}
    };

    auto const parseBridgeFromJson = [](boost::json::value const& bridgeJson) {
        auto const lockingDoor = *util::parseBase58Wrapper<ripple::AccountID>(
            boost::json::value_to<std::string>(bridgeJson.at(ripple::sfLockingChainDoor.getJsonName().c_str()))
        );
        auto const issuingDoor = *util::parseBase58Wrapper<ripple::AccountID>(
            boost::json::value_to<std::string>(bridgeJson.at(ripple::sfIssuingChainDoor.getJsonName().c_str()))
        );
        auto const lockingIssue =
            parseIssue(bridgeJson.at(ripple::sfLockingChainIssue.getJsonName().c_str()).as_object());
        auto const issuingIssue =
            parseIssue(bridgeJson.at(ripple::sfIssuingChainIssue.getJsonName().c_str()).as_object());

        return ripple::STXChainBridge{lockingDoor, lockingIssue, issuingDoor, issuingIssue};
    };

    auto const parseOracleFromJson = [](boost::json::value const& json) {
        auto const account =
            util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(json.at(JS(account))));
        auto const documentId = boost::json::value_to<uint32_t>(json.at(JS(oracle_document_id)));

        return ripple::keylet::oracle(*account, documentId).key;
    };

    auto const parseCredentialFromJson = [](boost::json::value const& json) {
        auto const subject =
            util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(json.at(JS(subject))));
        auto const issuer =
            util::parseBase58Wrapper<ripple::AccountID>(boost::json::value_to<std::string>(json.at(JS(issuer))));
        auto const credType = ripple::strUnHex(boost::json::value_to<std::string>(json.at(JS(credential_type))));

        return ripple::keylet::credential(*subject, *issuer, ripple::Slice(credType->data(), credType->size())).key;
    };

    auto const indexFieldType = std::ranges::find_if(kINDEX_FIELD_TYPE_MAP, [&jsonObject](auto const& pair) {
        auto const& [field, _] = pair;
        return jsonObject.contains(field) && jsonObject.at(field).is_string();
    });

    if (indexFieldType != kINDEX_FIELD_TYPE_MAP.end()) {
        input.index = boost::json::value_to<std::string>(jv.at(indexFieldType->first));
        input.expectedType = indexFieldType->second;
    }
    // check if request for account root
    else if (jsonObject.contains(JS(account_root))) {
        input.accountRoot = boost::json::value_to<std::string>(jv.at(JS(account_root)));
    } else if (jsonObject.contains(JS(did))) {
        input.did = boost::json::value_to<std::string>(jv.at(JS(did)));
    } else if (jsonObject.contains(JS(mpt_issuance))) {
        input.mptIssuance = boost::json::value_to<std::string>(jv.at(JS(mpt_issuance)));
    }
    // no need to check if_object again, validator only allows string or object
    else if (jsonObject.contains(JS(directory))) {
        input.directory = jv.at(JS(directory)).as_object();
    } else if (jsonObject.contains(JS(offer))) {
        input.offer = jv.at(JS(offer)).as_object();
    } else if (jsonObject.contains(JS(ripple_state))) {
        input.rippleStateAccount = jv.at(JS(ripple_state)).as_object();
    } else if (jsonObject.contains(JS(escrow))) {
        input.escrow = jv.at(JS(escrow)).as_object();
    } else if (jsonObject.contains(JS(deposit_preauth))) {
        input.depositPreauth = jv.at(JS(deposit_preauth)).as_object();
    } else if (jsonObject.contains(JS(ticket))) {
        input.ticket = jv.at(JS(ticket)).as_object();
    } else if (jsonObject.contains(JS(amm))) {
        input.amm = jv.at(JS(amm)).as_object();
    } else if (jsonObject.contains(JS(bridge))) {
        input.bridge.emplace(parseBridgeFromJson(jv.at(JS(bridge))));
        if (jsonObject.contains(JS(bridge_account)))
            input.bridgeAccount = boost::json::value_to<std::string>(jv.at(JS(bridge_account)));
    } else if (jsonObject.contains(JS(xchain_owned_claim_id))) {
        input.bridge.emplace(parseBridgeFromJson(jv.at(JS(xchain_owned_claim_id))));
        input.chainClaimId =
            boost::json::value_to<std::int32_t>(jv.at(JS(xchain_owned_claim_id)).at(JS(xchain_owned_claim_id)));
    } else if (jsonObject.contains(JS(xchain_owned_create_account_claim_id))) {
        input.bridge.emplace(parseBridgeFromJson(jv.at(JS(xchain_owned_create_account_claim_id))));
        input.createAccountClaimId = boost::json::value_to<std::int32_t>(
            jv.at(JS(xchain_owned_create_account_claim_id)).at(JS(xchain_owned_create_account_claim_id))
        );
    } else if (jsonObject.contains(JS(oracle))) {
        input.oracleNode = parseOracleFromJson(jv.at(JS(oracle)));
    } else if (jsonObject.contains(JS(credential))) {
        input.credential = parseCredentialFromJson(jv.at(JS(credential)));
    } else if (jsonObject.contains(JS(mptoken))) {
        input.mptoken = jv.at(JS(mptoken)).as_object();
    } else if (jsonObject.contains(JS(permissioned_domain))) {
        input.permissionedDomain = jv.at(JS(permissioned_domain)).as_object();
    }

    if (jsonObject.contains("include_deleted"))
        input.includeDeleted = jv.at("include_deleted").as_bool();

    return input;
}

}  // namespace rpc
