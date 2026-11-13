psp2_module(os0_kd_bootimage_usb_ether_rtl KIND kernel COMPILER gcc48
	SOURCES usb_ether_rtl.c rtl_register.c
	IMPORTS sceKernelDeleteEventFlag SceUsbdForDriver_216F108D)

# The patches for the controller's PLA and USB MCUs, assembled from source
# (tools/rtlmcuasm.py) into phy_patch.h, which usb_ether_rtl.c includes.
set(rtlmcu "${CMAKE_CURRENT_BINARY_DIR}/rtlmcu")
file(MAKE_DIRECTORY "${rtlmcu}")
add_custom_command(OUTPUT "${rtlmcu}/phy_patch.h"
	COMMAND python3 -I "${PSP2_SRC}/tools/rtlmcuasm.py" "${rtlmcu}/phy_patch.h"
		"plaPhyPatch=${CMAKE_CURRENT_SOURCE_DIR}/pla_patch.asm"
		"usbPhyPatch=${CMAKE_CURRENT_SOURCE_DIR}/usb_patch.asm"
	DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/pla_patch.asm" "${CMAKE_CURRENT_SOURCE_DIR}/usb_patch.asm"
		"${PSP2_SRC}/tools/rtlmcuasm.py" VERBATIM)
set_source_files_properties(usb_ether_rtl.c PROPERTIES
	COMPILE_OPTIONS "-I${rtlmcu}"
	OBJECT_DEPENDS "${rtlmcu}/phy_patch.h")
