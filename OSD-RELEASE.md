# MPCVR-Plus OSD v1.0.9

## Changes

- Merge upstream [MPCVR-Plus 3.0](https://github.com/HumbleUser33/MPCVR-Plus/releases/tag/MPCVR-Plus-3.0), preserving the complete Git merge history and including the 2.9 updates.
- Add EWA Lanczos Sharp, EWA Ginseng, EWA Lanczos4 Sharpest chroma upsamplers, Anime4K CNN x2 M AR, optional moving dithering and HDR measured-peak scene-cut reset.
- Include NVIDIA app RTX Video HDR / Super Resolution statistics, automatic FP16 textures for HDR output, and upstream frame-pacing and snapshot fixes.
- Use upstream's accepted MPC-BE dark-settings implementation (PR #6). Dark and light settings pages retain native host styling without duplicate patches.
- Retain the fork's color-coded OSD, audio and video metadata, low-frequency CPU/GPU sampling, and OSD-Plus 1.0.9 version labels.
- Keep the lower-right synchronization graph hidden while preserving textual timing and dropped-frame statistics.

## Acceptance and build provenance

- User playback and settings acceptance completed for the `1.0.9` test build.
- Source commit: `a1930290fbeb6d592f803a89b83201160515e519`.
- Upstream tag: `MPCVR-Plus-3.0` (`3157c1123d0d20139cab8bbd28bcdd3c6412d928`).
- [GitHub Actions OSD Plus build](https://github.com/rampageX/MPCVR-Plus/actions/runs/44296627370) succeeded for both Win32 and x64.
- The release reuses the accepted binaries without recompilation. Only release metadata, documentation, and the restored publishing script differ from the accepted source snapshot.

## Package

Both Win32 and x64 filters, installation scripts, English documentation, build provenance, and SHA-256 checksums are included.
