#include "skillissue/ast.hpp"

#include <functional>
#include <iostream>
#include <limits>
#include <string>
#include <utility>

namespace {
template <typename T>
concept CanTokenize = requires(T &&source) { skillissue::tokenize(std::forward<T>(source)); };
static_assert(CanTokenize<skillissue::Source &>);
static_assert(CanTokenize<const skillissue::Source &>);
static_assert(!CanTokenize<skillissue::Source>);
static_assert(!CanTokenize<const skillissue::Source>);

int passed = 0, failed = 0;
void require(bool condition, const std::string &message) {
    if (!condition)
        throw std::runtime_error(message);
}
void test(const std::string &name, const std::function<void()> &body) {
    try {
        body();
        ++passed;
    } catch (const std::exception &error) {
        ++failed;
        std::cerr << "FAIL " << name << ": " << error.what() << '\n';
    }
}
std::string program(const std::string &body) {
    return "mission main() -> int { " + body + " gg 0; }";
}
skillissue::Compilation compile_body(const std::string &body, bool folding = true,
                                     bool ast = false) {
    return skillissue::compile({"unit.skill", program(body)}, {folding, false, ast});
}
void rejects(const std::string &source, const std::string &code) {
    try {
        static_cast<void>(skillissue::compile({"bad.skill", source}));
    } catch (const skillissue::CompileError &error) {
        require(error.code == code, "expected " + code + ", received " + error.code);
        return;
    }
    throw std::runtime_error("invalid source was accepted");
}
} // namespace

int main() {
    using namespace skillissue;
    test("tokens and CRLF comments", [] {
        Source source{"tokens.skill", "// comment\r\nloot x: int = 2;\n"};
        const auto tokens = tokenize(source);
        require(tokens.front().kind == TokenKind::Loot, "keyword not recognized");
        require(tokens.front().span.line == 2 && tokens.front().span.column == 1,
                "incorrect CRLF location");
        require(tokens[1].text == "x", "identifier lexeme changed");
        require(tokens.back().kind == TokenKind::End && tokens.back().span.line == 3,
                "EOF location");
    });
    test("multi-character tokens", [] {
        Source source{"tokens.skill", "-> == != <= >= && ||"};
        const auto tokens = tokenize(source);
        const TokenKind expected[] = {
            TokenKind::Arrow,        TokenKind::Equal, TokenKind::NotEqual, TokenKind::LessEqual,
            TokenKind::GreaterEqual, TokenKind::And,   TokenKind::Or};
        require(tokens.size() == 8, "wrong token count");
        for (std::size_t i = 0; i < 7; ++i)
            require(tokens[i].kind == expected[i], "wrong operator token");
    });
    test("precedence AST", [] {
        const auto result = compile_body("yap(1 + 2 * 3);", false, true);
        require(result.ast.find("(+ 1 (* 2 3))") != std::string::npos, result.ast);
    });
    test("left associativity", [] {
        const auto result = compile_body("yap(10 - 3 - 2);", false, true);
        require(result.ast.find("(- (- 10 3) 2)") != std::string::npos, result.ast);
    });
    test("constant folding", [] {
        const auto folded = compile_body("yap(1 + 2 * 3);");
        const auto plain = compile_body("yap(1 + 2 * 3);", false);
        require(folded.ir.find("@skill_yap_int(i64 7)") != std::string::npos, "wrong folded value");
        require(plain.ir.find("mul i64 2, 3") != std::string::npos &&
                    plain.ir.find("add i64") != std::string::npos,
                "unfolded operations missing");
        require(compile_body("yap((1 + 2) * 3);").ir.find("@skill_yap_int(i64 9)") !=
                    std::string::npos,
                "parentheses ignored");
    });
    test("signed wrap folding", [] {
        require(
            compile_body("yap(9223372036854775807 + 1);").ir.find("i64 -9223372036854775808)") !=
                std::string::npos,
            "addition did not wrap");
        require(
            compile_body("yap(-(-9223372036854775808));").ir.find("i64 -9223372036854775808)") !=
                std::string::npos,
            "negation did not wrap");
        require(
            compile_body("yap(-9223372036854775808 - 1);").ir.find("i64 9223372036854775807)") !=
                std::string::npos,
            "subtraction did not wrap");
        require(compile_body("yap(9223372036854775807 * 2);").ir.find("i64 -2)") !=
                    std::string::npos,
                "multiplication did not wrap");
    });
    test("fold preserves division faults", [] {
        require(compile_body("yap(1 / 0);").ir.find("call i64 @skill_div") != std::string::npos,
                "division fault erased");
        require(compile_body("yap(-9223372036854775808 / -1);").ir.find("call i64 @skill_div") !=
                    std::string::npos,
                "overflow fault erased");
    });
    test("source escapes in IR", [] {
        const auto result = compile({"C:\\a\"b.skill", program("yap(1 / 0);")});
        require(result.ir.find("C:\\5Ca\\22b.skill") != std::string::npos,
                "source path not escaped");
    });
    test("short-circuit CFG", [] {
        const auto ir = compile_body("loot a: bool = true; yap(a && (false || a));", false).ir;
        require(ir.find("phi i1") != std::string::npos && ir.find("logic.rhs") != std::string::npos,
                "missing short-circuit control flow");
    });
    test("scopes allow sibling names", [] {
        static_cast<void>(
            compile_body("{ loot x: int = 1; yap(x); } { loot x: bool = true; yap(x); }"));
    });
    test("declaration initializer uses old scope",
         [] { rejects(program("loot x: int = x;"), "E010"); });
    test("unknown variable", [] { rejects(program("yap(score);"), "E010"); });
    test("unknown assignment", [] { rejects(program("score = 10;"), "E010"); });
    test("expired scope", [] { rejects(program("{ loot x: int = 1; } yap(x);"), "E010"); });
    test("duplicate declaration",
         [] { rejects(program("loot x: int = 1; loot x: int = 2;"), "E011"); });
    test("shadowing rejected",
         [] { rejects(program("loot x: int = 1; { loot x: int = 2; }"), "E011"); });
    test("initializer type", [] { rejects(program("loot x: bool = 1;"), "E012"); });
    test("assignment type", [] { rejects(program("loot x: int = 1; x = false;"), "E012"); });
    test("condition type", [] { rejects(program("sus (1) { yap(0); }"), "E012"); });
    test("loop condition type", [] { rejects(program("grind (1) {}"), "E012"); });
    test("arithmetic type", [] { rejects(program("yap(true + 1);"), "E012"); });
    test("equality type", [] { rejects(program("yap(1 == false);"), "E012"); });
    test("logical type", [] { rejects(program("yap(true && 1);"), "E012"); });
    test("unreachable code still type checked",
         [] { rejects(program("sus (false) { yap(missing); }"), "E010"); });
    test("return type", [] { rejects("mission main() -> int { gg true; }", "E012"); });
    test("missing gg", [] { rejects("mission main() -> int { yap(42); }", "E013"); });
    test("early gg", [] { rejects(program("gg 0;"), "E013"); });
    test("nested gg", [] { rejects(program("sus (true) { gg 0; }"), "E013"); });
    test("entry point", [] { rejects("mission other() -> int { gg 0; }", "E004"); });
    test("missing semicolon", [] { rejects(program("yap(42)"), "E004"); });
    test("unexpected character", [] { rejects(program("yap(@);"), "E001"); });
    test("single ampersand", [] { rejects(program("yap(true & false);"), "E001"); });
    test("positive integer limit", [] { rejects(program("yap(9223372036854775808);"), "E002"); });
    test("negative integer limit", [] { rejects(program("yap(-9223372036854775809);"), "E002"); });
    test("uint64 overflow literal",
         [] { rejects(program("yap(999999999999999999999999);"), "E002"); });
    test("minimum signed literal accepted",
         [] { static_cast<void>(compile_body("yap(-9223372036854775808);")); });
    test("source limit", [] { rejects(std::string(max_source_bytes + 1, ' '), "E003"); });
    test("exact source limit accepted", [] {
        Source source{"limit.skill", program("yap(42);")};
        source.text.resize(max_source_bytes, ' ');
        require(compile(source).ir.find("@skill_yap_int(i64 42)") != std::string::npos,
                "a source at the documented limit was rejected");
    });
    test("empty input diagnostic", [] { rejects("", "E004"); });
    test("unterminated block diagnostic",
         [] { rejects("mission main() -> int { yap(1);", "E004"); });
    test("embedded null outside comments rejected", [] {
        auto text = program("yap(42);");
        text.insert(text.find("yap"), 1, '\0');
        rejects(text, "E001");
    });
    test("parenthesis depth", [] {
        rejects(program("yap(" + std::string(140, '(') + "1" + std::string(140, ')') + ");"),
                "E003");
    });
    test("unary depth", [] { rejects(program("yap(" + std::string(140, '-') + "1);"), "E003"); });
    test("block depth",
         [] { rejects(program(std::string(140, '{') + std::string(140, '}')), "E003"); });
    test("flat expression depth", [] {
        std::string expr = "1";
        for (int i = 0; i < 140; ++i)
            expr += "+1";
        rejects(program("yap(" + expr + ");"), "E003");
    });
    test("diagnostic source location", [] {
        Source source{"broken.skill", "mission main() -> int {\n  yap(score);\n  gg 0;\n}"};
        try {
            static_cast<void>(compile(source));
        } catch (const CompileError &error) {
            require(error.span.line == 2 && error.span.column == 7, "wrong variable location");
            const auto funny = format_diagnostic(source, error);
            require(funny.find("broken.skill:2:7: error E010") != std::string::npos &&
                        funny.find("^^^^^") != std::string::npos,
                    funny);
            require(format_diagnostic(source, error, false).find("Skill issue:") ==
                        std::string::npos,
                    "serious diagnostic contains joke");
            return;
        }
        throw std::runtime_error("missing diagnostic");
    });
    test("trailing junk", [] { rejects(program("") + "loot x: int = 1;", "E004"); });
    test("all keyword spellings", [] {
        Source source{"keywords", "mission loot grind sus nah yap gg int bool true false"};
        const auto tokens = tokenize(source);
        require(tokens.size() == 12, "keyword count");
        for (const auto &token : tokens)
            require(token.kind != TokenKind::Identifier, "keyword is identifier");
    });
    std::cout << passed << " unit cases passed, " << failed << " failed\n";
    return failed == 0 ? 0 : 1;
}
