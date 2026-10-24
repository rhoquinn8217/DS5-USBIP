// The Controller Configs window's tutorials: which have been seen, and the
// tray's request to show one again.
//
// ⭐ rhoquinn8217, 2026-10-14: *"I want a tutorial when a mode for each mode.
// Lets start with simple mode."* The page runs it. The listener keeps two
// things for it: whether Simple's has been seen, in window-state.txt beside
// Open on bridge as "tutorial_simple_seen = 0" or "= 1", so it starts by
// itself once and not with every window; and a request from the tray's "Show
// tutorial" (*"put it in the tray menu"*), which the page collects the way it
// collects Open on bridge, and which is not kept.
// ⓘ Its own file so the test binary can reach it.
#pragma once

#include <string>

namespace tutorial_state {

inline const char *const kSimpleSeenKey = "tutorial_simple_seen";

// The value a window-state.txt line gives: seen only when it says 1. A file
// from before there were tutorials has no line, and junk in a hand-edited
// file reads the same: the tutorial shows once more, which costs a Skip.
inline bool parse_seen(const std::string &value)
{
    return value == "1";
}

// The line window-state.txt keeps it in.
inline std::string seen_line(bool seen)
{
    return std::string(kSimpleSeenKey) + " = " + (seen ? "1" : "0") + "\n";
}

// What GET /api/v1/ui/tutorial answers: whether Simple's has been seen, and
// the layout the tray asked to show one in, or nothing. ⓘ Quick, whose tour
// is the one there is (rhoquinn8217, 2026-10-23: *"I want to demo quick mode
// tutorial first because its a smaller list"*).
inline std::string json(bool simpleSeen, bool asked)
{
    return std::string(R"({"simple_seen":)") + (simpleSeen ? "true" : "false") +
           R"(,"request":")" + (asked ? "quick" : "") + R"("})";
}

}  // namespace tutorial_state
