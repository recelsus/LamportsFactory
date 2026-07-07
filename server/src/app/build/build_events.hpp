#pragma once

#include <cstdint>
#include <string>

namespace lf {

std::string build_start_payload(const std::string& reason, int queued,
                                const std::string& tex_main);

std::string pdf_updated_payload(std::int64_t pdf_mtime,
                                const std::string& tex_main);

std::string build_ok_payload(std::int64_t pdf_mtime, std::int64_t duration_ms,
                             const std::string& tex_main);

std::string build_fail_payload(const std::string& message,
                               const std::string& tail_log,
                               std::int64_t duration_ms,
                               const std::string& tex_main);

}  // namespace lf
