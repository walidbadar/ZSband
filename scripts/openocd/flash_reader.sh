#!/bin/bash

# Simple Renesas Smartbond DA1469x Flash Reader

TARGET_CFG="${1:-renesas_smartbond.cfg}"
echo "TARGET_CFG: $TARGET_CFG"

ADAPTER_CFG="${2:-rpi-swd.cfg}"
echo "ADAPTER_CFG: $ADAPTER_CFG"

FLASH_BASE="0x16000000" # DA1469x Flash Base_Addr=0x16000000
READ_SIZE="0x1000000"  # 16MB (0x1000000)
OUTPUT_FILE="output.bin"

echo "Waiting for target... (Ctrl+C to quit)"

while true; do
    echo "$(date): Trying OpenOCD..."
    sudo openocd -f "$ADAPTER_CFG" -f "$TARGET_CFG" -c init -c "reset halt" -c "dump_image ${OUTPUT_FILE} ${FLASH_BASE} ${READ_SIZE}" -c "reset" -c shutdown
    
    if [ $? -eq 0 ] && [ -f "$OUTPUT_FILE" ]; then
        FILE_SIZE=$(stat -c%s "$OUTPUT_FILE")
        echo "$(date): Success! Flash read: $FILE_SIZE bytes."
        hexdump -C "$OUTPUT_FILE" | head -2
        break
    else
        echo "$(date): Target not ready, retrying in 3 seconds..."
        sleep 3
    fi
done
