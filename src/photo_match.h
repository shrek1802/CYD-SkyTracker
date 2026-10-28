#pragma once
#include <cctype>
#include <cstring>

// Ensures that a Commons search hit actually contains the exact aircraft
// registration as a complete token, not a coincidental substring.
namespace PhotoMatch {
inline bool titleHasRegistration(const char* title, const char* registration) {
    if (!title || !registration) return false;
    const size_t n = std::strlen(registration);
    if (n < 4 || n > 14) return false;
    for (const char* p = title; *p; ++p) {
        size_t i = 0;
        while (i < n && p[i] &&
               std::toupper(static_cast<unsigned char>(p[i])) ==
               std::toupper(static_cast<unsigned char>(registration[i]))) ++i;
        if (i != n) continue;
        // Don't accept G-EUUK in a filename mentioning G-EUUK2.
        const bool before = p == title || !std::isalnum(static_cast<unsigned char>(p[-1]));
        const bool after = !p[n] || !std::isalnum(static_cast<unsigned char>(p[n]));
        if (before && after) return true;
    }
    return false;
}
}
