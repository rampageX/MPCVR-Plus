// Background-only OSD sampling. Kept outside upstream's rendering implementation.
#pragma once
#include <pdh.h>
#include <dxgi1_4.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <map>
#include <atomic>
#pragma comment(lib, "pdh.lib")

// MPC-HC/MPC-BE splitter output pin statistics (include/IBitRateInfo.h).
interface __declspec(uuid("EB2CD9E6-BA08-4acb-AA0F-3D8D0DD521CA")) IOsdBitRateInfo : public IUnknown {
    STDMETHOD_(DWORD, GetCurrentBitRate)() PURE;
    STDMETHOD_(DWORD, GetAverageBitRate)() PURE;
};

class COsdTelemetry {
    std::thread m_worker;
    std::mutex m_mutex;
    std::condition_variable m_wake;
    bool m_stop = false;
    std::wstring m_text;
    std::atomic<ULONGLONG> m_lastDraw = 0;
    static ULONGLONG Ticks(FILETIME t) { return (ULONGLONG(t.dwHighDateTime) << 32) | t.dwLowDateTime; }

    // Sum processes on each physical engine, then take the busiest engine.
    // Adding independent engines would incorrectly exceed Task Manager's GPU usage.
    static double EngineUsage(PDH_HCOUNTER counter, const std::wstring& luid, bool decode) {
        DWORD bytes = 0, count = 0;
        if (!counter || PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &count, nullptr) != PDH_MORE_DATA) return -1;
        std::vector<BYTE> buffer(bytes);
        auto items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(buffer.data());
        if (PdhGetFormattedCounterArrayW(counter, PDH_FMT_DOUBLE, &bytes, &count, items) != ERROR_SUCCESS) return -1;
        std::map<std::wstring, double> engines;
        for (DWORD i = 0; i < count; ++i) {
            const std::wstring name(items[i].szName);
            if (name.find(luid) == std::wstring::npos || (decode && name.find(L"engtype_VideoDecode") == std::wstring::npos)) continue;
            const auto status = items[i].FmtValue.CStatus;
            if (status != PDH_CSTATUS_VALID_DATA && status != PDH_CSTATUS_NEW_DATA) continue;
            const auto engine = name.find(L"_phys_");
            if (engine != std::wstring::npos) engines[name.substr(engine)] += items[i].FmtValue.doubleValue;
        }
        double peak = -1;
        for (const auto& entry : engines) peak = std::max(peak, entry.second);
        return peak < 0 ? -1 : std::clamp(peak, 0.0, 100.0);
    }
    static std::wstring Percent(double x) { return x < 0 ? L"N/A" : std::format(L"{:.1f}%", x); }
    static std::wstring Rate(DWORD x) { return x ? std::format(L"{:.2f} Mb/s", x / 1000000.0) : L"N/A"; }

    void Run(LUID luid, IStream* stream) {
        const HRESULT co = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        CComPtr<IPin> output;
        if (stream) {
            // Always consumes and releases the marshal stream, even on failure.
            CoGetInterfaceAndReleaseStream(stream, IID_PPV_ARGS(&output));
        }
        CComPtr<IOsdBitRateInfo> bitrate;
        std::wstring codec = L"N/A";
        DWORD declaredRate = 0;
        if (output) {
            PIN_INFO info = {};
            if (SUCCEEDED(output->QueryPinInfo(&info)) && info.pFilter) {
                CComPtr<IEnumPins> pins;
                if (SUCCEEDED(info.pFilter->EnumPins(&pins))) {
                    CComPtr<IPin> pin;
                    while (pins->Next(1, &pin, nullptr) == S_OK) {
                        PIN_DIRECTION direction;
                        if (SUCCEEDED(pin->QueryDirection(&direction)) && direction == PINDIR_INPUT) {
                            CMediaType mt;
                            if (SUCCEEDED(pin->ConnectionMediaType(&mt)) && mt.majortype == MEDIATYPE_Video) {
                                if (mt.formattype == FORMAT_VideoInfo && mt.cbFormat >= sizeof(VIDEOINFOHEADER)) {
                                    declaredRate = reinterpret_cast<VIDEOINFOHEADER*>(mt.pbFormat)->dwBitRate;
                                } else if (mt.formattype == FORMAT_VideoInfo2 && mt.cbFormat >= sizeof(VIDEOINFOHEADER2)) {
                                    declaredRate = reinterpret_cast<VIDEOINFOHEADER2*>(mt.pbFormat)->dwBitRate;
                                }
                                const DWORD fourcc = mt.subtype.Data1;
                                std::wstring tag;
                                for (unsigned j = 0; j < 4; ++j) {
                                    const WCHAR c = (fourcc >> (8 * j)) & 255;
                                    if (c < 32 || c > 126) { tag.clear(); break; }
                                    tag += c;
                                }
                                if (!tag.empty()) codec = tag;
                                CComPtr<IPin> compressed;
                                if (SUCCEEDED(pin->ConnectedTo(&compressed))) compressed->QueryInterface(IID_PPV_ARGS(&bitrate));
                                break;
                            }
                        }
                        pin.Release();
                    }
                }
                info.pFilter->Release();
            }
        }
        output.Release();
        CComPtr<IDXGIFactory4> factory;
        CComPtr<IDXGIAdapter3> adapter;
        if (SUCCEEDED(CreateDXGIFactory1(IID_PPV_ARGS(&factory)))) factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(&adapter));
        const std::wstring key = std::format(L"luid_0x{:08x}_0x{:08x}", static_cast<UINT>(luid.HighPart), luid.LowPart);
        PDH_HQUERY query = nullptr;
        PDH_HCOUNTER gpu = nullptr;
        if (PdhOpenQueryW(nullptr, 0, &query) == ERROR_SUCCESS) {
            PdhAddEnglishCounterW(query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &gpu);
            PdhCollectQueryData(query);
        }
        FILETIME idle = {}, kernel = {}, user = {}, creation, exit, pk = {}, pu = {};
        GetSystemTimes(&idle, &kernel, &user);
        GetProcessTimes(GetCurrentProcess(), &creation, &exit, &pk, &pu);
        ULONGLONG prevIdle = Ticks(idle), prevTotal = Ticks(kernel) + Ticks(user), prevProcess = Ticks(pk) + Ticks(pu);
        for (;;) {
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                if (m_wake.wait_for(lock, std::chrono::seconds(1), [&] { return m_stop; })) break;
            }
            // Hide the OSD: suspend expensive driver queries, but keep the worker
            // and its counters ready. Resume with a fresh CPU baseline below.
            if (GetTickCount64() - m_lastDraw.load(std::memory_order_relaxed) > 2000) {
                GetSystemTimes(&idle, &kernel, &user);
                GetProcessTimes(GetCurrentProcess(), &creation, &exit, &pk, &pu);
                prevIdle = Ticks(idle); prevTotal = Ticks(kernel) + Ticks(user); prevProcess = Ticks(pk) + Ticks(pu);
                continue;
            }
            double cpu = -1, process = -1;
            if (GetSystemTimes(&idle, &kernel, &user) && GetProcessTimes(GetCurrentProcess(), &creation, &exit, &pk, &pu)) {
                const auto total = Ticks(kernel) + Ticks(user), busy = Ticks(pk) + Ticks(pu);
                const auto delta = total - prevTotal, idleDelta = Ticks(idle) - prevIdle;
                if (delta && idleDelta <= delta) {
                    cpu = 100.0 * (delta - idleDelta) / delta;
                    process = std::clamp(100.0 * (busy - prevProcess) / delta, 0.0, 100.0);
                }
                prevIdle = Ticks(idle); prevTotal = total; prevProcess = busy;
            }
            double usage = -1, decode = -1;
            if (query && PdhCollectQueryData(query) == ERROR_SUCCESS) {
                usage = EngineUsage(gpu, key, false); decode = EngineUsage(gpu, key, true);
            }
            DXGI_QUERY_VIDEO_MEMORY_INFO memory = {};
            const bool hasMemory = adapter && SUCCEEDED(adapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &memory));
            DWORD average = bitrate ? bitrate->GetAverageBitRate() : 0;
            const DWORD current = bitrate ? bitrate->GetCurrentBitRate() : 0;
            if (!average) average = declaredRate;
            std::wstring text = std::format(L"\nCodec         : {}\nVideo bitrate : avg {}, current {}\n-- Hardware --------------------------------------------------------------------\nCPU           : {} total, {} player\nGPU           : {} busiest engine, decode {}\nVRAM (player) : {}",
                codec, Rate(average), Rate(current), Percent(cpu), Percent(process), Percent(usage), Percent(decode),
                hasMemory ? std::format(L"{:.0f} MiB", memory.CurrentUsage / 1048576.0) : L"N/A");
            std::lock_guard<std::mutex> lock(m_mutex);
            m_text = std::move(text);
        }
        if (query) PdhCloseQuery(query);
        bitrate.Release(); adapter.Release(); factory.Release();
        if (SUCCEEDED(co)) CoUninitialize();
    }
public:
    ~COsdTelemetry() { Stop(); }
    void Stop() {
        { std::lock_guard<std::mutex> lock(m_mutex); m_stop = true; }
        m_wake.notify_all();
        if (m_worker.joinable()) m_worker.join();
    }
    bool Started() const { return m_worker.joinable(); }
    void Start(LUID luid, IPin* output) {
        if (Started()) return;
        IStream* stream = nullptr;
        if (output) CoMarshalInterThreadInterfaceInStream(__uuidof(IPin), output, &stream);
        try { m_worker = std::thread([this, luid, stream] { Run(luid, stream); }); }
        catch (...) {
            if (stream) { LARGE_INTEGER zero = {}; stream->Seek(zero, STREAM_SEEK_SET, nullptr); CoReleaseMarshalData(stream); stream->Release(); }
        }
    }
    std::wstring Text() {
        m_lastDraw.store(GetTickCount64(), std::memory_order_relaxed);
        std::unique_lock<std::mutex> lock(m_mutex, std::try_to_lock);
        return lock.owns_lock() ? m_text : std::wstring();
    }
};
