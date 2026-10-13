// What Vibepollo runs as a stream starts and ends, DS5-USBIP start and
// DS5-USBIP stop (stream_commands.inl puts them in its list), and the command
// the installer puts them there with.
//
// ⛔⛔ VIBEPOLLO WAITS FOR A START COMMAND TO END, WITH NO TIME LIMIT, and one
// that fails keeps the stream from starting. Run as the listener itself, this
// program would never end, and the stream would hang. So `start` starts the
// listener as a process of its own, hands it nothing of its own (no handles,
// and its own folder to start in), and returns at once; `stop` asks the
// listener to quit and waits a little. Both end with 0 whatever happens, and
// say what happened on their output, which Vibepollo keeps in its log.

#pragma once

#include <fstream>
#include <sstream>
#include <string>

namespace stream_start {

inline std::wstring own_path()
{
    std::wstring path(32768, L'\0');
    const DWORD n = GetModuleFileNameW(nullptr, &path[0], static_cast<DWORD>(path.size()));
    if (n == 0 || n >= path.size()) return std::wstring();
    path.resize(n);
    return path;
}

inline std::string utf8_of(const std::wstring &text)
{
    if (text.empty()) return std::string();
    const int n = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                      nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(n > 0 ? n : 0), '\0');
    if (n > 0) {
        WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), &out[0], n, nullptr, nullptr);
    }
    return out;
}

// ⭐ START: the listener as a process of its own, as a double-click starts it
// (agent --ui) but with no window (--no-start-window). rhoquinn8217,
// 2026-10-12: "on stream start, no controller configs window", now that
// aurora-tv's badge and button say the listener is on. The window still opens
// when a controller bridges, unless "Open on bridge" is off
// (open_on_bridge.inl). One already running is left exactly as it is, by the
// new process itself (main.cpp's agent mode), which then ends.
inline int start()
{
    const std::wstring exe = own_path();
    if (exe.empty()) {
        std::wcout << L"start: this program's own path is unknown, so nothing was started\n";
        return 0;
    }
    const std::wstring folder = exe.substr(0, exe.find_last_of(L"\\/"));
    std::wstring command = L"\"" + exe + L"\" agent --ui --no-start-window";
    STARTUPINFOW si = {};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi = {};
    // ⓘ Out of any job Vibepollo runs its commands in, where the job allows
    // it: a job closed at the end of the stream would end the listener with
    // it, bridges and all, instead of the stop that releases them.
    const DWORD flags = DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP;
    BOOL ok = CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, flags | CREATE_BREAKAWAY_FROM_JOB,
                             nullptr, folder.c_str(), &si, &pi);
    if (!ok && GetLastError() == ERROR_ACCESS_DENIED) {
        ok = CreateProcessW(nullptr, &command[0], nullptr, nullptr, FALSE, flags, nullptr, folder.c_str(), &si, &pi);
    }
    if (!ok) {
        std::wcout << L"start: DS5-USBIP did not start, error " << GetLastError() << L"\n";
        return 0;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    std::wcout << L"start: DS5-USBIP started with no window, or left as it was if it was running\n";
    return 0;
}

// ⭐ STOP: the running listener asked to quit, the way its tray menu's Quit
// does (quit_request.inl), so its bridges are released, and given ten
// seconds to go. Nothing running is nothing to do.
inline int stop()
{
    HWND tray = FindWindowW(quit_request::kTrayClass, nullptr);
    if (tray == nullptr) {
        std::wcout << L"stop: DS5-USBIP is not running\n";
        return 0;
    }
    DWORD pid = 0;
    GetWindowThreadProcessId(tray, &pid);
    HANDLE process = (pid != 0) ? OpenProcess(SYNCHRONIZE, FALSE, pid) : nullptr;
    PostMessageW(tray, RegisterWindowMessageW(quit_request::kMessageName), 0, 0);
    const bool gone = process != nullptr && WaitForSingleObject(process, 10000) == WAIT_OBJECT_0;
    if (process != nullptr) CloseHandle(process);
    std::wcout << (gone ? L"stop: DS5-USBIP quit\n" : L"stop: DS5-USBIP was asked to quit\n");
    return 0;
}

// The installer's: our entry put in Vibepollo's list, or taken out of it.
//     DS5-USBIP stream-commands add|remove <sunshine.conf>
// ⚠️ installer/ds5-usbip.iss reads these endings: 0 when the file already said
// so, 10 when it was changed (the file as it was is kept beside it, as
// <file>.before-ds5-usbip), 2 for a wrong command, 3 when the file could not
// be read, 4 when its list is not one this understands (then it is left
// alone), 5 when it could not be written.
inline int edit_commands(int argc, wchar_t **argv)
{
    const std::wstring verb = argc > 2 ? argv[2] : L"";
    if (argc != 4 || (verb != L"add" && verb != L"remove")) {
        std::wcout << L"usage: DS5-USBIP stream-commands add|remove <sunshine.conf>\n";
        return 2;
    }
    const std::wstring path = argv[3];
    std::string conf;
    {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            std::wcout << L"stream-commands: could not read " << path << L"\n";
            return 3;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
        conf = buffer.str();
    }
    std::string changed;
    const stream_commands::Result result = verb == L"add"
        ? stream_commands::with_ours(conf, utf8_of(own_path()), &changed)
        : stream_commands::without_ours(conf, &changed);
    if (result == stream_commands::Result::Unchanged) {
        std::wcout << L"stream-commands: " << path << L" already says so\n";
        return 0;
    }
    if (result == stream_commands::Result::NotUnderstood) {
        std::wcout << L"stream-commands: the global_prep_cmd in " << path
                   << L" is not a list this understands, so it was left alone\n";
        return 4;
    }
    {
        std::ofstream kept(path + L".before-ds5-usbip", std::ios::binary | std::ios::trunc);
        kept.write(conf.data(), static_cast<std::streamsize>(conf.size()));
        if (!kept) {
            std::wcout << L"stream-commands: could not keep a copy of " << path << L", so it was left alone\n";
            return 5;
        }
    }
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(changed.data(), static_cast<std::streamsize>(changed.size()));
        if (!out) {
            std::wcout << L"stream-commands: could not write " << path << L"\n";
            return 5;
        }
    }
    std::wcout << L"stream-commands: " << verb << L" done in " << path << L"\n";
    return 10;
}

}  // namespace stream_start
