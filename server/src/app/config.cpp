#include "app/config.hpp"

#include <filesystem>
#include <vector>

#include "app/utils.hpp"

namespace {

std::string resolve_static_dir(const std::string& override_path) {
  if (!override_path.empty()) {
    return override_path;
  }
  std::error_code ec;
  std::filesystem::path cwd = std::filesystem::current_path(ec);
  if (ec) {
    cwd.clear();
  }
  const std::vector<std::filesystem::path> candidates = {
      std::filesystem::path{"/app/server/public"},
      std::filesystem::path{"/app/public"},
      cwd / "../public",
      cwd / "public",
      cwd / "../../public",
  };
  for (const auto& candidate : candidates) {
    std::error_code exists_ec;
    if (std::filesystem::exists(candidate, exists_ec) &&
        std::filesystem::is_directory(candidate, exists_ec)) {
      return candidate.lexically_normal().string();
    }
  }
  return "/app/server/public";
}

}  // namespace

namespace lf {

reload_mode parse_reload_mode(const std::string& value) {
  const std::string lower = to_lower_copy(value);
  if (lower == "ws") {
    return reload_mode::ws;
  }
  if (lower == "poll") {
    return reload_mode::poll;
  }
  return reload_mode::sse;
}

log_level_setting parse_log_level(const std::string& value) {
  const std::string lower = to_lower_copy(value);
  if (lower == "debug") {
    return log_level_setting::debug;
  }
  if (lower == "warn") {
    return log_level_setting::warn;
  }
  if (lower == "error") {
    return log_level_setting::error;
  }
  return log_level_setting::info;
}

app_config load_app_config() {
  app_config config{};
  config.tex_dir = get_env_string("TEX_DIR", "/app/tex");
  config.out_dir = get_env_string("OUT_DIR", "/app/tex/build");
  config.tex_main = get_env_string("TEX_MAIN", "main.tex");
  config.watch_globs_raw =
      split_multi_glob(get_env_string("WATCH_GLOB", "**/*.tex"));
  config.watch_globs = compile_globs(config.watch_globs_raw);
  config.watch_ignore_raw =
      split_semicolon(get_env_string("WATCH_IGNORE_GLOB",
                                     "build/**;**/*.aux;**/*.log;**/*.synctex.gz"));
  config.watch_ignore = compile_globs(config.watch_ignore_raw);
  config.initial_build = get_env_bool("INITIAL_BUILD", true);
  config.build_tool = to_lower_copy(get_env_string("BUILD_TOOL", "latexmk"));
  config.latexmk_opts =
      split_whitespace(get_env_string("LATEXMK_OPTS",
                                       "-pdf -interaction=nonstopmode"));
  config.tectonic_opts = split_whitespace(get_env_string("TECTONIC_OPTS", ""));
  config.build_timeout_sec = get_env_int("BUILD_TIMEOUT_SEC", 120);
  config.max_concurrent_builds = get_env_int("MAX_CONCURRENT_BUILDS", 1);
  config.file_change_batch_window_ms =
      get_env_int("FILE_CHANGE_BATCH_WINDOW_MS", 150);
  config.reload_choice = parse_reload_mode(get_env_string("RELOAD_MODE", "sse"));
  config.reload_interval_ms = get_env_int("RELOAD_INTERVAL_MS", 1500);
  config.reload_debounce_ms = get_env_int("RELOAD_DEBOUNCE_MS", 100);
  config.reload_retry_ms = get_env_int("RELOAD_RETRY_MS", 2000);
  config.server_addr = get_env_string("SERVER_ADDR", "0.0.0.0");
  config.server_port = get_env_int("SERVER_PORT", 8080);
  config.static_dir = resolve_static_dir(get_env_string("STATIC_DIR", ""));
  config.pdf_path = get_env_string("PDF_PATH", "/app/tex/build/main.pdf");
  config.pdf_route = get_env_string("PDF_ROUTE", "/pdf");
  config.cors_allow_origin = get_env_string("CORS_ALLOW_ORIGIN", "");
  config.basic_auth = get_env_string("BASIC_AUTH", "");
  if (!config.basic_auth.empty()) {
    config.basic_auth_header = "Basic " + base64_encode(config.basic_auth);
  }
  config.cache_control_pdf =
      get_env_string("CACHE_CONTROL_PDF", "no-store");
  config.log_level_choice = parse_log_level(get_env_string("LOG_LEVEL", "info"));
  config.build_log_buffer_lines = get_env_int("BUILD_LOG_BUFFER_LINES", 400);
  config.metrics_enabled = get_env_bool("METRICS_ENABLED", false);
  return config;
}

}  // namespace lf
