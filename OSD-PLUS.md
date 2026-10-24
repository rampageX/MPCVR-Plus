# OSD Plus (rampageX fork)

## Branches and upstream updates

`dlss5` stays the upstream mirror. All local features and their build workflow
live on `osd-plus`. Do not reset the feature branch to upstream or discard its
commits when synchronizing the fork.

The draft PR against `dlss5` is a review of the fork's delta, not a request to
merge the customization into the mirror. Build and use the `osd-plus` branch.

1. Synchronize `dlss5` with HumbleUser33/MPCVR-Plus's `dlss5` branch.
2. Merge `dlss5` into `osd-plus` on a temporary integration branch.
3. Review the diff and resolve any conflicts in the renderer integration hooks.
4. Build both platforms, then verify playback, SDR/HDR OSD, seeking and file changes.
5. Merge the tested integration branch into `osd-plus`.

Changes in different parts of the same file can merge automatically. A clean Git
merge does not prove API or runtime compatibility: always build and test.

## Ownership boundary

`Source/OsdPlus/` owns the overlay and sampling logic.
`.github/workflows/osd-plus.yml` builds this fork independently of upstream's
workflow. Upstream's public COM interfaces, settings layout, codec pipeline,
shaders and version numbers are unchanged.

The only renderer integration hooks are in `DX11VideoProcessor.{h,cpp}`:
the overlay member and include, adapter LUID capture, source invalidation, shutdown, cached text,
overlay update/draw, and sync-graph visibility. Keep these hooks small when
merging; move new behaviour into the extension directory.

## First preview

This preview extends the D3D11 renderer used by the DLSS features. The legacy
D3D9 renderer is unchanged.

- White labels; green source values, cyan pipeline values, yellow timing values,
  and orange hardware values. HDR values follow the existing OSD brightness.
- CPU total and player process utilization, GPU busiest-engine utilization,
  video decode utilization and **player-local** video memory use.
- Hardware counters are selected by the rendering adapter LUID, not GPU index 0.
- Encoded input FourCC and average/current **video stream** bitrate are read
  from the decoder input / splitter `IBitRateInfo` interface where available.
  Average bitrate falls back to the stream's declared `dwBitRate`. It is not a
  whole-file bitrate or an estimate based on uncompressed renderer samples.
- Native in-process DirectShow pin references are retained for the background
  reader, without requiring IPin proxy/stub marshaling. Source failures include
  a diagnostic reason instead of silently leaving every field unavailable.
- `File avg`, where available, uses local file size and graph duration. It is
  separately labelled because it includes audio and container overhead. Network
  and removable files are not probed; no full media scan is performed.
- Missing interfaces/counters show `N/A`; no extra media scan or external tools.
- Monitoring starts only after the OSD has been displayed for three seconds.
  Driver/CPU samples update once per second on a worker; expensive GPU queries
  suspend when the OSD is hidden. Rendering reads a cache with a nonblocking lock.
- The sync graph is hidden; sync and dropped-frame text remains.
- Font customization is deferred. No font enumeration, whole Unicode atlas,
  or new startup font load is added in this preview.

## Acceptance on Windows / RTX 4080

Audio rows show the active connected audio path: compressed decoder input,
decoder output, and audio renderer input. Each reports the negotiated codec,
sample rate, channels, valid PCM bit depth and bitrate where exposed. Compressed
live bitrate comes from the splitter; PCM bitrate is transport throughput. Lossy
codecs have no PCM source bit depth and show depth N/A. Renderer input does not
claim to describe an unexposed WASAPI/device mix format. Audio metadata refreshes
on the worker at one-second intervals. Ambiguous multiple connected inputs are
not guessed. MPC-BE's StreamSwitcher IsActive interface resolves multiple
connected inputs, and is queried again on every audio refresh so track changes
follow the selected path. File names are deliberately not displayed.

Compare the original and preview with the same player settings and files:

- Launch and load videos with OSD both disabled and enabled by default.
- Toggle OSD; hardware data should appear after about four seconds.
- Check label/value colors, SDR/HDR brightness and clipping at different sizes.
- Compare GPU counters with Task Manager on the actual rendering GPU. CPU uses
  elapsed CPU time, which can differ from frequency-normalized Windows displays.
- Check H.264/HEVC/AV1 with the player's splitter; unsupported metadata is N/A.
- Seek, pause, resume, switch files and close repeatedly; no stale data or hang.
- Test video decode and a GPU-heavy processing path; confirm no added dropped
  frames. Windows CI verifies compilation, not RTX hardware or load-time impact.
