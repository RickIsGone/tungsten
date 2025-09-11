module;

#include <unordered_map>
#include <string>

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
      Class,
      Public,
      Private,
      Protected,
      Static,
      Namespace,
      StaticCast,
      ConstCast,
      Alias,

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
      Num,
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
      Uint128,
      Int8,
      Int16,
      Int32,
      Int64,
      Int128,

      // literals
      IntLiteral,
      FloatLiteral,
      StringLiteral,
      Identifier,

      // constants
      Null,
      Nullptr,
      Nan,
      CodeSuccess,
      CodeFailure,

      OpenParen,
      CloseParen,
      OpenBrace,
      CloseBrace,
      OpenBracket,
      CloseBracket,

      Semicolon,
      Dot,
      Arrow,
      Comma,
      Colon,

      // core functions
      __BuiltinFunction,
      __BuiltinLine,
      __BuiltinColumn,
      __BuiltinFile,
      Typeof,
      Nameof,
      Sizeof,

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
       {"class", TokenType::Class},
       {"public", TokenType::Public},
       {"private", TokenType::Private},
       {"protected", TokenType::Protected},
       {"static", TokenType::Static},
       {"namespace", TokenType::Namespace},
       {"staticCast", TokenType::StaticCast},
       {"constCast", TokenType::ConstCast},
       // {"alias", TokenType::Alias},

       // types
       {"Num", TokenType::Num},
       // {"Auto", TokenType::Auto},
       // {"Int", TokenType::Int},
       // {"Uint", TokenType::Uint},
       // {"Float", TokenType::Float},
       // {"Double", TokenType::Double},
       {"Bool", TokenType::Bool},
       {"Char", TokenType::Char},
       {"String", TokenType::String},
       {"Void", TokenType::Void},
       // {"Uint8", TokenType::Uint8},
       // {"Uint16", TokenType::Uint16},
       // {"Uint32", TokenType::Uint32},
       // {"Uint64", TokenType::Uint64},
       // {"Uint128", TokenType::Uint128},
       // {"Int8", TokenType::Int8},
       // {"Int16", TokenType::Int16},
       // {"Int32", TokenType::Int32},
       // {"Int64", TokenType::Int64},
       // {"Int128", TokenType::Int128},

       // constants
       {"null", TokenType::Null},
       {"nullptr", TokenType::Nullptr},
       {"NaN", TokenType::Nan},
       {"CodeSuccess", TokenType::CodeSuccess},
       {"CodeFailure", TokenType::CodeFailure},

       // core functions
       {"__builtinFunction", TokenType::__BuiltinFunction},
       {"__builtinColumn", TokenType::__BuiltinColumn},
       {"__builtinLine", TokenType::__BuiltinLine},
       {"__builtinFile", TokenType::__BuiltinFile},
       {"nameof", TokenType::Nameof},
       {"typeof", TokenType::Typeof},
       {"sizeof", TokenType::Sizeof}};

   export struct Token {
      TokenType type{TokenType::Invalid};
      size_t position{};
      size_t length{};
   };

} // namespace tungsten