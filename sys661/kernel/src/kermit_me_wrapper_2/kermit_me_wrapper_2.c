#include <kermit.h>

extern void me_boot_code(void);
extern void me_boot_code_end(void);

extern int sceDmacplus_driver_var_AA2E7E54;
extern void *sceDmacplus_driver_var_C10B947A;

int sceIoOpen(const char *file, int flags, int mode);
SceOff sceIoLseek(SceUID fileDesc, SceOff offset, int whence);
int sceIoRead(SceUID fileDesc, void *data, SceSize size);
int sceIoClose(SceUID fileDesc);

int sceKernelMemset32(void *dst, int val, SceSize size);
int sceKernelGetCompiledSdkVersion(void);
int memcmp(const void *lhs, const void *rhs, SceSize n);
void *memcpy(void *dst, const void *src, SceSize n);

int sceKernelTryLockMutex(SceUID mutexid, int lock_count);
u64 sceKernelGetSystemTimeWide(void);

int UtilsForKernel_6C6887EE(void *dst, u32 max_size, const void *src, void **end_ptr);
void sceKernelIcacheInvalidateAll(void);
int sceDdrFlush(int mode);

int sceSysregAvcResetEnable(void);
int sceSysregVmeResetEnable(void);
int sceSysregMeBusClockEnable(void);
int sceSysregMeResetEnable(void);
int sceSysregMeResetDisable(void);
int sceSysregAvcResetDisable(void);
int sceSysregVmeResetDisable(void);
u32 sceSysregGetTachyonVersion(void);

int sceMeCore_driver_FA398D71(u32 cmd, ...);
int sceMeCore_driver_635397BB(u32 cmd, ...);
int sceMePowerControlAvcPower(int enable);
int sceMePower_driver_984E2608(int enable);
int sceMeWrapperEnd(void);

static int meCoreRpcAux(u32 cmd, ...);
static int meSysEventHandler(int ev_id, char *ev_name, void *param, int *result);
static int avcCheckError(u32 *info, int error);
static int mpegCheckError(u32 *info, int error);
static int audioCheckError(u32 codec, u32 *info, int error);

static const u32 g_at3Table[32] = {
	0x00000000, 0x00000100, 0x00000001, 0x00000100,
	0x00000000, 0x000000D4, 0x00000001, 0x000000D4,
	0x00000000, 0x000000C0, 0x00000001, 0x000000C0,
	0x00000000, 0x00000098, 0x00000001, 0x00000098,
	0x00000000, 0x00000088, 0x00000001, 0x00000088,
	0x00000000, 0x00000060, 0x00000001, 0x00000060,
	0x00000001, 0x00000044, 0x00000001, 0x00000030,
	0x00000002, 0x000000C0, 0x00000002, 0x00000098,
};

typedef struct SceMeSysEventParam {
	int size;
	int mode;
} SceMeSysEventParam;

typedef struct SceMeAvcDecodeParam {
	u32 streamWidth;
	u32 streamHeight;
	u32 frameCropLeft;
	u32 frameCropRight;
	u32 frameCropTop;
	u32 frameCropBottom;
	u32 numStoredFrames;
	u32 decodeStatus;
	u32 cscOutputActive;
	u32 frameMode;
} SceMeAvcDecodeParam;

static SceSysEventHandler g_meRpcSysEvent = {
	.size = sizeof(SceSysEventHandler),
	.name = "SceMeRpc",
	.type_mask = 0x01FFFF00,
	.handler = meSysEventHandler,
};

static int g_videoCodecMask;
static u64 g_lastRpcTime;
int sceMeCore_driver_var_385A4CC6;
static SceUID g_rpcMutex;
static SceUID g_rpcMutexAux;
static int g_hasAvcTimestamp;
int sceMeCore_driver_var_AB354CA1;
static SceUID g_kermitMeMutex;
static SceUID g_kermitMeMutexAux;
static void *g_kermitMeBufBase;
static void *g_kermitMeBufBaseAux;
static void *g_kermitMeWorkBuf;
static u32 g_kermitMeWorkSize;
static u32 g_avcTimestamp[4];
static u32 g_meStarted;
int sceMeCore_driver_var_1B960AB4;
int sceMeCore_driver_var_61FF4837;

int sceMeVideo_driver_C441994C(int codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	if (info[4] == 0) {
		return -1;
	}
	if (codec == 0) {
		int ret = sceMeCore_driver_635397BB(codec, info[4]);
		info[6] = ret;
		if (ret == 0) {
			info[2] = -8;
			info[8] = 0;
			return -3;
		}
		info[2] = 1;
		info[8] = ((SceMeAvcDecodeParam *)info[4])->streamHeight;
		return 0;
	} else if (codec == 1) {
		u32 val = sceMeCore_driver_FA398D71(0x24);
		u32 ret = sceMeCore_driver_FA398D71(0x23, info[11], info[12], info[13]);
		info[6] = val;
		info[8] = ret;
		if ((val != 0) & (ret != 0)) {
			info[2] = 0;
			return 0;
		}
		info[2] = 0x80000000u;
		return -3;
	}
	return -1;
}

int sceMeVideo_driver_E8CD3C75(int codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	info[2] = 0;
	int intr = sceKernelCpuSuspendIntr();
	int mask = g_videoCodecMask;
	if (sceKernelGetCompiledSdkVersion() > 0x05FFFFFF) {
		if (((mask >> codec) & 1) != 0) {
			sceKernelCpuResumeIntr(intr);
			return -4;
		}
		mask |= (1 << codec);
		if ((mask & 3) == 3) {
			sceKernelCpuResumeIntr(intr);
			return -4;
		}
	} else {
		if (mask != 0) {
			sceKernelCpuResumeIntr(intr);
			return -4;
		}
		mask = (1 << codec);
	}
	g_videoCodecMask = mask;
	sceKernelCpuResumeIntr(intr);

	if (codec == 1) {
		int ret = sceMeCore_driver_635397BB(0x25, &info[3], info[5], info[6], 0);
		if (ret != 0) {
			return mpegCheckError(info, ret);
		}
	} else if (codec == 0) {
		int ret = sceMeCore_driver_635397BB(1, &info[3], info[5], info[7]);
		if (ret < 0) {
			return avcCheckError(info, ret);
		}
		sceMePowerControlAvcPower(1);
	} else if (codec != 3) {
		return -1;
	}
	sceMePower_driver_984E2608(1);
	return 0;
}

int sceMeVideo_driver_8768915D(int codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	info[2] = 0;
	if (codec == 0) {
		u32 count = info[15];
		if ((count - 1u) >= 4u) {
			return -1;
		}
		u32 *list = (u32 *)info[11];
		u32 list_size = count * 0x2Cu;
		if (((s32)(((u32)list | ((u32)list + list_size) | list_size) & pspGetK1())) < 0) {
			return -1;
		}
		u32 seiBuf = info[14];
		if (seiBuf != 0) {
			u32 buf_size = count * 0x148u;
			if (((s32)((seiBuf | (seiBuf + buf_size) | buf_size) & pspGetK1())) < 0) {
				return -1;
			}
			for (int i = (int)count - 1; i >= 0; i--) {
				u32 val = seiBuf + (u32)i * 0x148u;
				list[i * 11 + 9] = val;
				list[i * 11 + 10] = val + 0xA4u;
			}
		} else {
			for (int i = (int)count - 1; i >= 0; i--) {
				list[i * 11 + 9] = 0;
				list[i * 11 + 10] = 0;
			}
		}
		sceKernelDcacheWritebackInvalidateRange(list, info[15] * 0x2Cu);
		sceKernelDcacheWritebackInvalidateRange(&info[11], 0x30);
		int ret = meCoreRpcAux(2, info[3], info[9], info[10], info[4], list, info[12], info[13], &info[11]);
		if (ret < 0) {
			return avcCheckError(info, ret);
		}
		return 0;
	} else if (codec == 1) {
		u32 *mpegCtx = (u32 *)info[4];
		if (((s32)(((u32)mpegCtx | ((u32)mpegCtx + 0x100u)) & pspGetK1())) < 0) {
			return -1;
		}
		mpegCtx[1] = info[10];
		mpegCtx[0] = info[9];
		mpegCtx[3] = 0x40;
		mpegCtx[2] = info[14];
		mpegCtx[4] = 0;
		sceKernelDcacheWritebackInvalidateRange(mpegCtx, 0x100);
		int ret = sceMeCore_driver_FA398D71(0x20, info[3], mpegCtx, info[7], info[8]);
		if (ret != 0) {
			return mpegCheckError(info, ret);
		}
		return 0;
	}
	return -1;
}

int sceMeVideo_driver_4D78330C(int codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	info[2] = 0;
	if (codec == 0) {
		u32 count = info[15];
		if ((count - 1u) >= 4u) {
			return -1;
		}
		u32 *list = (u32 *)info[11];
		u32 list_size = count * 0x2Cu;
		if (((s32)(((u32)list | ((u32)list + list_size) | list_size) & pspGetK1())) < 0) {
			return -1;
		}
		u32 seiBuf = info[14];
		if (seiBuf != 0) {
			u32 buf_size = count * 0x148u;
			if (((s32)((seiBuf | (seiBuf + buf_size) | buf_size) & pspGetK1())) < 0) {
				return -1;
			}
			for (int i = (int)count - 1; i >= 0; i--) {
				u32 val = seiBuf + (u32)i * 0x148u;
				list[i * 11 + 9] = val;
				list[i * 11 + 10] = val + 0xA4u;
			}
		} else {
			for (int i = (int)count - 1; i >= 0; i--) {
				list[i * 11 + 9] = 0;
				list[i * 11 + 10] = 0;
			}
		}
		int ret = sceMeCore_driver_635397BB(3, info[3], info[4], list);
		if (ret < 0) {
			return avcCheckError(info, ret);
		}
		return 0;
	} else if (codec == 1) {
		int ret = sceMeCore_driver_635397BB(0x21, info[3], -1);
		if (ret != 0) {
			return mpegCheckError(info, ret);
		}
		return 0;
	}
	return -1;
}

int sceMeVideo_driver_8DD56014(int codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	int intr = sceKernelCpuSuspendIntr();
	int mask = g_videoCodecMask;
	if (((mask >> codec) & 1) == 0) {
		sceKernelCpuResumeIntr(intr);
		return -4;
	}
	g_videoCodecMask = mask - (1 << codec);
	sceKernelCpuResumeIntr(intr);
	info[2] = 0;

	if (codec == 1) {
		int ret = sceMeCore_driver_635397BB(0x22, info[3]);
		if (ret != 0) {
			return mpegCheckError(info, ret);
		}
	} else if (codec == 0) {
		int ret = sceMeCore_driver_635397BB(4, info[3]);
		if (ret < 0) {
			return avcCheckError(info, ret);
		}
		sceMePowerControlAvcPower(0);
	} else if (codec != 3) {
		return -1;
	}
	if (g_videoCodecMask == 0) {
		sceMePower_driver_984E2608(0);
	}
	return 0;
}

int sceMeVideo_driver_6D68B223(int codec, u32 cmd, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	switch (cmd) {
	case 1: {
		if (codec != 0) {
			return -1;
		}
		info[2] = 0;
		int ret = sceMeCore_driver_635397BB(5, info[3], info[4], info[16], info[17], info[18], info[19]);
		info[2] = 0;
		if (ret < 0) {
			return avcCheckError(info, ret);
		}
		return 0;
	}
	case 2: {
		if (codec != 1) {
			return -1;
		}
		u32 *mpegCtx = (u32 *)info[4];
		if (((s32)(((u32)mpegCtx | ((u32)mpegCtx + 0x100u)) & pspGetK1())) < 0) {
			return -1;
		}
		mpegCtx[0] = info[9];
		mpegCtx[1] = info[10];
		int ret = sceMeCore_driver_635397BB(0x26, info[3], mpegCtx);
		if (ret != 0) {
			info[11] = 0;
			info[12] = 0;
			info[13] = 0;
			return mpegCheckError(info, ret);
		}
		info[2] = 0;
		info[11] = mpegCtx[12];
		info[12] = mpegCtx[13];
		info[13] = mpegCtx[14];
		return 0;
	}
	case 3:
		if (codec == 0) {
			sceMeCore_driver_635397BB(6, &info[1], 0);
			return 0;
		} else if (codec == 1) {
			info[1] = sceMeCore_driver_FA398D71(0x27);
			return 0;
		}
		return -1;
	case 5: {
		if (codec != 0) {
			return -1;
		}
		info[2] = 0;
		int ret = sceMeCore_driver_635397BB(9, info[3], info[20]);
		info[2] = 0;
		if (ret < 0) {
			return avcCheckError(info, ret);
		}
		return 0;
	}
	case 6: {
		if (codec != 0) {
			return -1;
		}
		info[2] = 0;
		int ret = sceMeCore_driver_635397BB(10, info[3], info[21]);
		info[2] = 0;
		if (ret < 0) {
			return avcCheckError(info, ret);
		}
		return 0;
	}
	case 7: {
		if (codec != 0) {
			return -1;
		}
		info[2] = 0;
		int ret = sceMeCore_driver_635397BB(11, info[3], info[22]);
		info[2] = 0;
		if (ret < 0) {
			return avcCheckError(info, ret);
		}
		return 0;
	}
	default:
		return -1;
	}
}

int sceMeAudio_driver_81956A0B(u32 codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	switch (codec) {
	case 0x1000: {
		int ret = sceMeCore_driver_635397BB(0x63, &info[13], &info[15], &info[16], &info[10]);
		if (ret < 0) {
			return audioCheckError(0x1000, info, ret);
		}
		info[4] = 0x7BC0;
		info[2] = 0;
		return 0;
	}
	case 0x1001:
		info[4] = 0x3DE0;
		info[2] = 0;
		return 0;
	case 0x1002:
		info[4] = 0x3B68;
		info[2] = 0;
		return 0;
	case 0x1003:
		info[4] = 0x18F20;
		info[2] = 0;
		return 0;
	case 0x1005:
		info[4] = sceMeCore_driver_FA398D71(0xE2);
		info[2] = 0;
		return 0;
	default:
		return -1;
	}
}

int sceMeAudio_driver_6AD33F60(u32 codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	info[2] = 0;
	int ret = 0;
	switch (codec) {
	case 0x1000: {
		ret = sceMeCore_driver_635397BB(0x63, &info[13], &info[15], &info[16], &info[10]);
		if (ret < 0) {
			break;
		}
		u32 ch = info[15];
		info[2] = 0;
		if (ch == 0 || ch >= 8u) {
			info[2] = 0x202;
			return -3;
		}
		if (ch == 1) {
			ch = 2;
		} else if (ch >= 5) {
			ch += 1;
		}
		info[18] = ch;
		ret = sceMeCore_driver_635397BB(0x67, info[13], info[15], info[16], ch, info[3]);
		if (ret < 0) {
			break;
		}
		ret = sceMeCore_driver_635397BB(0x69, &info[11], info[3]);
		if (ret < 0) {
			break;
		}
		ret = sceMeCore_driver_635397BB(0x64, &info[17], info[3]);
		if (ret < 0) {
			break;
		}
		ret = sceMeCore_driver_FA398D71(0x68, info[5], info[3]);
		break;
	}
	case 0x1001: {
		u32 idx = info[10];
		u32 frameBytes = g_at3Table[idx * 2 + 1];
		info[11] = 44100;
		info[12] = frameBytes;
		ret = sceMeCore_driver_635397BB(0x73, g_at3Table[idx * 2], 44100, frameBytes, info[3], info[13]);
		if ((info[10] - 14u) < 2u) {
			info[13] = 2;
		}
		break;
	}
	case 0x1002:
		ret = sceMeCore_driver_635397BB(0x8B, info[3]);
		break;
	case 0x1003: {
		u32 sr = info[10];
		if (sr != 96000 && sr != 88200 && sr != 64000 && sr != 48000 &&
		    sr != 44100 && sr != 32000 && sr != 24000 && sr != 22050 &&
		    sr != 16000 && sr != 11025 && sr != 8000) {
			return -1;
		}
		ret = sceMeCore_driver_635397BB(0x93, info[10], info[3]);
		if (ret < 0) {
			break;
		}
		if ((info[11] & 0xFFFFu) != 0) {
			ret = sceMeCore_driver_635397BB(0x95, info[11] & 0xFFu, info[3]);
			if (ret < 0) {
				break;
			}
		}
		if (((info[11] >> 8) & 0xFFu) != 0) {
			ret = sceMeCore_driver_635397BB(0x97, 0, 2, 0, info[3]);
		}
		break;
	}
	case 0x1005:
		ret = sceMeCore_driver_635397BB(0xE3, &info[10], info[3]);
		break;
	default:
		return -1;
	}
	if (ret < 0) {
		return audioCheckError(codec, info, ret);
	}
	return ret;
}

int sceMeAudio_driver_B57F033A(u32 codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	info[2] = 0;
	int ret;
	if (codec == 0x1000) {
		ret = sceMeCore_driver_635397BB(0x63, &info[13], &info[15], &info[16], &info[10]);
		if (ret < 0) {
			return audioCheckError(codec, info, ret);
		}
		info[2] = 0;
		if (info[15] != 1) {
			return -1;
		}
		info[18] = 1;
		ret = sceMeCore_driver_635397BB(0x67, info[13], 1, info[16], 1, info[3]);
		if (ret >= 0) {
			ret = sceMeCore_driver_635397BB(0x69, &info[11], info[3]);
			if (ret >= 0) {
				ret = sceMeCore_driver_635397BB(0x64, &info[17], info[3]);
				if (ret >= 0) {
					ret = sceMeCore_driver_FA398D71(0x68, info[5], info[3]);
				}
			}
		}
	} else if (codec == 0x1001) {
		u32 idx = info[10];
		if ((idx - 14u) >= 2u) {
			return -1;
		}
		u32 frameBytes = g_at3Table[idx * 2 + 1];
		info[13] = 1;
		info[11] = 44100;
		info[12] = frameBytes;
		ret = sceMeCore_driver_635397BB(0x73, g_at3Table[idx * 2], 44100, frameBytes, info[3], 1);
	} else {
		return -1;
	}
	if (ret < 0) {
		return audioCheckError(codec, info, ret);
	}
	return ret;
}

int sceMeAudio_driver_9A9E21EE(u32 codec, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	if (info[3] == 0) {
		return 0x80000103;
	}
	info[9] = 0;
	info[2] = 0;
	sceKernelDcacheWritebackInvalidateRange(info, 0x68);
	int ret;
	switch (codec) {
	case 0x1000:
		ret = sceMeCore_driver_FA398D71(0x60, info);
		break;
	case 0x1001:
		ret = sceMeCore_driver_FA398D71(0x70, info);
		break;
	case 0x1002:
		ret = sceMeCore_driver_FA398D71(0x8C, info);
		break;
	case 0x1003:
		ret = sceMeCore_driver_FA398D71(0x90, info[6], &info[7], info[8], &info[9], info[3]);
		break;
	case 0x1005:
		ret = sceMeCore_driver_FA398D71(0xE5, info[6], info[15], &info[16], info[8], &info[9], &info[17], info[3]);
		break;
	default:
		return -1;
	}
	if (ret < 0) {
		return audioCheckError(codec, info, ret);
	}
	return 0;
}

int sceMeAudio_driver_C300D466(u32 codec, u32 cmd, u32 *info)
{
	if (info[0] > 0x05100601u) {
		return -2;
	}
	info[2] = 0;
	if (cmd == 3) {
		int ret;
		switch (codec) {
		case 0x1000:
		case 0x1001:
		case 0x1002:
		case 0x1003:
			ret = 0;
			break;
		case 0x1005:
			ret = sceMeCore_driver_FA398D71(0xE1);
			break;
		default:
			return -1;
		}
		info[1] = ret;
		return ret;
	} else if (cmd == 2) {
		if (codec != 0x1002) {
			return -1;
		}
		int ret = sceMeCore_driver_635397BB(0x89, info[10], info[3]);
		if (ret == 0) {
			ret = sceMeCore_driver_635397BB(0x82, info[6], &info[11], &info[14], info[3]);
		}
		if (ret < 0) {
			return audioCheckError(codec, info, ret);
		}
		return ret;
	} else if (cmd == 4) {
		if (codec != 0x1000) {
			return -1;
		}
		int ret = sceMeCore_driver_635397BB(0x63, &info[13], &info[15], &info[16], &info[10]);
		if (ret < 0) {
			return audioCheckError(codec, info, ret);
		}
		return ret;
	}
	return -1;
}

int sceMeVideo_driver_21521BE5(void *params)
{
	return sceMeCore_driver_635397BB(0x10, params);
}

void *sceMeMalloc(SceSize size)
{
	return (void *)sceMeCore_driver_FA398D71(0x180, size);
}

void *sceMeCalloc(SceSize num, SceSize size)
{
	return (void *)sceMeCore_driver_FA398D71(0x182, num, size);
}

void sceMeFree(void *ptr)
{
	sceMeCore_driver_FA398D71(0x181, ptr);
}

u32 sceMeCore_driver_905A7500(void)
{
	return (u32)g_videoCodecMask;
}

static int __attribute__((noinline)) avcCheckError(u32 *info, int error)
{
	if (error == (int)0x80000003) {
		return -1;
	}
	info[2] = error;
	return (error != -8) ? -3 : -4;
}

static int __attribute__((noinline)) mpegCheckError(u32 *info, int error)
{
	if (error == (int)0x80000003) {
		return -1;
	}
	info[2] = error;
	return (error >= 0) ? -3 : -4;
}

static int __attribute__((noinline)) audioCheckError(u32 codec, u32 *info, int error)
{
	if (error == (int)0x80000003) {
		return -1;
	}
	int ret;
	switch (codec) {
	case 0x1000:
		ret = (error < 0x100) ? -3 : -4;
		sceMeCore_driver_635397BB(0x6A, &info[2], info[3]);
		break;
	case 0x1001:
		ret = (error < 0x100) ? -3 : -4;
		sceMeCore_driver_635397BB(0x71, &info[2], info[3]);
		break;
	case 0x1002:
		info[2] = error;
		ret = -3;
		break;
	case 0x1003:
		ret = (error < 0x100) ? -3 : -4;
		sceMeCore_driver_635397BB(0x91, &info[2], info[3]);
		break;
	case 0x1005:
		ret = (error >= 0) ? -3 : -4;
		sceMeCore_driver_635397BB(0xE6, &info[2], info[3]);
		break;
	default:
		return -1;
	}
	if (ret == -4) {
		info[9] = 0;
	}
	return ret;
}

static int __attribute__((noinline)) sceKermitMeControlAvcPower(u32 enable)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	req->cmd = 0x41A;
	req->argWords[0].lo = enable;
	u64 resp;
	int res = sceKermit_driver_36666181(req, KERMIT_MODE_PERIPHERAL, 0x41A, 1, 0, &resp);
	return (res == 0) ? (int)resp : res;
}

int sceMePowerControlAvcPower(int enable)
{
	return sceKermitMeControlAvcPower((u32)enable);
}

int sceMePower_driver_984E2608(int enable)
{
	if (enable) {
		return sceMeCore_driver_FA398D71(0x183);
	}
	return sceMeCore_driver_FA398D71(0x184);
}

int sceMePower_driver_1862B784(int numerator, int denominator)
{
	return sceMeCore_driver_FA398D71(0x186, numerator, denominator);
}

int sceMePower_driver_E9F69ACF(int numerator, int denominator)
{
	return sceMeCore_driver_FA398D71(0x187, numerator, denominator);
}

int sceMePowerSelectAvcClock(int clock)
{
	return sceMeCore_driver_FA398D71(7, clock);
}

static int __attribute__((noinline)) meCheckBusy(void)
{
	int ret = sceKernelTryLockMutex(g_rpcMutex, 1);
	if (ret >= 0) {
		sceKernelUnlockMutex(g_rpcMutex, 1);
	}
	ret = sceKernelTryLockMutex(g_rpcMutexAux, 1);
	if (ret >= 0) {
		sceKernelUnlockMutex(g_rpcMutexAux, 1);
	}
	if ((ret < 0) & (sceMeCore_driver_var_385A4CC6 != 0)) {
		u64 now = sceKernelGetSystemTimeWide();
		if ((now - g_lastRpcTime) > 100000ULL) {
			return 0;
		}
	}
	return ret;
}

static int __attribute__((noinline)) meRpcLock(void)
{
	return sceKernelLockMutex(g_rpcMutex, 1, NULL);
}

static int __attribute__((noinline)) meRpcUnlock(void)
{
	return sceKernelUnlockMutex(g_rpcMutex, 1);
}

static int __attribute__((noinline)) meRpcLockAux(void)
{
	return sceKernelLockMutex(g_rpcMutexAux, 1, NULL);
}

static int __attribute__((noinline)) meRpcUnlockAux(void)
{
	return sceKernelUnlockMutex(g_rpcMutexAux, 1);
}

static int __attribute__((noinline)) sceKermitMeSetAvcTimeStampInternal(u32 tsPtsLo, u32 tsPtsHi, u32 tsDtsLo, u32 tsDtsHi)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	int res = sceKernelLockMutex(g_kermitMeMutex, 1, NULL);
	if (res != 0) {
		return res;
	}
	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	req->argWords[0].lo = tsPtsLo;
	req->argWords[1].lo = tsPtsHi;
	req->argWords[2].lo = tsDtsLo;
	req->argWords[3].lo = tsDtsHi;
	u64 resp;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_ME, 0xC5F, 4, 0, &resp);
	if (kres == 0) {
		kres = (int)resp;
	}
	res = sceKernelUnlockMutex(g_kermitMeMutex, 1);
	return (res == 0) ? kres : res;
}

static int __attribute__((noinline)) kermitMeRpc(u32 cmd, u32 wordA, u32 wordB, u32 wordC, u32 wordD, u32 wordE, u32 wordF, u32 wordG, u32 wordH)
{
	u8 stack_buf[0xC0];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x80);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	if (cmd == 2 && g_hasAvcTimestamp != 0) {
		int ts_res = sceKermitMeSetAvcTimeStampInternal(g_avcTimestamp[0], g_avcTimestamp[1], g_avcTimestamp[2], g_avcTimestamp[3]);
		g_hasAvcTimestamp = 0;
		if (ts_res < 0) {
			Kprintf("Failed to sceKermitMeSetAvcTimeStampInternal() : 0x%08x \n", ts_res);
			return ts_res;
		}
	}

	int res = sceKernelLockMutex(g_kermitMeMutex, 1, NULL);
	if (res != 0) {
		return res;
	}

	switch (cmd) {
	case 2:
		sceKermitMemory_driver_80E1240A((void *)wordB, wordC);
		sceKermitMemory_driver_80E1240A((void *)wordD, 0x28);
		sceKermitMemory_driver_80E1240A((void *)wordE, 0x2C);
		break;
	case 3:
		sceKermitMemory_driver_80E1240A((void *)wordB, 0x28);
		sceKermitMemory_driver_80E1240A((void *)wordC, 0x2C);
		break;
	case 0x10:
		sceKermitMemory_driver_80E1240A((void *)wordA, 0x38);
		break;
	case 0x140:
		sceKermitMemory_driver_80E1240A((void *)wordA, 0x1C20);
		sceKermitMemory_driver_80E1240A((void *)wordB, wordC);
		break;
	case 0x142:
	case 0x143:
	case 0x145:
	case 0x146:
	case 0x147:
		sceKermitMemory_driver_80E1240A((void *)wordA, 0x1C20);
		break;
	case 0x144:
		sceKermitMemory_driver_80E1240A((void *)wordA, wordB);
		break;
	case 0x150: {
		int prod = (int)((wordC >> 16) * (wordC & 0xFFFFu));
		sceKermitMemory_driver_80E1240A((void *)wordB, (u32)((prod * 3) / 2));
		break;
	}
	case 0x151:
	case 0x152:
		sceKermitMemory_driver_80E1240A((void *)wordA, 0x2C);
		break;
	default:
		break;
	}

	req->cmd = 0xC60;
	req->argWords[0].lo = cmd;
	req->argWords[1].lo = wordA;
	req->argWords[2].lo = wordB;
	req->argWords[3].lo = wordC;
	req->argWords[4].lo = wordD;
	req->argWords[5].lo = wordE;
	req->argWords[6].lo = wordF;
	req->argWords[7].lo = wordG;
	req->argWords[8].lo = wordH;

	u64 resp;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_ME, 0xC60, 9, 0, &resp);
	if (kres == 0) {
		kres = (int)resp;
		switch (cmd) {
		case 0:
			sceKermitMemory_driver_90B662D0((void *)wordA, 0x28);
			break;
		case 1:
		case 6:
			sceKermitMemory_driver_90B662D0((void *)wordA, 4);
			break;
		case 2: {
			SceMeAvcDecodeParam *avcDetail = (SceMeAvcDecodeParam *)wordD;
			sceKermitMemory_driver_90B662D0((void *)wordD, 0x28);
			sceKermitMemory_driver_90B662D0((void *)wordE, 0x2C);
			sceKermitMemory_driver_90B662D0((void *)wordF, 0x64);
			if (sceDmacplus_driver_var_AA2E7E54 != 0 && kres == 1 && avcDetail != NULL && avcDetail->cscOutputActive != 0) {
				void *p = sceDmacplus_driver_var_C10B947A;
				if (p != NULL) {
					sceKermitMemory_driver_90B662D0(p, 0x88000);
				}
			}
			break;
		}
		case 3:
			sceKermitMemory_driver_90B662D0((void *)wordB, 0x28);
			sceKermitMemory_driver_90B662D0((void *)wordC, 0x2C);
			break;
		case 5:
			sceKermitMemory_driver_90B662D0((void *)wordB, 0x28);
			break;
		case 0x10: {
			u32 *info = (u32 *)wordA;
			u32 pixels = info[0] * info[1];
			sceKermitMemory_driver_90B662D0((void *)info[11], (pixels * 3u) >> 1);
			break;
		}
		case 0x140:
		case 0x142:
		case 0x143:
			sceKermitMemory_driver_90B662D0((void *)wordA, 0x1C20);
			break;
		case 0x144:
			sceKermitMemory_driver_90B662D0((void *)wordC, wordD);
			break;
		case 0x150:
			sceKermitMemory_driver_90B662D0((void *)wordA, (wordD * (wordC & 0xFFFFu)) << 2);
			break;
		case 0x151:
		case 0x152:
			sceKermitMemory_driver_90B662D0((void *)wordA, 0x2C);
			break;
		default:
			break;
		}
	}

	res = sceKernelUnlockMutex(g_kermitMeMutex, 1);
	return (res == 0) ? kres : res;
}

static int __attribute__((noinline)) kermitMeRpcAux(u32 cmd, u32 wordA, u32 wordB, u32 wordC, u32 wordD, u32 wordE, u32 wordF, u32 wordG, u32 wordH)
{
	u8 stack_buf[0xC0];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x80);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);

	int res = sceKernelLockMutex(g_kermitMeMutexAux, 1, NULL);
	if (res != 0) {
		return res;
	}

	if (cmd == 2) {
		sceKermitMemory_driver_80E1240A((void *)wordB, wordC);
		sceKermitMemory_driver_80E1240A((void *)wordD, 0x28);
		sceKermitMemory_driver_80E1240A((void *)wordE, 0x2C);
	}

	req->cmd = 0xC60;
	req->argWords[0].lo = cmd;
	req->argWords[1].lo = wordA;
	req->argWords[2].lo = wordB;
	req->argWords[3].lo = wordC;
	req->argWords[4].lo = wordD;
	req->argWords[5].lo = wordE;
	req->argWords[6].lo = wordF;
	req->argWords[7].lo = wordG;
	req->argWords[8].lo = wordH;

	u64 resp;
	int kres = sceKermit_driver_36666181(req, KERMIT_MODE_USBPSPCM, 0xC60, 9, 0, &resp);
	if (kres == 0) {
		kres = (int)resp;
		if (cmd == 2) {
			sceKermitMemory_driver_90B662D0((void *)wordD, 0x28);
			sceKermitMemory_driver_90B662D0((void *)wordE, 0x2C);
			sceKermitMemory_driver_90B662D0((void *)wordF, 0x64);
		}
	}

	res = sceKernelUnlockMutex(g_kermitMeMutexAux, 1);
	return (res == 0) ? kres : res;
}

int sceMeCore_driver_FA398D71(u32 cmd, ...)
{
	__builtin_va_list ap;
	__builtin_va_start(ap, cmd);
	u32 wordA = __builtin_va_arg(ap, u32);
	u32 wordB = __builtin_va_arg(ap, u32);
	u32 wordC = __builtin_va_arg(ap, u32);
	u32 wordD = __builtin_va_arg(ap, u32);
	u32 wordE = __builtin_va_arg(ap, u32);
	u32 wordF = __builtin_va_arg(ap, u32);
	u32 wordG = __builtin_va_arg(ap, u32);
	u32 wordH = __builtin_va_arg(ap, u32);
	__builtin_va_end(ap);

	g_lastRpcTime = sceKernelGetSystemTimeWide();
	int ret = sceKernelLockMutex(g_rpcMutex, 1, NULL);
	if (ret >= 0) {
		ret = kermitMeRpc(cmd, wordA, wordB, wordC, wordD, wordE, wordF, wordG, wordH);
		sceKernelUnlockMutex(g_rpcMutex, 1);
	}
	return ret;
}

static int __attribute__((noinline)) meCoreRpcAux(u32 cmd, ...)
{
	__builtin_va_list ap;
	__builtin_va_start(ap, cmd);
	u32 wordA = __builtin_va_arg(ap, u32);
	u32 wordB = __builtin_va_arg(ap, u32);
	u32 wordC = __builtin_va_arg(ap, u32);
	u32 wordD = __builtin_va_arg(ap, u32);
	u32 wordE = __builtin_va_arg(ap, u32);
	u32 wordF = __builtin_va_arg(ap, u32);
	u32 wordG = __builtin_va_arg(ap, u32);
	u32 wordH = __builtin_va_arg(ap, u32);
	__builtin_va_end(ap);

	g_lastRpcTime = sceKernelGetSystemTimeWide();
	int ret = sceKernelLockMutex(g_rpcMutexAux, 1, NULL);
	if (ret >= 0) {
		ret = kermitMeRpcAux(cmd, wordA, wordB, wordC, wordD, wordE, wordF, wordG, wordH);
		sceKernelUnlockMutex(g_rpcMutexAux, 1);
	}
	return ret;
}

int sceMeCore_driver_635397BB(u32 cmd, ...)
{
	__builtin_va_list ap;
	__builtin_va_start(ap, cmd);
	u32 wordA = __builtin_va_arg(ap, u32);
	u32 wordB = __builtin_va_arg(ap, u32);
	u32 wordC = __builtin_va_arg(ap, u32);
	u32 wordD = __builtin_va_arg(ap, u32);
	u32 wordE = __builtin_va_arg(ap, u32);
	u32 wordF = __builtin_va_arg(ap, u32);
	u32 wordG = __builtin_va_arg(ap, u32);
	u32 wordH = __builtin_va_arg(ap, u32);
	__builtin_va_end(ap);

	sceKernelDcacheWritebackInvalidateAll();
	g_lastRpcTime = sceKernelGetSystemTimeWide();
	int ret = sceKernelLockMutex(g_rpcMutex, 1, NULL);
	if (ret >= 0) {
		ret = kermitMeRpc(cmd, wordA, wordB, wordC, wordD, wordE, wordF, wordG, wordH);
		sceKernelUnlockMutex(g_rpcMutex, 1);
	}
	return ret;
}

static int __attribute__((noinline)) initRpc(void)
{
	g_rpcMutex = sceKernelCreateMutex("SceMediaEngineRpc", 0x101, 0, NULL);
	if (g_rpcMutex < 0) {
		return g_rpcMutex;
	}
	g_rpcMutexAux = sceKernelCreateMutex("SceMediaEngineRpc2", 0x101, 0, NULL);
	return (g_rpcMutexAux < 0) ? g_rpcMutexAux : 0;
}

static int __attribute__((noinline)) meSysEventOnPreSuspend(void)
{
	return 0;
}

static int __attribute__((noinline)) meSuspendLock(int mode)
{
	(void)mode;
	sceKernelLockMutex(g_rpcMutex, 1, NULL);
	sceKernelLockMutex(g_rpcMutexAux, 1, NULL);
	return 0;
}

static int __attribute__((noinline)) meSuspendUnlock(void)
{
	sceKernelUnlockMutex(g_rpcMutex, 1);
	sceKernelUnlockMutex(g_rpcMutexAux, 1);
	return 0;
}

int module_reboot_phase(int phase, void *argp)
{
	if (phase == 1) {
		sceMeWrapperEnd();
	}
	return 0;
}

static int __attribute__((noinline)) meSysEventOnSuspend(void)
{
	return 0;
}

static int __attribute__((noinline)) meSysEventOnResume(void)
{
	return 0;
}

static int __attribute__((noinline)) meSysEventOnPostResume(void)
{
	return 0;
}

static int __attribute__((noinline)) mePrepareBoot(void *addr, int size)
{
	(void)addr;
	__asm__ volatile("" : "+r"(size));
	return size;
}

static int __attribute__((noinline)) sceKermitMeInit(void)
{
	(void)g_kermitMeBufBase;
	(void)g_kermitMeBufBaseAux;
	(void)g_kermitMeWorkBuf;
	(void)g_kermitMeWorkSize;
	sceKernelMemset((void *)0xABD30000u, 0, 0x8000);
	g_kermitMeBufBase = (void *)0xABD30000u;
	sceKernelMemset((void *)0xABD40000u, 0, 0x8000);
	g_kermitMeWorkBuf = (void *)0xABD34000u;
	g_kermitMeBufBaseAux = (void *)0xABD40000u;
	g_kermitMeWorkSize = 0x4000;
	g_hasAvcTimestamp = 0;
	g_kermitMeMutex = sceKernelCreateMutex("SceKermitMe", SCE_KERNEL_SA_THPRI, 0, NULL);
	if (g_kermitMeMutex <= 0) {
		return 0x80020001;
	}
	g_kermitMeMutexAux = sceKernelCreateMutex("SceKermitMe2", SCE_KERNEL_SA_THPRI, 0, NULL);
	return (g_kermitMeMutex > 0) ? 0 : 0x80020001;
}

int sceMeCore_driver_9EB2943B(u32 tsPtsLo, u32 tsPtsHi, u32 tsDtsLo, u32 tsDtsHi)
{
	g_hasAvcTimestamp = 1;
	g_avcTimestamp[0] = tsPtsLo;
	g_avcTimestamp[1] = tsPtsHi;
	g_avcTimestamp[2] = tsDtsLo;
	g_avcTimestamp[3] = tsDtsHi;
	return 0;
}

static int __attribute__((noinline)) sceKermitMeBootStart(void)
{
	u8 stack_buf[0x80];
	SceKermitRequest *aligned = (SceKermitRequest *)(((u32)stack_buf + 0x3Fu) & ~0x3Fu);

	sceKernelDcacheInvalidateRange(aligned, 0x40);
	SceKermitRequest *req = KERMIT_UNCACHED(aligned);
	req->cmd = 0xC5E;
	u64 resp;
	int res = sceKermit_driver_36666181(req, KERMIT_MODE_ME, 0xC5E, 0, 0, &resp);
	return (res == 0) ? (int)resp : res;
}

int bootMeImage(void *data, int wait)
{
	sceSysregMeResetEnable();
	sceSysregMeBusClockEnable();
	u32 size = (u32)me_boot_code_end - (u32)me_boot_code;
	memcpy((void *)0xBFC00040u, (const void *)me_boot_code, size);
	HW(0xBFC00060u) |= (HW(0xBC100040u) & 3u);
	HW(0xBFC00064u) |= ((u32)data >> 16);
	HW(0xBFC00068u) |= ((u32)data & 0xFFFFu);
	sceKernelDcacheWritebackInvalidateAll();
	sceDdrFlush(4);
	if (wait) {
		HW(0xBFC0070Cu) = 0x01FF01FFu;
		HW(0xBFC00714u) = 2;
	}
	HW(0xBFC00700u) = (u32)-3;
	sceSysregVmeResetDisable();
	sceSysregMeResetDisable();
	if (wait) {
		while ((s32)HW(0xBFC00700u) == -3) {
			sceKernelDelayThread(1000);
		}
	}
	HW(0xBFC00700u) = 0;
	sceSysregAvcResetDisable();
	sceDdrFlush(8);
	return 0;
}

int sceMeBootStart(u32 mode)
{
	if ((mode - 1u) >= 4u) {
		return 0x80000102;
	}
	if ((int)sceSysregGetTachyonVersion() > 0x004FFFFF && mode != 2) {
		mode = 3;
	}
	if (mode == g_meStarted) {
		return 0;
	}
	int ret = meRpcLock();
	if (ret < 0) {
		return ret;
	}
	ret = meRpcLockAux();
	if (ret < 0) {
		meRpcUnlock();
		return ret;
	}
	if ((mode == 3 && g_meStarted == 2) || (mode == 2 && g_meStarted == 3)) {
		meRpcUnlock();
		g_meStarted = mode;
		meRpcUnlockAux();
		sceMeCore_driver_FA398D71(0x185, mode != 2);
		return 0;
	}
	const char *img_path;
	if ((int)sceSysregGetTachyonVersion() > 0x004FFFFF) {
		img_path = "flash0:/kd/resource/me_t2img.img";
	} else if (mode == 1) {
		img_path = "flash0:/kd/resource/me_sdimg.img";
	} else if (mode == 4) {
		img_path = "flash0:/kd/resource/me_blimg.img";
	} else {
		img_path = "flash0:/kd/resource/meimg.img";
	}
	SceUID fileDesc = sceIoOpen(img_path, 0x04000001, 0);
	if (fileDesc < 0) {
		meRpcUnlock();
		return fileDesc;
	}
	int size = (int)sceIoLseek(fileDesc, 0, 2);
	sceIoLseek(fileDesc, 0, 0);
	if (size > 0x63FFF) {
		sceIoClose(fileDesc);
		meRpcUnlock();
		return 0x80000022;
	}
	sceSysregMeResetEnable();
	void *addr = (void *)(0x883FF000u - ((u32)size & ~0x3Fu));
	int read = sceIoRead(fileDesc, addr, size);
	sceIoClose(fileDesc);
	if (read != size) {
		meRpcUnlock();
		return (read < 0) ? read : (int)0x80000022;
	}
	mePrepareBoot(addr, size);
	bootMeImage(addr, 1);
	meRpcUnlock();
	g_meStarted = mode;
	meRpcUnlockAux();
	sceMeCore_driver_FA398D71(0x185, mode != 2);
	return 0;
}

__attribute__((noreturn)) void decompressAndRunMeImage(void *data)
{
	void *end = data;
	int run = 1;
	if (((u32)data + 0x78000000u) <= 0x003FFFFFu) {
		HW(0xBFC00700u) = (u32)-4;
		run = 0;
		if (memcmp(data, "KL4E", 4) == 0) {
			int ret = UtilsForKernel_6C6887EE((void *)0x88300000u, (u32)data + 0x77D00000u, (const u8 *)data + 4, &end);
			if (ret >= 0) {
				sceKernelDcacheWritebackInvalidateAll();
				sceKernelIcacheInvalidateAll();
				run = 1;
			}
		}
	}
	if (run) {
		((void (*)(void *))0x88300000u)(end);
	}
	sceKernelMemset32(end, 0, 0x88400000u - (u32)end);
	HW(0xBFC00700u) = (u32)-5;
	HW(0xBFC00710u) = (u32)-2;
	for (;;) {
		__asm__ volatile(".word 0x70000000");
	}
}

int sceMeWrapperInit(SceSize args, void *argp)
{
	int res = sceKermitMeInit();
	if (res != 0) {
		Kprintf("Failed to init me %08x\n", res);
		return 1;
	}
	initRpc();
	res = sceKermitMeBootStart();
	if (res < 0) {
		Kprintf("Failed to sceKermitMeBootStart() : 0x%08x\n", res);
	}
	sceKernelRegisterSysEventHandler(&g_meRpcSysEvent);
	sceMePowerControlAvcPower(0);
	return 0;
}

int module_start(SceSize args, void *argp) __attribute__((alias("sceMeWrapperInit")));

int sceMeWrapperEnd(void)
{
	sceSysregVmeResetEnable();
	sceSysregAvcResetEnable();
	sceKernelUnregisterSysEventHandler(&g_meRpcSysEvent);
	return 0;
}

int sceMeCore_driver_4CD5B5BF(int id, int val)
{
	Kprintf("===== Me codec param - id:%x - val:%x =====\n", id, val);
	if (id == 1) {
		sceMeCore_driver_var_1B960AB4 = val;
	}
	return 0;
}

static int meSysEventHandler(int ev_id, char *ev_name, void *param, int *result)
{
	switch (ev_id) {
	case 0x402:
		return meCheckBusy();
	case 0x4005:
		return meSysEventOnPreSuspend();
	case 0x10005:
		return meSysEventOnSuspend();
	case 0x1000F:
		return meSysEventOnPostResume();
	case 0x100000:
		return meSysEventOnResume();
	case 0x1000002:
		meSuspendLock(((SceMeSysEventParam *)param)->mode);
		return 0;
	case 0x1000020:
		meSuspendUnlock();
		return 0;
	default:
		return 0;
	}
}
