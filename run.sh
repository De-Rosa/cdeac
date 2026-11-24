#!/usr/bin/env bash
# ./run.sh                       build, then boot
# ./run.sh --no-build            boot without building
# ./run.sh --reset               throw away the guest disk and start fresh
# ./run.sh -- <qemu options>     pass extra options to QEMU (e.g. -trace 'jla_*')
set -euo pipefail

# paths
ROOT="$(cd "$(dirname "$0")" && pwd)"
QEMU_BUILD="${QEMU_BUILD:-$ROOT/qemu/build}"
VM="$ROOT/vm"

# guest image and disk
IMAGE_URL="https://cloud.debian.org/images/cloud/trixie/latest/debian-13-nocloud-arm64.qcow2"
BASE="$VM/debian-13-nocloud-arm64.qcow2"
DISK="$VM/disk.qcow2"
VARS="$VM/efi-vars.fd"
DISK_SIZE=20G

# parse options
build=1
extra=()
while [ $# -gt 0 ]; do
    case "$1" in
        --no-build) build=0 ;;
        --reset) rm -f "$DISK" "$VARS" ;;
        --) shift; extra=("$@"); break ;;
        *) echo "unknown option: $1" >&2; exit 1 ;;
    esac
    shift
done

# build qemu
if [ "$build" -eq 1 ]; then
    ninja -C "$QEMU_BUILD" qemu-system-aarch64 qemu-img
fi

mkdir -p "$VM"

# download debian image
if [ ! -f "$BASE" ]; then
    echo "Downloading Debian image..."
    curl -L --fail -o "$BASE.part" "$IMAGE_URL"
    mv "$BASE.part" "$BASE"
fi

# create resized guest disk
if [ ! -f "$DISK" ]; then
    cp "$BASE" "$DISK"
    "$QEMU_BUILD/qemu-img" resize "$DISK" "$DISK_SIZE"
fi

# create uefi vars store
if [ ! -f "$VARS" ]; then
    cp "$QEMU_BUILD/pc-bios/edk2-arm-vars.fd" "$VARS"
fi

# pick accelerator for host
MACHINE="virt"
case "$(uname -s)-$(uname -m)" in
    Darwin-arm64)
        ACCEL=(-accel hvf -cpu host) ;;
    Linux-aarch64)
        if [ -r /dev/kvm ] && [ -w /dev/kvm ]; then
            MACHINE="virt,gic-version=3"
            ACCEL=(-accel kvm -cpu host)
        else
            echo "warning: /dev/kvm not accessible, using TCG (slow)" >&2
            ACCEL=(-accel tcg -cpu max)
        fi ;;
    *)
        echo "warning: host is not arm64, using TCG (slow)" >&2
        ACCEL=(-accel tcg -cpu max) ;;
esac
echo "Using -M $MACHINE ${ACCEL[*]}"

# boot guest with jla device
exec "$QEMU_BUILD/qemu-system-aarch64" \
    -M "$MACHINE" "${ACCEL[@]}" \
    -smp 4 -m 4G \
    -drive if=pflash,format=raw,readonly=on,file="$QEMU_BUILD/pc-bios/edk2-aarch64-code.fd" \
    -drive if=pflash,format=raw,file="$VARS" \
    -drive if=virtio,format=qcow2,file="$DISK" \
    -netdev user,id=net0,hostfwd=tcp::2222-:22 \
    -device virtio-net-pci,netdev=net0 \
    -device jla \
    -nographic \
    ${extra[@]+"${extra[@]}"}
