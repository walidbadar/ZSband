#!/usr/bin/env bash

MCU=${1:-DA14697}
SRAM_BASE_ADDR=${2:-0x20000000}

echo "Using MCU: $MCU"
echo "Loading firmware at: $SRAM_BASE_ADDR"

if [ -z "$ZEPHYR_BASE" ]; then
    echo "ZEPHYR_BASE is not set"
    exit 1
fi

JLinkExe -device "$MCU" -if SWD -speed 4000 -autoconnect 1 << EOF > jlink_output.log 2>&1
h
loadbin $ZEPHYR_BASE/build/zephyr/zephyr.bin, $SRAM_BASE_ADDR
mem32 $SRAM_BASE_ADDR, 2
q
EOF

cat jlink_output.log   # sanity check — see the raw output

# Parse SP and PC from the actual console output
VALUES=$(grep "20000000 =" jlink_output.log)
SP=0x$(echo "$VALUES" | awk '{print $3}')
PC=0x$(echo "$VALUES" | awk '{print $4}')

echo "Extracted SP=$SP PC=$PC"

if [ -z "$SP" ] || [ "$SP" = "0x" ] || [ -z "$PC" ] || [ "$PC" = "0x" ]; then
    echo "ERROR: failed to parse SP/PC — check jlink_output.log"
    exit 1
fi

cat > $ZEPHYR_BASE/build/zephyr/run.jlink << EOF2
h
loadbin $ZEPHYR_BASE/build/zephyr/zephyr.bin, $SRAM_BASE_ADDR
SetPC $PC
g
EOF2

JLinkExe -device "$MCU" -if SWD -speed 4000 -autoconnect 1 -CommanderScript $ZEPHYR_BASE/build/zephyr/run.jlink
