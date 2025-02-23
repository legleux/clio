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

#include "data/BackendInterface.hpp"
#include "data/LedgerCache.hpp"
#include "etl/CacheLoader.hpp"
#include "etl/ETLState.hpp"
#include "etl/LoadBalancer.hpp"
#include "etl/NetworkValidatedLedgersInterface.hpp"
#include "etl/SystemState.hpp"
#include "etl/impl/AmendmentBlockHandler.hpp"
#include "etl/impl/ExtractionDataPipe.hpp"
#include "etl/impl/Extractor.hpp"
#include "etl/impl/LedgerFetcher.hpp"
#include "etl/impl/LedgerLoader.hpp"
#include "etl/impl/LedgerPublisher.hpp"
#include "etl/impl/Transformer.hpp"
#include "feed/SubscriptionManagerInterface.hpp"
#include "util/log/Logger.hpp"

#include <boost/asio/io_context.hpp>
#include <boost/json/object.hpp>
#include <grpcpp/grpcpp.h>
#include <org/xrpl/rpc/v1/get_ledger.pb.h>
#include <xrpl/proto/org/xrpl/rpc/v1/xrp_ledger.grpc.pb.h>

#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>

struct AccountTransactionsData;
struct NFTTransactionsData;
struct NFTsData;

/**
 * @brief This namespace contains everything to do with the ETL and ETL sources.
 */
namespace etl {

/**
 * @brief A tag class to help identify ETLService in templated code.
 */
struct ETLServiceTag {
    virtual ~ETLServiceTag() = default;
};

template <typename T>
concept SomeETLService = std::derived_from<T, ETLServiceTag>;

/**
 * @brief This class is responsible for continuously extracting data from a p2p node, and writing that data to the
 * databases.
 *
 * Usually, multiple different processes share access to the same network accessible databases, in which case only one
 * such process is performing ETL and writing to the database. The other processes simply monitor the database for new
 * ledgers, and publish those ledgers to the various subscription streams. If a monitoring process determines that the
 * ETL writer has failed (no new ledgers written for some time), the process will attempt to become the ETL writer.
 *
 * If there are multiple monitoring processes that try to become the ETL writer at the same time, one will win out, and
 * the others will fall back to monitoring/publishing. In this sense, this class dynamically transitions from monitoring
 * to writing and from writing to monitoring, based on the activity of other processes running on different machines.
 */
class ETLService : public ETLServiceTag {
    // TODO: make these template parameters in ETLService
    using LoadBalancerType = LoadBalancer;
    using DataPipeType = etl::impl::ExtractionDataPipe<org::xrpl::rpc::v1::GetLedgerResponse>;
    using CacheType = data::LedgerCache;
    using CacheLoaderType = etl::CacheLoader<CacheType>;
    using LedgerFetcherType = etl::impl::LedgerFetcher<LoadBalancerType>;
    using ExtractorType = etl::impl::Extractor<DataPipeType, LedgerFetcherType>;
    using LedgerLoaderType = etl::impl::LedgerLoader<LoadBalancerType, LedgerFetcherType>;
    using LedgerPublisherType = etl::impl::LedgerPublisher<CacheType>;
    using AmendmentBlockHandlerType = etl::impl::AmendmentBlockHandler;
    using TransformerType =
        etl::impl::Transformer<DataPipeType, LedgerLoaderType, LedgerPublisherType, AmendmentBlockHandlerType>;

    util::Logger log_{"ETL"};

    std::shared_ptr<BackendInterface> backend_;
    std::shared_ptr<LoadBalancerType> loadBalancer_;
    std::shared_ptr<NetworkValidatedLedgersInterface> networkValidatedLedgers_;

    std::uint32_t extractorThreads_ = 1;
    std::thread worker_;

    CacheLoaderType cacheLoader_;
    LedgerFetcherType ledgerFetcher_;
    LedgerLoaderType ledgerLoader_;
    LedgerPublisherType ledgerPublisher_;
    AmendmentBlockHandlerType amendmentBlockHandler_;

    SystemState state_;

    size_t numMarkers_ = 2;
    std::optional<uint32_t> startSequence_;
    std::optional<uint32_t> finishSequence_;
    size_t txnThreshold_ = 0;

public:
    /**
     * @brief Create an instance of ETLService.
     *
     * @param config The configuration to use
     * @param ioc io context to run on
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param balancer Load balancer to use
     * @param ledgers The network validated ledgers datastructure
     */
    ETLService(
        util::config::ClioConfigDefinition const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
        std::shared_ptr<LoadBalancerType> balancer,
        std::shared_ptr<NetworkValidatedLedgersInterface> ledgers
    );

    /**
     * @brief A factory function to spawn new ETLService instances.
     *
     * Creates and runs the ETL service.
     *
     * @param config The configuration to use
     * @param ioc io context to run on
     * @param backend BackendInterface implementation
     * @param subscriptions Subscription manager
     * @param balancer Load balancer to use
     * @param ledgers The network validated ledgers datastructure
     * @return A shared pointer to a new instance of ETLService
     */
    static std::shared_ptr<ETLService>
    makeETLService(
        util::config::ClioConfigDefinition const& config,
        boost::asio::io_context& ioc,
        std::shared_ptr<BackendInterface> backend,
        std::shared_ptr<feed::SubscriptionManagerInterface> subscriptions,
        std::shared_ptr<LoadBalancerType> balancer,
        std::shared_ptr<NetworkValidatedLedgersInterface> ledgers
    )
    {
        auto etl = std::make_shared<ETLService>(config, ioc, backend, subscriptions, balancer, ledgers);
        etl->run();

        return etl;
    }

    /**
     * @brief Stops components and joins worker thread.
     */
    ~ETLService() override
    {
        if (not state_.isStopping)
            stop();
    }

    /**
     * @brief Stop the ETL service.
     * @note This method blocks until the ETL service has stopped.
     */
    void
    stop()
    {
        LOG(log_.info()) << "Stop called";

        state_.isStopping = true;
        cacheLoader_.stop();

        if (worker_.joinable())
            worker_.join();

        LOG(log_.debug()) << "Joined ETLService worker thread";
    }

    /**
     * @brief Get time passed since last ledger close, in seconds.
     *
     * @return Time passed since last ledger close
     */
    std::uint32_t
    lastCloseAgeSeconds() const
    {
        return ledgerPublisher_.lastCloseAgeSeconds();
    }

    /**
     * @brief Check for the amendment blocked state.
     *
     * @return true if currently amendment blocked; false otherwise
     */
    bool
    isAmendmentBlocked() const
    {
        return state_.isAmendmentBlocked;
    }

    /**
     * @brief Check whether Clio detected DB corruptions.
     *
     * @return true if corruption of DB was detected and cache was stopped.
     */
    bool
    isCorruptionDetected() const
    {
        return state_.isCorruptionDetected;
    }

    /**
     * @brief Get state of ETL as a JSON object
     *
     * @return The state of ETL as a JSON object
     */
    boost::json::object
    getInfo() const
    {
        boost::json::object result;

        result["etl_sources"] = loadBalancer_->toJson();
        result["is_writer"] = static_cast<int>(state_.isWriting);
        result["read_only"] = static_cast<int>(state_.isReadOnly);
        auto last = ledgerPublisher_.getLastPublish();
        if (last.time_since_epoch().count() != 0)
            result["last_publish_age_seconds"] = std::to_string(ledgerPublisher_.lastPublishAgeSeconds());
        return result;
    }

    /**
     * @brief Get the etl nodes' state
     * @return The etl nodes' state, nullopt if etl nodes are not connected
     */
    std::optional<etl::ETLState>
    getETLState() const noexcept
    {
        return loadBalancer_->getETLState();
    }

private:
    /**
     * @brief Run the ETL pipeline.
     *
     * Extracts ledgers and writes them to the database, until a write conflict occurs (or the server shuts down).
     * @note database must already be populated when this function is called
     *
     * @param startSequence the first ledger to extract
     * @param numExtractors number of extractors to use
     * @return The last ledger written to the database, if any
     */
    std::optional<uint32_t>
    runETLPipeline(uint32_t startSequence, uint32_t numExtractors);

    /**
     * @brief Monitor the network for newly validated ledgers.
     *
     * Also monitor the database to see if any process is writing those ledgers.
     * This function is called when the application starts, and will only return when the application is shutting down.
     * If the software detects the database is empty, this function will call loadInitialLedger(). If the software
     * detects ledgers are not being written, this function calls runETLPipeline(). Otherwise, this function publishes
     * ledgers as they are written to the database.
     */
    void
    monitor();

    /**
     * @brief Monitor the network for newly validated ledgers and publish them to the ledgers stream
     *
     * @param nextSequence the ledger sequence to publish
     * @return The next ledger sequence to publish
     */
    uint32_t
    publishNextSequence(uint32_t nextSequence);

    /**
     * @brief Monitor the database for newly written ledgers.
     *
     * Similar to the monitor(), except this function will never call runETLPipeline() or loadInitialLedger().
     * This function only publishes ledgers as they are written to the database.
     */
    void
    monitorReadOnly();

    /**
     * @return true if stopping; false otherwise
     */
    bool
    isStopping() const
    {
        return state_.isStopping;
    }

    /**
     * @brief Get the number of markers to use during the initial ledger download.
     *
     * This is equivelent to the degree of parallelism during the initial ledger download.
     *
     * @return The number of markers
     */
    std::uint32_t
    getNumMarkers() const
    {
        return numMarkers_;
    }

    /**
     * @brief Start all components to run ETL service.
     */
    void
    run();

    /**
     * @brief Spawn the worker thread and start monitoring.
     */
    void
    doWork();
};
}  // namespace etl
