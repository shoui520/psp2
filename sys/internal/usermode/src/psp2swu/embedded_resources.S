/*
 * Embedded resources and fonts carried verbatim by pup:psp2swu.self:
 * common_resource.rco, updater_plugin.rco, and the four system UI fonts
 * (Latin, Japanese, Chinese, Korean). Not kept in the source tree; each
 * path is set by fw/<FW>.local.cmake from ${PSP2_FW_DIR}.
 */
	.section .rodata.g_commonResourceRco, "a", %progbits
	.balign 16
	.global g_commonResourceRco
	.type g_commonResourceRco, %object
g_commonResourceRco:
	.incbin PSP2SWU_COMMON_RESOURCE_RCO
	.size g_commonResourceRco, . - g_commonResourceRco

	.section .rodata.g_updaterPluginRco, "a", %progbits
	.balign 16
	.global g_updaterPluginRco
	.type g_updaterPluginRco, %object
g_updaterPluginRco:
	.incbin PSP2SWU_UPDATER_PLUGIN_RCO
	.size g_updaterPluginRco, . - g_updaterPluginRco

	.section .rodata.g_latinFontOtf, "a", %progbits
	.balign 16
	.global g_latinFontOtf
	.type g_latinFontOtf, %object
g_latinFontOtf:
	.incbin PSP2SWU_LTN_FONT_OTF
	.size g_latinFontOtf, . - g_latinFontOtf

	.section .rodata.g_japaneseFontOtf, "a", %progbits
	.balign 16
	.global g_japaneseFontOtf
	.type g_japaneseFontOtf, %object
g_japaneseFontOtf:
	.incbin PSP2SWU_JPN_FONT_OTF
	.size g_japaneseFontOtf, . - g_japaneseFontOtf

	.section .rodata.g_chineseFontTtf, "a", %progbits
	.balign 16
	.global g_chineseFontTtf
	.type g_chineseFontTtf, %object
g_chineseFontTtf:
	.incbin PSP2SWU_CHN_FONT_TTF
	.size g_chineseFontTtf, . - g_chineseFontTtf

	.section .rodata.g_koreanFontOtf, "a", %progbits
	.balign 16
	.global g_koreanFontOtf
	.type g_koreanFontOtf, %object
g_koreanFontOtf:
	.incbin PSP2SWU_KR_FONT_OTF
	.size g_koreanFontOtf, . - g_koreanFontOtf

	.section .note.GNU-stack, "", %progbits
