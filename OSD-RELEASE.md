# MPCVR-Plus OSD v1.0.6

## Changes

- Integrate MPCVR-Plus 2.5 and MPC Video Renderer 0.11.1.
- Include upstream HDR-to-SDR Metadata and Measured modes, Spline tone mapping, Dolby Vision tone mapping selection, and R10k input support.
- Include upstream presentation timing, render-ahead settings, preset isolation, and HDR state fixes.
- Preserve OSD-Plus version labels, colored statistics, audio metadata, adapter-specific monitoring, and hidden sync graph with text statistics retained.
- Upstream 2.5 now includes OSD Plus and credits rampageX.

## Validation

- Audio input selection and reported bit-depth regression checks passed with UndefinedBehaviorSanitizer.
- OSD integration hooks and merge conflict markers checked.
- Win32 and x64 filters built by GitHub Actions.
- This release has not been playback-tested on an RTX 4080 or the user's laptop. Compilation and portable checks do not replace Windows playback acceptance.

## Accepted build

- Source commit: `4dab32a3ba35818114c0f6349342773e71a284d8`
- Upstream tag: `MPCVR-Plus-2.5` (`8f1bf90e7c468e362e8b2d1be16ed4f664ea1f5a`)
- Build: https://github.com/rampageX/MPCVR-Plus/actions/runs/41189687801
- Publication reuses this successful build without rebuilding the filters.

## Package

Includes Win32 and x64 filters, English documentation, build provenance and a SHA-256 package checksum. Existing limitations remain documented in OSD-PLUS.md.
