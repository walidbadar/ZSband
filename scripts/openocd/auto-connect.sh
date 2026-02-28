#!/bin/bash

TARGET_CFG="${1:-renesas_smartbond.cfg}"
echo "TARGET_CFG: $TARGET_CFG"

ADAPTER_CFG="${2:-rpi-swd.cfg}"
echo "ADAPTER_CFG: $ADAPTER_CFG"

while true; do
    sudo openocd -f "$ADAPTER_CFG" -f "$TARGET_CFG"
    echo "Trying to connect..."
    sleep 1
done
