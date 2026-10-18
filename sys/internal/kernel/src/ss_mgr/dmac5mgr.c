#include "dmac5mgr.h"
#include "ssmgr_internal.h"
#include <sysmem/debug.h>
#include <intrmgr/syscall.h>
#include <ss_mgr/ss_mgr.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/sysclib.h>
#include <dmacmgr/dmac_regs.h>
#include <sysmem/sysroot.h>

SceDmac5KeyringRegisters *dmacKeyringRegisters;
SceKernelFastMutex dmacKeyringMutex;
SceSblDmac5KeyRing dmacKeyringStorage[32];

int dmac5SetExternalKey(SceKernelDmaOpDirectParam *param,
	const void *key, uint32_t command);
int dmac5SetKeyRingIndex(SceKernelDmaOpDirectParam *param,
	unsigned int keyRingIndex);
int dmac5LoadIv(Dmac5Context *context, const void *iv);
int dmac5StoreIv(Dmac5Context *context, void *iv);
int decKeyRingKeyUseCount(unsigned int keyRingIndex);
int dmac5ReleaseNpKey(unsigned int keyRingIndex);
int dmac5LockUserBuffers(const void *src, SceSize srcSize, void *dst, SceSize dstSize);
int dmac5UnlockUserBuffers(const void *src, SceSize srcSize, void *dst, SceSize dstSize);

/* Name unknown: initialize a context for hardware keyring slots 28 through 31. */
int __attribute__((noinline, noclone)) dmac5InitReservedKeyContext(Dmac5Context *context, uint32_t command,
	unsigned int keyBits, int coherent, unsigned int key)
{
	uint32_t keyRingIndex;
	int ret;
	SceKernelDmaOpId operation;

	ret = dmac5ValidateReservedKey(key, &keyRingIndex);
	if (ret != 0)
		return ret;
	ret = dmac5SetKeySize(&command, keyBits);
	if (ret != 0)
		return ret;
	memset(context, 0, sizeof(*context));
	if (coherent)
		context->dataCoherency = UINT32_MAX >> 14;
	else
		context->dataCoherency = coherent;
	context->ivCoherency = 0x1ff01ff;
	context->command = command;
	operation = sceKernelDmaOpAlloc("SceSblDmac5Common");
	if (operation < 0)
		return (int)0x800f1528;
	context->operation = operation;
	context->keyRingIndex = keyRingIndex;
	return 0;
}

/* Sony inlines this address classification at each coherency check. */
static inline __attribute__((always_inline)) int dmac5IsUncached(const void *address)
{
	uintptr_t value = (uintptr_t)address;

	if (value - 0x18000000 < 0x08000000)
		return sceKernelIsUncacheAddressInTmpFsGame((void *)address);
	return value - 0x20000000 < 0x20000000 ||
		value - 0x60000000 < 0x21000000;
}

int _sceSblDmac5DirectCrypt(SceKernelDmaOpId operation, const void *src,
	void *dst, void *iv, SceSize size, unsigned int keyRingIndex,
	uint32_t command, uint32_t dataCoherency, uint32_t ivCoherency,
	const void *key)
{
	SceKernelDmaOpDirectParam param;
	uintptr_t ivPhysical = 0;
	unsigned int kind = command & 7;
	int mode = command & 0x38;
	int ret;
#if FW_DEBUG
	const SceKernelDebugInfo *errorInfo;
	const char *errorMessage;
#endif

	param.src = src;
	param.dst = dst;
	if (kind == 3) {
		uintptr_t dstPhysical = 0;

		if (!(command & 0x400)) {
			ret = SceSysmemForDriver_8D160E65(dst, &dstPhysical);
			if (ret < 0) {
				SCE_DBG_PRINTF(0xb, 490, "dst addr conversion failed\n");
				return (int)0x800f1528;
			}
		}
		param.dst = (void *)dstPhysical;
		param.len = size | 0x10000000;
		param.cmd = 0x3000;
		goto convertIv;
	}
	param.len = size;
	param.cmd = 0x3000;
	if (kind != 4 && ((kind != 1) | (mode != 0)) &&
		((kind != 2) | (mode != 0))) {
convertIv:
		if (iv == NULL)
			return (int)0x800f1516;
		ret = SceSysmemForDriver_8D160E65(iv, &ivPhysical);
		if (ret < 0)
			return (int)0x800f1528;
	}
	param.encDec.iv = (void *)ivPhysical;
	if (dataCoherency) {
		if (!dmac5IsUncached(src))
			command |= 0x01000000;
		if (!dmac5IsUncached(dst))
			command |= 0x02000000;
	}
	param.coherencyMask = dataCoherency;
	if (ivCoherency && !dmac5IsUncached(iv))
		command |= 0x0c000000;
	param.encDec.ivCoherencyMask = ivCoherency;
	SceKernelSuspendForDriver_4DF40893(0);
	if (((kind != 3) | ((mode & 0x29) != 0)) && kind != 4) {
		if (keyRingIndex > 255) {
			ret = dmac5SetExternalKey(&param, key, command);
			if (ret != 0)
				goto keyError;
			command |= 0x80;
		} else if (dmac5SetKeyRingIndex(&param, keyRingIndex) != 0) {
keyError:
			SceKernelSuspendForDriver_2BB92967(0);
			return (int)0x800f1528;
		}
	}
	param.cmd |= command;
	ret = sceKernelDmaOpAssign(operation, 19, 0);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 580,
			"   ERROR: assign dmac operation failed. 0x%08x\n", ret);
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	ret = _DmaOpSetupDirect(operation, &param,
		(command & 7) == 3 ? 1 : 17);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 604,
			"   ERROR: Setup dmac operation (Direct) failed. 0x%08x, for (0x%08x)\n",
			ret, operation);
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	ret = sceKernelDmaOpEnQueue(operation);
	if (ret < 0) {
#if FW_DEBUG
		errorInfo = SCE_DBGINFO(615);
		errorMessage = "   ERROR: Enqueue dmac operation failed. 0x%08x\n";
#endif
	} else {
		sceKernelSysrootDisableAutoClockDown();
		ret = sceKernelDmaOpSync(operation, 2, 0, 0);
		sceKernelSysrootEnableAutoClockDown();
#if FW_DEBUG
		if (ret < 0) {
			errorInfo = SCE_DBGINFO(629);
			errorMessage = "   ERROR: Sync dmac operation failed. 0x%08x\n";
		}
#endif
	}
#if FW_DEBUG
	if (ret < 0)
		SceDebugForDriver_D9703808(1, 0xb, errorInfo, errorMessage, ret);
#endif
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int sceSblDmac5Init(void)
{
	SceKernelAllocMemBlockKernelOpt option;
	void *base;
	SceUID block;
	int ret;

	memset(&option, 0, sizeof(option));
	option.size = sizeof(option);
	option.paddr = DMAC5_KEYRING_REG_BASE;
	option.attr = 2;
	block = SceSysmemForDriver_C94850C9("SceSblDMAC5DmacKRBase",
		(SceKernelMemBlockType)0x20100806, DMAC_REG_SIZE, &option);
	if (block < 0)
		SCE_DBG_ASSERT_FAIL(176);
	SceSysmemForDriver_A841EDDA(block, &base);
	dmacKeyringRegisters = base;
	ret = sceKernelInitializeFastMutex(&dmacKeyringMutex,
		"SceSblDmac5KeyRing", 0, NULL);
	if (ret != 0)
		return (int)0x800f1528;
	memset(dmacKeyringStorage, 0,
		sizeof(dmacKeyringStorage));
	return ret;
}

/* Name unknown: run one transfer, preserving caller-visible IV state. */
int __attribute__((noinline, noclone)) dmac5Crypt(Dmac5Context *context, const void *src, void *dst, void *iv,
	SceSize size, uint32_t command, const void *key)
{
	int ret;

	if (iv != NULL) {
		if (dmac5LoadIv(context, iv) != 0)
			goto error;
		ret = _sceSblDmac5DirectCrypt(context->operation, src, dst,
			context->state, size, context->keyRingIndex, command,
			context->dataCoherency, context->ivCoherency, key);
		if (__builtin_expect(ret != 0, 0))
			return ret;
		if (dmac5StoreIv(context, iv) != 0)
			goto error;
	} else {
		ret = _sceSblDmac5DirectCrypt(context->operation, src, dst,
			context->state, size, context->keyRingIndex, command,
			context->dataCoherency, context->ivCoherency, key);
		if (__builtin_expect(ret != 0, 0))
			return ret;
	}
	return 0;
error:
	return (int)0x800f1528;
}

int __attribute__((noinline, noclone)) _sceSblDmac5FreeOp(Dmac5Context *context)
{
	unsigned int keyRingIndex;

	sceKernelDmaOpFree(context->operation);
	keyRingIndex = context->keyRingIndex;
	if (keyRingIndex - 12 <= 11)
		dmac5ReleaseNpKey(keyRingIndex);
	else if (keyRingIndex <= 255)
		decKeyRingKeyUseCount(keyRingIndex);
	return 0;
}

int sceSblDmac5EncDecInternal(const void *src, void *dst, SceSize size,
	const void *key, unsigned int keyBits, void *iv, int coherent,
	uint32_t command)
{
	Dmac5Context context __attribute__((aligned(64)));
	SceKernelDmaOpId operation;
	uint32_t contextCommand;
	int ret;

	if (key == NULL) {
		ret = (int)0x800f1516;
		goto initError;
	}
	contextCommand = command;
	ret = dmac5SetKeySize(&contextCommand, keyBits);
	if (ret != 0) {
initError:
		SCE_DBG_PRINTF_FUNC(0xb, 787, "sceSblDmac5EncDec",
			"ctx init error (%d)\n", ret);
		return ret;
	}
	memset(&context, 0, sizeof(context));
	if (coherent)
		context.dataCoherency = 0x3ffff;
	context.ivCoherency = 0x1ff01ff;
	context.command = contextCommand;
	operation = sceKernelDmaOpAlloc("SceSblDmac5Common");
	if (operation < 0) {
		ret = (int)0x800f1528;
		goto initError;
	}
	context.operation = operation;
	context.keyRingIndex = 256;
	ret = dmac5Crypt(&context, src, dst, iv, size, context.command, key);
	_sceSblDmac5FreeOp(&context);
	return ret;
}

int sceSblDmac5EncDecWithReservedKey(const void *src, void *dst, SceSize size,
	unsigned int key, unsigned int keyBits, void *iv, int coherent,
	uint32_t command)
{
	Dmac5Context context __attribute__((aligned(64)));
	int ret;

	ret = dmac5InitReservedKeyContext(&context, command, keyBits, coherent, key);
	if (ret != 0) {
		SCE_DBG_PRINTF(0xb, 967, "ctx init error (%d)\n", ret);
		return ret;
	}
	ret = dmac5Crypt(&context, src, dst, iv, size, context.command, NULL);
	_sceSblDmac5FreeOp(&context);
	return ret;
}

int dmac5AllocOperation(void)
{
	SceKernelDmaOpId operation = sceKernelDmaOpAlloc("SceSblDmac5Common");

	if (operation < 0)
		return (int)0x800f1528;
	return operation;
}

int sceSblDmac5Rnd(void *dst, SceSize size, int coherent)
{
	uint8_t buffer[64] __attribute__((aligned(64)));
	SceKernelDmaOpId operation;
	uint32_t dataCoherency;
	int ret;

	if (size > sizeof(buffer))
		return (int)0x800f1516;
	operation = sceKernelDmaOpAlloc("SceSblDmac5Common");
	if (operation < 0)
		return (int)0x800f1528;
	dataCoherency = (uint32_t)-(coherent != 0) >> 14;
	ret = _sceSblDmac5DirectCrypt(operation, buffer, buffer, NULL,
		sizeof(buffer), 0, 4, dataCoherency, 0, NULL);
	sceKernelDmaOpFree(operation);
	if (__builtin_expect(ret != 0, 1))
		return ret;
	memcpy(dst, buffer, size);
	return 0;
}

/* Name unknown: hash transfer with an aligned temporary digest when needed. */
int dmac5HashCrypt(Dmac5Context *context, const void *src, void *dst,
	SceSize size, uint32_t flags, const void *key)
{
	uint8_t digest[32] __attribute__((aligned(32)));
	uint32_t command = context->command | 3 | flags;
	SceSize digestSize;
	int ret;

	if (((uintptr_t)dst & 31) != 0) {
		SceKernelDmaOpId operation = context->operation;
		unsigned int keyRingIndex = context->keyRingIndex;
		uint32_t ivCoherency = context->ivCoherency;
		uint32_t dataCoherency = context->dataCoherency | 0x3fe00;

		context->dataCoherency = dataCoherency;
		ret = _sceSblDmac5DirectCrypt(operation, src, digest,
			context->state, size, keyRingIndex, command,
			dataCoherency, ivCoherency, key);
		if ((flags & ~0x800u) == 0) {
			digestSize = (SceSize)0x800f1516;
			if ((command & 7) == 3) {
				if ((command & 0x38) == 0x38)
					digestSize = 16;
				else if ((command & 0x18) == 0)
					digestSize = 20;
				else if ((command & 0x18) == 0x10)
					digestSize = 32;
			}
			memcpy(dst, digest, digestSize);
		}
	} else {
		ret = _sceSblDmac5DirectCrypt(context->operation, src, dst,
			context->state, size, context->keyRingIndex, command,
			context->dataCoherency, context->ivCoherency, key);
	}
	return ret;
}

int SceSblSsMgrForDriver_E6E1AD15(const void *src, void *dst, int size,
	const void *key, int keyBits, void *iv, int coherent)
{
	return sceSblDmac5EncDecInternal(src, dst, size, key, keyBits,
		iv, coherent, 9);
}

int SceSblSsMgrForDriver_121FA69F(const void *src, void *dst, int size,
	const void *key, int keyBits, void *iv, int coherent)
{
	return sceSblDmac5EncDecInternal(src, dst, size, key, keyBits,
		iv, coherent, 10);
}

int SceSblSsMgrForDriver_82B5DCEF(const void *src, void *dst, int size,
	const void *key, int keyBits, void *iv, int coherent)
{
	return sceSblDmac5EncDecInternal(src, dst, size, key, keyBits,
		iv, coherent, 33);
}

int SceSblSsMgrForDriver_7D46768C(const void *src, void *dst, int size,
	const void *key, int keyBits, void *iv, int coherent)
{
	return sceSblDmac5EncDecInternal(src, dst, size, key, keyBits,
		iv, coherent, 34);
}

int SceSblSsMgrForDriver_C517770D(const void *src, void *dst, int size,
	const void *key, int keyBits, int coherent)
{
	return sceSblDmac5EncDecInternal(src, dst, size, key, keyBits,
		NULL, coherent, 1);
}

int SceSblSsMgrForDriver_7C978BE7(const void *src, void *dst, int size,
	const void *key, int keyBits, int coherent)
{
	return sceSblDmac5EncDecInternal(src, dst, size, key, keyBits,
		NULL, coherent, 2);
}

int SceSblSsMgrForDriver_01BE0374(const void *src, void *dst, int size,
	unsigned int key, int keyBits, int coherent)
{
	return sceSblDmac5EncDecWithReservedKey(src, dst, size, key, keyBits,
		NULL, coherent, 1);
}

int SceSblSsMgrForDriver_8B4700CB(const void *src, void *dst, int size,
	unsigned int key, int keyBits, int coherent)
{
	return sceSblDmac5EncDecWithReservedKey(src, dst, size, key, keyBits,
		NULL, coherent, 2);
}

int SceSblSsMgrForDriver_37DD5CBF(const void *src, void *dst, int size,
	unsigned int key, int keyBits, int coherent)
{
	return sceSblDmac5EncDecWithReservedKey(src, dst, size, key, keyBits,
		NULL, coherent, 65);
}

int SceSblSsMgrForDriver_8EAFB18A(const void *src, void *dst, int size,
	unsigned int key, int keyBits, int coherent)
{
	return sceSblDmac5EncDecWithReservedKey(src, dst, size, key, keyBits,
		NULL, coherent, 66);
}

int SceSblSsMgrForDriver_05B38698(const void *src, void *dst, int size,
	unsigned int key, int keyBits, void *iv, int coherent)
{
	return sceSblDmac5EncDecWithReservedKey(src, dst, size, key, keyBits,
		iv, coherent, 73);
}

int SceSblSsMgrForDriver_926BCCF0(const void *src, void *dst, int size,
	unsigned int key, int keyBits, void *iv, int coherent)
{
	return sceSblDmac5EncDecWithReservedKey(src, dst, size, key, keyBits,
		iv, coherent, 74);
}

int sceSblDmac5HmacTransform(const void *src, void *dst, SceSize size,
	const void *key, unsigned int keyBits, void *iv, int coherent,
	uint32_t command, uint32_t flags)
{
	Dmac5Context context __attribute__((aligned(64)));
	SceKernelDmaOpId operation;
	uint32_t contextCommand;
	int ret;

	SceKernelSuspendForDriver_4DF40893(0);
	contextCommand = command;
	ret = dmac5SetKeySize(&contextCommand, keyBits);
	if (__builtin_expect(ret != 0, 1)) {
initError:
		SCE_DBG_PRINTF(0xb, 1191, "ctx init error (0x%08x)\n", ret);
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	memset(&context, 0, sizeof(context));
	if (coherent)
		context.dataCoherency = 0x3ffff;
	context.ivCoherency = 0x1ff01ff;
	context.command = contextCommand;
	operation = sceKernelDmaOpAlloc("SceSblDmac5Common");
	if (operation < 0) {
		ret = (int)0x800f1528;
		goto initError;
	}
	context.keyRingIndex = 256;
	context.operation = operation;
	context.command |= flags;
	if (iv != NULL) {
		if (dmac5LoadIv(&context, iv) != 0 ||
			((ret = dmac5HashCrypt(&context, src, dst, size, flags, key)) == 0 &&
			(ret = dmac5StoreIv(&context, iv)) != 0)) {
			_sceSblDmac5FreeOp(&context);
			SceKernelSuspendForDriver_2BB92967(0);
			return (int)0x800f1528;
		}
		_sceSblDmac5FreeOp(&context);
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	ret = dmac5HashCrypt(&context, src, dst, size, flags, key);
	_sceSblDmac5FreeOp(&context);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblSsMgrForDriver_6704D985(const void *src, void *dst, int size,
	const void *key, void *iv, int coherent, uint32_t flags)
{
	return sceSblDmac5HmacTransform(src, dst, size, key, 256,
		iv, coherent, 0x23, flags);
}

int SceSblSsMgrForDriver_79F38554(const void *src, void *dst, int size,
	const void *key, void *iv, int coherent, uint32_t flags)
{
	return sceSblDmac5HmacTransform(src, dst, size, key, 256,
		iv, coherent, 0x33, flags);
}

/* Sony name unavailable: variable-size CMAC key, export NID 1B14658D. */
int SceSblSsMgrForDriver_1B14658D(const void *src, void *dst, int size,
	const void *key, unsigned int keyBits, void *iv, int coherent,
	uint32_t flags)
{
	return sceSblDmac5HmacTransform(src, dst, size, key, keyBits,
		iv, coherent, 0x3b, flags);
}

int sceSblDmac5HmacTransformWithReservedKey(const void *src, void *dst,
	SceSize size, unsigned int key, unsigned int keyBits, void *iv,
	int coherent, uint32_t command, uint32_t flags)
{
	Dmac5Context context __attribute__((aligned(64)));
	int initRet;
	int ret;

	SceKernelSuspendForDriver_4DF40893(0);
	initRet = dmac5InitReservedKeyContext(&context, command, keyBits, coherent, key);
	if (initRet != 0) {
		SCE_DBG_PRINTF(0xb, 1360, "ctx init error (0x%08x)\n", initRet);
		SceKernelSuspendForDriver_2BB92967(0);
		return initRet;
	}
	context.command |= flags;
	if (iv != NULL) {
		if (dmac5LoadIv(&context, iv) != 0 ||
			((ret = dmac5HashCrypt(&context, src, dst, size, flags, NULL)) == 0 &&
			(ret = dmac5StoreIv(&context, iv)) != 0))
			ret = (int)0x800f1528;
	} else {
		ret = dmac5HashCrypt(&context, src, dst, size, flags, NULL);
	}
	_sceSblDmac5FreeOp(&context);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblSsMgrForDriver_EA6ACB6D(const void *src, void *dst, int size,
	unsigned int key, unsigned int keyBits, void *iv, int coherent,
	uint32_t flags)
{
	return sceSblDmac5HmacTransformWithReservedKey(src, dst, size, key,
		keyBits, iv, coherent, 0x3b, flags);
}

int sceSblDmac5HashTransform(const void *src, void *dst, SceSize size,
	void *iv, int coherent, uint32_t command, uint32_t flags)
{
	Dmac5Context context __attribute__((aligned(64)));
	SceKernelDmaOpId operation;
	int ret;

	memset(&context, 0, sizeof(context));
	if (coherent)
		context.dataCoherency = 0x3ffff;
	context.command = command;
	context.ivCoherency = 0x1ff01ff;
	operation = sceKernelDmaOpAlloc("SceSblDmac5Common");
	if (operation < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 1418, "sceSblDmac5HashTransform",
			"ctx init error (0x%08x)\n",
			(int)0x800f1528);
		return (int)0x800f1528;
	}
	context.operation = operation;
	context.command |= flags;
	if (iv != NULL) {
		ret = dmac5LoadIv(&context, iv);
		if (__builtin_expect(ret != 0, 1))
			goto ivError;
		ret = dmac5HashCrypt(&context, src, dst, size, flags, NULL);
		if (ret != 0)
			goto freeOperation;
		ret = dmac5StoreIv(&context, iv);
		if (ret != 0) {
ivError:
			ret = (int)0x800f1528;
		}
	} else {
		ret = dmac5HashCrypt(&context, src, dst, size, flags, NULL);
	}
freeOperation:
	sceKernelDmaOpFree(context.operation);
	return ret;
}

int SceSblSsMgrForDriver_EB3AF9B5(const void *src, void *dst, int size, void *iv,
	int coherent, uint32_t flags)
{
	return sceSblDmac5HashTransform(src, dst, size, iv,
		coherent, 3, flags);
}

int sceSblDmac5EncDec(SceSblDmac5EncDecParam *param, SceUInt32 command)
{
	SceSblDmac5EncDecParam local;
	uint8_t iv[16], key[32];
	SceUInt32 syscallState = sceSyscallEnter();
	const void *src, *userKey;
	void *dst, *userIv;
	SceSize size, keyBits;
	SceSize ivSize = 0;
	int ret;

	if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0) == 0) {
		sceSyscallLeave(syscallState);
		return (int)0x80010058;
	}
	ret = SceSysmemForDriver_BC996A7A(&local, param, sizeof(local));
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	src = local.src;
	dst = local.dst;
	size = local.length;
	userKey = local.key;
	keyBits = local.keysize;
	userIv = local.iv;
	if (size == 0 ||
		((keyBits & ~0x80u) != 64 && keyBits != 128 &&
		keyBits != 256) || (command & 7) - 1 > 1) {
		sceSyscallLeave(syscallState);
		return (int)0x800f1516;
	}
	if (command & 0x38) {
		switch (command & 0x40) {
		case 0:
			ivSize = 16;
			break;
		case 0x40:
			ivSize = 8;
			break;
		default:
			sceSyscallLeave(syscallState);
			return (int)0x800f1528;
		}
		if (userIv == NULL) {
			sceSyscallLeave(syscallState);
			return (int)0x800f1516;
		}
		ret = SceSysmemForDriver_BC996A7A(iv, userIv, ivSize);
		if (ret != 0) {
			sceSyscallLeave(syscallState);
			return ret;
		}
	}
	ret = SceSysmemForDriver_BC996A7A(key, userKey, keyBits >> 3);
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = dmac5LockUserBuffers(src, size, dst, size);
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = sceSblDmac5EncDecInternal(src, dst, size,
		key, keyBits, iv, 1, command);
	dmac5UnlockUserBuffers(src, size, dst, size);
	if (ret == 0 && ivSize != 0)
		ret = SceSysmemForDriver_6D88EF8A(userIv, iv, ivSize);
	sceSyscallLeave(syscallState);
	return ret;
}

int sceSblDmac5HashTransformForUser(SceSblDmac5HashTransformParam *param,
	SceUInt32 command, SceUInt32 extra)
{
	uint8_t digest[32] __attribute__((aligned(32)));
	uint8_t context[sizeof(SceSblDmac5HashTransformContext)];
	SceSblDmac5HashTransformParam local;
	SceUInt32 syscallState = sceSyscallEnter();
	const void *src;
	void *dst, *userContext;
	SceSize size;
	SceSize digestSize, contextSize = 0;
	int copyDigest;
	int ret;

	if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0) == 0) {
		sceSyscallLeave(syscallState);
		return (int)0x80010058;
	}
	ret = SceSysmemForDriver_BC996A7A(&local, param, sizeof(local));
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	src = local.src;
	dst = local.dst;
	size = local.length;
	userContext = local.ctx;
	if (size == 0 || (command & 7) != 3)
		goto invalid;
	if ((command & 0x38) == 0x38)
		digestSize = 16;
	else if ((command & 0x18) == 0)
		digestSize = 20;
	else if ((command & 0x18) == 0x10)
		digestSize = 32;
	else
		goto invalid;
	if (extra & ~0x800u)
		digestSize = 0;
	copyDigest = (extra & ~0x800u) == 0;
	if ((uintptr_t)src & 0x3f)
		goto invalid;
	if (extra != 0) {
		contextSize = (command & 0x38) == 0x38 ? 16 : 40;
		if (userContext == NULL)
			goto invalid;
		ret = SceSysmemForDriver_BC996A7A(context, userContext, contextSize);
		if (ret != 0) {
			sceSyscallLeave(syscallState);
			return ret;
		}
	}
	ret = dmac5LockUserBuffers(src, size, dst, digestSize);
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = sceSblDmac5HashTransform(src, digest, size,
		context, 1, command, extra);
	dmac5UnlockUserBuffers(src, size, dst, digestSize);
	if (ret == 0) {
		if (contextSize != 0)
			ret = SceSysmemForDriver_6D88EF8A(userContext, context, contextSize);
		if (ret == 0 && copyDigest)
			ret = SceSysmemForDriver_6D88EF8A(dst, digest, digestSize);
	}
	sceSyscallLeave(syscallState);
	return ret;
invalid:
	sceSyscallLeave(syscallState);
	return (int)0x800f1516;
}

int dmac5ClearKeyring(void)
{
	memset(dmacKeyringStorage, 0,
		sizeof(dmacKeyringStorage));
	return 0;
}

int dmac5LockUserBuffers(const void *src, SceSize srcSize, void *dst,
	SceSize dstSize)
{
	int ret = sceKernelLockRangeWithMode(16, (void *)src, srcSize);

	if (ret >= 0) {
		if (dstSize != 0 &&
			(ret = sceKernelLockRangeWithMode(32, dst, dstSize)) < 0) {
			SceSysmemForDriver_22CBE925(16, (void *)src, srcSize);
			return ret;
		}
		ret = 0;
	}
	return ret;
}

int dmac5UnlockUserBuffers(const void *src, SceSize srcSize, void *dst,
	SceSize dstSize)
{
	int ret = SceSysmemForDriver_22CBE925(16, (void *)src, srcSize);

	if (dstSize != 0)
		return SceSysmemForDriver_22CBE925(32, dst, dstSize);
	return ret;
}

int dmac5LoadIv(Dmac5Context *context, const void *iv)
{
	uint32_t command = context->command;
	uint32_t kind = command & 7;
	uint32_t mode = command & 0x38;
	SceSize size;
	int ret;

	if (kind == 3) {
		ret = command & 0xc00;
		if (ret == 0)
			goto out;
		size = mode == 0x38 ? 16 : 40;
		goto copy;
	}
	if (kind - 1 > 1) {
		ret = (int)0x800f1516;
		if (kind == 4)
			ret = 0;
		goto out;
	}
	ret = mode;
	if (ret == 0)
		goto out;
	size = command & 0x40 ? 8 : 16;
copy:
	memcpy(context->state, iv, size);
	ret = 0;
out:
	return ret;
}

int dmac5StoreIv(Dmac5Context *context, void *iv)
{
	uint32_t command = context->command;
	uint32_t kind = command & 7;
	uint32_t mode = command & 0x38;
	int ret;

	if (kind == 3) {
		ret = command & 0xc00;
		if (ret == 0)
			goto out;
		ret = mode == 0x38 ? 16 : 40;
		goto copy;
	}
	if (kind - 1 > 1) {
		ret = (int)0x800f1516;
		if (kind == 4)
			ret = 0;
		goto out;
	}
	ret = mode;
	if (ret == 0)
		goto out;
	switch (command & 0x40) {
	case 0:
		ret = 16;
		break;
	case 0x40:
		ret = 8;
		break;
	default:
		ret = (int)0x800f1516;
		goto out;
	}
copy:
	memcpy(iv, context->state, ret);
	ret = 0;
out:
	return ret;
}
