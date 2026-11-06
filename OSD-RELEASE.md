# MPCVR-Plus OSD v1.0.7

## Changes

- Integrate MPCVR-Plus 2.8, including the changes from 2.6 and 2.7.
- Add upstream PP7/SPP deblocking, libplacebo-based debanding, and GPU channel-logo detection and removal.
- Add AMD CAS sharpening and upstream sharpening-level calibration. Unsharp + Clamp now sharpens more at the same saved level.
- Include upstream OSD draw batching and player private-working-set (DRAM) statistics.
- Include upstream HDR10/Dolby Vision startup deadlock fix.
- Use upstream's independent settings key, `HKCU\Software\MPC-BE Filters\MPC Video Renderer Plus`. Existing settings are copied on first use.
- Preserve the fork's OSD-Plus version labels and hidden sync graph, with text statistics retained.
- Update the displayed OSD-Plus version to 1.0.7.

## Validation

- The user confirmed playback acceptance of integration commit `a4f74344db6fc45e4b0bedd9a6c385bf954b983f`.
- The release build changes only `Source/OsdPlus/Version.h` from that accepted integration, updating the display label from 1.0.6 to 1.0.7.
- Audio input-selection and reported bit-depth regression checks passed with UndefinedBehaviorSanitizer.
- OSD integration hooks and merge conflict markers checked.
- GitHub Actions built both Win32 and x64 filters.
- The final version-label-only rebuild has not been separately playback-tested.

## Build provenance

- Source commit: `5d129a52fbc7efc47ff6c5b23f7b8f1d56aa728a`
- Upstream tag: `MPCVR-Plus-2.8` (`c9dc91b3f2736b8f955d56d3b9fca93123241792`)
- Build: https://github.com/rampageX/MPCVR-Plus/actions/runs/42172338047
- Publication reuses the successful build without rebuilding the filters.

## Package

Includes Win32 and x64 filters, English documentation, build provenance and a SHA-256 package checksum.

## Upstream limitations

- Artifact removal and logo hiding apply to supported progressive planar Direct3D 11 video; unsupported formats pass through.
- Logo hiding targets stationary logos.
- Upstream recommends Windows hardware-accelerated GPU scheduling when using NVIDIA RTX Video Super Resolution or DLSS SR.
