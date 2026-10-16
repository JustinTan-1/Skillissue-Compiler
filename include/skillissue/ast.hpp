#pragma once

#include "skillissue/compiler.hpp"

#include <limits>
#include <memory>
#include <span>

namespace skillissue {

enum class Type { Invalid, Int, Bool };
std::string_view type_name(Type type);
inline constexpr std::size_t no_symbol = std::numeric_limits<std::size_t>::max();

struct Expr {
    enum class Kind { Integer, Boolean, Name, Unary, Binary };
    Kind kind;
    Span span;
    Type type = Type::Invalid;
    std::int64_t integer = 0;
    bool boolean = false;
    std::string name;
    TokenKind op = TokenKind::End;
    std::size_t symbol = no_symbol;
    std::size_t depth = 1;
    std::unique_ptr<Expr> left;
    std::unique_ptr<Expr> right;
};

struct Stmt {
    enum class Kind { Declaration, Assignment, Print, If, While, Return, Block };
    Kind kind;
    Span span;
    std::string name;
    Type declared_type = Type::Invalid;
    std::size_t symbol = no_symbol;
    std::unique_ptr<Expr> expression;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<std::unique_ptr<Stmt>> alternative;
};

struct Symbol {
    std::string name;
    Type type;
    Span span;
};

struct Program {
    Span span;
    std::vector<std::unique_ptr<Stmt>> body;
    std::vector<Symbol> symbols;
};

Program parse(std::span<const Token> tokens);
void check_program(Program &program);
void fold_constants(Program &program);
std::string dump_ast(const Program &program);
std::string generate_ir(const Program &program, const Source &source);

} // namespace skillissue
