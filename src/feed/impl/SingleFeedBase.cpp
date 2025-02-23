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

#include "feed/impl/SingleFeedBase.hpp"

#include "feed/Types.hpp"
#include "feed/impl/TrackableSignal.hpp"
#include "feed/impl/Util.hpp"
#include "util/async/AnyExecutionContext.hpp"
#include "util/log/Logger.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace feed::impl {

SingleFeedBase::SingleFeedBase(util::async::AnyExecutionContext& executionCtx, std::string const& name)
    : strand_(executionCtx.makeStrand()), subCount_(getSubscriptionsGaugeInt(name)), name_(name)
{
}

void
SingleFeedBase::sub(SubscriberSharedPtr const& subscriber)
{
    auto const weakPtr = std::weak_ptr(subscriber);
    auto const added = signal_.connectTrackableSlot(subscriber, [weakPtr](std::shared_ptr<std::string> const& msg) {
        if (auto connectionPtr = weakPtr.lock())
            connectionPtr->send(msg);
    });

    if (added) {
        LOG(logger_.info()) << subscriber->tag() << "Subscribed " << name_;
        ++subCount_.get();
        subscriber->onDisconnect([this](SubscriberPtr connectionDisconnecting) {
            unsubInternal(connectionDisconnecting);
        });
    };
}

void
SingleFeedBase::unsub(SubscriberSharedPtr const& subscriber)
{
    unsubInternal(subscriber.get());
}

void
SingleFeedBase::pub(std::string msg)
{
    [[maybe_unused]] auto task = strand_.execute([this, msg = std::move(msg)]() {
        auto const msgPtr = std::make_shared<std::string>(msg);
        signal_.emit(msgPtr);
    });
}

std::uint64_t
SingleFeedBase::count() const
{
    return subCount_.get().value();
}

void
SingleFeedBase::unsubInternal(SubscriberPtr subscriber)
{
    if (signal_.disconnect(subscriber)) {
        LOG(logger_.info()) << subscriber->tag() << "Unsubscribed " << name_;
        --subCount_.get();
    }
}
}  // namespace feed::impl
