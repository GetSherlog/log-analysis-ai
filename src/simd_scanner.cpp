#include "simd_scanner.h"
#include <string>

#if defined(__SSE4_2__)
#include <nmmintrin.h>
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#endif

namespace logai {

SimdLogScanner::SimdLogScanner(const char* data, size_t length)
    : data_(data), length_(length), position_(0) {}

size_t SimdLogScanner::findChar(char c) const {
    if (position_ >= length_) return std::string::npos;
    
    const char* start = data_ + position_;
    size_t remaining = length_ - position_;
    
#if defined(__AVX2__)
    return findCharAVX2(start, remaining, c);
#elif defined(__SSE4_2__)
    return findCharSSE42(start, remaining, c);
#else
    return findCharScalar(start, remaining, c);
#endif
}

size_t SimdLogScanner::findNewline() const {
    size_t pos = findChar('\n');
    if (pos == std::string::npos) {
        pos = findChar('\r');
    }
    return pos;
}

void SimdLogScanner::advance(size_t offset) {
    position_ += offset;
    if (position_ > length_) {
        position_ = length_;
    }
}

size_t SimdLogScanner::position() const {
    return position_;
}

size_t SimdLogScanner::length() const {
    return length_;
}

std::string_view SimdLogScanner::getSubstring(size_t length) const {
    if (position_ + length > length_) {
        length = length_ - position_;
    }
    return std::string_view(data_ + position_, length);
}

std::string_view SimdLogScanner::getSubstringTo(char delimiter) const {
    size_t pos = findChar(delimiter);
    if (pos == std::string::npos) {
        return std::string_view(data_ + position_, length_ - position_);
    }
    return std::string_view(data_ + position_, pos - position_);
}

bool SimdLogScanner::atEnd() const {
    return position_ >= length_;
}

bool SimdLogScanner::eof() const {
    return atEnd();
}

size_t SimdLogScanner::findCharScalar(const char* start, size_t len, char c) const {
    for (size_t i = 0; i < len; ++i) {
        if (start[i] == c) {
            return i;
        }
    }
    return std::string::npos;
}

#if defined(__SSE4_2__)
size_t SimdLogScanner::findCharSSE42(const char* start, size_t len, char c) const {
    const __m128i search_char = _mm_set1_epi8(c);
    
    size_t pos = 0;
    while (pos + 16 <= len) {
        __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(start + pos));
        int mask = _mm_movemask_epi8(_mm_cmpeq_epi8(chunk, search_char));
        
        if (mask != 0) {
            unsigned int bit_pos = __builtin_ctz(mask);
            return pos + bit_pos;
        }
        
        pos += 16;
    }
    
    for (size_t i = pos; i < len; ++i) {
        if (start[i] == c) {
            return i;
        }
    }
    
    return std::string::npos;
}
#endif

#if defined(__AVX2__)
size_t SimdLogScanner::findCharAVX2(const char* start, size_t len, char c) const {
    const __m256i search_char = _mm256_set1_epi8(c);
    
    size_t pos = 0;
    while (pos + 32 <= len) {
        __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(start + pos));
        __m256i cmp_result = _mm256_cmpeq_epi8(chunk, search_char);
        int mask = _mm256_movemask_epi8(cmp_result);
        
        if (mask != 0) {
            unsigned int bit_pos = __builtin_ctz(mask);
            return pos + bit_pos;
        }
        
        pos += 32;
    }
    
#if defined(__SSE4_2__)
    if (pos + 16 <= len) {
        size_t sse_result = findCharSSE42(start + pos, len - pos, c);
        if (sse_result != std::string::npos) {
            return pos + sse_result;
        }
        return std::string::npos;
    }
#endif
    
    for (size_t i = pos; i < len; ++i) {
        if (start[i] == c) {
            return i;
        }
    }
    
    return std::string::npos;
}
#endif

} // namespace logai 