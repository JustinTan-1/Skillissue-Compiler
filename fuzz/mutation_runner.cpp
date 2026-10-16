#include "skillissue/compiler.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t *data, std::size_t size);

namespace {
constexpr std::size_t input_limit = skillissue::max_source_bytes + 1;

std::uint32_t positive_number(const char *text) {
    const std::string value = text;
    std::uint32_t number = 0;
    const auto [end, error] = std::from_chars(value.data(), value.data() + value.size(), number);
    if (error != std::errc{} || end != value.data() + value.size() || number == 0)
        throw std::invalid_argument("runs, seconds, and seed must be positive integers");
    return number;
}

std::string read_seed(const std::filesystem::path &path) {
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot read seed '" + path.string() + "'");
    std::string text(input_limit, '\0');
    input.read(text.data(), static_cast<std::streamsize>(text.size()));
    text.resize(static_cast<std::size_t>(input.gcount()));
    if (input.bad())
        throw std::runtime_error("failed to read seed");
    return text;
}

void exercise(const std::string &text, const std::filesystem::path &reproducer) {
    // Save the input before compiling so a sanitizer crash leaves a reproducer.
    std::ofstream output(reproducer, std::ios::binary | std::ios::trunc);
    output.write(text.data(), static_cast<std::streamsize>(text.size()));
    output.close();
    if (!output)
        throw std::runtime_error("cannot save the current fuzz input");
    static_cast<void>(
        LLVMFuzzerTestOneInput(reinterpret_cast<const std::uint8_t *>(text.data()), text.size()));
}

std::string mutate(std::string text, std::mt19937 &random,
                   const std::vector<std::string> &dictionary) {
    const auto edits = 1 + random() % 6;
    for (std::uint32_t edit = 0; edit < edits; ++edit) {
        const auto position = static_cast<std::size_t>(random()) % (text.size() + 1);
        switch (random() % 6) {
        case 0:
            if (position < text.size())
                text[position] = static_cast<char>(random() & 255);
            else
                text.push_back(static_cast<char>(random() & 255));
            break;
        case 1:
            text.insert(position, dictionary[random() % dictionary.size()]);
            break;
        case 2:
            text.erase(position, 1 + random() % 32);
            break;
        case 3: {
            const auto fragment = text.substr(position, 1 + random() % 32);
            text.insert(static_cast<std::size_t>(random()) % (text.size() + 1), fragment);
            break;
        }
        case 4:
            text.replace(position, random() % 16, dictionary[random() % dictionary.size()]);
            break;
        case 5:
            text.insert(position, 1 + random() % 8, static_cast<char>(random() & 255));
            break;
        }
        if (text.size() > input_limit)
            text.resize(input_limit);
    }
    return text;
}
} // namespace

int main(int argc, char **argv) {
    try {
        if (argc != 7)
            throw std::invalid_argument(
                "usage: mutation_fuzz <corpus> <artifacts> <dictionary> <runs> <seconds> <seed>");
        const std::filesystem::path corpus = argv[1], artifacts = argv[2];
        const auto runs = positive_number(argv[4]);
        const auto seconds = positive_number(argv[5]);
        const auto seed = positive_number(argv[6]);
        std::filesystem::create_directories(artifacts);
        std::vector<std::filesystem::path> files;
        for (const auto &entry : std::filesystem::directory_iterator(corpus)) {
            if (entry.is_regular_file())
                files.push_back(entry.path());
        }
        std::sort(files.begin(), files.end());
        if (files.empty())
            throw std::invalid_argument("the corpus must contain at least one seed");
        std::vector<std::string> seeds;
        for (const auto &file : files)
            seeds.push_back(read_seed(file));
        std::ifstream dictionary_file(argv[3]);
        if (!dictionary_file)
            throw std::runtime_error("cannot read the mutation dictionary");
        std::vector<std::string> dictionary;
        for (std::string line; std::getline(dictionary_file, line);) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.size() >= 2 && line.front() == '"' && line.back() == '"')
                dictionary.push_back(line.substr(1, line.size() - 2));
        }
        if (dictionary.empty())
            throw std::invalid_argument("dictionary requires simple quoted entries");
        const auto reproducer = artifacts / "current-input.skill";
        const auto start = std::chrono::steady_clock::now();
        for (const auto &text : seeds)
            exercise(text, reproducer);
        std::cout << "Validated " << seeds.size() << " seeds; mutation seed " << seed << '\n';
        std::mt19937 random(seed);
        std::uint32_t executed = 0;
        while (executed < runs &&
               std::chrono::steady_clock::now() - start < std::chrono::seconds(seconds)) {
            auto text = mutate(seeds[random() % seeds.size()], random, dictionary);
            exercise(text, reproducer);
            ++executed;
        }
        if (executed == 0)
            throw std::runtime_error("time budget ended before any mutations were tested");
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - start)
                                 .count();
        std::cout << "Passed " << executed << " mutated inputs (folding off/on), " << elapsed
                  << " ms; seed " << seed << '\n';
        if (executed < runs)
            std::cout << "Stopped at the time limit before the requested run count.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "mutation_fuzz: " << error.what() << '\n';
        return 1;
    }
}
