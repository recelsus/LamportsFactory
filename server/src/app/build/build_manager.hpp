#pragma once

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <set>
#include <string>
#include <thread>

#include "app/build/build_log_buffer.hpp"
#include "app/compiler/compiler_backend.hpp"
#include "app/config/config.hpp"
#include "app/events/event_bus.hpp"

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
  std::string current_document;
};

class build_manager {
public:
  build_manager(const app_config& config, const compiler_backend& compiler,
                event_bus& bus,
                build_log_buffer& log_buffer);
  ~build_manager();

  void start();
  void stop();
  void enqueue_build(const std::string& reason);
  build_snapshot snapshot() const;
  bool ready() const;
  bool set_main_document(const std::string& main_document);
  std::string current_document() const;

private:
  void worker_loop();
  void run_build(const std::string& reason);
  int queued_count() const;
  const app_config& config;
  const compiler_backend& compiler;
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
  std::string current_document_path;
};

}  // namespace lf
