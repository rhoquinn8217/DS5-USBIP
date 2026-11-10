// Is a UAC prompt up in this session, and what do Windows' settings make of
// it? uac_prompt_rule.inl says why and how each answer is used; agent.inl
// watches.
#pragma once

#include <windows.h>
#include <wtsapi32.h>

#pragma comment(lib, "wtsapi32.lib")

#include "uac_prompt_rule.inl"

namespace uac_watch {

// This process's session: the one the person signed in to, which the stream
// shows. ⓘ Asked of itself, which is never refused.
inline DWORD own_session()
{
    DWORD session = 0;
    if (!ProcessIdToSessionId(GetCurrentProcessId(), &session)) return static_cast<DWORD>(-1);
    return session;
}

// Is consent.exe running in that session? ⓘ Through the enumeration, which
// carries each process's session: ProcessIdToSessionId is refused for
// consent.exe, which runs as SYSTEM.
inline bool prompt_up(DWORD session)
{
    WTS_PROCESS_INFOW *info = nullptr;
    DWORD count = 0;
    if (!WTSEnumerateProcessesW(WTS_CURRENT_SERVER_HANDLE, 0, 1, &info, &count) || info == nullptr) {
        return false;
    }
    bool up = false;
    for (DWORD i = 0; i < count && !up; ++i) {
        up = info[i].SessionId == session && info[i].pProcessName != nullptr &&
             _wcsicmp(info[i].pProcessName, L"consent.exe") == 0;
    }
    WTSFreeMemory(info);
    return up;
}

// One of Windows' UAC settings, or `missing` when it is not there.
inline int policy_value(const wchar_t *name, int missing)
{
    DWORD value = 0;
    DWORD size = sizeof value;
    if (RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
                     name, RRF_RT_REG_DWORD, nullptr, &value, &size) != ERROR_SUCCESS) {
        return missing;
    }
    return static_cast<int>(value);
}

// Is the person an administrator? A split token, limited or full, is always
// one. A plain token is UAC off, the built-in Administrator or an account that
// is not an administrator, told apart by the Administrators group. ⓘ Not known
// counts as an administrator: a Yes or No prompt, as on most PCs.
inline bool administrator()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return true;
    TOKEN_ELEVATION_TYPE type = TokenElevationTypeDefault;
    DWORD size = 0;
    const bool known = GetTokenInformation(token, TokenElevationType, &type, sizeof type, &size) != 0;
    CloseHandle(token);
    if (known && type != TokenElevationTypeDefault) return true;
    BYTE sid[SECURITY_MAX_SID_SIZE] = {};
    DWORD sidSize = sizeof sid;
    if (!CreateWellKnownSid(WinBuiltinAdministratorsSid, nullptr, sid, &sidSize)) return true;
    BOOL member = FALSE;
    if (!CheckTokenMembership(nullptr, sid, &member)) return true;
    return member != FALSE;
}

// What decides the kind, read as a prompt opens.
inline uac_prompt_rule::Policy read_policy()
{
    uac_prompt_rule::Policy policy;
    policy.administrator = administrator();
    policy.consentPromptBehaviorAdmin = policy_value(L"ConsentPromptBehaviorAdmin", 5);
    policy.typeOfAdminApprovalMode = policy_value(L"TypeOfAdminApprovalMode", 1);
    return policy;
}

}  // namespace uac_watch
