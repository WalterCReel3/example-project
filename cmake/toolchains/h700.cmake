# Anbernic RG35XX Plus/H/SP, RG40XX, RG28XX — Allwinner H700
#
#   CPU  4x ARM Cortex-A53 @ 1.5GHz, aarch64
#   GPU  Mali-G31 MP2 (GLES 2.0 / 3.2)
#   OS   muOS, ROCKNIX, Batocera, stock Anbernic firmware
#
# NOTE: the *original* RG35XX (2022) is a completely different machine —
# Allwinner F1C100s, ARM926EJ-S, armv5te, no FPU — and is out of scope. This
# file is only for the H700 generation.
#
# See aarch64-handheld.cmake for the compile-check vs shippable distinction.

set(WREEL_TARGET_ID "h700")
set(WREEL_SOC_TUNE  "cortex-a53")

include("${CMAKE_CURRENT_LIST_DIR}/aarch64-handheld.cmake")
