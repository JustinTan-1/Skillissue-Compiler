#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <limits>

namespace {
[[noreturn]] void fail(const char *code, const char *message, const char *joke, const char *file,
                       std::int32_t line, std::int32_t column) {
    std::fprintf(stderr, "%s:%" PRId32 ":%" PRId32 ": runtime error %s: %s\n%s\n", file, line,
                 column, code, message, joke);
    std::fflush(stdout);
    std::exit(2);
}
} // namespace

extern "C" void skill_yap_int(std::int64_t value) {
    std::printf("%" PRId64 "\n", value);
}

extern "C" void skill_yap_bool(std::int32_t value) {
    std::puts(value ? "true" : "false");
}

extern "C" std::int64_t skill_div(std::int64_t left, std::int64_t right, const char *file,
                                  std::int32_t line, std::int32_t column) {
    if (right == 0)
        fail("R001", "division by zero", "Skill issue: zero cannot carry this division.", file,
             line, column);
    if (left == std::numeric_limits<std::int64_t>::min() && right == -1) {
        fail("R002", "signed division overflow",
             "Skill issue: that quotient exceeds the inventory limit.", file, line, column);
    }
    return left / right;
}

extern "C" std::int32_t skill_exit(std::int64_t value, const char *file, std::int32_t line,
                                   std::int32_t column) {
    if (value < 0 || value > 255) {
        fail("R003", "exit status must be between 0 and 255",
             "Skill issue: gg needs a valid scoreboard result.", file, line, column);
    }
    return static_cast<std::int32_t>(value);
}
