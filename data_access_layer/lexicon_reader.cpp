#include "lexicon_reader.hpp"

#include <fstream>
#include <stdexcept>

namespace teknegram {

namespace {

std::string ReadString(std::ifstream* input, std::uint32_t byte_count) {
    std::string value(byte_count, '\0');
    if (byte_count > 0U) {
        input->read(&value[0], static_cast<std::streamsize>(byte_count));
    }
    if (!(*input)) {
        throw std::runtime_error("Failed to read lexicon string data");
    }
    return value;
}

} // namespace

LexiconReader::LexiconReader() {}

LexiconReader::LexiconReader(const std::string& path) {
    open(path);
}

void LexiconReader::open(const std::string& path) {
    values_.clear();

    std::ifstream input(path.c_str(), std::ios::binary);
    if (!input) {
        throw std::runtime_error("Failed to open lexicon file: " + path);
    }

    while (input.peek() != std::ifstream::traits_type::eof()) {
        std::uint32_t byte_count = 0;
        input.read(reinterpret_cast<char*>(&byte_count), sizeof(byte_count));
        if (!input) {
            throw std::runtime_error("Failed to read lexicon entry length from: " + path);
        }
        values_.push_back(ReadString(&input, byte_count));
    }
}

bool LexiconReader::is_open() const {
    return !values_.empty();
}

std::size_t LexiconReader::size() const {
    return values_.size();
}

const std::string& LexiconReader::at(std::size_t index) const {
    if (index >= values_.size()) {
        throw std::runtime_error("Lexicon index out of range");
    }
    return values_[index];
}

} // namespace teknegram
