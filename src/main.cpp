#include "skillissue/compiler.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
void help() {
    std::cout << "SkillIssue compiler 0.1.0\n"
              << "Usage: skillc <file.skill> [-o output.ll] [options]\n\n"
              << "  --no-fold       Disable constant folding\n"
              << "  --dump-tokens   Print tokens and source locations\n"
              << "  --dump-ast      Print the typed AST before folding\n"
              << "  --serious       Omit compiler diagnostic jokes\n"
              << "  --version       Print the compiler version\n"
              << "  --help          Show this help\n\n"
              << "Use -o - to write LLVM IR to standard output.\n"
              << "Link the IR with runtime/skill_runtime.cpp using clang++.\n";
}
} // namespace

int main(int argc, char **argv) {
    skillissue::Source source{"<command line>", {}};
    bool humor = true;
    try {
        std::filesystem::path input, output;
        skillissue::CompilerOptions options;
        for (int i = 1; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--help" || arg == "-h") {
                help();
                return 0;
            }
            if (arg == "--version") {
                std::cout << "skillc 0.1.0\n";
                return 0;
            }
            if (arg == "--no-fold")
                options.fold_constants = false;
            else if (arg == "--dump-tokens")
                options.capture_tokens = true;
            else if (arg == "--dump-ast")
                options.capture_ast = true;
            else if (arg == "--serious")
                humor = false;
            else if (arg == "-o") {
                if (++i >= argc || !output.empty())
                    throw std::invalid_argument("-o requires one output path");
                output = argv[i];
            } else if (!arg.empty() && arg[0] == '-') {
                throw std::invalid_argument("unknown option '" + arg + "'");
            } else {
                if (!input.empty())
                    throw std::invalid_argument("only one source file is supported");
                input = arg;
            }
        }
        if (input.empty())
            throw std::invalid_argument("provide a .skill source file; use --help for usage");
        if (output.empty()) {
            output = input;
            output.replace_extension(".ll");
        }
        if (output == "-" && (options.capture_tokens || options.capture_ast)) {
            throw std::invalid_argument("IR on stdout cannot be combined with inspection dumps");
        }
        if (output != "-" && std::filesystem::exists(input) && std::filesystem::exists(output) &&
            std::filesystem::equivalent(input, output)) {
            throw std::invalid_argument("the output path must not overwrite the source file");
        }
        source.name = input.generic_string();
        std::ifstream in(input, std::ios::binary);
        if (!in)
            throw std::runtime_error("cannot read source file '" + source.name + "'");
        source.text.resize(skillissue::max_source_bytes + 1);
        in.read(source.text.data(), static_cast<std::streamsize>(source.text.size()));
        source.text.resize(static_cast<std::size_t>(in.gcount()));
        if (in.bad())
            throw std::runtime_error("failed while reading source file");
        const auto result = skillissue::compile(source, options);
        if (output == "-")
            std::cout << result.ir;
        else {
            std::ofstream out(output, std::ios::binary | std::ios::trunc);
            if (!out)
                throw std::runtime_error("cannot open output file '" + output.generic_string() +
                                         "'");
            out << result.ir;
            out.close();
            if (!out)
                throw std::runtime_error("failed while writing LLVM IR");
        }
        if (options.capture_tokens)
            std::cout << result.tokens;
        if (options.capture_ast)
            std::cout << result.ast;
        if (!std::cout)
            throw std::runtime_error("failed while writing standard output");
        return 0;
    } catch (const skillissue::CompileError &error) {
        std::cerr << skillissue::format_diagnostic(source, error, humor);
    } catch (const std::exception &error) {
        std::cerr << "skillc: error: " << error.what() << '\n';
    }
    return 1;
}
