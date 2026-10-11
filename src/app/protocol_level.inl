// Can this listener and a TV app work together? The protocol level each side
// has, and how this side says so.
//
// ⭐⭐ rhoquinn8217, 2026-10-08 ("1a and 2a"): a number that goes up ONLY WHEN
// THE WAY THE TWO TALK CHANGES, so an ordinary update on one side keeps working
// with the other; and when the two cannot work together, the TV refuses to
// bridge and says which side to update. The TV's half is the bridge core's
// protocol_level.inl, which reads what this side says.
//
// ➡️ Raise kLevel when a change means an older TV can no longer work with this
// listener -- a message it cannot skip, a layout it reads wrong -- and kMinTv
// when this listener can no longer work with a TV below some level. A change
// the other side copes with -- an optional word an older peer ignores, as the
// pairing proof and the port request are -- raises nothing.
//
// ⛔ WHY (code review, 2026-10-05): nothing compared the two sides. A listener
// that could not read the TV's greeting turned it down, and the pad hung
// "bridged" for the whole stream with nothing to say why.
//
// HOW. STATUS, which the TV asks every ten seconds, answers
// "OK CTM_AGENT_V1 level=1 min_tv=1", and the TV refuses to bridge to a
// listener below what it needs. A BRIDGE_START carries the TV's own level as
// "level=N", and one below kMinTv is turned away with "ERR update the TV app
// ...", which the TV shows in its bridge panel. A TV that says no level is
// level 1, the level of every TV from before there were levels.
// ⓘ Its own file so the test binary can reach it.

#pragma once

#include <string>

namespace protocol_level {

inline constexpr int kLevel = 1;   // this listener's
inline constexpr int kMinTv = 1;   // the oldest TV level it works with

// The number after `word` in a command, "level=" in
// "BRIDGE_START ds5 0 ctm-ds5-1 level=2", or `missing` when the word is not
// there, or no number follows it. Only a whole word, at the start or after a
// space: the pairing proof's signature is one long word of its own.
inline int word_value(const std::string &line, const std::string &word, int missing)
{
    size_t at = line.find(word);
    while (at != std::string::npos && at != 0 && line[at - 1] != ' ') {
        at = line.find(word, at + 1);
    }
    if (at == std::string::npos) return missing;
    size_t i = at + word.size();
    if (i >= line.size() || line[i] < '0' || line[i] > '9') return missing;
    long value = 0;
    for (int digits = 0; i < line.size() && line[i] >= '0' && line[i] <= '9' && digits < 6; ++i, ++digits) {
        value = value * 10 + (line[i] - '0');
    }
    return static_cast<int>(value);
}

// The answer to STATUS.
inline std::string status_reply()
{
    return "OK CTM_AGENT_V1 level=" + std::to_string(kLevel) + " min_tv=" + std::to_string(kMinTv) + "\n";
}

// The TV's level, from a BRIDGE_START.
inline int tv_level(const std::string &command)
{
    return word_value(command, "level=", 1);
}

// "" when a TV of that BRIDGE_START's level may bridge, or the refusal to send.
// ⚠️ The TV reads it by its first words, "ERR update the TV app".
inline std::string refusal_for(const std::string &command)
{
    if (tv_level(command) >= kMinTv) return "";
    return "ERR update the TV app: it is too old for DS5-USBIP on the PC\n";
}

} // namespace protocol_level
