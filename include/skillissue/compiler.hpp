#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace skillissue {

inline constexpr std::size_t max_source_bytes = 64 * 1024;
inline constexpr std::size_t max_nesting = 128;

struct Source {
    std::string name;
    std::string text;
};

struct Span {
    std::size_t offset = 0;
    std::size_t length = 1;
    std::uint32_t line = 1;
    std::uint32_t column = 1;
};

class CompileError : public std::runtime_error {
  public:
    CompileError(std::string code, Span span, std::string message,
                 std::string joke = "Skill issue: the compiler needs a clearer quest.");
    std::string code;
    Span span;
    std::string joke;
};

enum class TokenKind {
    End,
    Identifier,
    Integer,
    Mission,
    Loot,
    Grind,
    Sus,
    Nah,
    Yap,
    Gg,
    Int,
    Bool,
    True,
    False,
    LeftParen,
    RightParen,
    LeftBrace,
    RightBrace,
    Colon,
    Semicolon,
    Arrow,
    Plus,
    Minus,
    Star,
    Slash,
    Assign,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Bang,
    And,
    Or
};

// Lexemes borrow Source::text. Keep Source alive while using the tokens.
struct Token {
    TokenKind kind;
    std::string_view text;
    Span span;
};

std::string_view token_name(TokenKind kind);
std::vector<Token> tokenize(const Source &source);
// A temporary would be destroyed before the returned lexeme views could be used.
std::vector<Token> tokenize(const Source &&source) = delete;
std::string dump_tokens(const std::vector<Token> &tokens);
std::string format_diagnostic(const Source &source, const CompileError &error, bool humor = true);

struct CompilerOptions {
    bool fold_constants = true;
    bool capture_tokens = false;
    bool capture_ast = false;
};

struct Compilation {
    std::string ir;
    std::string tokens;
    std::string ast;
};

Compilation compile(const Source &source, CompilerOptions options = {});

} // namespace skillissue
