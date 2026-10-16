#include "skillissue/compiler.hpp"

#include <algorithm>
#include <sstream>
#include <utility>

namespace skillissue {

CompileError::CompileError(std::string error_code, Span error_span, std::string message,
                           std::string error_joke)
    : std::runtime_error(std::move(message)), code(std::move(error_code)), span(error_span),
      joke(std::move(error_joke)) {}

std::string_view token_name(TokenKind kind) {
    switch (kind) {
    case TokenKind::End:
        return "end of file";
    case TokenKind::Identifier:
        return "identifier";
    case TokenKind::Integer:
        return "integer";
    case TokenKind::Mission:
        return "mission";
    case TokenKind::Loot:
        return "loot";
    case TokenKind::Grind:
        return "grind";
    case TokenKind::Sus:
        return "sus";
    case TokenKind::Nah:
        return "nah";
    case TokenKind::Yap:
        return "yap";
    case TokenKind::Gg:
        return "gg";
    case TokenKind::Int:
        return "int";
    case TokenKind::Bool:
        return "bool";
    case TokenKind::True:
        return "true";
    case TokenKind::False:
        return "false";
    case TokenKind::LeftParen:
        return "(";
    case TokenKind::RightParen:
        return ")";
    case TokenKind::LeftBrace:
        return "{";
    case TokenKind::RightBrace:
        return "}";
    case TokenKind::Colon:
        return ":";
    case TokenKind::Semicolon:
        return ";";
    case TokenKind::Arrow:
        return "->";
    case TokenKind::Plus:
        return "+";
    case TokenKind::Minus:
        return "-";
    case TokenKind::Star:
        return "*";
    case TokenKind::Slash:
        return "/";
    case TokenKind::Assign:
        return "=";
    case TokenKind::Equal:
        return "==";
    case TokenKind::NotEqual:
        return "!=";
    case TokenKind::Less:
        return "<";
    case TokenKind::LessEqual:
        return "<=";
    case TokenKind::Greater:
        return ">";
    case TokenKind::GreaterEqual:
        return ">=";
    case TokenKind::Bang:
        return "!";
    case TokenKind::And:
        return "&&";
    case TokenKind::Or:
        return "||";
    }
    throw std::logic_error("unknown token kind");
}

namespace {
bool alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
bool digit(char c) {
    return c >= '0' && c <= '9';
}

TokenKind keyword(std::string_view word) {
    const std::pair<std::string_view, TokenKind> words[] = {
        {"mission", TokenKind::Mission}, {"loot", TokenKind::Loot},  {"grind", TokenKind::Grind},
        {"sus", TokenKind::Sus},         {"nah", TokenKind::Nah},    {"yap", TokenKind::Yap},
        {"gg", TokenKind::Gg},           {"int", TokenKind::Int},    {"bool", TokenKind::Bool},
        {"true", TokenKind::True},       {"false", TokenKind::False}};
    for (const auto &[name, kind] : words) {
        if (name == word)
            return kind;
    }
    return TokenKind::Identifier;
}
} // namespace

std::vector<Token> tokenize(const Source &source) {
    if (source.text.size() > max_source_bytes) {
        throw CompileError("E003", {}, "source exceeds the 64 KiB limit");
    }
    std::vector<Token> tokens;
    std::size_t offset = 0;
    std::uint32_t line = 1, column = 1;
    const auto advance = [&]() {
        const char c = source.text[offset++];
        if (c == '\n') {
            ++line;
            column = 1;
        } else {
            ++column;
        }
        return c;
    };
    const auto peek = [&](std::size_t ahead = 0) {
        return offset + ahead < source.text.size() ? source.text[offset + ahead] : '\0';
    };
    while (offset < source.text.size()) {
        const char c = peek();
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance();
            continue;
        }
        if (c == '/' && peek(1) == '/') {
            while (offset < source.text.size() && peek() != '\n')
                advance();
            continue;
        }
        Span span{offset, 1, line, column};
        TokenKind kind = TokenKind::End;
        if (alpha(c)) {
            do {
                advance();
            } while (alpha(peek()) || digit(peek()));
            span.length = offset - span.offset;
            kind = keyword(std::string_view(source.text).substr(span.offset, span.length));
        } else if (digit(c)) {
            do {
                advance();
            } while (digit(peek()));
            span.length = offset - span.offset;
            kind = TokenKind::Integer;
        } else {
            advance();
            const auto second = [&](char next, TokenKind two, TokenKind one) {
                if (peek() == next) {
                    advance();
                    span.length = 2;
                    return two;
                }
                return one;
            };
            switch (c) {
            case '(':
                kind = TokenKind::LeftParen;
                break;
            case ')':
                kind = TokenKind::RightParen;
                break;
            case '{':
                kind = TokenKind::LeftBrace;
                break;
            case '}':
                kind = TokenKind::RightBrace;
                break;
            case ':':
                kind = TokenKind::Colon;
                break;
            case ';':
                kind = TokenKind::Semicolon;
                break;
            case '+':
                kind = TokenKind::Plus;
                break;
            case '-':
                kind = second('>', TokenKind::Arrow, TokenKind::Minus);
                break;
            case '*':
                kind = TokenKind::Star;
                break;
            case '/':
                kind = TokenKind::Slash;
                break;
            case '=':
                kind = second('=', TokenKind::Equal, TokenKind::Assign);
                break;
            case '!':
                kind = second('=', TokenKind::NotEqual, TokenKind::Bang);
                break;
            case '<':
                kind = second('=', TokenKind::LessEqual, TokenKind::Less);
                break;
            case '>':
                kind = second('=', TokenKind::GreaterEqual, TokenKind::Greater);
                break;
            case '&':
                if (peek() == '&') {
                    advance();
                    span.length = 2;
                    kind = TokenKind::And;
                }
                break;
            case '|':
                if (peek() == '|') {
                    advance();
                    span.length = 2;
                    kind = TokenKind::Or;
                }
                break;
            default:
                break;
            }
            if (kind == TokenKind::End) {
                throw CompileError("E001", span, "unexpected character or unsupported operator",
                                   "Skill issue: that keybind does nothing here.");
            }
        }
        tokens.push_back(
            {kind, std::string_view(source.text).substr(span.offset, span.length), span});
    }
    tokens.push_back({TokenKind::End, {}, {offset, 0, line, column}});
    return tokens;
}

std::string dump_tokens(const std::vector<Token> &tokens) {
    std::ostringstream out;
    for (const auto &token : tokens) {
        out << token.span.line << ':' << token.span.column << ' ' << token_name(token.kind);
        if (!token.text.empty())
            out << " '" << token.text << '\'';
        out << '\n';
    }
    return out.str();
}

std::string format_diagnostic(const Source &source, const CompileError &error, bool humor) {
    std::ostringstream out;
    out << source.name << ':' << error.span.line << ':' << error.span.column << ": error "
        << error.code << ": " << error.what() << '\n';
    const auto at = std::min(error.span.offset, source.text.size());
    const auto newline = at == 0 ? std::string::npos : source.text.rfind('\n', at - 1);
    const auto line_start = newline == std::string::npos ? 0 : newline + 1;
    auto line_end = source.text.find('\n', at);
    if (line_end == std::string::npos)
        line_end = source.text.size();
    if (line_end > line_start && source.text[line_end - 1] == '\r')
        --line_end;
    const auto window_start = at > line_start + 80 ? at - 80 : line_start;
    const auto window_end = std::min(line_end, window_start + 180);
    const auto prefix = window_start > line_start ? "..." : "";
    out << "  " << prefix << source.text.substr(window_start, window_end - window_start) << '\n';
    out << "  " << std::string(std::string_view(prefix).size(), ' ');
    for (auto i = window_start; i < at; ++i)
        out << (source.text[i] == '\t' ? '\t' : ' ');
    out << std::string(std::max<std::size_t>(
                           1, std::min({error.span.length, window_end - at, std::size_t{32}})),
                       '^')
        << '\n';
    if (humor && !error.joke.empty())
        out << error.joke << '\n';
    return out.str();
}

} // namespace skillissue
