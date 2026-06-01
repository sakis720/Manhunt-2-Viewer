#pragma once

#include <vector>
#include <string>
#include <cstdint>
#include <cstring>

/**
 * Reads the entire content of a file into a byte vector.
 */
bool ReadFileBytes(const std::string& path, std::vector<uint8_t>& out);

/**
 * Decompresses Zlib data.
 */
bool DecompressZlib(const uint8_t* src, size_t srcLen, std::vector<uint8_t>& dst);

/**
 * Reads a Little-Endian value from a byte buffer with bounds checking.
 */
template<typename T>
inline T ReadLE(const uint8_t* data, size_t len, size_t pos) {
    if (pos + sizeof(T) > len) return T(0);
    T val;
    std::memcpy(&val, data + pos, sizeof(T));
    return val;
}

/**
 * Version of ReadLE that doesn't check bounds (useful when length is not known or already checked).
 */
template<typename T>
inline T ReadLE(const uint8_t* p) {
    T v;
    std::memcpy(&v, p, sizeof(T));
    return v;
}
