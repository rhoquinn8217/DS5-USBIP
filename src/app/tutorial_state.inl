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

#include <cstdint>
#include <string>
#include <vector>

namespace tutorial_state {

inline const char *const kSimpleSeenKey = "tutorial_simple_seen";
inline const char *const kQuickSeenKey = "tutorial_quick_seen";
inline const char *const kAdvancedSeenKey = "tutorial_advanced_seen";
inline const char *const kPartsDoneKey = "tutorial_parts_done";
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

// ⭐⭐ THE TOURS IN PARTS (rhoquinn8217, 2026-11-13: "I want to break up
// simples tour into parts. Simple Mode will have the quick switch part. Quick
// mode should also have it. What I want to do is have the app record if a
// part has been completed. The tutorials that start automatically when going
// to advance, simple and quick mode will always go through a set of tutorial
// parts that have not already been done. If you start the tutorial in the
// tray it goes through all of them."). Eight parts, each ONE part whatever
// tours have it (*"I said not to have duplicates"*): a part two tours share
// is done in either, and a tour starting by itself leaves it out. The page
// holds the steps, each tour's own for a part; the listener keeps which parts
// are done, in window-state.txt as "tutorial_parts_done = modes,switch", and
// lists them in the tray: under each mode its own, in its order, below Whole
// tutorial, and under By topic every part once, in this order.
// ⛔ THE PAGE HAS THE SAME LIST (TUT_PARTS in controller-config-test-client.html):
// the same ids, names and tours. A part added or renamed is added or renamed
// in both.
struct Part {
    const char *id;         // the page's name for it, and window-state.txt's
    const wchar_t *name;    // the tray's
    bool tours[4];          // by Kind: which tours have it
};
inline constexpr int kPartCount = 8;
inline constexpr Part kParts[kPartCount] = {
    { "modes",       L"Switching modes",       { false, true,  true,  true  } },
    { "controllers", L"Your controllers",      { false, true,  true,  false } },
    { "making",      L"Making configs",        { false, true,  false, true  } },
    { "managing",    L"Managing configs",      { false, true,  true,  true  } },
    { "window",      L"The window",            { false, true,  true,  true  } },
    { "switch",      L"Quick switch",          { false, true,  true,  false } },
    { "overview",    L"The Overview and tabs", { false, false, false, true  } },
    { "customizing", L"Customizing configs",   { false, false, false, true  } },
};

// Each tour's parts in the order it runs them, as places in kParts. ⓘ
// Advanced's makes a config before customizing it, and archives it after.
inline std::vector<int> tour_parts(int kind)
{
    if (kind == Simple) return { 0, 1, 2, 3, 4, 5 };
    if (kind == Quick) return { 0, 1, 3, 4, 5 };
    if (kind == Advanced) return { 0, 6, 2, 7, 3, 4 };
    return {};
}

// A part's place in kParts by its id, or -1.
inline int part_index(const std::string &id)
{
    for (int i = 0; i < kPartCount; ++i) {
        if (id == kParts[i].id) return i;
    }
    return -1;
}

// The parts done, one bit each by their place in kParts, from window-state.txt's
// "modes,switch". ⓘ An id this listener does not know is left out, and so is
// junk: a part not done shows again, which costs a Skip at most.
inline uint32_t parse_parts(const std::string &value)
{
    uint32_t done = 0;
    size_t at = 0;
    while (at <= value.size()) {
        size_t comma = value.find(',', at);
        if (comma == std::string::npos) comma = value.size();
        std::string id = value.substr(at, comma - at);
        while (!id.empty() && (id.front() == ' ' || id.front() == '\t')) id.erase(id.begin());
        while (!id.empty() && (id.back() == ' ' || id.back() == '\t')) id.pop_back();
        const int i = part_index(id);
        if (i >= 0) done |= 1u << i;
        at = comma + 1;
    }
    return done;
}

// The parts done as the page and window-state.txt name them, in kParts' order.
inline std::string parts_list(uint32_t done)
{
    std::string out;
    for (int i = 0; i < kPartCount; ++i) {
        if (!(done & (1u << i))) continue;
        if (!out.empty()) out += ",";
        out += kParts[i].id;
    }
    return out;
}

// The line window-state.txt keeps them in.
inline std::string parts_line(uint32_t done)
{
    return std::string(kPartsDoneKey) + " = " + parts_list(done) + "\n";
}

// ⭐ WHICH TOUR A PART FROM BY TOPIC RUNS IN: the one for the mode the window is
// in when that tour has it, else the first that does, Simple, Quick, Advanced.
inline int kind_for_part(int part, int currentKind)
{
    if (part < 0 || part >= kPartCount) return None;
    if (currentKind >= Simple && currentKind <= Advanced && kParts[part].tours[currentKind]) {
        return currentKind;
    }
    for (int kind = Simple; kind <= Advanced; ++kind) {
        if (kParts[part].tours[kind]) return kind;
    }
    return None;
}

// What GET /api/v1/ui/tutorial answers: whether each has been seen, the tour
// the tray asked for, or nothing, and the part of it, or nothing for the whole
// tour; and the parts done.
inline std::string json(bool simpleSeen, bool quickSeen, bool advancedSeen, int asked,
                        int askedPart = -1, uint32_t partsDone = 0)
{
    return std::string(R"({"simple_seen":)") + (simpleSeen ? "true" : "false") +
           R"(,"quick_seen":)" + (quickSeen ? "true" : "false") +
           R"(,"advanced_seen":)" + (advancedSeen ? "true" : "false") +
           R"(,"request":")" + kind_name(asked) +
           R"(","part":")" + (asked != None && askedPart >= 0 && askedPart < kPartCount ? kParts[askedPart].id : "") +
           R"(","parts_done":")" + parts_list(partsDone) + R"("})";
}

// A stamp read from the file: its first line, trimmed. ⓘ The installer writes
// the stamp first, then whether it found a copy installed
// (config/default_config_rule.inl); anything after the first line is not the
// stamp.
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
