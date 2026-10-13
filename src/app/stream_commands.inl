// Vibepollo's stream commands: the one entry this program keeps in the list
// of commands Vibepollo runs as every stream starts and ends.
//
// ⭐ WHY (rhoquinn8217, 2026-10-12: "I want to be able to set vibepollo to
// start DS5-USBIP when a stream starts and have the installer do it"; and,
// of the stream's end, 2026-10-06: "stop it when the stream ends").
// Vibepollo, like the Sunshine it comes from, keeps that list as one line of
// its config\sunshine.conf:
//
//     global_prep_cmd = [{"do":"<command>","elevated":false,"undo":"<command>"}]
//
// "do" runs as a stream starts and "undo" as it ends. The installer puts one
// entry there, "<this program>" start and "<this program>" stop, BESIDE what
// the person keeps there, and the uninstaller takes out ours and only ours.
// ⓘ No Vibepollo login is needed: it is a file, and the installer already
// runs as administrator.
//
// ⛔ What keeps Vibepollo going is stream_start.inl, not this: Vibepollo waits
// for a "do" command to end with no time limit, and one that fails keeps the
// stream from starting.
//
// ⓘ Text in and text out, so the tests can reach it; stream_start.inl reads
// and writes the file. That one line changes and nothing else, byte for byte.

#pragma once

#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

namespace stream_commands {

inline const char *const kKey = "global_prep_cmd";

// An entry is ours when either of its commands names this program, wherever
// it was installed. Compared lowercased: Windows paths ignore case.
inline const char *const kMarker = "ds5-usbip.exe";

enum class Result { Unchanged, Changed, NotUnderstood };

inline std::string json_escape(const std::string &text)
{
    std::string out;
    for (const unsigned char c : text) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", c);
            out += buf;
        } else {
            out += static_cast<char>(c);
        }
    }
    return out;
}

// Our entry for the program at exeUtf8: start as a stream starts, stop as it
// ends, never elevated (it runs as the person), the keys in the order
// Vibepollo writes them.
inline std::string our_entry(const std::string &exeUtf8)
{
    const std::string quoted = "\"" + exeUtf8 + "\"";
    return "{\"do\":\"" + json_escape(quoted + " start") + "\",\"elevated\":false,\"undo\":\"" +
           json_escape(quoted + " stop") + "\"}";
}

inline bool is_ours(const std::string &entry)
{
    std::string lowered;
    for (const char c : entry) lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lowered.find(kMarker) != std::string::npos;
}

// The entries of a JSON array of objects, each as its own text, untouched.
// False for anything else, which is then left alone. Empty is an empty list.
inline bool split_entries(const std::string &text, std::vector<std::string> *entries)
{
    entries->clear();
    const size_t n = text.size();
    size_t i = 0;
    auto skipSpace = [&]() {
        while (i < n && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
    };
    skipSpace();
    if (i == n) return true;
    if (text[i] != '[') return false;
    ++i;
    skipSpace();
    if (i < n && text[i] == ']') {
        ++i;
        skipSpace();
        return i == n;
    }
    for (;;) {
        skipSpace();
        if (i >= n || text[i] != '{') return false;
        const size_t start = i;
        int depth = 0;
        bool inString = false;
        bool closed = false;
        for (; i < n; ++i) {
            const char c = text[i];
            if (inString) {
                if (c == '\\') ++i;   // whatever is escaped, it ends nothing
                else if (c == '"') inString = false;
            } else if (c == '"') {
                inString = true;
            } else if (c == '{' || c == '[') {
                ++depth;
            } else if ((c == '}' || c == ']') && --depth == 0) {
                ++i;
                closed = true;
                break;
            }
        }
        if (!closed) return false;
        entries->push_back(text.substr(start, i - start));
        skipSpace();
        if (i < n && text[i] == ',') {
            ++i;
            continue;
        }
        if (i < n && text[i] == ']') {
            ++i;
            skipSpace();
            return i == n;
        }
        return false;
    }
}

inline std::string joined(const std::vector<std::string> &entries)
{
    std::string out = "[";
    for (size_t k = 0; k < entries.size(); ++k) {
        if (k != 0) out += ",";
        out += entries[k];
    }
    return out + "]";
}

// Where the key is set: its value, from after "= " to the end of its line.
// The LAST such line, as a reader that keeps the last value would see it;
// '#' starts a comment, and a key that only begins with the name is another.
struct Setting {
    bool found = false;
    size_t valueStart = 0;
    size_t valueEnd = 0;
};

inline Setting find_setting(const std::string &conf)
{
    Setting last;
    const std::string key = kKey;
    size_t pos = 0;
    while (pos < conf.size()) {
        size_t end = conf.find('\n', pos);
        if (end == std::string::npos) end = conf.size();
        size_t lineEnd = end;
        if (lineEnd > pos && conf[lineEnd - 1] == '\r') --lineEnd;
        size_t i = pos;
        while (i < lineEnd && (conf[i] == ' ' || conf[i] == '\t')) ++i;
        if (i + key.size() <= lineEnd && conf.compare(i, key.size(), key) == 0) {
            size_t j = i + key.size();
            while (j < lineEnd && (conf[j] == ' ' || conf[j] == '\t')) ++j;
            if (j < lineEnd && conf[j] == '=') {
                ++j;
                while (j < lineEnd && (conf[j] == ' ' || conf[j] == '\t')) ++j;
                last.found = true;
                last.valueStart = j;
                last.valueEnd = lineEnd;
            }
        }
        pos = end + 1;
    }
    return last;
}

// The settings with our entry in the list, once, after the person's own.
inline Result with_ours(const std::string &conf, const std::string &exeUtf8, std::string *out)
{
    *out = conf;
    const std::string entry = our_entry(exeUtf8);
    const Setting setting = find_setting(conf);
    if (!setting.found) {
        const std::string nl = conf.find("\r\n") != std::string::npos ? "\r\n" : "\n";
        std::string added = conf;
        if (!added.empty() && added.back() != '\n') added += nl;
        added += std::string(kKey) + " = [" + entry + "]" + nl;
        *out = added;
        return Result::Changed;
    }
    std::vector<std::string> entries;
    if (!split_entries(conf.substr(setting.valueStart, setting.valueEnd - setting.valueStart), &entries)) {
        return Result::NotUnderstood;
    }
    std::vector<std::string> kept;
    int oursAsWanted = 0;
    int oursOtherwise = 0;
    for (const std::string &e : entries) {
        if (!is_ours(e)) kept.push_back(e);
        else if (e == entry) ++oursAsWanted;
        else ++oursOtherwise;
    }
    if (oursAsWanted == 1 && oursOtherwise == 0) return Result::Unchanged;
    kept.push_back(entry);
    *out = conf.substr(0, setting.valueStart) + joined(kept) + conf.substr(setting.valueEnd);
    return Result::Changed;
}

// The settings with every entry of ours taken out, the person's left as they were.
inline Result without_ours(const std::string &conf, std::string *out)
{
    *out = conf;
    const Setting setting = find_setting(conf);
    if (!setting.found) return Result::Unchanged;
    std::vector<std::string> entries;
    if (!split_entries(conf.substr(setting.valueStart, setting.valueEnd - setting.valueStart), &entries)) {
        return Result::NotUnderstood;
    }
    std::vector<std::string> kept;
    for (const std::string &e : entries) {
        if (!is_ours(e)) kept.push_back(e);
    }
    if (kept.size() == entries.size()) return Result::Unchanged;
    *out = conf.substr(0, setting.valueStart) + joined(kept) + conf.substr(setting.valueEnd);
    return Result::Changed;
}

}  // namespace stream_commands
