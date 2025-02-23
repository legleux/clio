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

#include "etl/impl/SubscriptionSource.hpp"

#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "rpc/JS.hpp"
#include "util/Retry.hpp"
#include "util/log/Logger.hpp"
#include "util/prometheus/Label.hpp"
#include "util/prometheus/Prometheus.hpp"
#include "util/requests/Types.hpp"

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/strand.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/json/object.hpp>
#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>
#include <boost/json/value_to.hpp>
#include <fmt/core.h>
#include <xrpl/protocol/jss.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <exception>
#include <expected>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace etl::impl {

SubscriptionSource::SubscriptionSource(
    boost::asio::io_context& ioContext,
    std::string const& ip,
    std::string const& wsPort,
    std::shared_ptr<NetworkValidatedLedgersInterface> validatedLedgers,
    std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
    OnConnectHook onConnect,
    OnDisconnectHook onDisconnect,
    OnLedgerClosedHook onLedgerClosed,
    std::chrono::steady_clock::duration const wsTimeout,
    std::chrono::steady_clock::duration const retryDelay
)
    : log_(fmt::format("SubscriptionSource[{}:{}]", ip, wsPort))
    , wsConnectionBuilder_(ip, wsPort)
    , validatedLedgers_(std::move(validatedLedgers))
    , subscriptions_(std::move(subscriptions))
    , strand_(boost::asio::make_strand(ioContext))
    , wsTimeout_(wsTimeout)
    , retry_(util::makeRetryExponentialBackoff(retryDelay, kRETRY_MAX_DELAY, strand_))
    , onConnect_(std::move(onConnect))
    , onDisconnect_(std::move(onDisconnect))
    , onLedgerClosed_(std::move(onLedgerClosed))
    , lastMessageTimeSecondsSinceEpoch_(PrometheusService::gaugeInt(
          "subscription_source_last_message_time",
          util::prometheus::Labels({{"source", fmt::format("{}:{}", ip, wsPort)}}),
          "Seconds since epoch of the last message received from rippled subscription streams"
      ))
{
    wsConnectionBuilder_.addHeader({boost::beast::http::field::user_agent, "clio-client"})
        .addHeader({"X-User", "clio-client"})
        .setConnectionTimeout(wsTimeout_);
}

void
SubscriptionSource::run()
{
    subscribe();
}

bool
SubscriptionSource::hasLedger(uint32_t sequence) const
{
    auto validatedLedgersData = validatedLedgersData_.lock();
    for (auto& pair : validatedLedgersData->validatedLedgers) {
        if (sequence >= pair.first && sequence <= pair.second) {
            return true;
        }
        if (sequence < pair.first) {
            // validatedLedgers_ is a sorted list of disjoint ranges
            // if the sequence comes before this range, the sequence will
            // come before all subsequent ranges
            return false;
        }
    }
    return false;
}

bool
SubscriptionSource::isConnected() const
{
    return isConnected_;
}

bool
SubscriptionSource::isForwarding() const
{
    return isForwarding_;
}

void
SubscriptionSource::setForwarding(bool isForwarding)
{
    isForwarding_ = isForwarding;
    LOG(log_.info()) << "Forwarding set to " << isForwarding_;
}

std::chrono::steady_clock::time_point
SubscriptionSource::lastMessageTime() const
{
    return lastMessageTime_.lock().get();
}

std::string const&
SubscriptionSource::validatedRange() const
{
    return validatedLedgersData_.lock()->validatedLedgersRaw;
}

void
SubscriptionSource::stop(boost::asio::yield_context yield)
{
    stop_ = true;
    stopHelper_.asyncWaitForStop(yield);
}

void
SubscriptionSource::subscribe()
{
    boost::asio::spawn(strand_, [this, _ = boost::asio::make_work_guard(strand_)](boost::asio::yield_context yield) {
        if (auto connection = wsConnectionBuilder_.connect(yield); connection) {
            wsConnection_ = std::move(connection).value();
        } else {
            handleError(connection.error(), yield);
            return;
        }

        auto const& subscribeCommand = getSubscribeCommandJson();

        if (auto const writeErrorOpt = wsConnection_->write(subscribeCommand, yield, wsTimeout_); writeErrorOpt) {
            handleError(writeErrorOpt.value(), yield);
            return;
        }

        isConnected_ = true;
        LOG(log_.info()) << "Connected";
        onConnect_();

        retry_.reset();

        while (!stop_) {
            auto const message = wsConnection_->read(yield, wsTimeout_);
            if (not message) {
                handleError(message.error(), yield);
                return;
            }

            if (auto const handleErrorOpt = handleMessage(message.value()); handleErrorOpt) {
                handleError(handleErrorOpt.value(), yield);
                return;
            }
        }
        // Close the connection
        handleError(
            util::requests::RequestError{"Subscription source stopped", boost::asio::error::operation_aborted}, yield
        );
    });
}

std::optional<util::requests::RequestError>
SubscriptionSource::handleMessage(std::string const& message)
{
    setLastMessageTime();

    try {
        auto const raw = boost::json::parse(message);
        auto const object = raw.as_object();
        uint32_t ledgerIndex = 0;

        static constexpr auto kJS_LEDGER_CLOSED = "ledgerClosed";
        static constexpr auto kJS_VALIDATION_RECEIVED = "validationReceived";
        static constexpr auto kJS_MANIFEST_RECEIVED = "manifestReceived";

        if (object.contains(JS(result))) {
            auto const& result = object.at(JS(result)).as_object();
            if (result.contains(JS(ledger_index)))
                ledgerIndex = result.at(JS(ledger_index)).as_int64();

            if (result.contains(JS(validated_ledgers))) {
                auto validatedLedgers = boost::json::value_to<std::string>(result.at(JS(validated_ledgers)));
                setValidatedRange(std::move(validatedLedgers));
            }
            LOG(log_.debug()) << "Received a message on ledger subscription stream. Message: " << object;

        } else if (object.contains(JS(type)) && object.at(JS(type)) == kJS_LEDGER_CLOSED) {
            LOG(log_.debug()) << "Received a message of type 'ledgerClosed' on ledger subscription stream. Message: "
                              << object;
            if (object.contains(JS(ledger_index))) {
                ledgerIndex = object.at(JS(ledger_index)).as_int64();
            }
            if (object.contains(JS(validated_ledgers))) {
                auto validatedLedgers = boost::json::value_to<std::string>(object.at(JS(validated_ledgers)));
                setValidatedRange(std::move(validatedLedgers));
            }
            if (isForwarding_)
                onLedgerClosed_();

        } else {
            if (isForwarding_) {
                // Clio as rippled's proposed_transactions subscirber, will receive two jsons for each transaction
                // 1 - Proposed transaction
                // 2 - Validated transaction
                // Only forward proposed transaction, validated transactions are sent by Clio itself
                if (object.contains(JS(transaction)) and !object.contains(JS(meta))) {
                    LOG(log_.debug()) << "Forwarding proposed transaction: " << object;
                    subscriptions_->forwardProposedTransaction(object);
                } else if (object.contains(JS(type)) && object.at(JS(type)) == kJS_VALIDATION_RECEIVED) {
                    LOG(log_.debug()) << "Forwarding validation: " << object;
                    subscriptions_->forwardValidation(object);
                } else if (object.contains(JS(type)) && object.at(JS(type)) == kJS_MANIFEST_RECEIVED) {
                    LOG(log_.debug()) << "Forwarding manifest: " << object;
                    subscriptions_->forwardManifest(object);
                }
            }
        }

        if (ledgerIndex != 0) {
            LOG(log_.trace()) << "Pushing ledger sequence = " << ledgerIndex;
            validatedLedgers_->push(ledgerIndex);
        }

        return std::nullopt;
    } catch (std::exception const& e) {
        LOG(log_.error()) << "Exception in handleMessage: " << e.what();
        return util::requests::RequestError{fmt::format("Error handling message: {}", e.what())};
    }
}

void
SubscriptionSource::handleError(util::requests::RequestError const& error, boost::asio::yield_context yield)
{
    isConnected_ = false;
    bool const wasForwarding = isForwarding_.exchange(false);
    if (not stop_) {
        LOG(log_.info()) << "Disconnected";
        onDisconnect_(wasForwarding);
    }

    if (wsConnection_ != nullptr) {
        wsConnection_->close(yield);
        wsConnection_.reset();
    }

    logError(error);
    if (not stop_) {
        retry_.retry([this] { subscribe(); });
    } else {
        stopHelper_.readyToStop();
    }
}

void
SubscriptionSource::logError(util::requests::RequestError const& error) const
{
    auto const& errorCodeOpt = error.errorCode();

    if (not errorCodeOpt or
        (errorCodeOpt.value() != boost::asio::error::operation_aborted &&
         errorCodeOpt.value() != boost::asio::error::connection_refused)) {
        LOG(log_.error()) << error.message();
    } else {
        LOG(log_.warn()) << error.message();
    }
}

void
SubscriptionSource::setLastMessageTime()
{
    lastMessageTimeSecondsSinceEpoch_.get().set(
        std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count()
    );
    auto lock = lastMessageTime_.lock();
    lock.get() = std::chrono::steady_clock::now();
}

void
SubscriptionSource::setValidatedRange(std::string range)
{
    std::vector<std::string> ranges;
    boost::split(ranges, range, [](char const c) { return c == ','; });

    std::vector<std::pair<uint32_t, uint32_t>> pairs;
    pairs.reserve(ranges.size());
    for (auto& pair : ranges) {
        std::vector<std::string> minAndMax;

        boost::split(minAndMax, pair, boost::is_any_of("-"));

        if (minAndMax.size() == 1) {
            uint32_t const sequence = std::stoll(minAndMax[0]);
            pairs.emplace_back(sequence, sequence);
        } else {
            if (minAndMax.size() != 2) {
                throw std::runtime_error(fmt::format(
                    "Error parsing range: {}.Min and max should be of size 2. Got size = {}", range, minAndMax.size()
                ));
            }
            uint32_t const min = std::stoll(minAndMax[0]);
            uint32_t const max = std::stoll(minAndMax[1]);
            pairs.emplace_back(min, max);
        }
    }
    std::ranges::sort(pairs, [](auto left, auto right) { return left.first < right.first; });

    auto dataLock = validatedLedgersData_.lock();
    dataLock->validatedLedgers = std::move(pairs);
    dataLock->validatedLedgersRaw = std::move(range);
}

std::string const&
SubscriptionSource::getSubscribeCommandJson()
{
    static boost::json::object const kJSON_VALUE{
        {"command", "subscribe"},
        {"streams", {"ledger", "manifests", "validations", "transactions_proposed"}},
    };
    static std::string const kJSON_STRING = boost::json::serialize(kJSON_VALUE);
    return kJSON_STRING;
}

}  // namespace etl::impl
