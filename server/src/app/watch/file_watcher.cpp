#include "app/watch/file_watcher.hpp"

#include <poll.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>

#include "app/common/utils.hpp"

namespace lf {

file_watcher::file_watcher(const app_config& config, callback_type callback)
    : config(config), callback(std::move(callback)) {}

file_watcher::~file_watcher() {
  stop();
}

void file_watcher::start() {
  std::lock_guard<std::mutex> lock(mutex);
  if (running) {
    return;
  }
  init_inotify();
  running = true;
  worker = std::thread([this]() { watch_loop(); });
}

void file_watcher::stop() {
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!running) {
      return;
    }
    running = false;
    stop_flag = true;
  }
  if (inotify_fd >= 0) {
    close(inotify_fd);
    inotify_fd = -1;
  }
  if (worker.joinable()) {
    worker.join();
  }
}

void file_watcher::init_inotify() {
  inotify_fd = inotify_init1(IN_NONBLOCK);
  if (inotify_fd < 0) {
    return;
  }
  register_recursive(config.tex_dir);
}

void file_watcher::register_recursive(const std::filesystem::path& root) {
  std::error_code ec;
  if (!std::filesystem::exists(root, ec)) {
    return;
  }
  add_watch(root);
  for (const auto& entry : std::filesystem::recursive_directory_iterator(
           root, std::filesystem::directory_options::follow_directory_symlink,
           ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_directory(ec)) {
      continue;
    }
    const auto path = entry.path();
    if (is_build_output_path(config.tex_dir, config.build_dir_name, path)) {
      continue;
    }
    add_watch(path);
  }
}

void file_watcher::add_watch(const std::filesystem::path& path) {
  std::error_code ec;
  if (!std::filesystem::exists(path, ec)) {
    return;
  }
  const int mask = IN_CLOSE_WRITE | IN_MODIFY | IN_MOVED_TO | IN_CREATE |
                   IN_DELETE | IN_ATTRIB | IN_DELETE_SELF | IN_MOVE_SELF;
  const int wd = inotify_add_watch(inotify_fd, path.c_str(), mask);
  if (wd >= 0) {
    watch_paths[wd] = path;
  }
}

bool file_watcher::matches_glob(const std::vector<std::regex>& patterns,
                                const std::string& relative) const {
  if (patterns.empty()) {
    return true;
  }
  for (const auto& pattern : patterns) {
    if (std::regex_match(relative, pattern)) {
      return true;
    }
  }
  return false;
}

bool file_watcher::should_track_file(
    const std::filesystem::path& file_path) const {
  if (is_build_output_path(config.tex_dir, config.build_dir_name, file_path)) {
    return false;
  }
  std::error_code ec;
  std::filesystem::path relative =
      std::filesystem::relative(file_path, config.tex_dir, ec);
  if (ec) {
    relative = file_path;
  }
  const std::string rel = relative.generic_string();
  if (!matches_glob(config.watch_globs, rel)) {
    return false;
  }
  if (matches_glob(config.watch_ignore, rel)) {
    return false;
  }
  return true;
}

void file_watcher::handle_event(const struct inotify_event& event,
                                const std::filesystem::path& parent) {
  if ((event.mask & IN_ISDIR) != 0U) {
    if ((event.mask & IN_CREATE) != 0U || (event.mask & IN_MOVED_TO) != 0U) {
      const std::filesystem::path new_path = parent / event.name;
      if (!is_build_output_path(config.tex_dir, config.build_dir_name,
                                new_path)) {
        add_watch(new_path);
      }
    }
    return;
  }
  const std::filesystem::path file_path = parent / event.name;
  if (!should_track_file(file_path)) {
    return;
  }
  pending_changes.insert(file_path.generic_string());
  last_change_time = std::chrono::steady_clock::now();
}

void file_watcher::watch_loop() {
  if (inotify_fd < 0) {
    return;
  }
  const int timeout_ms = std::max(config.file_change_batch_window_ms, 50);
  while (!stop_flag) {
    struct pollfd pfd;
    pfd.fd = inotify_fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    const int ready = poll(&pfd, 1, timeout_ms);
    if (ready > 0 && (pfd.revents & POLLIN) != 0) {
      read_events();
    }
    if (pending_changes.empty()) {
      continue;
    }
    const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - last_change_time);
    if (since.count() >= config.file_change_batch_window_ms) {
      const std::set<std::string> batch = pending_changes;
      pending_changes.clear();
      callback(batch);
    }
  }
}

void file_watcher::read_events() {
  alignas(inotify_event) char buffer[4096];
  while (true) {
    const ssize_t bytes = read(inotify_fd, buffer, sizeof(buffer));
    if (bytes <= 0) {
      break;
    }
    std::size_t offset = 0;
    while (offset < static_cast<std::size_t>(bytes)) {
      auto* event = reinterpret_cast<inotify_event*>(buffer + offset);
      auto it = watch_paths.find(event->wd);
      if (it != watch_paths.end()) {
        handle_event(*event, it->second);
      }
      offset += sizeof(inotify_event) + event->len;
    }
  }
}

}  // namespace lf
