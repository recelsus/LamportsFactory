#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <vector>

namespace lf {

struct server_event {
  std::uint64_t sequence;
  std::string name;
  std::string data;
};

class event_bus {
public:
  void publish(const std::string& name, const std::string& data);
  std::vector<server_event> wait(std::uint64_t last_sequence,
                                 std::chrono::milliseconds timeout);
  std::uint64_t last_sequence() const;
  void shutdown();
  bool stopped() const;

private:
  mutable std::mutex mutex;
  std::condition_variable condition;
  std::deque<server_event> events;
  std::uint64_t sequence_counter = 0;
  bool stop_flag = false;
  static constexpr std::size_t max_events = 128;
};

}  // namespace lf

