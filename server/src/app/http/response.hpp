#pragma once

#include <filesystem>
#include <string>

#include <httplib.h>

#include "app/build/build_manager.hpp"
#include "app/config/config.hpp"

namespace lf {

bool ensure_basic_auth(const app_config& config,
                       const httplib::Request& request,
                       httplib::Response& response);

void apply_cors(const app_config& config, httplib::Response& response);

void set_text_response(const app_config& config, httplib::Response& response,
                       int status, const std::string& body);

void set_json_response(const app_config& config, httplib::Response& response,
                       int status, const std::string& body);

std::string load_file_contents(const std::filesystem::path& path);

std::string pdf_content_disposition_for(const std::filesystem::path& pdf_path);

void write_pdf_response(const app_config& config, const build_manager& manager,
                        const httplib::Request& request,
                        httplib::Response& response);

std::string sse_frame(const std::string& event_name, const std::string& data);

}  // namespace lf
