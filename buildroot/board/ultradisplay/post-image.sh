#!/bin/bash

# Generates the A/B partition layout image
# Layout:
# /dev/sda1: EFI System Partition (FAT32)
# /dev/sda2: System A (SquashFS)
# /dev/sda3: System B (SquashFS)
# /dev/sda4: Persistent Data (ext4)

BOARD_DIR="$(dirname $0)"
GENIMAGE_CFG="${BOARD_DIR}/genimage.cfg"
GENIMAGE_TMP="${BUILD_DIR}/genimage.tmp"

rm -rf "${GENIMAGE_TMP}"

# Generate the disk image using genimage
genimage \
    --rootpath "${TARGET_DIR}" \
    --tmppath "${GENIMAGE_TMP}" \
    --inputpath "${BINARIES_DIR}" \
    --outputpath "${BINARIES_DIR}" \
    --config "${GENIMAGE_CFG}"

echo "UltraDisplay OS Image built successfully: ${BINARIES_DIR}/sdcard.img"
