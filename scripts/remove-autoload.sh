#!/usr/bin/env bash
set -euo pipefail
[[ $EUID == 0 ]] || { echo 'Run as root' >&2; exit 1; }
root=/opt/meson8b-vcodec
state=$(cat "$root/state-path")
systemctl disable --now meson8b-vcodec.service
python3 "$root/current/scripts/runtime-device.py" rollback "$state"
rm /etc/systemd/system/meson8b-vcodec.service
rm -r "$root"
systemctl daemon-reload
printf 'Codec autoload removed, original firmware restored; backup: %s\n' "$state"
