#pragma once

#include <memory>
#include <string>
#include <vector>

#include "app/compiler/compiler_backend.hpp"

namespace lf {

std::vector<std::string> supported_compiler_backends();
std::unique_ptr<compiler_backend> create_compiler_backend(
    const std::string& name);

}  // namespace lf
