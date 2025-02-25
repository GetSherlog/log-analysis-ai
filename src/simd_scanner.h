#pragma once

#include <string_view>
#include <cstddef>

namespace logai {

class SimdLogScanner {
public:
    SimdLogScanner(const char* data, size_t length);
    
    size_t findChar(char c) const;
    size_t findNewline() const;
    void advance(size_t offset);
    size_t position() const;
    std::string_view getSubstring(size_t length) const;
    std::string_view getSubstringTo(char delimiter) const;
    bool atEnd() const;
    bool eof() const;

private:
    size_t findCharScalar(const char* start, size_t len, char c) const;
#ifdef __SSE4_2__
    size_t findCharSSE42(const char* start, size_t len, char c) const;
#endif
#ifdef __AVX2__
    size_t findCharAVX2(const char* start, size_t len, char c) const;
#endif

    const char* data_;
    size_t length_;
    size_t position_;
}; 

} // namespace logai 