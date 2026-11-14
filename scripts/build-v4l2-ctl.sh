#!/usr/bin/env bash
# Standalone armhf ioctl tool; no target package changes or libv4l wrapper.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
cache="$root/sources/v4l-utils"
mkdir -p "$cache"
archive="$cache/v4l-utils-1.22.1.tar.bz2"
if [[ ! -f "$archive" ]]; then
    curl -fsSL --max-time 90 https://linuxtv.org/downloads/v4l-utils/v4l-utils-1.22.1.tar.bz2 -o "$archive"
fi
printf '%s  %s\n' 65c6fbe830a44ca105c443b027182c1b2c9053a91d1e72ad849dfab388b94e31 "$archive" | sha256sum -c -
if [[ ! -d "$cache/v4l-utils-1.22.1" ]]; then tar -xjf "$archive" -C "$cache"; fi
cd "$cache/v4l-utils-1.22.1"
CC=arm-linux-gnueabihf-gcc-11 CXX=arm-linux-gnueabihf-g++-11 \
./configure --host=arm-linux-gnueabihf --disable-shared --enable-static \
  --disable-v4l2-ctl-libv4l --disable-v4l2-compliance-libv4l \
  --disable-v4l2-ctl-stream-to --disable-qv4l2 --disable-qvidcap \
  --disable-bpf --disable-libdvbv5 --without-jpeg --without-libudev
# libtool's -static only affects libtool libraries; -all-static reaches ld.
rm -f utils/v4l2-ctl/v4l2-ctl
make -C utils/v4l2-ctl -j"${JOBS:-4}" LDFLAGS=-all-static
mkdir -p "$root/build/v4l2-tools"
arm-linux-gnueabihf-strip -o "$root/build/v4l2-tools/v4l2-ctl" utils/v4l2-ctl/v4l2-ctl
if arm-linux-gnueabihf-readelf -l "$root/build/v4l2-tools/v4l2-ctl" | grep -q INTERP; then
    echo 'Expected standalone static v4l2-ctl' >&2
    exit 1
fi
printf 'Independent ioctl tool: %s\n' "$root/build/v4l2-tools/v4l2-ctl"
