#ifndef LOGAI_SIMD_SCANNER_H
#define LOGAI_SIMD_SCANNER_H

#include <string_view>
#include <cstddef>
#include <string>
#include <vector>

// Include SIMD-specific headers
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE4_2__)
#include <nmmintrin.h>
#elif defined(USE_NEON_SIMD)
#include <arm_neon.h>
#endif

namespace logai {

/**
 * @brief SIMD-optimized scanner for log data.
 * 
 * This class provides SIMD-optimized methods for finding characters and patterns in log data.
 * It automatically selects the best available SIMD instruction set at compile time (AVX2, SSE4.2, NEON).
 */
class SimdLogScanner {
public:
    /**
     * @brief Find the first occurrence of a character in a string.
     * 
     * @param data Pointer to the string data
     * @param len Length of the string
     * @param target Character to find
     * @return size_t Position of the first occurrence, or std::string::npos if not found
     */
    static size_t findChar(const char* data, size_t len, char target);

    /**
     * @brief Find the first occurrence of a substring in a string.
     * 
     * @param haystack Pointer to the string to search in
     * @param haystack_len Length of the haystack string
     * @param needle Pointer to the substring to find
     * @param needle_len Length of the needle substring
     * @return size_t Position of the first occurrence, or std::string::npos if not found
     */
    static size_t findSubstring(const char* haystack, size_t haystack_len, const char* needle, size_t needle_len);

    /**
     * @brief Find the last occurrence of a character in a string.
     * 
     * @param data Pointer to the string data
     * @param len Length of the string
     * @param target Character to find
     * @return size_t Position of the last occurrence, or std::string::npos if not found
     */
    static size_t findLast(const char* data, size_t len, char target);

    /**
     * @brief Count occurrences of a character in a string.
     * 
     * @param data Pointer to the string data
     * @param len Length of the string
     * @param target Character to count
     * @return size_t Number of occurrences
     */
    static size_t countChar(const char* data, size_t len, char target);

    /**
     * @brief Find all occurrences of a character in a string.
     * 
     * @param data Pointer to the string data
     * @param len Length of the string
     * @param target Character to find
     * @return std::vector<size_t> Vector of positions where the character was found
     */
    static std::vector<size_t> findAllChar(const char* data, size_t len, char target);

    /**
     * @brief Find a character in a std::string_view.
     * 
     * @param str String view to search in
     * @param target Character to find
     * @return size_t Position of the first occurrence, or std::string::npos if not found
     */
    static size_t findChar(std::string_view str, char target) {
        return findChar(str.data(), str.size(), target);
    }

    /**
     * @brief Find a substring in a std::string_view.
     * 
     * @param haystack String view to search in
     * @param needle String view to find
     * @return size_t Position of the first occurrence, or std::string::npos if not found
     */
    static size_t findSubstring(std::string_view haystack, std::string_view needle) {
        return findSubstring(haystack.data(), haystack.size(), needle.data(), needle.size());
    }

    /**
     * @brief Find the last occurrence of a character in a std::string_view.
     * 
     * @param str String view to search in
     * @param target Character to find
     * @return size_t Position of the last occurrence, or std::string::npos if not found
     */
    static size_t findLast(std::string_view str, char target) {
        return findLast(str.data(), str.size(), target);
    }

    /**
     * @brief Count occurrences of a character in a std::string_view.
     * 
     * @param str String view to search in
     * @param target Character to count
     * @return size_t Number of occurrences
     */
    static size_t countChar(std::string_view str, char target) {
        return countChar(str.data(), str.size(), target);
    }

    /**
     * @brief Find all occurrences of a character in a std::string_view.
     * 
     * @param str String view to search in
     * @param target Character to find
     * @return std::vector<size_t> Vector of positions where the character was found
     */
    static std::vector<size_t> findAllChar(std::string_view str, char target) {
        return findAllChar(str.data(), str.size(), target);
    }

    SimdLogScanner(const char* data, size_t length);
    
    size_t findChar(char c) const;
    size_t findNewline() const;
    void advance(size_t offset);
    size_t position() const;
    size_t length() const;
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
#ifdef USE_NEON_SIMD
    size_t findCharNEON(const char* start, size_t len, char c) const;
#endif

    const char* data_;
    size_t length_;
    size_t position_;
}; 

} // namespace logai 

#endif // LOGAI_SIMD_SCANNER_H 