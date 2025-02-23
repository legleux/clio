//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2022, the clio developers.

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

#include "util/SourceLocation.hpp"

#include <boost/algorithm/string/predicate.hpp>
#include <boost/filesystem.hpp>
#include <boost/json.hpp>
#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>
#include <boost/log/core/core.hpp>
#include <boost/log/core/record.hpp>
#include <boost/log/expressions/filter.hpp>
#include <boost/log/expressions/keyword.hpp>
#include <boost/log/expressions/predicates/channel_severity_filter.hpp>
#include <boost/log/keywords/channel.hpp>
#include <boost/log/keywords/severity.hpp>
#include <boost/log/sinks/unlocked_frontend.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>
#include <boost/log/sources/severity_feature.hpp>
#include <boost/log/sources/severity_logger.hpp>
#include <boost/log/utility/manipulators/add_value.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/formatter_parser.hpp>

#include <array>
#include <cstddef>
#include <optional>
#include <ostream>
#include <string>

namespace util {

namespace config {
class ClioConfigDefinition;
}  // namespace config

/**
 * @brief Skips evaluation of expensive argument lists if the given logger is disabled for the required severity level.
 *
 * Note: Currently this introduces potential shadowing (unlikely).
 */
#ifndef COVERAGE_ENABLED
#define LOG(x)                                   \
    if (auto clio_pump__ = x; not clio_pump__) { \
    } else                                       \
        clio_pump__
#else
#define LOG(x) x
#endif

/**
 * @brief Custom severity levels for @ref util::Logger.
 */
enum class Severity {
    TRC,
    DBG,
    NFO,
    WRN,
    ERR,
    FTL,
};

/** @cond */
// NOLINTBEGIN(readability-identifier-naming)
BOOST_LOG_ATTRIBUTE_KEYWORD(LogSeverity, "Severity", Severity);
BOOST_LOG_ATTRIBUTE_KEYWORD(LogChannel, "Channel", std::string);
// NOLINTEND(readability-identifier-naming)
/** @endcond */

/**
 * @brief Custom labels for @ref Severity in log output.
 *
 * @param stream std::ostream The output stream
 * @param sev Severity The severity to output to the ostream
 * @return The same ostream we were given
 */
std::ostream&
operator<<(std::ostream& stream, Severity sev);

/**
 * @brief A simple thread-safe logger for the channel specified
 * in the constructor.
 *
 * This is cheap to copy and move. Designed to be used as a member variable or
 * otherwise. See @ref LogService::init() for setup of the logging core and
 * severity levels for each channel.
 */
class Logger final {
    using LoggerType = boost::log::sources::severity_channel_logger_mt<Severity, std::string>;
    mutable LoggerType logger_;

    friend class LogService;  // to expose the Pump interface

    /**
     * @brief Helper that pumps data into a log record via `operator<<`.
     */
    class Pump final {
        using PumpOptType = std::optional<boost::log::aux::record_pump<LoggerType>>;

        boost::log::record rec_;
        PumpOptType pump_ = std::nullopt;

    public:
        ~Pump() = default;

        Pump(LoggerType& logger, Severity sev, SourceLocationType const& loc)
            : rec_{logger.open_record(boost::log::keywords::severity = sev)}
        {
            if (rec_) {
                pump_.emplace(boost::log::aux::make_record_pump(logger, rec_));
                pump_->stream() << boost::log::add_value("SourceLocation", prettyPath(loc));
            }
        }

        Pump(Pump&&) = delete;
        Pump(Pump const&) = delete;
        Pump&
        operator=(Pump const&) = delete;
        Pump&
        operator=(Pump&&) = delete;

        /**
         * @brief Perfectly forwards any incoming data into the underlying boost::log pump if the pump is available.
         *
         * @tparam T Type of data to pump
         * @param data The data to pump
         * @return Reference to itself for chaining
         */
        template <typename T>
        [[maybe_unused]] Pump&
        operator<<(T&& data)
        {
            if (pump_)
                pump_->stream() << std::forward<T>(data);
            return *this;
        }

        /**
         * @return true if logger is enabled; false otherwise
         */
        operator bool() const
        {
            return pump_.has_value();
        }

    private:
        [[nodiscard]] static std::string
        prettyPath(SourceLocationType const& loc, size_t maxDepth = 3);
    };

public:
    static constexpr std::array<char const*, 8> kCHANNELS = {
        "General",
        "WebServer",
        "Backend",
        "RPC",
        "ETL",
        "Subscriptions",
        "Performance",
        "Migration",
    };

    /**
     * @brief Construct a new Logger object that produces loglines for the
     * specified channel.
     *
     * See @ref LogService::init() for general setup and configuration of
     * severity levels per channel.
     *
     * @param channel The channel this logger will report into.
     */
    Logger(std::string channel) : logger_{boost::log::keywords::channel = channel}
    {
    }

    Logger(Logger const&) = default;
    ~Logger() = default;

    Logger(Logger&&) = default;
    Logger&
    operator=(Logger const&) = default;

    Logger&
    operator=(Logger&&) = default;

    /**
     * @brief Interface for logging at Severity::TRC severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    trace(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::DBG severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    debug(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::NFO severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    info(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::WRN severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    warn(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::ERR severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    error(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;

    /**
     * @brief Interface for logging at Severity::FTL severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] Pump
    fatal(SourceLocationType const& loc = CURRENT_SRC_LOCATION) const;
};

/**
 * @brief A global logging service.
 *
 * Used to initialize and setup the logging core as well as a globally available
 * entrypoint for logging into the `General` channel as well as raising alerts.
 */
class LogService {
    static Logger generalLog; /*< Global logger for General channel */
    static Logger alertLog;   /*< Global logger for Alerts channel */
    static boost::log::filter filter;

public:
    LogService() = delete;

    /**
     * @brief Global log core initialization from a @ref Config
     *
     * @param config The configuration to use
     */
    static void
    init(config::ClioConfigDefinition const& config);

    /**
     * @brief Globally accesible General logger at Severity::TRC severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    trace(SourceLocationType const& loc = CURRENT_SRC_LOCATION)
    {
        return generalLog.trace(loc);
    }

    /**
     * @brief Globally accesible General logger at Severity::DBG severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    debug(SourceLocationType const& loc = CURRENT_SRC_LOCATION)
    {
        return generalLog.debug(loc);
    }

    /**
     * @brief Globally accesible General logger at Severity::NFO severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    info(SourceLocationType const& loc = CURRENT_SRC_LOCATION)
    {
        return generalLog.info(loc);
    }

    /**
     * @brief Globally accesible General logger at Severity::WRN severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    warn(SourceLocationType const& loc = CURRENT_SRC_LOCATION)
    {
        return generalLog.warn(loc);
    }

    /**
     * @brief Globally accesible General logger at Severity::ERR severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    error(SourceLocationType const& loc = CURRENT_SRC_LOCATION)
    {
        return generalLog.error(loc);
    }

    /**
     * @brief Globally accesible General logger at Severity::FTL severity
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    fatal(SourceLocationType const& loc = CURRENT_SRC_LOCATION)
    {
        return generalLog.fatal(loc);
    }

    /**
     * @brief Globally accesible Alert logger
     *
     * @param loc The source location of the log message
     * @return The pump to use for logging
     */
    [[nodiscard]] static Logger::Pump
    alert(SourceLocationType const& loc = CURRENT_SRC_LOCATION)
    {
        return alertLog.warn(loc);
    }
};

};  // namespace util
