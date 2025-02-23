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

#include "data/AmendmentCenter.hpp"

#include "data/BackendInterface.hpp"
#include "data/Types.hpp"
#include "util/Assert.hpp"

#include <boost/asio/spawn.hpp>
#include <xrpl/basics/Slice.h>
#include <xrpl/basics/base_uint.h>
#include <xrpl/protocol/Feature.h>
#include <xrpl/protocol/Indexes.h>
#include <xrpl/protocol/SField.h>
#include <xrpl/protocol/STLedgerEntry.h>
#include <xrpl/protocol/Serializer.h>
#include <xrpl/protocol/digest.h>

#include <algorithm>
#include <cstdint>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

std::unordered_set<std::string>&
supportedAmendments()
{
    static std::unordered_set<std::string> kAMENDMENTS = {};
    return kAMENDMENTS;
}

bool
lookupAmendment(auto const& allAmendments, std::vector<ripple::uint256> const& ledgerAmendments, std::string_view name)
{
    namespace rg = std::ranges;
    if (auto const am = rg::find(allAmendments, name, &data::Amendment::name); am != rg::end(allAmendments))
        return rg::find(ledgerAmendments, am->feature) != rg::end(ledgerAmendments);
    return false;
}

}  // namespace

namespace data {
namespace impl {

WritingAmendmentKey::WritingAmendmentKey(std::string amendmentName) : AmendmentKey{std::move(amendmentName)}
{
    ASSERT(not supportedAmendments().contains(name), "Attempt to register the same amendment twice");
    supportedAmendments().insert(name);
}

}  // namespace impl

AmendmentKey::operator std::string const&() const
{
    return name;
}

AmendmentKey::operator std::string_view() const
{
    return name;
}

AmendmentKey::operator ripple::uint256() const
{
    return Amendment::getAmendmentId(name);
}

AmendmentCenter::AmendmentCenter(std::shared_ptr<data::BackendInterface> const& backend) : backend_{backend}
{
    namespace rg = std::ranges;
    namespace vs = std::views;

    rg::copy(
        ripple::allAmendments() | vs::transform([&](auto const& p) {
            auto const& [name, support] = p;
            return Amendment{
                .name = name,
                .feature = Amendment::getAmendmentId(name),
                .isSupportedByXRPL = support != ripple::AmendmentSupport::Unsupported,
                .isSupportedByClio = rg::find(supportedAmendments(), name) != rg::end(supportedAmendments()),
                .isRetired = support == ripple::AmendmentSupport::Retired
            };
        }),
        std::back_inserter(all_)
    );

    for (auto const& am : all_ | vs::filter([](auto const& am) { return am.isSupportedByClio; }))
        supported_.insert_or_assign(am.name, am);
}

bool
AmendmentCenter::isSupported(AmendmentKey const& key) const
{
    return supported_.contains(key);
}

std::map<std::string, Amendment> const&
AmendmentCenter::getSupported() const
{
    return supported_;
}

std::vector<Amendment> const&
AmendmentCenter::getAll() const
{
    return all_;
}

bool
AmendmentCenter::isEnabled(AmendmentKey const& key, uint32_t seq) const
{
    return data::synchronous([this, &key, seq](auto yield) { return isEnabled(yield, key, seq); });
}

bool
AmendmentCenter::isEnabled(boost::asio::yield_context yield, AmendmentKey const& key, uint32_t seq) const
{
    if (auto const listAmendments = fetchAmendmentsList(yield, seq); listAmendments)
        return lookupAmendment(all_, *listAmendments, key);

    return false;
}

std::vector<bool>
AmendmentCenter::isEnabled(boost::asio::yield_context yield, std::vector<AmendmentKey> const& keys, uint32_t seq) const
{
    namespace rg = std::ranges;

    if (auto const listAmendments = fetchAmendmentsList(yield, seq); listAmendments) {
        std::vector<bool> out;
        rg::transform(keys, std::back_inserter(out), [this, &listAmendments](auto const& key) {
            return lookupAmendment(all_, *listAmendments, key);
        });

        return out;
    }

    return std::vector<bool>(keys.size(), false);
}

Amendment const&
AmendmentCenter::getAmendment(AmendmentKey const& key) const
{
    ASSERT(supported_.contains(key), "The amendment '{}' must be present in supported amendments list", key.name);
    return supported_.at(key);
}

Amendment const&
AmendmentCenter::operator[](AmendmentKey const& key) const
{
    return getAmendment(key);
}

ripple::uint256
Amendment::getAmendmentId(std::string_view name)
{
    return ripple::sha512Half(ripple::Slice(name.data(), name.size()));
}

std::optional<std::vector<ripple::uint256>>
AmendmentCenter::fetchAmendmentsList(boost::asio::yield_context yield, uint32_t seq) const
{
    // the amendments should always be present on the ledger
    auto const amendments = backend_->fetchLedgerObject(ripple::keylet::amendments().key, seq, yield);
    if (not amendments.has_value())
        throw std::runtime_error("Amendments ledger object must be present in the database");

    ripple::SLE const amendmentsSLE{
        ripple::SerialIter{amendments->data(), amendments->size()}, ripple::keylet::amendments().key
    };

    return amendmentsSLE[~ripple::sfAmendments];
}

}  // namespace data
