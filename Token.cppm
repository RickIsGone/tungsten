module;

#include <unordered_map>
#include <fstream>

export module Tungsten.token;


namespace tungsten {
   export std::string _fileContents{};

   export enum class TokenType {
      INVALID = -1,

      // keywords
      RETURN = -2,
      EXIT = -3,
      NEW = -4,
      FREE = -5,
      EXTERN = -6,
      MODULE = -7,
      EXPORT = -8,
      IMPORT = -9,

      // operators
      PLUS = -10,
      PLUS_EQUAL = -11,
      PLUS_PLUS = -12,
      MINUS = -13,
      MINUS_EQUAL = -14,
      MINUS_MINUS = -15,
      EQUAL = -16,
      EQUAL_EQUAL = -17,
      MULTIPLY = -18,
      MULTIPLY_EQUAL = -19,
      DIVIDE = -20,
      DIVIDE_EQUAL = -21,
      MODULE_OPERATOR = -22,
      MODULE_EQUAL = -23,
      LOGICAL_AND = -24,
      BITWISE_AND = -25,
      AND_EQUAL = -26,
      LOGICAL_OR = -27,
      BITWISE_OR = -28,
      OR_EQUAL = -29,
      BITWISE_XOR = -30,
      XOR_EQUAL = -31,
      LOGICAL_NOT = -32,
      NOT_EQUAL = -33,
      TERNARY = -34,

      // types
      INT = -35,
      FLOAT = -36,
      DOUBLE = -37,
      BOOL = -38,
      CHAR = -39,
      STRING = -40,
      VOID = -41,
      UINT = -42,
      UINT8 = -43,
      UINT16 = -44,
      UINT32 = -45,
      UINT64 = -46,
      INT8 = -47,
      INT16 = -48,
      INT32 = -49,
      INT64 = -50,

      // literals
      INT_LITERAL = -51,
      STRING_LITERAL = -52,
      IDENTIFIER = -53,

      OPEN_PAREN = -54,
      CLOSE_PAREN = -55,
      OPEN_BRACE = -56,
      CLOSE_BRACE = -57,
      OPEN_BRACKET = -58,
      CLOSE_BRACKET = -59,

      SEMICOLON = -60,
      DOT = -61,
      COMMA = -62,
      COLON = -63,

      END_OF_FILE = -64
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

   export std::ostream& operator<<(std::ostream& out, const std::vector<Token>& tokens) {
      out << "Tokens: {";

      for (int i = 1; const Token& token : tokens) {
         out << "{" << tokenTypeNames.at(token.type);
         if (token.type == TokenType::INT_LITERAL || token.type == TokenType::STRING_LITERAL || token.type == TokenType::IDENTIFIER || token.type == TokenType::INVALID)
            out << ", " << _fileContents.substr(token.position, token.length);

         out << (i++ < tokens.size() ? "},\n\t " : "}");
      }
      out << "}\n";
      return out;
   }

} // namespace tungsten