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

typedef struct SceFlashfsFileSlot {
	SceIoIob *iob;
	SceUID mutexId;
	int kermitFd;
	int flags;
	SceOff offset;
} SceFlashfsFileSlot;

int sceIoGetIobUserLevel(SceIoIob *iob);
int sceIoAddDrv(const SceIoDeviceTable *drv);
int sceIoDelDrv(const char *drv_name);

u32 strlen(const char *s);
int strncmp(const char *s1, const char *s2, u32 n);
int memcmp(const void *b1, const void *b2, u32 len);
int snprintf(char *buf, u32 size, const char *fmt, ...);

static int _flashIoInit(SceIoDeviceEntry *de);
static int _flashIoExit(SceIoDeviceEntry *de);
static int _flashIoOpen(SceIoIob *iob, char *file, int flags, int mode);
static int _flashIoClose(SceIoIob *iob);
static int _flashIoRead(SceIoIob *iob, char *data, SceSize len);
static int _flashIoWrite(SceIoIob *iob, const char *data, SceSize len);
static SceOff _flashIoLseek(SceIoIob *iob, SceOff ofs, int whence);
static int _flashIoIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
static int _flashIoRemove(SceIoIob *iob, const char *name);
static int _flashIoMkdir(SceIoIob *iob, const char *name, int mode);
static int _flashIoRmdir(SceIoIob *iob, const char *name);
static int _flashIoDopen(SceIoIob *iob, const char *dirname);
static int _flashIoDclose(SceIoIob *iob);
static int _flashIoDread(SceIoIob *iob, void *dir);
static int _flashIoGetstat(SceIoIob *iob, const char *name, void *stat);
static int _flashIoChstat(SceIoIob *iob, const char *name, void *stat, int bits);
static int _flashIoRename(SceIoIob *iob, const char *oldname, const char *newname);
static int _flashIoChdir(SceIoIob *iob, const char *name);
static int _flashIoDevctl(SceIoIob *iob, const char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);
static int _flashfatIoMount(SceIoIob *iob);
static int _flashfatIoUmount(SceIoIob *iob);
static int _flashSysEventHandler(int ev_id, char *ev_name, void *param, int *result);

static int _flashOpenInternal(SceIoIob *iob, const char *file, int flags, int mode, u32 cmd);
static int _flashCloseInternal(SceIoIob *iob, u32 cmd);
static int _flashCheckAccess(SceIoIob *iob, const char *path);
static int _flashGetPathPerm(int unit, const char *path);

static const SceIoDeviceFunction g_flashDrvFuncs = {
	.dt_init = _flashIoInit,
	.dt_exit = _flashIoExit,
	.dt_open = _flashIoOpen,
	.dt_close = _flashIoClose,
	.dt_read = _flashIoRead,
	.dt_write = _flashIoWrite,
	.dt_lseek = _flashIoLseek,
	.dt_ioctl = _flashIoIoctl,
	.dt_remove = _flashIoRemove,
	.dt_mkdir = _flashIoMkdir,
	.dt_rmdir = _flashIoRmdir,
	.dt_dopen = _flashIoDopen,
	.dt_dclose = _flashIoDclose,
	.dt_dread = _flashIoDread,
	.dt_getstat = _flashIoGetstat,
	.dt_chstat = _flashIoChstat,
	.dt_rename = _flashIoRename,
	.dt_chdir = _flashIoChdir,
	.dt_mount = NULL,
	.dt_umount = NULL,
	.dt_devctl = _flashIoDevctl,
	.dt_cancel = NULL,
};

static const SceIoDeviceFunction g_flashfatDrvFuncs = {
	.dt_init = _flashIoInit,
	.dt_exit = _flashIoExit,
	.dt_open = _flashIoOpen,
	.dt_close = _flashIoClose,
	.dt_read = _flashIoRead,
	.dt_write = NULL,
	.dt_lseek = NULL,
	.dt_ioctl = NULL,
	.dt_remove = NULL,
	.dt_mkdir = NULL,
	.dt_rmdir = NULL,
	.dt_dopen = NULL,
	.dt_dclose = NULL,
	.dt_dread = NULL,
	.dt_getstat = _flashIoGetstat,
	.dt_chstat = NULL,
	.dt_rename = NULL,
	.dt_chdir = NULL,
	.dt_mount = _flashfatIoMount,
	.dt_umount = _flashfatIoUmount,
	.dt_devctl = NULL,
	.dt_cancel = NULL,
};

static const SceIoDeviceTable g_flashDrv = {
	.dt_string = "flash",
	.dt_type = 0x10,
	.dt_size = 1,
	.dt_desc = "FLASH file",
	.dt_func = &g_flashDrvFuncs,
};

static const SceIoDeviceTable g_flashfatDrv = {
	.dt_string = "flashfat",
	.dt_type = 0x10,
	.dt_size = 1,
	.dt_desc = "FLASH fat file",
	.dt_func = &g_flashfatDrvFuncs,
};

static const char g_allowedVshRes[19][26] = {
	"htmlviewer.res",
	"savedata_utility.rco",
	"msgdialog_plugin.rco",
	"auth_plugin.rco",
	"netconf_dialog.rco",
	"dnas_plugin.rco",
	"netplay_plugin.rco",
	"system_plugin.rco",
	"system_plugin_fg.rco",
	"game_install_plugin.rco",
	"dd_helper.rco",
	"npsignin_plugin.rco",
	"screenshot_plugin.rco",
	"ps3scan_plugin.rco",
	"store_checkout_plugin.rco",
	"npinstaller_plugin.rco",
	"npsignup_plugin.rco",
	"psn_plugin.rco",
	"store_browser_plugin.rco",
};

static SceSysEventHandler g_flashSysEvent = {
	.size = sizeof(SceSysEventHandler),
	.name = "sceKermitFlashfs",
	.type_mask = 0x00FFFF00,
	.handler = _flashSysEventHandler,
};

static SceUID g_mutexId;
static void *g_bufBase;
static void *g_workBuf;
static u32 g_workSize;
static SceFlashfsFileSlot g_fileSlots[32];

static inline int _flashIsValidBufferAddr(const void *addr)
{
	u32 a = (u32)addr;
	if (((a >> 14) & 0x7FFFu) == 4u) {
		if (((0x35u >> (a >> 29)) & 1u) != 0) {
			return 1;
		}
	}
	if (((a >> 23) & 0x3Fu) == 8u) {
		if (((0x35u >> (a >> 29)) & 1u) != 0) {
			return 1;
		}
	}
	return ((0x00220202u >> (a >> 27)) & 1u) != 0;
}

int module_start(SceSize args, void *argp)
{
	sceKernelMemset((void *)0xABD20000u, 0, 0x8000);
	g_workBuf = (void *)0xABD24000u;
	g_bufBase = (void *)0xABD20000u;
	g_workSize = 0x4000;

	SceUID mtx = sceKernelCreateMutex("SceKermitFlashfsFile", SCE_KERNEL_SA_THPRI, 0, NULL);
	g_mutexId = mtx;
	if (mtx <= 0) {
		return 1;
	}

	sceIoDelDrv("flashfat");
	if (sceIoAddDrv(&g_flashfatDrv) < 0) {
		sceKernelDeleteMutex(g_mutexId);
		return 1;
	}

	sceIoDelDrv("flash");
	if (sceIoAddDrv(&g_flashDrv) < 0) {
		sceKernelDeleteMutex(g_mutexId);
		return 1;
	}

	sceKernelRegisterSysEventHandler(&g_flashSysEvent);
	return 0;
}

static int _flashIoExit(SceIoDeviceEntry *de)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	sceKermitMemory_driver_AAF047AC(req, 0, de, 0xC, 3);
	u64 resp = 0;
	int res = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC6E, 1, 0, &resp);
	if (res != 0) {
		return res;
	}
	sceKermitMemory_driver_90B662D0(de, 0xC);
	return 0;
}

static int _flashIoRead(SceIoIob *iob, char *data, SceSize len)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	void *work = g_workBuf;

	if (iob->i_unit >= 5) {
		return 0x80010013;
	}
	if (!_flashIsValidBufferAddr(data)) {
		return 0x8001000E;
	}
	if ((s32)len < 0) {
		return 0x80010016;
	}

	SceFlashfsFileSlot *slot = (SceFlashfsFileSlot *)iob->i_private;
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

		u64 resp = 0;
		int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC5E, 3, 0, &resp);
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
		data += r;
		len -= (u32)r;
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

			u64 resp = 0;
			int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC5E, 3, 0, &resp);
			if (kres != 0) {
				int res = sceKernelUnlockMutex(g_mutexId, 1);
				return (res == 0) ? kres : res;
			}

			s32 r = (s32)resp;
			if (r <= 0) {
				if (r < 0) {
					totalRead = r;
				}
				goto done;
			}

			sceKermitMemory_driver_90B662D0(data, (u32)r);
			len -= (u32)r;
			data += r;
			totalRead += r;
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

			u64 resp = 0;
			int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC5E, 3, 0, &resp);
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

static int _flashIoWrite(SceIoIob *iob, const char *data, SceSize len)
{
	if (iob->i_unit >= 5) {
		return 0x80010013;
	}
	if (!_flashIsValidBufferAddr(data)) {
		return 0x8001000E;
	}
	if ((s32)len < 0) {
		return 0x80010016;
	}

	SceFlashfsFileSlot *slot = (SceFlashfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return 0x80010009;
	}

	if ((s32)len > 0) {
		slot->offset += (s32)len;
		iob->i_fpos = slot->offset;
	}
	return (int)len;
}

static SceOff _flashIoLseek(SceIoIob *iob, SceOff ofs, int whence)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit >= 5) {
		return (SceOff)0x80010013u;
	}
	if ((u32)whence >= 3u) {
		return (SceOff)0x80010016u;
	}

	SceFlashfsFileSlot *slot = (SceFlashfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return (SceOff)0x80010009u;
	}

	req->args[0] = (u32)slot->kermitFd;
	req->args[1] = (u64)ofs;
	req->args[2] = (u32)whence;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC62, 3, 0, &resp);
	if (kres != 0) {
		return (SceOff)kres;
	}

	s32 r = (s32)resp;
	if (r < 0) {
		return (SceOff)r;
	}

	slot->offset = (SceOff)r;
	iob->i_fpos = (SceOff)r;
	return slot->offset;
}

static int _flashIoRemove(SceIoIob *iob, const char *name)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	char *work = (char *)g_workBuf;
	u32 half = g_workSize >> 1;

	if (_flashCheckAccess(iob, name) != 0) {
		return 0x8001000D;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	snprintf(work, half, "%s%d:%s", "flash", iob->i_unit, name);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC6C, 1, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _flashIoMkdir(SceIoIob *iob, const char *name, int mode)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	char *work = (char *)g_workBuf;
	u32 half = g_workSize >> 1;

	if (_flashCheckAccess(iob, name) != 0) {
		return 0x8001000D;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	snprintf(work, half, "%s%d:%s", "flash", iob->i_unit, name);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);
	req->args[1] = (u32)mode;

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC66, 2, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _flashIoRmdir(SceIoIob *iob, const char *name)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	char *work = (char *)g_workBuf;
	u32 half = g_workSize >> 1;

	if (_flashCheckAccess(iob, name) != 0) {
		return 0x8001000D;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	snprintf(work, half, "%s%d:%s", "flash", iob->i_unit, name);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC61, 1, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _flashIoDread(SceIoIob *iob, void *dir)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit >= 5) {
		return 0x80010013;
	}

	SceFlashfsFileSlot *slot = (SceFlashfsFileSlot *)iob->i_private;
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

	void *work = g_workBuf;
	req->args[0] = (u32)slot->kermitFd;
	sceKermitMemory_driver_AAF047AC(req, 1, work, 0x160, 2);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC65, 2, 0, &resp);
	if (kres != 0) {
		res = sceKernelUnlockMutex(g_mutexId, 1);
		sceKernelUnlockMutex(slot->mutexId, 1);
		return (res != 0) ? res : kres;
	}

	s32 cmdRes = (s32)resp;
	if (cmdRes > 0) {
		iob->i_fpos++;
		sceKernelMemcpy(dir, work, 0x160);
		cmdRes = (s32)strlen((const char *)dir + 0x58);
	}

	res = sceKernelUnlockMutex(g_mutexId, 1);
	sceKernelUnlockMutex(slot->mutexId, 1);
	if (res != 0) {
		return res;
	}

	if (cmdRes <= 0) {
		sceKernelMemset(dir, 0, 0x160);
	}
	return cmdRes;
}

static int _flashIoGetstat(SceIoIob *iob, const char *name, void *stat)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (_flashCheckAccess(iob, name) != 0) {
		return 0x8001000D;
	}
	if (iob->i_unit >= 5) {
		return 0x80010013;
	}

	u32 half = g_workSize >> 1;
	char *work = (char *)g_workBuf;
	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	snprintf(work, half, "%s%d:%s", "flash", iob->i_unit, name);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);

	void *statWork = (u8 *)g_workBuf + (g_workSize >> 1);
	sceKermitMemory_driver_AAF047AC(req, 1, statWork, 0x58, 2);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC69, 2, 0, &resp);
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

static int _flashIoChstat(SceIoIob *iob, const char *name, void *stat, int bits)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u32 half = g_workSize >> 1;

	if (_flashCheckAccess(iob, name) != 0) {
		return 0x8001000D;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	char *nameWork = NULL;
	if (name != NULL) {
		nameWork = (char *)g_workBuf;
		snprintf(nameWork, half, "%s%d:%s", "flash", iob->i_unit, name);
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
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC5D, 3, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _flashIoRename(SceIoIob *iob, const char *oldname, const char *newname)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	u32 half = g_workSize >> 1;
	char *work1 = (char *)g_workBuf;

	if (_flashCheckAccess(iob, oldname) != 0) {
		return 0x8001000D;
	}
	if (_flashCheckAccess(iob, newname) != 0) {
		return 0x8001000D;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	snprintf(work1, half, "%s%d:%s", "flash", iob->i_unit, oldname);
	work1[half - 1] = '\0';

	char *work2 = (char *)g_workBuf + (g_workSize >> 1);
	snprintf(work2, half, "%s%d:%s", "flash", iob->i_unit, newname);
	work2[half - 1] = '\0';

	sceKermitMemory_driver_AAF047AC(req, 0, work1, strlen(work1) + 1u, 1);
	sceKermitMemory_driver_AAF047AC(req, 1, work2, strlen(work2) + 1u, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC68, 2, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _flashIoChdir(SceIoIob *iob, const char *name)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	char *work = (char *)g_workBuf;
	u32 half = g_workSize >> 1;

	if (_flashCheckAccess(iob, name) != 0) {
		return 0x8001000D;
	}

	int res = sceKernelLockMutex(g_mutexId, 1, NULL);
	if (res != 0) {
		return res;
	}

	snprintf(work, half, "%s%d:%s", "flash", iob->i_unit, name);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);

	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC6D, 1, 0, &resp);
	int cmdRes = (kres == 0) ? (int)resp : kres;
	res = sceKernelUnlockMutex(g_mutexId, 1);
	return (res == 0) ? cmdRes : res;
}

static int _flashIoDevctl(SceIoIob *iob, const char *dev, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen)
{
	if (_flashCheckAccess(iob, dev) != 0) {
		return 0x8001000D;
	}
	return (cmd == 0x100) ? 0 : 0x80010016;
}

static int _flashIoInit(SceIoDeviceEntry *de)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	sceKermitMemory_driver_AAF047AC(req, 0, de, 0xC, 3);
	u64 resp = 0;
	int res = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, 0xC63, 1, 0, &resp);
	if (res != 0) {
		return res;
	}
	sceKermitMemory_driver_90B662D0(de, 0xC);
	return (int)resp;
}

static int _flashIoOpen(SceIoIob *iob, char *file, int flags, int mode)
{
	if (_flashCheckAccess(iob, file) != 0) {
		return 0x8001000D;
	}
	return _flashOpenInternal(iob, file, flags, mode, 0xC67);
}

static int _flashIoClose(SceIoIob *iob)
{
	return _flashCloseInternal(iob, 0xC6B);
}

static int _flashIoIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen)
{
	if (cmd <= 0x208009) {
		if (cmd >= 0x208006 || cmd == 0x208003) {
			return 0;
		}
		return 0x80010016;
	}
	if (cmd == 0x208020) {
		return 0;
	}
	return 0x80010016;
}

static int _flashIoDopen(SceIoIob *iob, const char *dirname)
{
	if (_flashCheckAccess(iob, dirname) != 0) {
		return 0x8001000D;
	}
	return _flashOpenInternal(iob, dirname, 0, 0, 0xC5F);
}

static int _flashIoDclose(SceIoIob *iob)
{
	return _flashCloseInternal(iob, 0xC64);
}

static int _flashSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	return 0;
}

static int _flashfatIoMount(SceIoIob *iob)
{
	return 0;
}

static int _flashfatIoUmount(SceIoIob *iob)
{
	return 0;
}

static int _flashOpenInternal(SceIoIob *iob, const char *file, int flags, int mode, u32 cmd)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit >= 5) {
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

	SceFlashfsFileSlot *slot = &g_fileSlots[slotIdx];
	slot->offset = 0;
	slot->flags = flags;
	iob->i_private = slot;
	slot->iob = iob;
	sceKernelCpuResumeIntr(intr);

	SceUID mtx = sceKernelCreateMutex("SceFlashfs", SCE_KERNEL_SA_THPRI, 1, NULL);
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

	snprintf(work, half, "%s%d:%s", "flash", iob->i_unit, file);
	work[half - 1] = '\0';
	sceKermitMemory_driver_AAF047AC(req, 0, work, strlen(work) + 1u, 1);
	req->args[1] = (u32)flags;
	req->args[2] = (u32)mode;

	u64 resp = 0;
	res = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, cmd, 3, 0, &resp);
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

static int _flashCloseInternal(SceIoIob *iob, u32 cmd)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (iob->i_unit >= 5) {
		return 0x80010013;
	}

	SceFlashfsFileSlot *slot = (SceFlashfsFileSlot *)iob->i_private;
	if (slot < &g_fileSlots[0] || slot >= &g_fileSlots[32] || slot->iob != iob) {
		return 0x80010009;
	}

	req->args[0] = (u32)slot->kermitFd;
	u64 resp = 0;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_FLASHFS, cmd, 1, 0, &resp);
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

static int _flashCheckAccess(SceIoIob *iob, const char *path)
{
	int perm = _flashGetPathPerm(iob->i_unit, path);
	if ((perm & 4) != 0) {
		int level = sceIoGetIobUserLevel(iob);
		if (level != 4 && level != 8) {
			return 0x8001000D;
		}
	}
	if ((perm & 2) != 0) {
		int level = sceIoGetIobUserLevel(iob);
		if (level != 4 && level != 8) {
			return 0x8001000D;
		}
	}
	return 0;
}

static int _flashGetPathPerm(int unit, const char *path)
{
	strlen(path);
	if (unit == 1) {
		return 0x27;
	}
	if (unit >= 2) {
		return 2;
	}
	if (unit != 0) {
		return 0x27;
	}
	if (memcmp("/kd", path, 3) == 0) {
		return 0x27;
	}
	if (memcmp("/vsh/resource/", path, 14) == 0) {
		for (int i = 0; i < 19; i++) {
			if (strncmp(g_allowedVshRes[i], path + 14, 26) == 0) {
				return 0x21;
			}
		}
		return 0x27;
	}
	if (memcmp("/vsh/etc/", path, 9) == 0) {
		return (strncmp("index_01g.dat", path + 9, 14) == 0) ? 0x27 : 0x21;
	}
	if (memcmp("/vsh", path, 4) == 0) {
		return 0x27;
	}
	if (memcmp("/data", path, 5) == 0) {
		return 0x27;
	}
	return (path[0] == '/') ? 0x21 : 0x20;
}
