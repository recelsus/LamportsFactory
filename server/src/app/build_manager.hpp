#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

#include "app/build_log_buffer.hpp"
#include "app/config.hpp"
#include "app/event_bus.hpp"
#include "app/process.hpp"

namespace lf {

struct build_snapshot {
  std::string status;
  bool building;
  bool last_success;
  bool ready;
  std::optional<std::int64_t> pdf_mtime;
  std::int64_t last_duration_ms;
  std::string last_error;
  std::int64_t updated_at;
  std::string current_main;
};

class build_manager {
public:
  build_manager(const app_config& config, event_bus& bus,
                build_log_buffer& log_buffer);
  ~build_manager();

  void start();
  void stop();
  void enqueue_build(const std::string& reason);
  build_snapshot snapshot() const;
  bool ready() const;
  bool set_main_target(const std::string& tex_main);
  std::string current_main() const;

private:
  void worker_loop();
  void run_build(const std::string& reason);
  int queued_count() const;
  process_result execute_tool();
  std::optional<std::int64_t> probe_pdf_mtime(const std::string& main_target);

  const app_config& config;
  event_bus& bus;
  build_log_buffer& log_buffer;
  mutable std::mutex mutex;
  std::condition_variable condition;
  std::thread worker;
  bool running = false;
  bool stop_flag = false;
  bool pending_request = false;
  std::string pending_reason;
  bool building = false;
  bool ready_flag = false;
  std::string status_text = "idle";
  bool last_success = false;
  std::optional<std::int64_t> pdf_mtime;
  std::int64_t last_duration_ms = 0;
  std::string last_error_message;
  std::int64_t last_update_ms = 0;
  int queued_requests = 0;
  std::string current_main_tex;
};

}  // namespace lf
