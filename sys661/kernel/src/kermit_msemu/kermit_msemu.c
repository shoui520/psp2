#include <kermit.h>

extern void *memcpy(void *dest, const void *src, SceSize n);

typedef struct SceIoIob {
	int i_flgs;
	int i_unit;
	void *i_de;
	int d_type;
	void *i_private;
	int i_cwd;
	SceOff i_fpos;
	SceUID i_thread;
	int reserved[3];
} SceIoIob;

typedef struct SceIoDeviceFunction {
	int (*df_init)(void *de);
	int (*df_exit)(void *de);
	int (*df_open)(SceIoIob *iob, char *file, int flags, int mode);
	int (*df_close)(SceIoIob *iob);
	SceSSize (*df_read)(SceIoIob *iob, void *data, SceSize len);
	SceSSize (*df_write)(SceIoIob *iob, const void *data, SceSize len);
	SceOff (*df_lseek)(SceIoIob *iob, SceOff ofs, int whence);
	int (*df_ioctl)(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
	int (*df_remove)(SceIoIob *iob, const char *name);
	int (*df_mkdir)(SceIoIob *iob, const char *name, int mode);
	int (*df_rmdir)(SceIoIob *iob, const char *name);
	int (*df_dopen)(SceIoIob *iob, const char *dirname);
	int (*df_dclose)(SceIoIob *iob);
	int (*df_dread)(SceIoIob *iob, void *dir);
	int (*df_getstat)(SceIoIob *iob, const char *file, void *stat);
	int (*df_chstat)(SceIoIob *iob, const char *file, void *stat, int bits);
	int (*df_rename)(SceIoIob *iob, const char *oldname, const char *newname);
	int (*df_chdir)(SceIoIob *iob, const char *dir);
	int (*df_mount)(SceIoIob *iob, const char *fs, const char *dev, int mode, void *mountArg, int len);
	int (*df_umount)(SceIoIob *iob, const char *dev);
	int (*df_devctl)(SceIoIob *iob, const char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
	int (*df_cancel)(SceIoIob *iob);
} SceIoDeviceFunction;

typedef struct SceIoDeviceEntry {
	const char *d_dp;
	int d_type;
	int d_user;
	const char *d_desc;
	const SceIoDeviceFunction *d_f;
} SceIoDeviceEntry;

typedef struct SceUidControlBlock {
	struct SceUidControlBlock *parent;
	struct SceUidControlBlock *nextChild;
	struct SceUidControlBlock *meta;
	SceUID uid;
	const char *name;
	u8 childSize;
	u8 size;
	u16 attr;
} SceUidControlBlock;

typedef struct MsMediaInfo {
	u16 status;
	u8 mediaType;
	u8 writeProtect;
	u8 reservedByte;
} MsMediaInfo;

typedef struct MsStatInfo {
	u32 suspendCount;
	u32 resumeCount;
	u32 ejectCount;
	u32 removeCount;
	u32 mask;
} MsStatInfo;

int sceIoAddDrv(const SceIoDeviceEntry *de);
int sceIoDelDrv(const char *drv);
int IoFileMgrForKernel_E5323C5B(const char *alias, const char *dev);
int sceKermitMsfs_driver_1A4DC9B7(u32 *info);

static int msstorIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
static int mscmhcIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
static int mscmhcDevctl(SceIoIob *iob, const char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
static int msemuSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int msstorInit(void *de);
static int msstorExit(void *de);
static int msstorOpen(SceIoIob *iob, char *file, int flags, int mode);
static int msstorClose(SceIoIob *iob);
static int mscmhcInit(void *de);
static int mscmhcExit(void *de);
static int mscmhcOpen(SceIoIob *iob, char *file, int flags, int mode);
static int mscmhcClose(SceIoIob *iob);
static int mscmhcMount(SceIoIob *iob, const char *fs, const char *dev, int mode, void *mountArg, int len);
static int mscmhcUmount(SceIoIob *iob, const char *dev);

static const SceIoDeviceFunction g_msstorFuncs = {
	.df_init  = msstorInit,
	.df_exit  = msstorExit,
	.df_open  = msstorOpen,
	.df_close = msstorClose,
	.df_ioctl = msstorIoctl,
};

static const SceIoDeviceFunction g_mscmhcFuncs = {
	.df_init   = mscmhcInit,
	.df_exit   = mscmhcExit,
	.df_open   = mscmhcOpen,
	.df_close  = mscmhcClose,
	.df_ioctl  = mscmhcIoctl,
	.df_mount  = mscmhcMount,
	.df_umount = mscmhcUmount,
	.df_devctl = mscmhcDevctl,
};

static const SceIoDeviceEntry g_msstorDrv = {
	.d_dp   = "msstor",
	.d_type = 1,
	.d_user = 0,
	.d_desc = "msstor emulation",
	.d_f    = &g_msstorFuncs,
};

static const SceIoDeviceEntry g_mscmhcDrv = {
	.d_dp   = "mscmhc",
	.d_type = 1,
	.d_user = 0,
	.d_desc = "mscm emulation",
	.d_f    = &g_mscmhcFuncs,
};

static SceSysEventHandler g_sysEventHandler = {
	.size = sizeof(SceSysEventHandler),
	.name = "SceMsEmuSysEvent",
	.type_mask = 0x00FFFF00,
	.handler = msemuSysEventHandler,
};

static const u32 g_msDescriptor[24] = {
	0x00100002, 0x2A0F0010, 0x00000800, 0x01D80700,
	0x01010101, 0x04030201, 0x00000001, 0xD7009800,
	0x00000000, 0x00002121, 0x401F0120, 0x00030002,
	0x01000000, 0x00010006, 0x00000000, 0x00000000,
	0x67452301, 0xEFCDAB89, 0x67452301, 0xEFCDAB89,
	0x00000000, 0x00000000, 0x00000000, 0x00000000,
};

static int g_msState = 4;
static int g_ejectCount = 1;
static int g_msemuReserved[2];
static int g_lockMode;
static int g_isSuspended;
static int g_suspendCount;
static int g_resumeCount;
static int g_removeCount;
static SceUID g_cbList[32];
static SceUID g_altCbList[32];

static int __attribute__((noinline)) msstorIoctlGetStatus(u32 *out, SceSize outlen);
static int __attribute__((noinline)) msstorIoctlGetCapacity(SceIoIob *iob, u32 *out, SceSize outlen);
static int __attribute__((noinline)) msstorIoctlGetSectorSize(u32 *out, SceSize outlen);
static int __attribute__((noinline)) msstorIoctlGetMediaType(u32 *out, SceSize outlen);
static int __attribute__((noinline)) msstorIoctlGetWriteProtect(u32 *out, SceSize outlen);
static int __attribute__((noinline)) msstorIoctlGetDescriptor(void *out, SceSize outlen);
static int __attribute__((noinline)) mscmhcIoctlGetMediaInfo(MsMediaInfo *out, SceSize outlen);
static int __attribute__((noinline)) mscmhcIoctlGetMaxTransfer(u32 *out, SceSize outlen);
static int __attribute__((noinline)) mscmhcDevctlGetState(u32 *out, SceSize outlen);
static int __attribute__((noinline)) mscmhcRegisterCb(SceUID *in, SceSize inlen, void *outdata);
static int __attribute__((noinline)) mscmhcUnregisterCb(SceUID *in, SceSize inlen, void *outdata);
static int __attribute__((noinline)) mscmhcDevctlGetInsertStatus(u32 *out, SceSize outlen);
static int __attribute__((noinline)) mscmhcDevctlGetLockMode(void *indata, u32 *out, SceSize outlen);
static int __attribute__((noinline)) mscmhcDevctlSetLockMode(u32 *in, SceSize inlen, void *outdata);
static int __attribute__((noinline)) mscmhcDevctlGetStats(u32 *out, SceSize outlen);
static int __attribute__((noinline)) mscmhcDevctlSetStats(const MsStatInfo *in, SceSize inlen);
static int __attribute__((noinline)) notifyCbList(int eventCode);
static int __attribute__((noinline)) notifyAltCbList(int eventCode);

int module_start(SceSize args, void *argp)
{
	int res = sceIoAddDrv(&g_mscmhcDrv);
	if (res < 0) {
		return res;
	}
	res = sceIoAddDrv(&g_msstorDrv);
	if (res < 0) {
		return res;
	}
	res = IoFileMgrForKernel_E5323C5B("msstor0p:", "msstor0:");
	if (res < 0) {
		return res;
	}
	res = IoFileMgrForKernel_E5323C5B("msstor0p1:", "msstor0:");
	if (res < 0) {
		return res;
	}
	for (int i = 0; i < 32; i++) {
		g_cbList[i] = 0;
	}
	for (int i = 0; i < 32; i++) {
		g_altCbList[i] = 0;
	}
	sceKernelRegisterSysEventHandler(&g_sysEventHandler);
	return 0;
}

static int msstorIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen)
{
	switch ((u32)cmd) {
	case 0x02105002:
	case 0x02105003:
	case 0x02105004:
	case 0x02105005:
		return 0x80220183;
	case 0x02125001:
		return msstorIoctlGetStatus(outdata, outlen);
	case 0x02125006:
		return msstorIoctlGetCapacity(iob, outdata, outlen);
	case 0x02125007:
		return msstorIoctlGetSectorSize(outdata, outlen);
	case 0x02125008:
		return msstorIoctlGetMediaType(outdata, outlen);
	case 0x02125009:
		return msstorIoctlGetWriteProtect(outdata, outlen);
	case 0x02125803:
		return msstorIoctlGetDescriptor(outdata, outlen);
	default:
		return 0x80010086;
	}
}

static int __attribute__((noinline)) msstorIoctlGetStatus(u32 *out, SceSize outlen)
{
	if (outlen < 4 || out == NULL || ((s32)(((u32)out | ((u32)out + 4)) & pspGetK1())) < 0) {
		return 0x80220183;
	}
	*out = 1;
	return 0;
}

static int __attribute__((noinline)) msstorIoctlGetCapacity(SceIoIob *iob, u32 *out, SceSize outlen)
{
	(void)iob;
	if (outlen < 4 || out == NULL || ((s32)(((u32)out | ((u32)out + 4)) & pspGetK1())) < 0) {
		return 0x80220183;
	}
	u32 info[8];
	int res = sceKermitMsfs_driver_1A4DC9B7(info);
	if (res < 0) {
		return res;
	}
	s64 cap = (s64)(u64)info[0] * (s32)info[4];
	if (cap > 0x7FFFFFFFLL) {
		*out = 0x7FFFFFFFu;
	} else {
		*out = (u32)cap;
	}
	return 0;
}

static int __attribute__((noinline)) msstorIoctlGetSectorSize(u32 *out, SceSize outlen)
{
	if (outlen < 4 || out == NULL || ((s32)(((u32)out | ((u32)out + 4)) & pspGetK1())) < 0) {
		return 0x80220183;
	}
	*out = 0x200;
	return 0;
}

static int __attribute__((noinline)) msstorIoctlGetMediaType(u32 *out, SceSize outlen)
{
	if (outlen < 4 || out == NULL || ((s32)(((u32)out | ((u32)out + 4)) & pspGetK1())) < 0) {
		return 0x80220183;
	}
	*out = 1;
	return 0;
}

static int __attribute__((noinline)) msstorIoctlGetWriteProtect(u32 *out, SceSize outlen)
{
	if (outlen < 4 || out == NULL || ((s32)(((u32)out | ((u32)out + 4)) & pspGetK1())) < 0) {
		return 0x80220183;
	}
	*out = 0;
	return 0;
}

static int mscmhcIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen)
{
	if (iob->i_unit != 0) {
		return 0x80010013;
	}
	if ((u32)cmd == 0x02125801) {
		return mscmhcIoctlGetMediaInfo(outdata, outlen);
	}
	if ((u32)cmd == 0x02125802) {
		return mscmhcIoctlGetMaxTransfer(outdata, outlen);
	}
	return 0x80010086;
}

static int __attribute__((noinline)) mscmhcIoctlGetMediaInfo(MsMediaInfo *out, SceSize outlen)
{
	if (out == NULL || outlen < 5 || ((s32)(((u32)out | ((u32)out + 5)) & pspGetK1())) < 0) {
		return 0x80220183;
	}
	out->writeProtect = 1;
	out->reservedByte = 0;
	out->status = 1;
	out->mediaType = 0;
	return 0;
}

static int __attribute__((noinline)) mscmhcIoctlGetMaxTransfer(u32 *out, SceSize outlen)
{
	if (out == NULL || outlen < 4 || ((s32)(((u32)out | ((u32)out + 4)) & pspGetK1())) < 0) {
		return 0x80220183;
	}
	*out = 0x40;
	return 0;
}

static int __attribute__((noinline)) msstorIoctlGetDescriptor(void *out, SceSize outlen)
{
	if (out == NULL || outlen < 0x60 || ((s32)(((u32)out | ((u32)out + 0x60)) & pspGetK1())) < 0) {
		return 0x80220183;
	}
	memcpy(out, g_msDescriptor, 0x60);
	return 0;
}

static int mscmhcDevctl(SceIoIob *iob, const char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen)
{
	(void)g_msemuReserved;
	if (iob->i_unit != 0) {
		return 0x80010013;
	}
	switch ((u32)cmd) {
	case 0x02015804:
		return mscmhcRegisterCb(indata, inlen, outdata);
	case 0x02015805:
		return mscmhcUnregisterCb(indata, inlen, outdata);
	case 0x02015807:
		return mscmhcDevctlGetLockMode(indata, outdata, outlen);
	case 0x0201580B:
		return mscmhcDevctlSetStats(indata, inlen);
	case 0x02025801:
		return mscmhcDevctlGetState(outdata, outlen);
	case 0x02025806:
		return mscmhcDevctlGetInsertStatus(outdata, outlen);
	case 0x02025808:
		return mscmhcDevctlSetLockMode(indata, inlen, outdata);
	case 0x0202580A:
		return mscmhcDevctlGetStats(outdata, outlen);
	default:
		return 0x80010016;
	}
}

static int __attribute__((noinline)) mscmhcDevctlGetState(u32 *out, SceSize outlen)
{
	u32 k1 = pspGetK1();
	if (out == NULL || outlen < 4 || ((s32)(k1 & (u32)out)) < 0) {
		return 0x80220081;
	}
	*out = g_msState;
	return 0;
}

static int __attribute__((noinline)) mscmhcRegisterCb(SceUID *in, SceSize inlen, void *outdata)
{
	u32 k1 = pspGetK1();
	if (in == NULL || inlen < 4 || outdata != NULL || ((s32)(k1 & (u32)in)) < 0) {
		return 0x80220081;
	}
	void *cbRaw = NULL;
	if (sceKernelGetUIDcontrolBlock(*in, &cbRaw) != 0) {
		return 0x80220081;
	}
	SceUidControlBlock *cb = (SceUidControlBlock *)cbRaw;
	u32 perm = 2;
	if ((s32)k1 < 0) {
		perm = cb->attr & 2u;
	}
	if (perm == 0) {
		return 0x80220081;
	}

	int intr = sceKernelSuspendDispatchThread();
	int found = 0;
	for (int i = 0; i < 32; i++) {
		if (g_cbList[i] == 0) {
			SceUID cbid = *in;
			g_cbList[i] = cbid;
			found = 1;
			if (g_msState == 4) {
				if (sceKernelNotifyCallback(cbid, 1) == (int)0x800201A1) {
					g_cbList[i] = 0;
				}
			}
			break;
		}
	}
	sceKernelResumeDispatchThread(intr);
	return found ? 0 : 0x80220082;
}

static int __attribute__((noinline)) mscmhcUnregisterCb(SceUID *in, SceSize inlen, void *outdata)
{
	u32 k1 = pspGetK1();
	if (in == NULL || inlen < 4 || outdata != NULL || ((s32)(k1 & (u32)in)) < 0) {
		return 0x80220081;
	}
	void *cbRaw = NULL;
	if (sceKernelGetUIDcontrolBlock(*in, &cbRaw) != 0) {
		return 0x80220081;
	}
	SceUidControlBlock *cb = (SceUidControlBlock *)cbRaw;
	u32 perm = 2;
	if ((s32)k1 < 0) {
		perm = cb->attr & 2u;
	}
	if (perm == 0) {
		return 0x80220081;
	}

	int intr = sceKernelSuspendDispatchThread();
	SceUID target = *in;
	int found = 0;
	for (int i = 0; i < 32; i++) {
		if (g_cbList[i] == target) {
			g_cbList[i] = 0;
			found = 1;
			break;
		}
	}
	sceKernelResumeDispatchThread(intr);
	return found ? 0 : 0x80220081;
}

static int __attribute__((noinline)) mscmhcDevctlGetInsertStatus(u32 *out, SceSize outlen)
{
	u32 k1 = pspGetK1();
	if (out == NULL || outlen < 4 || ((s32)(k1 & (u32)out)) < 0) {
		return 0x80220081;
	}
	*out = (g_msState == 4) ? 1 : 2;
	return 0;
}

static int __attribute__((noinline)) mscmhcDevctlGetLockMode(void *indata, u32 *out, SceSize outlen)
{
	u32 k1 = pspGetK1();
	if (out == NULL || outlen < 4 || indata != NULL || ((s32)(k1 & (u32)out)) < 0) {
		return 0x80220081;
	}
	sceKernelPowerLockForUser(0);
	*out = g_lockMode;
	sceKernelPowerUnlockForUser(0);
	return 0;
}

static int __attribute__((noinline)) mscmhcDevctlSetLockMode(u32 *in, SceSize inlen, void *outdata)
{
	u32 k1 = pspGetK1();
	if (in == NULL || inlen < 4 || outdata != NULL || ((s32)(k1 & (u32)in)) < 0) {
		return 0x80220081;
	}
	u32 mode = *in;
	if ((mode - 1u) >= 2u) {
		return 0x80220081;
	}
	int res = 0x80220086;
	sceKernelPowerLockForUser(0);
	if (g_isSuspended == 0) {
		g_lockMode = mode;
		res = 0;
	}
	sceKernelPowerUnlockForUser(0);
	return res;
}

static int __attribute__((noinline)) mscmhcDevctlGetStats(u32 *out, SceSize outlen)
{
	if (out == NULL || outlen < 0x10) {
		return 0x80220081;
	}
	out[0] = g_suspendCount;
	out[1] = g_resumeCount;
	out[2] = g_ejectCount;
	out[3] = g_removeCount;
	return 0;
}

static int __attribute__((noinline)) mscmhcDevctlSetStats(const MsStatInfo *in, SceSize inlen)
{
	if (in == NULL || inlen < 0x14) {
		return 0x80220081;
	}
	if ((in->mask & 0x80000001u) != 0) {
		g_suspendCount = in->suspendCount;
	}
	if ((in->mask & 0x80000002u) != 0) {
		g_resumeCount = in->resumeCount;
	}
	if ((in->mask & 0x80000004u) != 0) {
		g_ejectCount = in->ejectCount;
	}
	if ((in->mask & 0x80000008u) != 0) {
		g_removeCount = in->removeCount;
	}
	return 0;
}

static int msemuSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	if (g_lockMode == 1) {
		if (ev_id == 0x400D) {
			g_suspendCount++;
		} else if (ev_id == 0x400) {
			g_msState = 1;
			g_removeCount++;
			g_isSuspended = 1;
			notifyCbList(2);
			notifyAltCbList(2);
		} else if (ev_id == 0x1000D) {
			g_resumeCount++;
			g_msState = 4;
			g_ejectCount++;
			notifyCbList(1);
			notifyAltCbList(1);
		} else if (ev_id == 0x400000) {
			g_isSuspended = 0;
		}
	} else {
		if (ev_id == 0x400D) {
			g_suspendCount++;
		} else if (ev_id == 0x400) {
			g_isSuspended = 1;
		} else if (ev_id == 0x1000D) {
			g_resumeCount++;
			g_msState = 1;
			g_removeCount++;
			g_ejectCount++;
			notifyCbList(2);
			notifyAltCbList(2);
		} else if (ev_id == 0x100002) {
			if (g_msState != 4) {
				g_msState = 4;
				notifyCbList(1);
				notifyAltCbList(1);
			}
		} else if (ev_id == 0x400000) {
			g_isSuspended = 0;
		}
	}
	return 0;
}

static int __attribute__((noinline)) notifyCbList(int eventCode)
{
	int intr = sceKernelSuspendDispatchThread();
	for (int i = 0; i < 32; i++) {
		if (g_cbList[i] != 0) {
			if (sceKernelNotifyCallback(g_cbList[i], eventCode) == (int)0x800201A1) {
				g_cbList[i] = 0;
			}
		}
	}
	return sceKernelResumeDispatchThread(intr);
}

static int __attribute__((noinline)) notifyAltCbList(int eventCode)
{
	int intr = sceKernelSuspendDispatchThread();
	for (int i = 0; i < 32; i++) {
		if (g_altCbList[i] != 0) {
			if (sceKernelNotifyCallback(g_altCbList[i], eventCode) == (int)0x800201A1) {
				g_altCbList[i] = 0;
			}
		}
	}
	return sceKernelResumeDispatchThread(intr);
}

static int msstorInit(void *de)
{
	return 0;
}

static int msstorExit(void *de)
{
	return 0;
}

static int msstorOpen(SceIoIob *iob, char *file, int flags, int mode)
{
	if (iob == NULL) {
		return 0x80020328;
	}
	return (iob->i_unit == 0) ? 0 : 0x80220183;
}

static int msstorClose(SceIoIob *iob)
{
	return 0;
}

static int mscmhcInit(void *de)
{
	return 0;
}

static int mscmhcExit(void *de)
{
	return 0;
}

static int mscmhcOpen(SceIoIob *iob, char *file, int flags, int mode)
{
	return 0;
}

static int mscmhcClose(SceIoIob *iob)
{
	return 0;
}

static int mscmhcMount(SceIoIob *iob, const char *fs, const char *dev, int mode, void *mountArg, int len)
{
	return 0;
}

static int mscmhcUmount(SceIoIob *iob, const char *dev)
{
	return 0;
}
