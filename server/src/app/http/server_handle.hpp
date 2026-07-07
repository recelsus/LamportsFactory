#pragma once

#include "app/build/build_log_buffer.hpp"
#include "app/build/build_manager.hpp"
#include "app/config/config.hpp"
#include "app/events/event_bus.hpp"

namespace lf {

class server_handle {
public:
  server_handle(const app_config& config, event_bus& bus,
                build_manager& manager, build_log_buffer& log_buffer);
  void run() const;

private:
  const app_config& config;
  event_bus& bus;
  build_manager& manager;
  build_log_buffer& log_buffer;
};

void set_http_log_level(log_level_setting level);

}  // namespace lf
