#include "app/build/build_executor.hpp"

#include <filesystem>

#include "app/common/utils.hpp"

namespace lf {

process_result execute_build_tool(const app_config& config,
                                  const std::string& main_target) {
  std::vector<std::string> args;
  const auto build_dir =
      build_dir_for(config.tex_dir, config.build_dir_name, main_target);
  if (config.build_tool == "tectonic") {
    args.push_back("tectonic");
    for (const auto& opt : config.tectonic_opts) {
      args.push_back(opt);
    }
    args.push_back("--outdir=" + build_dir.string());
    args.push_back(main_target);
  } else {
    args.push_back("latexmk");
    for (const auto& opt : config.latexmk_opts) {
      args.push_back(opt);
    }
    args.push_back("-outdir=" + build_dir.string());
    args.push_back(main_target);
  }
  std::filesystem::create_directories(build_dir);
  return run_process(args, config.tex_dir, config.build_timeout_sec);
}

std::optional<std::int64_t> probe_pdf_mtime(const app_config& config,
                                            const std::string& main_target) {
  const auto pdf_path =
      pdf_path_for(config.tex_dir, config.build_dir_name, main_target,
                   config.pdf_path);
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
