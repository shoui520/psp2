#include "ssmgr_internal.h"
#include "dmac5mgr.h"
#include <threadmgr/threadmgr.h>
#include <sysmem/sysclib.h>
#include <authmgr/sblauthmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/sysmem.h>
#include <ss_mgr/ss_mgr.h>
#include <intrmgr/syscall.h>

int __attribute__((noinline, noclone)) decKeyRingKeyUseCount(unsigned int keyRingIndex)
{
	if (keyRingIndex > 31)
		return (int)0x800f1516;
	lockFastMutex(&dmacKeyringMutex);
	--dmacKeyringStorage[keyRingIndex].useCount;
	sceKernelUnlockFastMutex(&dmacKeyringMutex);
	return 0;
}

int dmac5SetExternalKey(SceKernelDmaOpDirectParam *param, const void *key,
	uint32_t command)
{
	uint32_t keyWords[16];
	volatile uint32_t *keyData = (volatile uint32_t *)param->encDec.key;
	SceSize size;

	if ((command & 0x300) == 0)
		size = 8;
	else if ((command & 0x300) == 0x100)
		size = 16;
	else if ((command & 0x300) == 0x300)
		size = 32;
	else if ((command & 0x300) == 0x200)
		size = 24;
	else
		size = 0;
	memset(keyWords, 0, sizeof(keyWords));
	memcpy(keyWords, key, size);
	keyData[0] = keyWords[0];
	keyData[1] = keyWords[1];
	keyData[2] = keyWords[2];
	keyData[3] = keyWords[3];
	keyData[4] = keyWords[4];
	keyData[5] = keyWords[5];
	keyData[6] = keyWords[6];
	keyData[7] = keyWords[7];
	keyData[8] = keyWords[8];
	keyData[9] = keyWords[9];
	keyData[10] = keyWords[10];
	keyData[11] = keyWords[11];
	keyData[12] = keyWords[12];
	keyData[13] = keyWords[13];
	keyData[14] = keyWords[14];
	keyData[15] = keyWords[15];
	return 0;
}

int dmac5ValidateReservedKey(unsigned int key, uint32_t *index)
{
	if (key - 28 > 3)
		return (int)0x800f1516;
	*index = key;
	return 0;
}

int __attribute__((noinline, noclone)) dmac5SetKeySize(uint32_t *command, unsigned int bits)
{
	if (bits == 192) {
		*command |= 0x200;
		return 0;
	}
	if (bits > 192) {
		if (bits == 256 || bits == 512) {
			*command |= 0x300;
			return 0;
		}
		return (int)0x800f1516;
	}
	if (bits != 64) {
		if (bits == 128) {
			*command |= 0x100;
			return 0;
		}
		return (int)0x800f1516;
	}
	return 0;
}

int dmac5SetKeyRingIndex(SceKernelDmaOpDirectParam *param,
	unsigned int keyRingIndex)
{
	param->encDec.keyring = keyRingIndex;
	return 0;
}

int checkKeyRingKeyExist(const void *key, unsigned int keyBits,
	unsigned int *keyRingIndex, int keyId)
{
	SceSblDmac5KeyRing *entry = &dmacKeyringStorage[12];
	SceSize keySize = keyBits >> 3;
	unsigned int index = 12;
	uint16_t useCount;
	int ret;

	lockFastMutex(&dmacKeyringMutex);
	for (;;) {
		if (entry->valid == 1 && entry->keyId == keyId &&
			entry->keySize == keySize) {
			ret = timingsafe_memcmp(entry->key, key, keySize);
			if (ret == 0)
				break;
		}
		++index;
		++entry;
		if (index == 24) {
			sceKernelUnlockFastMutex(&dmacKeyringMutex);
			return (int)0x800f1502;
		}
	}
	if (dmacKeyringStorage[index].useCount == UINT16_MAX) {
		sceKernelUnlockFastMutex(&dmacKeyringMutex);
		return (int)0x800f1510;
	}
	for (SceSblDmac5KeyRing *other = &dmacKeyringStorage[12];
		other != &dmacKeyringStorage[24]; ++other) {
		if (other->age != UINT8_MAX)
			++other->age;
	}
	entry = &dmacKeyringStorage[index];
	useCount = entry->useCount;
	entry->age = 0;
	*keyRingIndex = index;
	entry->useCount = useCount + 1;
	sceKernelUnlockFastMutex(&dmacKeyringMutex);
	return ret;
}

static inline __attribute__((always_inline)) void cacheKeyRingKey(
	unsigned int index, const void *key, unsigned int keyBits, int keyId)
{
	SceSblDmac5KeyRing *entry;

	if (index > 31 || ((keyBits & ~0x80u) != 64 && keyBits != 128 &&
		keyBits != 256))
		return;
	entry = &dmacKeyringStorage[index];
	__builtin_memset(entry->key, 0, sizeof(entry->key));
	entry = memcpy(entry->key, key, keyBits >> 3);
	entry->keyId = keyId;
	entry->age = 0;
	entry->keySize = keyBits >> 3;
	entry->useCount = 1;
	entry->valid = 1;
}

int setNewKeyRingKey(const void *key, unsigned int keyBits,
	unsigned int *keyRingIndex, int keyId)
{
	unsigned int index = 12;
	SceSblDmac5KeyRing *entry = &dmacKeyringStorage[12];
	int ret;

	lockFastMutex(&dmacKeyringMutex);
	for (;;) {
		if (entry->valid == 0)
			break;
		++entry;
		if (++index == 24) {
			unsigned int oldest = UINT16_MAX;
			unsigned int age = 0;

			for (index = 12; index != 24; ++index) {
				entry = &dmacKeyringStorage[index];
				if (entry->useCount == 0 && entry->age >= age) {
					age = entry->age;
					oldest = index;
				}
			}
			if (oldest == UINT16_MAX) {
				sceKernelUnlockFastMutex(&dmacKeyringMutex);
				return (int)0x800f1510;
			}
			ret = sceSblAuthMgrSetDmac5Key(key, keyBits >> 3, oldest, keyId);
			if (ret != 0) {
				sceKernelUnlockFastMutex(&dmacKeyringMutex);
				return ret;
			}
			for (index = 12; index != 24; ++index) {
				entry = &dmacKeyringStorage[index];
				if (entry->age != UINT8_MAX)
					++entry->age;
			}
			cacheKeyRingKey(oldest, key, keyBits, keyId);
			*keyRingIndex = oldest;
			sceKernelUnlockFastMutex(&dmacKeyringMutex);
			return 0;
		}
	}
	ret = sceSblAuthMgrSetDmac5Key(key, keyBits >> 3, index, keyId);
	if (ret != 0) {
		sceKernelUnlockFastMutex(&dmacKeyringMutex);
		return ret;
	}
	for (unsigned int other = 12; other != 24; ++other) {
		entry = &dmacKeyringStorage[other];
		if (entry->age != UINT8_MAX)
			++entry->age;
	}
	cacheKeyRingKey(index, key, keyBits, keyId);
	*keyRingIndex = index;
	sceKernelUnlockFastMutex(&dmacKeyringMutex);
	return 0;
}

int __attribute__((noinline, noclone)) dmac5ReleaseNpKey(unsigned int keyRingIndex)
{
	if (keyRingIndex - 12 > 11)
		return (int)0x800f1516;
	return decKeyRingKeyUseCount(keyRingIndex);
}

int __attribute__((noinline, noclone)) dmac5InitNpKeyContext(
	Dmac5Context *context, uint32_t command, unsigned int keyBits,
	int coherent, const void *key, int keyId)
{
	unsigned int keyRingIndex;
	SceKernelDmaOpId operation;
	int ret;

	ret = checkKeyRingKeyExist(key, keyBits, &keyRingIndex, keyId);
	if (ret == (int)0x800f1502)
		ret = setNewKeyRingKey(key, keyBits, &keyRingIndex, keyId);
	if (__builtin_expect(ret != 0, 0))
		return ret;
	ret = dmac5SetKeySize(&command, keyBits);
	if (ret != 0) {
		dmac5ReleaseNpKey(keyRingIndex);
		return ret;
	}
	memset(context, 0, sizeof(*context));
	if (coherent)
		context->dataCoherency = UINT32_MAX >> 14;
	else
		context->dataCoherency = coherent;
	context->ivCoherency = 0x1ff01ff;
	context->command = command;
	operation = dmac5AllocOperation();
	if (operation < 0) {
		dmac5ReleaseNpKey(keyRingIndex);
		return (int)0x800f1528;
	}
	context->operation = operation;
	context->keyRingIndex = keyRingIndex;
	return 0;
}

int dmac5Crypt(Dmac5Context *context, const void *src, void *dst,
	void *iv, SceSize size, uint32_t command, const void *key);
int _sceSblDmac5FreeOp(Dmac5Context *context);

int __attribute__((noinline, noclone)) sceSblDmac5EncDecNP(
	const void *src, void *dst, SceSize size, const void *key,
	unsigned int keyBits, void *iv, int keyId, int coherent, uint32_t command)
{
	Dmac5Context context __attribute__((aligned(64)));
	int ret;

	SceKernelSuspendForDriver_4DF40893(0);
	ret = dmac5InitNpKeyContext(&context, command, keyBits, coherent, key, keyId);
	if (ret != 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	ret = dmac5Crypt(&context, src, dst, iv, size, context.command, NULL);
	_sceSblDmac5FreeOp(&context);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int sceSblDmac5AesCbcEncNP(const void *src, void *dst, int size,
	const void *key, int keyBits, void *iv, int keyId, int coherent)
{
	return sceSblDmac5EncDecNP(src, dst, size, key, keyBits, iv,
		keyId, coherent, 9);
}

int SceSblSsMgrForDriver_1901CB5E(const void *src, void *dst, int size,
	const void *key, int keyBits, void *iv, int keyId, int coherent)
{
	return sceSblDmac5EncDecNP(src, dst, size, key, keyBits, iv,
		keyId, coherent, 10);
}

int sceSblDmac5AesEcbEncNP(const void *src, void *dst, int size,
	const void *key, int keyBits, int keyId, int coherent)
{
	return sceSblDmac5EncDecNP(src, dst, size, key, keyBits, NULL,
		keyId, coherent, 1);
}

int SceSblSsMgrForDriver_197ACF6F(const void *src, void *dst, int size,
	const void *key, int keyBits, int keyId, int coherent)
{
	return sceSblDmac5EncDecNP(src, dst, size, key, keyBits, NULL,
		keyId, coherent, 2);
}

int __attribute__((noinline, noclone)) sceSblDmac5HmacTransformNP(
	const void *src, void *dst, SceSize size, const void *key,
	unsigned int keyBits, void *iv, int keyId, int coherent,
	uint32_t command, uint32_t flags)
{
	Dmac5Context context __attribute__((aligned(64)));
	int ret;

	SceKernelSuspendForDriver_4DF40893(0);
	ret = dmac5InitNpKeyContext(&context, command, keyBits, coherent, key, keyId);
	if (ret != 0) {
		SceKernelSuspendForDriver_2BB92967(0);
		return ret;
	}
	context.command |= flags;
	if (iv != NULL) {
		if (__builtin_expect(dmac5LoadIv(&context, iv) != 0, 1) ||
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

int SceSblSsMgrForDriver_92E37656(const void *src, void *dst, SceSize size,
	const void *key, void *iv, int keyId, int coherent, uint32_t flags)
{
	return sceSblDmac5HmacTransformNP(src, dst, size, key, 256, iv,
		keyId, coherent, 0x23, flags);
}

int SceSblSsMgrForDriver_83B058F5(const void *src, void *dst, SceSize size,
	const void *key, unsigned int keyBits, void *iv, int keyId,
	int coherent, uint32_t flags)
{
	return sceSblDmac5HmacTransformNP(src, dst, size, key, keyBits, iv,
		keyId, coherent, 0x3b, flags);
}

int sceSblDmac5EncDecKeyGen(SceSblDmac5EncDecParam *param, int keyId,
	uint32_t command)
{
	SceSblDmac5EncDecParam local;
	uint8_t iv[16], key[32];
	const void *src, *userKey;
	void *dst, *userIv;
	SceSize size;
	unsigned int keyBits;
	SceUInt32 syscallState = sceSyscallEnter();
	SceSize ivSize = 0;
	int ret;

	if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0) == 0) {
		sceSyscallLeave(syscallState);
		return (int)0x80010058;
	}
	ret = sceKernelCopyFromUser(&local, param, sizeof(local));
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
		((keyBits & ~0x40u) != 128 && keyBits != 256) ||
		(command & 7) - 1 > 1)
		goto invalid;
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
		if (userIv == NULL)
			goto invalid;
		ret = sceKernelCopyFromUser(iv, userIv, ivSize);
		if (ret != 0) {
			sceSyscallLeave(syscallState);
			return ret;
		}
	}
	ret = sceKernelCopyFromUser(key, userKey, keyBits >> 3);
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = dmac5LockUserBuffers(src, size, dst, size);
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = sceSblDmac5EncDecNP(src, dst, size,
		key, keyBits, iv, keyId, 1, command);
	dmac5UnlockUserBuffers(src, size, dst, size);
	if (ret == 0 && ivSize != 0)
		ret = sceKernelCopyToUser(userIv, iv, ivSize);
	sceSyscallLeave(syscallState);
	return ret;
invalid:
	sceSyscallLeave(syscallState);
	return (int)0x800f1516;
}

int sceSblDmac5HmacKeyGen(SceSblDmac5HashTransformParam *param, int keyId,
	uint32_t command, uint32_t flags)
{
	uint8_t digest[32] __attribute__((aligned(32)));
	uint32_t key[8];
	uint8_t context[sizeof(SceSblDmac5HashTransformContext)];
	SceSblDmac5HashTransformParam local;
	const void *src, *userKey;
	void *dst, *userContext;
	SceSize size;
	unsigned int keyBits;
	SceUInt32 syscallState = sceSyscallEnter();
	SceSize digestSize, contextSize = 0;
	uint32_t contextFlags = flags & 0xc00;
	int copyDigest;
	int ret;

	if (SceSblACMgrForKernel_sceSblACMgrIsSystemProgram(0) == 0) {
		sceSyscallLeave(syscallState);
		return (int)0x80010058;
	}
	ret = sceKernelCopyFromUser(&local, param, sizeof(local));
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	src = local.src;
	dst = local.dst;
	size = local.length;
	userKey = local.key;
	keyBits = local.keysize;
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
	if (keyBits != 256 && keyBits != 128)
		goto invalid;
	copyDigest = (flags & ~0x800u) == 0;
	if (!copyDigest)
		digestSize = 0;
	if ((uintptr_t)src & 0x3f)
		goto invalid;
	if (contextFlags != 0) {
		contextSize = (command & 0x38) == 0x38 ? 16 : 40;
		if (userContext == NULL)
			goto invalid;
		ret = sceKernelCopyFromUser(context, userContext, contextSize);
		if (ret != 0) {
			sceSyscallLeave(syscallState);
			return ret;
		}
	}
	__builtin_memset(key, 0, sizeof(key));
	ret = sceKernelCopyFromUser(key, userKey, keyBits >> 3);
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = dmac5LockUserBuffers(src, size, dst, digestSize);
	if (ret != 0) {
		sceSyscallLeave(syscallState);
		return ret;
	}
	ret = sceSblDmac5HmacTransformNP(src, digest, size,
		key, 256, context, keyId, 1, command, contextFlags);
	dmac5UnlockUserBuffers(src, size, dst, digestSize);
	if (ret == 0) {
		if (contextSize != 0)
			ret = sceKernelCopyToUser(userContext, context, contextSize);
		if (ret == 0 && copyDigest)
			ret = sceKernelCopyToUser(dst, digest, digestSize);
	}
	sceSyscallLeave(syscallState);
	return ret;
invalid:
	sceSyscallLeave(syscallState);
	return (int)0x800f1516;
}
