/*
 * pocs_assets.c: verbatim PocketStation binary assets embedded via
 * fw/<FW>.local.cmake from ${PSP2_FW_DIR} (no binary files in src/):
 * - PocketStation ARM7TDMI 16 KiB kernel BIOS ROM (.data, runtime-patched)
 * - PocketStation GXM vertex and fragment GXP shader programs (.rodata)
 * - PocketStation LCD shadow and LED indicator textures/palettes (.rodata)
 */
#include "pocs_internal.h"

#define POCS_INCBIN_STR(x) #x
#define POCS_INCBIN_EVAL(x) POCS_INCBIN_STR(x)

__asm__(
	".section .data.g_pocsBiosRom, \"aw\", %progbits\n"
	".balign 16\n"
	".global g_pocsBiosRom\n"
	".type g_pocsBiosRom, %object\n"
	"g_pocsBiosRom:\n"
	".incbin \"" PSPEMU_POCS_BIOS_BIN "\"\n"
	".size g_pocsBiosRom, . - g_pocsBiosRom\n"

	".section .rodata.g_pocsVertexShaderGxp, \"a\", %progbits\n"
	".balign 16\n"
	".global g_pocsVertexShaderGxp\n"
	".type g_pocsVertexShaderGxp, %object\n"
	"g_pocsVertexShaderGxp:\n"
	".incbin \"" PSPEMU_POCS_TEX_V_GXP "\"\n"
	".size g_pocsVertexShaderGxp, . - g_pocsVertexShaderGxp\n"

	".section .rodata.g_pocsFragmentShaderGxp, \"a\", %progbits\n"
	".balign 16\n"
	".global g_pocsFragmentShaderGxp\n"
	".type g_pocsFragmentShaderGxp, %object\n"
	"g_pocsFragmentShaderGxp:\n"
	".incbin \"" PSPEMU_POCS_TEX_F_GXP "\"\n"
	".size g_pocsFragmentShaderGxp, . - g_pocsFragmentShaderGxp\n"

	".section .rodata.g_pocsLedShadowData, \"a\", %progbits\n"
	".balign 4\n"
	".global g_pocsLedShadowData\n"
	".type g_pocsLedShadowData, %object\n"
	"g_pocsLedShadowData:\n"
	".incbin \"" PSPEMU_POCS_LED_SHADOW_BIN "\"\n"
	".size g_pocsLedShadowData, . - g_pocsLedShadowData\n"

	".section .rodata.g_pocsLedLargeData, \"a\", %progbits\n"
	".balign 16\n"
	".global g_pocsLedLargeData\n"
	".type g_pocsLedLargeData, %object\n"
	"g_pocsLedLargeData:\n"
	".incbin \"" PSPEMU_POCS_LED_LARGE_BIN "\"\n"
	".size g_pocsLedLargeData, . - g_pocsLedLargeData\n"

	".section .rodata.g_pocsLedSmallData, \"a\", %progbits\n"
	".balign 16\n"
	".global g_pocsLedSmallData\n"
	".type g_pocsLedSmallData, %object\n"
	"g_pocsLedSmallData:\n"
	".incbin \"" PSPEMU_POCS_LED_SMALL_BIN "\"\n"
	".size g_pocsLedSmallData, . - g_pocsLedSmallData\n"
	".previous\n"
);
