#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace teknegram {

class LexiconReader {
    public:
        LexiconReader();
        explicit LexiconReader(const std::string& path);

        void open(const std::string& path);
        bool is_open() const;
        std::size_t size() const;
        const std::string& at(std::size_t index) const;

    private:
        std::vector<std::string> values_;
};

} // namespace teknegram
