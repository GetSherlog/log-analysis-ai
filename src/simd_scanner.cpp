#include "simd_scanner.h"
#include <string>
#include <cstring>

#if defined(__SSE4_2__)
#include <nmmintrin.h>
#endif

#if defined(__AVX2__)
#include <immintrin.h>
#endif

#if defined(USE_NEON_SIMD)
#include <arm_neon.h>
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
#elif defined(USE_NEON_SIMD)
    return findCharNEON(start, remaining, c);
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

size_t SimdLogScanner::findChar(const char* data, size_t len, char target) {
    if (data == nullptr || len == 0) {
        return std::string::npos;
    }

    size_t pos = 0;

#if defined(__AVX2__)
    // Use AVX2 for 32 bytes at a time
    const __m256i target_vec = _mm256_set1_epi8(target);
    
    // Process 32 bytes at a time
    while (pos + 32 <= len) {
        const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i eq = _mm256_cmpeq_epi8(chunk, target_vec);
        const int mask = _mm256_movemask_epi8(eq);
        
        if (mask != 0) {
            // Found a match, get the position of the first set bit
            return pos + __builtin_ctz(mask);
        }
        
        pos += 32;
    }
#elif defined(__SSE4_2__)
    // Use SSE4.2 for 16 bytes at a time
    const __m128i target_vec = _mm_set1_epi8(target);
    
    // Process 16 bytes at a time
    while (pos + 16 <= len) {
        const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + pos));
        const __m128i eq = _mm_cmpeq_epi8(chunk, target_vec);
        const int mask = _mm_movemask_epi8(eq);
        
        if (mask != 0) {
            // Found a match, get the position of the first set bit
            return pos + __builtin_ctz(mask);
        }
        
        pos += 16;
    }
#elif defined(USE_NEON_SIMD)
    // Use NEON for 16 bytes at a time
    const uint8x16_t target_vec = vdupq_n_u8(target);
    
    // Process 16 bytes at a time
    while (pos + 16 <= len) {
        const uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + pos));
        const uint8x16_t eq = vceqq_u8(chunk, target_vec);
        
        // Convert vector comparison result to a mask
        uint64x2_t eq_64 = vreinterpretq_u64_u8(eq);
        uint64_t low = vgetq_lane_u64(eq_64, 0);
        uint64_t high = vgetq_lane_u64(eq_64, 1);
        
        // Fast check if any matches exist
        if (low | high) {
            // Determine the exact position of the first match
            uint64_t combined_mask = 0;
            for (int i = 0; i < 16; i++) {
                // Extract the MSB from each comparison result byte
                uint8_t byte_result = vgetq_lane_u8(eq, i);
                combined_mask |= (static_cast<uint64_t>(byte_result != 0) << i);
            }
            
            // Find first set bit (trailing zeros count)
            unsigned long index;
#if defined(__GNUC__) || defined(__clang__)
            index = __builtin_ctzll(combined_mask);
#else
            // Fallback bit scanning implementation if __builtin_ctzll is not available
            unsigned long mask = combined_mask;
            index = 0;
            while (!(mask & 1)) {
                mask >>= 1;
                ++index;
            }
#endif
            return pos + index;
        }
        
        pos += 16;
    }
#endif

    // Process remaining bytes
    while (pos < len) {
        if (data[pos] == target) {
            return pos;
        }
        pos++;
    }

    return std::string::npos;
}

size_t SimdLogScanner::findSubstring(const char* haystack, size_t haystack_len, const char* needle, size_t needle_len) {
    if (haystack == nullptr || needle == nullptr || haystack_len == 0 || needle_len == 0 || needle_len > haystack_len) {
        return std::string::npos;
    }

    // For single character case, use findChar for efficiency
    if (needle_len == 1) {
        return findChar(haystack, haystack_len, needle[0]);
    }

#if defined(__SSE4_2__)
    // SSE4.2 provides string comparison instructions which can be used for substring search
    if (needle_len <= 16) {
        // For needle length <= 16 bytes, we can use _mm_cmpestri
        const int mode = _SIDD_CMP_EQUAL_ORDERED | _SIDD_UBYTE_OPS | _SIDD_POSITIVE_POLARITY | _SIDD_LEAST_SIGNIFICANT;
        
        size_t pos = 0;
        while (pos <= haystack_len - needle_len) {
            // Calculate remaining length of haystack to search
            int remaining_len = static_cast<int>(haystack_len - pos);
            
            // Load the current chunk of haystack (up to 16 bytes)
            __m128i haystack_chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack + pos));
            // Load the needle
            __m128i needle_chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needle));
            
            // Find the position of the first match
            int idx = _mm_cmpestri(needle_chunk, static_cast<int>(needle_len), 
                                    haystack_chunk, remaining_len > 16 ? 16 : remaining_len, mode);
            
            if (idx < 16) {
                // Found a match starting at haystack[pos + idx]
                // Verify the match (necessary if needle_len > 16)
                if (pos + idx + needle_len <= haystack_len && 
                    memcmp(haystack + pos + idx, needle, needle_len) == 0) {
                    return pos + idx;
                }
            }
            
            // Move to the next position
            pos += idx < 16 ? idx + 1 : 16;
        }
    }
    else {
        // For needle longer than 16 bytes, use a rolling window approach with _mm_cmpestri
        // This implementation searches for the first 16 bytes of the needle using SSE4.2,
        // then verifies the full match using memcmp
        
        const int mode = _SIDD_CMP_EQUAL_ORDERED | _SIDD_UBYTE_OPS | _SIDD_POSITIVE_POLARITY | _SIDD_LEAST_SIGNIFICANT;
        __m128i needle_prefix = _mm_loadu_si128(reinterpret_cast<const __m128i*>(needle));
        
        size_t pos = 0;
        while (pos <= haystack_len - needle_len) {
            // Calculate remaining length of haystack to search
            int remaining_len = static_cast<int>(haystack_len - pos);
            
            // Load the current chunk of haystack
            __m128i haystack_chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(haystack + pos));
            
            // Find the position of the potential match (just checking first 16 bytes of needle)
            int idx = _mm_cmpestri(needle_prefix, 16, 
                                 haystack_chunk, remaining_len > 16 ? 16 : remaining_len, mode);
            
            if (idx < 16 && pos + idx + needle_len <= haystack_len) {
                // Potential match found, verify the full needle
                if (memcmp(haystack + pos + idx, needle, needle_len) == 0) {
                    return pos + idx;
                }
            }
            
            // Move to the next position
            pos += idx < 16 ? idx + 1 : 16;
        }
    }
#elif defined(USE_NEON_SIMD)
    // NEON doesn't have direct string comparison instructions like SSE4.2
    // Instead, implement an optimized substring search
    
    // If needle is short enough, we can use NEON for prefix matching
    if (needle_len <= 16) {
        const uint8x16_t first_char_vec = vdupq_n_u8(needle[0]);
        
        size_t pos = 0;
        while (pos <= haystack_len - needle_len) {
            // Process 16 bytes at a time, looking for the first character
            const uint8x16_t haystack_chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(haystack + pos));
            const uint8x16_t eq = vceqq_u8(haystack_chunk, first_char_vec);
            
            // Convert vector to a 64-bit value where each bit represents a match
            uint64x2_t eq_64 = vreinterpretq_u64_u8(eq);
            uint64_t low = vgetq_lane_u64(eq_64, 0);
            uint64_t high = vgetq_lane_u64(eq_64, 1);
            
            while (low != 0 || high != 0) {
                // Find position of first potential match
                size_t match_pos = 0;
                
                if (low != 0) {
                    // Find first set bit in low 64 bits
                    match_pos = __builtin_ctzll(low) / 8;
                    // Clear the bit so we can find the next match if needed
                    low &= ~(1ULL << (match_pos * 8));
                } else {
                    // Find first set bit in high 64 bits
                    match_pos = 8 + (__builtin_ctzll(high) / 8);
                    // Clear the bit
                    high &= ~(1ULL << ((match_pos - 8) * 8));
                }
                
                // Check if this is a real match by comparing the full needle
                if (pos + match_pos + needle_len <= haystack_len &&
                    memcmp(haystack + pos + match_pos, needle, needle_len) == 0) {
                    return pos + match_pos;
                }
            }
            
            pos += 16;
        }
    }
#endif

    // Fall back to standard string search algorithm
    // Can use Boyer-Moore or KMP for better performance
    for (size_t i = 0; i <= haystack_len - needle_len; ++i) {
        if (memcmp(haystack + i, needle, needle_len) == 0) {
            return i;
        }
    }

    return std::string::npos;
}

size_t SimdLogScanner::findLast(const char* data, size_t len, char target) {
    if (data == nullptr || len == 0) {
        return std::string::npos;
    }

#if defined(USE_NEON_SIMD)
    // NEON implementation for findLast
    const uint8x16_t target_vec = vdupq_n_u8(target);
    
    // Start from the end and work backwards in 16-byte chunks
    size_t pos = len;
    while (pos >= 16) {
        pos -= 16;
        
        const uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + pos));
        const uint8x16_t eq = vceqq_u8(chunk, target_vec);
        
        // Convert vector to a 64-bit value where each bit represents a match
        uint64x2_t eq_64 = vreinterpretq_u64_u8(eq);
        uint64_t low = vgetq_lane_u64(eq_64, 0);
        uint64_t high = vgetq_lane_u64(eq_64, 1);
        
        if (high != 0 || low != 0) {
            // We have at least one match. Find the last one.
            for (int i = 15; i >= 0; --i) {
                if (i >= 8) {
                    // Check high 64 bits
                    if ((high & (1ULL << ((i - 8) * 8))) != 0) {
                        return pos + i;
                    }
                } else {
                    // Check low 64 bits
                    if ((low & (1ULL << (i * 8))) != 0) {
                        return pos + i;
                    }
                }
            }
        }
    }
    
    // Process any remaining bytes (less than 16)
    for (int i = pos - 1; i >= 0; --i) {
        if (data[i] == target) {
            return i;
        }
    }
#else
    // Scalar implementation for other platforms
    for (size_t i = len; i > 0; --i) {
        if (data[i - 1] == target) {
            return i - 1;
        }
    }
#endif

    return std::string::npos;
}

size_t SimdLogScanner::countChar(const char* data, size_t len, char target) {
    if (data == nullptr || len == 0) {
        return 0;
    }

    size_t count = 0;
    size_t pos = 0;

#if defined(__AVX2__)
    // AVX2 implementation
    const __m256i target_vec = _mm256_set1_epi8(target);
    
    while (pos + 32 <= len) {
        const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i eq = _mm256_cmpeq_epi8(chunk, target_vec);
        const int mask = _mm256_movemask_epi8(eq);
        count += __builtin_popcount(mask);
        pos += 32;
    }
#elif defined(__SSE4_2__)
    // SSE4.2 implementation
    const __m128i target_vec = _mm_set1_epi8(target);
    
    while (pos + 16 <= len) {
        const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + pos));
        const __m128i eq = _mm_cmpeq_epi8(chunk, target_vec);
        const int mask = _mm_movemask_epi8(eq);
        count += __builtin_popcount(mask);
        pos += 16;
    }
#elif defined(USE_NEON_SIMD)
    // NEON implementation
    const uint8x16_t target_vec = vdupq_n_u8(target);
    
    while (pos + 16 <= len) {
        const uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + pos));
        const uint8x16_t eq = vceqq_u8(chunk, target_vec);
        
        // Count the number of matches in the vector
        // First, add pairs of bytes
        uint8x8_t sum8 = vpadd_u8(vget_low_u8(eq), vget_high_u8(eq));
        // Then, add pairs of 16-bit values
        uint16x4_t sum16 = vpaddl_u8(sum8);
        // Then, add pairs of 32-bit values
        uint32x2_t sum32 = vpaddl_u16(sum16);
        // Finally, add the two 32-bit values
        uint64x1_t sum64 = vpaddl_u32(sum32);
        
        // Extract the final count and add it to our running total
        count += vget_lane_u64(sum64, 0) >> 56;
        
        pos += 16;
    }
#endif

    // Process remaining bytes
    while (pos < len) {
        if (data[pos] == target) {
            count++;
        }
        pos++;
    }

    return count;
}

std::vector<size_t> SimdLogScanner::findAllChar(const char* data, size_t len, char target) {
    std::vector<size_t> positions;
    
    if (data == nullptr || len == 0) {
        return positions;
    }
    
    size_t pos = 0;

#if defined(__AVX2__)
    // AVX2 implementation
    const __m256i target_vec = _mm256_set1_epi8(target);
    
    while (pos + 32 <= len) {
        const __m256i chunk = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(data + pos));
        const __m256i eq = _mm256_cmpeq_epi8(chunk, target_vec);
        int mask = _mm256_movemask_epi8(eq);
        
        while (mask != 0) {
            const int bit_pos = __builtin_ctz(mask);
            positions.push_back(pos + bit_pos);
            mask &= (mask - 1);  // Clear the least significant bit
        }
        
        pos += 32;
    }
#elif defined(__SSE4_2__)
    // SSE4.2 implementation
    const __m128i target_vec = _mm_set1_epi8(target);
    
    while (pos + 16 <= len) {
        const __m128i chunk = _mm_loadu_si128(reinterpret_cast<const __m128i*>(data + pos));
        const __m128i eq = _mm_cmpeq_epi8(chunk, target_vec);
        int mask = _mm_movemask_epi8(eq);
        
        while (mask != 0) {
            const int bit_pos = __builtin_ctz(mask);
            positions.push_back(pos + bit_pos);
            mask &= (mask - 1);  // Clear the least significant bit
        }
        
        pos += 16;
    }
#elif defined(USE_NEON_SIMD)
    // NEON implementation
    const uint8x16_t target_vec = vdupq_n_u8(target);
    
    while (pos + 16 <= len) {
        const uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(data + pos));
        const uint8x16_t eq = vceqq_u8(chunk, target_vec);
        
        // Convert vector to a pair of 64-bit values where each bit represents a match
        uint64x2_t eq_64 = vreinterpretq_u64_u8(eq);
        uint64_t low = vgetq_lane_u64(eq_64, 0);
        uint64_t high = vgetq_lane_u64(eq_64, 1);
        
        // Process matches in low 64 bits
        for (int i = 0; i < 8; ++i) {
            if ((low & (1ULL << (i * 8))) != 0) {
                positions.push_back(pos + i);
            }
        }
        
        // Process matches in high 64 bits
        for (int i = 0; i < 8; ++i) {
            if ((high & (1ULL << (i * 8))) != 0) {
                positions.push_back(pos + 8 + i);
            }
        }
        
        pos += 16;
    }
#endif

    // Process remaining bytes
    while (pos < len) {
        if (data[pos] == target) {
            positions.push_back(pos);
        }
        pos++;
    }
    
    return positions;
}

#if defined(USE_NEON_SIMD)
size_t SimdLogScanner::findCharNEON(const char* start, size_t len, char c) const {
    const uint8x16_t search_char = vdupq_n_u8(c);
    
    size_t pos = 0;
    while (pos + 16 <= len) {
        uint8x16_t chunk = vld1q_u8(reinterpret_cast<const uint8_t*>(start + pos));
        uint8x16_t eq = vceqq_u8(chunk, search_char);
        
        // Convert vector comparison result to a mask
        uint64x2_t eq_64 = vreinterpretq_u64_u8(eq);
        uint64_t low = vgetq_lane_u64(eq_64, 0);
        uint64_t high = vgetq_lane_u64(eq_64, 1);
        
        // Fast check if any matches exist
        if (low | high) {
            // Determine the exact position of the first match
            uint64_t combined_mask = 0;
            for (int i = 0; i < 16; i++) {
                // Extract the MSB from each comparison result byte
                uint8_t byte_result = vgetq_lane_u8(eq, i);
                combined_mask |= (static_cast<uint64_t>(byte_result != 0) << i);
            }
            
            // Find first set bit (trailing zeros count)
            unsigned long index;
#if defined(__GNUC__) || defined(__clang__)
            index = __builtin_ctzll(combined_mask);
#else
            // Fallback bit scanning implementation if __builtin_ctzll is not available
            unsigned long mask = combined_mask;
            index = 0;
            while (!(mask & 1)) {
                mask >>= 1;
                ++index;
            }
#endif
            return pos + index;
        }
        
        pos += 16;
    }
    
    // Process remaining bytes with scalar code
    for (size_t i = pos; i < len; ++i) {
        if (start[i] == c) {
            return i;
        }
    }
    
    return std::string::npos;
}
#endif

} // namespace logai 