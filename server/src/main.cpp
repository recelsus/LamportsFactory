#include <filesystem>
#include <iostream>
#include <set>
#include <string>

#include "app/build/build_log_buffer.hpp"
#include "app/build/build_manager.hpp"
#include "app/config/config.hpp"
#include "app/events/event_bus.hpp"
#include "app/watch/file_watcher.hpp"
#include "app/http/server_handle.hpp"
#include "version.h"

namespace {

bool handle_cli(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "-h" || arg == "--help") {
      std::cout << "LamportsFactory server" << '\n';
      std::cout << "Usage: lf [options]" << '\n';
      std::cout << "  -h, --help        Show this help" << '\n';
      std::cout << "  -v, --version     Print version" << '\n';
      return true;
    }
    if (arg == "-v" || arg == "--version") {
      std::cout << "LamportsFactory " << LAMPORTS_FACTORY_VERSION << '\n';
      return true;
    }
  }
  return false;
}

}  // namespace

int main(int argc, char** argv) {
  if (handle_cli(argc, argv)) {
    return 0;
  }

  const lf::app_config config = lf::load_app_config();
  std::filesystem::create_directories(config.tex_dir);

  lf::set_http_log_level(config.log_level_choice);

  lf::build_log_buffer log_buffer(config.build_log_buffer_lines);
  lf::event_bus bus;
  lf::build_manager manager(config, bus, log_buffer);
  manager.start();
  if (config.initial_build) {
    manager.enqueue_build("initial");
  }

  lf::file_watcher watcher(config, [&](const std::set<std::string>&) {
    manager.enqueue_build("watcher");
  });
  watcher.start();

  const lf::server_handle server(config, bus, manager, log_buffer);
  std::cout << "LamportsFactory server listening on " << config.server_addr << ':'
            << config.server_port << '\n';
  server.run();

  bus.shutdown();
  watcher.stop();
  manager.stop();
  return 0;
}
