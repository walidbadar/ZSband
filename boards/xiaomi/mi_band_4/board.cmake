#
# Copyright (c) 2022 Renesas Electronics Corporation
# Copyright (c) 2025 Muhammad Waleed Badar
#
# SPDX-License-Identifier: Apache-2.0
#

board_runner_args(ezflashcli)
board_runner_args(jlink --device=DA14697)
include(${ZEPHYR_BASE}/boards/common/ezflashcli.board.cmake)
include(${ZEPHYR_BASE}/boards/common/jlink.board.cmake)
