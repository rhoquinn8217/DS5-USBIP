// A Windows UAC prompt, and what the TV is told about it.
//
// ⭐ rhoquinn8217, 2026-11-09: *"can DS5-USBIP tell when a user account control
// prompt has opened?"*, then *"tell the TV, but override the controller inputs
// bridge or not, similar to the lock screen"* and *"I want to insure that the
// controller will be able to answer any UAC prompt in any case including when a
// controller is not bridged and or bridge but the controller has a config that
// doesn't have d-pad movement and x as select or a mouse mode and click."* For a
// bridged controller their choice was *"tv takes them"*.
// ➡️ So this side only notices the prompt and says what kind it is, in its
// STATUS answer (protocol_level.inl). The TV takes every controller while a
// prompt is up, bridged or not, and shows what each button does; a bridged
// controller's input is held back from this PC meanwhile, so no config here
// is in the way.
//
// ⭐ HOW A PROMPT IS SEEN (uac_watch.inl; agent.inl watches). Windows runs
// consent.exe in the person's session for exactly as long as a prompt is up,
// whether it is on the dimmed secure desktop or not. Measured on this project's
// PC, 2026-11-09: it appeared as the prompt opened and was gone the moment it
// was answered, and a process like this one, not elevated, sees it and its
// session through WTSEnumerateProcesses. ⓘ ProcessIdToSessionId is refused
// for it (error 5), so the session comes from the enumeration.
//
// ⭐ WHICH KIND. The prompt cannot be read from here -- the secure desktop is
// closed to this process -- so the kind comes from Windows' own settings, read
// from HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System as the
// prompt opens:
//   - an account that is not an administrator is asked for an administrator's
//     password or PIN;
//   - an administrator is too when ConsentPromptBehaviorAdmin is 1 or 3
//     ("prompt for credentials"), or with Administrator protection
//     (TypeOfAdminApprovalMode 2), which asks for Windows Hello;
//   - otherwise an administrator is asked Yes or No (ConsentPromptBehaviorAdmin
//     5, the default, and 2 and 4).
// A prompt that can ask for a password or PIN is the one where the TV offers
// its keyboard (their words: *"remove numpad and keyboard(unless there is a UAC
// screen that may require keyboard input, then add the keybaord(aurora-tv's))"*).
//
// ⓘ NEVER WHILE WINDOWS IS LOCKED. The lock screen has its own list on the TV,
// and its own rule here (lock_screen_rule.inl), so STATUS never says both.
//
// ⓘ Its own file, with no windows.h, so the test binary can reach the rule.
#pragma once

namespace uac_prompt_rule {

// What STATUS says, as "uac=0", "uac=1" or "uac=2".
enum Kind : int {
    kNone = 0,     // no prompt
    kYesNo = 1,    // a Yes or No prompt
    kTyping = 2,   // a prompt that can ask for a password or PIN
};

// Windows' settings that decide the kind, each with its default for when the
// value is not there.
struct Policy {
    bool administrator = true;
    int consentPromptBehaviorAdmin = 5;
    int typeOfAdminApprovalMode = 1;
};

// The kind of a prompt that is up.
inline Kind kind_of_prompt(const Policy &policy)
{
    if (!policy.administrator) return kTyping;
    if (policy.typeOfAdminApprovalMode == 2) return kTyping;
    if (policy.consentPromptBehaviorAdmin == 1 || policy.consentPromptBehaviorAdmin == 3) return kTyping;
    return kYesNo;
}

// What the TV is told: nothing with no prompt, and nothing while Windows is
// locked.
inline Kind status_for(bool promptUp, bool windowsLocked, const Policy &policy)
{
    if (!promptUp || windowsLocked) return kNone;
    return kind_of_prompt(policy);
}

// A kind as STATUS may carry it: anything else is no prompt.
inline int status_word_value(int kind)
{
    return kind == kYesNo || kind == kTyping ? kind : kNone;
}

}  // namespace uac_prompt_rule
