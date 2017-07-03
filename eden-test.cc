TEST(Eden, ReadPrint) {
  const std::vector<std::pair<string, string>> cases = {
    {"(a (b))",
     "[(a\n"
     "  (b))]"},
    {"\"A\"; foo bar baz\n\"B\"",
     "[\"A\"\n"
     " \"B\"]"},
    {"foo.bar.baz",
     "[foo.bar.baz]"},
    {"\"Hello World\\\"\\n\\t\\txyz\"",
     "[\"Hello World\\\"\n"
     "\t\txyz\"]"},
    {"\\space \\a \\b \\newline",
     "[\\space\n"
     " \\a\n"
     " \\b\n"
     " \\newline]"},
    {"(aa.bb.cc (b) {:a 1 :b \"Some string\"}) ;; some comments\n (x y)",
     "[(aa.bb.cc\n"
     "  (b)\n"
     "  {:a 1,\n"
     "   :b \"Some string\"})\n"
     " (x\n"
     "  y)]"},
    {"{:a 1 :b 2 :c \\o142}",
     "[{:a 1,\n"
     "  :b 2,\n"
     "  :c \\b}]"},
  };
  for (const auto& testcase : cases) {
    const std::string& in = testcase.first;
    const std::string& want = testcase.second;
    const std::string got = eden::pprint(*eden::read(in));
    EXPECT_EQ(want, got);
  }
}

TEST(Eden, AaFromFile) {
  const std::string aa_contents = strings::ReadFileToString("AA");
  const std::string pprinted = eden::pprint(*eden::read(aa_contents));
  std::cerr << pprinted;
}
