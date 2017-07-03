#include <memory>
#include <string>
#include <vector>

namespace eden {
// Maybe a small hierarchy of nodes (a base one without value/values fields),
// with subclasses containing the actual values.
struct Node {
  enum class Type {
    // Atoms
    Nil = 0,
    Bool = 1,
    Char = 2,
    Int = 3,
    Float = 4,
    String = 5,
    Symbol = 6,
    Keyword = 7,
    // Collections
    List = 8,  // (type & Type::List) != 0 for collection types.
    Vector = 9,
    Map = 10,
    Set = 11,
  };
  Type type;
  void* value;

  const std::string& Typename() const {
    static const std::string typenames[] = {
        "nil", "bool", "char", "int", "float", "string", "symbol",
        "keyword", "list", "vector", "map", "set",
    };
    return typenames[static_cast<int>(type)];
  }

  bool IsCollection() const {
    return static_cast<int>(type) & static_cast<int>(Node::Type::List);
  }

  template <typename T>
  const T& As() const {
    if (value == nullptr) {
      // Die?
      exit(1);
    }
    return *static_cast<const T*>(value);
  }

  const std::string& AsString() const {
    return *static_cast<std::string*>(value);
  }

  std::string* AsStringMut() {
    return static_cast<std::string*>(value);
  }

  const std::vector<Node*>& AsNodes() const {
    return *static_cast<std::vector<Node*>*>(value);
  }

  std::vector<Node*>* AsNodesMut() {
    return static_cast<std::vector<Node*>*>(value);
  }

  bool IsNil() const { return type == Type::Nil; }
  bool IsBool() const { return type == Type::Bool; }
  bool IsChar() const { return type == Type::Char; }
  bool IsInt() const { return type == Type::Int; }
  bool IsFloat() const { return type == Type::Float; }
  bool IsString() const { return type == Type::String; }
  bool IsSymbol() const { return type == Type::Symbol; }
  bool IsKeyword() const { return type == Type::Keyword; }
  bool IsList() const { return type == Type::List; }
  bool IsVector() const { return type == Type::Vector; }
  bool IsMap() const { return type == Type::Map; }
  bool IsSet() const { return type == Type::Set; }
};

std::unique_ptr<Node> read(const std::string& s);

const std::string pprint(const Node& node, size_t indent = 1);

}  // ::eden
