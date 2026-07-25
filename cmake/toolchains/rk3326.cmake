# Anbernic RG351P/M/V, RG353P/M/V — Rockchip RK3326
#
#   CPU  4x ARM Cortex-A35 @ 1.3GHz, aarch64
#   GPU  Mali-G31 (GLES 2.0 / 3.2 via panfrost or vendor blobs)
#   OS   ArkOS, ROCKNIX, Batocera, JELOS
#
# See aarch64-handheld.cmake for the compile-check vs shippable distinction.

set(WREEL_TARGET_ID "rk3326")
set(WREEL_SOC_TUNE  "cortex-a35")

include("${CMAKE_CURRENT_LIST_DIR}/aarch64-handheld.cmake")
