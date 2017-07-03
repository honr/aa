#ifndef _BASIC_H_
#define _BASIC_H_

#include <pwd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <algorithm>
#include <deque>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <streambuf>
#include <string>
#include <utility>
#include <vector>

using std::list;
using std::map;
using std::pair;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

typedef string error;

namespace path {
// Ext("foo/bar.baz") -> ".baz"
// Ext("foo.bar/baz.quax") -> ".quax"
// Ext("foo.bar/baz.") -> "."
// Ext("foo/bar") -> ""
const string Ext(const string& path) {
  size_t i = path.rfind('.');
  return i == string::npos || path.find('/', i) != string::npos
      ? ""
      : path.substr(i);
}

// SansExt("foo/bar.baz") -> "foo/bar"
// SansExt("foo/bar.") -> "foo/bar"
// SansExt("foo/bar") -> "foo/bar"
const string SansExt(const string& path) {
  size_t i = path.rfind(".");
  return i == string::npos ? path : path.substr(0, i);
}

error MakeContainingDir(const string& path) {
  return "";
}

} // ::path

namespace strings {
vector<string> Split(const string& s, const char sep) {
  vector<string> chunks;
  for (size_t a = 0, b = 0;;) {
    b = s.find(sep, a);
    chunks.push_back(s.substr(a, b - a));
    if (b == string::npos) {
      break;
    }
    a = b + 1 /* len sep = 1 */;
  }
  return chunks;
}

string Join(const vector<string>& v, const string& sep) {
  string r;
  for (const string& s : v) {
    r += s + sep;
  }
  return r.substr(0, r.size() - sep.size());
}

string ReadFileToString(const string& filepath) {
  std::ifstream stream(filepath);
  std::string contents((std::istreambuf_iterator<char>(stream)),
                       std::istreambuf_iterator<char>());
  return contents;
}

} // ::strings

namespace os {
// Arguments are passed by value because we need the clones.
error ForkExecWait(const string program, const vector<string> args) {
  int childpid = fork();
  if (childpid == 0) { // at the child
    size_t n = args.size();
    vector<char*> argv(n + 2);
    argv[0] = const_cast<char*>(program.c_str());
    for (size_t i = 0; i <= n; i++) {
      argv[i + 1] = const_cast<char*>(args[i].c_str());
    }
    argv[n+1] = nullptr;
    execv(argv[0], &argv[0]);
    return ""; // Dummy; execv() should never return.
  }
  int status = 0;
  waitpid(childpid, &status, 0);
  if (status == 0) {
    return "";
  }
  return "program " + program + " returned with status " +
      std::to_string(status);
}

class Runtime {
 public:
  Runtime(int argc, char* argv[], char** envp) {
    if (argc <= 0) {
      return; // Probably shouldn't happen.
    }
    size_t num_args = static_cast<size_t>(argc - 1);
    program_ = argv[0];
    args_.reserve(num_args);
    for (size_t i = 0; i < num_args; ++i) {
      args_.emplace_back(argv[i + 1]);
    }
    // TODO: initialize env.
  }
  ~Runtime() {}
  string program() { return program_; }
  vector<string> args() { return args_; }

 private:
  string program_;
  vector<string> args_;
};

const string HomeDir() {
  return string(getpwuid(getuid())->pw_dir);
}

} // ::os

#endif // _BASIC_H_
