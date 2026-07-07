#include "app/http/api_payloads.hpp"

#include <sstream>

#include "app/common/utils.hpp"
#include "app/workspace/workspace_documents.hpp"

namespace lf {

std::string config_json(const app_config& config) {
  std::ostringstream out;
  out << "{\"reload_mode\":\""
      << (config.reload_choice == reload_mode::ws
              ? "ws"
              : (config.reload_choice == reload_mode::poll ? "poll" : "sse"))
      << "\",";
  out << "\"reload_interval_ms\":" << config.reload_interval_ms << ",";
  out << "\"reload_debounce_ms\":" << config.reload_debounce_ms << ",";
  out << "\"reload_retry_ms\":" << config.reload_retry_ms << ",";
  out << "\"base_url\":\"" << json_escape(config.base_url) << "\",";
  out << "\"latex_engine\":\"" << json_escape(config.latex_engine) << "\",";
  out << "\"layout_mode\":\"" << json_escape(config.layout_mode) << "\",";
  out << "\"ttyd_enabled\":" << (config.ttyd_enabled ? "true" : "false")
      << ",";
  out << "\"ttyd_url\":\"" << json_escape(config.ttyd_url) << "\"";
  out << "}";
  return out.str();
}

std::string build_snapshot_json(const app_config& config,
                                build_manager& manager) {
  const build_snapshot snap = manager.snapshot();
  const auto files = list_tex_documents(config);
  std::ostringstream body;
  body << "{\"status\":\"" << snap.status << "\",";
  body << "\"building\":" << (snap.building ? "true" : "false") << ",";
  body << "\"ready\":" << (snap.ready ? "true" : "false") << ",";
  body << "\"last_success\":" << (snap.last_success ? "true" : "false")
       << ",";
  body << "\"last_duration_ms\":" << snap.last_duration_ms << ",";
  body << "\"updated_at\":" << snap.updated_at;
  if (snap.pdf_mtime.has_value()) {
    body << ",\"pdf_mtime\":" << snap.pdf_mtime.value();
  }
  if (!snap.last_error.empty()) {
    body << ",\"last_error\":\"" << json_escape(snap.last_error) << "\"";
  }
  if (!snap.current_main.empty()) {
    body << ",\"tex_main\":\"" << json_escape(snap.current_main) << "\"";
  }
  body << ",\"files\":[";
  for (std::size_t i = 0; i < files.size(); ++i) {
    body << "\"" << json_escape(files[i]) << "\"";
    if (i + 1 < files.size()) {
      body << ",";
    }
  }
  body << "]}";
  return body.str();
}

}  // namespace lf
