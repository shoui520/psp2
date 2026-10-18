/*
 * SceVshBridge user-pointer exports (Sony's file name unknown): each copies
 * the caller's buffers into kernel memory, calls the bridge in vshbridge.c
 * and copies the results out. Buffers of variable size take their size from
 * an argument block the caller passes last; the larger requests run on a
 * separate kernel stack (SceThreadmgrForDriver_E54FD746).
 */
#include <psp2kern/types.h>
#include <vshbridge/vshbridge_imports.h>
#include "vshbridge.h"
#include "vshbridge_copy.h"
#include <alloca.h>

/* ===== RunWithStack bodies ===== */

typedef struct VshLegacyDocKeyArgs {
	void *out;				/* 0x10 bytes */
	SceSize keySize;			/* up to 0x200 */
	SceUInt32 reserved[2];
} __attribute__((aligned(8))) VshLegacyDocKeyArgs;

/*
 * The parameters of the requests that run on their own stack, as the
 * export passes them to its body.
 */
typedef struct NpDrmGetLegacyDocKeyParam {
	const void *a;
	const void *key;
	void *c;
	const VshLegacyDocKeyArgs *uargs;
} NpDrmGetLegacyDocKeyParam;

typedef struct NpDrmEbootSigConvertParam {
	const char *path;
	const void *oldSignature;
	void *newSignature;
	const SyscallUnusedArgs *uargs;
} NpDrmEbootSigConvertParam;

typedef struct AppMgrCloudDataCreateHeaderParam {
	const void *a;
	int b;
	const void *c;
	void *header;
} AppMgrCloudDataCreateHeaderParam;

typedef struct AppMgrAcInstGetAcdirParamParam {
	const void *a;
	const void *b;
	void *param;
} AppMgrAcInstGetAcdirParamParam;

static int npDrmGetLegacyDocKey(const void *a, const void *key, void *c, const VshLegacyDocKeyArgs *uargs)
{
	VshLegacyDocKeyArgs args;
	SceUInt8 *pa = NULL, *pkey = NULL, *out;
	int ret;

	ret = SceSysmemForDriver_BC996A7A(&args, uargs, sizeof(args));
	if (ret < 0)
		return ret;
	if (a != NULL) {
		pa = alloca(0x200);
		ret = SceSysmemForDriver_BC996A7A(pa, a, 0x200);
		if (ret < 0)
			return ret;
	}
	if (key != NULL) {
		if (args.keySize > 0x200)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		pkey = alloca(0x200);
		ret = SceSysmemForDriver_BC996A7A(pkey, key, args.keySize);
		if (ret < 0)
			return ret;
	}
	if (args.out == NULL)
		return vshNpDrmGetLegacyDocKey(pa, pkey, c, NULL);
	out = alloca(0x10);
	__builtin_memset(out, 0, 0x10);
	ret = vshNpDrmGetLegacyDocKey(pa, pkey, c, out);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(args.out, out, 0x10));
}

static int npDrmGetLegacyDocKeyOnStack(void *argp)
{
	const NpDrmGetLegacyDocKeyParam *param = argp;

	return npDrmGetLegacyDocKey(param->a, param->key, param->c, param->uargs);
}

static int npDrmEbootSigConvert(const char *path, const void *oldSignature, void *newSignature,
				const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *ppath = NULL;
	SceUInt8 *pold = NULL, *pnew;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(ppath, path, 0x100);
	BLOCK_IN(pold, oldSignature, 0x100);
	if (newSignature == NULL)
		return vshNpDrmEbootSigConvert(ppath, pold, NULL);
	pnew = alloca(0x200);
	__builtin_memset(pnew, 0, 0x200);
	ret = vshNpDrmEbootSigConvert(ppath, pold, pnew);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(newSignature, pnew, 0x200));
}

static int npDrmEbootSigConvertOnStack(void *argp)
{
	const NpDrmEbootSigConvertParam *param = argp;

	return npDrmEbootSigConvert(param->path, param->oldSignature, param->newSignature, param->uargs);
}

static int appMgrCloudDataCreateHeader(const void *a, int b, const void *c, void *header)
{
	SceUInt8 *pa = NULL, *pc = NULL, *kheader;
	int ret;

	if (a != NULL) {
		pa = alloca(0x18);
		ret = SceSysmemForDriver_BC996A7A(pa, a, 0x18);
		if (ret < 0)
			return ret;
	}
	if (c != NULL) {
		pc = alloca(0x20);
		ret = SceSysmemForDriver_BC996A7A(pc, c, 0x20);
		if (ret < 0)
			return ret;
	}
	if (header == NULL)
		return vshAppMgrCloudDataCreateHeader(pa, b, pc, NULL);
	kheader = alloca(0x170);
	__builtin_memset(kheader, 0, 0x170);
	ret = vshAppMgrCloudDataCreateHeader(pa, b, pc, kheader);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(header, kheader, 0x170));
}

static int appMgrCloudDataCreateHeaderOnStack(void *argp)
{
	const AppMgrCloudDataCreateHeaderParam *param = argp;

	return appMgrCloudDataCreateHeader(param->a, param->b, param->c, param->header);
}

static int appMgrCloudDataVerifyHeader(const void *header)
{
	SceUInt8 *p = NULL;
	int ret;

	if (header != NULL) {
		p = alloca(0x170);
		ret = SceSysmemForDriver_BC996A7A(p, header, 0x170);
		if (ret < 0)
			return ret;
	}
	return vshAppMgrCloudDataVerifyHeader(p);
}

static int appMgrCloudDataVerifyHeaderOnStack(void *argp)
{
	return appMgrCloudDataVerifyHeader(*(const void **)argp);
}

static int appMgrAcInstGetAcdirParam(const void *a, const void *b, void *param)
{
	SceUInt8 *pa = NULL, *pb = NULL, *kparam;
	int ret;

	if (a != NULL) {
		pa = alloca(0x10);
		ret = SceSysmemForDriver_BC996A7A(pa, a, 0x10);
		if (ret < 0)
			return ret;
	}
	if (b != NULL) {
		pb = alloca(0x14);
		ret = SceSysmemForDriver_BC996A7A(pb, b, 0x14);
		if (ret < 0)
			return ret;
	}
	if (param == NULL)
		return vshAppMgrAcInstGetAcdirParam(pa, pb, NULL);
	kparam = alloca(0xe0);
	__builtin_memset(kparam, 0, 0xe0);
	ret = vshAppMgrAcInstGetAcdirParam(pa, pb, kparam);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(param, kparam, 0xe0));
}

static int appMgrAcInstGetAcdirParamOnStack(void *argp)
{
	const AppMgrAcInstGetAcdirParamParam *param = argp;

	return appMgrAcInstGetAcdirParam(param->a, param->b, param->param);
}

static int appMgrLocalBackupVerifyOfflineHeader(const void *header)
{
	SceUInt8 *p = NULL;
	int ret;

	if (header != NULL) {
		p = alloca(0x170);
		ret = SceSysmemForDriver_BC996A7A(p, header, 0x170);
		if (ret < 0)
			return ret;
	}
	return vshAppMgrLocalBackupVerifyOfflineHeader(p);
}

static int appMgrLocalBackupVerifyOfflineHeaderOnStack(void *argp)
{
	return appMgrLocalBackupVerifyOfflineHeader(*(const void **)argp);
}

/* ===== the exports ===== */

int _vshKernelShutdownSystem(const SceKernelShutdownParam *param, const SyscallSizeArgs *uargs)
{
	SceKernelShutdownParam *p = NULL;
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(p, param, args.size, sizeof(*p));
	return vshKernelShutdownSystemForUser(p);
}

int _vshRtcSetCurrentTick(const SceRtcTick *tick)
{
	SceRtcTick *p = NULL;
	int ret;

	BLOCK_IN(p, tick, sizeof(*p));
	return vshRtcSetCurrentTick(p);
}

int _vshRtcSetCurrentSecureTick(const SceRtcTick *tick)
{
	SceRtcTick *p = NULL;
	int ret;

	BLOCK_IN(p, tick, sizeof(*p));
	return vshRtcSetCurrentSecureTick(p);
}

int _vshRtcGetCurrentSecureTick(SceRtcTick *tick)
{
	int ret;

	BLOCK_OUT(tick, sizeof(*tick), vshRtcGetCurrentSecureTick(NULL),
		  vshRtcGetCurrentSecureTick((SceRtcTick *)k));
}

int _vshRtcSetCurrentNetworkTick(const SceRtcTick *tick)
{
	SceRtcTick *p = NULL;
	int ret;

	BLOCK_IN(p, tick, sizeof(*p));
	return vshRtcSetCurrentNetworkTick(p);
}

int _vshRtcSetCurrentDebugNetworkTick(const SceRtcTick *tick)
{
	SceRtcTick *p = NULL;
	int ret;

	BLOCK_IN(p, tick, sizeof(*p));
	return vshRtcSetCurrentDebugNetworkTick(p);
}

int _vshRtcSetAlarmTick(const SceRtcTick *tick, int unk)
{
	SceRtcTick *p = NULL;
	int ret;

	BLOCK_IN(p, tick, sizeof(*p));
	return vshRtcSetAlarmTick(p, unk);
}

int _vshRtcGetAlarmTick(SceRtcTick *tick, int *unk)
{
	SceRtcTick *ktick = NULL;
	int *kunk = NULL;
	int ret, ret2;

	if (tick != NULL) {
		ktick = alloca(sizeof(*ktick));
		ktick->tick = 0;
	}
	if (unk != NULL) {
		kunk = alloca(sizeof(*kunk));
		*kunk = 0;
	}
	ret = vshRtcGetAlarmTick(ktick, kunk);
	if (tick != NULL) {
		ret2 = SceSysmemForDriver_6D88EF8A(tick, ktick, sizeof(*ktick));
		if (ret2 < 0)
			return ret2;
	}
	if (unk == NULL)
		return ret;
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(unk, kunk, sizeof(*kunk)));
}

int _vshRtcSetSecureAlarmTick(const SceRtcTick *tick)
{
	SceRtcTick *p = NULL;
	int ret;

	BLOCK_IN(p, tick, sizeof(*p));
	return vshRtcSetSecureAlarmTick(p);
}

int _vshRtcGetSecureAlarmTick(SceRtcTick *tick)
{
	int ret;

	BLOCK_OUT(tick, sizeof(*tick), vshRtcGetSecureAlarmTick(NULL),
		  vshRtcGetSecureAlarmTick((SceRtcTick *)k));
}

int _vshDisplaySetViewportConf(int head, const void *unused, const void *conf, const SyscallSizeArgs *uargs)
{
	SceUInt8 *p = NULL;
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(p, conf, args.size, 0x14);
	return vshDisplaySetViewportConf(head, unused, (int)p);
}

/* the conf is read in first: its size field passes through */
int _vshDisplayGetActualViewportConf(int head, void *unused, void *conf, const SyscallSizeArgs *uargs)
{
	SceUInt8 *kconf;
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	if (conf == NULL)
		return vshDisplayGetActualViewportConf(head, unused, 0);
	if (args.size > 0x14)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	kconf = alloca(0x14);
	__builtin_memset(kconf, 0, 0x14);
	ret = SceSysmemForDriver_BC996A7A(kconf, conf, args.size);
	if (ret < 0)
		return ret;
	ret = vshDisplayGetActualViewportConf(head, unused, (int)kconf);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(conf, kconf, args.size));
}

int _vshTouchSetTouchEmulationData(const void *front, const void *back)
{
	SceUInt8 *pfront = NULL, *pback = NULL;
	int ret;

	BLOCK_IN(pfront, front, 0x90);
	BLOCK_IN(pback, back, 0x90);
	return vshTouchSetTouchEmulationData(pfront, pback);
}

int _vshLedSetMode(int led, int mode, const void *conf, const SyscallSizeArgs *uargs)
{
	SceUInt8 *p = NULL;
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(p, conf, args.size, 8);
	return vshLedSetMode(led, mode, p);
}

int _vshPowerSetBatteryFakeStatus(const void *status, const SyscallSizeArgs *uargs)
{
	SceUInt8 *p = NULL;
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(p, status, args.size, 0x10);
	return vshPowerSetBatteryFakeStatus(p);
}

int _vshSysconGetManufacturesStatus(int *status)
{
	int ret;

	BLOCK_OUT(status, sizeof(*status), vshSysconGetManufacturesStatus(NULL),
		  vshSysconGetManufacturesStatus((int *)k));
}

int _vshSysconGetManualChargeMode(int *mode)
{
	int ret;

	BLOCK_OUT(mode, sizeof(*mode), vshSysconGetManualChargeMode(NULL), vshSysconGetManualChargeMode((int *)k));
}

int _vshSysconCtrlManualChargeMode(int mode)
{
	return vshSysconCtrlManualChargeMode(mode);
}

int _vshSysconLogReadData(int a, void *buf, SceSize size, const SyscallSizeArgs *uargs)
{
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_OUT_SIZED(buf, args.size, 0x20, vshSysconLogReadData(a, NULL, size), vshSysconLogReadData(a, k, size));
}

int _vshSysconGetTemperatureLog(SceUInt8 *log)
{
	int ret;

	BLOCK_OUT(log, 0x10, vshSysconGetTemperatureLog(NULL), vshSysconGetTemperatureLog(k));
}

int _vshSysconGetUsbDetStatus(int *status)
{
	int ret;

	BLOCK_OUT(status, sizeof(*status), vshSysconGetUsbDetStatus(NULL), vshSysconGetUsbDetStatus((int *)k));
}

int _vshSysconGetHardwareInfo(SceUInt32 *info)
{
	int ret;

	BLOCK_OUT(info, sizeof(*info), vshSysconGetHardwareInfo(NULL), vshSysconGetHardwareInfo((SceUInt32 *)k));
}

int _vshSysconGetHardwareInfo2(void *info, const SyscallSizeArgs *uargs)
{
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_OUT_SIZED(info, args.size, 0x10, vshSysconGetHardwareInfo2(NULL), vshSysconGetHardwareInfo2(k));
}

int _vshIdStorageCreateAtomicLeaves(const SceUInt16 *leaves, int num)
{
	return vshIdStorageCreateAtomicLeaves(leaves, num);
}

/* SceIofilemgrForDriver_D070BC48's data and its 64-bit reserved word, in two halves */
typedef struct VshIoMountArgs {
	void *data;
	SceUInt32 reservedLow;
	SceUInt32 reservedHigh;
	SceUInt32 reserved[3];
} __attribute__((aligned(8))) VshIoMountArgs;

int _vshIoMount(int id, const char *path, int permission, const VshIoMountArgs *uargs)
{
	VshIoMountArgs args;
	char *p = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, path, 0x400);
	return vshIoMount(id, p, permission, args.data, args.reservedLow, args.reservedHigh);
}

int _vshIoGetstat(const char *path, void *stat, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *p = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, path, 0x400);
	BLOCK_OUT(stat, 0x58, vshIoGetstat(p, NULL), vshIoGetstat(p, k));
}

int _vshIoChstat(const char *path, const void *stat, int bits, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *p = NULL;
	SceUInt8 *kstat = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, path, 0x400);
	BLOCK_IN(kstat, stat, 0x58);
	return vshIoChstat(p, kstat, bits);
}

/* the dirent is read in first: its private data passes through */
int _vshIoDread(SceUID fd, void *dirent)
{
	SceUInt8 *kdirent;
	int ret;

	if (dirent == NULL)
		return vshIoDread(fd, NULL);
	kdirent = alloca(0x160);
	__builtin_memset(kdirent, 0, 0x160);
	ret = SceSysmemForDriver_BC996A7A(kdirent, dirent, 0x160);
	if (ret < 0)
		return ret;
	ret = vshIoDread(fd, kdirent);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(dirent, kdirent, 0x160));
}

int _vshSblAimgrGetConsoleId(void *id)
{
	int ret;

	BLOCK_OUT(id, 0x10, vshSblAimgrGetConsoleId(NULL), vshSblAimgrGetConsoleId(k));
}

int _vshSblAimgrGetPscode(void *code)
{
	int ret;

	BLOCK_OUT(code, 8, vshSblAimgrGetPscode(NULL), vshSblAimgrGetPscode(k));
}

int _vshSblAimgrGetPscode2(void *code)
{
	int ret;

	BLOCK_OUT(code, 8, vshSblAimgrGetPscode2(NULL), vshSblAimgrGetPscode2(k));
}

int _vshSblAimgrGetVisibleId(void *id)
{
	int ret;

	BLOCK_OUT(id, 0x20, vshSblAimgrGetVisibleId(NULL), vshSblAimgrGetVisibleId(k));
}

int _vshSblSsGetNvsData(SceUInt32 offset, void *buf, SceSize size, const SyscallSizeArgs *uargs)
{
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_OUT_SIZED(buf, args.size, 0x20, vshSblSsGetNvsData(offset, NULL, size),
			vshSblSsGetNvsData(offset, k, size));
}

int _vshSblSsSetNvsData(SceUInt32 offset, const void *buf, SceSize size, const SyscallSizeArgs *uargs)
{
	SceUInt8 *p = NULL;
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(p, buf, args.size, 0x20);
	return vshSblSsSetNvsData(offset, p, size);
}

int _vshSblSsEncryptWithPortability(SceUInt32 mode, const void *iv, const void *in, void *out)
{
	SceUInt8 *piv = NULL, *pin = NULL;
	int ret;

	BLOCK_IN(piv, iv, 0x10);
	BLOCK_IN(pin, in, 0x24);
	BLOCK_OUT(out, 0x24, vshSblSsEncryptWithPortability(mode, piv, (const SceSize *)pin, NULL),
		  vshSblSsEncryptWithPortability(mode, piv, (const SceSize *)pin, (SceSize *)k));
}

int _vshSblSsDecryptWithPortability(SceUInt32 mode, const void *iv, const void *in, void *out)
{
	SceUInt8 *piv = NULL, *pin = NULL;
	int ret;

	BLOCK_IN(piv, iv, 0x10);
	BLOCK_IN(pin, in, 0x24);
	BLOCK_OUT(out, 0x24, vshSblSsDecryptWithPortability(mode, piv, (const SceSize *)pin, NULL),
		  vshSblSsDecryptWithPortability(mode, piv, (const SceSize *)pin, (SceSize *)k));
}

int _vshSblGcAuthMgrMlnpsnlAuth1(const void *a, const void *b, void *out, const VshMlnpsnlAuth1Args *uargs)
{
	MLNPSNL_AUTH1(vshSblGcAuthMgrMlnpsnlAuth1);
}

int _vshSblGcAuthMgrMlnpsnlAuth2(const void *a, const void *b, void *out, const VshMlnpsnlAuth2Args *uargs)
{
	MLNPSNL_AUTH2(vshSblGcAuthMgrMlnpsnlAuth2);
}

int _vshNpDrmGetLegacyDocKey(const void *a, const void *key, void *c, const void *uargs)
{
	NpDrmGetLegacyDocKeyParam param = { a, key, c, uargs };

	return SceThreadmgrForDriver_E54FD746(VSH_STACK_SIZE, npDrmGetLegacyDocKeyOnStack, &param);
}

/* the argument block of the EBOOT signature generators */
typedef struct VshEbootSigArgs {
	void *d;				/* passed on */
	SceUInt32 reserved[3];
} __attribute__((aligned(8))) VshEbootSigArgs;

/* EBOOT signatures: a path, a block in, 0x200 bytes out */
#define EBOOT_SIG_GEN(_impl, _bsize) do {				\
	VshEbootSigArgs args;						\
	char *ppath = NULL;						\
	SceUInt8 *pb = NULL;						\
	int ret;							\
									\
	ARGS_IN(args, uargs);						\
	STRING_IN(ppath, path, 0x100);					\
	BLOCK_IN(pb, b, (_bsize));					\
	BLOCK_OUT(sig, 0x200, _impl(ppath, pb, NULL, args.d),		\
		  _impl(ppath, pb, k, args.d));				\
} while (0)

int _vshNpDrmEbootSigGenPsp(const char *path, const void *b, void *sig, const VshEbootSigArgs *uargs)
{
	EBOOT_SIG_GEN(vshNpDrmEbootSigGenPsp, 0x20);
}

int _vshNpDrmEbootSigGenPs1(const char *path, const void *b, void *sig, const VshEbootSigArgs *uargs)
{
	EBOOT_SIG_GEN(vshNpDrmEbootSigGenPs1, 0x20);
}

int _vshNpDrmEbootSigGenMultiDisc(const char *path, const void *b, void *sig, const VshEbootSigArgs *uargs)
{
	EBOOT_SIG_GEN(vshNpDrmEbootSigGenMultiDisc, 0x100);
}

int _vshNpDrmEbootSigConvert(const char *path, const void *oldSignature, void *newSignature,
			     const SyscallUnusedArgs *uargs)
{
	NpDrmEbootSigConvertParam param = { path, oldSignature, newSignature, uargs };

	return SceThreadmgrForDriver_E54FD746(VSH_STACK_SIZE, npDrmEbootSigConvertOnStack, &param);
}

int _vshNpDrmEbootSigVerify(const char *path, const void *b, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *ppath = NULL;
	SceUInt8 *pb = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(ppath, path, 0x100);
	BLOCK_IN(pb, b, 0x200);
	return vshNpDrmEbootSigVerify(ppath, pb);
}

int _vshNpDrmPspEbootSigGen(const char *path, const void *b, void *sig, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *ppath = NULL;
	SceUInt8 *pb = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(ppath, path, 0x100);
	BLOCK_IN(pb, b, 0x20);
	BLOCK_OUT(sig, 0x100, vshNpDrmPspEbootSigGen(ppath, pb, NULL), vshNpDrmPspEbootSigGen(ppath, pb, k));
}

int _vshNpDrmPspEbootVerify(const char *path, const void *b, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *ppath = NULL;
	SceUInt8 *pb = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(ppath, path, 0x100);
	BLOCK_IN(pb, b, 0x100);
	return vshNpDrmPspEbootVerify(ppath, pb);
}

SceUID _vshKernelSearchModuleByName(const char *name, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *p = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, name, 0x20);
	return vshKernelSearchModuleByName(p);
}

int _vshSblAuthMgrVerifySpsfo(const char *path, void *buf, SceSize size, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *p = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, path, 0x100);
	return vshSblAuthMgrVerifySpsfo(p, buf, size);
}

int _vshSDfCtlSetBootParameter(const char *param, int unused, int value, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *p = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, param, 0x80);
	return vshSDfCtlSetBootParameter(p, unused, value);
}

int _vshKernelGetCompiledSdkVersionByPid(SceUID pid, SceUInt32 *version)
{
	return vshKernelGetCompiledSdkVersionByPid(pid, version);
}

int _vshAppMgrSystemParamDateTimeSetConf(const void *conf)
{
	SceUInt8 *p = NULL;
	int ret;

	BLOCK_IN(p, conf, 0x14);
	return vshAppMgrSystemParamDateTimeSetConf(p);
}

int _vshAppMgrBgdlSetQueueStatus(const void *status)
{
	SceUInt8 *p = NULL;
	int ret;

	BLOCK_IN(p, status, 0x700);
	return vshAppMgrBgdlSetQueueStatus(p);
}

int _vshAppMgrUpdateRifInfo(const void *info)
{
	SceUInt8 *p = NULL;
	int ret;

	BLOCK_IN(p, info, 0x10);
	return vshAppMgrUpdateRifInfo(p);
}

int _vshAppMgrRegisterPath(const char *path, void *a, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *p = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, path, 0x40);
	return vshAppMgrRegisterPath(p, a);
}

int _vshAppMgrCheckPfsMounted(const char *path, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *p = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, path, 0x124);
	return vshAppMgrCheckPfsMounted(p);
}

int _vshAppMgrCloudDataCreateHeader(const void *a, int b, const void *c, void *header)
{
	AppMgrCloudDataCreateHeaderParam param = { a, b, c, header };

	return SceThreadmgrForDriver_E54FD746(VSH_STACK_SIZE, appMgrCloudDataCreateHeaderOnStack, &param);
}

int _vshAppMgrCloudDataSrcMount(int a, const void *b, void *mount)
{
	SceUInt8 *pb = NULL;
	int ret;

	BLOCK_IN(pb, b, 0x10);
	BLOCK_OUT(mount, 0x10, vshAppMgrCloudDataSrcMount(a, pb, NULL), vshAppMgrCloudDataSrcMount(a, pb, k));
}

int _vshAppMgrCloudDataDstCreateMount(const void *a, void *mount)
{
	SceUInt8 *pa = NULL;
	int ret;

	BLOCK_IN(pa, a, 0x20);
	BLOCK_OUT(mount, 0x10, vshAppMgrCloudDataDstCreateMount(pa, NULL), vshAppMgrCloudDataDstCreateMount(pa, k));
}

int _vshAppMgrCloudDataVerifyHeader(const void *header)
{
	const void *arg = header;

	return SceThreadmgrForDriver_E54FD746(VSH_STACK_SIZE, appMgrCloudDataVerifyHeaderOnStack, &arg);
}

int _vshAppMgrCloudDataSetupKey(void *key)
{
	int ret;

	BLOCK_OUT(key, 0x18, vshAppMgrCloudDataSetupKey(NULL), vshAppMgrCloudDataSetupKey(k));
}

int _vshAppMgrCloudDataGetMcId(int a, void *id)
{
	int ret;

	BLOCK_OUT(id, 8, vshAppMgrCloudDataGetMcId(a, NULL), vshAppMgrCloudDataGetMcId(a, k));
}

int _vshAppMgrCloudDataSetMcId(int a, const void *id)
{
	SceUInt8 *p = NULL;
	int ret;

	BLOCK_IN(p, id, 8);
	return vshAppMgrCloudDataSetMcId(a, p);
}

int _vshAppMgrFakeSaveDataCreateMount(const char *path, void *mount, const SyscallUnusedArgs *uargs)
{
	SyscallUnusedArgs args;
	char *p = NULL;
	int ret;

	ARGS_IN(args, uargs);
	STRING_IN(p, path, 0x124);
	BLOCK_OUT(mount, 0x10, vshAppMgrFakeSaveDataCreateMount(p, NULL), vshAppMgrFakeSaveDataCreateMount(p, k));
}

int _vshAppMgrIsExclusiveProcessRunning(const void *a)
{
	SceUInt8 *p = NULL;
	int ret;

	BLOCK_IN(p, a, 0x10);
	return vshAppMgrIsExclusiveProcessRunning(p);
}

int _vshAppMgrAcInstGetAcdirParam(const void *a, const void *b, void *param)
{
	AppMgrAcInstGetAcdirParamParam stackParam = { a, b, param };

	return SceThreadmgrForDriver_E54FD746(VSH_STACK_SIZE, appMgrAcInstGetAcdirParamOnStack, &stackParam);
}

int _vshAppMgrSaveDataLocalBackupTargetGetList(void *list)
{
	int ret;

	BLOCK_OUT(list, 0xc0, vshAppMgrSaveDataLocalBackupTargetGetList(NULL),
		  vshAppMgrSaveDataLocalBackupTargetGetList(k));
}

int _vshAppMgrSaveDataLocalBackupTargetRemoveItem(const void *item)
{
	SceUInt8 *p = NULL;
	int ret;

	BLOCK_IN(p, item, 0x10);
	return vshAppMgrSaveDataLocalBackupTargetRemoveItem(p);
}

int _vshAppMgrLocalBackupGetOfflineId(void *id, void *b)
{
	SceUInt64 *kid = NULL;
	SceUInt32 *kb = NULL;
	int ret, ret2;

	if (id != NULL) {
		kid = alloca(sizeof(*kid));
		*kid = 0;
	}
	if (b != NULL) {
		kb = alloca(sizeof(*kb));
		*kb = 0;
	}
	ret = vshAppMgrLocalBackupGetOfflineId(kid, kb);
	if (id != NULL) {
		ret2 = SceSysmemForDriver_6D88EF8A(id, kid, sizeof(*kid));
		if (ret2 < 0)
			return ret2;
	}
	if (b == NULL)
		return ret;
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(b, kb, sizeof(*kb)));
}

int _vshAppMgrLocalBackupVerifyOfflineHeader(const void *header)
{
	const void *arg = header;

	return SceThreadmgrForDriver_E54FD746(VSH_STACK_SIZE, appMgrLocalBackupVerifyOfflineHeaderOnStack, &arg);
}

int _vshAppMgrSaveDataNotifyBackupFinished(const void *a)
{
	SceUInt8 *p = NULL;
	int ret;

	BLOCK_IN(p, a, 0x10);
	return vshAppMgrSaveDataNotifyBackupFinished(p);
}

int _vshCoredumpCafHeaderTransform(SceUID ctx, const void *data, SceSize size,
	const SyscallSizeArgs *uargs)
{
	SceUInt8 *p = NULL;
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(p, data, args.size, 0x400);
	return vshCoredumpCafHeaderTransform(ctx, p, size);
}

int _vshCoredumpCafHeaderFinal(SceUID ctx, void *buf, SceSize size,
	const SyscallSizeArgs *uargs)
{
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_OUT_SIZED(buf, args.size, 0x20, vshCoredumpCafHeaderFinal(ctx, NULL, size),
			vshCoredumpCafHeaderFinal(ctx, k, size));
}

int _vshCoredumpCafCreateIv(void *iv, SceSize size, const SyscallSizeArgs *uargs)
{
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_OUT_SIZED(iv, args.size, 0x10, vshCoredumpCafCreateIv(NULL, size),
			vshCoredumpCafCreateIv(k, size));
}

typedef struct VshCafSegmentInitArgs {
	void *d;
	const void *e;				/* up to 0x10 bytes */
	void *f;
	SceSize eSize;
	SceUInt32 reserved[2];
} __attribute__((aligned(8))) VshCafSegmentInitArgs;

int _vshCoredumpCafSegmentInit(SceUID ctx, void *b, void *c, const VshCafSegmentInitArgs *uargs)
{
	VshCafSegmentInitArgs args;
	SceUInt8 *ke = NULL;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(ke, args.e, args.eSize, 0x10);
	return vshCoredumpCafSegmentInit(ctx, b, c, args.d, ke, args.f);
}

typedef struct VshCafSegmentTransformArgs {
	void *d;
	SceSize inSize;				/* up to 0x400 */
	SceSize outSize;			/* up to 0x400 */
	SceUInt32 reserved;
} __attribute__((aligned(8))) VshCafSegmentTransformArgs;

int _vshCoredumpCafSegmentTransform(SceUID ctx, const void *in, void *out,
				    const VshCafSegmentTransformArgs *uargs)
{
	VshCafSegmentTransformArgs args;
	SceUInt8 *pin = NULL;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(pin, in, args.inSize, 0x400);
	BLOCK_OUT_SIZED(out, args.outSize, 0x400, vshCoredumpCafSegmentTransform(ctx, pin, NULL, args.d),
			vshCoredumpCafSegmentTransform(ctx, pin, k, args.d));
}

int _vshCoredumpCafSegmentFinal(SceUID ctx, void *buf, void *c, const SyscallSizeArgs *uargs)
{
	SyscallSizeArgs args;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_OUT_SIZED(buf, args.size, 0x20, vshCoredumpCafSegmentFinal(ctx, NULL, c),
			vshCoredumpCafSegmentFinal(ctx, k, c));
}

typedef struct VshCreateDumpArgs {
	const void *in;				/* up to 0x200 bytes */
	int e;
	int f;
	void *out;				/* up to 0x200 bytes */
	int h;
	SceSize bSize;				/* up to 0x10 */
	SceSize inSize;
	SceSize outSize;
	SceUInt32 reserved[2];
} __attribute__((aligned(8))) VshCreateDumpArgs;

int _vshCoredumpCreateDump(int a, const void *b, int c, const VshCreateDumpArgs *uargs)
{
	VshCreateDumpArgs args;
	SceUInt8 *pb = NULL, *kin = NULL;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(pb, b, args.bSize, 0x10);
	BLOCK_IN_SIZED(kin, args.in, args.inSize, 0x200);
	BLOCK_OUT_SIZED(args.out, args.outSize, 0x200,
			vshCoredumpCreateDump(a, pb, c, kin, args.e, args.f, NULL, args.h),
			vshCoredumpCreateDump(a, pb, c, kin, args.e, args.f, k, args.h));
}

typedef struct VshEventLogArgs {
	const void *pBuf;
	SceSize bufSize;			/* passed on */
	SceSize copySize;			/* of pBuf, up to 0x7c0 */
	SceUInt32 reserved;
} __attribute__((aligned(8))) VshEventLogArgs;

int _vshEventLogPut(SceUInt16 type, SceUInt16 subtype, SceUInt32 arg, const VshEventLogArgs *uargs)
{
	VshEventLogArgs args;
	SceUInt8 *kbuf = NULL;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN_SIZED(kbuf, args.pBuf, args.copySize, 0x7c0);
	return vshEventLogPut(type, subtype, arg, kbuf, args.bufSize);
}

/* in and out: the bridge checks the record's size */
int _vshSblGetSystemSwVersion(SceKernelSystemSwVersion *info)
{
	SceKernelSystemSwVersion *kinfo;
	int ret;

	if (info == NULL)
		return vshSblGetSystemSwVersion(NULL);
	kinfo = alloca(sizeof(*kinfo));
	__builtin_memset(kinfo, 0, sizeof(*kinfo));
	ret = SceSysmemForDriver_BC996A7A(kinfo, info, sizeof(*kinfo));
	if (ret < 0)
		return ret;
	ret = vshSblGetSystemSwVersion(kinfo);
	return copyResult(ret, SceSysmemForDriver_6D88EF8A(info, kinfo, sizeof(*kinfo)));
}

int _vshSblAimgrGetSMI(void *smi)
{
	int ret;

	BLOCK_OUT(smi, 4, vshSblAimgrGetSMI(NULL), vshSblAimgrGetSMI(k));
}

int _vshSblSsGenerateAppKey(const void *a, void *key)
{
	SceUInt8 *pa = NULL;
	int ret;

	BLOCK_IN(pa, a, 0x24);
	BLOCK_OUT(key, 0x24, vshSblSsGenerateAppKey(pa, NULL), vshSblSsGenerateAppKey(pa, k));
}

int _vshSblSsCreatePassPhrase(const void *a, void *phrase, const SyscallSizeArgs *uargs)
{
	SyscallSizeArgs args;
	SceUInt8 *pa = NULL;
	int ret;

	ARGS_IN(args, uargs);
	BLOCK_IN(pa, a, 0x18);
	BLOCK_OUT_SIZED(phrase, args.size, 0x200, vshSblSsCreatePassPhrase(pa, NULL),
			vshSblSsCreatePassPhrase(pa, k));
}
