# os0:kd/wlanbt_robin_img_ax.skprx - image container module
psp2_module(os0_kd_wlanbt_robin_img_ax KIND kernel COMPILER gcc48 SOURCES
	image.S
)

# The WLAN/Bluetooth firmware, built from firmware/ (its Makefile: GCC 4.8 for the ARM
# code, SDCC for the two radio controllers' 8051 programs, pack.py for the download
# blocks) into wlanbt_robin_img_ax.bin, which image.S carries.
set(robin "${CMAKE_CURRENT_BINARY_DIR}/robin")
file(GLOB_RECURSE robin_sources CONFIGURE_DEPENDS
	"${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.c" "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.S"
	"${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.h" "${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.ld"
	"${CMAKE_CURRENT_SOURCE_DIR}/firmware/*.py")
add_custom_command(OUTPUT "${robin}/wlanbt_robin_img_ax.bin"
	COMMAND make -C "${CMAKE_CURRENT_SOURCE_DIR}/firmware" -j8 "O=${robin}" image
	DEPENDS ${robin_sources} "${CMAKE_CURRENT_SOURCE_DIR}/firmware/Makefile"
	VERBATIM)
target_compile_definitions(os0_kd_wlanbt_robin_img_ax PRIVATE IMAGE_BIN="${robin}/wlanbt_robin_img_ax.bin")
set_source_files_properties(image.S PROPERTIES OBJECT_DEPENDS "${robin}/wlanbt_robin_img_ax.bin")
