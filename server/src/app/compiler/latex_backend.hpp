#pragma once

#include <string>
#include <vector>

#include "app/compiler/compiler_backend.hpp"

namespace lf {

struct latex_backend_config {
  std::string build_tool;
  std::string engine;
  std::vector<std::string> latexmk_opts;
  std::vector<std::string> tectonic_opts;
};

latex_backend_config load_latex_backend_config();

class latex_backend : public compiler_backend {
public:
  explicit latex_backend(latex_backend_config config);

  std::string name() const override;
  compiler_command build_command(
      const app_config& app_config,
      const std::string& main_document) const override;
  std::filesystem::path output_pdf_path(
      const app_config& app_config,
      const std::string& main_document) const override;

private:
  latex_backend_config config;
};

}  // namespace lf
