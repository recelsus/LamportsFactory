#include "app/http/response.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#include "app/common/utils.hpp"
#include "app/workspace/workspace_documents.hpp"

namespace lf {
namespace {

bool is_disposition_token_char(unsigned char ch) {
  return std::isalnum(ch) != 0 || ch == '.' || ch == '_' || ch == '-';
}

std::string ascii_filename_fallback(const std::string& filename) {
  std::string fallback;
  fallback.reserve(filename.size());
  for (unsigned char ch : filename) {
    fallback.push_back(is_disposition_token_char(ch) ? static_cast<char>(ch)
                                                     : '_');
  }
  if (fallback.empty() || fallback == ".") {
    return "preview.pdf";
  }
  return fallback;
}

std::string percent_encode_filename(const std::string& filename) {
  std::ostringstream out;
  out << std::uppercase << std::hex;
  for (unsigned char ch : filename) {
    if (is_disposition_token_char(ch)) {
      out << static_cast<char>(ch);
    } else {
      out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
    }
  }
  return out.str();
}

}  // namespace

bool ensure_basic_auth(const app_config& config,
                       const httplib::Request& request,
                       httplib::Response& response) {
  if (config.basic_auth_header.empty()) {
    return true;
  }
  const std::string header = request.get_header_value("Authorization");
  if (header == config.basic_auth_header) {
    return true;
  }
  response.status = 401;
  response.set_header("WWW-Authenticate", "Basic realm=\"LamportsFactory\"");
  response.set_content("Unauthorized", "text/plain; charset=utf-8");
  return false;
}

void apply_cors(const app_config& config, httplib::Response& response) {
  if (!config.cors_allow_origin.empty()) {
    response.set_header("Access-Control-Allow-Origin", config.cors_allow_origin);
  }
}

void set_text_response(const app_config& config, httplib::Response& response,
                       int status, const std::string& body) {
  response.status = status;
  apply_cors(config, response);
  response.set_content(body, "text/plain; charset=utf-8");
}

void set_json_response(const app_config& config, httplib::Response& response,
                       int status, const std::string& body) {
  response.status = status;
  apply_cors(config, response);
  response.set_content(body, "application/json; charset=utf-8");
}

std::string load_file_contents(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::in | std::ios::binary);
  if (!stream) {
    return "";
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

std::string pdf_content_disposition_for(const std::filesystem::path& pdf_path) {
  std::string filename = pdf_path.filename().string();
  if (filename.empty()) {
    filename = "preview.pdf";
  }
  return "inline; filename=\"" + ascii_filename_fallback(filename) +
         "\"; filename*=UTF-8''" + percent_encode_filename(filename);
}

void write_pdf_response(const app_config& config, const build_manager& manager,
                        const httplib::Request& request,
                        httplib::Response& response) {
  const std::string requested =
      request.has_param("tex") ? request.get_param_value("tex") : "";
  const auto pdf_path =
      requested_pdf_path(config, manager.current_main(), requested);
  std::error_code ec;
  if (!std::filesystem::exists(pdf_path, ec)) {
    set_text_response(config, response, 404, "pdf not found");
    return;
  }
  const std::string body = load_file_contents(pdf_path);
  if (body.empty()) {
    set_text_response(config, response, 500, "failed to read pdf");
    return;
  }
  response.status = 200;
  response.set_header("cache-control", config.cache_control_pdf);
  response.set_header("content-disposition",
                      pdf_content_disposition_for(pdf_path));
  apply_cors(config, response);
  response.set_content(body, "application/pdf");
}

std::string sse_frame(const std::string& event_name, const std::string& data) {
  return "event: " + event_name + "\n" + "data: " + data + "\n\n";
}

}  // namespace lf
