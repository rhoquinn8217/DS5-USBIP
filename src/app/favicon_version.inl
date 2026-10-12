// The address the Controller Configs window asks for its icon, with the
// icon's fingerprint in it.
//
// ⭐⭐ WHY (rhoquinn8217, 2026-10-12: *"The controller configs browser box still
// uses the black bridge icon"*). The window is a browser's, and the browser
// draws its title bar from the page's favicon. It keeps a favicon BY ITS
// ADDRESS and does not ask again, whatever Cache-Control says: the page
// linked /favicon.ico?v=1, the icon changed, the address did not, and the
// window went on showing the old picture with a new listener installed.
// ➡️ So the address carries a fingerprint of the icon's own bytes. A new
// icon is a new address, which the browser has never seen and has to fetch,
// and nobody has to remember to change a number.
//
// ⓘ Pure, so the tests can reach it: ui_page.inl hands it the bytes.

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace favicon_version {

// What the page links, and what the listener swaps for the fingerprinted
// address as it serves the page. ⛔ The page must keep exactly this.
inline const char *const kMark = "/favicon.ico?v=1";

// FNV-1a over the bytes, as 8 hex digits. Not a security measure: only a name
// that changes when the picture does.
inline std::string fingerprint(const void *bytes, size_t size)
{
    uint32_t h = 2166136261u;
    const unsigned char *p = static_cast<const unsigned char *>(bytes);
    for (size_t i = 0; i < size; ++i) {
        h ^= p[i];
        h *= 16777619u;
    }
    static const char hex[] = "0123456789abcdef";
    std::string out(8, '0');
    for (int i = 7; i >= 0; --i) {
        out[static_cast<size_t>(i)] = hex[h & 0xFu];
        h >>= 4;
    }
    return out;
}

// The page with its favicon link pointing at the fingerprinted address; the
// page as it was when it has no such link, or there is no fingerprint.
inline std::string with_fingerprint(std::string html, const std::string &print)
{
    if (print.empty()) return html;
    const size_t at = html.find(kMark);
    if (at == std::string::npos) return html;
    html.replace(at, std::char_traits<char>::length(kMark), std::string("/favicon.ico?v=") + print);
    return html;
}

}  // namespace favicon_version
