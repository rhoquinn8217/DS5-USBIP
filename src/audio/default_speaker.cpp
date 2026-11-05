// Windows' default speaker, taken back from a controller. default_speaker.h
// says what this is for; default_speaker_rule.inl decides when.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// ⓘ First, as in pc_sound_capture.cpp, so the property keys the headers below
// declare are defined in this unit too; they are "select any", so two units
// defining them is no clash.
#include <initguid.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <mutex>
#include <string>
#include <thread>

#include "default_speaker.h"
#include "controller_sound.inl"
#include "default_speaker_rule.inl"

#pragma comment(lib, "ole32.lib")

using Microsoft::WRL::ComPtr;

namespace {

// ⓘ Windows publishes no call that chooses the default sound device. This is
// the one its own Sound settings use, called the same way by EarTrumpet and
// SoundSwitch since Windows 7. Only the order of its slots matters here: ten
// not used (GetMixFormat, GetDeviceFormat, ResetDeviceFormat, SetDeviceFormat,
// GetProcessingPeriod, SetProcessingPeriod, GetShareMode, SetShareMode,
// GetPropertyValue, SetPropertyValue), then SetDefaultEndpoint.
MIDL_INTERFACE("f8679f50-850a-41cf-9c72-430f290290c8")
IPolicyConfig : public IUnknown
{
public:
    virtual HRESULT STDMETHODCALLTYPE Unused1() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused2() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused3() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused4() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused5() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused6() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused7() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused8() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused9() = 0;
    virtual HRESULT STDMETHODCALLTYPE Unused10() = 0;
    virtual HRESULT STDMETHODCALLTYPE SetDefaultEndpoint(PCWSTR deviceId, ERole role) = 0;
};
class DECLSPEC_UUID("870af99c-171d-4f9e-af0d-e63df40c2bc9") CPolicyConfigClient;

uint64_t now_ms()
{
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

struct Event {
    enum Kind { Arrived, Left, Default, Stop };
    Kind kind;
    int role;
    std::wstring id;
    uint64_t atMs;
};

// ⓘ Never freed: its thread may still be waiting on it as the program ends,
// and a destructor run under a waiting thread is a crash at exit.
struct State {
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable done;
    std::deque<Event> events;
    bool started = false;
    bool finished = false;
};

State &state()
{
    static State *s = new State();
    return *s;
}

void post(Event::Kind kind, int role, LPCWSTR id)
{
    State &s = state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.events.push_back(Event{ kind, role, id != nullptr ? std::wstring(id) : std::wstring(), now_ms() });
    }
    s.wake.notify_one();
}

// Windows' word of a change, put in the queue and nothing more: calling back
// into Windows' sound API from in here can deadlock.
class Listener final : public IMMNotificationClient {
public:
    ULONG STDMETHODCALLTYPE AddRef() override { return ++refs_; }

    ULONG STDMETHODCALLTYPE Release() override
    {
        const ULONG n = --refs_;
        if (n == 0) delete this;
        return n;
    }

    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void **out) override
    {
        if (out == nullptr) return E_POINTER;
        if (IsEqualIID(riid, __uuidof(IUnknown)) || IsEqualIID(riid, __uuidof(IMMNotificationClient))) {
            *out = static_cast<IMMNotificationClient *>(this);
            AddRef();
            return S_OK;
        }
        *out = nullptr;
        return E_NOINTERFACE;
    }

    HRESULT STDMETHODCALLTYPE OnDefaultDeviceChanged(EDataFlow flow, ERole role, LPCWSTR id) override
    {
        if (flow == eRender && id != nullptr) post(Event::Default, static_cast<int>(role), id);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceAdded(LPCWSTR id) override
    {
        post(Event::Arrived, 0, id);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceRemoved(LPCWSTR id) override
    {
        post(Event::Left, 0, id);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnDeviceStateChanged(LPCWSTR id, DWORD newState) override
    {
        post(newState == DEVICE_STATE_ACTIVE ? Event::Arrived : Event::Left, 0, id);
        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE OnPropertyValueChanged(LPCWSTR, const PROPERTYKEY) override { return S_OK; }

private:
    std::atomic<ULONG> refs_{1};
};

std::wstring id_of(IMMDevice *device)
{
    LPWSTR id = nullptr;
    if (FAILED(device->GetId(&id)) || id == nullptr) return std::wstring();
    std::wstring out(id);
    CoTaskMemFree(id);
    return out;
}

std::wstring name_of(IMMDevice *device)
{
    ComPtr<IPropertyStore> store;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &store))) return std::wstring();
    PROPVARIANT value;
    PropVariantInit(&value);
    std::wstring out;
    if (SUCCEEDED(store->GetValue(PKEY_Device_FriendlyName, &value)) && value.vt == VT_LPWSTR && value.pwszVal != nullptr) {
        out = value.pwszVal;
    }
    PropVariantClear(&value);
    return out;
}

// A speaker's name. Empty for a device that is gone, or one that records.
std::wstring speaker_name(IMMDeviceEnumerator *enumerator, const std::wstring &id)
{
    if (id.empty()) return std::wstring();
    ComPtr<IMMDevice> device;
    if (FAILED(enumerator->GetDevice(id.c_str(), &device))) return std::wstring();
    ComPtr<IMMEndpoint> endpoint;
    EDataFlow flow = eCapture;
    if (FAILED(device.As(&endpoint)) || FAILED(endpoint->GetDataFlow(&flow)) || flow != eRender) return std::wstring();
    return name_of(device.Get());
}

// Whether a sound device is connected now.
bool is_connected(IMMDeviceEnumerator *enumerator, const std::wstring &id)
{
    ComPtr<IMMDevice> device;
    DWORD state = 0;
    return SUCCEEDED(enumerator->GetDevice(id.c_str(), &device)) && SUCCEEDED(device->GetState(&state)) && state == DEVICE_STATE_ACTIVE;
}

const wchar_t *role_name(int role)
{
    return role == eConsole ? L"console" : role == eMultimedia ? L"multimedia" : L"communications";
}

std::wstring hex(HRESULT hr)
{
    wchar_t text[16];
    swprintf_s(text, L"0x%08lX", static_cast<unsigned long>(hr));
    return text;
}

void watch(default_speaker::Log log)
{
    State &s = state();
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        ComPtr<IPolicyConfig> policy;
        Listener *listener = nullptr;
        bool registered = false;
        default_speaker_rule::Rule rule;
        if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))) &&
            SUCCEEDED(CoCreateInstance(__uuidof(CPolicyConfigClient), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&policy)))) {
            // What is there already: a controller's speaker, left alone, and
            // the default to keep for each role.
            ComPtr<IMMDeviceCollection> all;
            if (SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &all))) {
                UINT count = 0;
                all->GetCount(&count);
                for (UINT i = 0; i < count; ++i) {
                    ComPtr<IMMDevice> device;
                    if (SUCCEEDED(all->Item(i, &device)) && controller_sound::is_controller(name_of(device.Get()))) {
                        rule.present_before(id_of(device.Get()));
                    }
                }
            }
            for (int role = 0; role < default_speaker_rule::kRoles; ++role) {
                ComPtr<IMMDevice> device;
                if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, static_cast<ERole>(role), &device))) {
                    rule.default_changed(role, id_of(device.Get()), controller_sound::is_controller(name_of(device.Get())), now_ms());
                }
            }
            listener = new Listener();
            registered = SUCCEEDED(enumerator->RegisterEndpointNotificationCallback(listener));
        }
        if (!registered) {
            log(L"default speaker: Windows' sound devices could not be watched, so a controller given the default speaker keeps it");
        } else {
            const std::wstring kept = speaker_name(enumerator.Get(), rule.kept(eConsole));
            log(L"default speaker: watching; a controller Windows makes the default as it arrives gives it back to " +
                (kept.empty() ? std::wstring(L"the speaker chosen before it") : kept));
        }
        for (bool run = registered; run;) {
            Event e;
            {
                std::unique_lock<std::mutex> lock(s.mutex);
                s.wake.wait(lock, [&s] { return !s.events.empty(); });
                e = std::move(s.events.front());
                s.events.pop_front();
            }
            if (e.kind == Event::Stop) {
                run = false;
            } else if (e.kind == Event::Left) {
                rule.left(e.id);
            } else if (e.kind == Event::Arrived) {
                if (controller_sound::is_controller(speaker_name(enumerator.Get(), e.id))) rule.arrived(e.id, e.atMs);
            } else {
                const std::wstring name = speaker_name(enumerator.Get(), e.id);
                // ⓘ Whether a pad a person chose is still there, read now: when
                // it has gone, a speaker that is not a controller taking the
                // default is Windows' doing, not their change of mind.
                const std::wstring chosen = rule.chosen(e.role);
                const bool chosenThere = chosen.empty() || is_connected(enumerator.Get(), chosen);
                const std::wstring back = rule.default_changed(e.role, e.id, controller_sound::is_controller(name), e.atMs, chosenThere);
                if (!back.empty()) {
                    const std::wstring backName = speaker_name(enumerator.Get(), back);
                    const HRESULT hr = policy->SetDefaultEndpoint(back.c_str(), static_cast<ERole>(e.role));
                    if (SUCCEEDED(hr)) {
                        log(L"default speaker: Windows gave the " + std::wstring(role_name(e.role)) + L" default to " + name +
                            L" as it arrived; " + backName + L" has it again");
                    } else {
                        log(L"default speaker: Windows gave the " + std::wstring(role_name(e.role)) + L" default to " + name +
                            L" as it arrived, and " + backName + L" could not have it back (" + hex(hr) + L")");
                    }
                }
            }
        }
        if (registered) enumerator->UnregisterEndpointNotificationCallback(listener);
        if (listener != nullptr) listener->Release();
    }
    if (SUCCEEDED(com)) CoUninitialize();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        s.finished = true;
    }
    s.done.notify_all();
}

}  // namespace

namespace default_speaker {

void start(Log log)
{
    State &s = state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (s.started) return;
        s.started = true;
    }
    // ⓘ Detached, and stop() waits for it rather than joining: a thread still
    // joinable as the program ends stops it with abort().
    std::thread(watch, std::move(log)).detach();
}

void stop()
{
    State &s = state();
    {
        std::lock_guard<std::mutex> lock(s.mutex);
        if (!s.started || s.finished) return;
    }
    post(Event::Stop, 0, nullptr);
    std::unique_lock<std::mutex> lock(s.mutex);
    s.done.wait_for(lock, std::chrono::seconds(2), [&s] { return s.finished; });
}

}  // namespace default_speaker
