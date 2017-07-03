
void println_one(const std::deque<string>& q) {
  std::cout << "deque<string> [";
  string sep = "";
  for (const string& x : q) {
    std::cout << sep << "'" << x << "'";
    sep = ", ";
  }
  std::cout << "],\n";
}

void println_one(const map<string, set<string>>& m) {
  std::cout << "map<string, set<string>> {\n";
  for (const auto& kv : m) {
    const string& key = kv.first;
    const set<string>& values = kv.second;
    std::cout << "  '" << key << "' => [";
    string sep = "";
    for (const string& value : values) {
      std::cout << sep << "'" << value << "'";
      sep = ", ";
    }
    std::cout << "],\n";
  }
  std::cout << "}\n";
}
