#include "../Source/OsdPlus/AudioFormats.h"
#include <cassert>
#include <iostream>
#include <random>
#include <vector>
#include <algorithm>

using namespace OsdAudio;
using Bytes = std::vector<std::uint8_t>;
static void Put16(Bytes& b, std::size_t p, unsigned value) { b.at(p)=value; b.at(p+1)=value>>8; }
static void Put32(Bytes& b, std::size_t p, std::uint32_t value) {
    for (unsigned i=0;i<4;++i) b.at(p+i)=static_cast<std::uint8_t>(value>>(i*8));
}
static void PutGuid(Bytes& b, std::size_t p, Guid id) {
    Put32(b,p,id.Data1); Put16(b,p+4,id.Data2); Put16(b,p+6,id.Data3);
    std::copy(id.Data4,id.Data4+8,b.begin()+p+8);
}
static Bytes Wave(unsigned tag, unsigned bits=24, unsigned extra=0) {
    Bytes b(18+extra); Put16(b,0,tag); Put16(b,2,2); Put32(b,4,96000);
    Put32(b,8,576000); Put16(b,12,6); Put16(b,14,bits); Put16(b,16,extra); return b;
}
static Details W(Guid id, const Bytes& b, unsigned current=0) { return Parse(id,WaveFormat,b.data(),b.size(),current); }
static Bytes StreamInfo(unsigned bits=24) {
    Bytes b(34); b[0]=b[2]=0x10;
    const std::uint32_t packed=(96000u<<12)|(1u<<9)|((bits-1)<<4);
    for(unsigned i=0;i<4;++i) b[10+i]=packed>>(24-8*i);
    return b;
}
int main() {
    // Exact subtype identity wins over conflicting WAVE tags.
    const auto wrongPcm=Wave(1);
    for (const auto& example : std::vector<std::pair<Guid,std::wstring_view>>{
        {DdPlus,L"E-AC-3 (Dolby Digital Plus)"},{TrueHd,L"Dolby TrueHD"},
        {DvdLpcm,L"DVD LPCM"},{HdmvLpcm,L"Blu-ray LPCM"},{BdLpcm,L"Blu-ray LPCM"},
        {DtsHd,L"DTS-HD"},{DtsHdHra,L"DTS-HD High Resolution"},
        {DvdGuid(0xe06d802c),L"AC-3"},{DvdGuid(0xe06d8033),L"DTS"},
        {FlacFramed,L"FLAC"},{WaveGuid(0x63616c61),L"ALAC"},
        {WaveGuid(0x20455041),L"APE"},{WaveGuid(0x20504c4d),L"MLP"},
        {WaveGuid(0x5756),L"WavPack"},{WaveGuid(0x77a1),L"TTA"},
        {WaveGuid(0x5355504f),L"Opus"},{WaveGuid(0x704f),L"Opus"}}) {
        auto value=W(example.first,wrongPcm);
        assert(value.Encoding.Name==example.second && !value.LinearPcm);
        assert(value.Rate==96000 && value.Channels==2 && value.Bits==24);
        assert(value.BitRate==4608000);
    }
    // Lookalike Data1 values with different GUID tails must not identify codecs.
    auto impostor=TrueHd; impostor.Data4[7]^=1;
    assert(GuidCodec(impostor).Name.empty());
    auto impostorPcm=WaveGuid(1); impostorPcm.Data4[7]^=1;
    assert(GuidCodec(impostorPcm).Name.empty());
    assert(W(impostor,Wave(0)).Encoding.Name.empty());
    assert(W(impostor,Wave(0)).Identity==impostor);
    // Every basic tag remains usable without a recognized subtype.
    for (const unsigned tag : {1u,3u,0xffu,0x2000u,0x2001u,0xf1acu,0x704fu,0x50u,0x55u})
        assert(!W({},Wave(tag)).Encoding.Name.empty());
    assert(W({},Wave(1)).LinearPcm && W({},Wave(3)).LinearPcm);
    assert(!W(DvdLpcm,Wave(0)).LinearPcm); // Packed source should still identify decoder boundary.
    assert(W({},Wave(0xff,16)).Bits==16);
    assert(W({},Wave(0x704f,16)).Bits==16);
    assert(W(DdPlus,Wave(0,0)).Bits==0);
    assert(W({},Wave(0x2000,0)).Bits==0);
    // A DTS-HD source can use the ordinary DTS GUID plus MPC-BE's private byte.
    auto dts=Wave(0x2001,24,1);
    for (unsigned profile=1;profile<=7;++profile) {
        dts[18]=static_cast<std::uint8_t>(profile);
        assert(Parse(WaveGuid(0x2001),WaveFormat,dts.data(),dts.size()).Encoding.Name==L"DTS");
        const auto identified=Parse(WaveGuid(0x2001),WaveFormat,dts.data(),dts.size(),0,true);
        assert(identified.Encoding.Name==MpcBeDtsProfile(profile) && identified.Bits==24 && !identified.LinearPcm);
    }
    dts[18]=2;
    assert(Parse(WaveGuid(0x2001),WaveFormat,dts.data(),dts.size(),0,true).Encoding.Name==L"DTS-HD Master Audio");
    for (unsigned profile : {0u,8u,255u}) {
        dts[18]=static_cast<std::uint8_t>(profile);
        assert(Parse(WaveGuid(0x2001),WaveFormat,dts.data(),dts.size(),0,true).Encoding.Name==L"DTS");
    }
    dts[18]=2; Put16(dts,16,65535);
    assert(Parse(WaveGuid(0x2001),WaveFormat,dts.data(),dts.size(),0,true).Encoding.Name==L"DTS");
    Put16(dts,16,1); dts.push_back(0);
    assert(Parse(WaveGuid(0x2001),WaveFormat,dts.data(),dts.size(),0,true).Encoding.Name==L"DTS");
    auto atmos=Wave(0,24,1); atmos[18]=1;
    assert(Parse(TrueHd,WaveFormat,atmos.data(),atmos.size(),0,true).Encoding.Name==L"Dolby TrueHD + Dolby Atmos");
    assert(Parse(DdPlus,WaveFormat,atmos.data(),atmos.size(),0,true).Encoding.Name==L"E-AC-3 (Dolby Digital Plus) + Dolby Atmos");
    assert(Parse(TrueHd,WaveFormat,atmos.data(),atmos.size()).Encoding.Name==L"Dolby TrueHD");
    atmos[18]=2;
    assert(Parse(TrueHd,WaveFormat,atmos.data(),atmos.size(),0,true).Encoding.Name==L"Dolby TrueHD");
    assert(MpcBeProfileSource({0x0a68c3b5,0x9164,0x4a54,{0xaf,0xaf,0x99,0x5b,0x2f,0xf0,0xe0,0xd4}}));
    assert(!MpcBeProfileSource(TrueHd));
    assert(W({},Wave(1,128)).Bits==0);
    assert(W(TrueHd,Wave(0,24),4191000).BitRate==4191000);
    auto largeRate=Wave(1); Put32(largeRate,8,0xffffffffu);
    assert(W({},largeRate).BitRate==34359738360ull); // No 32-bit rate overflow.
    // 16-byte PCM format blocks are legal; truncated compressed blocks are not parsed.
    auto pcm16=Wave(1,16); pcm16.resize(16);
    assert(W({},pcm16).Bits==16 && W({},pcm16).LinearPcm);
    auto shortCompressed=Wave(0xff); shortCompressed.resize(16);
    assert(W({},shortCompressed).Rate==0);
    // WAVEFORMATEXTENSIBLE preserves valid bits and full-width codec GUIDs.
    auto ext=Wave(0xfffe,32,22); Put16(ext,18,24); PutGuid(ext,24,WaveGuid(1));
    auto value=W({},ext); assert(value.Bits==24 && value.LinearPcm && value.Encoding.Name==L"PCM");
    PutGuid(ext,24,WaveGuid(0x5355504f)); value=W({},ext);
    assert(value.Encoding.Name==L"Opus" && !value.LinearPcm && value.Bits==24);
    PutGuid(ext,24,DtsHd); value=W({},ext);
    assert(value.Encoding.Name==L"DTS-HD" && !value.LinearPcm);
    PutGuid(ext,24,IecGuid(0x0c)); value=W({},ext);
    assert(value.Encoding.Name==L"MLP/TrueHD (IEC 61937)" && !value.LinearPcm);
    PutGuid(ext,24,impostorPcm); value=W({},ext);
    assert(value.Encoding.Name.empty() && !value.LinearPcm && value.Identity==impostorPcm);
    Put16(ext,16,65535); assert(W({},ext).Encoding.Name.empty()); // Do not read a forged extension.
    // Previously accepted FLAC variants, now also available through framed FLAC GUID.
    for (unsigned depth : {16u,24u,32u}) {
        auto raw=StreamInfo(depth);
        Bytes metadata{0x80,0,0,34}; metadata.insert(metadata.end(),raw.begin(),raw.end());
        Bytes marked{'f','L','a','C'}; marked.insert(marked.end(),metadata.begin(),metadata.end());
        for (const auto& extra : {raw,metadata,marked}) {
            auto b=Wave(0,0,static_cast<unsigned>(extra.size())); std::copy(extra.begin(),extra.end(),b.begin()+18);
            assert(W(FlacFramed,b).Bits==depth);
            Put16(b,0,0xf1ac); assert(W({},b).Bits==depth);
            Put32(b,4,48000); assert(W({},b).Bits==0); // Reject unrelated private data.
        }
        for (std::size_t n=0;n<raw.size();++n) assert(FlacDepth(raw.data(),n,96000,2)==0);
    }
    // Vorbis 1 uses aligned DWORDs and a bitrate already expressed in bits/s.
    Bytes vorbis1(24); Put16(vorbis1,0,2); Put32(vorbis1,4,48000); Put32(vorbis1,12,192000);
    value=Parse(Vorbis,VorbisFormat,vorbis1.data(),vorbis1.size());
    assert(value.Encoding.Name==L"Vorbis" && value.Rate==48000 && value.Channels==2 && value.Bits==0 && value.BitRate==192000);
    // Vorbis 2: declared depth and optional identification header, no borrowed output precision.
    Bytes vorbis2(54); Put32(vorbis2,0,2); Put32(vorbis2,4,48000); Put32(vorbis2,8,16); Put32(vorbis2,12,30);
    const unsigned char signature[]={1,'v','o','r','b','i','s'};
    std::copy(signature,signature+7,vorbis2.begin()+24); vorbis2[35]=2;
    Put32(vorbis2,36,48000); Put32(vorbis2,44,160000); vorbis2[52]=0xb8; vorbis2[53]=1;
    value=Parse(Vorbis2,VorbisFormat2,vorbis2.data(),vorbis2.size());
    assert(value.Encoding.Name==L"Vorbis" && value.Rate==48000 && value.Channels==2 && value.Bits==16 && value.BitRate==160000);
    assert(Parse(Vorbis2,VorbisFormat2,vorbis2.data(),vorbis2.size(),177000).BitRate==177000);
    Put32(vorbis2,44,0xffffffffu); assert(Parse(Vorbis2,VorbisFormat2,vorbis2.data(),vorbis2.size()).BitRate==0);
    Put32(vorbis2,44,160000); Put32(vorbis2,16,0xffffffffu); Put32(vorbis2,20,0xffffffffu);
    assert(Parse(Vorbis2,VorbisFormat2,vorbis2.data(),vorbis2.size()).BitRate==0);
    for (std::size_t n=0;n<24;++n) {
        assert(Parse(Vorbis,VorbisFormat,vorbis1.data(),n).Rate==0);
        assert(Parse(Vorbis2,VorbisFormat2,vorbis2.data(),n).Rate==0);
    }
    // Names survive absent/private format buffers; numeric fields remain unavailable.
    value=Parse(TrueHd,{},nullptr,1000000);
    assert(value.Encoding.Name==L"Dolby TrueHD" && !value.Rate && !value.Bits && !value.LinearPcm);
    assert(Parse(WaveGuid(0x63616c61),{},wrongPcm.data(),wrongPcm.size()).Rate==0);
    // Byte-based parsing is alignment independent.
    Bytes unaligned(1+wrongPcm.size()); std::copy(wrongPcm.begin(),wrongPcm.end(),unaligned.begin()+1);
    assert(Parse(TrueHd,WaveFormat,unaligned.data()+1,wrongPcm.size()).Bits==24);
    // Truncation/forged lengths/mixed layouts under ASan/UBSan.
    std::mt19937 rng(0x4f5344u);
    for (unsigned round=0;round<50000;++round) {
        Bytes data(rng()%257); for (auto& b:data) b=static_cast<std::uint8_t>(rng());
        const Guid formats[]={WaveFormat,VorbisFormat,VorbisFormat2,{}};
        const Guid codecs[]={TrueHd,DdPlus,FlacFramed,Vorbis2,WaveGuid(1),{}};
        const auto parsed=Parse(codecs[rng()%6],formats[rng()%4],data.data(),data.size(),rng(),(rng()&1)!=0);
        assert(parsed.Bits<=64);
    }
    std::cout << "PASS: subtype precedence, GUID identity, 16-byte PCM, extensible formats, source depth, FLAC, Vorbis, verified MPC-BE DTS/Atmos profiles, unknown layouts, unaligned buffers, 64-bit rates and 50000 bounded-buffer cases.\n";
}
