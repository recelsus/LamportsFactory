#pragma once

#include <string>
#include <vector>

namespace lf {

struct process_result {
  int exit_code;
  bool timed_out;
  std::string output;
};

process_result run_process(const std::vector<std::string>& args,
                           const std::string& workdir,
                           int timeout_sec);

}  // namespace lf

