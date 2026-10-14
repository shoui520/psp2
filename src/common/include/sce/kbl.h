/*
 * SceKblForKernel: services of the non-secure kernel boot loader (NSKBL) to the
 * boot-time modules. Names are not known yet (the NSKBL debug build in slb2
 * will give them); the comments say what the callers show.
 */

#pragma once

#include <psp2kern/types.h>

/* load the modules named in paths (os0:kd/ relative), UIDs to uids */
int SceKblForKernel_6D7A1F18(const char *const *paths, SceUID *uids, int count, int flags);
/* start loaded modules, passing (args, argp) to each */
int SceKblForKernel_9A92436E(const SceUID *uids, int count, SceSize args, void *argp);

int SceKblForKernel_C8F4DE71(int id);			/* boolean query by id (0xc1, 0xd2) */
int SceKblForKernel_8A416887(void);			/* boolean */
int SceKblForKernel_5945F065(void);			/* boolean */
int SceKblForKernel_C3DDDE15(void);			/* boolean */
int SceKblForKernel_CE94F329(void);			/* boolean: load deci4p_sdbgp/sdrfp */
int SceKblForKernel_B6D00D6D(void);			/* 0: unit has a built-in panel */
int SceKblForKernel_D3A516D5(SceUInt32 *flags);		/* bits 0x9: LCD (else OLED) panel */
int SceKblForKernel_838466E9(void);			/* 0: unit has HDMI */

int SceKblForKernel_261F2747(void);
int SceKblForKernel_79241ACF(void);

int SceKblForKernel_DDB3A1A8(SceSize args);
int SceKblForKernel_08E9FAEB();				/* stored in the boot params, +0x2fc */
int SceKblForKernel_13A5ABEF();				/* stored in the boot params, +0x300 */
SceUInt32 SceKblForKernel_9B868276(void);		/* value for boot params +0x304 */
SceUInt32 SceKblForKernel_C011935A(void);		/* value for boot params +0x308 */
int SceKblForKernel_1DB28F02(void);
int SceKblForKernel_9F4F3F98(void);
int SceKblForKernel_161D6FCC(void);
int SceKblForKernel_C7B77991(void);
