// libstdc++, starting at gcc 7:
// #include <optional>
// using std::optional;
//
// TODO:
//
// 1. Need some caching!  Read input from git, compile each piece, and then
//    store the blobs back in a parallel object store similar to git's.
//
// 2. Move individual resolvers to a separate file.  Either one file per rule,
//    or even a tiny set of functions in eden that expresses the resolvers.
//
// 3. Log output into a common place (in /var/log?).
//
// 4. Keep an installation transcript that can be used for reverting the
//    installation.
//
// 5. Use a chroot or some other kind of isolation for the "run" resolver (not
//    implemented yet).
//
// 6. We need a way to avoid repeating, for example, cflags of gmock.  Perhaps
//    the flags should be part of depending on a c++lib.  Or maybe the vendor
//    (./v/...) libraries should only expose a single .h?  Or maybe they
//    should be "installed" in ./.include ?

// TODO: Currently only the one src can be present.  Fix this.
error compileCpp(const vector<string>& srcs,
                 const string& oFile,
                 const map<string, eden::Node>& attrs) {
  const string compiler_program = attrs.at(":compiler").AsString();
  vector<string> flags;
  if (auto it = attrs.find(":inc"); it != attrs.end()) {
    for (auto x : it->second.AsNodes()) {
      flags.push_back("-include");
      flags.push_back(x->AsString());
    }
  }
  if (auto it = attrs.find(":cflags-default"); it != attrs.end()) {
    for (auto x : it->second.AsNodes()) {
      flags.push_back(x->AsString());
    }
  }
  if (auto it = attrs.find(":cflags"); it != attrs.end()) {
    for (auto x : it->second.AsNodes()) {
      flags.push_back(x->AsString());
    }
  }
  string srcs_str;
  string sep = "";
  flags.push_back("-c");
  for (const string& src : srcs) {
    flags.push_back(src);
    srcs_str += sep + src;
    sep = ", ";
  }
  flags.push_back("-o");
  flags.push_back(oFile);

  // TODO: this condition should come from the command line, not from the AA
  // file.
  if (attrs.count(":mockingly")) {
    std::cout << "  compiling (mockingly) " + srcs_str
              << " => " << oFile << "\n";
    flags.insert(flags.begin(), compiler_program);
    for (const auto& flag : flags) {
      std::cout << " " << flag;
    }
    std::cout << "\n";
  }
  std::cout << "  compiling " + srcs_str << " => " << oFile << "\n";
  return os::ForkExecWait(compiler_program, flags);
}

error linkCppBinary(const vector<string>& oFiles, const string& binFile,
                    const map<string, eden::Node>& attrs) {
  const string linker_program = attrs.at(":linker").AsString();
  vector<string> flags(oFiles.begin(), oFiles.end());
  flags.push_back("-o");
  flags.push_back(binFile);
  if (auto it = attrs.find(":lib"); it != attrs.end()) {
    for (auto x : it->second.AsNodes()) {
      flags.push_back("-l" + x->AsString());
    }
  }
  if (auto it = attrs.find(":lflags-default"); it != attrs.end()) {
    for (auto x : it->second.AsNodes()) {
      flags.push_back(x->AsString());
    }
  }
  if (auto it = attrs.find(":lflags"); it != attrs.end()) {
    for (auto x : it->second.AsNodes()) {
      flags.push_back(x->AsString());
    }
  }
  std::cout << "  linking => " + binFile << "\n";
  os::ForkExecWait(linker_program, flags);
  return "";
}

class Resolver { // interface
 public:
  virtual error Resolve(const string& target) = 0;
  virtual const vector<string>& Deps() = 0;
};

class CppbinResolver : public Resolver {
 public:
  CppbinResolver(const vector<string>& deps,
                 const map<string, eden::Node>& attrs)
      : deps_(deps), attrs_(attrs) {}
  ~CppbinResolver() {}
  const vector<string>& Deps() override { return deps_; }

  error Resolve(const string& target) override {
    const string outDir = attrs_.at(":out-dir").AsString();
    const string binDir = attrs_.at(":bin-dir").AsString();
    auto it_srcs = attrs_.find(":src");
    if (it_srcs == attrs_.end()) {
      return ":src key not found for target " + target;
    }
    std::vector<std::string> srcs;
    for (const eden::Node* node : it_srcs->second.AsNodes()) {
      if (!node->IsString()) {
        return "src has the wrone type " + node->Typename();
      }
      srcs.push_back(node->AsString());
    }
    const string oFile = outDir + target + ".o";
    vector<string> oFiles = {oFile};
    for (const string& dep : deps_) {
      oFiles.push_back(outDir + dep + ".o");
    }
    const string binFile = binDir + target;
    error err = compileCpp(srcs, oFile, attrs_);
    if (err != "") {
      return "[compiling]" + err;
    }
    err = linkCppBinary(oFiles, binFile, attrs_);
    if (err != "") {
      return "[linking]" + err;
    }
    return "";
  }
 private:
  const vector<string> deps_;
  const map<string, eden::Node> attrs_;
};

class CpplibResolver : public Resolver {
 public:
  CpplibResolver(const vector<string>& deps,
                 const map<string, eden::Node>& attrs)
      : deps_(deps), attrs_(attrs) {}
  ~CpplibResolver() {}
  const vector<string>& Deps() override { return deps_; }

  error Resolve(const string& target) override {
    const string outDir = attrs_.at(":out-dir").AsString();

    auto it_srcs = attrs_.find(":src");
    if (it_srcs == attrs_.end()) {
      return ":src key not found for target " + target;
    }
    std::vector<std::string> srcs;
    for (const eden::Node* node : it_srcs->second.AsNodes()) {
      if (!node->IsString()) {
        return "src has the wrone type " + node->Typename();
      }
      srcs.push_back(node->AsString());
    }
    const string oFile = outDir + target + ".o";
    error err = compileCpp(srcs, oFile, attrs_);
    if (err != "") {
      return "[compiling] " + err;
    }
    return "";
  }
 private:
  const vector<string> deps_;
  const map<string, eden::Node> attrs_;
};

class InstallResolver : public Resolver {
 public:
  InstallResolver(const vector<string>& deps,
                 const map<string, eden::Node>& attrs)
      : deps_(deps), attrs_(attrs) {}
  ~InstallResolver() {}

  const vector<string>& Deps() override { return deps_; }

  error Resolve(const string& target) override {
    const string binDir = attrs_.at(":bin-dir").AsString();

    for (const string& dep : deps_) {
      // cp .bin/DEP ~/.local/bin/DEP
      const string program_path = os::HomeDir() + "/.local/bin/" + dep;
      os::ForkExecWait("/bin/cp", {binDir + dep, program_path});
      std::cout << "  install => " << program_path << "\n";
    }
    return "";
  }
 private:
  const vector<string> deps_;
  const map<string, eden::Node> attrs_;
};

class NoopResolver : public Resolver {
 public:
  NoopResolver() {}
  ~NoopResolver() {}
  const vector<string> deps_;  // empty

  const vector<string>& Deps() override { return deps_; }

  error Resolve(const string& target) override {
    std::cout << "  noop => " << target << "\n";
    return "";
  }
};

class Manager {
 public:
  Manager(const eden::Node& global_attrs_root) {
    global_attrs_.clear();
    auto it = global_attrs_root.AsNodes().cbegin();
    auto itEnd = global_attrs_root.AsNodes().cend();
    if (it != itEnd && (*it)->IsMap()) {
      std::cerr << processAttributes(**it, &global_attrs_);
    }
  }
  ~Manager() {}
  error Read();
  error Resolve(const vector<string>& targets);
  const string ListTargets();

 private:
  error processAttributes(const eden::Node& attrs_root,
                          map<string, eden::Node>* attrs);
  error processRule(const string& targetname, const eden::Node& rule);

  map<string, eden::Node> global_attrs_;
  map<string, eden::Node> module_attrs_;
  map<string, unique_ptr<Resolver>> rules_;
};

error Manager::Read() {
  const string aaFile = global_attrs_[":aa"].AsString();
  const string specStr = strings::ReadFileToString(aaFile);
  std::unique_ptr<eden::Node> spec_root = eden::read(specStr);

  // populate list of rules and parameters from the node tree.
  // Node tree should look like the following:
  // (module MODULE-NAME {:ATTR-KEY ATTR-VALUE ...}
  //   (RULENAME TARGET [DEP1 DEP2 ...] {:PARAMETER VALUE}))
  auto it = spec_root->AsNodes().cbegin();
  auto itEnd = spec_root->AsNodes().cend();
  if (it == itEnd) {
    return "";  // Empty spec.
  }
  if ((*it)->IsMap()) {
    module_attrs_ = global_attrs_; // Copy.
    error err = processAttributes(**it, &module_attrs_);
    if (err != "") {
      return err;
    }
    ++it;
  }
  for (; it != itEnd; ++it) {
    if (!(*it)->IsSymbol()) {
      return "Target name is required here.";
    }
    const string& target = (*it)->AsString();
    ++it;
    error err = processRule(target, **it);
    if (err != "") {
      return err;
    }
  }
  return "";
}

error Manager::processAttributes(const eden::Node& attrs_root,
                                 map<string, eden::Node>* attrs) {
  auto it = attrs_root.AsNodes().cbegin();
  auto itEnd = attrs_root.AsNodes().cend();
  while (it != itEnd) {
    if (!(*it)->IsKeyword()) {
      return "Map key is expected to be a keyword here";
    }
    const string& key = ":" + (*it)->AsString();
    if (++it == itEnd) {
      return "Map key found without a value "
             "(i.e., odd number of Atoms between {})";
    }
    const eden::Node& value = **it;
    ++it;
    attrs->emplace(key, value);
  }
  return "";
}

Resolver* CreateResolverByName(const string& resolver_name,
                               const vector<string>& deps,
                               const map<string, eden::Node>& attrs) {
  if (resolver_name == "c++bin") {
    return new CppbinResolver(deps, attrs);
  }
  if (resolver_name == "c++lib") {
    return new CpplibResolver(deps, attrs);
  }
  if (resolver_name == "install") {
    return new InstallResolver(deps, attrs);
  }
  if (resolver_name == "noop") {
    return new NoopResolver();
  }
  return nullptr;
}

error Manager::processRule(const string& target, const eden::Node& rule) {
  if (!rule.IsList()) {
    return "rule should be of type list";
  }
  auto it = rule.AsNodes().begin();
  auto itEnd = rule.AsNodes().end();
  const string& resolver_name = (*it)->AsString();
  // We need a vector of dependencies here.
  if (++it == itEnd || !(*it)->IsVector()) {
    return "Missing array of dependencies";
  }
  vector<string> deps;
  deps.reserve((*it)->AsNodes().size());
  for (const eden::Node* node : (*it)->AsNodes()) {
    deps.push_back(node->AsString());
  }

  // Often, there is a map of rule attributes here.
  map<string, eden::Node> attrs(module_attrs_.begin(), module_attrs_.end());
  if (++it != itEnd && (*it)->IsMap()) {
    auto m = (*it)->AsNodes().begin();
    auto mEnd = (*it)->AsNodes().end();
    for (;;) {
      if (m == mEnd) {
        break;
      }
      if (!(*m)->IsKeyword()) {
        return "Map key is expected to be a keyword here";
      }
      const string& key = ":" + (*m)->AsString();
      if (++m == mEnd) {
        return "Map key found without a value "
               "(i.e., odd number of Atoms between {})";
      }
      const eden::Node& value = **m;
      ++m;
      attrs[key] = value;
    }
  }
  // Dispatch on resolver_name.
  // TODO: The `if' branches should be replaced with a map or something.
  Resolver* resolver = CreateResolverByName(resolver_name, deps, attrs);
  if (resolver == nullptr) {
    return "Unknown rule resolver " + resolver_name;
  }
  rules_[target].reset(resolver);
  return "";
}

// Given the digraph F in which x->y means y should be resolved before x,
// create the reverse digraph B going from y to x.  Given B find out the
// leaves (nodes with no outgoing edges), and put them in the layer (phase) 1.
// Then remove the leaves from the dependencies of the cover of leaves, and
// repeat the process.
pair<error, vector<set<string>>> sortIntoPhases(const set<string>& targets,
                                                map<string, set<string>> F) {
  vector<set<string>> phases;
  set<string> seen;
  set<string> leaves;
  map<string, set<string>> rdepends;
  std::deque<string> q_init(targets.cbegin(), targets.cend());
  for (std::queue<string> q(q_init); !q.empty();) {
    const string target = q.front();
    q.pop();
    if (!seen.insert(target).second) {
      continue;
    }
    const auto it_deps = F.find(target);
    if (it_deps == F.end()) {
      return make_pair("Couldn't find node " + target + " in rules.", phases);
    }
    const set<string>& deps = it_deps->second;
    if (deps.empty()) {
      leaves.insert(target);
      continue;
    }
    for (const string& dep : deps) {
      if (F.find(dep) == F.end()) {
        return make_pair("Couldn't find node " + dep + " in rules.", phases);
      }
      q.push(dep);
      rdepends[dep].insert(target);
    }
  }

  while (!leaves.empty()) {
    phases.push_back(leaves);
    set<string> leaves_cover;
    for (const string& leaf : leaves) {
      const set<string>& leaf_cover = rdepends[leaf];
      leaves_cover.insert(leaf_cover.cbegin(), leaf_cover.cend());
    }
    set<string> next_leaves;
    for (const string& x : leaves_cover) {
      auto it_deps = F.find(x);
      if (it_deps == F.end()) {
        return make_pair("Couldn't find node " + x + " in F.", phases);
      }
      set<string>* deps = &(it_deps->second);
      for (const string& leaf : leaves) {
        deps->erase(leaf);
      }
      if (deps->empty()) {
        next_leaves.insert(x);
      }
    }
    leaves = next_leaves;
  }

  return make_pair("", phases);
}

error Manager::Resolve(const vector<string>& targets) {
  error err = "";
  set<string> target_set(targets.begin(), targets.end());
  map<string, set<string>> dependencies;
  for (const auto& kv : rules_) {
    const string& target = kv.first;
    const vector<string>& deps = kv.second->Deps();
    dependencies[target].insert(deps.begin(), deps.end());
  }
  vector<set<string>> phases;
  {
    auto err_and_phases = sortIntoPhases(target_set, dependencies);
    err = err_and_phases.first;
    if (err != "") {
      return err;
    }
    phases = err_and_phases.second;
  }

  for (size_t i = 0; i < phases.size(); ++i) {
    const set<string> phase_targets = phases[i];
    std::cout << "Phase " << i << ":\n";
    for (const string& target : phase_targets) {
      auto it = rules_.find(target);
      if (it == rules_.end()) {
        continue;
      }
      Resolver* resolver = it->second.get();
      error err1 = resolver->Resolve(target);
      if (err1 != "") {
        err += "[target=" + target + "] " + err1 + "\n";
      }
    }
  }
  return err;
}

const string Manager::ListTargets() {
  string s;
  for (const auto& kv : rules_) {
    const string& target = kv.first;
    s += "  " + target + "\n";
  }
  return s;
}

// Given a source file, say "hello/greet.cc" return the preferred target name,
// i.e., "greet.hello" for that case.
const string targetNameFromSource(const string& source) {
  const string sourceSansExt = path::SansExt(source);
  vector<string> chunks = strings::Split(sourceSansExt, '/');
  std::reverse(chunks.begin(), chunks.end());
  return strings::Join(chunks, ".");
}

int main(int argc, char* argv[], char** envp) {
  os::Runtime runtime(argc, argv, envp);
  const vector<string>& targets = runtime.args();

  std::unique_ptr<eden::Node> global_attrs_root = eden::read(
      strings::ReadFileToString(os::HomeDir() + "/.config/aa/defaults"));

  std::unique_ptr<Manager> m(new Manager(*global_attrs_root));
  error err = m->Read();
  if (err != "") {
    std::cerr << err << "\n";
  }
  if (targets.empty()) {
    std::cout << m->ListTargets();
    return 0;
  }
  err = m->Resolve(targets);
  if (err != "") {
    std::cerr << err << "\n";
    return 1;
  }
  return 0;
}
