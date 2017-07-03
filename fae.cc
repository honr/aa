int main(int argc, char* argv[], char** envp) {
  os::Runtime runtime(argc, argv, envp);
  std::cout << "#args " << runtime.args().size() << "\n";
}
