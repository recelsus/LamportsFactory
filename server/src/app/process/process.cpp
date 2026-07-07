#include "app/process/process.hpp"

#include <fcntl.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <thread>

namespace lf {

process_result run_process(const std::vector<std::string>& args,
                           const std::string& workdir,
                           int timeout_sec) {
  if (args.empty()) {
    return {-1, false, ""};
  }
  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (const auto& arg : args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  int pipe_fd[2];
  if (pipe(pipe_fd) == -1) {
    return {-1, false, ""};
  }

  pid_t pid = fork();
  if (pid == -1) {
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    return {-1, false, ""};
  }

  if (pid == 0) {
    if (!workdir.empty()) {
      (void)chdir(workdir.c_str());
    }
    dup2(pipe_fd[1], STDOUT_FILENO);
    dup2(pipe_fd[1], STDERR_FILENO);
    close(pipe_fd[0]);
    close(pipe_fd[1]);
    execvp(argv[0], argv.data());
    _exit(127);
  }

  close(pipe_fd[1]);
  const int flags = fcntl(pipe_fd[0], F_GETFL, 0);
  fcntl(pipe_fd[0], F_SETFL, flags | O_NONBLOCK);

  std::string output;
  bool timed_out = false;
  auto start_time = std::chrono::steady_clock::now();
  bool process_done = false;

  while (true) {
    if (!process_done) {
      int status = 0;
      pid_t result = waitpid(pid, &status, WNOHANG);
      if (result == pid) {
        process_done = true;
        if (WIFSIGNALED(status)) {
          timed_out = timed_out || WTERMSIG(status) == SIGKILL;
        }
      }
    }

    char buffer[4096];
    ssize_t bytes = read(pipe_fd[0], buffer, sizeof(buffer));
    if (bytes > 0) {
      output.append(buffer, static_cast<std::size_t>(bytes));
    }

    if (process_done) {
      if (bytes <= 0) {
        break;
      }
    } else {
      const auto elapsed =
          std::chrono::duration_cast<std::chrono::seconds>(
              std::chrono::steady_clock::now() - start_time);
      if (timeout_sec > 0 && elapsed.count() >= timeout_sec) {
        timed_out = true;
        kill(pid, SIGKILL);
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  }

  close(pipe_fd[0]);
  int status = 0;
  waitpid(pid, &status, 0);
  int exit_code = 0;
  if (WIFEXITED(status)) {
    exit_code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    exit_code = 128 + WTERMSIG(status);
  }

  return {exit_code, timed_out, output};
}

}  // namespace lf
