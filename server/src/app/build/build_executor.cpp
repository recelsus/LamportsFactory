#include "app/build/build_executor.hpp"

#include <filesystem>

#include "app/common/utils.hpp"

namespace lf {

process_result execute_build_tool(const app_config& config,
                                  const compiler_backend& compiler,
                                  const std::string& main_target) {
  const compiler_command command = compiler.build_command(config, main_target);
  return run_process(command.args, command.working_dir,
                     config.build_timeout_sec);
}

std::optional<std::int64_t> probe_pdf_mtime(const app_config& config,
                                            const compiler_backend& compiler,
                                            const std::string& main_target) {
  const auto pdf_path = compiler.output_pdf_path(config, main_target);
  std::error_code ec;
  if (!std::filesystem::exists(pdf_path, ec)) {
    return std::nullopt;
  }
  const auto ft = std::filesystem::last_write_time(pdf_path, ec);
  if (ec) {
    return std::nullopt;
  }
  return to_epoch_millis(ft);
}

}  // namespace lf
