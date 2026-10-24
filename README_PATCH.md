# OSD audio format coverage patch

## Install

Keep the local checkout on `osd-plus`. Copy the supplied `Source` directory
into the repository root, replacing `Source/OsdPlus/AudioInfo.h` and adding
`Source/OsdPlus/AudioFormats.h`. Keep the existing `AudioDepth.h` and
`AudioSelection.h`. Commit both production headers and push normally.
No Visual Studio project-file edit is required for an included header.

Suggested commit title:

```text
Expand OSD audio formats and recognize MPC-BE DTS-HD and Atmos profiles
```

Download the resulting **OSD Plus build** artifact for playback acceptance.
The patch does not publish a release or change release metadata.

## Behavior

- Uses full 128-bit subtype GUIDs before falling back to a WAVE format tag.
- Handles WAVEFORMATEXTENSIBLE's full SubFormat GUID without truncating it
  to a 16-bit tag. Valid PCM precision is retained when the structure declares it.
- Adds DVD/Blu-ray LPCM, Dolby TrueHD, E-AC-3 and legacy aliases, DTS/DTS-HD,
  ALAC, APE, MLP, WavPack, TTA, TAK, ALS, Musepack, AMR, Speex, DSD/DST,
  QuickTime PCM, AC-4 and other common tag/FourCC identities.
- Supports framed FLAC and both Opus GUID conventions.
- Recognizes common IEC 61937 encoded transports. Their displayed rates,
  channels and depth describe the carrier, not the original encoded recording.
- Reads MPC-BE's exact one-byte DTS profile or Dolby Atmos extension only when
  the actual upstream pin belongs to a verified MPC-BE Matroska/MP4/MPEG
  splitter/source CLSID. This distinguishes DTS-HD Master Audio, HRA, Express,
  DTS:X variants, and declared TrueHD/E-AC-3 Atmos without scanning the file.
- Parses FORMAT_VorbisFormat and FORMAT_VorbisFormat2, including a bounded
  identification header when available. Nominal bitrate is a hint, not an
  average measured over the file; a current graph bitrate takes precedence.
- Preserves the previous FLAC STREAMINFO depth fallback and graph-reported
  AAC/Opus depths. Missing/invalid values remain N/A.
- Known codec names remain visible when the numeric format structure is
  unavailable. Unknown formats include their GUID for diagnosis.
- Keeps existing active-track traversal, background cadence and rendering hooks.

There is no media-file scan, external process, new library dependency or
additional startup task. Parsing uses fixed-size reads from the graph's existing
format buffer. Tables are constant data with no dynamic initialization work.

## Limits and acceptance

This extends common DirectShow audio format coverage; it cannot identify every
private codec/format block or information a splitter does not expose. A recognized
subtype does not guarantee that all numeric fields are available.
Dolby Atmos and DTS:X are not inferred from TrueHD or DTS-HD alone. They require
the verified MPC-BE private extension or an explicit standardized Atmos transport
GUID. Other players/splitters may only expose
generic DTS/TrueHD, so their profile can remain unspecified. Generic DTS-HD is
displayed as DTS-HD without guessing Master Audio. The dedicated DTS-HD HRA
subtype is identified explicitly. No player UI title/track name is parsed as
evidence. No MFC CString ABI or decoder configuration interface is called.
Reported compressed-stream depth is not proof of original recording precision.
`Audio render` still means negotiated renderer input, not a WASAPI device mix.
No decoded-output bit depth is substituted into an unknown source format.

Portable parser regression tests are included in `validation/`. These are not
needed in the repository or for playback. Windows/DirectShow build and live
MPC-BE/PotPlayer acceptance require the user's GitHub Actions build and players.
Check previous AAC/Opus, E-AC-3, DVD LPCM and FLAC cases, plus TrueHD, a
Blu-ray LPCM or DTS-HD case and an Ogg/Vorbis file if available. Switch tracks
and confirm the displayed source changes with the active track.

## Source baseline

Based on `rampageX/MPCVR-Plus`, branch `osd-plus`, `AudioInfo.h` containing the
accepted DVD LPCM and FLAC fixes and the user's new TrueHD mapping. Files were
retrieved read-only; no repository write, commit, workflow or release was made.
If either production header has local changes after this baseline, review the
diff instead of overwriting those changes.

## Format definition references

The parser is independently implemented. Codec identifiers and binary layout
facts were checked against these public definitions:

- [MPC-BE format identifiers](https://github.com/Aleksoid1978/MPC-BE/blob/master/include/moreuuids.h)
- [MPC-BE audio naming and structure handling](https://github.com/Aleksoid1978/MPC-BE/blob/master/src/DSUtil/MediaTypeEx.cpp)
- [LAVFilters DirectShow format definitions](https://github.com/Nevcairiel/LAVFilters/blob/master/common/includes/moreuuids.h)
- [Windows SDK wmcodecdsp.h](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/um/wmcodecdsp.h)
- [Windows SDK ksuuids.h](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/shared/ksuuids.h)
- [Windows SDK encoded transport GUIDs](https://github.com/microsoft/win32metadata/blob/main/generation/WinSDK/RecompiledIdlHeaders/shared/ksmedia.h)
- [MPC-BE Matroska private audio extension](https://github.com/Aleksoid1978/MPC-BE/blob/master/src/filters/parser/MatroskaSplitter/MatroskaSplitter.cpp)
- [MPC-BE MP4 private audio extension](https://github.com/Aleksoid1978/MPC-BE/blob/master/src/filters/parser/MP4Splitter/MP4Splitter.cpp)
- [MPC-BE MPEG private audio extension](https://github.com/Aleksoid1978/MPC-BE/blob/master/src/filters/parser/MpegSplitter/MpegSplitterFile.cpp)
- [MPC-BE DTS profile values](https://github.com/Aleksoid1978/MPC-BE/blob/master/src/DSUtil/AudioParser.h)
- [Vorbis identification header specification](https://xiph.org/vorbis/doc/Vorbis_I_spec.html#x1-630004.2.2)
- [FLAC format specification](https://www.rfc-editor.org/rfc/rfc9639.html)
