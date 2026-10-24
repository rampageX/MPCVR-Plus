// Audio connection formats, read only on the telemetry worker.
#pragma once
#include <mmreg.h>
#include "AudioSelection.h"
#include "AudioDepth.h"

// MPC-BE AudioSwitcher/StreamSwitcher.h: read-only active-input identity.
interface __declspec(uuid("DA395FA3-4A3E-4D85-805E-0BEFF53D4BCD")) IOsdStreamSwitcherInput : public IUnknown {
    STDMETHOD_(bool, IsActive)() PURE;
};

class COsdAudioInfo {
    CComPtr<IPin> m_rendererInput;
    static std::wstring Format(const CMediaType& mt, DWORD current = 0) {
        if (mt.formattype != FORMAT_WaveFormatEx || !mt.pbFormat || mt.cbFormat < sizeof(WAVEFORMATEX)) return L"N/A";
        const auto* w = reinterpret_cast<const WAVEFORMATEX*>(mt.pbFormat);
        UINT tag = w->wFormatTag, bits = w->wBitsPerSample;
        if (tag == WAVE_FORMAT_EXTENSIBLE && mt.cbFormat >= sizeof(WAVEFORMATEXTENSIBLE) && w->cbSize >= 22) {
            const auto* ex = reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(w);
            tag = ex->SubFormat.Data1;
            if (ex->Samples.wValidBitsPerSample && ex->Samples.wValidBitsPerSample <= bits) bits = ex->Samples.wValidBitsPerSample;
        }
        std::wstring codec;
        // Windows SDK wmcodecdsp.h subtypes. Some splitters use wFormatTag=0
        // for these codecs, so the complete media subtype must take precedence.
        static constexpr GUID ddplus = {0xa7fb87af,0x2d02,0x42fb,{0xa4,0xd4,0x05,0xcd,0x93,0x84,0x3b,0xdd}};
        static constexpr GUID truehd = {0xeb27cec4,0x163e,0x4ca3,{0x8b,0x74,0x8e,0x25,0xf9,0x1b,0x51,0x7e}};
        if (mt.subtype == ddplus) { codec = L"E-AC-3 (Dolby Digital Plus)"; tag = 0; }
        else if (mt.subtype == MEDIASUBTYPE_DVD_LPCM_AUDIO) { codec = L"DVD LPCM"; }
        else if (mt.subtype == truehd) { codec = L"Dolby TrueHD"; }
        if (codec.empty()) {
        switch (tag) {
        case 1: codec = L"PCM"; break;
        case 3: codec = L"Float PCM"; break;
        case 0x50: codec = L"MPEG Audio"; break;
        case 0x55: codec = L"MP3"; break;
        case 0xff: case 0x1600: case 0x1610: codec = L"AAC"; break;
        case 0x160: case 0x161: case 0x162: case 0x163: codec = L"WMA"; break;
        case 0x2000: codec = L"AC-3"; break;
        case 0x2001: codec = L"DTS"; break;
        case 0x92: codec = L"AC-3 S/PDIF"; break;
        case 0xf1ac: codec = L"FLAC"; break;
        case 0x704f: codec = L"Opus"; break;
        default: codec = std::format(L"Audio 0x{:X}", tag); break;
        }
        }
        // FLAC STREAMINFO may carry source precision omitted from WAVEFORMATEX.
        // Read only bounded codec-private data already supplied by the graph.
        if (tag == 0xf1ac && bits == 0 && w->cbSize <= mt.cbFormat - sizeof(WAVEFORMATEX)) {
            const BYTE* p = mt.pbFormat + sizeof(WAVEFORMATEX);
            UINT n = w->cbSize;
            bool marked = false;
            if (n >= 4 && p[0] == 'f' && p[1] == 'L' && p[2] == 'a' && p[3] == 'C') {
                p += 4;
                n -= 4;
                marked = true;
            }
            // Accept a STREAMINFO metadata header or an exact raw payload.
            if (n >= 38 && (p[0] & 0x7f) == 0 && p[1] == 0 && p[2] == 0 && p[3] == 34) {
                p += 4;
                n = 34;
            } else if (marked || n != 34) {
                n = 0;
            }
            if (n == 34) {
                const UINT minBlock = (UINT(p[0]) << 8) | p[1];
                const UINT maxBlock = (UINT(p[2]) << 8) | p[3];
                const UINT sampleRate = (UINT(p[10]) << 12) | (UINT(p[11]) << 4) | (p[12] >> 4);
                const UINT channels = ((p[12] >> 1) & 7) + 1;
                const UINT sourceBits = (((UINT(p[12]) & 1) << 4) | (p[13] >> 4)) + 1;
                if (minBlock >= 16 && maxBlock >= minBlock && sourceBits >= 4 && sourceBits <= 32
                    && sampleRate != 0 && sampleRate == w->nSamplesPerSec && channels == w->nChannels) {
                    bits = sourceBits;
                }
            }
        }
        bits = OsdAudioDepth(tag, bits);
        const double rate = current ? current / 1000.0 : w->nAvgBytesPerSec * 8.0 / 1000.0;
        return std::format(L"{}, {} Hz, {} ch, {}, {}", codec, w->nSamplesPerSec, w->nChannels,
            bits ? std::format(L"{}-bit", bits) : L"depth N/A",
            rate > 0 ? std::format(L"{:.0f} kb/s", rate) : L"rate N/A");
    }
    static bool IsPcm(const CMediaType& mt) {
        if (mt.formattype != FORMAT_WaveFormatEx || !mt.pbFormat || mt.cbFormat < sizeof(WAVEFORMATEX)) return false;
        const auto* w = reinterpret_cast<const WAVEFORMATEX*>(mt.pbFormat);
        const UINT tag = w->wFormatTag == WAVE_FORMAT_EXTENSIBLE && mt.cbFormat >= sizeof(WAVEFORMATEXTENSIBLE)
            ? reinterpret_cast<const WAVEFORMATEXTENSIBLE*>(w)->SubFormat.Data1 : w->wFormatTag;
        return tag == 1 || tag == 3;
    }
public:
    void Reset() { m_rendererInput.Release(); }
    void Find(IFilterGraph* graph) {
        Reset();
        CComPtr<IEnumFilters> filters;
        if (!graph || FAILED(graph->EnumFilters(&filters))) return;
        CComPtr<IBaseFilter> filter;
        while (filters->Next(1, &filter, nullptr) == S_OK) {
            CComPtr<IEnumPins> pins;
            if (SUCCEEDED(filter->EnumPins(&pins))) {
                CComPtr<IPin> pin, audio;
                bool downstream = false;
                while (pins->Next(1, &pin, nullptr) == S_OK) {
                    PIN_DIRECTION direction;
                    CMediaType mt;
                    if (SUCCEEDED(pin->QueryDirection(&direction)) && SUCCEEDED(pin->ConnectionMediaType(&mt)) && mt.majortype == MEDIATYPE_Audio) {
                        if (direction == PINDIR_INPUT) audio = pin;
                        else downstream = true;
                    }
                    pin.Release();
                }
                if (audio && !downstream) { m_rendererInput = audio; return; }
            }
            filter.Release();
        }
    }
    std::wstring Text() {
        std::wstring input = L"N/A", decoded = L"N/A", render = L"N/A";
        CMediaType mt;
        if (m_rendererInput && SUCCEEDED(m_rendererInput->ConnectionMediaType(&mt))) render = Format(mt);
        CComPtr<IPin> output;
        if (m_rendererInput) m_rendererInput->ConnectedTo(&output);
        // Follow only connected audio inputs, starting at the actual renderer.
        for (unsigned depth = 0; output && depth < 16; ++depth) {
            CMediaType outType;
            output->ConnectionMediaType(&outType);
            PIN_INFO info = {};
            if (FAILED(output->QueryPinInfo(&info)) || !info.pFilter) break;
            CComPtr<IBaseFilter> filter; filter.Attach(info.pFilter);
            CComPtr<IEnumPins> pins;
            if (FAILED(filter->EnumPins(&pins))) break;
            CComPtr<IPin> pin, upstream;
            CMediaType inType;
            std::vector<CComPtr<IPin>> upstreamPins;
            std::vector<CMediaType> inputTypes;
            std::vector<int> states;
            while (pins->Next(1, &pin, nullptr) == S_OK) {
                PIN_DIRECTION direction;
                CMediaType candidate;
                if (SUCCEEDED(pin->QueryDirection(&direction)) && direction == PINDIR_INPUT
                    && SUCCEEDED(pin->ConnectionMediaType(&candidate)) && candidate.majortype == MEDIATYPE_Audio) {
                    CComPtr<IPin> connected;
                    if (SUCCEEDED(pin->ConnectedTo(&connected))) {
                        CComPtr<IOsdStreamSwitcherInput> selection;
                        pin->QueryInterface(IID_PPV_ARGS(&selection));
                        states.push_back(selection ? (selection->IsActive() ? 1 : 0) : -1);
                        upstreamPins.push_back(connected);
                        inputTypes.push_back(candidate);
                    }
                }
                pin.Release();
            }
            if (states.empty()) { if (IsPcm(outType)) input = decoded = Format(outType); break; }
            const int active = SelectOsdAudioInput(states);
            if (active < 0) break;
            upstream = upstreamPins[active];
            inType = inputTypes[active];
            if (!IsPcm(inType)) {
                DWORD rate = 0;
                CComPtr<IOsdBitRateInfo> bitrate;
                if (upstream) upstream->QueryInterface(IID_PPV_ARGS(&bitrate));
                if (bitrate) rate = bitrate->GetCurrentBitRate();
                input = Format(inType, rate);
                decoded = Format(outType);
                break;
            }
            output = upstream;
        }
        return L"\n-- Audio -----------------------------------------------------------------------\nAudio input   : " + input
            + L"\nAudio decoded : " + decoded + L"\nAudio render  : " + render;
    }
};
