# Deployment and acceptance status, 2026-11-14

Final driver is runtime-only on original 5.15.79-meson, with no reboot or
/boot writes. JPEG/HEVC and concurrent B-frame input are validated on
OneCloud. The systemd loader is reversible and its start/stop, failure
cleanup and removal were tested. The final installation is intended to
leave codecs available, with original files retained in its state backup.

See evidence/HEVC-ACCEPTANCE-2026-11-14.md and
BFRAME-TRANSCODE-ACCEPTANCE-2026-11-14.md for frame counts, hash/PSNR checks,
malformed data boundaries and mixed-load run length. HEVC supports Main
8-bit linear output. Main10 and tiles remain explicitly unsupported; no
claim of arbitrary-stream or indefinite production soak is made.

Original WanOS ffmpeg remains installed. The independent static tool has
HEVC M2M decoding and a separate FFmpeg V4L2 drain/POLLERR patch. Both
sources and exact build flags are in the deliverable. On malformed data
ffmpeg may output partial frames; exit code alone is not acceptance.
Use scripts/verify-hashes.py and the recorded frame counts for correctness.

OpenList and Arcane were found stopped at the inherited baseline; their
original containers were restarted without configuration changes. They,
SublinkPro, VoCat and Home Assistant remained running during mixed-load
acceptance. The final HTTP and container checks are recorded separately.

The local linux-media RFC draft is prepared but unsubmitted. A current
mainline static-DT/power-domain port and review are still required for
upstream acceptance. No email or external publication was requested.
