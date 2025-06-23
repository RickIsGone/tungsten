module;

#include <unordered_map>
#include <fstream>

export module Tungsten.token;

namespace tungsten {

   export enum class TokenType {
      Invalid,

      // keywords
      Return,
      Exit,
      New,
      Free,
      Extern,
      Module,
      Export,
      Import,
      True,
      False,
      If,
      Else,
      For,
      While,
      Do,
      Switch,
      Case,
      Default,
      Break,
      Continue,

      // operators
      Plus,
      PlusEqual,
      PlusPlus,
      Minus,
      MinusEqual,
      MinusMinus,
      Equal,
      EqualEqual,
      Multiply,
      MultiplyEqual,
      Divide,
      DivideEqual,
      ModuleOperator,
      ModuleEqual,
      LogicalAnd,
      BitwiseAnd,
      AndEqual,
      LogicalOr,
      BitwiseOr,
      OrEqual,
      BitwiseXor,
      XorEqual,
      LogicalNot,
      NotEqual,
      Ternary,
      Greater,
      GreaterEqual,
      Less,
      LessEqual,
      ShiftLeft,
      ShiftLeftEqual,
      ShiftRight,
      ShiftRightEqual,

      // types
      Auto,
      Int,
      Uint,
      Float,
      Double,
      Bool,
      Char,
      String,
      Void,
      Uint8,
      Uint16,
      Uint32,
      Uint64,
      Int8,
      Int16,
      Int32,
      Int64,

      // literals
      IntLiteral,
      StringLiteral,
      Identifier,

      OpenParen,
      CloseParen,
      OpenBrace,
      CloseBrace,
      OpenBracket,
      CloseBracket,

      Semicolon,
      Dot,
      Comma,
      Colon,

      EndOFFile
   };

   export inline std::unordered_map<std::string, TokenType> tokensMap = {
       // keywords
       {"return", TokenType::Return},
       {"exit", TokenType::Exit},
       {"new", TokenType::New},
       {"free", TokenType::Free},
       {"extern", TokenType::Extern},
       {"module", TokenType::Module},
       {"export", TokenType::Export},
       {"import", TokenType::Import},
       {"true", TokenType::True},
       {"false", TokenType::False},
       {"if", TokenType::If},
       {"else", TokenType::Else},
       {"for", TokenType::For},
       {"while", TokenType::While},
       {"do", TokenType::Do},
       {"switch", TokenType::Switch},
       {"case", TokenType::Case},
       {"default", TokenType::Default},
       {"break", TokenType::Break},
       {"continue", TokenType::Continue},

       // types
       {"Auto", TokenType::Auto},
       {"Int", TokenType::Int},
       {"Uint", TokenType::Uint},
       {"Float", TokenType::Float},
       {"Double", TokenType::Double},
       {"Bool", TokenType::Bool},
       {"Char", TokenType::Char},
       {"String", TokenType::String},
       {"Void", TokenType::Void},
       {"Uint8", TokenType::Uint8},
       {"Uint16", TokenType::Uint16},
       {"Uint32", TokenType::Uint32},
       {"Uint64", TokenType::Uint64},
       {"Int8", TokenType::Int8},
       {"Int16", TokenType::Int16},
       {"Int32", TokenType::Int32},
       {"Int64", TokenType::Int64}};

   export struct Token {
      TokenType type{TokenType::Invalid};
      size_t position{};
      size_t length{};
   };

   export inline std::unordered_map<TokenType, std::string> tokenTypeNames = {
       {TokenType::Invalid, "Invalid"},

       // keywords
       {TokenType::Return, "Return"},
       {TokenType::Exit, "Exit"},
       {TokenType::New, "New"},
       {TokenType::Free, "Free"},
       {TokenType::Extern, "Extern"},
       {TokenType::Module, "Module"},
       {TokenType::Export, "Export"},
       {TokenType::Import, "Import"},
       {TokenType::True, "True"},
       {TokenType::False, "False"},
       {TokenType::If, "If"},
       {TokenType::Else, "Else"},
       {TokenType::For, "For"},
       {TokenType::While, "While"},
       {TokenType::Do, "Do"},
       {TokenType::Switch, "Switch"},
       {TokenType::Case, "Case"},
       {TokenType::Default, "Default"},
       {TokenType::Break, "Break"},
       {TokenType::Continue, "Continue"},

       // operators
       {TokenType::Plus, "Plus"},
       {TokenType::PlusEqual, "PlusEqual"},
       {TokenType::PlusPlus, "PlusPlus"},
       {TokenType::Minus, "Minus"},
       {TokenType::MinusEqual, "MinusEqual"},
       {TokenType::MinusMinus, "MinusMinus"},
       {TokenType::Equal, "Equal"},
       {TokenType::EqualEqual, "EqualEqual"},
       {TokenType::Multiply, "Multiply"},
       {TokenType::MultiplyEqual, "MultiplyEqual"},
       {TokenType::Divide, "Divide"},
       {TokenType::DivideEqual, "DivideEqual"},
       {TokenType::ModuleOperator, "ModuleOperator"},
       {TokenType::ModuleEqual, "ModuleEqual"},
       {TokenType::LogicalAnd, "LogicalAnd"},
       {TokenType::BitwiseAnd, "BitwiseAnd"},
       {TokenType::AndEqual, "AndEqual"},
       {TokenType::LogicalOr, "LogicalOr"},
       {TokenType::BitwiseOr, "BitwiseOr"},
       {TokenType::OrEqual, "OrEqual"},
       {TokenType::BitwiseXor, "BitwiseXor"},
       {TokenType::XorEqual, "XorEqual"},
       {TokenType::LogicalNot, "LogicalNot"},
       {TokenType::NotEqual, "NotEqual"},
       {TokenType::Ternary, "Ternary"},
       {TokenType::Greater, "Greater"},
       {TokenType::GreaterEqual, "GreaterEqual"},
       {TokenType::Less, "Less"},
       {TokenType::LessEqual, "LessEqual"},
       {TokenType::ShiftLeft, "ShiftLeft"},
       {TokenType::ShiftLeftEqual, "ShiftLeftEqual"},
       {TokenType::ShiftRight, "ShiftRight"},
       {TokenType::ShiftRightEqual, "ShiftRightEqual"},

       // types
       {TokenType::Auto, "Auto"},
       {TokenType::Int, "Int"},
       {TokenType::Uint, "Uint"},
       {TokenType::Float, "Float"},
       {TokenType::Double, "Double"},
       {TokenType::Bool, "Bool"},
       {TokenType::Char, "Char"},
       {TokenType::String, "String"},
       {TokenType::Void, "Void"},
       {TokenType::Uint8, "Uint8"},
       {TokenType::Uint16, "Uint16"},
       {TokenType::Uint32, "Uint32"},
       {TokenType::Uint64, "Uint64"},
       {TokenType::Int8, "Int8"},
       {TokenType::Int16, "Int16"},
       {TokenType::Int32, "Int32"},
       {TokenType::Int64, "Int64"},

       // literals
       {TokenType::IntLiteral, "IntLiteral"},
       {TokenType::StringLiteral, "StringLiteral"},
       {TokenType::Identifier, "Identifier"},

       {TokenType::OpenParen, "OpenParen"},
       {TokenType::CloseParen, "CloseParen"},
       {TokenType::OpenBrace, "OpenBrace"},
       {TokenType::CloseBrace, "CloseBrace"},
       {TokenType::OpenBracket, "OpenBracket"},
       {TokenType::CloseBracket, "CloseBracket"},

       {TokenType::Semicolon, "Semicolon"},
       {TokenType::Dot, "Dot"},
       {TokenType::Comma, "Comma"},
       {TokenType::Colon, "Colon"},

       {TokenType::EndOFFile, "EndOFFile"}};

} // namespace tungsten