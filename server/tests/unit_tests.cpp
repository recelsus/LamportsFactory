#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "app/build/build_events.hpp"
#include "app/build/build_log_buffer.hpp"
#include "app/common/utils.hpp"
#include "app/config/config.hpp"
#include "app/events/event_bus.hpp"
#include "app/http/response.hpp"
#include "app/workspace/workspace_documents.hpp"

namespace {

class test_failure : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw test_failure(message);
  }
}

template <typename T, typename U>
void require_eq(const T& actual, const U& expected, const std::string& message) {
  if (!(actual == expected)) {
    throw test_failure(message);
  }
}

class env_guard {
public:
  explicit env_guard(std::vector<std::string> names) : names(std::move(names)) {
    for (const auto& name : this->names) {
      const char* value = std::getenv(name.c_str());
      values.push_back(value == nullptr ? "" : value);
      present.push_back(value != nullptr);
    }
  }

  ~env_guard() {
    for (std::size_t i = 0; i < names.size(); ++i) {
      if (present[i]) {
        setenv(names[i].c_str(), values[i].c_str(), 1);
      } else {
        unsetenv(names[i].c_str());
      }
    }
  }

private:
  std::vector<std::string> names;
  std::vector<std::string> values;
  std::vector<bool> present;
};

std::filesystem::path make_temp_dir(const std::string& name) {
  const auto base = std::filesystem::temp_directory_path() /
                    ("lamportsfactory-" + name + "-" +
                     std::to_string(std::rand()));
  std::filesystem::create_directories(base);
  return base;
}

void write_file(const std::filesystem::path& path, const std::string& body) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::binary);
  out << body;
}

lf::app_config minimal_config(const std::filesystem::path& tex_dir) {
  lf::app_config config{};
  config.tex_dir = tex_dir.string();
  config.out_dir = (tex_dir / "build").string();
  config.build_dir_name = "build";
  config.tex_main = "tex/main.tex";
  config.pdf_path = "";
  config.reload_choice = lf::reload_mode::sse;
  config.layout_mode = "preview";
  config.ttyd_enabled = false;
  return config;
}

void test_common_utils() {
  require_eq(lf::trim_copy("  abc \n"), std::string("abc"), "trim_copy");
  require_eq(lf::to_lower_copy("AbC"), std::string("abc"), "to_lower_copy");
  require_eq(lf::split_semicolon("a;b;;c").size(), std::size_t{3},
             "split_semicolon count");
  const auto words = lf::split_whitespace(" latexmk  -pdf main.tex ");
  require_eq(words.size(), std::size_t{3}, "split_whitespace count");
  require_eq(words[1], std::string("-pdf"), "split_whitespace item");

  require_eq(lf::json_escape("a\"b\nc"), std::string("a\\\"b\\nc"),
             "json_escape");
  require_eq(lf::base64_encode("user:pass"), std::string("dXNlcjpwYXNz"),
             "base64_encode");
  require_eq(lf::sha1_hash_base64("hello"),
             std::string("qvTGHdzF6KLavt4PO0gs2a6pQ00="),
             "sha1_hash_base64");
}

void test_globs_and_paths() {
  const auto globs = lf::split_multi_glob("**/*.tex,src/*.bib; ");
  require_eq(globs.size(), std::size_t{2}, "split_multi_glob count");
  const auto patterns = lf::compile_globs(globs);
  require(std::regex_match(std::string("paper/main.tex"), patterns[0]),
          "recursive tex glob");
  require(std::regex_match(std::string("src/ref.bib"), patterns[1]),
          "single-level bib glob");

  const auto temp = make_temp_dir("paths");
  std::filesystem::create_directories(temp / "a");
  write_file(temp / "a" / "main.tex", "\\documentclass{article}");
  require(lf::path_is_within(temp, temp / "a" / "main.tex"),
          "path_is_within child");
  require(!lf::path_is_within(temp / "a", temp / "b" / "main.tex"),
          "path_is_within sibling");

  require_eq(lf::pdf_path_for(temp.string(), "build",
                              "tex/main.tex", "")
                 .string(),
             (temp / "tex" / "build" / "main.pdf").string(),
             "pdf_path_for sibling build dir");
  require_eq(lf::build_dir_for(temp.string(), "build",
                               "tex/main.tex")
                 .string(),
             (temp / "tex" / "build").string(),
             "build_dir_for sibling build dir");
  require_eq(lf::build_dir_for(temp.string(), "build",
                               "main.tex")
                 .string(),
             (temp / "build").string(), "build_dir_for root tex");
  require(lf::is_build_output_path(temp.string(), "build",
                                   temp / "tex" / "build" / "main.aux"),
          "is_build_output_path nested");
  std::filesystem::remove_all(temp);
}

void test_config_env() {
  env_guard guard({"BASE_URL", "TTYD_ENABLED", "LAYOUT_MODE", "LATEX_ENGINE",
                   "LATEXMK_OPTS", "STATIC_DIR", "TEX_DIR", "OUT_DIR",
                   "BUILD_DIR_NAME", "TEX_MAIN"});
  setenv("BASE_URL", "preview/", 1);
  setenv("TTYD_ENABLED", "disable", 1);
  setenv("LAYOUT_MODE", "split", 1);
  setenv("LATEX_ENGINE", "platex", 1);
  unsetenv("LATEXMK_OPTS");
  setenv("STATIC_DIR", "/tmp", 1);
  setenv("OUT_DIR", "/app/workspace/legacy-build", 1);
  unsetenv("BUILD_DIR_NAME");

  const lf::app_config config = lf::load_app_config();
  require_eq(config.base_url, std::string("/preview"), "base url normalize");
  require_eq(config.layout_mode, std::string("preview"),
             "split falls back when ttyd disabled");
  require(!config.ttyd_enabled, "ttyd disabled");
  require_eq(config.ttyd_url, std::string("/preview/terminal/"),
             "default ttyd url joins base url");
  require_eq(config.latexmk_opts[0], std::string("-pdfdvi"),
             "platex default latexmk option");
  require_eq(config.build_dir_name, std::string("legacy-build"),
             "build dir name falls back to OUT_DIR filename");

  setenv("TTYD_ENABLED", "enable", 1);
  setenv("LAYOUT_MODE", "split", 1);
  setenv("LATEXMK_OPTS", "-xelatex -halt-on-error", 1);
  setenv("BUILD_DIR_NAME", "build", 1);
  const lf::app_config enabled = lf::load_app_config();
  require(enabled.ttyd_enabled, "ttyd enabled");
  require_eq(enabled.layout_mode, std::string("split"), "split enabled");
  require_eq(enabled.latexmk_opts[0], std::string("-xelatex"),
             "latexmk override");
  require_eq(enabled.build_dir_name, std::string("build"),
             "build dir name explicit");
}

void test_workspace_documents() {
  const auto temp = make_temp_dir("workspace");
  write_file(temp / "tex" / "main.tex", "main");
  write_file(temp / "tex" / "chapter.tex", "chapter");
  write_file(temp / "other" / "z.tex", "z");
  write_file(temp / "build" / "ignored.tex", "ignored");
  write_file(temp / "notes.txt", "ignored");

  lf::app_config config = minimal_config(temp);
  const auto files = lf::list_tex_documents(config);
  require_eq(files.size(), std::size_t{3}, "list tex files excludes build");
  require_eq(files[0], std::string("other/z.tex"), "list tex sorted 0");
  require_eq(files[1], std::string("tex/chapter.tex"), "list tex sorted 1");
  require_eq(files[2], std::string("tex/main.tex"), "list tex sorted 2");

  require_eq(lf::resolve_tex_document(config, "tex/main.tex"),
             std::string("tex/main.tex"), "resolve valid tex");
  require_eq(lf::resolve_tex_document(config, "../outside.tex"),
             std::string(""), "reject outside tex");
  require_eq(lf::resolve_tex_document(config, "notes.txt"), std::string(""),
             "reject non-tex");

  require_eq(lf::requested_pdf_path(config, "tex/main.tex", "other/z.tex")
                 .string(),
             (temp / "other" / "build" / "z.pdf").string(),
             "requested pdf path override");
  std::filesystem::remove_all(temp);
}

void test_pdf_response_headers() {
  require_eq(lf::pdf_content_disposition_for("/tmp/main.pdf"),
             std::string("inline; filename=\"main.pdf\"; "
                         "filename*=UTF-8''main.pdf"),
             "ascii pdf content disposition");
  const auto japanese =
      lf::pdf_content_disposition_for("/tmp/\346\227\245\346\234\254.pdf");
  require(japanese.find("filename=\"______.pdf\"") != std::string::npos,
          "utf8 pdf content disposition fallback");
  require(japanese.find("filename*=UTF-8''%E6%97%A5%E6%9C%AC.pdf") !=
              std::string::npos,
          "utf8 pdf content disposition encoded");
}

void test_build_log_buffer() {
  lf::build_log_buffer buffer(3);
  buffer.append("a\nb\n");
  buffer.append("c\nd\n");
  require_eq(buffer.tail_joined(10), std::string("b\nc\nd"),
             "build log keeps capacity");
  require_eq(buffer.tail_joined(2), std::string("c\nd"), "build log tail");
}

void test_build_events() {
  require_eq(lf::build_start_payload("file\"change", 2, "tex/main.tex"),
             std::string("{\"reason\":\"file\\\"change\",\"queued\":2,"
                         "\"tex_main\":\"tex/main.tex\"}"),
             "build_start_payload");
  require_eq(lf::pdf_updated_payload(123, "tex/main.tex"),
             std::string("{\"pdf_mtime\":123,\"tex_main\":\"tex/main.tex\"}"),
             "pdf_updated_payload");
  require_eq(lf::build_ok_payload(123, 45, "tex/main.tex"),
             std::string("{\"pdf_mtime\":123,\"duration_ms\":45,"
                         "\"tex_main\":\"tex/main.tex\"}"),
             "build_ok_payload");
  require_eq(lf::build_fail_payload("bad\nbuild", "tail", 45, "tex/main.tex"),
             std::string("{\"message\":\"bad\\nbuild\",\"tail_log\":\"tail\","
                         "\"duration_ms\":45,\"tex_main\":\"tex/main.tex\"}"),
             "build_fail_payload");
}

void test_event_bus() {
  lf::event_bus bus;
  bus.publish("build_start", "{}");
  const auto events = bus.wait(0, std::chrono::milliseconds(1));
  require_eq(events.size(), std::size_t{1}, "event_bus wait count");
  require_eq(events[0].name, std::string("build_start"), "event_bus name");
  require_eq(bus.last_sequence(), std::uint64_t{1}, "event_bus sequence");
  bus.shutdown();
  require(bus.stopped(), "event_bus stopped");
}

}  // namespace

int main() {
  const std::vector<std::pair<std::string, void (*)()>> tests = {
      {"common_utils", test_common_utils},
      {"globs_and_paths", test_globs_and_paths},
      {"config_env", test_config_env},
      {"workspace_documents", test_workspace_documents},
      {"pdf_response_headers", test_pdf_response_headers},
      {"build_log_buffer", test_build_log_buffer},
      {"build_events", test_build_events},
      {"event_bus", test_event_bus},
  };

  int failures = 0;
  for (const auto& test : tests) {
    try {
      test.second();
      std::cout << "[PASS] " << test.first << '\n';
    } catch (const std::exception& error) {
      failures += 1;
      std::cerr << "[FAIL] " << test.first << ": " << error.what() << '\n';
    }
  }
  return failures == 0 ? 0 : 1;
}
