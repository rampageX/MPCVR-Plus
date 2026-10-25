# OSD telemetry recovery and CPU utility

Copy only `Source/OsdPlus/Telemetry.h` over the same file in your local
`osd-plus` checkout. No audio, rendering, release or upstream files are changed.

Suggested commit: `Improve OSD GPU sampling diagnostics and recovery; use CPU Utility`

## Changes

- GPU instance matching is case-insensitive and requires a complete LUID token.
- Retry counter-array reads when the required buffer size changes; bound allocation
  to 16 MiB and retry at most three times per read.
- Ignore non-finite and negative GPU values.
- Recreate failed GPU queries at most once every 15 seconds while the OSD is visible.
- Report initialization, collection, array-read and adapter-match failures in
  `Monitor status`, including PDH error codes where available.
- Prefer `Processor Information(_Total) / % Processor Utility` for total CPU.
  Clamp the displayed value to 0–100%. If unavailable, explicitly label the existing
  busy-time fallback. Player CPU remains a time-based process share.
- Keep all PDH operations on the existing background worker, at one-second sampling.
  Counter initialization is delayed until the OSD is visible; hidden OSDs do not
  trigger query recreation. No extra worker, executable, registry change or service.

## Validation and acceptance

The system's GPU counters were confirmed working by the user's PowerShell samples.
This patch improves recovery and diagnostics; the original failure's exact cause
has not yet been established. It must not be described as a confirmed driver fix.

Automated portable tests exercise the production GPU aggregation function with
mock PDH results: mixed-case names, process summation, separate engines, exact
adapter matching, invalid values, buffer resizing and read failure.

Windows AX compilation and real PDH/player acceptance remain required. Use the
OSD Plus build workflow, then test in MPC-BE. Keep the OSD open for at least 20
seconds. Compare CPU and GPU trends over several seconds, not single snapshots.
If GPU is still N/A, send the complete Monitor status line and the build identifier.
Do not publish until this build has passed playback acceptance.

Microsoft references:
- https://learn.microsoft.com/windows/win32/api/pdh/nf-pdh-pdhgetformattedcounterarrayw
- https://learn.microsoft.com/windows/win32/api/pdh/nf-pdh-pdhaddenglishcounterw
- https://learn.microsoft.com/troubleshoot/windows-client/performance/cpu-usage-exceeds-100
