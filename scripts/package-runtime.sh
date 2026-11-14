#!/usr/bin/env bash
# Collect already built modules; never compiles on the production target.
set -euo pipefail
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
dest=${1:?Usage: package-runtime.sh NEW_PACKAGE_DIRECTORY}
[[ ! -e "$dest" ]] || { echo 'Refusing existing package directory' >&2; exit 1; }
for module in runtime/meson8b-runtime.ko vdec/meson-vdec.ko venc/meson-venc.ko; do
    [[ $(modinfo -F vermagic "$root/$module") == '5.15.79-meson '* ]] || {
        echo "Module does not match target kernel: $module" >&2; exit 1;
    }
done
[[ -s "$root/firmware/meson/vdec/meson8b-runtime.dtbo" ]]
if [[ -f "$root/SOURCE_COMMIT" ]]; then
    source_commit=$(cat "$root/SOURCE_COMMIT")
else
    source_commit=$(git -C "$root" rev-parse HEAD)
fi
[[ "$source_commit" =~ ^[0-9a-f]{40}$ ]] || { echo 'Invalid source commit' >&2; exit 1; }
mkdir -p "$dest/modules" "$dest/firmware" "$dest/scripts"
for module in runtime/meson8b-runtime.ko vdec/meson-vdec.ko venc/meson-venc.ko; do
    install -m 644 "$root/$module" "$dest/modules/"
done
cp -a "$root/firmware/meson" "$dest/firmware/"
install -m 755 "$root/scripts/runtime-device.py" "$root/scripts/install-autoload.sh" "$root/scripts/remove-autoload.sh" "$dest/scripts/"
cp -a "$root/systemd" "$dest/systemd"
install -m 644 "$root/README.md" "$dest/README.md"
install -m 644 "$root/LICENSE" "$dest/LICENSE"
cp -a "$root/patches" "$dest/patches"
cp -a "$root/config" "$dest/config"
if [[ -x "$root/build/ffmpeg-test/ffmpeg" && -x "$root/build/ffmpeg-test/ffprobe" ]]; then
    mkdir "$dest/tools"
    install -m 755 "$root/build/ffmpeg-test/ffmpeg" "$root/build/ffmpeg-test/ffprobe" "$dest/tools/"
    install -m 644 "$root/sources/ffmpeg/ffmpeg-7.0.2/COPYING.LGPLv2.1" "$dest/tools/"
fi
python3 - "$root/meson8b-runtime.dts" "$dest/TARGET_PROVIDERS.json" <<'PYCODE'
import json,re,sys
from pathlib import Path
s=Path(sys.argv[1]).read_text()
patterns={'clkc':r'clocks = <(\d+) ', 'canvas':r'amlogic,canvas = <(\d+)>',
          'pmu':r'amlogic,ao-sysctrl = <(\d+)>', 'reset':r'resets = <(\d+) '}
values={k:int(re.search(p,s)[1]) for k,p in patterns.items()}
Path(sys.argv[2]).write_text(json.dumps(values,indent=2)+'\n')
PYCODE
printf '%s\n' 5.15.79-meson > "$dest/KERNEL_RELEASE"
printf '%s\n' "$source_commit" > "$dest/SOURCE_COMMIT"
manifest=$(mktemp)
trap 'rm -f "$manifest"' EXIT
(cd "$dest"; find . -type f -printf '%P\0' | sort -z | xargs -0 sha256sum) > "$manifest"
install -m 644 "$manifest" "$dest/SHA256SUMS"
printf 'Runtime package: %s\n' "$dest"
