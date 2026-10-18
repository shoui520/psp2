/* Sony: prxexport.c */
#include <sysmem/sysroot.h>
#include <iofilemgr/iofilemgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/debug.h>
#include <iofilemgr/vfs.h>

extern SceVfsInfo g_exfatfsInfo;
extern int _register_exfatfs_vops(SceVfsInfo *info);
extern int sceExfatfsInit(void);
/* SceSysrootForDriver NID 0x7B7F8171; absent from the pinned SDK header. */

int sceExfatInitPhase2(void *unused)
{
	char memoryCard[20];
	char storageCard[20];
	int result;

	result = SceIofilemgrForDriver_D070BC48(0x300, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 194,
			"_mount_drive of exfatfs for VSH on eMMC failed[%#x]\n", result);
#endif
	result = SceIofilemgrForDriver_D070BC48(0xb00, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 205,
			"_mount_drive of exfatfs for system data on eMMC failed[%#x]\n", result);
#endif
	result = SceIofilemgrForDriver_D070BC48(0x400, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 216,
			"_mount_drive of exfatfs for VSH DATA on eMMC failed[%#x]\n", result);
#endif
	result = SceIofilemgrForDriver_D070BC48(0x500, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 227,
			"_mount_drive of exfatfs for VTRM on eMMC failed[%#x]\n", result);
#endif
	result = SceIofilemgrForDriver_D070BC48(0x700, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 238,
			"_mount_drive of exfatfs for UPDATER WORK on eMMC failed[%#x]\n", result);
#endif
	result = SceIofilemgrForDriver_D070BC48(0xc00, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 249,
			"_mount_drive of exfatfs for pre-install DATA on eMMC failed[%#x]\n", result);
#endif
	result = SceIofilemgrForDriver_D070BC48(0x600, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 262,
			"_mount_drive of exfatfs for USER on eMMC failed[%#x]\n", result);
#endif
	if (!SceSysrootForDriver_55392965()) {
		__builtin_memcpy(memoryCard, "mcd-lp-act-entire", 18);
		__builtin_memcpy(storageCard, "usd-lp-act-entire", 18);
		if (SceSysrootForDriver_50FE3B4D()) {
			result = SceIofilemgrForDriver_D070BC48(0x800, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL(2, 0, 279,
					"mc emu mount failed[%#x]\n", result);
#endif
		}
		sceIoDevctl("sdstor0:", 5, memoryCard, 18, NULL, 0);
		sceIoDevctl("sdstor0:", 5, storageCard, 18, NULL, 0);
		if (!SceSysrootForDriver_55392965()) {
			__builtin_memcpy(storageCard, "gcd-lp-act-entire", 18);
			sceIoDevctl("sdstor0:", 5, storageCard, 18, NULL, 0);
		}
	}
	return 1;
}

int sceExfatInitPhase1(SceSize arglen, const void *argp)
{
	int bootMode;
	int result;

	_register_exfatfs_vops(&g_exfatfsInfo);
	result = sceExfatfsInit();
	if (result < 0) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(1, 0, 128,
			"%s: sceExfatfsInit failed[%#x]\n", "sceExfatInitPhase1", result);
#endif
		return 2;
	}
	result = SceIofilemgrForDriver_D070BC48(0x200, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 139,
			"_mount_drive of exfatfs for OS on eMMC failed[%#x]\n", result);
#endif
	if (SceSysrootForDriver_55392965()) {
		bootMode = SceSysrootForDriver_7B7F8171();
		if (bootMode) {
			result = SceIofilemgrForDriver_D070BC48(0x100, NULL, 0, NULL, 0);
			if (result == (int)0x8001000d)
				result = SceIofilemgrForDriver_D070BC48(0x100, NULL, 1, NULL, 0);
#if FW_VERSION >= 0x04000000
			if (result < 0)
				SCE_DBG_PRINTF_LEVEL(2, 0, 152,
					"_mount_drive of exfatfs for SD failed[%#x]\n", result);
#endif
		}
		result = SceIofilemgrForDriver_D070BC48(0x800, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
		if (result < 0)
			SCE_DBG_PRINTF_LEVEL(2, 0, 158,
				"_mount_drive of exfatfs for User ext failed[%#x]\n", result);
#endif
		if (!bootMode)
			goto mountGameMedia;
	}
enqueue:
	sceKernelScheduleWorkTask(0x10023, "SceExfatInit", sceExfatInitPhase2, NULL);
	return 0;
mountGameMedia:
	result = SceIofilemgrForDriver_D070BC48(0x900, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 166,
			"_mount_drive of exfatfs for Game Media RO failed[%#x]\n", result);
#endif
	result = SceIofilemgrForDriver_D070BC48(0xa00, NULL, 0, NULL, 0);
#if FW_VERSION >= 0x04000000
	if (result < 0)
		SCE_DBG_PRINTF_LEVEL(2, 0, 174,
			"_mount_drive of exfatfs for Game Media RW failed[%#x]\n", result);
#endif
	goto enqueue;
}

