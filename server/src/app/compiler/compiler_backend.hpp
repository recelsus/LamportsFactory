#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "app/config/config.hpp"

namespace lf {

struct compiler_command {
  std::vector<std::string> args;
  std::filesystem::path working_dir;
  std::filesystem::path output_pdf;
};

class compiler_backend {
public:
  virtual ~compiler_backend() = default;

  virtual std::string name() const = 0;
  virtual compiler_command build_command(
      const app_config& config,
      const std::string& main_document) const = 0;
  virtual std::filesystem::path output_pdf_path(
      const app_config& config,
      const std::string& main_document) const = 0;
};

}  // namespace lf
