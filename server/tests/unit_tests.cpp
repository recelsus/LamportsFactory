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
#include "app/compiler/compiler_backend_factory.hpp"
#include "app/compiler/latex_backend.hpp"
#include "app/compiler/typst_backend.hpp"
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

lf::app_config minimal_config(const std::filesystem::path& workspace_dir) {
  lf::app_config config{};
  config.workspace_dir = workspace_dir.string();
  config.out_dir = (workspace_dir / "build").string();
  config.build_dir_name = "build";
  config.main_document = "tex/main.tex";
  config.document_extension = ".tex";
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

  require_eq(lf::output_pdf_path_for(temp.string(), "build",
                                     "tex/main.tex", "")
                 .string(),
             (temp / "tex" / "build" / "main.pdf").string(),
             "output_pdf_path_for sibling build dir");
  require_eq(lf::build_dir_for_document(temp.string(), "build",
                                        "tex/main.tex")
                 .string(),
             (temp / "tex" / "build").string(),
             "build_dir_for_document sibling build dir");
  require_eq(lf::build_dir_for_document(temp.string(), "build",
                                        "main.tex")
                 .string(),
             (temp / "build").string(), "build_dir_for_document root");
  require(lf::is_build_output_path(temp.string(), "build",
                                   temp / "tex" / "build" / "main.aux"),
          "is_build_output_path nested");
  std::filesystem::remove_all(temp);
}

void test_config_env() {
  env_guard guard({"BASE_URL", "TTYD_ENABLED", "LAYOUT_MODE", "STATIC_DIR",
                   "WORKSPACE_DIR", "OUT_DIR", "BUILD_DIR_NAME",
                   "MAIN_DOCUMENT", "DOCUMENT_EXTENSION",
                   "COMPILER_BACKEND"});
  setenv("BASE_URL", "preview/", 1);
  setenv("TTYD_ENABLED", "disable", 1);
  setenv("LAYOUT_MODE", "split", 1);
  setenv("STATIC_DIR", "/tmp", 1);
  setenv("WORKSPACE_DIR", "/tmp/workspace", 1);
  setenv("MAIN_DOCUMENT", "sample/main.tex", 1);
  setenv("OUT_DIR", "/app/workspace/legacy-build", 1);
  unsetenv("BUILD_DIR_NAME");
  setenv("DOCUMENT_EXTENSION", "tex", 1);
  setenv("COMPILER_BACKEND", "latex", 1);

  const lf::app_config config = lf::load_app_config();
  require_eq(config.base_url, std::string("/preview"), "base url normalize");
  require_eq(config.workspace_dir, std::string("/tmp/workspace"),
             "workspace dir");
  require_eq(config.main_document, std::string("sample/main.tex"),
             "main document");
  require_eq(config.layout_mode, std::string("preview"),
             "split falls back when ttyd disabled");
  require(!config.ttyd_enabled, "ttyd disabled");
  require_eq(config.ttyd_url, std::string("/preview/terminal/"),
             "default ttyd url joins base url");
  require_eq(config.build_dir_name, std::string("legacy-build"),
             "build dir name falls back to OUT_DIR filename");
  require_eq(config.document_extension, std::string(".tex"),
             "document extension normalizes dot");
  require_eq(config.compiler_backend, std::string("latex"),
             "compiler backend");

  setenv("TTYD_ENABLED", "enable", 1);
  setenv("LAYOUT_MODE", "split", 1);
  setenv("BUILD_DIR_NAME", "build", 1);
  const lf::app_config enabled = lf::load_app_config();
  require(enabled.ttyd_enabled, "ttyd enabled");
  require_eq(enabled.layout_mode, std::string("split"), "split enabled");
  require_eq(enabled.build_dir_name, std::string("build"),
             "build dir name explicit");
}

void test_latex_backend_config() {
  env_guard guard({"LATEX_BUILD_TOOL", "LATEX_ENGINE", "LATEXMK_OPTS",
                   "TECTONIC_OPTS"});
  setenv("LATEX_BUILD_TOOL", "latexmk", 1);
  setenv("LATEX_ENGINE", "platex", 1);
  unsetenv("LATEXMK_OPTS");
  unsetenv("TECTONIC_OPTS");

  const lf::latex_backend_config config = lf::load_latex_backend_config();
  require_eq(config.build_tool, std::string("latexmk"),
             "latex build tool");
  require_eq(config.latexmk_opts[0], std::string("-pdfdvi"),
             "platex default latexmk option");

  setenv("LATEXMK_OPTS", "-xelatex -halt-on-error", 1);
  const lf::latex_backend_config overridden = lf::load_latex_backend_config();
  require_eq(overridden.latexmk_opts[0], std::string("-xelatex"),
             "latexmk override");
}

void test_typst_backend_config() {
  env_guard guard({"TYPST_OPTS"});
  setenv("TYPST_OPTS", "--root . --font-path /app/fonts", 1);

  const lf::typst_backend_config config = lf::load_typst_backend_config();
  require_eq(config.opts.size(), std::size_t{4}, "typst opts count");
  require_eq(config.opts[0], std::string("--root"), "typst opts first");
  require_eq(config.opts[3], std::string("/app/fonts"), "typst opts value");
}

void test_compiler_backend_factory() {
  env_guard guard({"LATEX_BUILD_TOOL", "LATEX_ENGINE", "LATEXMK_OPTS",
                   "TECTONIC_OPTS", "TYPST_OPTS"});
  auto latex = lf::create_compiler_backend("latex");
  require(static_cast<bool>(latex), "latex backend factory");
  require_eq(latex->name(), std::string("latex"), "latex backend name");

  auto typst = lf::create_compiler_backend("typst");
  require(static_cast<bool>(typst), "typst backend factory");
  require_eq(typst->name(), std::string("typst"), "typst backend name");

  auto unsupported = lf::create_compiler_backend("unknown");
  require(!unsupported, "unsupported backend factory");
}

void test_typst_backend_command() {
  env_guard guard({"TYPST_OPTS"});
  unsetenv("TYPST_OPTS");
  const auto temp = make_temp_dir("typst");
  lf::app_config config = minimal_config(temp);
  config.main_document = "doc/main.typ";
  config.document_extension = ".typ";

  const lf::typst_backend backend(lf::load_typst_backend_config());
  const lf::compiler_command command =
      backend.build_command(config, config.main_document);

  require_eq(command.args.size(), std::size_t{4}, "typst command arg count");
  require_eq(command.args[0], std::string("typst"), "typst command binary");
  require_eq(command.args[1], std::string("compile"), "typst command mode");
  require_eq(command.args[2], std::string("doc/main.typ"),
             "typst command input");
  require_eq(command.args[3],
             (temp / "doc" / "build" / "main.pdf").string(),
             "typst command output");
  require_eq(command.working_dir.string(), temp.string(),
             "typst command working dir");

  std::filesystem::remove_all(temp);
}

void test_workspace_documents() {
  const auto temp = make_temp_dir("workspace");
  write_file(temp / "tex" / "main.tex", "main");
  write_file(temp / "tex" / "chapter.tex", "chapter");
  write_file(temp / "other" / "z.tex", "z");
  write_file(temp / "build" / "ignored.tex", "ignored");
  write_file(temp / "notes.txt", "ignored");

  lf::app_config config = minimal_config(temp);
  const auto files = lf::list_documents(config);
  require_eq(files.size(), std::size_t{3}, "list documents excludes build");
  require_eq(files[0], std::string("other/z.tex"), "list documents sorted 0");
  require_eq(files[1], std::string("tex/chapter.tex"),
             "list documents sorted 1");
  require_eq(files[2], std::string("tex/main.tex"),
             "list documents sorted 2");

  require_eq(lf::resolve_document(config, "tex/main.tex"),
             std::string("tex/main.tex"), "resolve valid document");
  require_eq(lf::resolve_document(config, "../outside.tex"),
             std::string(""), "reject outside document");
  require_eq(lf::resolve_document(config, "notes.txt"), std::string(""),
             "reject non-document extension");

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
                         "\"main_document\":\"tex/main.tex\"}"),
             "build_start_payload");
  require_eq(lf::pdf_updated_payload(123, "tex/main.tex"),
             std::string("{\"pdf_mtime\":123,\"main_document\":\"tex/main.tex\"}"),
             "pdf_updated_payload");
  require_eq(lf::build_ok_payload(123, 45, "tex/main.tex"),
             std::string("{\"pdf_mtime\":123,\"duration_ms\":45,"
                         "\"main_document\":\"tex/main.tex\"}"),
             "build_ok_payload");
  require_eq(lf::build_fail_payload("bad\nbuild", "tail", 45, "tex/main.tex"),
             std::string("{\"message\":\"bad\\nbuild\",\"tail_log\":\"tail\","
                         "\"duration_ms\":45,\"main_document\":\"tex/main.tex\"}"),
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
      {"latex_backend_config", test_latex_backend_config},
      {"typst_backend_config", test_typst_backend_config},
      {"compiler_backend_factory", test_compiler_backend_factory},
      {"typst_backend_command", test_typst_backend_command},
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
