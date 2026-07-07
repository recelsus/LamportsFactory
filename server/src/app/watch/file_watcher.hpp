#pragma once

#include <chrono>
#include <filesystem>
#include <functional>
#include <map>
#include <mutex>
#include <regex>
#include <set>
#include <string>
#include <thread>
#include <sys/inotify.h>

#include "app/config/config.hpp"

namespace lf {

class file_watcher {
public:
  using callback_type = std::function<void(const std::set<std::string>&)>;

  file_watcher(const app_config& config, callback_type callback);
  ~file_watcher();

  void start();
  void stop();

private:
  void init_inotify();
  void register_recursive(const std::filesystem::path& root);
  void add_watch(const std::filesystem::path& path);
  bool matches_glob(const std::vector<std::regex>& patterns,
                    const std::string& relative) const;
  bool should_track_file(const std::filesystem::path& file_path) const;
  void handle_event(const struct inotify_event& event,
                    const std::filesystem::path& parent);
  void watch_loop();
  void read_events();

  const app_config& config;
  callback_type callback;
  mutable std::mutex mutex;
  std::thread worker;
  int inotify_fd = -1;
  std::map<int, std::filesystem::path> watch_paths;
  std::set<std::string> pending_changes;
  std::chrono::steady_clock::time_point last_change_time =
      std::chrono::steady_clock::now();
  bool running = false;
  bool stop_flag = false;
};

}  // namespace lf
