#include "skillissue/ast.hpp"

#include <algorithm>
#include <charconv>
#include <limits>
#include <utility>

namespace skillissue {
namespace {

class DepthGuard {
  public:
    DepthGuard(std::size_t &depth, Span span) : depth_(depth) {
        if (depth_ >= max_nesting)
            throw CompileError("E003", span, "nesting exceeds the limit of 128");
        ++depth_;
    }
    ~DepthGuard() {
        --depth_;
    }
    DepthGuard(const DepthGuard &) = delete;
    DepthGuard &operator=(const DepthGuard &) = delete;

  private:
    std::size_t &depth_;
};

class Parser {
  public:
    explicit Parser(std::span<const Token> tokens) : tokens_(tokens) {
        if (tokens.empty() || tokens.back().kind != TokenKind::End) {
            throw std::invalid_argument("parser requires an EOF token");
        }
    }

    Program run() {
        const auto start = expect(TokenKind::Mission);
        const auto name = expect(TokenKind::Identifier);
        if (name.text != "main")
            throw CompileError("E004", name.span, "the only supported mission is 'main'");
        expect(TokenKind::LeftParen);
        expect(TokenKind::RightParen);
        expect(TokenKind::Arrow);
        expect(TokenKind::Int);
        Program program;
        program.span = start.span;
        program.body = block();
        expect(TokenKind::End);
        return program;
    }

  private:
    std::span<const Token> tokens_;
    std::size_t position_ = 0, block_depth_ = 0, expression_depth_ = 0;

    const Token &peek() const {
        return tokens_[position_];
    }
    bool match(TokenKind kind) {
        if (peek().kind != kind)
            return false;
        if (kind != TokenKind::End)
            ++position_;
        return true;
    }
    Token take() {
        auto token = peek();
        if (token.kind != TokenKind::End)
            ++position_;
        return token;
    }
    Token expect(TokenKind kind) {
        if (peek().kind != kind) {
            throw CompileError("E004", peek().span,
                               "expected '" + std::string(token_name(kind)) + "', found '" +
                                   std::string(token_name(peek().kind)) + "'",
                               "Skill issue: check your quest punctuation.");
        }
        return take();
    }
    Type parse_type() {
        if (match(TokenKind::Int))
            return Type::Int;
        if (match(TokenKind::Bool))
            return Type::Bool;
        throw CompileError("E004", peek().span, "expected type 'int' or 'bool'");
    }
    std::vector<std::unique_ptr<Stmt>> block() {
        const auto open = expect(TokenKind::LeftBrace);
        DepthGuard guard(block_depth_, open.span);
        std::vector<std::unique_ptr<Stmt>> statements;
        while (peek().kind != TokenKind::RightBrace) {
            if (peek().kind == TokenKind::End)
                expect(TokenKind::RightBrace);
            statements.push_back(statement());
        }
        expect(TokenKind::RightBrace);
        return statements;
    }
    std::unique_ptr<Stmt> statement() {
        auto stmt = std::make_unique<Stmt>();
        stmt->span = peek().span;
        switch (peek().kind) {
        case TokenKind::Loot: {
            take();
            stmt->kind = Stmt::Kind::Declaration;
            const auto name = expect(TokenKind::Identifier);
            stmt->name = std::string(name.text);
            stmt->span = name.span;
            expect(TokenKind::Colon);
            stmt->declared_type = parse_type();
            expect(TokenKind::Assign);
            stmt->expression = expression();
            expect(TokenKind::Semicolon);
            break;
        }
        case TokenKind::Identifier: {
            stmt->kind = Stmt::Kind::Assignment;
            stmt->name = std::string(take().text);
            expect(TokenKind::Assign);
            stmt->expression = expression();
            expect(TokenKind::Semicolon);
            break;
        }
        case TokenKind::Yap:
            take();
            stmt->kind = Stmt::Kind::Print;
            expect(TokenKind::LeftParen);
            stmt->expression = expression();
            expect(TokenKind::RightParen);
            expect(TokenKind::Semicolon);
            break;
        case TokenKind::Gg:
            take();
            stmt->kind = Stmt::Kind::Return;
            stmt->expression = expression();
            expect(TokenKind::Semicolon);
            break;
        case TokenKind::Sus:
        case TokenKind::Grind: {
            const bool loop = take().kind == TokenKind::Grind;
            stmt->kind = loop ? Stmt::Kind::While : Stmt::Kind::If;
            expect(TokenKind::LeftParen);
            stmt->expression = expression();
            expect(TokenKind::RightParen);
            stmt->body = block();
            if (!loop && match(TokenKind::Nah))
                stmt->alternative = block();
            break;
        }
        case TokenKind::LeftBrace:
            stmt->kind = Stmt::Kind::Block;
            stmt->body = block();
            break;
        default:
            throw CompileError(
                "E004", peek().span,
                "expected a declaration, assignment, block, or SkillIssue statement");
        }
        return stmt;
    }

    std::unique_ptr<Expr> expression(int level = 0) {
        if (level == 6)
            return unary();
        auto left = expression(level + 1);
        while (at_level(peek().kind, level)) {
            const auto op = take();
            auto right = expression(level + 1);
            auto node = std::make_unique<Expr>();
            node->kind = Expr::Kind::Binary;
            node->span = op.span;
            node->op = op.kind;
            node->depth = 1 + std::max(left->depth, right->depth);
            check_depth(*node);
            node->left = std::move(left);
            node->right = std::move(right);
            left = std::move(node);
        }
        return left;
    }
    static bool at_level(TokenKind kind, int level) {
        switch (level) {
        case 0:
            return kind == TokenKind::Or;
        case 1:
            return kind == TokenKind::And;
        case 2:
            return kind == TokenKind::Equal || kind == TokenKind::NotEqual;
        case 3:
            return kind == TokenKind::Less || kind == TokenKind::LessEqual ||
                   kind == TokenKind::Greater || kind == TokenKind::GreaterEqual;
        case 4:
            return kind == TokenKind::Plus || kind == TokenKind::Minus;
        case 5:
            return kind == TokenKind::Star || kind == TokenKind::Slash;
        default:
            return false;
        }
    }
    static void check_depth(const Expr &expression) {
        if (expression.depth > max_nesting) {
            throw CompileError("E003", expression.span, "expression tree exceeds the limit of 128");
        }
    }
    static std::uint64_t magnitude(const Token &token) {
        std::uint64_t result = 0;
        const auto [end, error] =
            std::from_chars(token.text.data(), token.text.data() + token.text.size(), result);
        if (error != std::errc{} || end != token.text.data() + token.text.size()) {
            throw CompileError("E002", token.span,
                               "integer literal is outside the signed 64-bit range");
        }
        return result;
    }
    static std::unique_ptr<Expr> integer(Span span, std::int64_t value) {
        auto node = std::make_unique<Expr>();
        node->kind = Expr::Kind::Integer;
        node->span = span;
        node->integer = value;
        return node;
    }
    std::unique_ptr<Expr> unary() {
        DepthGuard guard(expression_depth_, peek().span);
        if (peek().kind == TokenKind::Minus || peek().kind == TokenKind::Bang) {
            const auto op = take();
            // The positive magnitude of INT64_MIN cannot be a standalone int.
            if (op.kind == TokenKind::Minus && peek().kind == TokenKind::Integer &&
                magnitude(peek()) == (std::uint64_t{1} << 63)) {
                const auto literal = take();
                auto span = op.span;
                span.length = literal.span.offset + literal.span.length - span.offset;
                return integer(span, std::numeric_limits<std::int64_t>::min());
            }
            auto node = std::make_unique<Expr>();
            node->kind = Expr::Kind::Unary;
            node->span = op.span;
            node->op = op.kind;
            node->left = unary();
            node->depth = 1 + node->left->depth;
            check_depth(*node);
            return node;
        }
        const auto token = take();
        if (token.kind == TokenKind::Integer) {
            const auto value = magnitude(token);
            if (value > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                throw CompileError("E002", token.span,
                                   "integer literal is outside the signed 64-bit range");
            }
            return integer(token.span, static_cast<std::int64_t>(value));
        }
        if (token.kind == TokenKind::LeftParen) {
            auto node = expression();
            expect(TokenKind::RightParen);
            return node;
        }
        auto node = std::make_unique<Expr>();
        node->span = token.span;
        if (token.kind == TokenKind::True || token.kind == TokenKind::False) {
            node->kind = Expr::Kind::Boolean;
            node->boolean = token.kind == TokenKind::True;
        } else if (token.kind == TokenKind::Identifier) {
            node->kind = Expr::Kind::Name;
            node->name = std::string(token.text);
        } else {
            throw CompileError("E004", token.span, "expected an expression");
        }
        return node;
    }
};
} // namespace

Program parse(std::span<const Token> tokens) {
    return Parser(tokens).run();
}

} // namespace skillissue
