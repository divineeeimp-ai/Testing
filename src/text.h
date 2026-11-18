#pragma once
// Pure text helpers (no game dependencies).

#include <cstddef>
#include <cstdint>
#include <string>

namespace slm {

// UTF-16 (as stored in System.String) to UTF-8. Unpaired surrogates become U+FFFD.
inline std::string Utf16ToUtf8(const uint16_t* s, size_t n)
{
    std::string out;
    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        uint32_t cp = s[i];
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            if (i + 1 < n && s[i + 1] >= 0xDC00 && s[i + 1] <= 0xDFFF) {
                cp = 0x10000 + ((cp - 0xD800) << 10) + (s[i + 1] - 0xDC00);
                ++i;
            } else {
                cp = 0xFFFD;
            }
        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
            cp = 0xFFFD;
        }

        if (cp < 0x80) {
            out += static_cast<char>(cp);
        } else if (cp < 0x800) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }
    return out;
}

// Interprets the first `size` bytes of a little-endian enum value. `isSigned` comes from the
// enum's underlying type (value__ field), read at runtime.
inline int64_t EnumValueFromBytes(const uint8_t* bytes, int size, bool isSigned)
{
    uint64_t u = 0;
    for (int i = 0; i < size && i < 8; ++i)
        u |= static_cast<uint64_t>(bytes[i]) << (8 * i);
    if (isSigned && size > 0 && size < 8) {
        const uint64_t sign = 1ull << (size * 8 - 1);
        if (u & sign)
            u |= ~((sign << 1) - 1);
    }
    return static_cast<int64_t>(u);
}

} // namespace slm
