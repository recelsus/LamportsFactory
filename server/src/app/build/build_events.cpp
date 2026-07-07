#include "app/build/build_events.hpp"

#include "app/common/utils.hpp"

namespace lf {

std::string build_start_payload(const std::string& reason, int queued,
                                const std::string& tex_main) {
  return "{\"reason\":\"" + json_escape(reason) + "\",\"queued\":" +
         std::to_string(queued) + ",\"tex_main\":\"" +
         json_escape(tex_main) + "\"}";
}

std::string pdf_updated_payload(std::int64_t pdf_mtime,
                                const std::string& tex_main) {
  return "{\"pdf_mtime\":" + std::to_string(pdf_mtime) +
         ",\"tex_main\":\"" + json_escape(tex_main) + "\"}";
}

std::string build_ok_payload(std::int64_t pdf_mtime, std::int64_t duration_ms,
                             const std::string& tex_main) {
  return "{\"pdf_mtime\":" + std::to_string(pdf_mtime) +
         ",\"duration_ms\":" + std::to_string(duration_ms) +
         ",\"tex_main\":\"" + json_escape(tex_main) + "\"}";
}

std::string build_fail_payload(const std::string& message,
                               const std::string& tail_log,
                               std::int64_t duration_ms,
                               const std::string& tex_main) {
  return "{\"message\":\"" + json_escape(message) + "\",\"tail_log\":\"" +
         json_escape(tail_log) + "\",\"duration_ms\":" +
         std::to_string(duration_ms) + ",\"tex_main\":\"" +
         json_escape(tex_main) + "\"}";
}

}  // namespace lf
