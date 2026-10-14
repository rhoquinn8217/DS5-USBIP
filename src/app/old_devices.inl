// The devices Windows keeps from past bridges: read off Windows, listed, and
// removed. old_devices_rule.inl says which are ours and why.
//
//   DS5-USBIP old-devices               lists them, as JSON; changes nothing
//   DS5-USBIP remove-old-devices        removes them; needs an administrator
//       --report                        and says how it went in a message box
//
// ⭐ The installer runs remove-old-devices on every install, upgrade and
// uninstall, being an administrator already.
//
// ⛔ NOTHING HERE WRITES device.log. These run without the listener's folder
// settled, so a log would land wherever they were started from: Program
// Files, for the installer.

#pragma once

#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cwchar>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "old_devices_rule.inl"

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

namespace old_devices {

// DEVPKEY_Device_Parent, written out so nothing here needs <initguid.h>, which
// would change what every later header's GUIDs mean in this translation unit.
// ⓘ Windows keeps it for a device that is not connected too, as the parent it
// last had: that is what makes a record ours or not.
inline const DEVPROPKEY kParentKey = { { 0x4340a6c5, 0x93fa, 0x4706, { 0x97, 0x2c, 0x7b, 0x64, 0x80, 0x08, 0xa5, 0xa7 } }, 8 };
inline const wchar_t *const kUsbipHardwareId = L"ROOT\\USBIP_WIN2\\UDE";

inline std::wstring parent_of(HDEVINFO set, SP_DEVINFO_DATA *info)
{
    DEVPROPTYPE type = 0;
    DWORD size = 0;
    SetupDiGetDevicePropertyW(set, info, &kParentKey, &type, nullptr, 0, &size, 0);
    if (size < sizeof(wchar_t)) return std::wstring();
    std::vector<BYTE> buf(size + sizeof(wchar_t), 0);
    if (!SetupDiGetDevicePropertyW(set, info, &kParentKey, &type, buf.data(), size, &size, 0) ||
        type != DEVPROP_TYPE_STRING) {
        return std::wstring();
    }
    return std::wstring(reinterpret_cast<const wchar_t *>(buf.data()));
}

// A registry property of a device as lines: a name, or each of a list of ids.
inline std::vector<std::wstring> registry_lines(HDEVINFO set, SP_DEVINFO_DATA *info, DWORD property)
{
    std::vector<std::wstring> out;
    DWORD type = 0, size = 0;
    SetupDiGetDeviceRegistryPropertyW(set, info, property, &type, nullptr, 0, &size);
    if (size < sizeof(wchar_t)) return out;
    std::vector<BYTE> buf(size + 2 * sizeof(wchar_t), 0);
    if (!SetupDiGetDeviceRegistryPropertyW(set, info, property, &type, buf.data(), size, &size)) return out;
    for (const wchar_t *p = reinterpret_cast<const wchar_t *>(buf.data()); *p; p += std::wcslen(p) + 1) {
        out.push_back(p);
        if (type != REG_MULTI_SZ) break;
    }
    return out;
}

struct Found {
    std::vector<old_devices_rule::Record> records;
    std::map<std::wstring, std::wstring> names;   // id -> what Windows calls it
    bool usbipFound = false;
};

// Every device Windows keeps a record of, connected or not.
inline Found read_all()
{
    Found found;
    HDEVINFO set = SetupDiGetClassDevsW(nullptr, nullptr, nullptr, DIGCF_ALLCLASSES);
    if (set == INVALID_HANDLE_VALUE) return found;
    SP_DEVINFO_DATA info = {};
    info.cbSize = sizeof info;
    for (DWORD i = 0; SetupDiEnumDeviceInfo(set, i, &info); ++i) {
        wchar_t id[MAX_DEVICE_ID_LEN + 1] = {};
        if (!SetupDiGetDeviceInstanceIdW(set, &info, id, MAX_DEVICE_ID_LEN, nullptr)) continue;
        old_devices_rule::Record r;
        r.id = id;
        r.parent = parent_of(set, &info);
        DEVINST node = 0;
        r.present = CM_Locate_DevNodeW(&node, id, CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS;
        for (const std::wstring &hardwareId : registry_lines(set, &info, SPDRP_HARDWAREID)) {
            if (_wcsicmp(hardwareId.c_str(), kUsbipHardwareId) == 0) r.usbipController = true;
        }
        if (r.usbipController) found.usbipFound = true;
        std::vector<std::wstring> name = registry_lines(set, &info, SPDRP_FRIENDLYNAME);
        if (name.empty()) name = registry_lines(set, &info, SPDRP_DEVICEDESC);
        found.names[r.id] = name.empty() ? std::wstring() : name[0];
        found.records.push_back(std::move(r));
    }
    SetupDiDestroyDeviceInfoList(set);
    return found;
}

// Removes the record of one device that is not connected. ⛔ Never one that
// is: it is looked for as a connected device first, and left if found.
inline CONFIGRET remove_one(const std::wstring &id)
{
    std::wstring copy = id;
    DEVINST node = 0;
    if (CM_Locate_DevNodeW(&node, copy.data(), CM_LOCATE_DEVNODE_NORMAL) == CR_SUCCESS) return CR_REMOVE_VETOED;
    const CONFIGRET located = CM_Locate_DevNodeW(&node, copy.data(), CM_LOCATE_DEVNODE_PHANTOM);
    if (located != CR_SUCCESS) return located;
    return CM_Uninstall_DevNode(node, 0);
}

inline bool is_administrator()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) return false;
    TOKEN_ELEVATION elevation = {};
    DWORD size = 0;
    const bool elevated = GetTokenInformation(token, TokenElevation, &elevation, sizeof elevation, &size) &&
                          elevation.TokenIsElevated != 0;
    CloseHandle(token);
    return elevated;
}

inline std::string json_text(const std::wstring &text)
{
    std::string out;
    for (wchar_t c : text) {
        if (c == L'"' || c == L'\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20) {
            char buf[8];
            std::snprintf(buf, sizeof buf, "\\u%04x", static_cast<unsigned>(c));
            out += buf;
        } else if (c < 0x80) {
            out += static_cast<char>(c);
        } else {
            out += '?';
        }
    }
    return out;
}

// `old-devices`: what would be removed, as JSON. Changes nothing, and needs
// no administrator. Exit 0, or 3 when usbip-win2's host controller is not
// there to judge by (then nothing can be called ours).
inline int list_mode()
{
    const Found found = read_all();
    const std::vector<old_devices_rule::Removal> removals = old_devices_rule::to_remove(found.records);
    std::cout << "{\"usbip_win2\":" << (found.usbipFound ? "true" : "false")
              << ",\"records\":" << found.records.size()
              << ",\"old_devices\":" << old_devices_rule::devices_among(removals)
              << ",\"old_entries\":" << removals.size()
              << ",\"entries\":[";
    for (size_t i = 0; i < removals.size(); ++i) {
        const auto name = found.names.find(removals[i].id);
        std::cout << (i ? ",\n " : "\n ")
                  << "{\"depth\":" << removals[i].depth
                  << ",\"id\":\"" << json_text(removals[i].id)
                  << "\",\"name\":\"" << json_text(name == found.names.end() ? std::wstring() : name->second) << "\"}";
    }
    std::cout << "\n]}\n";
    return found.usbipFound ? 0 : 3;
}

// What a removal has come to so far; shared with the worker doing it, which
// may outlive the wait for it.
struct Progress {
    std::atomic<bool> listed{false};
    std::atomic<bool> finished{false};
    std::atomic<bool> usbipFound{false};
    std::atomic<size_t> devices{0};
    std::atomic<size_t> entries{0};
    std::atomic<size_t> removed{0};
    std::atomic<size_t> failed{0};
    std::atomic<unsigned long> firstError{0};
};

// `remove-old-devices [--report]`. Exit 0 when nothing is left (or there was
// nothing), 5 without an administrator, 6 when some could not be removed, 7
// when Windows stopped answering.
inline int remove_mode(bool report)
{
    auto say = [report](const std::wstring &text, UINT icon) {
        std::wcout << L"remove-old-devices: " << text << L"\n";
        if (report) MessageBoxW(nullptr, text.c_str(), L"DS5-USBIP", icon | MB_OK | MB_TOPMOST | MB_SETFOREGROUND);
    };
    if (!is_administrator()) {
        say(L"Removing old controllers needs an administrator.", MB_ICONWARNING);
        return 5;
    }
    // ⛔ ON A WORKER, WITH A DEADLINE. When the USB/IP host controller wedges,
    // Device Manager and pnputil hang (2026-09), and this may too: better to
    // say so and stop than to leave the installer or a message box waiting on
    // it for ever. ⓘ Detached, so a hung one dies with the process.
    auto progress = std::make_shared<Progress>();
    std::thread([progress]() {
        const Found found = read_all();
        const std::vector<old_devices_rule::Removal> removals = old_devices_rule::to_remove(found.records);
        progress->usbipFound = found.usbipFound;
        progress->devices = old_devices_rule::devices_among(removals);
        progress->entries = removals.size();
        progress->listed = true;
        for (const old_devices_rule::Removal &r : removals) {
            const CONFIGRET result = remove_one(r.id);
            if (result == CR_SUCCESS) {
                ++progress->removed;
            } else if (progress->failed++ == 0) {
                progress->firstError = result;
            }
        }
        progress->finished = true;
    }).detach();

    size_t lastCount = 0;
    auto lastChange = std::chrono::steady_clock::now();
    while (!progress->finished) {
        Sleep(200);
        const size_t count = progress->removed + progress->failed + (progress->listed ? 1 : 0);
        if (count != lastCount) {
            lastCount = count;
            lastChange = std::chrono::steady_clock::now();
        } else if (std::chrono::steady_clock::now() - lastChange > std::chrono::seconds(30)) {
            break;
        }
    }

    const size_t devices = progress->devices, entries = progress->entries;
    const size_t removed = progress->removed, failed = progress->failed;
    wchar_t text[400];
    if (!progress->finished) {
        std::swprintf(text, 400, L"Windows stopped answering after %zu of %zu old entries were removed. Restart the PC, then try again.",
                      removed, entries);
        say(text, MB_ICONWARNING);
        return 7;
    }
    if (entries == 0) {
        say(progress->usbipFound ? L"Windows is keeping no old controllers from past bridges."
                                 : L"usbip-win2 is not installed, so Windows keeps nothing of DS5-USBIP's.",
            MB_ICONINFORMATION);
        return 0;
    }
    if (failed == 0) {
        std::swprintf(text, 400, L"Removed %zu old controllers and devices Windows was keeping from past bridges (%zu entries with their parts).",
                      devices, entries);
        say(text, MB_ICONINFORMATION);
        return 0;
    }
    std::swprintf(text, 400, L"Removed %zu of %zu old entries Windows was keeping from past bridges. %zu could not be removed (error 0x%lX).",
                  removed, entries, failed, progress->firstError.load());
    say(text, MB_ICONWARNING);
    return 6;
}

}  // namespace old_devices
