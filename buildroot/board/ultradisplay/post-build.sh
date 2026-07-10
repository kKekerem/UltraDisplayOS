#!/bin/bash
# UltraDisplay OS Post-Build Script
# Runs BEFORE rootfs image generation

set -e
TARGET_DIR=$1
REPO_DIR="$(dirname $0)/../../.."

echo "[UltraDisplay] Building Client for target architecture..."

CLIENT_BUILD_DIR="${REPO_DIR}/buildroot-client-build"
mkdir -p "${CLIENT_BUILD_DIR}"
cd "${CLIENT_BUILD_DIR}"

cmake "${REPO_DIR}" \
    -DCMAKE_TOOLCHAIN_FILE="${REPO_DIR}/buildroot-src/output/host/share/buildroot/toolchainfile.cmake" \
    -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)

echo "[UltraDisplay] Installing Client to rootfs..."
mkdir -p "${TARGET_DIR}/usr/bin"
cp client/ultradisplay_client "${TARGET_DIR}/usr/bin/"
chmod +x "${TARGET_DIR}/usr/bin/ultradisplay_client"

echo "[UltraDisplay] Post-build script completed."
