# MPCVR-Plus OSD v1.0.5

## Changes

- ArtCNN keeps its shared neighbourhood where one invocation writes one texel
- The Jinc chroma sums its four parities in one pass, without a branch
- Sharpening ends on the dither when it is the last step before it
- A picture that only has to be put in its place is read there by the next step
- DLSS 5 NR and the renderer wait for each other on the GPU, not on the CPU
- The stabilizer and the composition draw their passes with one helper
- The Information page names the guides DLSS 5 NR is given
- This fork's own parts of the D3D11 processor move to files of their own
- The reference is taken again, after a review of the whole filter
- Upstream 0.10.9 comes in
- The bench reads an HDR desktop in half floats, and pauses a film under each way of enlarging
- A 10-bit picture asked for as a DIB is converted no wider than the DIB
- Super Resolution on an 8-bit picture is given eight bits to write on an HDR desktop
- The chroma replacement waits for a picture really interlaced
- The statistics say when the driver leaves RTX Video HDR out
- A small picture the shaders enlarge is first enlarged past 640x360 for RTX Video HDR
- The manual says what RTX Video HDR needs of a picture, and what the chroma replacement waits for
- The Scaling step starts where the processor left the picture
- The reference is taken again, after upstream 0.10.9 and the RTX Video HDR fixes
- Add independent OSD Plus version display

## Accepted build

- Source commit: `b86cfbaf42d43cae31d7eda5b1fb4cbd4d4adf97`
- Build: https://github.com/rampageX/MPCVR-Plus/actions/runs/40543448373
- Publication reuses this successful build without rebuilding the filters.
- Confirm playback acceptance before committing these release files.

## Package

Includes Win32 and x64 filters, English documentation, build provenance
and a SHA-256 package checksum. Existing functionality and limitations
remain as documented in OSD-PLUS.md.
