#!/usr/bin/env bash
# Install the checked runtime package and a reversible systemd loader.
set -euo pipefail
package=$(realpath "${1:?Usage: install-autoload.sh PACKAGE NEW_STATE_DIRECTORY}")
state=${2:?Usage: install-autoload.sh PACKAGE NEW_STATE_DIRECTORY}
[[ $EUID == 0 ]] || { echo 'Run as root on OneCloud' >&2; exit 1; }
[[ ! -e /opt/meson8b-vcodec ]] || { echo 'Refusing existing /opt/meson8b-vcodec' >&2; exit 1; }
[[ ! -e /etc/systemd/system/meson8b-vcodec.service ]] || { echo 'Service already exists' >&2; exit 1; }
python3 "$package/scripts/runtime-device.py" check "$package"
python3 "$package/scripts/runtime-device.py" apply "$package" "$state"
python3 "$package/scripts/runtime-device.py" unload
mkdir /opt/meson8b-vcodec
cp -a "$package" /opt/meson8b-vcodec/current
printf '%s\n' "$(realpath "$state")" > /opt/meson8b-vcodec/state-path
install -m 644 "$package/systemd/meson8b-vcodec.service" /etc/systemd/system/
systemctl daemon-reload
if ! systemctl enable --now meson8b-vcodec.service; then
    systemctl disable --now meson8b-vcodec.service || true
    python3 "$package/scripts/runtime-device.py" rollback "$state"
    rm /etc/systemd/system/meson8b-vcodec.service
    rm -r /opt/meson8b-vcodec
    systemctl daemon-reload
    exit 1
fi
printf 'Autoload enabled; original files retained in %s\n' "$state"
