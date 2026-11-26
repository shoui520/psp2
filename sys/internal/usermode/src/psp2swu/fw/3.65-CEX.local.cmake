# 3.65-CEX: embedded RCO resources and UI fonts (extracted outside src/ by payloads.py --embedded)
set(PSP2SWU_PAYLOAD_DIR "${PSP2_FW_DIR}/3.65-CEX/payload/psp2swu")
set(MODULE_DEFINES
	PSP2SWU_COMMON_RESOURCE_RCO="${PSP2SWU_PAYLOAD_DIR}/common_resource.rco"
	PSP2SWU_UPDATER_PLUGIN_RCO="${PSP2SWU_PAYLOAD_DIR}/updater_plugin.rco"
	PSP2SWU_LTN_FONT_OTF="${PSP2SWU_PAYLOAD_DIR}/ltn_font.otf"
	PSP2SWU_JPN_FONT_OTF="${PSP2SWU_PAYLOAD_DIR}/jpn_font.otf"
	PSP2SWU_CHN_FONT_TTF="${PSP2SWU_PAYLOAD_DIR}/chn_font.ttf"
	PSP2SWU_KR_FONT_OTF="${PSP2SWU_PAYLOAD_DIR}/kr_font.otf"
)
set(MODULE_DEPENDS
	${PSP2SWU_PAYLOAD_DIR}/common_resource.rco
	${PSP2SWU_PAYLOAD_DIR}/updater_plugin.rco
	${PSP2SWU_PAYLOAD_DIR}/ltn_font.otf
	${PSP2SWU_PAYLOAD_DIR}/jpn_font.otf
	${PSP2SWU_PAYLOAD_DIR}/chn_font.ttf
	${PSP2SWU_PAYLOAD_DIR}/kr_font.otf
)
