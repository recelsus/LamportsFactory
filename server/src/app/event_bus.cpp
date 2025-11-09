#include "app/event_bus.hpp"

namespace lf {

void event_bus::publish(const std::string& name, const std::string& data) {
  std::lock_guard<std::mutex> lock(mutex);
  const std::uint64_t seq = ++sequence_counter;
  events.push_back(server_event{seq, name, data});
  if (events.size() > max_events) {
    events.pop_front();
  }
  condition.notify_all();
}

std::vector<server_event> event_bus::wait(std::uint64_t last_sequence,
                                          std::chrono::milliseconds timeout) {
  std::unique_lock<std::mutex> lock(mutex);
  auto predicate = [&]() {
    return stop_flag || (!events.empty() &&
                         events.back().sequence > last_sequence);
  };
  if (timeout.count() < 0) {
    condition.wait(lock, predicate);
  } else {
    condition.wait_for(lock, timeout, predicate);
  }
  if (stop_flag) {
    return {};
  }
  std::vector<server_event> ready;
  for (const auto& event : events) {
    if (event.sequence > last_sequence) {
      ready.push_back(event);
    }
  }
  return ready;
}

std::uint64_t event_bus::last_sequence() const {
  std::lock_guard<std::mutex> lock(mutex);
  return sequence_counter;
}

void event_bus::shutdown() {
  std::lock_guard<std::mutex> lock(mutex);
  stop_flag = true;
  condition.notify_all();
}

bool event_bus::stopped() const {
  std::lock_guard<std::mutex> lock(mutex);
  return stop_flag;
}

}  // namespace lf

