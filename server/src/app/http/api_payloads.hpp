#pragma once

#include <string>

#include "app/build/build_manager.hpp"
#include "app/config/config.hpp"

namespace lf {

std::string config_json(const app_config& config);

std::string build_snapshot_json(const app_config& config,
                                build_manager& manager);

}  // namespace lf
