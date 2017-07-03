namespace hello::trans {

int f() {
  static int i = 0;
  synchronized {
    std::cout << i << " -> ";
    ++i; // each call to f() obtains a unique value of i
    std::cout << i << '\n';
    return i;
  }
}

} // ::hello

int main() {
  std::vector<std::thread> v(5);
  for(auto& t: v) {
    t = std::thread([]{
      for(int n = 0; n < 3; ++n) {
        hello::trans::f();
      }
    });
  }
  for(auto& t: v) {
    t.join();
  }
}
