# 1080p B-frame input concurrent transcode

The vendor private encoder DCT area was 0x6ba000 bytes, though this V4L2
path feeds raw MFDIN planes. Allocate PAGE_ALIGN(canvas_stride *
ALIGN(height,16) * 3/2) instead and preserve negotiated format/zero padding.
This saves about 3.8 MiB CMA at 1080p and permits simultaneous H.264 decode
with four references and two reordered B pictures under the existing 64 MiB
CMA reservation. No services are stopped and no kernel reboot is used.

The final M8 EOS batch can be emitted in decode order. Snapshot each
picture's firmware status and packet-offset PTS before returning buffers,
then order that drain batch by presentation timestamp. Sorting byte offsets
would be incorrect for B frames. Fix is applied only to Meson8b draining.

Verification: 60-frame 1080p source with max_num_ref_frames=4, bframes=2;
initial three complete runs exposed final two pictures swapped. After the
PTS ordering fix, all 60 pictures match expected frame positions; PSNR
average 45.82 dB, minimum 45.03 dB. Ten mixed-load sessions then returned
all 60 frames and were software-decodable. Raw logs/PSNR:
evidence/bframes-2026-11-14 and hevc-2026-11-14/long-results.json.

JPEG mixed-load test: 3 x 300 1080p JPEG pictures, standard V4L2 client
checks payload, timestamps, KEYFRAME, exclusive open and LAST/EOS. Each
compressed stream was probed as 300 images and software-decoded. Raw
references were sent to /dev/null to avoid filling the 496 MiB /tmp tmpfs.
This is a bounded 900-frame validation, not indefinite production soak.
