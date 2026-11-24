# MPCVR-Plus OSD v1.0.10

## Upstream updates

Merged [MPCVR-Plus 3.2](https://github.com/HumbleUser33/MPCVR-Plus/releases/tag/MPCVR-Plus-3.2), including all 21 upstream commits since 3.0.

- HDR curve: dedicated settings tab, presets, live strength adjustment (0–200%), display-peak control and HDR Auto Tune.
- Post-scaling DLAA, DLAA after downscaling, and a 2/3 optical-flow resolution option.
- Improved RTX Video Super Resolution reporting at native resolution, render-ahead and hiccup statistics.
- Includes the 3.0.1 and 3.0.2 fixes for chroma upsampling, RTX Video HDR overexposure, Windows ACM compatibility with RTX VSR and DLSS 5 NR, and potential renderer deadlocks.

## OSD Plus

- Retains the independent OSD-Plus 1.0.10 label, color-coded OSD, audio metadata, and low-frequency CPU/GPU sampling.
- Keeps the synchronization graph hidden while preserving textual timing statistics.
- Retains the upstream MPC-BE dark-dialog integration, including the new HDR curve page.

## Acceptance and build provenance

- User acceptance passed for the 1.0.10 test package.
- Accepted source: `ecb0bdf698180241f3a7a765f968f2b00db01123`.
- Upstream 3.2: `e5f1140d3375a0e14af6c98f8c6d77239dae66a2`.
- [Win32 and x64 CI build](https://github.com/rampageX/MPCVR-Plus/actions/runs/45417451298) completed successfully.
- The release reuses the accepted binaries without recompilation. Only release metadata and documentation are updated.

## Downloads

The ZIP includes Win32 and x64 filters, installation scripts, upstream documentation and licenses, OSD Plus documentation, and BUILD-INFO.json. SHA256SUMS.txt is provided separately.
