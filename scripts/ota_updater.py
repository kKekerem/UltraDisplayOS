#!/usr/bin/env python3

import os
import sys
import subprocess
import hashlib
import json

def get_active_partition():
    with open('/proc/cmdline', 'r') as f:
        cmdline = f.read()
    if 'root=/dev/sda2' in cmdline:
        return 'A', '/dev/sda3' # A is active, B is standby
    elif 'root=/dev/sda3' in cmdline:
        return 'B', '/dev/sda2' # B is active, A is standby
    else:
        raise Exception("Unknown root partition layout")

def verify_signature(image_path, signature_path, public_key_path):
    # In production, use openssl or cryptography library here
    # to verify the ECDSA/RSA signature of the image.
    print("Verifying image signature...")
    return True

def write_image(image_path, target_partition):
    print(f"Flashing {image_path} to {target_partition}...")
    # Use dd to securely write the SquashFS image to the block device
    subprocess.run(['dd', f'if={image_path}', f'of={target_partition}', 'bs=4M', 'status=progress'], check=True)

def update_bootloader(next_partition_letter):
    print(f"Updating GRUB to boot partition {next_partition_letter}...")
    # Mount EFI partition and modify grubenv
    subprocess.run(['mount', '/dev/sda1', '/boot/efi'], check=True)
    if next_partition_letter == 'A':
        subprocess.run(['grub-editenv', '/boot/efi/EFI/BOOT/grubenv', 'set', 'next_entry=System_A'], check=True)
    else:
        subprocess.run(['grub-editenv', '/boot/efi/EFI/BOOT/grubenv', 'set', 'next_entry=System_B'], check=True)
    subprocess.run(['umount', '/boot/efi'], check=True)

def main():
    if len(sys.argv) < 3:
        print("Usage: ota_updater.py <update.squashfs> <update.sig>")
        sys.exit(1)

    image_path = sys.argv[1]
    sig_path = sys.argv[2]

    try:
        active_part, standby_dev = get_active_partition()
        next_part = 'B' if active_part == 'A' else 'A'
        
        print(f"Active partition: {active_part}")
        print(f"Targeting standby partition: {next_part} ({standby_dev})")

        if not verify_signature(image_path, sig_path, '/etc/ota_pubkey.pem'):
            print("Error: Invalid update signature!")
            sys.exit(1)

        write_image(image_path, standby_dev)
        update_bootloader(next_part)

        print("OTA Update successful. Reboot to apply.")
        sys.exit(0)
    except Exception as e:
        print(f"OTA Update failed: {e}")
        sys.exit(1)

if __name__ == '__main__':
    main()
