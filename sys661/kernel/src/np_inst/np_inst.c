/* Copyright (C) 2011, 2012, 2013, 2014 The uOFW team
 See the file COPYING for copying permission.
 */

#include <common_imp.h>
#include <iofilemgr_kernel.h>
#include <threadman_kernel.h>
#include <sysmem_sysclib.h>

#define SCE_ERROR_PRIV_REQUIRED 0x80000023
#define SCE_NULL_POINTER 0x80550980
#define SCE_ERROR_READ_WRITE 0x80550981
#define SCE_ERROR_IO 0x80550982
#define SCE_ERROR_UNKNOWN_0 0x80550983
#define SCE_ERROR_UNKNOWN_1 0x80550984
#define SCE_ERROR_UNKNOWN_2 0x8055098

#define TIME_OUT 0x004C4B40

SCE_MODULE_INFO("sceNpInstall_Driver", SCE_MODULE_KIRK_SEMAPHORE_LIB | SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_START | SCE_MODULE_ATTR_EXCLUSIVE_LOAD, 1, 1);

/*
 * Structure for the act.dat file located in flash2
 * 
 * Act.dat FORMAT -> 
 * 	Version 	(0x0 - 0x4)
 * 	License Type(0x4 - 0x8)
 * 	Account ID	(0x8 - 0x10)
 * 	Data table	(0x10 - 0x1010)
 * 	Signature	(0x1010 - 0x1030)
 * 	Unknown 	(0x1030 - 0x1038)
 */
typedef struct ActDat {
	int version;
	int licenseType;
	int accountID[2];
} ActDat;

/*
 * Structure for a PSID
 */
typedef struct PspOpenPSID {
	unsigned char data[16];
} PspOpenPSID;

/*
 * .data / .bss section
 */
ActDat g_act = {
	.version = 0,
	.licenseType = 0,
	.accountID = {
		[0] = 0,
		[1] = 0,
	}
};

u64 g_challengeExpireTick;
int g_fd;

/*
 * Headers
 */

extern int sceKernelTerminateThread(SceUID thid);

extern int sceRtc_driver_CEEF238F(u64 *tick);
extern int sceRtc_driver_89FA4262(u64 *dst, const u64 *src, int sec, int dummy);

extern int sceOpenPSIDGetPSID(void *psid, int mode);

extern int sceNpDrmDecActivation(void *act, void *key);
extern int sceNpDrmVerifyAct(void *data);

extern int scePcactAuth1BB(int mode, void *psid, void *req, u64 *utcMs, void *out1, void *out2);
extern int scePcactAuth2BB(void *in, void *out, void *key);

s32 sceNpInstall_driver_5847D8C7(int mode, int challengeBuf, int outBufPrimary, int outBufSecondary)
		__attribute__((alias("sceNpInstall_user_5847D8C7")));

s32 sceNpInstall_driver_0B039B36(int actAddr, int actSize, int outBuf)
		__attribute__((alias("sceNpInstall_user_0B039B36")));

s32 sceNpInstall_driver_91F9D50D(int accountAddr)
		__attribute__((alias("sceNpInstall_user_91F9D50D")));

void sceNpInstall_driver_7AE4C8BC(void)
		__attribute__((alias("sceNpInstall_user_7AE4C8BC")));

s32 getNpChallenge(int mode, int challengeBuf, int outBufPrimary, int outBufSecondary);
s32 installActivationDat(int addr, int size, int outBuf);
s32 storeAccountID(int addr);
u32 removeAct(void);
s32 registerNpDeactivationThread(void);

/*
 *  Address 0x00000000
 */
u32 module_start(SceSize args __attribute__((unused)),
		void *argp __attribute__((unused))) {
	return 0;
}

/*
 * Address 0x00000008
 */
u32 module_stop(SceSize args __attribute__((unused)),
		void *argp __attribute__((unused))) {
	return 0;
}

/*
 * sceNpInstallGetChallenge
 * Address 0x00000010
 */
s32 sceNpInstall_user_5847D8C7(int mode, int challengeBuf, int outBufPrimary, int outBufSecondary) {
	s32 res = SCE_ERROR_PRIV_REQUIRED;
	s32 oldk1 = pspShiftK1();
	if ((((challengeBuf + 32) | challengeBuf) & (oldk1 << 11)) >= 0) {
		if ((((outBufPrimary + 128) | outBufPrimary) & (oldk1 << 11)) >= 0) {
			if ((((outBufSecondary + 64) | outBufSecondary) & (oldk1 << 11)) >= 0) {
				res = getNpChallenge(mode, challengeBuf, outBufPrimary, outBufSecondary);
			}
		}
	}
	pspSetK1(oldk1);
	return res;
}

/*
 * sceNpInstallActivation
 * Address 0x00000088
 */
s32 sceNpInstall_user_0B039B36(int actAddr, int actSize, int outBuf) {
	s32 res = SCE_ERROR_PRIV_REQUIRED;
	s32 oldk1 = pspShiftK1();
	if (((oldk1 << 11) & (((actAddr + actSize) | actAddr) | actSize)) >= 0) {
		if ((((outBuf + 64) | outBuf) & (oldk1 << 11)) >= 0) {
			res = installActivationDat(actAddr, actSize, outBuf);
		}
	}
	pspSetK1(oldk1);
	return res;
}

/*
 * sceNpInstallCheckActivation
 * Address 0x000000F4
 */
s32 sceNpInstall_user_91F9D50D(int accountAddr) {
	s32 res = SCE_ERROR_PRIV_REQUIRED;
	s32 oldk1 = pspShiftK1();
	if ((((accountAddr + 8) | accountAddr) & (oldk1 << 11)) >= 0) {
		res = storeAccountID(accountAddr);
	}
	pspSetK1(oldk1);
	return res;
}

/*
 * sceNpInstallDeactivation
 * Address 0x00000140
 */
void sceNpInstall_user_7AE4C8BC(void) {
	s32 oldk1 = pspShiftK1();
	registerNpDeactivationThread();
	pspSetK1(oldk1);
	return;
}

s32 getNpChallenge(int mode, int challengeBuf, int outBufPrimary, int outBufSecondary) {
	u8 psid[16];
	u8 req[48];
	u64 curTick;
	u64 utcMs;
	int fileDesc = -1;
	int authMode;
	s32 res;

	if (challengeBuf == 0 || outBufSecondary == 0 || outBufPrimary == 0)
		return SCE_NULL_POINTER;

	res = sceRtc_driver_CEEF238F(&curTick);
	if (res < 0)
		return res;

	if (mode == 1) {
		fileDesc = sceIoOpen("flash2:/act.dat", 0x04000001, 0);
		authMode = 1;
		if (fileDesc >= 0) {
			sceIoClose(fileDesc);
			return SCE_ERROR_UNKNOWN_0;
		}
	} else if (mode == 0) {
		authMode = 0;
		if (curTick < g_challengeExpireTick)
			return SCE_ERROR_UNKNOWN_1;
	} else if (mode == 2) {
		authMode = 2;
	} else {
		return SCE_NULL_POINTER;
	}

	memset(req, 0, sizeof(req));
	req[0] = 16;
	req[1] = 0;
	memcpy(&req[16], (const void *)challengeBuf, 32);
	utcMs = (curTick - 62135596800000000ULL) / 1000ULL;

	res = sceOpenPSIDGetPSID(psid, 1);
	if (res < 0)
		return res;

	res = scePcactAuth1BB(authMode, psid, req, &utcMs, (void *)outBufPrimary, (void *)outBufSecondary);
	if (mode == 1)
		res = sceRtc_driver_89FA4262(&g_challengeExpireTick, &curTick, 30, 0);

	return res;
}

s32 installActivationDat(int addr, int size, int outBuf) {
	SceUID fileDesc;
	s32 res;
	void *actData = (void *)(addr + 80);

	if (addr == 0 || size != 0x1090 || outBuf == 0)
		return SCE_NULL_POINTER;

	res = scePcactAuth2BB((void *)addr, (void *)outBuf, &g_act);
	if (res >= 0)
		res = sceNpDrmDecActivation(actData, &g_act);
	if (res >= 0)
		res = sceNpDrmVerifyAct(actData);
	if (res >= 0) {
		fileDesc = sceIoOpen("flash2:/act.dat", 0x04000602, 0x1B6);
		res = fileDesc;
		if (fileDesc >= 0) {
			res = sceIoWrite(fileDesc, actData, 0x1038);
			if (res != 0x1038 && res >= 0)
				res = SCE_ERROR_READ_WRITE;
			sceIoClose(fileDesc);
		}
	}
	if (res < 0)
		sceIoRemove("flash2:/act.dat");

	memset(actData, 0, 0x1040);
	memset((void *)outBuf, 0, 64);
	memset(&g_act, 0, 16);
	return res;
}

s32 storeAccountID(int addr) {
	ActDat act;
	s32 res = SCE_NULL_POINTER;
	SceUID fileDesc;
	int *out = (int *)addr;

	if (addr == 0)
		return res;

	fileDesc = sceIoOpen("flash2:/act.dat", 0x04000001, 0);
	res = SCE_ERROR_IO;
	if (fileDesc >= 0) {
		res = sceIoRead(fileDesc, &act, 16);
		if (res == 16) {
			out[0] = act.accountID[0];
			out[1] = act.accountID[1];
		} else if (res >= 0) {
			res = SCE_ERROR_READ_WRITE;
		}
		sceIoClose(fileDesc);
	}
	return res;
}

u32 removeAct(void) {
	g_fd = sceIoRemove("flash2:/act.dat");
	return 0;
}

s32 registerNpDeactivationThread(void) {
	SceUInt timeout = TIME_OUT;
	s32 res;
	SceUID thid;

	thid = sceKernelCreateThread("SceNpDeactivation",
			(SceKernelThreadEntry)removeAct, 0x20, 0x4000, 0, NULL);
	if (thid < 0)
		return thid;

	g_fd = 0;
	res = sceKernelStartThread(thid, 0, NULL);
	if (res >= 0) {
		res = sceKernelWaitThreadEnd(thid, &timeout);
		if (res >= 0)
			return g_fd;
	}
	sceKernelTerminateThread(thid);
	sceKernelDeleteThread(thid);
	return res;
}
