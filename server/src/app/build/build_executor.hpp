#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "app/config/config.hpp"
#include "app/compiler/compiler_backend.hpp"
#include "app/process/process.hpp"

namespace lf {

process_result execute_build_tool(const app_config& config,
                                  const compiler_backend& compiler,
                                  const std::string& main_target);

std::optional<std::int64_t> probe_pdf_mtime(const app_config& config,
                                            const compiler_backend& compiler,
                                            const std::string& main_target);

}  // namespace lf
