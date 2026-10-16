#include "skillissue/ast.hpp"

#include <unordered_map>

namespace skillissue {

std::string_view type_name(Type type) {
    switch (type) {
    case Type::Int:
        return "int";
    case Type::Bool:
        return "bool";
    case Type::Invalid:
        return "unresolved";
    }
    throw std::logic_error("unknown type");
}

namespace {
class Checker {
  public:
    explicit Checker(Program &program) : program_(program) {}
    void run() {
        program_.symbols.clear();
        if (program_.body.empty() || program_.body.back()->kind != Stmt::Kind::Return) {
            throw CompileError("E013", program_.span,
                               "main must end with a top-level 'gg' statement",
                               "Skill issue: finish the mission with gg.");
        }
        list(program_.body, true);
    }

  private:
    Program &program_;
    std::vector<std::unordered_map<std::string, std::size_t>> scopes_;

    std::size_t lookup(const std::string &name) const {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            if (const auto it = scope->find(name); it != scope->end())
                return it->second;
        }
        return no_symbol;
    }
    std::size_t resolve(const std::string &name, Span span) const {
        const auto id = lookup(name);
        if (id == no_symbol) {
            throw CompileError("E010", span, "unknown variable '" + name + "'",
                               "Skill issue: you haven't unlocked this variable.");
        }
        return id;
    }
    static void require(Type actual, Type expected, Span span) {
        if (actual != expected) {
            throw CompileError("E012", span,
                               "expected " + std::string(type_name(expected)) + ", received " +
                                   std::string(type_name(actual)),
                               "Skill issue: that value is in the wrong inventory slot.");
        }
    }
    Type expression(Expr &expr) {
        switch (expr.kind) {
        case Expr::Kind::Integer:
            expr.type = Type::Int;
            break;
        case Expr::Kind::Boolean:
            expr.type = Type::Bool;
            break;
        case Expr::Kind::Name:
            expr.symbol = resolve(expr.name, expr.span);
            expr.type = program_.symbols[expr.symbol].type;
            break;
        case Expr::Kind::Unary: {
            const auto type = expr.op == TokenKind::Bang ? Type::Bool : Type::Int;
            require(expression(*expr.left), type, expr.span);
            expr.type = type;
            break;
        }
        case Expr::Kind::Binary: {
            const auto left = expression(*expr.left);
            const auto right = expression(*expr.right);
            if (expr.op == TokenKind::And || expr.op == TokenKind::Or) {
                require(left, Type::Bool, expr.span);
                require(right, Type::Bool, expr.span);
                expr.type = Type::Bool;
            } else if (expr.op == TokenKind::Equal || expr.op == TokenKind::NotEqual) {
                require(right, left, expr.span);
                expr.type = Type::Bool;
            } else {
                require(left, Type::Int, expr.span);
                require(right, Type::Int, expr.span);
                expr.type = (expr.op == TokenKind::Plus || expr.op == TokenKind::Minus ||
                             expr.op == TokenKind::Star || expr.op == TokenKind::Slash)
                                ? Type::Int
                                : Type::Bool;
            }
            break;
        }
        }
        return expr.type;
    }
    void list(std::vector<std::unique_ptr<Stmt>> &statements, bool top = false) {
        scopes_.emplace_back();
        for (std::size_t i = 0; i < statements.size(); ++i) {
            auto &stmt = *statements[i];
            switch (stmt.kind) {
            case Stmt::Kind::Declaration:
                if (lookup(stmt.name) != no_symbol) {
                    throw CompileError("E011", stmt.span,
                                       "duplicate or shadowed variable '" + stmt.name + "'",
                                       "Skill issue: that inventory slot is already occupied.");
                }
                require(expression(*stmt.expression), stmt.declared_type, stmt.span);
                stmt.symbol = program_.symbols.size();
                program_.symbols.push_back({stmt.name, stmt.declared_type, stmt.span});
                scopes_.back().emplace(stmt.name, stmt.symbol);
                break;
            case Stmt::Kind::Assignment:
                stmt.symbol = resolve(stmt.name, stmt.span);
                require(expression(*stmt.expression), program_.symbols[stmt.symbol].type,
                        stmt.span);
                break;
            case Stmt::Kind::Print:
                expression(*stmt.expression);
                break;
            case Stmt::Kind::If:
            case Stmt::Kind::While:
                require(expression(*stmt.expression), Type::Bool, stmt.expression->span);
                list(stmt.body);
                if (stmt.kind == Stmt::Kind::If)
                    list(stmt.alternative);
                break;
            case Stmt::Kind::Return:
                if (!top || i + 1 != statements.size()) {
                    throw CompileError("E013", stmt.span,
                                       "'gg' is only allowed as the final top-level statement");
                }
                require(expression(*stmt.expression), Type::Int, stmt.span);
                break;
            case Stmt::Kind::Block:
                list(stmt.body);
                break;
            }
        }
        scopes_.pop_back();
    }
};
} // namespace

void check_program(Program &program) {
    Checker(program).run();
}

} // namespace skillissue
