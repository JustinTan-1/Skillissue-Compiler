#include "skillissue/ast.hpp"

#include <sstream>

namespace skillissue {
namespace {
void print_expression(std::ostream &out, const Expr &expr) {
    switch (expr.kind) {
    case Expr::Kind::Integer:
        out << expr.integer;
        break;
    case Expr::Kind::Boolean:
        out << (expr.boolean ? "true" : "false");
        break;
    case Expr::Kind::Name:
        out << expr.name << '#' << expr.symbol;
        break;
    case Expr::Kind::Unary:
        out << '(' << token_name(expr.op) << ' ';
        print_expression(out, *expr.left);
        out << ')';
        break;
    case Expr::Kind::Binary:
        out << '(' << token_name(expr.op) << ' ';
        print_expression(out, *expr.left);
        out << ' ';
        print_expression(out, *expr.right);
        out << ')';
        break;
    }
}

void print_statements(std::ostream &out, const std::vector<std::unique_ptr<Stmt>> &statements,
                      std::size_t depth) {
    for (const auto &stmt : statements) {
        out << std::string(depth * 2, ' ');
        switch (stmt->kind) {
        case Stmt::Kind::Declaration:
            out << "loot " << stmt->name << '#' << stmt->symbol << ':'
                << type_name(stmt->declared_type);
            break;
        case Stmt::Kind::Assignment:
            out << "assign " << stmt->name << '#' << stmt->symbol;
            break;
        case Stmt::Kind::Print:
            out << "yap";
            break;
        case Stmt::Kind::If:
            out << "sus";
            break;
        case Stmt::Kind::While:
            out << "grind";
            break;
        case Stmt::Kind::Return:
            out << "gg";
            break;
        case Stmt::Kind::Block:
            out << "block";
            break;
        }
        if (stmt->expression) {
            out << ' ';
            print_expression(out, *stmt->expression);
            out << " : " << type_name(stmt->expression->type);
        }
        out << '\n';
        print_statements(out, stmt->body, depth + 1);
        if (!stmt->alternative.empty()) {
            out << std::string(depth * 2, ' ') << "nah\n";
            print_statements(out, stmt->alternative, depth + 1);
        }
    }
}
} // namespace

std::string dump_ast(const Program &program) {
    std::ostringstream out;
    out << "mission main -> int\n";
    print_statements(out, program.body, 1);
    return out.str();
}

Compilation compile(const Source &source, CompilerOptions options) {
    const auto tokens = tokenize(source);
    auto program = parse(tokens);
    check_program(program);
    Compilation result;
    if (options.capture_tokens)
        result.tokens = dump_tokens(tokens);
    if (options.capture_ast)
        result.ast = dump_ast(program);
    if (options.fold_constants)
        fold_constants(program);
    result.ir = generate_ir(program, source);
    return result;
}

} // namespace skillissue
