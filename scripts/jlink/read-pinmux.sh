#!/usr/bin/env bash
#
# Copyright (c) 2026 Muhammad Waleed Badar
#
# SPDX-License-Identifier: Apache-2.0
#
# Read and decode the DA1469x pin mode registers (Px_yy_MODE_REG) over
# J-Link without halting the CPU, e.g. to see how the stock firmware
# configures the pins.
#
# Usage:
#   read-pinmux.sh [MCU]     read the registers from the target
#   read-pinmux.sh -f LOG    decode a saved J-Link mem32 or OpenOCD mdw dump
#
# Equivalent OpenOCD commands:
#   mdw 0x50020A18 32
#   mdw 0x50020A98 23

P0_BASE=0x50020A18
P0_PINS=32
P1_BASE=0x50020A98
P1_PINS=23

if [ "$1" = "-f" ]; then
    LOG="$2"
    if [ ! -f "$LOG" ]; then
        echo "Usage: $0 -f LOG"
        exit 1
    fi
else
    MCU=${1:-DA14697}
    LOG=$(mktemp)
    CMD=$(mktemp)
    trap 'rm -f "$LOG" "$CMD"' EXIT

    cat > "$CMD" << EOF
mem32 $P0_BASE, $P0_PINS
mem32 $P1_BASE, $P1_PINS
q
EOF

    JLinkExe -device "$MCU" -if SWD -speed 4000 -autoconnect 1 \
        -CommanderScript "$CMD" > "$LOG" 2>&1
fi

awk -v p0_pins=$P0_PINS -v p1_pins=$P1_PINS '
BEGIN {
    split("GPIO UART_RX UART_TX UART2_RX UART2_TX UART2_CTSN UART2_RTSN " \
          "UART3_RX UART3_TX UART3_CTSN UART3_RTSN ISO_CLK ISO_DATA SPI_DI " \
          "SPI_DO SPI_CLK SPI_EN SPI2_DI SPI2_DO SPI2_CLK SPI2_EN I2C_SCL " \
          "I2C_SDA I2C2_SCL I2C2_SDA USB_SOF ADC USB PCM_DI PCM_DO PCM_FSC " \
          "PCM_CLK PDM_DATA PDM_CLK COEX_EXT_ACT COEX_SMART_ACT " \
          "COEX_SMART_PRI PORT0_DCF PORT1_DCF PORT2_DCF PORT3_DCF PORT4_DCF " \
          "CLOCK PG LCD LCD_SPI_DC LCD_SPI_DO LCD_SPI_CLK LCD_SPI_EN TIM_PWM " \
          "TIM2_PWM TIM_1SHOT TIM2_1SHOT TIM3_PWM TIM4_PWM Reserved " \
          "CMAC_DIAG0 CMAC_DIAG1 CMAC_DIAG2 CMAC_DIAGX", pid_name, " ")
    split("input pull-up pull-down output", pupd, " ")
    n = 0
}

# J-Link: "50020A18 = 00000200 ..."  OpenOCD: "0x50020a18: 00000200 ..."
tolower($1) ~ /^(0x)?50020a[0-9a-f]+ *[=:]?$/ {
    for (i = 2; i <= NF; i++) {
        if ($i ~ /^[0-9A-Fa-f]{8}$/) {
            val[n++] = strtonum("0x" $i)
        }
    }
}

END {
    if (n < p0_pins + p1_pins) {
        print "Expected " p0_pins + p1_pins " registers, found " n > "/dev/stderr"
        exit 1
    }

    printf "%-6s %-10s %-15s %-10s %s\n", "PIN", "VALUE", "FUNCTION", "PUPD", "PPOD"
    for (i = 0; i < p0_pins + p1_pins; i++) {
        port = (i < p0_pins) ? 0 : 1
        pin = (i < p0_pins) ? i : i - p0_pins
        v = val[i]
        pid = and(v, 0x3F)
        name = (pid + 1 in pid_name) ? pid_name[pid + 1] : "Reserved"
        printf "P%d_%02d  0x%08X %-15s %-10s %s\n", port, pin, v, name,
               pupd[and(rshift(v, 8), 3) + 1],
               and(rshift(v, 10), 1) ? "open-drain" : "push-pull"
    }
}
' "$LOG" || { echo "Raw output:"; cat "$LOG"; exit 1; }
