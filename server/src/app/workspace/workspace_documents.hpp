#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "app/config/config.hpp"

namespace lf {

std::vector<std::string> list_tex_documents(const app_config& config);

std::string resolve_tex_document(const app_config& config,
                                 const std::string& requested);

std::filesystem::path requested_pdf_path(const app_config& config,
                                         const std::string& current_main,
                                         const std::string& requested_tex);

}  // namespace lf
