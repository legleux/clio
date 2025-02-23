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

#include "app/CliArgs.hpp"

#include "migration/MigrationApplication.hpp"
#include "util/build/Build.hpp"

#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/positional_options.hpp>
#include <boost/program_options/value_semantic.hpp>
#include <boost/program_options/variables_map.hpp>

#include <cstdlib>
#include <iostream>
#include <string>
#include <utility>

namespace app {

CliArgs::Action
CliArgs::parse(int argc, char const* argv[])
{
    namespace po = boost::program_options;
    // clang-format off
    po::options_description description("Options");
    description.add_options()
        ("help,h", "print help message and exit")
        ("version,v", "print version and exit")
        ("conf,c", po::value<std::string>()->default_value(kDEFAULT_CONFIG_PATH), "configuration file")
        ("ng-web-server,w", "Use ng-web-server")
        ("migrate", po::value<std::string>(), "start migration helper")
        ("verify", "Checks the validity of config values")
    ;
    // clang-format on
    po::positional_options_description positional;
    positional.add("conf", 1);

    po::variables_map parsed;
    po::store(po::command_line_parser(argc, argv).options(description).positional(positional).run(), parsed);
    po::notify(parsed);

    if (parsed.count("help") != 0u) {
        std::cout << "Clio server " << util::build::getClioFullVersionString() << "\n\n" << description;
        return Action{Action::Exit{EXIT_SUCCESS}};
    }

    if (parsed.count("version") != 0u) {
        std::cout << util::build::getClioFullVersionString() << '\n';
        return Action{Action::Exit{EXIT_SUCCESS}};
    }

    auto configPath = parsed["conf"].as<std::string>();

    if (parsed.count("migrate") != 0u) {
        auto const opt = parsed["migrate"].as<std::string>();
        if (opt == "status")
            return Action{Action::Migrate{.configPath = std::move(configPath), .subCmd = MigrateSubCmd::status()}};
        return Action{Action::Migrate{.configPath = std::move(configPath), .subCmd = MigrateSubCmd::migration(opt)}};
    }

    if (parsed.count("verify") != 0u)
        return Action{Action::VerifyConfig{.configPath = std::move(configPath)}};

    return Action{Action::Run{.configPath = std::move(configPath), .useNgWebServer = parsed.count("ng-web-server") != 0}
    };
}

}  // namespace app
