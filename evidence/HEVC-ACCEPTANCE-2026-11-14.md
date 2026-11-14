# S805 HEVC Main 8-bit acceptance

The pinned vendor firmware uses a different RPM prefix from GX/SM1.
The port reuses the GPL stateful codec from unifreq/linux-5.15.y at
commit e7a67bbcd082c6038daa4ff1ab863cb648fa32b9, with S805 4-word tile
arrays, 1080p motion-vector workspace and scratch L/M/N protocol.
Firmware origin and SHA-256 are recorded in firmware/sources.json.

HEVC uses its separate engine at 255 MHz via fixed fclk_div5, original
5.15.79-meson kernel, runtime overlay, PMU regmap and shadow NV12 buffers.
The bounded SPS prefix parser restores conformance-window geometry absent
from vendor RPM. CPU output handles 32-byte reference strides and crop.
CAPTURE backpressure pauses the pending slice until a buffer is queued.
Requeued references are held until RPS drops them. Firmware EOS is drained
before an empty LAST buffer; the EOS event follows LAST consumption.

Verified live results:

- 320x240: 10/10 NV12 frame hashes match software decode.
- 1920x1080 Main, bframes=2: 30/30 hashes match; repeated 10 sessions
  and an additional 10 mixed-load sessions returned all frames.
- 854x480 (coded width/crop): final 10/10 raw pictures match software;
  interim eight-frame stride/EOS tests are not acceptance evidence.
- Truncated/missing firmware produce queue errors, restored firmware
  permits a fresh session. Malformed stream stalls are bounded by a
  5-second firmware watchdog; subsequent small/1080p/crop sessions work.
- Host ASan/UBSan SPS checks cover dimensions, sublayer prefix, crop,
  reference/reorder fields, truncation and invalid bitstreams.

The target's original static ffmpeg omits hevc_v4l2m2m. An independent
static armhf FFmpeg 7.0.2 test tool is built externally and left under the
codec package tools directory; the original binary is preserved.
FFmpeg's early EOF when all CAPTURE buffers were user-held caused missing
frames, and its POLLERR path failed to publish decoder EOF. The independent
tool includes patches/ffmpeg-7.0.2-v4l2-drain.patch and uses LAST/EPIPE.
Build command and pinned archive hash are in scripts/build-ffmpeg-test.sh.

Raw logs and hashes: evidence/hevc-2026-11-14. All production services
were present during final mixed-load testing; OpenList and Arcane original
containers were restored before that run. No codec kernel Oops or watchdog
reboot was observed. Existing USB urb -121 warnings predate this work.

Main10, tiles and long-term-reference HEVC are not advertised by this
8-bit linear port. A current-mainline RFC remains a separate port/review
exercise; docs/linux-media-rfc.md is a local submission draft only.
