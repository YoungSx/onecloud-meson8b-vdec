# Kernel and FFmpeg patches

[series](series) contains the single current additive Linux 5.15.79 patch.
Apply it after the matching [Armbian baseline](../baseline/series). It adds
runtime/decoder/encoder modules under drivers/media/platform/meson/meson8b-vcodec.
The deployment still uses a removable runtime overlay.

[ffmpeg-7.0.2-v4l2-drain.patch](ffmpeg-7.0.2-v4l2-drain.patch) is applied only
to the independent static test tool, not to the production FFmpeg.

`research/` contains superseded platform/binding/static-DT experiments.
They are not in the current series and must not be stacked on the current patch.
CI compares a freshly exported current patch against the checked-in file.
