#!/usr/bin/env bash
# Generated for this exact ThinkPad X380 Yoga firmware image.
# Writes and verifies the IFD BIOS region; no automatic backup or reboot.
set -euo pipefail

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
image_name=coreboot.bin
expected_sha256=e11f266f5392f9872fdfbb3b83a7526c5e1c30adcf5b4236eaa17d63078c2c64
expected_size=16777216
image="$script_dir/$image_name"
dmi_dir=/sys/class/dmi/id

fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }

for tool in sha256sum stat flashrom sync grep; do
    command -v "$tool" >/dev/null 2>&1 || fail "Missing tool: $tool"
done
[[ -f "$image" ]] || fail "Image not found: $image"
[[ "$(stat -c %s -- "$image")" == "$expected_size" ]] || fail 'Image must be exactly 16 MiB.'
actual_sha256="$(sha256sum -- "$image")"
actual_sha256="${actual_sha256%% *}"
[[ "$actual_sha256" == "$expected_sha256" ]] || fail 'SHA-256 mismatch. Nothing was written.'

[[ -r "$dmi_dir/sys_vendor" && -r "$dmi_dir/product_name" ]] || fail 'Cannot read system model from DMI.'
vendor="$(<"$dmi_dir/sys_vendor")"
model="$(<"$dmi_dir/product_name")"
[[ "$vendor" == LENOVO && "$model" == *'X380 Yoga'* ]] || fail "Wrong target machine: $vendor / $model"

run_as_root=()
if (( EUID != 0 )); then
    command -v sudo >/dev/null 2>&1 || fail 'Run as root or install sudo.'
    sudo -v || fail 'Administrator access was not granted.'
    run_as_root=(sudo)
fi

log_dir="${X380_FLASH_LOG_DIR:-$script_dir}"
mkdir -p -- "$log_dir"
log_file="$log_dir/${image_name%.bin}-$(date +%Y%m%d-%H%M%S)-$$.log"
: > "$log_file"
printf 'Target: %s / %s\nImage: %s\nSHA-256: %s\nLog: %s\n' "$vendor" "$model" "$image" "$actual_sha256" "$log_file"
printf 'Native internal programmer: BIOS only. Descriptor/ME/GbE are preserved.\n'
probe_log="${log_file%.log}-probe.log"
"${run_as_root[@]}" flashrom -p internal -VV -o "$probe_log" || fail 'SPI probe failed; nothing written.'
grep -Eq 'FREG1: BIOS region .* is read-write\.' "$probe_log" || fail 'BIOS region is not confirmed writable; nothing written.' 
printf 'Keep the AC adapter connected. Do not interrupt writing or verification.\n'

if "${run_as_root[@]}" flashrom -p internal --ifd -i bios -N -w "$image" -o "$log_file"; then
    sync -f "$log_dir" || fail 'The flash operation finished, but saving the log failed. Keep the drive connected.'
    printf '\nSUCCESS: flashrom write and verification completed.\n'
    printf 'No reboot was performed. You may now run: sudo poweroff\n'
else
    status=$?
    printf '\nFAILED (exit %s). Keep the machine powered on and save this log:\n%s\n' "$status" "$log_file" >&2
    exit "$status"
fi
