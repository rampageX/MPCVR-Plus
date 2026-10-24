# MPCVR-Plus OSD v1.0.0

First stable OSD customization release based on MPCVR-Plus. Includes both Win32
and x64 filters.

## Changes

- White labels with category-specific colors for source, processing, audio,
  hardware and timing values.
- CPU total and player-process utilization, GPU busiest-engine and video-decode
  utilization, and player-local video memory usage.
- Video codec, average/current video-stream bitrate, and whole-file average
  bitrate where available without a media scan.
- Audio source, decoded output and renderer-input formats, including sample rate,
  channels, valid bit depth and bitrate where exposed by the playback graph.
- Active-track detection through MPC-BE's StreamSwitcher input interface.
- E-AC-3 / Dolby Digital Plus identification by its complete media subtype GUID,
  including streams with a zero wave-format tag.
- Declared DTS source precision is preserved rather than unconditionally hidden.
- Sync graph hidden while textual synchronization statistics remain available.
- File-name display removed.
- Deferred background monitoring: starts after the OSD has been displayed for
  three seconds, refreshes once per second, and suspends expensive GPU queries
  while the OSD is hidden. Source changes invalidate stale statistics.

## Validation and build provenance

The user accepted the OSD changes during playback in MPC-BE and PotPlayer,
including E-AC-3 identification in both players. Multi-track selection and
source-depth policies also have regression checks.

This release reuses the accepted CI binaries for commit
`5762bee2d2b843c9776c3f924a8b23edbc917994`, built in Actions run `40047473332`.
Win32 and x64 compilation completed without C/C++ compiler warnings or errors.
The filters are not rebuilt during publication. `BUILD-INFO.json` records the
source commit, original CI run and individual filter hashes. `SHA256SUMS.txt`
provides the downloadable package checksum.

## Installation and limitations

Close the player and back up the current filter before replacing the configured
`.ax` with the matching architecture. Alternatively, use the matching registration
script supplied in the package.

- OSD enhancements apply to the **D3D11** path; the legacy D3D9 path is unchanged.
- Custom font, size and weight controls are deferred.
- `Audio render` describes the negotiated input of the audio renderer, not an
  unexposed WASAPI/device output or shared-mode mix format.
- Missing metadata remains `N/A`. DTS precision is displayed only when declared;
  E-AC-3 source precision is not assumed to be 24-bit. Decoder output precision
  is reported separately.
- `File avg` includes all streams and container overhead; video-stream bitrate
  is reported separately. `VRAM (player)` is process-local usage.
- NVIDIA DLSS model DLLs are not redistributed. Refer to the original project's
  documentation for DLSS setup.

The OSD customization uses a separate release version without changing upstream's
internal version. `dlss5` remains the upstream mirror and `osd-plus` contains the
customization. The release tag includes publication documentation and automation;
its production sources are verified against the accepted binary source commit.
