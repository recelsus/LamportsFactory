#include "app/workspace/workspace_documents.hpp"

#include <algorithm>
#include <filesystem>

#include "app/common/utils.hpp"

namespace lf {

std::vector<std::string> list_documents(const app_config& config) {
  std::vector<std::string> files;
  std::error_code ec;
  const std::filesystem::path base(config.workspace_dir);
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
      if (entry.path().extension() != config.document_extension) {
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
      if (!ec && is_build_output_path(config.workspace_dir, config.build_dir_name,
                                      it->path())) {
        it.disable_recursion_pending();
      }
      continue;
    }
    if (ec || !it->is_regular_file(ec)) {
      continue;
    }
    if (it->path().extension() != config.document_extension) {
      continue;
    }
    if (is_build_output_path(config.workspace_dir, config.build_dir_name,
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
  const auto main_document_path = trim_copy(config.main_document);
  files.erase(std::unique(files.begin(), files.end()), files.end());
  const auto main_document_full_path = base / main_document_path;
  if (!main_document_path.empty() && std::filesystem::exists(main_document_full_path, ec) &&
      std::find(files.begin(), files.end(), main_document_path) == files.end()) {
    files.insert(files.begin(), main_document_path);
  }
  return files;
}

std::string resolve_document(const app_config& config,
                                 const std::string& requested) {
  const std::string target = trim_copy(requested);
  if (target.empty()) {
    return "";
  }
  const std::filesystem::path absolute =
      std::filesystem::path(config.workspace_dir) / target;
  std::error_code ec;
  if (!std::filesystem::exists(absolute, ec) ||
      absolute.extension() != config.document_extension ||
      !path_is_within(config.workspace_dir, absolute)) {
    return "";
  }
  std::string normalised =
      std::filesystem::relative(absolute, config.workspace_dir, ec).generic_string();
  if (ec || normalised.empty()) {
    return target;
  }
  return normalised;
}

}  // namespace lf
