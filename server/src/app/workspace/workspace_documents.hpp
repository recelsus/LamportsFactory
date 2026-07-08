#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "app/config/config.hpp"

namespace lf {

std::vector<std::string> list_documents(const app_config& config);

std::string resolve_document(const app_config& config,
                                 const std::string& requested);

}  // namespace lf
