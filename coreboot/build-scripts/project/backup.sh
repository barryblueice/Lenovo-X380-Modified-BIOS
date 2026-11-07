#!/usr/bin/env bash
# X380 Yoga 16 MiB SPI backup. Firmware contents are only read.
# Usage: bash backup-bios.sh [auto|bios|full] [destination-parent]
set -euo pipefail
umask 077

script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
mode="${1:-auto}"
destination_parent="${2:-$script_dir}"
expected_size=16777216
dmi_dir=/sys/class/dmi/id
fail() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
case "$mode" in auto|bios|full) ;; *) fail 'Usage: bash backup-bios.sh [auto|bios|full] [destination-parent]' ;; esac
(( $# <= 2 )) || fail 'Too many arguments.'
for tool in flashrom sha256sum stat date mkdir mv sync; do
    command -v "$tool" >/dev/null 2>&1 || fail "Missing tool: $tool"
done
[[ -r "$dmi_dir/sys_vendor" && -r "$dmi_dir/product_name" ]] || fail 'Cannot read system model from DMI.'
vendor="$(<"$dmi_dir/sys_vendor")"
model="$(<"$dmi_dir/product_name")"
version=''
if [[ -r "$dmi_dir/product_version" ]]; then version="$(<"$dmi_dir/product_version")"; fi
[[ "$vendor" == LENOVO && ( "$model" == *'X380 Yoga'* || "$version" == *'X380 Yoga'* ) ]] || fail "Wrong target machine: $vendor / $model / $version"

run_as_root=()
if (( EUID != 0 )); then
    command -v sudo >/dev/null 2>&1 || fail 'Run as root or install sudo.'
    sudo -v || fail 'Administrator access was not granted.'
    run_as_root=(sudo)
fi
mkdir -p -- "$destination_parent"
destination_parent="$(cd -- "$destination_parent" && pwd)"
out="$destination_parent"
prefix="x380-$(date +%Y%m%d-%H%M%S)-$$"
printf 'Target: %s / %s\nBackup directory: %s\n' "$vendor" "$model" "$out"

read_image() {
    local target="$1" log="$2"
    shift 2
    # Precreate as the caller so sudo does not leave inaccessible root-owned files.
    : > "$target"
    : > "$log"
    if "${run_as_root[@]}" flashrom -p internal "$@" -r "$target" -o "$log"; then
        if [[ "$(stat -c %s -- "$target")" == "$expected_size" ]]; then
            return 0
        fi
        printf 'Unexpected file size; this is not a validated backup.\n' >&2
    fi
    mv -- "$target" "$target.partial-not-a-backup"
    return 1
}

scope=''
image=''
if [[ "$mode" != bios ]]; then
    image="$out/$prefix-FULL_SPI_16MiB.bin"
    if read_image "$image" "$out/$prefix-full-read.log"; then
        scope=FULL_SPI
    elif [[ "$mode" == full ]]; then
        printf 'Full-chip read failed. No successful backup. See: %s\n' "$out/$prefix-full-read.log" >&2
        exit 1
    else
        printf 'Full-chip read failed; see %s. Trying BIOS-only read.\n' "$out/$prefix-full-read.log"
    fi
fi

if [[ -z "$scope" ]]; then
    image="$out/$prefix-BIOS_ONLY_NOT_FULL_CHIP_16MiB.bin"
    if ! read_image "$image" "$out/$prefix-bios-read.log" --ifd -i bios; then
        printf 'BIOS-region read also failed. No successful backup. Logs: %s\n' "$out" >&2
        exit 1
    fi
    scope=BIOS_ONLY
fi

image_name="${image##*/}"
(cd -- "$out" && sha256sum -- "$image_name" > "$prefix-SHA256SUMS.txt")
{
    printf 'Scope: %s\nModel: %s / %s / %s\nFile: %s\nSize: %s\n' "$scope" "$vendor" "$model" "$version" "$image_name" "$expected_size"
    printf 'The SHA-256 describes the saved file; it is not a second independent flash read.\n'
    printf 'This backup command did not write or erase firmware.\n'
    if [[ "$scope" == BIOS_ONLY ]]; then
        printf '\nWARNING: THIS IS NOT A FULL-CHIP BACKUP.\n'
        printf 'Only the IFD BIOS region was read. Bytes outside that region are placeholders and must not be restored.\n'
        printf 'Do not write this entire file with an external programmer. It does not back up ME, descriptor, or GbE.\n'
        printf 'For the current unchanged descriptor layout, BIOS-only restoration uses:\n'
        printf 'sudo flashrom -p internal --ifd -i bios -N -w "%s"\n' "$image_name"
    else
        printf '\nFull-chip read completed successfully, including descriptor, GbE, ME and BIOS regions.\n'
    fi
} > "$out/$prefix-README.txt"
sync -f "$out" || fail 'Flushing backup files failed. Keep the drive connected; do not treat the backup as safely stored.'
printf '\nSUCCESS: %s backup saved.\n%s\n' "$scope" "$image"
if [[ "$scope" == BIOS_ONLY ]]; then
    printf 'WARNING: BIOS-only backup; NOT suitable for full-chip restoration. Read %s.\n' "$prefix-README.txt"
fi
printf 'Checksums and flashrom logs are in: %s\n' "$out"
