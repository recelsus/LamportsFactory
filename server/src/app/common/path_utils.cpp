#include "app/common/utils.hpp"

#include <filesystem>

namespace lf {
std::filesystem::path normalize_build_dir_name(
    const std::string& build_dir_name) {
  std::filesystem::path name(build_dir_name);
  if (name.is_absolute()) {
    name = name.filename();
  }
  if (name.empty() || name == "." || name == "/") {
    return "build";
  }
  return name.lexically_normal();
}

bool path_is_within(const std::filesystem::path& parent,
                    const std::filesystem::path& child) {
  std::error_code ec;
  auto parent_canonical = std::filesystem::weakly_canonical(parent, ec);
  if (ec) {
    ec.clear();
    parent_canonical =
        std::filesystem::weakly_canonical(std::filesystem::absolute(parent),
                                          ec);
    if (ec) {
      return false;
    }
  }
  auto child_canonical = std::filesystem::weakly_canonical(child, ec);
  if (ec) {
    ec.clear();
    child_canonical =
        std::filesystem::weakly_canonical(std::filesystem::absolute(child), ec);
    if (ec) {
      return false;
    }
  }
  auto parent_it = parent_canonical.begin();
  auto child_it = child_canonical.begin();
  for (; parent_it != parent_canonical.end() && child_it != child_canonical.end();
       ++parent_it, ++child_it) {
    if (*parent_it != *child_it) {
      return false;
    }
  }
  return parent_it == parent_canonical.end();
}

std::filesystem::path output_pdf_path_for(const std::string& workspace_dir,
                                          const std::string& build_dir_name,
                                          const std::string& main_document,
                                          const std::string& fallback_pdf) {
  if (main_document.empty()) {
    if (!fallback_pdf.empty()) {
      return std::filesystem::path(fallback_pdf);
    }
    std::filesystem::path base(workspace_dir);
    return base / "main.pdf";
  }
  std::filesystem::path base =
      build_dir_for_document(workspace_dir, build_dir_name, main_document);
  std::filesystem::path main_path(main_document);
  std::string stem = main_path.stem().string();
  if (stem.empty()) {
    if (!fallback_pdf.empty()) {
      return std::filesystem::path(fallback_pdf);
    }
    stem = "main";
  }
  return base / (stem + ".pdf");
}

std::filesystem::path build_dir_for_document(
    const std::string& workspace_dir,
    const std::string& build_dir_name,
    const std::string& main_document) {
  std::filesystem::path base(workspace_dir);
  if (base.empty()) {
    base = std::filesystem::current_path();
  }
  const std::filesystem::path normalized_build_dir_name =
      normalize_build_dir_name(build_dir_name);
  if (main_document.empty()) {
    return base / normalized_build_dir_name;
  }
  const std::filesystem::path main_path(main_document);
  const std::filesystem::path parent = main_path.parent_path();
  if (parent.empty()) {
    return base / normalized_build_dir_name;
  }
  return base / parent.lexically_normal() / normalized_build_dir_name;
}

bool is_build_output_path(const std::string& workspace_dir,
                          const std::string& build_dir_name,
                          const std::filesystem::path& path) {
  std::error_code ec;
  const auto relative = std::filesystem::relative(path, workspace_dir, ec);
  if (ec) {
    return false;
  }
  const std::filesystem::path normalized_build_dir_name =
      normalize_build_dir_name(build_dir_name);
  for (const auto& part : relative) {
    if (part == normalized_build_dir_name) {
      return true;
    }
  }
  return false;
}

}  // namespace lf
