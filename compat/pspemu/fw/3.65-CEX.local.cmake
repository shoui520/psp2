# 3.65-CEX: embedded PocketStation BIOS ROM, GXM shaders, and LED assets
# (extracted into PSP2_FW_DIR before configuring)
set(PSPEMU_PAYLOAD_DIR "${PSP2_FW_DIR}/3.65-CEX/payload/eboot")
set(MODULE_DEFINES
	PSPEMU_POCS_BIOS_BIN="${PSPEMU_PAYLOAD_DIR}/pocs_bios.bin"
	PSPEMU_POCS_TEX_V_GXP="${PSPEMU_PAYLOAD_DIR}/pocs_tex_v.gxp"
	PSPEMU_POCS_TEX_F_GXP="${PSPEMU_PAYLOAD_DIR}/pocs_tex_f.gxp"
	PSPEMU_POCS_LED_SHADOW_BIN="${PSPEMU_PAYLOAD_DIR}/pocs_led_shadow.bin"
	PSPEMU_POCS_LED_LARGE_BIN="${PSPEMU_PAYLOAD_DIR}/pocs_led_large.bin"
	PSPEMU_POCS_LED_SMALL_BIN="${PSPEMU_PAYLOAD_DIR}/pocs_led_small.bin"
)
set(MODULE_DEPENDS
	${PSPEMU_PAYLOAD_DIR}/pocs_bios.bin
	${PSPEMU_PAYLOAD_DIR}/pocs_tex_v.gxp
	${PSPEMU_PAYLOAD_DIR}/pocs_tex_f.gxp
	${PSPEMU_PAYLOAD_DIR}/pocs_led_shadow.bin
	${PSPEMU_PAYLOAD_DIR}/pocs_led_large.bin
	${PSPEMU_PAYLOAD_DIR}/pocs_led_small.bin
)
