#include <kermit.h>

typedef struct SceIoDeviceEntry SceIoDeviceEntry;

typedef struct SceIoIob {
	int i_flgs;
	int i_unit;
	SceIoDeviceEntry *i_de;
	int d_type;
	void *i_private;
	int i_cwd;
	SceOff i_fpos;
} SceIoIob;

typedef struct SceIoDeviceFunction {
	int (*dt_init)(SceIoDeviceEntry *de);
	int (*dt_exit)(SceIoDeviceEntry *de);
	int (*dt_open)(SceIoIob *iob, char *file, int flags, int mode);
	int (*dt_close)(SceIoIob *iob);
	int (*dt_read)(SceIoIob *iob, char *data, SceSize len);
	int (*dt_write)(SceIoIob *iob, const char *data, SceSize len);
	SceOff (*dt_lseek)(SceIoIob *iob, SceOff ofs, int whence);
	int (*dt_ioctl)(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
	int (*dt_remove)(SceIoIob *iob, const char *name);
	int (*dt_mkdir)(SceIoIob *iob, const char *name, int mode);
	int (*dt_rmdir)(SceIoIob *iob, const char *name);
	int (*dt_dopen)(SceIoIob *iob, const char *dirname);
	int (*dt_dclose)(SceIoIob *iob);
	int (*dt_dread)(SceIoIob *iob, void *dir);
	int (*dt_getstat)(SceIoIob *iob, const char *name, void *stat);
	int (*dt_chstat)(SceIoIob *iob, const char *name, void *stat, int bits);
	int (*dt_rename)(SceIoIob *iob, const char *oldname, const char *newname);
	int (*dt_chdir)(SceIoIob *iob, const char *name);
	int (*dt_mount)(SceIoIob *iob);
	int (*dt_umount)(SceIoIob *iob);
	int (*dt_devctl)(SceIoIob *iob, const char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
	int (*dt_cancel)(SceIoIob *iob);
} SceIoDeviceFunction;

typedef struct SceIoDeviceTable {
	const char *dt_string;
	int dt_type;
	int dt_size;
	const char *dt_desc;
	const SceIoDeviceFunction *dt_func;
} SceIoDeviceTable;

typedef struct SceMsfsFileSlot {
	SceIoIob *iob;
	SceUID mutexId;
	int kermitFd;
	int flags;
	SceOff offset;
} SceMsfsFileSlot;

typedef struct SceMsfsCloseRequest {
	u32 tag;
	SceIoIob *iob;
	u32 cmd;
	int prio;
} SceMsfsCloseRequest;

typedef struct SceMsfsCloseResult {
	u32 tag;
	int result;
} SceMsfsCloseResult;

u32 sceKernelGetSyscallRA(void);
int sceIoTerminateFd(const char *drive);
int sceIoAddDrv(const SceIoDeviceTable *drv);
int sceIoDelDrv(const char *drv_name);
int sceKernelGetCompiledSdkVersion(void);

void *memset(void *dst, int c, SceSize len);
u32 strlen(const char *s);
char *strncpy(char *dst, const char *src, u32 n);

int sceKernelWaitThreadEnd(SceUID thid, SceUInt *timeout);
SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize args, void *argp), int initPriority, int stackSize, SceUInt attr, void *option);
int sceKernelChangeThreadPriority(SceUID thid, int priority);
int sceKernelGetThreadCurrentPriority(void);
int sceKernelDeleteThread(SceUID thid);
int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);

static void _msInitCallbacks(void);
static int _msSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int _msCloseDirect(SceIoIob *iob, u32 cmd);
static int _msIoInit(SceIoDeviceEntry *de);
static int _msIoExit(SceIoDeviceEntry *de);
static int _msIoOpen(SceIoIob *iob, char *file, int flags, int mode);
static int _msIoClose(SceIoIob *iob);
static int _msIoRead(SceIoIob *iob, char *data, SceSize len);
static int _msIoWrite(SceIoIob *iob, const char *data, SceSize len);
static SceOff _msIoLseek(SceIoIob *iob, SceOff ofs, int whence);
static int _msIoIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
static int _msIoRemove(SceIoIob *iob, const char *name);
static int _msIoMkdir(SceIoIob *iob, const char *name, int mode);
static int _msIoRmdir(SceIoIob *iob, const char *name);
static int _msIoDopen(SceIoIob *iob, const char *dirname);
static int _msIoDclose(SceIoIob *iob);
static int _msIoDread(SceIoIob *iob, void *dir);
static int _msIoGetstat(SceIoIob *iob, const char *name, void *stat);
static int _msIoChstat(SceIoIob *iob, const char *name, void *stat, int bits);
static int _msIoRename(SceIoIob *iob, const char *oldname, const char *newname);
static int _msIoChdir(SceIoIob *iob, const char *name);
static int _msIoMount(SceIoIob *iob);
static int _msIoUmount(SceIoIob *iob);
static int _msIoDevctl(SceIoIob *iob, const char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);

static void _msNotifyCallbacks(int arg);
static int _msRegisterCallbackDevctl(SceIoIob *iob, const SceUID *indata, SceSize inlen);
static int _msUnregisterCallbackDevctl(SceIoIob *iob, const SceUID *indata, SceSize inlen);
static int _msOpenInternal(SceIoIob *iob, const char *file, int flags, int mode, u32 cmd);
static int _msKermitDevctl(const char *dev, u32 cmd, const void *indata, u32 inlen, void *outdata, u32 outlen);
static int _msGetFreeSpaceDevctl(SceIoIob *iob, void *indata, SceSize inlen);
static int _msRemoveCallback(SceUID cbId);
static int _msAddCallback(SceUID cbId);
static int _msQueryFreeSpace(u32 *info);
static int _msInitCloseWorker(void);
static int _msTermCloseWorker(void);
static int _msSendCloseReq(SceIoIob *iob, u32 cmd);
static int _msCloseWorkerThread(SceSize args, void *argp);
static int _msStartCloseWorker(void);

static const SceIoDeviceFunction g_msDrvFuncs = {
	.dt_init = _msIoInit,
	.dt_exit = _msIoExit,
	.dt_open = _msIoOpen,
	.dt_close = _msIoClose,
	.dt_read = _msIoRead,
	.dt_write = _msIoWrite,
	.dt_lseek = _msIoLseek,
	.dt_ioctl = _msIoIoctl,
	.dt_remove = _msIoRemove,
	.dt_mkdir = _msIoMkdir,
	.dt_rmdir = _msIoRmdir,
	.dt_dopen = _msIoDopen,
	.dt_dclose = _msIoDclose,
	.dt_dread = _msIoDread,
	.dt_getstat = _msIoGetstat,
	.dt_chstat = _msIoChstat,
	.dt_rename = _msIoRename,
	.dt_chdir = _msIoChdir,
	.dt_mount = _msIoMount,
	.dt_umount = _msIoUmount,
	.dt_devctl = _msIoDevctl,
	.dt_cancel = NULL,
};

static const SceIoDeviceFunction g_fatmsDrvFuncs = {
	.dt_init = _msIoInit,
	.dt_exit = _msIoExit,
	.dt_open = _msIoOpen,
	.dt_close = _msIoClose,
	.dt_read = _msIoRead,
	.dt_write = _msIoWrite,
	.dt_lseek = _msIoLseek,
	.dt_ioctl = _msIoIoctl,
	.dt_remove = _msIoRemove,
	.dt_mkdir = _msIoMkdir,
	.dt_rmdir = _msIoRmdir,
	.dt_dopen = _msIoDopen,
	.dt_dclose = _msIoDclose,
	.dt_dread = _msIoDread,
	.dt_getstat = _msIoGetstat,
	.dt_chstat = _msIoChstat,
	.dt_rename = _msIoRename,
	.dt_chdir = _msIoChdir,
	.dt_mount = _msIoMount,
	.dt_umount = _msIoUmount,
	.dt_devctl = _msIoDevctl,
	.dt_cancel = NULL,
};

static const SceIoDeviceTable g_msDrv = {
	.dt_string = "ms",
	.dt_type = 0x000A0010,
	.dt_size = 1,
	.dt_desc = "Memory Stick file",
	.dt_func = &g_msDrvFuncs,
};

static const SceIoDeviceTable g_fatmsDrv = {
	.dt_string = "fatms",
	.dt_type = 0x000A0010,
	.dt_size = 1,
	.dt_desc = "fatms emulation",
	.dt_func = &g_fatmsDrvFuncs,
};

static SceSysEventHandler g_msSysEvent = {
	.size = sizeof(SceSysEventHandler),
	.name = "sceMsfs",
	.type_mask = 0x00FFFF00,
	.handler = _msSysEventHandler,
};

static int g_msInserted = 1;
static SceUID g_closeThreadId = -1;
static SceUID g_closeMutexId = -1;
static SceUID g_closeReqPipeId = -1;
static SceUID g_closeResPipeId = -1;

int sceKermitMsfs_driver_var_93B61A29;
int sceKermitMsfs_driver_var_C1B4213A;
int sceKermitMsfs_driver_var_7668D90C;
int sceKermitMsfs_driver_var_BE954163;

static u32 g_msFlags;
static int g_clampFreeSpace;
static SceUID g_mutexId;
static void *g_bufBase;
static void *g_workBuf;
static u32 g_workSize;
static SceMsfsFileSlot g_fileSlots[32];
static SceUID g_callbacks[32];

int module_start(SceSize args, void *argp)
{
	sceKernelMemset((void *)0xABD18000u, 0, 0x8000);
	g_workBuf = (void *)0xABD1C000u;
	g_bufBase = (void *)0xABD18000u;
	g_workSize = 0x4000;

	SceUID mtx = sceKernelCreateMutex("SceKermitMsfsFile", SCE_KERNEL_SA_THPRI, 0, NULL);
	g_mutexId = mtx;
	if (mtx <= 0) {
		return 1;
	}

	if (_msInitCloseWorker() < 0) {
		sceKernelDeleteMutex(g_mutexId);
		return 1;
	}

	sceIoDelDrv("ms");
	if (sceIoAddDrv(&g_msDrv) < 0) {
		_msTermCloseWorker();
		sceKernelDeleteMutex(g_mutexId);
		return 1;
	}

	sceIoDelDrv("fatms");
	if (sceIoAddDrv(&g_fatmsDrv) < 0) {
		_msTermCloseWorker();
		sceKernelDeleteMutex(g_mutexId);
		return 1;
	}

	if (_msStartCloseWorker() < 0) {
		_msTermCloseWorker();
		sceKernelDeleteMutex(g_mutexId);
		return 1;
	}

	_msInitCallbacks();
	sceKernelRegisterSysEventHandler(&g_msSysEvent);
	return 0;
}

static void _msInitCallbacks(void)
{
	int intr = sceKernelCpuSuspendIntr();
	for (int i = 0; i < 32; i++) {
		g_callbacks[i] = 0;
	}
	sceKernelCpuResumeIntr(intr);
}

static int _msSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	int res = 0;
	if (ev_id == 0x402) {
		return 0;
	}
	if (ev_id < 0x403) {
		if (ev_id == 0x102) {
			int intr = sceKernelCpuSuspendIntr();
			g_msFlags |= 0x80u;
			sceKernelCpuResumeIntr(intr);
			return 0;
		}
		if (ev_id == 0x400) {
			int intr = sceKernelCpuSuspendIntr();
			g_msFlags &= ~4u;
			sceKernelCpuResumeIntr(intr);
			if ((g_msFlags & 0x400u) == 0) {
				res = sceIoTerminateFd("fatms");
			}
			if (sceKermitMsfs_driver_var_7668D90C != 0) {
				g_msInserted = 0;
			}
			_msNotifyCallbacks(2);
			return res;
		}
		return 0;
	}
	if (ev_id == 0x10000) {
		int intr = sceKernelCpuSuspendIntr();
		g_msFlags &= ~0x80u;
		sceKernelCpuResumeIntr(intr);
		return 0;
	}
	if (ev_id == 0x100000) {
		int intr = sceKernelCpuSuspendIntr();
		g_msFlags |= 4u;
		sceKernelCpuResumeIntr(intr);
		g_msInserted = 1;
		_msNotifyCallbacks(1);
		return 0;
	}
	return 0;
}

static int _msCloseDirect(SceIoIob *iob, u32 cmd)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit > 0) {
		return 0x80010013;
	}

	SceMsfsFileSlot *slot = (SceMsfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return 0x80010009;
	}

	req->args[0] = (u32)slot->kermitFd;
	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, cmd, 1, 0, &resp);
	if (kres != 0) {
		return kres;
	}

	sceKernelDeleteMutex(slot->mutexId);
	int intr = sceKernelCpuSuspendIntr();
	slot->iob->i_private = NULL;
	slot->iob = NULL;
	sceKernelCpuResumeIntr(intr);
	return (int)resp;
}

static int _msIoRead(SceIoIob *iob, char *data, SceSize len)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	void *work = g_workBuf;

	if (iob->i_unit > 0) {
		return 0x80010013;
	}
	if ((s32)len < 0) {
		return 0x80010016;
	}

	SceMsfsFileSlot *slot = (SceMsfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return 0x80010009;
	}

	s32 totalRead = 0;
	u32 unaligned = (u32)data & 0x3Fu;
	if (unaligned != 0) {
		int res = sceKernelLockMutex(g_mutexId, 1, NULL);
		if (res != 0) {
			return res;
		}

		u32 chunk = 0x40u - unaligned;
		if (len < chunk) {
			chunk = len;
		}

		req->args[0] = (u32)slot->kermitFd;
		sceKermitMemory_driver_AAF047AC(req, 1, work, chunk, 2);
		req->args[2] = chunk;

		if (sceKermitMsfs_driver_var_BE954163 != 0) {
			sceKernelDelayThread((SceUInt)sceKermitMsfs_driver_var_BE954163);
		}

		u64 resp = 0;
		int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC5E, 3, 0, &resp);
		if (kres != 0) {
			res = sceKernelUnlockMutex(g_mutexId, 1);
			return (res == 0) ? kres : res;
		}

		s32 r = (s32)resp;
		if (r <= 0) {
			totalRead = (r < 0) ? r : 0;
			res = sceKernelUnlockMutex(g_mutexId, 1);
			if (res != 0) {
				return res;
			}
			goto done;
		}

		sceKernelMemcpy(data, work, (u32)r);
		totalRead = r;
		len -= (u32)r;
		data += r;
		if ((u32)r != chunk) {
			res = sceKernelUnlockMutex(g_mutexId, 1);
			if (res != 0) {
				return res;
			}
			goto done;
		}

		res = sceKernelUnlockMutex(g_mutexId, 1);
		if (res != 0) {
			return res;
		}
	}

	if (len != 0) {
		u32 alignedLen = len & ~0x3Fu;
		if (alignedLen != 0) {
			req->args[0] = (u32)slot->kermitFd;
			sceKermitMemory_driver_AAF047AC(req, 1, data, alignedLen, 2);
			req->args[2] = alignedLen;
			sceKernelDcacheInvalidateRange(data, alignedLen);

			if (sceKermitMsfs_driver_var_BE954163 != 0) {
				sceKernelDelayThread((SceUInt)sceKermitMsfs_driver_var_BE954163);
			}

			u64 resp = 0;
			int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC5E, 3, 0, &resp);
			if (kres != 0) {
				return kres;
			}

			s32 r = (s32)resp;
			if (r <= 0) {
				if (r < 0) {
					totalRead = r;
				}
				goto done;
			}

			sceKermitMemory_driver_90B662D0(data, (u32)r);
			totalRead += r;
			len -= (u32)r;
			data += r;
			if ((u32)r != alignedLen) {
				goto done;
			}
		}

		if (len != 0) {
			int res = sceKernelLockMutex(g_mutexId, 1, NULL);
			if (res != 0) {
				return res;
			}

			req->args[0] = (u32)slot->kermitFd;
			sceKermitMemory_driver_AAF047AC(req, 1, work, len, 2);
			req->args[2] = len;

			if (sceKermitMsfs_driver_var_BE954163 != 0) {
				sceKernelDelayThread((SceUInt)sceKermitMsfs_driver_var_BE954163);
			}

			u64 resp = 0;
			int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC5E, 3, 0, &resp);
			if (kres != 0) {
				res = sceKernelUnlockMutex(g_mutexId, 1);
				return (res == 0) ? kres : res;
			}

			s32 r = (s32)resp;
			if (r > 0) {
				sceKernelMemcpy(data, work, (u32)r);
				totalRead += r;
			} else if (r < 0) {
				totalRead = r;
			}

			res = sceKernelUnlockMutex(g_mutexId, 1);
			if (res != 0) {
				return res;
			}
		}
	}

done:
	slot->offset += totalRead;
	iob->i_fpos = slot->offset;
	return totalRead;
}

static int _msIoWrite(SceIoIob *iob, const char *data, SceSize len)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit > 0) {
		return 0x80010013;
	}
	if (sceKermitMsfs_driver_var_93B61A29 != 0 && len == 0) {
		return 0x80010016;
	}
	if ((s32)len < 0) {
		return 0x80010016;
	}

	SceMsfsFileSlot *slot = (SceMsfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return 0x80010009;
	}

	u32 low6 = (u32)data & 0x3Fu;
	sceKernelDcacheWritebackRange((const void *)((u32)data - low6), (low6 + len + 0x3Fu) & ~0x3Fu);

	req->args[0] = (u32)slot->kermitFd;
	sceKermitMemory_driver_AAF047AC(req, 1, (void *)data, len, 1);
	req->args[2] = len;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC6A, 3, 0, &resp);
	if (kres != 0) {
		return kres;
	}

	s32 r = (s32)resp;
	if (r > 0) {
		slot->offset += r;
		iob->i_fpos = slot->offset;
	}
	return r;
}

static SceOff _msIoLseek(SceIoIob *iob, SceOff ofs, int whence)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit > 0) {
		return (SceOff)0x80010013u;
	}
	if ((u32)whence >= 3u) {
		return (SceOff)0x80010016u;
	}

	SceMsfsFileSlot *slot = (SceMsfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return (SceOff)0x80010009u;
	}

	req->args[0] = (u32)slot->kermitFd;
	req->args[1] = (u64)ofs;
	req->args[2] = (u32)whence;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC62, 3, 0, &resp);
	if (kres != 0) {
		return (SceOff)kres;
	}

	SceOff newPos = (SceOff)resp;
	if (newPos >= 0) {
		slot->offset = newPos;
		iob->i_fpos = newPos;
	}
	return newPos;
}

static int _msIoIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (cmd <= 0x208014) {
		if (cmd >= 0x208013 || cmd == 0x208002) {
			return 0;
		}
		return 0x80010016;
	}
	if (cmd == 0x208082) {
		return 0;
	}
	if (cmd != 0x02415050) {
		return 0x80010016;
	}

	u32 half = g_workSize >> 1;
	char *work1 = (char *)g_workBuf;
	char *work2 = work1 + half;

	SceMsfsFileSlot *slot = (SceMsfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return 0x80010009;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	strncpy(work2, *(const char **)indata, half);
	work2[half - 1] = '\0';
	*(char **)work1 = work2;

	req->args[0] = (u32)slot->kermitFd;
	req->args[1] = (u32)cmd;
	sceKermitMemory_driver_AAF047AC(req, 2, work1, inlen, 1);
	req->args[3] = inlen;
	sceKermitMemory_driver_AAF047AC(req, 4, NULL, 0, 2);
	req->args[5] = 0;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC6F, 6, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoRemove(SceIoIob *iob, const char *name)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u32 half = g_workSize >> 1;
	char *work = (char *)g_workBuf;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	strncpy(work, name, half);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC6C, 1, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoMkdir(SceIoIob *iob, const char *name, int mode)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u32 half = g_workSize >> 1;
	char *work = (char *)g_workBuf;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	strncpy(work, name, half);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);
	req->args[1] = (u32)mode;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC66, 2, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoRmdir(SceIoIob *iob, const char *name)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u32 half = g_workSize >> 1;
	char *work = (char *)g_workBuf;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	strncpy(work, name, half);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC61, 1, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoDread(SceIoIob *iob, void *dir)
{
	u8 stack_buf[0x80];
	int useOldShortName = 0;
	if (sceKermitMsfs_driver_var_C1B4213A != 0) {
		useOldShortName = (sceKernelGetCompiledSdkVersion() <= 0x0307FFFF) ? 1 : 0;
	}

	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);
	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit > 0) {
		return 0x80010013;
	}

	SceMsfsFileSlot *slot = (SceMsfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return 0x80010009;
	}

	int res = sceKernelLockMutex(slot->mutexId, 1, NULL);
	if (res < 0) {
		return res;
	}

	res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		sceKernelUnlockMutex(slot->mutexId, 1);
		return res;
	}

	req->args[0] = (u32)slot->kermitFd;
	u8 *work1 = (u8 *)g_workBuf;
	u8 *work2 = work1 + (g_workSize >> 1);
	void *dPriv = *(void **)((u8 *)dir + 0x158);

	if (useOldShortName == 0) {
		if (((u32)dPriv & 3u) != 0) {
			dPriv = NULL;
		}
		if (dPriv != NULL) {
			if (((s32)(pspGetK1() & (u32)dPriv)) < 0) {
				sceKernelUnlockMutex(slot->mutexId, 1);
				return 0x80010016;
			}
			if (*(u32 *)dPriv != 0x414u) {
				dPriv = NULL;
			}
		}
	} else {
		if (dPriv != NULL) {
			if (((s32)(pspGetK1() & (u32)dPriv)) < 0) {
				sceKernelUnlockMutex(slot->mutexId, 1);
				return 0x80010016;
			}
		}
	}

	sceKernelMemcpy(work1, dir, 0x160);
	if (dPriv != NULL) {
		sceKernelMemcpy(work2, dPriv, 0x414);
		if (useOldShortName != 0) {
			*(u32 *)work2 = 0x414u;
		}
		*(void **)(work1 + 0x158) = work2;
	}

	sceKermitMemory_driver_AAF047AC(req, 1, work1, 0x160, 3);
	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC65, 2, 0, &resp);
	if (kres != 0) {
		res = sceKernelUnlockMutex(g_mutexId, 1);
		if (res != 0) {
			kres = res;
		}
		sceKernelUnlockMutex(slot->mutexId, 1);
		return kres;
	}

	s32 cmdRes = (s32)resp;
	if (cmdRes > 0) {
		iob->i_fpos++;
		*(void **)(work1 + 0x158) = *(void **)((u8 *)dir + 0x158);
		sceKernelMemcpy(dir, work1, 0x160);
		if (dPriv != NULL) {
			if (useOldShortName == 0) {
				if (*(u32 *)dPriv == 0x414u) {
					sceKernelMemcpy(dPriv, work2, 0x414);
				}
			} else {
				memset(dPriv, 0, 0x10D);
				strncpy((char *)dPriv, (const char *)work2 + 4, 0xD);
				strncpy((char *)dPriv + 0xD, (const char *)work2 + 0x14, 0x100);
			}
		}
		cmdRes = 1;
	}

	res = sceKernelUnlockMutex(g_mutexId, 1);
	sceKernelUnlockMutex(slot->mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoGetstat(SceIoIob *iob, const char *name, void *stat)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit > 0) {
		return 0x80010013;
	}

	u32 half = g_workSize >> 1;
	char *work = (char *)g_workBuf;
	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	strncpy(work, name, half);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);

	void *statWork = (u8 *)g_workBuf + (g_workSize >> 1);
	sceKermitMemory_driver_AAF047AC(req, 1, statWork, 0x58, 2);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC69, 2, 0, &resp);
	int cmdRes = kres;
	if (kres == 0) {
		cmdRes = (int)resp;
		if (cmdRes < 0) {
			sceKernelMemset(stat, 0, 0x58);
		} else {
			sceKernelMemcpy(stat, statWork, 0x58);
			cmdRes = 0;
		}
	}

	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoChstat(SceIoIob *iob, const char *name, void *stat, int bits)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u32 half = g_workSize >> 1;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	char *nameWork = NULL;
	if (name != NULL) {
		nameWork = (char *)g_workBuf;
		strncpy(nameWork, name, half);
		nameWork[half - 1] = '\0';
	}

	void *statWork = NULL;
	if (stat != NULL) {
		statWork = (u8 *)g_workBuf + (g_workSize >> 1);
		sceKernelMemcpy(statWork, stat, 0x58);
	}

	req->cmd = 0xC5D;
	sceKermitMemory_driver_AAF047AC(req, 0, nameWork, strlen(nameWork) + 1u, 1);
	sceKermitMemory_driver_AAF047AC(req, 1, statWork, 0x58, 1);
	req->args[2] = (u32)bits;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC5D, 3, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoRename(SceIoIob *iob, const char *oldname, const char *newname)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u32 half = g_workSize >> 1;
	char *work1 = (char *)g_workBuf;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	strncpy(work1, oldname, half);
	work1[half - 1] = '\0';

	char *work2 = (char *)g_workBuf + (g_workSize >> 1);
	strncpy(work2, newname, half);
	work2[half - 1] = '\0';

	sceKermitMemory_driver_AAF047AC(req, 0, work1, strlen(work1) + 1u, 1);
	sceKermitMemory_driver_AAF047AC(req, 1, work2, strlen(work2) + 1u, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC68, 2, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoChdir(SceIoIob *iob, const char *name)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u32 half = g_workSize >> 1;
	char *work = (char *)g_workBuf;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	strncpy(work, name, half);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC6D, 1, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _msIoDevctl(SceIoIob *iob, const char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen)
{
	if (cmd == 0x02425818) {
		return _msGetFreeSpaceDevctl(iob, indata, inlen);
	}
	if (cmd > 0x02425818) {
		if (cmd == 0x02425825) {
			if (outdata == NULL) {
				return 0x80010016;
			}
			*(int *)outdata = 1;
			return 0;
		}
		if (cmd > 0x02425825) {
			if (cmd == 0x02425858) {
				if (indata == NULL || inlen != 4) {
					return 0x80010016;
				}
				int val = *(int *)indata;
				if (val != 0 && val != 1) {
					return 0x80010016;
				}
				g_clampFreeSpace = val;
				return 0;
			}
			if (cmd == 0x0242585A) {
				int intr = sceKernelCpuSuspendIntr();
				g_msFlags |= 0x400u;
				sceKernelCpuResumeIntr(intr);
				return 0;
			}
			return 0x80010016;
		}
		if (cmd == 0x02425823) {
			if (outdata == NULL) {
				return 0x80010016;
			}
			*(int *)outdata = g_msInserted;
			return 0;
		}
		if (cmd == 0x02425824) {
			if (outdata == NULL) {
				return 0x80010016;
			}
			*(int *)outdata = 0;
			return 0;
		}
		return 0x80010016;
	}
	if (cmd == 0x02415821) {
		return _msRegisterCallbackDevctl(iob, (const SceUID *)indata, inlen);
	}
	if (cmd > 0x02415821) {
		if (cmd == 0x02415822) {
			return _msUnregisterCallbackDevctl(iob, (const SceUID *)indata, inlen);
		}
		if (cmd == 0x02415859) {
			if (outdata == NULL || outlen != 4) {
				return 0x80010016;
			}
			*(int *)outdata = g_clampFreeSpace;
			return 0;
		}
		return 0x80010016;
	}
	if (cmd == 0x100) {
		return 0;
	}
	if (cmd == 0x0240D81E) {
		int intr = sceKernelCpuSuspendIntr();
		g_msFlags |= 4u;
		sceKernelCpuResumeIntr(intr);
		return 0;
	}
	return 0x80010016;
}

int sceKermitMsfs_driver_1A4DC9B7(void *outbuf)
{
	return _msKermitDevctl("ms0:", 0x02425818, NULL, 0, outbuf, 0x14);
}

static int _msIoInit(SceIoDeviceEntry *de)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	sceKermitMemory_driver_AAF047AC(req, 0, de, 0xC, 3);
	u64 resp = 0;
	int res = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC63, 1, 0, &resp);
	if (res != 0) {
		return res;
	}
	sceKermitMemory_driver_90B662D0(de, 0xC);
	return (int)resp;
}

static int _msIoExit(SceIoDeviceEntry *de)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	sceKermitMemory_driver_AAF047AC(req, 0, de, 0xC, 3);
	u64 resp = 0;
	int res = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC6E, 1, 0, &resp);
	if (res != 0) {
		return res;
	}
	sceKermitMemory_driver_90B662D0(de, 0xC);
	return 0;
}

static int _msIoOpen(SceIoIob *iob, char *file, int flags, int mode)
{
	return _msOpenInternal(iob, file, flags, mode, 0xC67);
}

static int _msIoClose(SceIoIob *iob)
{
	return _msSendCloseReq(iob, 0xC6B);
}

static int _msIoDopen(SceIoIob *iob, const char *dirname)
{
	return _msOpenInternal(iob, dirname, 0, 0, 0xC5F);
}

static int _msIoDclose(SceIoIob *iob)
{
	return _msSendCloseReq(iob, 0xC64);
}

static int _msIoMount(SceIoIob *iob)
{
	return 0;
}

static int _msIoUmount(SceIoIob *iob)
{
	return 0;
}

static void _msNotifyCallbacks(int arg)
{
	int state = sceKernelSuspendDispatchThread();
	for (int i = 0; i < 32; i++) {
		SceUID cb = g_callbacks[i];
		if (cb != 0) {
			if (sceKernelNotifyCallback(cb, arg) == (int)0x800201A1) {
				g_callbacks[i] = 0;
			}
		}
	}
	sceKernelResumeDispatchThread(state);
}

static int _msRegisterCallbackDevctl(SceIoIob *iob, const SceUID *indata, SceSize inlen)
{
	if (inlen < 4 || indata == NULL) {
		return 0x80010016;
	}

	void *cbBlock = NULL;
	if (sceKernelGetUIDcontrolBlock(*indata, &cbBlock) != 0) {
		return 0x80010016;
	}

	u32 allowed = 2;
	if ((pspGetK1() >> 31) != 0) {
		allowed = *(u16 *)((u8 *)cbBlock + 0x16) & 2u;
	}
	if (allowed == 0) {
		return 0x80010016;
	}
	return _msAddCallback(*indata);
}

static int _msUnregisterCallbackDevctl(SceIoIob *iob, const SceUID *indata, SceSize inlen)
{
	if (inlen < 4 || indata == NULL) {
		return 0x80010016;
	}

	void *cbBlock = NULL;
	if (sceKernelGetUIDcontrolBlock(*indata, &cbBlock) != 0) {
		return 0x80010016;
	}

	u32 allowed = 2;
	if ((pspGetK1() >> 31) != 0) {
		allowed = *(u16 *)((u8 *)cbBlock + 0x16) & 2u;
	}
	if (allowed == 0) {
		return 0x80010016;
	}
	return _msRemoveCallback(*indata);
}

static int _msOpenInternal(SceIoIob *iob, const char *file, int flags, int mode, u32 cmd)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	u32 syscallRa = ((s32)pspGetK1() < 0) ? sceKernelGetSyscallRA() : 0;
	if (iob->i_unit > 0) {
		return 0x80010013;
	}

	int intr = sceKernelCpuSuspendIntr();
	int slotIdx = -1;
	for (int i = 0; i < 32; i++) {
		if (g_fileSlots[i].iob == NULL) {
			slotIdx = i;
			break;
		}
	}
	if (slotIdx < 0) {
		sceKernelCpuResumeIntr(intr);
		return 0x80010018;
	}

	SceMsfsFileSlot *slot = &g_fileSlots[slotIdx];
	slot->offset = 0;
	slot->flags = flags;
	iob->i_private = slot;
	slot->iob = iob;
	sceKernelCpuResumeIntr(intr);

	SceUID mtx = sceKernelCreateMutex("SceMsfs", SCE_KERNEL_SA_THPRI, 1, NULL);
	if (mtx < 0) {
		return mtx;
	}
	slot->mutexId = mtx;

	u32 half = g_workSize >> 1;
	char *work = (char *)g_workBuf;
	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		goto fail;
	}

	strncpy(work, file, half);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);
	req->args[1] = (u32)flags;
	req->args[2] = (u32)mode;
	req->args[3] = syscallRa;

	u64 resp = 0;
	res = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, cmd, 4, 0, &resp);
	if (res != 0) {
		int ures = sceKernelUnlockMutex(g_mutexId, 1);
		if (ures != 0) {
			res = ures;
		}
		goto fail;
	}

	int fd = (int)resp;
	slot->kermitFd = fd;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	if (res != 0) {
		goto fail;
	}

	sceKernelUnlockMutex(slot->mutexId, 1);
	if (fd >= 0) {
		return fd;
	}
	res = fd;

fail:
	sceKernelDeleteMutex(mtx);
	intr = sceKernelCpuSuspendIntr();
	iob->i_private = NULL;
	slot->iob = NULL;
	sceKernelCpuResumeIntr(intr);
	return res;
}

static int _msKermitDevctl(const char *dev, u32 cmd, const void *indata, u32 inlen, void *outdata, u32 outlen)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);
	u32 chunkSize = (g_workSize >> 3) & ~0x3Fu;

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	char *workDev = (char *)g_workBuf;
	u8 *workIn = (u8 *)workDev + chunkSize;
	u8 *workOut = workIn + chunkSize;

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	strncpy(workDev, dev, chunkSize);
	workDev[chunkSize - 1] = '\0';
	sceKernelMemcpy(workIn, indata, inlen);
	memset(workOut, 0, outlen);

	req->cmd = 0xC60;
	sceKermitMemory_driver_AAF047AC(req, 0, workDev, strlen(workDev) + 1u, 1);
	req->args[1] = cmd;
	sceKermitMemory_driver_AAF047AC(req, 2, workIn, inlen, 1);
	req->args[3] = inlen;
	sceKermitMemory_driver_AAF047AC(req, 4, workOut, outlen, 2);
	req->args[5] = outlen;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_MSFS, 0xC60, 6, 0, &resp);
	if (kres == 0) {
		kres = (int)resp;
		sceKernelMemcpy(outdata, workOut, outlen);
	}

	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? kres : res;
}

static int _msGetFreeSpaceDevctl(SceIoIob *iob, void *indata, SceSize inlen)
{
	if (indata == NULL) {
		return 0x80010016;
	}
	u32 *info = *(u32 **)indata;
	if (((s32)(pspGetK1() & (u32)info)) < 0 || info == NULL) {
		return 0x80010016;
	}
	int res = _msQueryFreeSpace(info);
	return (res < 0) ? res : 0;
}

static int _msRemoveCallback(SceUID cbId)
{
	int state = sceKernelSuspendDispatchThread();
	int found = 0;
	for (int i = 0; i < 32; i++) {
		if (g_callbacks[i] == cbId) {
			g_callbacks[i] = 0;
			found = 1;
			break;
		}
	}
	sceKernelResumeDispatchThread(state);
	return found ? 0 : (int)0x80010016;
}

static int _msAddCallback(SceUID cbId)
{
	int state = sceKernelSuspendDispatchThread();
	int added = 0;
	for (int i = 0; i < 32; i++) {
		if (g_callbacks[i] == 0) {
			g_callbacks[i] = cbId;
			if (sceKernelNotifyCallback(cbId, 1) == (int)0x800201A1) {
				g_callbacks[i] = 0;
			} else {
				added = 1;
			}
			break;
		}
	}
	sceKernelResumeDispatchThread(state);
	return added ? 0 : (int)0x80010016;
}

static int _msQueryFreeSpace(u32 *info)
{
	int res = _msKermitDevctl("ms0:", 0x02425818, NULL, 0, info, 0x14);
	if (res < 0 || g_clampFreeSpace == 0) {
		return res;
	}

	s32 secSize = (s32)info[3];
	s32 secCount = (s32)info[4];
	s64 clusterBytes = (s64)secCount * (s64)secSize;

	s64 total0 = (s64)(u64)info[0] * clusterBytes;
	if (total0 > 0x7FFFFFFFLL || total0 < 0) {
		info[0] = (u32)(0x7FFFFFFF / (secCount * secSize));
	}

	s64 total1 = (s64)(u64)info[1] * clusterBytes;
	if (total1 > 0x7FFFFFFFLL || total1 < 0) {
		info[1] = (u32)(0x7FFFFFFF / (secCount * secSize));
	}

	s64 total2 = (s64)(u64)info[2] * clusterBytes;
	if (total2 > 0x7FFFFFFFLL || total2 < 0) {
		info[2] = (u32)(0x7FFFFFFF / (secCount * secSize));
	}

	return res;
}

static int _msInitCloseWorker(void)
{
	SceUID thid = sceKernelCreateThread("SceKermitMsfsClose", _msCloseWorkerThread, 0x10, 0x1000, 0x00100001u, NULL);
	if (thid < 0) {
		_msTermCloseWorker();
		return thid;
	}
	g_closeThreadId = thid;

	SceUID mtx = sceKernelCreateMutex("SceKermitMsfsClose", SCE_KERNEL_SA_THPRI, 0, NULL);
	if (mtx < 0) {
		_msTermCloseWorker();
		return mtx;
	}
	g_closeMutexId = mtx;

	SceUID reqPipe = sceKernelCreateMsgPipe("SceCompatMsfsCloseRequest", 1, 0, (void *)0x10, NULL);
	if (reqPipe < 0) {
		_msTermCloseWorker();
		return reqPipe;
	}
	g_closeReqPipeId = reqPipe;

	SceUID resPipe = sceKernelCreateMsgPipe("SceKermitMsfsCloseResult", 1, 0, (void *)0x8, NULL);
	if (resPipe < 0) {
		_msTermCloseWorker();
		return resPipe;
	}
	g_closeResPipeId = resPipe;
	return 0;
}

static int _msTermCloseWorker(void)
{
	if (g_closeResPipeId > 0) {
		sceKernelDeleteMsgPipe(g_closeResPipeId);
		g_closeResPipeId = -1;
	}
	if (g_closeReqPipeId > 0) {
		sceKernelDeleteMsgPipe(g_closeReqPipeId);
		g_closeReqPipeId = -1;
	}
	if (g_closeMutexId > 0) {
		sceKernelDeleteMutex(g_closeMutexId);
		g_closeMutexId = -1;
	}
	if (g_closeThreadId > 0) {
		sceKernelDeleteThread(g_closeThreadId);
		g_closeThreadId = -1;
	}
	return 0;
}

static int _msSendCloseReq(SceIoIob *iob, u32 cmd)
{
	int res = sceKernelLockMutex(g_closeMutexId, 1, NULL);
	if (res < 0) {
		return res;
	}

	SceMsfsCloseRequest req;
	req.tag = sceKernelGetSystemTimeLow();
	req.iob = iob;
	req.cmd = cmd;
	req.prio = sceKernelGetThreadCurrentPriority();

	res = sceKernelSendMsgPipe(g_closeReqPipeId, &req, sizeof(req), 0, NULL, NULL);
	if (res >= 0) {
		SceMsfsCloseResult reply;
		do {
			res = sceKernelReceiveMsgPipe(g_closeResPipeId, &reply, sizeof(reply), 0, NULL, NULL);
			if (res < 0) {
				goto out;
			}
		} while (reply.tag != req.tag);
		res = reply.result;
	}

out:
	sceKernelUnlockMutex(g_closeMutexId, 1);
	return res;
}

static int _msCloseWorkerThread(SceSize args, void *argp)
{
	for (;;) {
		sceKernelChangeThreadPriority(0, 0x10);
		SceMsfsCloseRequest req;
		if (sceKernelReceiveMsgPipe(g_closeReqPipeId, &req, sizeof(req), 0, NULL, NULL) < 0) {
			break;
		}
		if (req.iob == NULL) {
			break;
		}
		sceKernelChangeThreadPriority(0, req.prio);
		SceMsfsCloseResult reply;
		reply.tag = req.tag;
		reply.result = _msCloseDirect(req.iob, req.cmd);
		if (sceKernelSendMsgPipe(g_closeResPipeId, &reply, sizeof(reply), 0, NULL, NULL) < 0) {
			break;
		}
	}
	return 0;
}

static int _msStartCloseWorker(void)
{
	if (g_closeThreadId <= 0) {
		return 0x80000001;
	}
	int res = sceKernelStartThread(g_closeThreadId, 0, NULL);
	return (res < 0) ? res : 0;
}
