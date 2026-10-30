#!/bin/sh
# dump-emmc.sh — self-dump your own projector's firmware from the mboot prompt
# (run AFTER boot-orchestrator.py has caught "<< MT5867 >>#"; stick inserted,
# FAT32, lots of free space). emmcbin dumpsize is HEX BYTES; it APPENDS to the
# output files, so use fresh names on reruns. Do big partitions in <=64MB
# chunks with distinct names (fatwrite retries otherwise).
# Own-device use: this reads YOUR unit's flash for backup/restore.
PYTHON=${PYTHON:-python3}
run() { # $1=file $2=partition $3=blocks(512B)
  hex=$(printf '0x%X' $(( $3 * 512 )))
  echo "=== $2 ($3 blocks) $(date +%H:%M:%S) ==="
  $PYTHON "$(dirname "$0")/mcmd.py" "usb start 0; emmcbin 0 1 $1.BIN $2 $hex"
}
run MBOOT     MBOOT       10240
run MBOOTBAK  MBOOTBAK   10240
run ENV_P17   env          128
run DTBFULL   DTB         2048
run KL        KL         49152
run VBMETA    vbmeta        128
run ROOTFS    rootfs   1024000
echo "DONE — verify .BIN sizes (blocks*512) on the stick before leaving mboot"
