namespace hello {

int f() {
  static int i = 0;
  return ++i;
}

} // ::hello

int main() {
  std::cout << "without digit separators: " << 123456 << '\n';
  std::cout << "with digit separators: " << 123'456'789 << '\n';
  std::cout << "binary literal 5: " << 0b101 << '\n';

  std::string s = "foo-bar";
  std::cout << "s.substr(1): '" << s.substr(1) << "'.\n";

  std::map<std::string, int> m{{"foo", 10}};
  std::vector<std::string> keys{"foo", "bar"};
  for (auto& key : keys) {
    if (auto p = m.try_emplace(key, 20); p.second) {
      std::cerr << "Emplaced key " << key << "\n";
    } else {
      std::cerr << "Element " << key << " already registered\n";
    }
  }
}
