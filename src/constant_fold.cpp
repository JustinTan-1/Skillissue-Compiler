#include "skillissue/ast.hpp"

#include <bit>
#include <limits>

namespace skillissue {
namespace {
bool literal(const Expr &expr) {
    return expr.kind == Expr::Kind::Integer || expr.kind == Expr::Kind::Boolean;
}
void fold(std::unique_ptr<Expr> &node) {
    if (node->left)
        fold(node->left);
    if (node->right)
        fold(node->right);
    if (node->kind != Expr::Kind::Unary && node->kind != Expr::Kind::Binary)
        return;
    if (!literal(*node->left) || (node->right && !literal(*node->right)))
        return;
    auto result = std::make_unique<Expr>();
    result->span = node->span;
    result->type = node->type;
    result->kind = node->type == Type::Int ? Expr::Kind::Integer : Expr::Kind::Boolean;
    const auto a = node->left->integer;
    const auto b = node->right ? node->right->integer : 0;
    const auto ua = std::bit_cast<std::uint64_t>(a);
    const auto ub = std::bit_cast<std::uint64_t>(b);
    const auto wrap = [](std::uint64_t value) { return std::bit_cast<std::int64_t>(value); };
    switch (node->op) {
    case TokenKind::Plus:
        result->integer = wrap(ua + ub);
        break;
    case TokenKind::Minus:
        result->integer = wrap(node->right ? ua - ub : std::uint64_t{0} - ua);
        break;
    case TokenKind::Star:
        result->integer = wrap(ua * ub);
        break;
    case TokenKind::Slash:
        if (b == 0 || (a == std::numeric_limits<std::int64_t>::min() && b == -1))
            return;
        result->integer = a / b;
        break;
    case TokenKind::Less:
        result->boolean = a < b;
        break;
    case TokenKind::LessEqual:
        result->boolean = a <= b;
        break;
    case TokenKind::Greater:
        result->boolean = a > b;
        break;
    case TokenKind::GreaterEqual:
        result->boolean = a >= b;
        break;
    case TokenKind::Equal:
    case TokenKind::NotEqual: {
        const auto equal =
            node->left->type == Type::Int ? a == b : node->left->boolean == node->right->boolean;
        result->boolean = node->op == TokenKind::Equal ? equal : !equal;
        break;
    }
    case TokenKind::Bang:
        result->boolean = !node->left->boolean;
        break;
    case TokenKind::And:
        result->boolean = node->left->boolean && node->right->boolean;
        break;
    case TokenKind::Or:
        result->boolean = node->left->boolean || node->right->boolean;
        break;
    default:
        throw std::logic_error("unsupported constant operator");
    }
    node = std::move(result);
}

void fold_list(std::vector<std::unique_ptr<Stmt>> &list) {
    for (auto &statement : list) {
        if (statement->expression)
            fold(statement->expression);
        fold_list(statement->body);
        fold_list(statement->alternative);
    }
}
} // namespace

void fold_constants(Program &program) {
    fold_list(program.body);
}

} // namespace skillissue
