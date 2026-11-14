#!/usr/bin/env bash
# Source companion for the already installed, immutable runtime payload.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
dest=${1:?Usage: package-source.sh NEW_SOURCE_DIRECTORY}
[[ ! -e "$dest" ]] || { echo 'Refusing existing source directory' >&2; exit 1; }
payload_commit=bd1cb79ad502d0b8cdcf9667a0ceb0ca0f7876ab
linux_archive="$root/sources/linux-5.15.79.tar.xz"
mkdir -p "$root/sources"
if [[ ! -f "$linux_archive" ]]; then
    if [[ -f "$root/../sources/linux-5.15.79.tar.xz" ]]; then
        cp "$root/../sources/linux-5.15.79.tar.xz" "$linux_archive"
    else
        curl -fsSL --max-time 180 https://cdn.kernel.org/pub/linux/kernel/v5.x/linux-5.15.79.tar.xz -o "$linux_archive"
    fi
fi
mkdir -p "$dest/payload-source" "$dest/sources/ffmpeg" "$dest/sources/vendor" \
  "$dest/sources/v4l-utils" "$dest/supplement/scripts" "$dest/tools"
git -C "$root" archive "$payload_commit" Makefile README.md LICENSE config \
  firmware meson8b-runtime.dts patches runtime scripts systemd tests vdec venc |
  tar -xf - -C "$dest/payload-source"
git -C "$root" archive HEAD baseline docs scripts evidence |
  tar -xf - -C "$dest/supplement"
cp "$root/sources/ffmpeg/ffmpeg-7.0.2.tar.xz" "$dest/sources/ffmpeg/"
cp "$linux_archive" "$dest/sources/"
cp "$root/sources/v4l-utils/v4l-utils-1.22.1.tar.bz2" "$dest/sources/v4l-utils/"
cp "$root/build/v4l2-tools/v4l2-ctl" "$dest/tools/"
cp "$root/sources/v4l-utils/v4l-utils-1.22.1/COPYING" "$dest/tools/COPYING.v4l-utils"
python3 - "$root" "$dest" <<'PYCODE'
import hashlib
import json
from pathlib import Path
import shutil
import sys
root, dest = map(Path, sys.argv[1:])
metadata = json.loads((root / 'firmware/sources.json').read_text())
for codec, entry in metadata.items():
    if not isinstance(entry, dict):
        continue
    for item in entry.get('files', [entry]):
        source = root / 'sources/vendor' / item['path']
        if hashlib.sha256(source.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Vendor source checksum mismatch: ' + str(source))
        target = dest / 'sources/vendor' / item['path']
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, target)
for relative, digest in {
    'sources/linux-5.15.79.tar.xz': 'cba39031dbc0eed0785b8afdc8c58cf23df83e47001b2354fa44486ae699c154',
    'sources/ffmpeg/ffmpeg-7.0.2.tar.xz': '8646515b638a3ad303e23af6a3587734447cb8fc0a0c064ecdb8e95c4fd8b389',
    'sources/v4l-utils/v4l-utils-1.22.1.tar.bz2': '65c6fbe830a44ca105c443b027182c1b2c9053a91d1e72ad849dfab388b94e31',
}.items():
    if hashlib.sha256((dest / relative).read_bytes()).hexdigest() != digest:
        raise ValueError('Upstream archive checksum mismatch: ' + relative)
PYCODE
printf '%s\n' "$payload_commit" > "$dest/PAYLOAD_SOURCE_COMMIT"
git -C "$root" rev-parse HEAD > "$dest/SUPPLEMENT_SOURCE_COMMIT"
cat > "$dest/README.txt" <<'DOC'
payload-source/ contains the exact sources of the installed runtime package.
sources/ supplies original Linux, FFmpeg and v4l-utils archives and pinned firmware
microcode sources. Copy sources/ into payload-source/ before building offline.
supplement/ adds current build scripts, matching Armbian baseline, docs and
acceptance evidence; use its scripts and baseline in payload-source/.
See supplement/docs/reproduce.md. tools/v4l2-ctl is a separate static ioctl
tool, not a modification of the installed runtime package. No boot image
is supplied here. Source and binary licenses are included with their files.
DOC
manifest=$(mktemp)
trap 'rm -f "$manifest"' EXIT
(cd "$dest"; find . -type f -printf '%P\0' | sort -z | xargs -0 sha256sum) > "$manifest"
install -m 644 "$manifest" "$dest/SHA256SUMS"
printf 'Source companion: %s\n' "$dest"
