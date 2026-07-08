#include "app/compiler/typst_backend.hpp"

#include <filesystem>
#include <utility>

#include "app/common/utils.hpp"

namespace lf {

typst_backend_config load_typst_backend_config() {
  typst_backend_config config{};
  config.opts = split_whitespace(get_env_string("TYPST_OPTS", ""));
  return config;
}

typst_backend::typst_backend(typst_backend_config config)
    : config(std::move(config)) {}

std::string typst_backend::name() const {
  return "typst";
}

compiler_command typst_backend::build_command(
    const app_config& app_config,
    const std::string& main_document) const {
  compiler_command command{};
  const auto build_dir = build_dir_for_document(
      app_config.workspace_dir, app_config.build_dir_name, main_document);
  command.working_dir = app_config.workspace_dir;
  command.output_pdf = output_pdf_path(app_config, main_document);

  command.args.push_back("typst");
  command.args.push_back("compile");
  for (const auto& opt : config.opts) {
    command.args.push_back(opt);
  }
  command.args.push_back(main_document);
  command.args.push_back(command.output_pdf.string());

  std::filesystem::create_directories(build_dir);
  return command;
}

std::filesystem::path typst_backend::output_pdf_path(
    const app_config& app_config,
    const std::string& main_document) const {
  return output_pdf_path_for(app_config.workspace_dir,
                             app_config.build_dir_name, main_document,
                             app_config.pdf_path);
}

}  // namespace lf
