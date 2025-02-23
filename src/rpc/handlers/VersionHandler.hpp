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

#include "rpc/common/APIVersion.hpp"
#include "rpc/common/Types.hpp"
#include "rpc/common/impl/APIVersionParser.hpp"
#include "util/newconfig/ConfigDefinition.hpp"

#include <boost/json/conversion.hpp>
#include <boost/json/value.hpp>

#include <cstdint>

namespace rpc {

/**
 * @brief The version command returns the min,max and current api Version we are using
 *
 */
class VersionHandler {
    rpc::impl::ProductionAPIVersionParser apiVersionParser_;

public:
    /**
     * @brief A struct to hold the output data of the command
     */
    struct Output {
        uint32_t minVersion;
        uint32_t maxVersion;
        uint32_t currVersion;
    };

    /**
     * @brief Construct a new Version Handler object
     *
     * @param config The configuration to use
     */
    explicit VersionHandler(util::config::ClioConfigDefinition const& config)
        : apiVersionParser_(
              config.get<uint32_t>("api_version.default"),
              config.get<uint32_t>("api_version.min"),
              config.get<uint32_t>("api_version.max")
          )
    {
    }

    using Result = HandlerReturnType<Output>;

    /**
     * @brief Process the version command
     *
     * @param ctx The context of the request
     * @return The result of the command
     */
    Result
    process([[maybe_unused]] Context const& ctx) const
    {
        using namespace rpc;

        auto output = Output{};
        output.currVersion = apiVersionParser_.getDefaultVersion();
        output.minVersion = apiVersionParser_.getMinVersion();
        output.maxVersion = apiVersionParser_.getMaxVersion();
        return output;
    }

private:
    friend void
    tag_invoke(boost::json::value_from_tag, boost::json::value& jv, Output const& output)
    {
        jv = {
            {"version",
             {
                 {"first", output.minVersion},
                 {"last", output.maxVersion},
                 {"good", output.currVersion},
             }},
        };
    }
};

}  // namespace rpc
