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

#include "data/cassandra/Types.hpp"
#include "data/cassandra/impl/Cluster.hpp"
#include "util/newconfig/ObjectView.hpp"

#include <cstdint>
#include <optional>
#include <string>

namespace data::cassandra {

/**
 * @brief Provides settings for @ref BasicCassandraBackend.
 */
class SettingsProvider {
    util::config::ObjectView config_;

    std::string keyspace_;
    std::optional<std::string> tablePrefix_;
    uint16_t replicationFactor_;
    Settings settings_;

public:
    /**
     * @brief Create a settings provider from the specified config.
     *
     * @param cfg The config of Clio to use
     */
    explicit SettingsProvider(util::config::ObjectView const& cfg);

    /**
     * @return The cluster settings
     */
    [[nodiscard]] Settings
    getSettings() const;

    /**
     * @return The specified keyspace
     */
    [[nodiscard]] std::string
    getKeyspace() const
    {
        return keyspace_;
    }

    /**
     * @return The optional table prefix to use in all queries
     */
    [[nodiscard]] std::optional<std::string>
    getTablePrefix() const
    {
        return tablePrefix_;
    }

    /**
     * @return The replication factor
     */
    [[nodiscard]] uint16_t
    getReplicationFactor() const
    {
        return replicationFactor_;
    }

private:
    [[nodiscard]] std::optional<std::string>
    parseOptionalCertificate() const;

    [[nodiscard]] Settings
    parseSettings() const;
};

}  // namespace data::cassandra
