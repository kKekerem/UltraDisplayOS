#!/bin/bash

# UltraDisplay OS Post-Image Script
# Generates:
#   1. sdcard.img  - A/B partition disk image for direct flashing
#   2. ultradisplay-os.iso - Hybrid bootable ISO (BIOS + UEFI)

set -e

BOARD_DIR="$(dirname $0)"
BINARIES_DIR="${BINARIES_DIR:-${BUILD_DIR}/../images}"

# ============================================================
# Part 1: Generate the A/B partition disk image via genimage
# ============================================================
GENIMAGE_CFG="${BOARD_DIR}/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

rm -rf "${GENIMAGE_TMP}"

genimage \
    --rootpath "${TARGET_DIR}" \
    --tmppath "${GENIMAGE_TMP}" \
    --inputpath "${BINARIES_DIR}" \
    --outputpath "${BINARIES_DIR}" \
    --config "${GENIMAGE_CFG}"

echo "[UltraDisplay] sdcard.img built successfully."

# ============================================================
# Part 2: Build a hybrid bootable ISO (BIOS + UEFI)
# ============================================================

ISO_DIR="${BUILD_DIR}/iso-root"
rm -rf "${ISO_DIR}"
mkdir -p "${ISO_DIR}/boot/grub/i386-pc"
mkdir -p "${ISO_DIR}/EFI/BOOT"
mkdir -p "${ISO_DIR}/live"

# Copy kernel
cp "${BINARIES_DIR}/bzImage" "${ISO_DIR}/boot/vmlinuz"

# Copy root filesystem
cp "${BINARIES_DIR}/rootfs.squashfs" "${ISO_DIR}/live/filesystem.squashfs"

# Copy initrd if available (for live boot)
if [ -f "${BINARIES_DIR}/rootfs.cpio.xz" ]; then
    cp "${BINARIES_DIR}/rootfs.cpio.xz" "${ISO_DIR}/boot/initrd.img"
elif [ -f "${BINARIES_DIR}/rootfs.cpio" ]; then
    cp "${BINARIES_DIR}/rootfs.cpio" "${ISO_DIR}/boot/initrd.img"
fi

# Copy GRUB config for the ISO
cp "${BOARD_DIR}/grub-iso.cfg" "${ISO_DIR}/boot/grub/grub.cfg"

# --- EFI Boot Setup ---
# Create a small FAT image containing the EFI bootloader
EFI_IMG="${BINARIES_DIR}/efi-boot.img"
rm -f "${EFI_IMG}"
dd if=/dev/zero of="${EFI_IMG}" bs=1M count=4 2>/dev/null
mkfs.fat -F 12 "${EFI_IMG}" >/dev/null
mmd -i "${EFI_IMG}" ::EFI ::EFI/BOOT
mcopy -i "${EFI_IMG}" "${BINARIES_DIR}/grub-efi.efi" ::EFI/BOOT/BOOTX64.EFI
mcopy -i "${EFI_IMG}" "${BOARD_DIR}/grub-iso.cfg" ::EFI/BOOT/grub.cfg
cp "${EFI_IMG}" "${ISO_DIR}/efi-boot.img"

# Also copy the EFI binary directly for some firmware
cp "${BINARIES_DIR}/grub-efi.efi" "${ISO_DIR}/EFI/BOOT/BOOTX64.EFI"

# --- BIOS Boot Setup ---
# Generate BIOS GRUB boot image for El Torito
GRUB_MODULES="biosdisk iso9660 normal search linux configfile part_msdos part_gpt fat ext2 ls cat echo test true"
grub-mkimage -O i386-pc -o "${ISO_DIR}/boot/grub/core.img" \
    -p /boot/grub \
    ${GRUB_MODULES}

# Concatenate cdboot.img + core.img to form the BIOS El Torito image
cat /usr/lib/grub/i386-pc/cdboot.img "${ISO_DIR}/boot/grub/core.img" \
    > "${ISO_DIR}/boot/grub/i386-pc/eltorito.img"

# --- Build the Hybrid ISO ---
xorriso -as mkisofs \
    -o "${BINARIES_DIR}/ultradisplay-os.iso" \
    -R -J -joliet-long \
    -V "UltraDisplay" \
    -isohybrid-mbr /usr/lib/grub/i386-pc/boot_hybrid.img \
    -b boot/grub/i386-pc/eltorito.img \
        -no-emul-boot \
        -boot-load-size 4 \
        -boot-info-table \
        --grub2-boot-info \
    -eltorito-alt-boot \
    -e efi-boot.img \
        -no-emul-boot \
        -isohybrid-gpt-basdat \
    -append_partition 2 0xef "${EFI_IMG}" \
    "${ISO_DIR}"

echo "[UltraDisplay] Bootable hybrid ISO built: ${BINARIES_DIR}/ultradisplay-os.iso"
echo "[UltraDisplay] Post-image script completed successfully."
