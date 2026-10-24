# MPCVR-Plus OSD v1.0.1

## Fixed

- Preserve valid audio bit-depth values reported by the playback graph,
  including AAC and Opus, instead of suppressing them based on codec type.
- Keep unavailable or invalid bit-depth values as N/A.
- Reported compressed-stream bit depth does not establish the precision
  of the original PCM recording.

## Validation

- Playback display changes verified by the user.
- Publication reuses the accepted successful OSD Plus build specified
  in OSD-RELEASE.json without rebuilding the filters.

## Package

Includes Win32 and x64 filters, English documentation, build provenance
and a SHA-256 package checksum.

Existing v1.0.0 functionality and limitations remain unchanged.