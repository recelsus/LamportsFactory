#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace lf {

std::string get_env_string(const std::string& name, const std::string& fallback);
int get_env_int(const std::string& name, int fallback);
bool get_env_bool(const std::string& name, bool fallback);
std::vector<std::string> split_semicolon(const std::string& text);
std::vector<std::string> split_whitespace(const std::string& text);
std::string trim_copy(const std::string& text);
std::string to_lower_copy(const std::string& text);
std::string json_escape(const std::string& text);
std::string base64_encode(const std::string& input);
std::string sha1_hash_base64(const std::string& text);
std::int64_t to_epoch_millis(const std::filesystem::file_time_type& ft);
std::int64_t now_epoch_millis();
bool path_is_within(const std::filesystem::path& parent, const std::filesystem::path& child);
std::vector<std::string> split_multi_glob(const std::string& text);
std::vector<std::regex> compile_globs(const std::vector<std::string>& globs);
std::filesystem::path pdf_path_for(const std::string& out_dir,
                                   const std::string& tex_main,
                                   const std::string& fallback_pdf);

}  // namespace lf
