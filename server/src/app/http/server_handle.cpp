#include "app/http/server_handle.hpp"

#include <clask/core.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include "app/utils.hpp"

namespace lf {
namespace {

bool ensure_basic_auth(const app_config& config, clask::response_writer& writer,
                       clask::request& request) {
  if (config.basic_auth_header.empty()) {
    return true;
  }
  const std::string header = request.header_value("Authorization");
  if (header == config.basic_auth_header) {
    return true;
  }
  writer.code = 401;
  writer.set_header("WWW-Authenticate", "Basic realm=\"LamportsFactory\"");
  writer.set_header("content-type", "text/plain; charset=utf-8");
  writer.write("Unauthorized");
  return false;
}

void apply_cors(const app_config& config, clask::response_writer& writer) {
  if (!config.cors_allow_origin.empty()) {
    writer.set_header("Access-Control-Allow-Origin", config.cors_allow_origin);
  }
}

std::string websocket_accept_key(const std::string& client_key) {
  static const std::string magic =
      "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
  return sha1_hash_base64(client_key + magic);
}

void write_websocket_text(clask::response_writer& writer,
                          const std::string& payload) {
  std::string frame;
  frame.push_back(static_cast<char>(0x81));
  const std::size_t len = payload.size();
  if (len < 126) {
    frame.push_back(static_cast<char>(len));
  } else if (len <= 0xFFFF) {
    frame.push_back(126);
    frame.push_back(static_cast<char>((len >> 8) & 0xFF));
    frame.push_back(static_cast<char>(len & 0xFF));
  } else {
    frame.push_back(127);
    for (int i = 7; i >= 0; --i) {
      frame.push_back(static_cast<char>((len >> (8 * i)) & 0xFF));
    }
  }
  frame.append(payload);
  writer.write(frame.data(), frame.size());
}

std::vector<std::string> list_tex_documents(const app_config& config) {
  std::vector<std::string> files;
  std::error_code ec;
  const std::filesystem::path base(config.tex_dir);
  if (!std::filesystem::exists(base, ec)) {
    std::filesystem::path fallback("/app/workspace");
    if (!std::filesystem::exists(fallback, ec)) {
      return files;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             fallback, std::filesystem::directory_options::follow_directory_symlink, ec)) {
      if (ec || !entry.is_regular_file(ec)) {
        continue;
      }
      if (entry.path().extension() != ".tex") {
        continue;
      }
      files.push_back(entry.path().filename().generic_string());
    }
    if (files.empty()) {
      files.push_back("main.tex");
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
  }
  const std::filesystem::path out_dir(config.out_dir);
  std::filesystem::recursive_directory_iterator it(
      base, std::filesystem::directory_options::follow_directory_symlink, ec);
  std::filesystem::recursive_directory_iterator end;
  for (; it != end; ++it) {
    ec.clear();
    if (it->is_directory(ec)) {
      if (!ec && path_is_within(out_dir, it->path())) {
        it.disable_recursion_pending();
      }
      continue;
    }
    if (ec || !it->is_regular_file(ec)) {
      continue;
    }
    if (it->path().extension() != ".tex") {
      continue;
    }
    if (path_is_within(out_dir, it->path())) {
      continue;
    }
    auto rel = std::filesystem::relative(it->path(), base, ec);
    if (ec) {
      continue;
    }
    files.push_back(rel.generic_string());
  }
  std::sort(files.begin(), files.end());
  const auto tex_main_path = trim_copy(config.tex_main);
  files.erase(std::unique(files.begin(), files.end()), files.end());
  if (!tex_main_path.empty() && std::find(files.begin(), files.end(), tex_main_path) == files.end()) {
    files.insert(files.begin(), tex_main_path);
  }
  if (files.empty() && !tex_main_path.empty()) {
    files.push_back(tex_main_path);
  } else if (files.empty()) {
    files.push_back("main.tex");
  }
  if (std::find(files.begin(), files.end(), "sample.tex") == files.end()) {
    files.push_back("sample.tex");
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
  }
  return files;
}

std::string build_config_json(const app_config& config) {
  std::ostringstream out;
  out << "{\"reload_mode\":\""
      << (config.reload_choice == reload_mode::ws
              ? "ws"
              : (config.reload_choice == reload_mode::poll ? "poll" : "sse"))
      << "\",";
  out << "\"reload_interval_ms\":" << config.reload_interval_ms << ",";
  out << "\"reload_debounce_ms\":" << config.reload_debounce_ms << ",";
  out << "\"reload_retry_ms\":" << config.reload_retry_ms;
  out << "}";
  return out.str();
}

void write_pdf_response(const app_config& config, const build_manager& manager,
                        clask::response_writer& writer,
                        const clask::request& request) {
  std::string requested = request.uri_params.count("tex")
                              ? trim_copy(request.uri_params.at("tex"))
                              : "";
  std::string current_main = manager.current_main().empty()
                                 ? config.tex_main
                                 : manager.current_main();
  if (!requested.empty() && requested != current_main) {
    current_main = requested;
  }
  const auto pdf_path =
      pdf_path_for(config.out_dir, current_main, config.pdf_path).lexically_normal();
  std::error_code ec;
  if (!std::filesystem::exists(pdf_path, ec)) {
    writer.code = 404;
    writer.set_header("content-type", "text/plain; charset=utf-8");
    apply_cors(config, writer);
    writer.write("pdf not found");
    return;
  }
  std::ifstream file(pdf_path, std::ios::binary);
  if (!file) {
    writer.code = 500;
    writer.set_header("content-type", "text/plain; charset=utf-8");
    apply_cors(config, writer);
    writer.write("failed to read pdf");
    return;
  }
  writer.code = 200;
  writer.set_header("content-type", "application/pdf");
  writer.set_header("cache-control", config.cache_control_pdf);
  apply_cors(config, writer);
  char buffer[8192];
  while (file.good()) {
    file.read(buffer, sizeof(buffer));
    const std::streamsize count = file.gcount();
    if (count > 0) {
      writer.write(buffer, static_cast<std::size_t>(count));
    }
  }
  writer.end();
}

std::string load_file_contents(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::in | std::ios::binary);
  if (!stream) {
    return "";
  }
  std::ostringstream buffer;
  buffer << stream.rdbuf();
  return buffer.str();
}

void configure_routes(clask::server_t& server, const app_config& config,
                      event_bus& bus, build_manager& manager,
                      build_log_buffer& log_buffer) {
  auto send_snapshot_json = [&](clask::response_writer& writer) {
    const build_snapshot snap = manager.snapshot();
    const auto files = list_tex_documents(config);
    std::ostringstream body;
    body << "{\"status\":\"" << snap.status << "\",";
    body << "\"building\":" << (snap.building ? "true" : "false") << ",";
    body << "\"ready\":" << (snap.ready ? "true" : "false") << ",";
    body << "\"last_success\":" << (snap.last_success ? "true" : "false")
         << ",";
    body << "\"last_duration_ms\":" << snap.last_duration_ms << ",";
    body << "\"updated_at\":" << snap.updated_at;
    if (snap.pdf_mtime.has_value()) {
      body << ",\"pdf_mtime\":" << snap.pdf_mtime.value();
    }
    if (!snap.last_error.empty()) {
      body << ",\"last_error\":\"" << json_escape(snap.last_error) << "\"";
    }
    if (!snap.current_main.empty()) {
      body << ",\"tex_main\":\"" << json_escape(snap.current_main) << "\"";
    }
    body << ",\"files\":[";
    for (std::size_t i = 0; i < files.size(); ++i) {
      body << "\"" << json_escape(files[i]) << "\"";
      if (i + 1 < files.size()) {
        body << ",";
      }
    }
    body << "]";
    body << "}";
    writer.code = 200;
    writer.set_header("content-type", "application/json; charset=utf-8");
    apply_cors(config, writer);
    writer.write(body.str());
  };

  server.GET("/healthz", [&](clask::response_writer& writer,
                             clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    writer.code = 200;
    writer.set_header("content-type", "text/plain; charset=utf-8");
    apply_cors(config, writer);
    writer.write("ok");
  });

  server.GET("/readyz", [&](clask::response_writer& writer,
                             clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    if (manager.ready()) {
      writer.code = 200;
      writer.set_header("content-type", "text/plain; charset=utf-8");
      apply_cors(config, writer);
      writer.write("ready");
    } else {
      writer.code = 503;
      writer.set_header("content-type", "text/plain; charset=utf-8");
      apply_cors(config, writer);
      writer.write("not ready");
    }
  });

  server.GET("/", [&](clask::response_writer& writer, clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    const auto index_path = std::filesystem::path(config.static_dir) / "index.html";
    const std::string body = load_file_contents(index_path);
    if (body.empty()) {
      writer.code = 500;
      writer.set_header("content-type", "text/plain; charset=utf-8");
      apply_cors(config, writer);
      writer.write("missing index.html");
      return;
    }
    writer.code = 200;
    writer.set_header("content-type", "text/html; charset=utf-8");
    writer.set_header("cache-control", "no-store");
    apply_cors(config, writer);
    writer.write(body);
  });

  server.GET(config.pdf_route, [&](clask::response_writer& writer,
                                   clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    write_pdf_response(config, manager, writer, request);
  });

  server.POST("/api/build", [&](clask::response_writer& writer,
                                 clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    manager.enqueue_build("manual");
    writer.code = 200;
    writer.set_header("content-type", "application/json; charset=utf-8");
    apply_cors(config, writer);
    writer.write("{\"ok\":true}");
  });

  server.GET("/api/mtime", [&](clask::response_writer& writer,
                                 clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    send_snapshot_json(writer);
  });

  server.GET("/api/build/log", [&](clask::response_writer& writer,
                                    clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    std::size_t tail_count = 40;
    auto it = request.uri_params.find("tail");
    if (it != request.uri_params.end()) {
      try {
        const int value = std::stoi(it->second);
        if (value > 0) {
          tail_count = static_cast<std::size_t>(value);
        }
      } catch (...) {
      }
    }
    const std::string body =
        "{\"tail\":\"" + json_escape(log_buffer.tail_joined(tail_count)) +
        "\"}";
    writer.code = 200;
    writer.set_header("content-type", "application/json; charset=utf-8");
    apply_cors(config, writer);
    writer.write(body);
  });

  server.GET("/api/files", [&](clask::response_writer& writer,
                                 clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    const auto files = list_tex_documents(config);
    const std::string current = manager.current_main().empty()
                                    ? config.tex_main
                                    : manager.current_main();
    std::ostringstream body;
    body << "{\"files\":[";
    for (std::size_t i = 0; i < files.size(); ++i) {
      body << "{\"path\":\"" << json_escape(files[i]) << "\"}";
      if (i + 1 < files.size()) {
        body << ",";
      }
    }
    body << "],\"current\":\"" << json_escape(current) << "\"}";
    writer.code = 200;
    writer.set_header("content-type", "application/json; charset=utf-8");
    apply_cors(config, writer);
    writer.write(body.str());
  });

  server.POST("/api/main", [&](clask::response_writer& writer,
                                 clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    auto it = request.uri_params.find("tex");
    std::string target = (it != request.uri_params.end()) ? it->second : "";
    target = trim_copy(target);
    if (target.empty()) {
      writer.code = 400;
      writer.set_header("content-type", "application/json; charset=utf-8");
      apply_cors(config, writer);
      writer.write("{\"error\":\"missing tex parameter\"}");
      return;
    }
    std::filesystem::path absolute =
        std::filesystem::path(config.tex_dir) / target;
    std::error_code ec;
    if (!std::filesystem::exists(absolute, ec) ||
        absolute.extension() != ".tex" ||
        !path_is_within(config.tex_dir, absolute)) {
      writer.code = 404;
      writer.set_header("content-type", "application/json; charset=utf-8");
      apply_cors(config, writer);
      writer.write("{\"error\":\"tex not found\"}");
      return;
    }
    std::string normalised =
        std::filesystem::relative(absolute, config.tex_dir, ec).generic_string();
    if (ec || normalised.empty()) {
      normalised = target;
    }
    if (manager.set_main_target(normalised)) {
      manager.enqueue_build("main_changed");
    }
    writer.code = 200;
    writer.set_header("content-type", "application/json; charset=utf-8");
    apply_cors(config, writer);
    writer.write("{\"ok\":true,\"tex_main\":\"" +
                 json_escape(manager.current_main()) + "\"}");
  });

  server.GET("/api/config", [&](clask::response_writer& writer,
                                  clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    writer.code = 200;
    writer.set_header("content-type", "application/json; charset=utf-8");
    apply_cors(config, writer);
    writer.write(build_config_json(config));
  });

  server.GET("/events", [&](clask::response_writer& writer,
                              clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    if (config.reload_choice != reload_mode::sse) {
      writer.code = 409;
      writer.set_header("content-type", "application/json; charset=utf-8");
      apply_cors(config, writer);
      writer.write("{\"error\":\"sse disabled\"}");
      return;
    }
    writer.code = 200;
    writer.set_header("content-type", "text/event-stream; charset=utf-8");
    writer.set_header("cache-control", "no-store");
    writer.set_header("connection", "keep-alive");
    apply_cors(config, writer);
    clask::server_sent_event_writer sse(writer);
    std::uint64_t sequence = bus.last_sequence();
    while (!bus.stopped()) {
      const auto events = bus.wait(sequence, std::chrono::milliseconds(1000));
      if (events.empty()) {
        sse.write("ping", "{}");
        continue;
      }
      for (const auto& event : events) {
        sse.write(event.name, event.data);
        sequence = event.sequence;
      }
    }
    writer.end();
  });

  server.GET("/ws", [&](clask::response_writer& writer,
                          clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    if (config.reload_choice != reload_mode::ws) {
      writer.code = 409;
      writer.set_header("content-type", "application/json; charset=utf-8");
      apply_cors(config, writer);
      writer.write("{\"error\":\"websocket disabled\"}");
      return;
    }
    const std::string upgrade = to_lower_copy(request.header_value("Upgrade"));
    const std::string connection =
        to_lower_copy(request.header_value("Connection"));
    const std::string key = trim_copy(request.header_value("Sec-WebSocket-Key"));
    if (upgrade != "websocket" || key.empty() ||
        connection.find("upgrade") == std::string::npos) {
      writer.code = 400;
      writer.set_header("content-type", "application/json; charset=utf-8");
      apply_cors(config, writer);
      writer.write("{\"error\":\"invalid handshake\"}");
      return;
    }
    writer.code = 101;
    writer.set_header("Upgrade", "websocket");
    writer.set_header("Connection", "Upgrade");
    writer.set_header("Sec-WebSocket-Accept", websocket_accept_key(key));
    apply_cors(config, writer);
    writer.write_headers();
    std::uint64_t sequence = bus.last_sequence();
    while (!bus.stopped()) {
      const auto events = bus.wait(sequence, std::chrono::milliseconds(1000));
      if (events.empty()) {
        write_websocket_text(writer, "{\"type\":\"ping\"}");
        continue;
      }
      for (const auto& event : events) {
        write_websocket_text(writer, "{\"event\":\"" + event.name +
                                         "\",\"data\":" + event.data +
                                         "}");
        sequence = event.sequence;
      }
    }
    writer.end();
  });

  server.GET("/metrics", [&](clask::response_writer& writer,
                               clask::request& request) {
    if (!ensure_basic_auth(config, writer, request)) {
      return;
    }
    if (!config.metrics_enabled) {
      writer.code = 404;
      writer.set_header("content-type", "text/plain; charset=utf-8");
      apply_cors(config, writer);
      writer.write("metrics disabled");
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
    writer.code = 200;
    writer.set_header("content-type", "text/plain; charset=utf-8");
    apply_cors(config, writer);
    writer.write(body.str());
  });
}

}  // namespace

server_handle::server_handle(const app_config& config, event_bus& bus,
                             build_manager& manager,
                             build_log_buffer& log_buffer)
    : config(config), bus(bus), manager(manager), log_buffer(log_buffer) {}

void server_handle::run() const {
  clask::server_t server;
  server.static_dir("/static", config.static_dir);
  configure_routes(server, config, bus, manager, log_buffer);
  server.run(config.server_addr + ":" + std::to_string(config.server_port));
}

void set_clask_log_level(log_level_setting level) {
  switch (level) {
    case log_level_setting::debug:
      clask::logger::level() = clask::log_level::DEBUG;
      break;
    case log_level_setting::warn:
      clask::logger::level() = clask::log_level::WARN;
      break;
    case log_level_setting::error:
      clask::logger::level() = clask::log_level::ERR;
      break;
    default:
      clask::logger::level() = clask::log_level::INFO;
      break;
  }
}

}  // namespace lf
