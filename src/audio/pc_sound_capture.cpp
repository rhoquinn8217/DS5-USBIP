// The PC's sound as it plays, and the silent stream that keeps a pad's own
// sound open. pc_sound_capture.h says what each is for, and why this is a
// translation unit of its own.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
// ⓘ First, so the property keys and format ids the headers below declare are
// defined here, in this one unit.
#include <initguid.h>
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <functiondiscoverykeys_devpkey.h>
#include <mmreg.h>
#include <ks.h>
#include <ksmedia.h>
#include <cfgmgr32.h>
#include <devpkey.h>
#include <wrl/client.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "pc_sound_capture.h"

#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "cfgmgr32.lib")

using Microsoft::WRL::ComPtr;

namespace pc_sound_capture {
namespace {

std::mutex g_lifecycle;
std::atomic<bool> g_run{false};
std::thread g_thread;

// Sleeps in small steps, so stop() is never kept waiting.
void wait_ms(int ms)
{
    for (int waited = 0; waited < ms && g_run.load(); waited += 50) Sleep(50);
}

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

bool container_of(IMMDevice *device, GUID *out)
{
    ComPtr<IPropertyStore> store;
    if (FAILED(device->OpenPropertyStore(STGM_READ, &store))) return false;
    PROPVARIANT value;
    PropVariantInit(&value);
    bool found = false;
    if (SUCCEEDED(store->GetValue(PKEY_Device_ContainerId, &value)) && value.vt == VT_CLSID && value.puuid != nullptr) {
        *out = *value.puuid;
        found = true;
    }
    PropVariantClear(&value);
    return found;
}

// A controller's own sound device, by the name Windows gives it: a
// DualSense's is "DualSense Wireless Controller", an Edge's "DualSense Edge
// Wireless Controller", a DS4's "Wireless Controller".
bool is_controller(const std::wstring &name)
{
    return name.find(L"DualSense") != std::wstring::npos || name.find(L"Wireless Controller") != std::wstring::npos;
}

struct Format {
    unsigned channels = 0;
    unsigned rate = 0;
    unsigned bits = 0;
    bool isFloat = false;
    int centre = -1;
    int lfe = -1;
};

Format read_format(const WAVEFORMATEX *wf)
{
    Format f;
    f.channels = wf->nChannels;
    f.rate = wf->nSamplesPerSec;
    f.bits = wf->wBitsPerSample;
    f.isFloat = wf->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
    DWORD mask = 0;
    if (wf->wFormatTag == WAVE_FORMAT_EXTENSIBLE && wf->cbSize >= 22) {
        const WAVEFORMATEXTENSIBLE *ext = reinterpret_cast<const WAVEFORMATEXTENSIBLE *>(wf);
        f.isFloat = IsEqualGUID(ext->SubFormat, KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) != 0;
        mask = ext->dwChannelMask;
    }
    // A channel's place is the number of lower speaker bits the mask has set.
    auto place = [mask](DWORD bit) -> int {
        if ((mask & bit) == 0) return -1;
        DWORD below = mask & (bit - 1);
        int n = 0;
        for (; below != 0; below >>= 1) n += static_cast<int>(below & 1u);
        return n;
    };
    f.centre = place(SPEAKER_FRONT_CENTER);
    f.lfe = place(SPEAKER_LOW_FREQUENCY);
    return f;
}

// One packet as floats, whatever Windows mixed it as.
void to_float(const BYTE *data, UINT32 frames, const Format &f, bool silent, std::vector<float> *out)
{
    const size_t n = static_cast<size_t>(frames) * f.channels;
    out->assign(n, 0.0f);
    if (silent || data == nullptr) return;
    if (f.isFloat && f.bits == 32) {
        std::memcpy(out->data(), data, n * sizeof(float));
    } else if (f.bits == 16) {
        const int16_t *s = reinterpret_cast<const int16_t *>(data);
        for (size_t i = 0; i < n; ++i) (*out)[i] = s[i] / 32768.0f;
    } else if (f.bits == 32) {
        const int32_t *s = reinterpret_cast<const int32_t *>(data);
        for (size_t i = 0; i < n; ++i) (*out)[i] = static_cast<float>(s[i] / 2147483648.0);
    } else if (f.bits == 24) {
        for (size_t i = 0; i < n; ++i) {
            const BYTE *p = data + i * 3;
            const int32_t v = static_cast<int32_t>((static_cast<uint32_t>(p[0]) << 8) | (static_cast<uint32_t>(p[1]) << 16) |
                                                   (static_cast<uint32_t>(p[2]) << 24));
            (*out)[i] = static_cast<float>(v / 2147483648.0);
        }
    }
}

void capture_loop(Sink sink, Log log)
{
    const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    {
        ComPtr<IMMDeviceEnumerator> enumerator;
        if (FAILED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator)))) {
            log(L"audio haptics: Windows' sound devices could not be reached, so nothing is captured");
            g_run = false;
        }
        std::wstring refused;
        while (g_run.load()) {
            ComPtr<IMMDevice> device;
            if (FAILED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device))) {
                wait_ms(1000);
                continue;
            }
            const std::wstring id = id_of(device.Get());
            const std::wstring name = name_of(device.Get());
            if (is_controller(name)) {
                if (id != refused) {
                    log(L"audio haptics: the PC's sound plays on a controller (" + name +
                        L"), so it is not captured: its own vibration would come back into it");
                    refused = id;
                }
                wait_ms(1000);
                continue;
            }
            refused.clear();
            ComPtr<IAudioClient> client;
            WAVEFORMATEX *mix = nullptr;
            if (FAILED(device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(client.GetAddressOf()))) ||
                FAILED(client->GetMixFormat(&mix)) ||
                FAILED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 1000000, 0, mix, nullptr))) {
                if (mix != nullptr) CoTaskMemFree(mix);
                log(L"audio haptics: " + name + L" could not be captured; trying again");
                wait_ms(3000);
                continue;
            }
            const Format format = read_format(mix);
            CoTaskMemFree(mix);
            ComPtr<IAudioCaptureClient> capture;
            if (FAILED(client->GetService(IID_PPV_ARGS(&capture))) || FAILED(client->Start())) {
                wait_ms(3000);
                continue;
            }
            log(L"audio haptics: capturing " + name + L", " + std::to_wstring(format.rate) + L" Hz, " +
                std::to_wstring(format.channels) + L" channels" + (format.isFloat ? L", float" : L""));
            std::vector<float> samples;
            auto lastCheck = std::chrono::steady_clock::now();
            bool reopen = false;
            while (g_run.load() && !reopen) {
                Sleep(5);
                UINT32 packet = 0;
                HRESULT hr = S_OK;
                while (SUCCEEDED(hr = capture->GetNextPacketSize(&packet)) && packet > 0) {
                    BYTE *data = nullptr;
                    UINT32 frames = 0;
                    DWORD flags = 0;
                    if (FAILED(capture->GetBuffer(&data, &frames, &flags, nullptr, nullptr))) break;
                    to_float(data, frames, format, (flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0, &samples);
                    capture->ReleaseBuffer(frames);
                    if (frames > 0) {
                        sink(Block{ samples.data(), frames, format.channels, format.rate, format.centre, format.lfe });
                    }
                }
                if (hr == AUDCLNT_E_DEVICE_INVALIDATED) reopen = true;
                // ⭐ The default output can change under it (a headset plugged
                // in, the streaming host's speakers coming and going): looked at
                // once a second, and followed.
                if (std::chrono::steady_clock::now() - lastCheck > std::chrono::seconds(1)) {
                    lastCheck = std::chrono::steady_clock::now();
                    ComPtr<IMMDevice> current;
                    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &current)) && id_of(current.Get()) != id) {
                        reopen = true;
                    }
                }
            }
            client->Stop();
        }
    }
    if (SUCCEEDED(com)) CoUninitialize();
}

struct KeepAlive {
    std::atomic<bool> run{true};
    std::thread thread;
};

}  // namespace

void start(Sink sink, Log log)
{
    std::lock_guard<std::mutex> lock(g_lifecycle);
    if (g_run.load()) return;
    if (g_thread.joinable()) g_thread.join();
    g_run = true;
    g_thread = std::thread(capture_loop, std::move(sink), std::move(log));
}

void stop()
{
    std::lock_guard<std::mutex> lock(g_lifecycle);
    g_run = false;
    if (g_thread.joinable()) g_thread.join();
}

bool running()
{
    return g_run.load();
}

void *keep_alive_open(const std::wstring &usbInstanceId, Log log)
{
    // The pad's container: every part of one device in Windows shares it, its
    // sound devices included.
    GUID container = {};
    {
        std::wstring id = usbInstanceId;
        DEVINST node = 0;
        DEVPROPTYPE type = 0;
        ULONG size = sizeof container;
        if (CM_Locate_DevNodeW(&node, id.data(), CM_LOCATE_DEVNODE_NORMAL) != CR_SUCCESS ||
            CM_Get_DevNode_PropertyW(node, &DEVPKEY_Device_ContainerId, &type, reinterpret_cast<PBYTE>(&container), &size, 0) != CR_SUCCESS ||
            type != DEVPROP_TYPE_GUID) {
            log(L"audio haptics: " + usbInstanceId + L" is not in Windows yet; its keep-alive waits");
            return nullptr;
        }
    }
    KeepAlive *alive = new KeepAlive();
    alive->thread = std::thread([alive, container, log, usbInstanceId]() {
        const HRESULT com = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        {
            ComPtr<IMMDeviceEnumerator> enumerator;
            ComPtr<IMMDeviceCollection> all;
            ComPtr<IMMDevice> pad;
            if (SUCCEEDED(CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator))) &&
                SUCCEEDED(enumerator->EnumAudioEndpoints(eRender, DEVICE_STATE_ACTIVE, &all))) {
                UINT count = 0;
                all->GetCount(&count);
                for (UINT i = 0; i < count && !pad; ++i) {
                    ComPtr<IMMDevice> candidate;
                    GUID c = {};
                    if (SUCCEEDED(all->Item(i, &candidate)) && container_of(candidate.Get(), &c) && IsEqualGUID(c, container)) {
                        pad = candidate;
                    }
                }
            }
            ComPtr<IAudioClient> client;
            ComPtr<IAudioRenderClient> render;
            WAVEFORMATEX *mix = nullptr;
            UINT32 bufferFrames = 0;
            bool ok = pad &&
                      SUCCEEDED(pad->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(client.GetAddressOf()))) &&
                      SUCCEEDED(client->GetMixFormat(&mix)) &&
                      SUCCEEDED(client->Initialize(AUDCLNT_SHAREMODE_SHARED, 0, 1000000, 0, mix, nullptr)) &&
                      SUCCEEDED(client->GetBufferSize(&bufferFrames)) &&
                      SUCCEEDED(client->GetService(IID_PPV_ARGS(&render)));
            if (mix != nullptr) CoTaskMemFree(mix);
            if (ok) {
                BYTE *buffer = nullptr;
                if (SUCCEEDED(render->GetBuffer(bufferFrames, &buffer))) render->ReleaseBuffer(bufferFrames, AUDCLNT_BUFFERFLAGS_SILENT);
                ok = SUCCEEDED(client->Start());
            }
            if (!ok) {
                log(L"audio haptics: no silent stream on " + usbInstanceId + L"'s sound device" +
                    (pad ? L"" : L": not among Windows' sound devices"));
            } else {
                log(L"audio haptics: keep-alive open on " + name_of(pad.Get()));
                while (alive->run.load()) {
                    Sleep(20);
                    UINT32 padding = 0;
                    if (FAILED(client->GetCurrentPadding(&padding))) break;
                    const UINT32 room = bufferFrames - padding;
                    BYTE *buffer = nullptr;
                    if (room > 0 && SUCCEEDED(render->GetBuffer(room, &buffer))) render->ReleaseBuffer(room, AUDCLNT_BUFFERFLAGS_SILENT);
                }
                client->Stop();
            }
        }
        if (SUCCEEDED(com)) CoUninitialize();
    });
    return alive;
}

void keep_alive_close(void *handle)
{
    if (handle == nullptr) return;
    KeepAlive *alive = static_cast<KeepAlive *>(handle);
    alive->run = false;
    if (alive->thread.joinable()) alive->thread.join();
    delete alive;
}

}  // namespace pc_sound_capture
