#!/bin/sh
set -eu
cd /root/venc-deploy
exec >jpeg-matrix.log 2>&1
cleanup() { rmmod meson_venc 2>/dev/null || true; kill -TERM "$guard_pid" 2>/dev/null || true; }
nohup python3 wdt-guard.py --minutes 10 --log /root/venc-deploy/jpeg-guard.log >jpeg-guard.out 2>&1 &
guard_pid=$!
trap cleanup EXIT
sleep 2
cat /sys/class/watchdog/watchdog0/state
before=$(dmesg | wc -l)
insmod meson-venc.ko
for cfg in '320 240 NV12 40' '320 240 NV12 90' '320 240 NV21 90' '320 240 YU12 90' '854 480 YU12 90' '1280 720 NV12 90' '1920 1080 NV12 90' '1920 1080 YU12 90'; do
 set -- $cfg
 name="$1-$2-$3-$4"
 timeout 60 ./encode-test /dev/video0 /tmp/jpeg-$name.mjpg /tmp/jpeg-$name.nv12 "$1" "$2" 5 "$3" "$4" JPEG
 /usr/local/bin/ffmpeg -hide_banner -v info -f rawvideo -pixel_format nv12 -video_size "$1"x"$2" -framerate 25 -color_range pc -i /tmp/jpeg-$name.nv12 -i /tmp/jpeg-$name.mjpg -lavfi '[0:v]format=yuvj420p,setpts=N/(25*TB)[ref];[1:v]setpts=N/(25*TB)[dec];[dec][ref]psnr' -f null - 2>&1 | tail -3
 rm /tmp/jpeg-$name.nv12
 echo "PASS $name"
done
timeout 60 ./encode-test /dev/video0 /tmp/jpeg-h264-regress.h264 /tmp/jpeg-h264-regress.nv12 320 240 8 NV12 90 H264
/usr/local/bin/ffmpeg -v error -i /tmp/jpeg-h264-regress.h264 -f null -
timeout 60 /usr/local/bin/ffmpeg -v info -f lavfi -i testsrc2=size=1920x1080:rate=30 -frames:v 12 -pix_fmt nv12 -c:v h264_v4l2m2m -f h264 -y /tmp/jpeg-ffmpeg-regress.h264 2>jpeg-ffmpeg-regress.log
dmesg | tail -n +$((before + 1))
echo DONE
