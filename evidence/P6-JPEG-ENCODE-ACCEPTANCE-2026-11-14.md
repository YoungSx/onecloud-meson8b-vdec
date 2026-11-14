# JPEG hardware encode acceptance — 2026-11-14

Target: OneCloud / Meson8b, original Linux 5.15.79-meson. The runtime
overlay remained loaded; the encoder module was removed after each test.
No boot files, kernel image or persistent service configuration changed.

Vendor reference: Hardkernel linux commit 5aed95d35d252cafc75ce613a3a0052285662de2,
drivers/amlogic/amports/jpegenc.c (sha256
dbe73fa56d794ef8bfaf96b9bb3306a551686827e8b9a71b27c7291e12a1a833) and
jpegenc.h (975e3b4e380d519aa2c72df02b80fafee27ed326af4c801e13161a03c0e500cd).
Microcode: meson8b_jpeg_enc.bin, sha256
1542e1c65fdfdcfd86364a932473e1d887ae15b514f4b41999141b84dbd6dd6f.

JPEG is CAPTURE format V4L2_PIX_FMT_JPEG on the existing encoder device.
OUTPUT supports NV12M/NV21M/YUV420M, with the same single-open HCODEC
ownership as AVC. CPU constructs a 608-byte JFIF header and programs Annex K
quantization/Huffman tables; HCODEC performs DCT and entropy encoding.
V4L2_CID_JPEG_COMPRESSION_QUALITY supports 1–100, default 90.
Source samples are full-range YCbCr; callers must convert limited-range
video before encoding JPEG. The driver does not silently rescale samples.

## Results

Every row is five frames, all software-decodable, each with the source
timestamp and KEYFRAME flag. Each session also checked a second open
returns EBUSY, S_FMT with allocated queues returns EBUSY, and drain produces
an empty LAST buffer plus V4L2_EVENT_EOS.

| Input | Quality | Average PSNR against deterministic source |
| --- | --- | --- |
| 320x240 NV12 | 40 | 43.33 dB |
| 320x240 NV12 | 90 | 53.48 dB |
| 320x240 NV21 | 90 | 53.48 dB |
| 320x240 YUV420 | 90 | 53.48 dB |
| 854x480 YUV420 | 90 | 55.96 dB |
| 1280x720 NV12 | 90 | 56.49 dB |
| 1920x1080 NV12 | 90 | 59.77 dB |
| 1920x1080 YUV420 | 90 | 59.77 dB |

These are smooth synthetic gradients, not a general quality benchmark.
JPEG output from NV12/NV21/YUV420 is identical at matching dimensions and
quality. Raw reference is NV12 with full-range metadata, software JPEG
decode is yuvj420p, and PSNR uses matching 25 fps timestamps.

Regression: eight H.264 frames through the same ioctl client, then twelve
1080p frames through ffmpeg 7.0.2 h264_v4l2m2m; all output software-decodes.
Local ffprobe independently confirms 5 JPEG frames and 12 H.264 frames.
No ffmpeg JPEG v4l2m2m encoder wrapper is installed: JPEG acceptance uses
tests/encode-test.c through the standard V4L2 API.

Truncated (256-byte) and absent firmware return error buffers without DMA
or hangs. Restoring the original image permits a new JPEG session to encode
successfully without unloading the module. See jpeg-2026-11-14/jpeg-negative.log.

Host verification: tests/test_jpeg.c with ASan/UBSan validates all 1–100
quantizers, reciprocal values, canonical Huffman LUTs, JPEG marker lengths,
DQT zigzag ordering, dimensions and header bounds; seven existing Python
tests pass. Cross-build uses GCC 11 and matching 5.15.79-meson vermagic.

## Corrections found during verification

- Empty-output STOP used the core helper to complete LAST without invoking
  the work callback. It now also queues the EOS event.
- ffmpeg's visible 1080-row payload need not include 1088 macroblock rows.
  Staging copies visible rows and leaves padding in the zeroed DMA buffer;
  payload validation checks visible bytes, not padding.
- The target currently has ffmpeg but no ffprobe or v4l2-ctl. Output probe
  verification was performed on the external build host.

Existing SublinkPro, VoCat and Home Assistant stayed active before/after.
OpenList and Arcane were already absent from the running-service inventory;
this run cannot claim their acceptance. USB urb -121 messages existed before
testing and were not treated as codec regressions. No codec WARN/Oops or
watchdog reboot occurred. Long-duration JPEG throughput is not yet measured.

Raw logs, 1080p JPEG and ffmpeg H.264 outputs: evidence/jpeg-2026-11-14/.
Reproduction: cross-build tests/encode-test.c to build/encode-test, stage it
and meson-venc.ko in /root/venc-deploy with the firmware; run
scripts/jpeg-matrix.sh detached. This is a target-specific test script.
