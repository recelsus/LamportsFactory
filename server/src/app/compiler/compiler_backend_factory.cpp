#include "app/compiler/compiler_backend_factory.hpp"

#include "app/common/utils.hpp"
#include "app/compiler/latex_backend.hpp"
#include "app/compiler/typst_backend.hpp"

namespace lf {

std::vector<std::string> supported_compiler_backends() {
  return {"latex", "typst"};
}

std::unique_ptr<compiler_backend> create_compiler_backend(
    const std::string& name) {
  const std::string normalized = to_lower_copy(trim_copy(name));
  if (normalized == "latex") {
    return std::make_unique<latex_backend>(load_latex_backend_config());
  }
  if (normalized == "typst") {
    return std::make_unique<typst_backend>(load_typst_backend_config());
  }
  return nullptr;
}

}  // namespace lf
