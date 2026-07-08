#pragma once

#include <string>
#include <vector>

#include "app/compiler/compiler_backend.hpp"

namespace lf {

struct typst_backend_config {
  std::vector<std::string> opts;
};

typst_backend_config load_typst_backend_config();

class typst_backend : public compiler_backend {
public:
  explicit typst_backend(typst_backend_config config);

  std::string name() const override;
  compiler_command build_command(
      const app_config& config,
      const std::string& main_document) const override;
  std::filesystem::path output_pdf_path(
      const app_config& config,
      const std::string& main_document) const override;

private:
  typst_backend_config config;
};

}  // namespace lf
