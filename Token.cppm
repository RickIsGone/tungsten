module;

#include <unordered_map>
#include <fstream>

export module Tungsten.token;


namespace tungsten {

   export enum class TokenType {
      INVALID,

      // keywords
      RETURN,
      EXIT,
      NEW,
      FREE,
      EXTERN,
      MODULE,
      EXPORT,
      IMPORT,
      TRUE,
      FALSE,

      // operators
      PLUS,
      PLUS_EQUAL,
      PLUS_PLUS,
      MINUS,
      MINUS_EQUAL,
      MINUS_MINUS,
      EQUAL,
      EQUAL_EQUAL,
      MULTIPLY,
      MULTIPLY_EQUAL,
      DIVIDE,
      DIVIDE_EQUAL,
      MODULE_OPERATOR,
      MODULE_EQUAL,
      LOGICAL_AND,
      BITWISE_AND,
      AND_EQUAL,
      LOGICAL_OR,
      BITWISE_OR,
      OR_EQUAL,
      BITWISE_XOR,
      XOR_EQUAL,
      LOGICAL_NOT,
      NOT_EQUAL,
      TERNARY,
      GREATER,
      GREATER_EQUAL,
      LESS,
      LESS_EQUAL,
      SHIFT_LEFT,
      SHIFT_LEFT_EQUAL,
      SHIFT_RIGHT,
      SHIFT_RIGHT_EQUAL,

      // types
      INT,
      FLOAT,
      DOUBLE,
      BOOL,
      CHAR,
      STRING,
      VOID,
      UINT,
      UINT8,
      UINT16,
      UINT32,
      UINT64,
      INT8,
      INT16,
      INT32,
      INT64,

      // literals
      INT_LITERAL,
      STRING_LITERAL,
      IDENTIFIER,

      OPEN_PAREN,
      CLOSE_PAREN,
      OPEN_BRACE,
      CLOSE_BRACE,
      OPEN_BRACKET,
      CLOSE_BRACKET,

      SEMICOLON,
      DOT,
      COMMA,
      COLON,

      END_OF_FILE
   };
   export inline std::unordered_map<std::string, TokenType> tokensMap = {
       // keywords
       {"return", TokenType::RETURN},
       {"exit", TokenType::EXIT},
       {"new", TokenType::NEW},
       {"free", TokenType::FREE},
       {"extern", TokenType::EXTERN},
       {"module", TokenType::MODULE},
       {"export", TokenType::EXPORT},
       {"import", TokenType::IMPORT},
       {"true", TokenType::TRUE},
       {"false", TokenType::FALSE},

       // types
       {"Int", TokenType::INT},
       {"Float", TokenType::FLOAT},
       {"Double", TokenType::DOUBLE},
       {"Bool", TokenType::BOOL},
       {"Char", TokenType::CHAR},
       {"String", TokenType::STRING},
       {"Void", TokenType::VOID},
       {"Uint", TokenType::UINT},
       {"Uint8", TokenType::UINT8},
       {"Uint16", TokenType::UINT16},
       {"Uint32", TokenType::UINT32},
       {"Uint64", TokenType::UINT64},
       {"Int8", TokenType::INT8},
       {"Int16", TokenType::INT16},
       {"Int32", TokenType::INT32},
       {"Int64", TokenType::INT64}};

   export struct Token {
      TokenType type{TokenType::INVALID};
      size_t position{};
      size_t length{};
   };

   export inline std::unordered_map<TokenType, std::string> tokenTypeNames = {
       {TokenType::INVALID, "INVALID"},

       {TokenType::RETURN, "RETURN"},
       {TokenType::EXIT, "EXIT"},
       {TokenType::NEW, "NEW"},
       {TokenType::FREE, "FREE"},
       {TokenType::EXTERN, "EXTERN"},
       {TokenType::MODULE, "MODULE"},
       {TokenType::EXPORT, "EXPORT"},
       {TokenType::IMPORT, "IMPORT"},

       {TokenType::PLUS, "PLUS"},
       {TokenType::PLUS_EQUAL, "PLUS_EQUAL"},
       {TokenType::PLUS_PLUS, "PLUS_PLUS"},
       {TokenType::MINUS, "MINUS"},
       {TokenType::MINUS_EQUAL, "MINUS_EQUAL"},
       {TokenType::MINUS_MINUS, "MINUS_MINUS"},
       {TokenType::EQUAL, "EQUAL"},
       {TokenType::EQUAL_EQUAL, "EQUAL_EQUAL"},
       {TokenType::MULTIPLY, "MULTIPLY"},
       {TokenType::MULTIPLY_EQUAL, "MULTIPLY_EQUAL"},
       {TokenType::DIVIDE, "DIVIDE"},
       {TokenType::DIVIDE_EQUAL, "DIVIDE_EQUAL"},
       {TokenType::MODULE, "MODULE"},
       {TokenType::MODULE_EQUAL, "MODULE_EQUAL"},
       {TokenType::LOGICAL_AND, "LOGICAL_AND"},
       {TokenType::BITWISE_AND, "BITWISE_AND"},
       {TokenType::AND_EQUAL, "AND_EQUAL"},
       {TokenType::LOGICAL_OR, "LOGICAL_OR"},
       {TokenType::BITWISE_OR, "BITWISE_OR"},
       {TokenType::OR_EQUAL, "OR_EQUAL"},
       {TokenType::BITWISE_XOR, "BITWISE_XOR"},
       {TokenType::XOR_EQUAL, "XOR_EQUAL"},
       {TokenType::LOGICAL_NOT, "LOGICAL_NOT"},
       {TokenType::NOT_EQUAL, "NOT_EQUAL"},
       {TokenType::TERNARY, "TERNARY"},
       {TokenType::GREATER, "GREATER"},
       {TokenType::GREATER_EQUAL, "GREATER_EQUAL"},
       {TokenType::LESS, "LESS"},
       {TokenType::LESS_EQUAL, "LESS_EQUAL"},
       {TokenType::SHIFT_LEFT, "SHIFT_LEFT"},
       {TokenType::SHIFT_LEFT_EQUAL, "SHIFT_LEFT_EQUAL"},
       {TokenType::SHIFT_RIGHT, "SHIFT_RIGHT"},
       {TokenType::SHIFT_RIGHT_EQUAL, "SHIFT_RIGHT_EQUAL"},

       {TokenType::INT, "INT"},
       {TokenType::FLOAT, "FLOAT"},
       {TokenType::DOUBLE, "DOUBLE"},
       {TokenType::BOOL, "BOOL"},
       {TokenType::CHAR, "CHAR"},
       {TokenType::STRING, "STRING"},
       {TokenType::VOID, "VOID"},
       {TokenType::UINT, "UINT"},
       {TokenType::UINT8, "UINT8"},
       {TokenType::UINT16, "UINT16"},
       {TokenType::UINT32, "UINT32"},
       {TokenType::UINT64, "UINT64"},
       {TokenType::INT8, "INT8"},
       {TokenType::INT16, "INT16"},
       {TokenType::INT32, "INT32"},
       {TokenType::INT64, "INT64"},

       {TokenType::INT_LITERAL, "INT_LITERAL"},
       {TokenType::STRING_LITERAL, "STRING_LITERAL"},
       {TokenType::IDENTIFIER, "IDENTIFIER"},

       {TokenType::OPEN_PAREN, "OPEN_PAREN"},
       {TokenType::CLOSE_PAREN, "CLOSE_PAREN"},
       {TokenType::OPEN_BRACE, "OPEN_BRACE"},
       {TokenType::CLOSE_BRACE, "CLOSE_BRACE"},
       {TokenType::OPEN_BRACKET, "OPEN_BRACKET"},
       {TokenType::CLOSE_BRACKET, "CLOSE_BRACKET"},

       {TokenType::SEMICOLON, "SEMICOLON"},
       {TokenType::DOT, "DOT"},
       {TokenType::COMMA, "COMMA"},
       {TokenType::COLON, "COLON"},

       {TokenType::END_OF_FILE, "END_OF_FILE"}};

} // namespace tungsten