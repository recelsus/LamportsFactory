#include "app/compiler/latex_backend.hpp"

#include <filesystem>
#include <utility>

#include "app/common/utils.hpp"

namespace lf {
namespace {

std::string default_latexmk_opts_for_engine(const std::string& engine) {
  if (engine == "pdflatex" || engine == "pdf") {
    return "-pdf -interaction=nonstopmode";
  }
  if (engine == "platex" || engine == "uplatex") {
    return "-pdfdvi -interaction=nonstopmode";
  }
  if (engine == "xelatex") {
    return "-xelatex -interaction=nonstopmode";
  }
  return "-lualatex -interaction=nonstopmode";
}

}  // namespace

latex_backend_config load_latex_backend_config() {
  latex_backend_config config{};
  config.build_tool =
      to_lower_copy(get_env_string("LATEX_BUILD_TOOL", "latexmk"));
  config.engine = to_lower_copy(get_env_string("LATEX_ENGINE", "lualatex"));
  config.latexmk_opts = split_whitespace(get_env_string(
      "LATEXMK_OPTS", default_latexmk_opts_for_engine(config.engine)));
  config.tectonic_opts =
      split_whitespace(get_env_string("TECTONIC_OPTS", ""));
  return config;
}

latex_backend::latex_backend(latex_backend_config config)
    : config(std::move(config)) {}

std::string latex_backend::name() const {
  return "latex";
}

compiler_command latex_backend::build_command(
    const app_config& app_config,
    const std::string& main_document) const {
  compiler_command command{};
  const auto build_dir = build_dir_for_document(
      app_config.workspace_dir, app_config.build_dir_name, main_document);
  command.working_dir = app_config.workspace_dir;
  command.output_pdf = output_pdf_path(app_config, main_document);

  if (config.build_tool == "tectonic") {
    command.args.push_back("tectonic");
    for (const auto& opt : config.tectonic_opts) {
      command.args.push_back(opt);
    }
    command.args.push_back("--outdir=" + build_dir.string());
    command.args.push_back(main_document);
  } else {
    command.args.push_back("latexmk");
    for (const auto& opt : config.latexmk_opts) {
      command.args.push_back(opt);
    }
    command.args.push_back("-outdir=" + build_dir.string());
    command.args.push_back(main_document);
  }

  std::filesystem::create_directories(build_dir);
  return command;
}

std::filesystem::path latex_backend::output_pdf_path(
    const app_config& app_config,
    const std::string& main_document) const {
  return output_pdf_path_for(app_config.workspace_dir,
                             app_config.build_dir_name, main_document,
                             app_config.pdf_path);
}

}  // namespace lf
