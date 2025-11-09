#include "app/build_manager.hpp"

#include <chrono>
#include <filesystem>
#include <sstream>
#include <utility>

#include "app/utils.hpp"

namespace lf {

build_manager::build_manager(const app_config& config, event_bus& bus,
                             build_log_buffer& log_buffer)
    : config(config), bus(bus), log_buffer(log_buffer),
      current_main_tex(config.tex_main) {}

build_manager::~build_manager() {
  stop();
}

void build_manager::start() {
  std::lock_guard<std::mutex> lock(mutex);
  if (running) {
    return;
  }
  running = true;
  worker = std::thread([this]() { worker_loop(); });
}

void build_manager::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!running) {
      return;
    }
    running = false;
    stop_flag = true;
  }
  condition.notify_all();
  if (worker.joinable()) {
    worker.join();
  }
}

void build_manager::enqueue_build(const std::string& reason) {
  std::lock_guard<std::mutex> lock(mutex);
  if (stop_flag) {
    return;
  }
  if (!building && !pending_request) {
    pending_request = true;
    pending_reason = reason;
    condition.notify_one();
    return;
  }
  if (config.max_concurrent_builds <= 0) {
    return;
  }
  if (queued_requests >= config.max_concurrent_builds) {
    return;
  }
  queued_requests += 1;
}

build_snapshot build_manager::snapshot() const {
  std::lock_guard<std::mutex> lock(mutex);
  build_snapshot snap;
  snap.status = status_text;
  snap.building = building;
  snap.last_success = last_success;
  snap.ready = ready_flag;
  snap.pdf_mtime = pdf_mtime;
  snap.last_duration_ms = last_duration_ms;
  snap.last_error = last_error_message;
  snap.updated_at = last_update_ms;
  snap.current_main = current_main_tex;
  return snap;
}

bool build_manager::ready() const {
  std::lock_guard<std::mutex> lock(mutex);
  return ready_flag;
}

bool build_manager::set_main_target(const std::string& tex_main) {
  std::lock_guard<std::mutex> lock(mutex);
  if (tex_main.empty()) {
    return false;
  }
  if (tex_main == current_main_tex) {
    return false;
  }
  current_main_tex = tex_main;
  status_text = "idle";
  ready_flag = false;
  last_success = false;
  pdf_mtime.reset();
  return true;
}

std::string build_manager::current_main() const {
  std::lock_guard<std::mutex> lock(mutex);
  return current_main_tex;
}

void build_manager::worker_loop() {
  while (true) {
    std::string reason;
    {
      std::unique_lock<std::mutex> lock(mutex);
      condition.wait(lock, [&]() { return stop_flag || pending_request; });
      if (stop_flag) {
        break;
      }
      reason = pending_reason;
      pending_request = false;
      building = true;
      status_text = "building";
    }
    run_build(reason);
    {
      std::lock_guard<std::mutex> lock(mutex);
      building = false;
      if (queued_requests > 0) {
        queued_requests -= 1;
        pending_request = true;
        pending_reason = "queued";
        condition.notify_one();
      }
    }
  }
}

void build_manager::run_build(const std::string& reason) {
  const auto start = std::chrono::steady_clock::now();
  const int queued = queued_count();
  const std::string main_target = current_main();
  bus.publish("build_start",
              "{\"reason\":\"" + json_escape(reason) +
                  "\",\"queued\":" + std::to_string(queued) +
                  ",\"tex_main\":\"" + json_escape(main_target) + "\"}");
  process_result result = execute_tool();
  log_buffer.append(result.output);
  const auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - start)
                               .count();
  std::optional<std::int64_t> new_mtime;
  bool ok = !result.timed_out && result.exit_code == 0;
  std::string message;
  if (result.timed_out) {
    ok = false;
    message = "build timed out";
  } else if (result.exit_code != 0) {
    ok = false;
    message = "build failed";
  }
  if (ok) {
    new_mtime = probe_pdf_mtime(main_target);
    if (!new_mtime.has_value()) {
      ok = false;
      message = "pdf missing";
    }
  }
  {
    std::lock_guard<std::mutex> lock(mutex);
    last_duration_ms = duration_ms;
    last_update_ms = now_epoch_millis();
    if (ok) {
      status_text = "ok";
      last_success = true;
      ready_flag = true;
      last_error_message.clear();
      if (!pdf_mtime.has_value() || pdf_mtime.value() != new_mtime.value()) {
        pdf_mtime = new_mtime;
        bus.publish("pdf_updated",
                    "{\"pdf_mtime\":" +
                        std::to_string(pdf_mtime.value()) +
                        ",\"tex_main\":\"" +
                        json_escape(current_main_tex) + "\"}");
      } else {
        pdf_mtime = new_mtime;
      }
      bus.publish("build_ok",
                  "{\"pdf_mtime\":" +
                      std::to_string(pdf_mtime.value()) +
                      ",\"duration_ms\":" + std::to_string(duration_ms) +
                      ",\"tex_main\":\"" +
                      json_escape(current_main_tex) + "\"}");
    } else {
      status_text = "failed";
      last_success = false;
      last_error_message = message;
      bus.publish("build_fail",
                  "{\"message\":\"" + json_escape(message) +
                      "\",\"tail_log\":\"" +
                      json_escape(log_buffer.tail_joined(32)) +
                      "\",\"duration_ms\":" +
                      std::to_string(duration_ms) +
                      ",\"tex_main\":\"" + json_escape(current_main_tex) +
                      "\"}");
    }
  }
}

int build_manager::queued_count() const {
  std::lock_guard<std::mutex> lock(mutex);
  return queued_requests;
}

process_result build_manager::execute_tool() {
  std::vector<std::string> args;
  std::string main_target = current_main();
  if (main_target.empty()) {
    main_target = config.tex_main;
  }
  if (config.build_tool == "tectonic") {
    args.push_back("tectonic");
    for (const auto& opt : config.tectonic_opts) {
      args.push_back(opt);
    }
    args.push_back("--outdir=" + config.out_dir);
    args.push_back(main_target);
  } else {
    args.push_back("latexmk");
    for (const auto& opt : config.latexmk_opts) {
      args.push_back(opt);
    }
    args.push_back("-outdir=" + config.out_dir);
    args.push_back(main_target);
  }
  std::filesystem::create_directories(config.out_dir);
  return run_process(args, config.tex_dir, config.build_timeout_sec);
}

std::optional<std::int64_t> build_manager::probe_pdf_mtime(
    const std::string& main_target) {
  const auto pdf_path = pdf_path_for(config.out_dir, main_target, config.pdf_path);
  std::error_code ec;
  if (!std::filesystem::exists(pdf_path, ec)) {
    return std::nullopt;
  }
  const auto ft = std::filesystem::last_write_time(pdf_path, ec);
  if (ec) {
    return std::nullopt;
  }
  return to_epoch_millis(ft);
}

}  // namespace lf
