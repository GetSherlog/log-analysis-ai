#ifndef LOGAI_SIMD_STRING_OPS_H
#define LOGAI_SIMD_STRING_OPS_H

#include <string>
#include <string_view>
#include <vector>
#include <algorithm>
#include <cstring>

// Include appropriate SIMD headers
#if defined(__AVX2__)
#include <immintrin.h>
#elif defined(__SSE4_2__)
#include <nmmintrin.h>
#elif defined(USE_NEON_SIMD)
#include <arm_neon.h>
#endif

namespace logai {

/**
 * @brief SIMD-optimized string operations.
 * 
 * This class provides SIMD-optimized methods for common string operations used in log processing.
 * It automatically selects the best available SIMD instruction set at compile time (AVX2, SSE4.2, NEON).
 */
class SimdStringOps {
public:
    /**
     * @brief Replace all occurrences of a character in a string with another character.
     * 
     * Uses SIMD instructions for better performance when available.
     * 
     * @param input Input string
     * @param delimiter Character to replace
     * @param replacement Replacement character
     * @return std::string New string with replaced characters
     */
    static std::string replace_char(std::string_view input, char delimiter, char replacement);

    /**
     * @brief Replace all occurrences of multiple characters with a single character.
     * 
     * Uses SIMD instructions for better performance when available.
     * 
     * @param input Input string
     * @param delimiters Vector of characters to replace
     * @param replacement Replacement character
     * @return std::string New string with replaced characters
     */
    static std::string replace_chars(std::string_view input, const std::vector<char>& delimiters, char replacement);

    /**
     * @brief Trim whitespace from the beginning and end of a string.
     * 
     * Uses SIMD instructions for scanning when available.
     * 
     * @param input Input string
     * @return std::string Trimmed string
     */
    static std::string trim(std::string_view input);

    /**
     * @brief Check if a string contains a substring.
     * 
     * Uses SIMD instructions for better performance when available.
     * 
     * @param haystack String to search in
     * @param needle Substring to search for
     * @return bool True if haystack contains needle
     */
    static bool contains(std::string_view haystack, std::string_view needle);

    /**
     * @brief Convert a string to lowercase.
     * 
     * Uses SIMD instructions for better performance when available.
     * 
     * @param input Input string
     * @return std::string Lowercase string
     */
    static std::string to_lower(std::string_view input);

    /**
     * @brief Split a string by a delimiter character.
     * 
     * Uses SIMD instructions to efficiently find delimiters.
     * 
     * @param input Input string
     * @param delimiter Character to split by
     * @return std::vector<std::string_view> Vector of string views for each segment
     */
    static std::vector<std::string_view> split(std::string_view input, char delimiter);

    // Scalar fallbacks for platforms without SIMD support
    static std::string replace_char_scalar(std::string_view input, char delimiter, char replacement);
    static std::string replace_chars_scalar(std::string_view input, const std::vector<char>& delimiters, char replacement);
    static std::string trim_scalar(std::string_view input);
    static bool contains_scalar(std::string_view haystack, std::string_view needle);
    static std::string to_lower_scalar(std::string_view input);
};

} // namespace logai

#endif // LOGAI_SIMD_STRING_OPS_H 