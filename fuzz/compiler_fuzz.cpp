#include "skillissue/compiler.hpp"

#include <cstddef>
#include <cstdint>
#include <string>

// This target compiles source in memory; it never executes generated programs.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size) {
    skillissue::Source source{"fuzz.skill", {}};
    if (size != 0)
        source.text.assign(reinterpret_cast<const char *>(data), size);
    for (const bool folding : {false, true}) {
        try {
            static_cast<void>(skillissue::compile(source, {folding, true, true}));
        } catch (const skillissue::CompileError &error) {
            // Invalid source is expected. Internal exceptions and sanitizer findings fail the run.
            static_cast<void>(skillissue::format_diagnostic(source, error));
        }
    }
    return 0;
}
