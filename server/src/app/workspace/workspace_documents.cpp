#include "app/workspace/workspace_documents.hpp"

#include <algorithm>
#include <filesystem>

#include "app/common/utils.hpp"

namespace lf {

std::vector<std::string> list_tex_documents(const app_config& config) {
  std::vector<std::string> files;
  std::error_code ec;
  const std::filesystem::path base(config.tex_dir);
  if (!std::filesystem::exists(base, ec)) {
    std::filesystem::path fallback("/app/workspace");
    if (!std::filesystem::exists(fallback, ec)) {
      return files;
    }
    for (const auto& entry : std::filesystem::recursive_directory_iterator(
             fallback,
             std::filesystem::directory_options::follow_directory_symlink, ec)) {
      if (ec || !entry.is_regular_file(ec)) {
        continue;
      }
      if (entry.path().extension() != ".tex") {
        continue;
      }
      auto rel = std::filesystem::relative(entry.path(), fallback, ec);
      if (ec) {
        continue;
      }
      files.push_back(rel.generic_string());
    }
    std::sort(files.begin(), files.end());
    files.erase(std::unique(files.begin(), files.end()), files.end());
    return files;
  }
  std::filesystem::recursive_directory_iterator it(
      base, std::filesystem::directory_options::follow_directory_symlink, ec);
  std::filesystem::recursive_directory_iterator end;
  for (; it != end; ++it) {
    ec.clear();
    if (it->is_directory(ec)) {
      if (!ec && is_build_output_path(config.tex_dir, config.build_dir_name,
                                      it->path())) {
        it.disable_recursion_pending();
      }
      continue;
    }
    if (ec || !it->is_regular_file(ec)) {
      continue;
    }
    if (it->path().extension() != ".tex") {
      continue;
    }
    if (is_build_output_path(config.tex_dir, config.build_dir_name,
                             it->path())) {
      continue;
    }
    auto rel = std::filesystem::relative(it->path(), base, ec);
    if (ec) {
      continue;
    }
    files.push_back(rel.generic_string());
  }
  std::sort(files.begin(), files.end());
  const auto tex_main_path = trim_copy(config.tex_main);
  files.erase(std::unique(files.begin(), files.end()), files.end());
  const auto tex_main_full_path = base / tex_main_path;
  if (!tex_main_path.empty() && std::filesystem::exists(tex_main_full_path, ec) &&
      std::find(files.begin(), files.end(), tex_main_path) == files.end()) {
    files.insert(files.begin(), tex_main_path);
  }
  return files;
}

std::string resolve_tex_document(const app_config& config,
                                 const std::string& requested) {
  const std::string target = trim_copy(requested);
  if (target.empty()) {
    return "";
  }
  const std::filesystem::path absolute =
      std::filesystem::path(config.tex_dir) / target;
  std::error_code ec;
  if (!std::filesystem::exists(absolute, ec) || absolute.extension() != ".tex" ||
      !path_is_within(config.tex_dir, absolute)) {
    return "";
  }
  std::string normalised =
      std::filesystem::relative(absolute, config.tex_dir, ec).generic_string();
  if (ec || normalised.empty()) {
    return target;
  }
  return normalised;
}

std::filesystem::path requested_pdf_path(const app_config& config,
                                         const std::string& current_main,
                                         const std::string& requested_tex) {
  std::string main = current_main.empty() ? config.tex_main : current_main;
  const std::string requested = trim_copy(requested_tex);
  if (!requested.empty() && requested != main) {
    main = requested;
  }
  return pdf_path_for(config.tex_dir, config.build_dir_name, main,
                      config.pdf_path)
      .lexically_normal();
}

}  // namespace lf
