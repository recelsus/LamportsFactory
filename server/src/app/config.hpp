#pragma once

#include <regex>
#include <string>
#include <vector>

namespace lf {

enum class reload_mode { sse, ws, poll };

enum class log_level_setting { debug, info, warn, error };

struct app_config {
  std::string tex_dir;
  std::string out_dir;
  std::string tex_main;
  std::vector<std::string> watch_globs_raw;
  std::vector<std::regex> watch_globs;
  std::vector<std::string> watch_ignore_raw;
  std::vector<std::regex> watch_ignore;
  bool initial_build;
  std::string build_tool;
  std::vector<std::string> latexmk_opts;
  std::vector<std::string> tectonic_opts;
  int build_timeout_sec;
  int max_concurrent_builds;
  int file_change_batch_window_ms;
  reload_mode reload_choice;
  int reload_interval_ms;
  int reload_debounce_ms;
  int reload_retry_ms;
  std::string server_addr;
  int server_port;
  std::string static_dir;
  std::string pdf_path;
  std::string pdf_route;
  std::string cors_allow_origin;
  std::string basic_auth;
  std::string basic_auth_header;
  std::string cache_control_pdf;
  log_level_setting log_level_choice;
  int build_log_buffer_lines;
  bool metrics_enabled;
};

reload_mode parse_reload_mode(const std::string& value);
log_level_setting parse_log_level(const std::string& value);
app_config load_app_config();

}  // namespace lf

