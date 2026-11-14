# VDEC1 + HCODEC concurrent transcode — 2026-11-14

Target: original OneCloud Linux 5.15.79-meson, 64 MiB CMA, ffmpeg 7.0.2.
Both modules probe simultaneously as /dev/video0 (decoder) and /dev/video1
(encoder). No kernel image/boot changes, no service stops, no watchdog reboot.

## Changes required

The old decoder and encoder each requested the same entire DOS MMIO
resource. The second probe failed with resource busy. The runtime module
now owns one request_mem_region/ioremap for DOS; both drivers borrow that
mapping and their symbol dependencies prevent early runtime removal.

VDEC1 and HCODEC touch disjoint engines but share DOS_GCLK_EN0 and
DOS_GEN_CTRL0. Their masked updates now use one IRQ-safe spinlock. Parser
selection changes only bits 2:1; HCODEC auto-gate uses bit 0. Both use the
same syscon PMU regmap for atomic sleep/isolation updates, avoiding lost
power bits during parallel stream start/stop.

The encoder formerly allocated the whole vendor 0x1160000 (~17 MiB) layout
as one CMA block. Concurrent decode exposed allocation failures. It now
allocates the vendor regions separately: DCT/input 0x6ba000, each reference
0x300000, firmware workspace 0xc0000, bitstream 0x100000, analysis
0x110000. Only gaps and the unused VDEC2 tail are omitted; register addresses
are independently programmed, and the firmware workspace-relative layout
is preserved. JPEG and AVC use the same allocation/cleanup path.
Queue initialization failure now marks both queues in error so one failing
allocation does not produce repeated frame errors and a success-looking
empty ffmpeg output. Negative firmware tests verify recovery by reopening.

## Verification

- 720p: 60 frames hard decode -> hard encode, software decode succeeds.
- 1080p Baseline: 60 frames, reference count 1, no B frames. Five consecutive
  sessions while both drivers remain loaded, each produces all 60 frames;
  software decode succeeds after each. Encoder output has no reference,
  color or motion artifacts.
- External ffprobe independently reports 1920x1080 H.264, 60/60 frames.
  Per-frame PSNR against software decode of the input: average 45.41 dB,
  minimum 44.42 dB, maximum 47.68 dB (fixed AVC QP 26).
- Independent hardware decode of the same stream: software/hardware NV12
  frame MD5 matches 60/60. Compare both as NV12: hashing yuv420p against
  NV12 hashes different plane layouts even when pictures match.
- Full JPEG matrix after allocation changes: all eight rows pass, plus
  H.264 client and ffmpeg 1080p regressions.
- Missing/truncated firmware errors, replacement and session recovery.
- Module build + seven Python tests + host copy/JPEG sanitizer checks.

Five runs are bounded bring-up stress, not a long-duration production test.
Raw logs and checks: evidence/concurrency-2026-11-14/.

Input source SHA-256:
bdcd61909e29f7b7655b00c9a8f7ae50001480100d2825dddd7e61925acd1b80
(the 60-frame 1080p Baseline test generated from testsrc2 via libx264).
Output SHA-256:
33673cf14bec2b8a4ffd74bbb7231deb374433c45e8389b91af5b5fdb81e6942.

## Memory boundary

The 1080p Main/B-frame sample (max_num_ref_frames=4, reorder=2) needs more
DPB canvases and reference-MV memory. Concurrent encoder allocations still
failed under this target's 64 MiB CMA and the live service load. It is not
advertised as a supported concurrent mode. Independent B-frame decode was
validated previously; this limitation is the simultaneous memory budget.
No CMA enlargement, reboot or stop of production services was used.

Setting fewer CAPTURE buffers than the driver MIN_BUFFERS_FOR_CAPTURE can
also stall ffmpeg's source-change sequence. Use the negotiated minimum (7
for the verified Baseline stream), not a guessed constant for arbitrary
content. Timeout wrappers must include a kill deadline (`timeout -k 5`).
The passthrough pipeline still includes decoder byte-order copy and encoder
input staging; no end-to-end zero-copy claim is made. Performance numbers
in concurrent-results.json are observed process times, not a new comparison
with software transcoding.

SublinkPro, VoCat, Home Assistant were running at this run's baseline;
OpenList/Arcane were absent. Acceptance applies to the running services.

Final package installation check also passed: checksum + kernel + live
provider validation, all three modules loaded, two JPEG frames encoded,
then runtime-device.py rollback restored firmware and removed all codec
nodes/modules and the DOS resource. Running service states remained active.
See concurrency-2026-11-14/runtime-cycle.log. Installer host tests cover
interrupted staging recovery, repeat rollback, later-update refusal and
symlink/path escape checks (10 Python tests total).
