#include "app/http/server_handle.hpp"

#include <httplib.h>

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

#include "app/http/api_payloads.hpp"
#include "app/http/response.hpp"
#include "app/common/utils.hpp"
#include "app/workspace/workspace_documents.hpp"

namespace lf {
namespace {

void configure_routes(httplib::Server& server, const app_config& config,
                      event_bus& bus, build_manager& manager,
                      build_log_buffer& log_buffer) {
  server.Get("/healthz", [&](const httplib::Request& request,
                             httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    set_text_response(config, response, 200, "ok");
  });

  server.Get("/readyz", [&](const httplib::Request& request,
                            httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    if (manager.ready()) {
      set_text_response(config, response, 200, "ready");
    } else {
      set_text_response(config, response, 503, "not ready");
    }
  });

  server.Get("/", [&](const httplib::Request& request,
                      httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    const auto index_path =
        std::filesystem::path(config.static_dir) / "index.html";
    const std::string body = load_file_contents(index_path);
    if (body.empty()) {
      set_text_response(config, response, 500, "missing index.html");
      return;
    }
    response.status = 200;
    response.set_header("cache-control", "no-store");
    apply_cors(config, response);
    response.set_content(body, "text/html; charset=utf-8");
  });

  server.Get(config.pdf_route, [&](const httplib::Request& request,
                                   httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    write_pdf_response(config, manager, request, response);
  });

  server.Post("/api/build", [&](const httplib::Request& request,
                                httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    manager.enqueue_build("manual");
    set_json_response(config, response, 200, "{\"ok\":true}");
  });

  server.Get("/api/mtime", [&](const httplib::Request& request,
                               httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    set_json_response(config, response, 200,
                      build_snapshot_json(config, manager));
  });

  server.Get("/api/status", [&](const httplib::Request& request,
                                httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    set_json_response(config, response, 200,
                      build_snapshot_json(config, manager));
  });

  server.Get("/api/build/log", [&](const httplib::Request& request,
                                   httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    std::size_t tail_count = 40;
    if (request.has_param("tail")) {
      try {
        const int value = std::stoi(request.get_param_value("tail"));
        if (value > 0) {
          tail_count = static_cast<std::size_t>(value);
        }
      } catch (...) {
      }
    }
    const std::string body =
        "{\"tail\":\"" + json_escape(log_buffer.tail_joined(tail_count)) +
        "\"}";
    set_json_response(config, response, 200, body);
  });

  server.Get("/api/files", [&](const httplib::Request& request,
                               httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    const auto files = list_tex_documents(config);
    const std::string current =
        manager.current_main().empty() ? config.tex_main : manager.current_main();
    std::ostringstream body;
    body << "{\"files\":[";
    for (std::size_t i = 0; i < files.size(); ++i) {
      body << "{\"path\":\"" << json_escape(files[i]) << "\"}";
      if (i + 1 < files.size()) {
        body << ",";
      }
    }
    body << "],\"current\":\"" << json_escape(current) << "\"}";
    set_json_response(config, response, 200, body.str());
  });

  server.Post("/api/main", [&](const httplib::Request& request,
                               httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    const std::string target =
        request.has_param("tex") ? trim_copy(request.get_param_value("tex")) : "";
    if (target.empty()) {
      set_json_response(config, response, 400,
                        "{\"error\":\"missing tex parameter\"}");
      return;
    }
    const std::string normalised = resolve_tex_document(config, target);
    if (normalised.empty()) {
      set_json_response(config, response, 404, "{\"error\":\"tex not found\"}");
      return;
    }
    if (manager.set_main_target(normalised)) {
      manager.enqueue_build("main_changed");
    }
    set_json_response(config, response, 200,
                      "{\"ok\":true,\"tex_main\":\"" +
                          json_escape(manager.current_main()) + "\"}");
  });

  server.Get("/api/config", [&](const httplib::Request& request,
                                httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    set_json_response(config, response, 200, config_json(config));
  });

  server.Get("/events", [&](const httplib::Request& request,
                            httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    if (config.reload_choice != reload_mode::sse) {
      set_json_response(config, response, 409, "{\"error\":\"sse disabled\"}");
      return;
    }
    response.status = 200;
    response.set_header("cache-control", "no-store");
    response.set_header("connection", "keep-alive");
    apply_cors(config, response);
    response.set_chunked_content_provider(
        "text/event-stream; charset=utf-8",
        [&](std::size_t, httplib::DataSink& sink) {
          std::uint64_t sequence = bus.last_sequence();
          while (!bus.stopped() && sink.is_writable()) {
            const auto events =
                bus.wait(sequence, std::chrono::milliseconds(1000));
            if (events.empty()) {
              const std::string ping = sse_frame("ping", "{}");
              if (!sink.write(ping.data(), ping.size())) {
                return false;
              }
              continue;
            }
            for (const auto& event : events) {
              const std::string frame = sse_frame(event.name, event.data);
              if (!sink.write(frame.data(), frame.size())) {
                return false;
              }
              sequence = event.sequence;
            }
          }
          sink.done();
          return false;
        });
  });

  server.Get("/ws", [&](const httplib::Request& request,
                        httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    set_json_response(config, response, 410,
                      "{\"error\":\"websocket removed; use sse\"}");
  });

  server.Get("/metrics", [&](const httplib::Request& request,
                             httplib::Response& response) {
    if (!ensure_basic_auth(config, request, response)) {
      return;
    }
    if (!config.metrics_enabled) {
      set_text_response(config, response, 404, "metrics disabled");
      return;
    }
    const build_snapshot snap = manager.snapshot();
    std::ostringstream body;
    body << "lamportsfactory_last_duration_ms " << snap.last_duration_ms
         << '\n';
    body << "lamportsfactory_ready " << (snap.ready ? 1 : 0) << '\n';
    body << "lamportsfactory_last_success " << (snap.last_success ? 1 : 0)
         << '\n';
    if (snap.pdf_mtime.has_value()) {
      body << "lamportsfactory_pdf_mtime " << snap.pdf_mtime.value() << '\n';
    }
    set_text_response(config, response, 200, body.str());
  });
}

}  // namespace

server_handle::server_handle(const app_config& config, event_bus& bus,
                             build_manager& manager,
                             build_log_buffer& log_buffer)
    : config(config), bus(bus), manager(manager), log_buffer(log_buffer) {}

void server_handle::run() const {
  httplib::Server server;
  server.set_mount_point("/static", config.static_dir);
  configure_routes(server, config, bus, manager, log_buffer);
  if (!server.listen(config.server_addr, config.server_port)) {
    std::cerr << "failed to listen on " << config.server_addr << ':'
              << config.server_port << '\n';
  }
}

void set_http_log_level(log_level_setting) {}

}  // namespace lf
