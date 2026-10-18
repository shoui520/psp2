/*
 * SceDipswForDriver (os0:kd/sysmem.skprx): the DIP switches, each export declared once, with
 * sysmem's definition as the prototype.
 */
#pragma once

#include <stdint.h>
#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* the DIP switch words the KBL parameters carry (sysmem copies them at boot) */
typedef struct SceDipsw {
	uint32_t cp_timestamp_1;
	uint16_t cp_version;
	uint16_t cp_build_id;
	uint32_t cp_timestamp_2;
	uint32_t aslr_seed;
	uint32_t sce_sdk_flags;		/* 0x10 */
	uint32_t shell_flags;
	uint32_t debug_control_flags;
	uint32_t system_control_flags;
} SceDipsw;

#ifndef __cplusplus
_Static_assert(sizeof(SceDipsw) == 0x20, "SceDipsw size");
#endif

void SceDipswForDriver_82E45FBF(unsigned int bit);	/* vitasdk: ksceKernelSetDipsw */
int SceDipswForDriver_A98FC2FD(unsigned int bit);	/* vitasdk: ksceKernelCheckDipsw */
/* vitasdk: ksceKernelGetDipswInfo */
SceUInt32 SceDipswForDriver_B2AD48BE(unsigned int index);
void SceDipswForDriver_F1F3E9FE(unsigned int bit);	/* vitasdk: ksceKernelClearDipsw */
int SceDipswForTZS_A98FC2FD(unsigned int bit);	/* vitasdk: ksceKernelCheckDipsw */
int sceKernelCheckDipsw(unsigned int bit);
int sceKernelClearDipsw(unsigned int bit);
int sceKernelSetDipsw(unsigned int bit);

#ifdef __cplusplus
}
#endif
