#!/bin/sh
set -eu
cd /root/venc-deploy
exec >concurrent-probe.log 2>&1
cleanup() {
 rmmod meson_venc 2>/dev/null || true
 rmmod meson_vdec 2>/dev/null || true
 kill -TERM "$guard_pid" 2>/dev/null || true
}
nohup python3 wdt-guard.py --minutes 5 --log /root/venc-deploy/concurrent-guard.log >concurrent-guard.out 2>&1 &
guard_pid=$!
trap cleanup EXIT
sleep 2
rmmod meson8b_runtime
cp /lib/firmware/meson/vdec/meson8b-runtime.dtbo overlay-before-concurrency.dtbo
cp meson8b-runtime.dtbo /lib/firmware/meson/vdec/
before=$(dmesg | wc -l)
insmod meson8b-runtime.ko
insmod meson-vdec.ko
insmod meson-venc.ko
ls -l /dev/video*
timeout -k 5 60 /usr/local/bin/ffmpeg -hide_banner -v info -c:v h264_v4l2m2m -num_capture_buffers 7 -i baseline.mp4 -frames:v 60 -an -c:v h264_v4l2m2m -b:v 0 -f h264 -y /tmp/concurrent.h264 2>concurrent-ffmpeg.log
/usr/local/bin/ffmpeg -hide_banner -v error -i /tmp/concurrent.h264 -f null -
dmesg | tail -n +$((before+1))
echo DONE
