#include "app/common/utils.hpp"

#include <chrono>
#include <filesystem>

namespace lf {

std::int64_t to_epoch_millis(const std::filesystem::file_time_type& ft) {
  using namespace std::chrono;
  const auto sctp = time_point_cast<milliseconds>(
      ft - std::filesystem::file_time_type::clock::now() + system_clock::now());
  return duration_cast<milliseconds>(sctp.time_since_epoch()).count();
}

std::int64_t now_epoch_millis() {
  using namespace std::chrono;
  const auto now = time_point_cast<milliseconds>(system_clock::now());
  return now.time_since_epoch().count();
}

}  // namespace lf
