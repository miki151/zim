#pragma once

#include <iostream>
#include <vector>
#include "unistd.h"
#include "string.h"
#include "sys/types.h"
#include "sys/wait.h"
#include <fcntl.h>
#include <cassert>
#include <optional>
#include <algorithm>

class LspClientBase {
  public:

  const std::string contentLength = "Content-Length: ";

  void sendMessage(const std::string& message) {
    auto header = contentLength + std::to_string(message.size()) + "\r\n\r\n";
    write(pipeP2C[1], header.data(), header.size());
    write(pipeP2C[1], message.data(), message.size());
  }

  std::optional<std::string> readMessage() {
    std::string header;
    std::string content;
    while (true) {
      char buf[100] = {0};
      int cnt = read(pipeC2P[0], buf, contentLength.size() + 10);
      //if (cnt == -1) 
      //  retur 
        //std::cout << "Error " << strerror(errno) << std::endl;
      if (cnt <= 0)
        return std::nullopt;
      bool done = false;
      for (int i = 0; i < cnt - 3; ++i)
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
          std::string tmp(buf);
          header += tmp.substr(0, i);
          content += tmp.substr(i + 4);
          done = true;
          break;
        }
      if (!done)
        header += std::string(buf);
      else
        break;
    }
    assert(header.substr(0, contentLength.size()) == contentLength);
    int contentSize = stoi(header.substr(contentLength.size()));
    assert(contentSize > 0);
    while (content.size() < contentSize) {
      char buf[100000] = {0};
      int cnt = read(pipeC2P[0], buf, std::min(contentSize - content.size(), sizeof(buf)));
      assert(cnt > 0);
      content += std::string(buf);
    }
    return content;
  }

  LspClientBase(std::string serverCommand, std::vector<std::string> args, bool serverStderr) {
    assert(pipe(pipeP2C) == 0);
    assert(pipe2(pipeC2P, O_NONBLOCK) == 0);
    int pid = fork();
    assert(pid >= 0);
    if (pid > 0) {
      close(pipeP2C[0]);
      close(pipeC2P[1]);
    } else {
      close(pipeP2C[1]);
      close(pipeC2P[0]);
      dup2(pipeP2C[0], STDIN_FILENO);
      dup2(pipeC2P[1], STDOUT_FILENO);
      close(pipeP2C[0]);
      close(pipeC2P[1]);
      if (!serverStderr) {
        int fd = open("/dev/null", O_WRONLY);
        dup2(fd, 2);
      }
      vector<char*> argv {
        serverCommand.data()
      };
      for (auto& arg : args)
        argv.push_back(arg.data());
      argv.push_back(nullptr);
      char * envp[] = {nullptr}; 
      execvpe(serverCommand.data(), argv.data(), envp);
      fprintf(stderr, "Error running %s: %s\n", serverCommand.data(), strerror(errno));
      assert(false);
    }
  }

  /*~LspClientBase() {
    std::vector<std::string> text = {
      "{\"jsonrpc\":\"2.0\",\"method\":\"shutdown\",\"id\":2}",
      "{\"jsonrpc\":\"2.0\",\"method\":\"exit\"}"
    };
    sendMessage(text[0]);
    while (!readMessage()) {}
    sendMessage(text[1]);
  }*/

  private:

  int pipeP2C[2], pipeC2P[2];
};
