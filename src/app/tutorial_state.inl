// The Controller Configs window's tutorials: which have been seen, the tray's
// request to show one, and whether this is the first start after an install.
//
// ⭐ rhoquinn8217, 2026-10-14: *"I want a tutorial when a mode for each mode.
// Lets start with simple mode."* The page runs them, Simple's, Quick's and,
// since 2026-10-29, Advanced's. The listener keeps three things for them:
// whether each has been seen, in window-state.txt beside Open on bridge as
// "tutorial_simple_seen = 0" or "= 1" and the same for Quick and Advanced, so
// each starts by itself once and not with every window; and a request from
// the tray's list of tutorials (*"put a list
// of tutorials in the tray for now"*), which the page collects the way it
// collects Open on bridge, and which is not kept.
//
// ⭐ AN INSTALL STARTS THEM AGAIN, IN SIMPLE (rhoquinn8217, 2026-10-26:
// *"install or re-install should always make you start in simple mode"*, and a
// tour completed, skipped or ended does not come back by itself *"unless the
// user specifically asks or if DS5-USBIP has been re-installed"*). The
// installer writes a stamp of its own beside the program each time it runs
// (install-stamp.txt); the window's state keeps the last one this person's
// listener saw. A stamp it has not seen is a new install.
// ⓘ Its own file so the test binary can reach it.
#pragma once

#include <string>

namespace tutorial_state {

inline const char *const kSimpleSeenKey = "tutorial_simple_seen";
inline const char *const kQuickSeenKey = "tutorial_quick_seen";
inline const char *const kAdvancedSeenKey = "tutorial_advanced_seen";
inline const char *const kInstallStampKey = "install_stamp";
// ⓘ Beside the exe, written by installer/ds5-usbip.iss. A build run from its
// output folder has none, and never starts the tours again by itself.
inline const wchar_t *const kStampFile = L"install-stamp.txt";

// The tours, as the page and the tray name them. ⓘ 0 is none.
enum Kind { None = 0, Simple = 1, Quick = 2, Advanced = 3 };

inline const char *kind_name(int kind)
{
    return kind == Simple ? "simple" : kind == Quick ? "quick" : kind == Advanced ? "advanced" : "";
}

inline int kind_of(const std::string &name)
{
    return name == "simple" ? Simple : name == "quick" ? Quick : name == "advanced" ? Advanced : None;
}

// The value a window-state.txt line gives: seen only when it says 1. A file
// from before there were tutorials has no line, and junk in a hand-edited
// file reads the same: the tutorial shows once more, which costs a Skip.
inline bool parse_seen(const std::string &value)
{
    return value == "1";
}

// The line window-state.txt keeps a tour's in.
inline std::string seen_line(const char *key, bool seen)
{
    return std::string(key) + " = " + (seen ? "1" : "0") + "\n";
}

// What GET /api/v1/ui/tutorial answers: whether each has been seen, and the
// tour the tray asked for, or nothing.
inline std::string json(bool simpleSeen, bool quickSeen, bool advancedSeen, int asked)
{
    return std::string(R"({"simple_seen":)") + (simpleSeen ? "true" : "false") +
           R"(,"quick_seen":)" + (quickSeen ? "true" : "false") +
           R"(,"advanced_seen":)" + (advancedSeen ? "true" : "false") +
           R"(,"request":")" + kind_name(asked) + R"("})";
}

// A stamp read from the file: its first line, trimmed. ⓘ The installer writes
// one line; anything after it is not the stamp.
inline std::string stamp_from_file(const std::string &text)
{
    std::string line = text.substr(0, text.find_first_of("\r\n"));
    while (!line.empty() && (line.front() == ' ' || line.front() == '\t')) line.erase(line.begin());
    while (!line.empty() && (line.back() == ' ' || line.back() == '\t')) line.pop_back();
    return line;
}

// Whether this start is the first after an install: the installer left a
// stamp, and it is not the one the window's state last saw. ⓘ No stamp, as
// from a build's output folder, is never an install.
inline bool fresh_install(const std::string &fileStamp, const std::string &seenStamp)
{
    return !fileStamp.empty() && fileStamp != seenStamp;
}

}  // namespace tutorial_state
