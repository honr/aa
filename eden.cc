#include "eden.h"

#include <algorithm>
#include <cstring>
#include <iostream>

namespace eden {

namespace {
class Reader {
 public:
  static std::unique_ptr<Node> Read(const std::string& s);

 private:
  enum Context {
    NORMAL = 0,
    STRING = 1,
    ESCAPING_STRING = 2,
    COMMENT = 3,
    TOKEN = 4,
  };

  Reader();

  static int signals(const char c);
  bool recordToken();
  void eatParenthesis(const char c);
  void startMetadataMap();
  void startEscapableQuote();
  void startQuote();
  void startUnquote();
  bool eat(const char c);
  std::unique_ptr<Node> release();

  Context context_;
  std::unique_ptr<Node> root_;
  std::vector<Node*> coll_stack_;
  Node::Type token_type_;
  std::string token_;
  std::string error_;
};

// static
std::unique_ptr<Node> Reader::Read(const std::string& s) {
  Reader reader;
  reader.eat('[');
  for (const char c : s) {
    if (!reader.eat(c)) {
      std::cerr << "Error: " << reader.error_ << "\n";
      return nullptr;
    }
  }
  if (!reader.eat(']')) {
    std::cerr << "Error: " << reader.error_ << "\n";
    return nullptr;
  }
  return reader.release();
}

Reader::Reader()
    : context_(Context::NORMAL),
      root_(nullptr),
      token_type_(Node::Type::Nil),
      token_(""),
      error_("") {}

// static
int Reader::signals(const char c) {
  // [ 0 0 0 0 0 0 0 0 ]
  //           ^ ^ ^ ^
  //           | | | |
  //           | | | `-- non-control (alphanum, etc.) whitespace
  //           | | `---- paren, ", '.
  //           | `------ whitespace
  //            `------- control
  static const int kSignals[256] = {
    // (dotimes (i 256) (insert (format "      0b00000000, // %3d (%c)\n" i i)))
    0b00000000, //   0
    0b00000000, //   1
    0b00000000, //   2
    0b00000000, //   3
    0b00000000, //   4
    0b00000000, //   5
    0b00000000, //   6
    0b00000000, //   7
    0b00000000, //   8
    0b00000100, //   9 (\t)
    0b00000100, //  10 (\n)
    0b00000000, //  11
    0b00000000, //  12
    0b00000100, //  13 (\r)
    0b00000000, //  14
    0b00000000, //  15
    0b00000000, //  16
    0b00000000, //  17
    0b00000000, //  18
    0b00000000, //  19
    0b00000000, //  20
    0b00000000, //  21
    0b00000000, //  22
    0b00000000, //  23
    0b00000000, //  24
    0b00000000, //  25
    0b00000000, //  26
    0b00000000, //  27
    0b00000000, //  28
    0b00000000, //  29
    0b00000000, //  30
    0b00000000, //  31
    0b00000100, //  32 ( )
    0b00000001, //  33 (!)
    0b00000010, //  34 (")
    0b00001000, //  35 (#)
    0b00000001, //  36 ($)
    0b00000001, //  37 (%)
    0b00001000, //  38 (&)
    0b00001000, //  39 (')
    0b00000010, //  40 (()
    0b00000010, //  41 ())
    0b00000001, //  42 (*)
    0b00000001, //  43 (+)
    0b00000100, //  44 (,) - Considered whitespace
    0b00000001, //  45 (-)
    0b00000001, //  46 (.)
    0b00000001, //  47 (/)
    0b00000001, //  48 (0)
    0b00000001, //  49 (1)
    0b00000001, //  50 (2)
    0b00000001, //  51 (3)
    0b00000001, //  52 (4)
    0b00000001, //  53 (5)
    0b00000001, //  54 (6)
    0b00000001, //  55 (7)
    0b00000001, //  56 (8)
    0b00000001, //  57 (9)
    0b00000001, //  58 (:)
    0b00001000, //  59 (;)
    0b00000001, //  60 (<)
    0b00000001, //  61 (=)
    0b00000001, //  62 (>)
    0b00000001, //  63 (?)
    0b00000001, //  64 (@)
    0b00000001, //  65 (A)
    0b00000001, //  66 (B)
    0b00000001, //  67 (C)
    0b00000001, //  68 (D)
    0b00000001, //  69 (E)
    0b00000001, //  70 (F)
    0b00000001, //  71 (G)
    0b00000001, //  72 (H)
    0b00000001, //  73 (I)
    0b00000001, //  74 (J)
    0b00000001, //  75 (K)
    0b00000001, //  76 (L)
    0b00000001, //  77 (M)
    0b00000001, //  78 (N)
    0b00000001, //  79 (O)
    0b00000001, //  80 (P)
    0b00000001, //  81 (Q)
    0b00000001, //  82 (R)
    0b00000001, //  83 (S)
    0b00000001, //  84 (T)
    0b00000001, //  85 (U)
    0b00000001, //  86 (V)
    0b00000001, //  87 (W)
    0b00000001, //  88 (X)
    0b00000001, //  89 (Y)
    0b00000001, //  90 (Z)
    0b00000010, //  91 ([)
    0b00000001, //  92 (\)
    0b00000010, //  93 (])
    0b00001000, //  94 (^)
    0b00000001, //  95 (_)
    0b00001000, //  96 (`)
    0b00000001, //  97 (a)
    0b00000001, //  98 (b)
    0b00000001, //  99 (c)
    0b00000001, // 100 (d)
    0b00000001, // 101 (e)
    0b00000001, // 102 (f)
    0b00000001, // 103 (g)
    0b00000001, // 104 (h)
    0b00000001, // 105 (i)
    0b00000001, // 106 (j)
    0b00000001, // 107 (k)
    0b00000001, // 108 (l)
    0b00000001, // 109 (m)
    0b00000001, // 110 (n)
    0b00000001, // 111 (o)
    0b00000001, // 112 (p)
    0b00000001, // 113 (q)
    0b00000001, // 114 (r)
    0b00000001, // 115 (s)
    0b00000001, // 116 (t)
    0b00000001, // 117 (u)
    0b00000001, // 118 (v)
    0b00000001, // 119 (w)
    0b00000001, // 120 (x)
    0b00000001, // 121 (y)
    0b00000001, // 122 (z)
    0b00000010, // 123 ({)
    0b00000001, // 124 (|)
    0b00000010, // 125 (})
    0b00001000, // 126 (~)
    0b00000000, // 127
    0b00000000, // 128
    0b00000000, // 129
    0b00000000, // 130
    0b00000000, // 131
    0b00000000, // 132
    0b00000000, // 133
    0b00000000, // 134
    0b00000000, // 135
    0b00000000, // 136
    0b00000000, // 137
    0b00000000, // 138
    0b00000000, // 139
    0b00000000, // 140
    0b00000000, // 141
    0b00000000, // 142
    0b00000000, // 143
    0b00000000, // 144
    0b00000000, // 145
    0b00000000, // 146
    0b00000000, // 147
    0b00000000, // 148
    0b00000000, // 149
    0b00000000, // 150
    0b00000000, // 151
    0b00000000, // 152
    0b00000000, // 153
    0b00000000, // 154
    0b00000000, // 155
    0b00000000, // 156
    0b00000000, // 157
    0b00000000, // 158
    0b00000000, // 159
    0b00000000, // 160
    0b00000001, // 161 (¡)
    0b00000001, // 162 (¢)
    0b00000001, // 163 (£)
    0b00000001, // 164 (¤)
    0b00000001, // 165 (¥)
    0b00000001, // 166 (¦)
    0b00000001, // 167 (§)
    0b00000001, // 168 (¨)
    0b00000001, // 169 (©)
    0b00000001, // 170 (ª)
    0b00000010, // 171 («)
    0b00000001, // 172 (¬)
    0b00000000, // 173
    0b00000001, // 174 (®)
    0b00000001, // 175 (¯)
    0b00000001, // 176 (°)
    0b00000001, // 177 (±)
    0b00000001, // 178 (²)
    0b00000001, // 179 (³)
    0b00000010, // 180 (´)
    0b00000001, // 181 (µ)
    0b00000001, // 182 (¶)
    0b00000001, // 183 (·)
    0b00000001, // 184 (¸)
    0b00000001, // 185 (¹)
    0b00000001, // 186 (º)
    0b00000010, // 187 (»)
    0b00000001, // 188 (¼)
    0b00000001, // 189 (½)
    0b00000001, // 190 (¾)
    0b00000001, // 191 (¿)
    0b00000001, // 192 (À)
    0b00000001, // 193 (Á)
    0b00000001, // 194 (Â)
    0b00000001, // 195 (Ã)
    0b00000001, // 196 (Ä)
    0b00000001, // 197 (Å)
    0b00000001, // 198 (Æ)
    0b00000001, // 199 (Ç)
    0b00000001, // 200 (È)
    0b00000001, // 201 (É)
    0b00000001, // 202 (Ê)
    0b00000001, // 203 (Ë)
    0b00000001, // 204 (Ì)
    0b00000001, // 205 (Í)
    0b00000001, // 206 (Î)
    0b00000001, // 207 (Ï)
    0b00000001, // 208 (Ð)
    0b00000001, // 209 (Ñ)
    0b00000001, // 210 (Ò)
    0b00000001, // 211 (Ó)
    0b00000001, // 212 (Ô)
    0b00000001, // 213 (Õ)
    0b00000001, // 214 (Ö)
    0b00000001, // 215 (×)
    0b00000001, // 216 (Ø)
    0b00000001, // 217 (Ù)
    0b00000001, // 218 (Ú)
    0b00000001, // 219 (Û)
    0b00000001, // 220 (Ü)
    0b00000001, // 221 (Ý)
    0b00000001, // 222 (Þ)
    0b00000001, // 223 (ß)
    0b00000001, // 224 (à)
    0b00000001, // 225 (á)
    0b00000001, // 226 (â)
    0b00000001, // 227 (ã)
    0b00000001, // 228 (ä)
    0b00000001, // 229 (å)
    0b00000001, // 230 (æ)
    0b00000001, // 231 (ç)
    0b00000001, // 232 (è)
    0b00000001, // 233 (é)
    0b00000001, // 234 (ê)
    0b00000001, // 235 (ë)
    0b00000001, // 236 (ì)
    0b00000001, // 237 (í)
    0b00000001, // 238 (î)
    0b00000001, // 239 (ï)
    0b00000001, // 240 (ð)
    0b00000001, // 241 (ñ)
    0b00000001, // 242 (ò)
    0b00000001, // 243 (ó)
    0b00000001, // 244 (ô)
    0b00000001, // 245 (õ)
    0b00000001, // 246 (ö)
    0b00000001, // 247 (÷)
    0b00000001, // 248 (ø)
    0b00000001, // 249 (ù)
    0b00000001, // 250 (ú)
    0b00000001, // 251 (û)
    0b00000001, // 252 (ü)
    0b00000001, // 253 (ý)
    0b00000001, // 254 (þ)
    0b00000001, // 255 (ÿ)
  };
  return kSignals[static_cast<uint8_t>(c)];
}

char CharFromName(const std::string& token) {
  if (token.size() < 2) {
    return '\0';
  }
  if (token.size() == 2) {
    return token[1];
  }
  if (token.size() == 5 && token[1] == 'o') { // Octal: \oNNN
    const char a = static_cast<char>(token[2] - '0');
    const char b = static_cast<char>(token[3] - '0');
    const char c = static_cast<char>(token[4] - '0');
    if (0 > a || a > 3 || 0 > b || b > 7 || 0 > c || c > 7) {
      return '\0';
    }
    return static_cast<char>(a << 6 | b << 3 | c);
  }
  if (token.size() == 6 && token[1] == 'u') {
    const char a = static_cast<char>(token[2] - '0');
    const char b = static_cast<char>(token[3] - '0');
    const char c = static_cast<char>(token[4] - '0');
    const char d = static_cast<char>(token[5] - '0');
    if (0 > a || a > 7 || 0 > b || b > 7 || 0 > c || c > 7 || 0 > d || d > 7) {
      return '\0';
    }
    // Unicode, \uNNNN as in Java.
    // TODO: return a << 24 | b << 16 | c << 8 | d;
    return 'U';
  }
  return
      token == "\\newline" ? '\n' :
      token == "\\space" ? ' ' :
      token == "\\tab" ? '\t' :
      token == "\\formfeed" ? '\f' :
      token == "\\backspace" ? '\b' :
      token == "\\return" ? '\r' :
      '\0';
}

Node* CreateNodeFromToken(const Node::Type type, const std::string& token) {
  auto* node = new Node();
  if (type == Node::Type::String) {
    node->type = type;
    node->value = static_cast<void*>(new std::string(token));
    return node;
  }
  if (type != Node::Type::Symbol || token.size() < 1) {
    return nullptr;
  }
  const char first_char = token[0];

  if (first_char == '\\') {
    node->type = Node::Type::Char;
    node->value = static_cast<void*>(new char(CharFromName(token)));
    return node;
  }

  if (first_char == ':') {
    node->type = Node::Type::Keyword;
    node->value = static_cast<void*>(new std::string(token.substr(1)));
    return node;
  }

  // Starts with a digit or (- or +) followed by a digit.
  if (('0' <= first_char && first_char <= '9') ||
      (token.size() > 1 &&
       (first_char == '+' || first_char == '-') &&
       '0' <= token[1] && token[1] <= '9')) {
    // TODO: Parse number (integer or float, possibly with width and unit).
    node->type = Node::Type::Int;
    node->value = static_cast<void*>(new std::string(token));
  }

  node->type = type;
  node->value = static_cast<void*>(new std::string(token));
  return node;
}

bool Reader::recordToken() {
  Node* token_node = CreateNodeFromToken(token_type_, token_);
  if (token_node == nullptr) {
    return false;
  }
  coll_stack_.back()->AsNodesMut()->push_back(token_node);
  token_type_ = Node::Type::Nil;
  token_ = "";
  return true;
}

void Reader::eatParenthesis(const char c) {
  if (c == '(' || c == '[' || c == '{') { // open paren
    auto* node = new Node();
    node->type = (c == '(') ? Node::Type::List :
                 (c == '[') ? Node::Type::Vector :
                 /* c == '{' */ Node::Type::Map;
    node->value = new std::vector<Node*>;
    if (root_ == nullptr) {
      root_.reset(node);
    } else {
      coll_stack_.back()->AsNodesMut()->push_back(node);
    }
    coll_stack_.push_back(node);
  } else if (c == ')' || c == ']' || c == '}') { // close paren
    auto* coll = coll_stack_.back();
    Node::Type expected_type = (c == ')') ? Node::Type::List :
                               (c == ']') ? Node::Type::Vector :
                               /* c == '}' */ Node::Type::Map;
    if (coll->type != expected_type) {
        std::cerr << "Mismatched parens.  Tried to close a " << coll->Typename()
                  << " with a '" << c << "'.";
    }
    coll_stack_.pop_back();
  } else {
    std::cerr << "Unknown paren '" << c << "'.";
  }
}

void Reader::startMetadataMap() {
}

void Reader::startEscapableQuote() {
}

void Reader::startQuote() {
}

void Reader::startUnquote() {
}

bool Reader::eat(const char c) {
  int s = signals(c);
  if (context_ == Context::STRING) {
    if (c == '\\') {
      context_ = Context::ESCAPING_STRING;
    } else if (c == '"') {
      context_ = Context::NORMAL;
      recordToken();
    } else {
      token_ += c;
    }
    return true;
  }

  if (context_ == Context::ESCAPING_STRING) {
    context_ = Context::STRING;
    token_ += (c == 'n' ? '\n' :
               c == 't' ? '\t' :
               c == 'r' ? '\r' :
               c == 'f' ? '\f' :
               c);
    return true;
  }

  if (context_ == Context::COMMENT) {
    if (c == '\n') {
      context_ = Context::NORMAL;
    }
    return true;
  }

  if (s & 1) {
    if (context_ == Context::NORMAL) {
      context_ = Context::TOKEN;
      token_type_ = Node::Type::Symbol;
      token_ = "";
    }
    token_ += c;
    return true;
  }

  // s & 1 == 0, context is either NORMAL or TOKEN.

  if (s & 4) { // whitespace
    if (context_ == Context::NORMAL) { // more whitespace
      return true;
    }
    if (context_ == Context::TOKEN) { // control-ish
      recordToken();
      context_ = Context::NORMAL;
      return true;
    }
    std::cerr << "Unexpected context for a whitespace\n";
  }

  if (context_ == Context::TOKEN) { // control-ish
    recordToken(); // Do not return, we need to react to `c' further down.
  }

  if (c == '"') {
    context_ = Context::STRING;
    token_type_ = Node::Type::String;
    token_ = "";
    return true;
  }

  if (c == ';') {
    context_ = Context::COMMENT;
    return true;
  }

  if (s & 2) { // Paren start or end
    eatParenthesis(c);
    context_ = Context::NORMAL;
    return true;
  }

  if (c == '#') {
    //// context_ = Context::Normal;
    return true;
  }

  if (c == '^') {
    startMetadataMap();
    context_ = Context::NORMAL;
    return true;
  }

  if (c == '`') {
    startEscapableQuote();
    context_ = Context::NORMAL;
    return true;
  }

  if (c == '\'') {
    startQuote();
    context_ = Context::NORMAL;
    return true;
  }

  if (c == '~') {
    startUnquote();
    context_ = Context::NORMAL;
    return true;
  }

  std::cerr << "Unknown state: {:context " << context_
            << ", :char '" << c << "'}\n";
  exit(1);
}

std::unique_ptr<Node> Reader::release() {
  return std::move(root_);
}
} // ::

std::unique_ptr<Node> read(const std::string& s) {
  return Reader::Read(s);
}

namespace {
std::string escapeQuotes(const std::string& before) {
  std::string after;
  after.reserve(before.length() + 4);
  for (std::string::size_type i = 0; i < before.length(); ++i) {
    switch (before[i]) {
      case '"':
      case '\\':
        after += '\\';
      default:
        after += before[i];
    }
  }
  return after;
}

std::string charName(const char c) {
  return
    c == '\n' ? "newline" :
    c == ' ' ? "space" :
    c == '\t' ? "tab" :
    c == '\f' ? "formfeed" :
    c == '\b' ? "backspace" :
    c == '\r' ? "return" :
    std::string(1, c);
}
} // ::

const std::string pprint(const Node& node, size_t indent) {
  std::string prefix(indent, ' ');
  std::string output;
  if (node.IsCollection()) {
    static const std::string paren_pairs[] =
        {"(", ")", "[", "]", "{", "}", "#{", "}"};
    const int paren_type =
        static_cast<int>(node.type) - static_cast<int>(Node::Type::List);
    output += paren_pairs[paren_type * 2];
    const std::vector<Node*>& values = node.AsNodes();
    bool first_iteration = true;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
      if (first_iteration) {
        first_iteration = false;
      } else {
        output += (node.IsMap() ? ",\n" : "\n") + prefix;
      }
      output += pprint(**it, indent + 1);
      if (node.IsMap()) {
        ++it;
        output += " " + pprint(**it, indent + 1);
      }
    }
    output += paren_pairs[1 + paren_type * 2];
  } else if (node.IsNil()) {
    output = "nil";
  } else if (node.IsBool()) {
    output = node.As<bool>() ? "true" : "false";
  } else if (node.IsChar()) {
    output = "\\" + charName(node.As<char>());
  } else if (node.IsInt()) {
    output = std::to_string(node.As<int>());
  } else if (node.IsFloat()) {
    output = std::to_string(node.As<double>());
  } else if (node.IsString()) {
    output = "\"" + escapeQuotes(node.AsString()) + "\"";
  } else if (node.IsSymbol()) {
    output = node.AsString();
  } else if (node.IsKeyword()) {
    output = ":" + node.AsString();
  }
  return output;
}

}  // ::eden
