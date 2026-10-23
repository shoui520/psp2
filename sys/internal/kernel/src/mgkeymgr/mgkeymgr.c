/*
 * SceSblMgKeyMgr: MagicGate keyslot transforms and secure-module setup.
 * All supported firmware originals are identical. Private names are inferred;
 * exported names retain their unresolved NIDs.
 */
#include <stdint.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <sm_comm/sm_comm.h>
#include <mgkeymgr/mgkeymgr.h>
#include <dmacmgr/dmac_regs.h>

#define MG_ERROR_ARGUMENT	((int)0x800f1616)
#define MG_ERROR_UNSUPPORTED	((int)0x800f1625)
#define MG_ERROR_OPERATION	((int)0x800f1628)

static SceDmac5KeyringRegisters *s_keyring;
static SceSblSmCommId s_smId;

/* Original SceSelfAuthInfo request, including unknown capability/attribute bits. */
static const uint32_t s_request[0x90 / 4] = {
	0x00000001, 0x28080000, 0, 0,
	0x00000080, 0x00f000c0, 0, 0xffffffff,
	0, 0, 0, 0,
	0x03800980, 0x00c30000, 0x09800000, 0x00000080,
	0, 0, 0, 0xffffffff,
};

int MapKeyRegisters(void);

int module_start(unsigned args, void *argp)
{
	MapKeyRegisters();
	return 0;
}

int module_stop(unsigned args, void *argp)
{
	return 0;
}

/* name unknown: map the DMAC5 key-register page. */
int __attribute__((noinline)) MapKeyRegisters(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	SceUID uid;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = DMAC5_KEYRING_REG_BASE;
	uid = sceKernelAllocMemBlockForKernel("SceSblDMAC5DmacKRBase", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
		DMAC_REG_SIZE, &opt);
	if (uid < 0)
		return MG_ERROR_OPERATION;
	sceKernelGetMemBlockBase(uid, &base);
	s_keyring = base;
	return 0;
}

int SceSblMgKeyMgrForDriver_B6AC3AE1(const void *input, void *output)
{
	if ((input == NULL) | (output == NULL))
		return MG_ERROR_ARGUMENT;
	return MG_ERROR_UNSUPPORTED;
}

/* name unknown: stage a block of up to 32 bytes and run DMAC5 AES-ECB with a keyslot. */
int __attribute__((noinline)) AesTransform(const void *input, void *output,
	unsigned size, unsigned slot, unsigned encrypt)
{
	uint8_t plain[32] __attribute__((aligned(16)));
	uint8_t cipher[32] __attribute__((aligned(16)));
	void *staged;
	int ret;

	if (encrypt > 1 || size > 32)
		return MG_ERROR_ARGUMENT;
	staged = __memcpy_chk(plain, input, size, sizeof(plain));
	if (encrypt == 1)
		ret = SceSblSsMgrForDriver_01BE0374(staged, cipher, size,
			slot, 0x80, 1);
	else
		ret = SceSblSsMgrForDriver_8B4700CB(staged, cipher, size,
			slot, 0x80, 1);
	if (ret != 0)
		return MG_ERROR_OPERATION;
	memcpy(output, cipher, size);
	return 0;
}

int __attribute__((noinline)) AesDecrypt32(const void *input, void *output, unsigned slot)
{
	return AesTransform(input, output, 32, slot, 0);
}

int __attribute__((noinline)) AesEncrypt(const void *input, void *output, unsigned slot)
{
	return AesTransform(input, output, 16, slot, 1);
}

int __attribute__((noinline)) AesDecrypt(const void *input, void *output, unsigned slot)
{
	return AesTransform(input, output, 16, slot, 0);
}

/* name unknown: the DES stage copies words, without a memcpy import. */
int __attribute__((noinline)) DesTransform(const uint32_t *input, uint32_t *output,
	unsigned slot, unsigned encrypt)
{
	uint32_t plain[8] __attribute__((aligned(16)));
	uint32_t cipher[8] __attribute__((aligned(16)));
	int ret;

	if (encrypt > 1)
		return MG_ERROR_ARGUMENT;
	plain[0] = input[0];
	plain[1] = input[1];
	plain[2] = input[2];
	plain[3] = input[3];
	if (encrypt == 1)
		ret = SceSblSsMgrForDriver_37DD5CBF(plain, cipher, 16,
			slot, 0xc0, encrypt);
	else
		ret = SceSblSsMgrForDriver_8EAFB18A(plain, cipher, 16,
			slot, 0xc0, 1);
	if (ret != 0)
		return MG_ERROR_OPERATION;
	output[0] = cipher[0];
	output[1] = cipher[1];
	output[2] = cipher[2];
	output[3] = cipher[3];
	return 0;
}

int __attribute__((noinline)) DesDecrypt(const void *input, void *output, unsigned slot)
{
	return DesTransform(input, output, slot, 0);
}

int __attribute__((noinline)) DesEncrypt(const void *input, void *output, unsigned slot)
{
	return DesTransform(input, output, slot, 1);
}

/* name unknown: start a secure module with the original authorization request. */
int __attribute__((noinline)) StartSecureModule(const char *path)
{
	SelfAuthRequest auth;
	int ret;

	memset(&auth, 0, sizeof(auth));
	memcpy(&auth.caller, s_request, sizeof(s_request));
	ret = sceSblACMgrGetMediaType(path, &auth.mediaType);
	if (ret < 0)
		return ret;
	auth.selfType = (auth.selfType & ~0xfU) | 2;
	return sceSblSmCommStartSm(0, path, 0, &auth, &s_smId);
}

int __attribute__((noinline)) StopSecureModule(void)
{
	SceSblSmCommPair stopResult;

	return sceSblSmCommStopSm(s_smId, &stopResult);
}

/* The secure-module command payload is intentionally not initialized by Sony. */
int __attribute__((noinline)) CallSecureModule(unsigned command)
{
	uint32_t result = 0;
	uint8_t data[0x30];
	int ret;

	ret = StartSecureModule("os0:sm/mgkm_sm.self");
	if (ret != 0)
		return ret;
	if (sceSblSmCommCallFunc(s_smId, command, &result, data, sizeof(data)) != 0)
		result = MG_ERROR_OPERATION;
	if (StopSecureModule() != 0)
		return MG_ERROR_OPERATION;
	return result;
}

/* name unknown: mgkm_sm command 2 sets up the keyslots the exports use. */
int __attribute__((noinline)) SetKeyslots(void)
{
	return CallSecureModule(2);
}

/* Two identical halves form the 32-byte wrapped DES key, decrypted into keyslot 0x1d. */
static inline int InstallDesKey(const uint32_t *key)
{
	uint32_t wrapped[8];

	wrapped[0] = key[0];
	wrapped[1] = key[1];
	wrapped[2] = key[2];
	wrapped[3] = key[3];
	wrapped[4] = wrapped[0];
	wrapped[5] = wrapped[1];
	wrapped[6] = wrapped[2];
	wrapped[7] = wrapped[3];
	return AesDecrypt32(wrapped, (void *)s_keyring->slot[0x1d].words, 0x1e);
}

int SceSblMgKeyMgrForDriver_20B66ADA(const uint32_t *key, const void *input, void *output)
{
	int ret;

	if (((key == NULL) | (output == NULL)) || input == NULL)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = InstallDesKey(key);
		if (ret == 0)
			ret = DesEncrypt(input, output, 0x1d);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_5FECCE03(const uint32_t *key, const void *input, void *output)
{
	int ret;

	if (((key == NULL) | (output == NULL)) || input == NULL)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = InstallDesKey(key);
		if (ret == 0)
			ret = DesDecrypt(input, output, 0x1d);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

/* name unknown: decrypt two key halves, xor them, and install keyslot 0x1d. */
static inline int InstallXorKey(const void *first, const void *second)
{
	uint32_t left[4], right[4];
	uint8_t *l = (uint8_t *)left, *r = (uint8_t *)right;
	int ret;

	ret = AesDecrypt(first, left, 0x1e);
	if (ret != 0)
		return ret;
	ret = AesDecrypt(second, right, 0x1e);
	if (ret != 0)
		return ret;
	for (unsigned i = 0; i < 16; i++)
		r[i] ^= l[i];
	s_keyring->slot[0x1d].words[0] = right[0];
	s_keyring->slot[0x1d].words[1] = right[1];
	s_keyring->slot[0x1d].words[2] = right[2];
	s_keyring->slot[0x1d].words[3] = right[3];
	return 0;
}

int SceSblMgKeyMgrForDriver_EA534495(const void *first, const void *second,
	const void *input, void *output)
{
	int ret;

	if (((first == NULL) | (second == NULL)) || ((input == NULL) | (output == NULL)))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = InstallXorKey(first, second);
		if (ret == 0)
			ret = AesEncrypt(input, output, 0x1d);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_DF3B808D(const void *first, const void *second,
	const void *input, void *output)
{
	int ret;

	if (((first == NULL) | (second == NULL)) || ((input == NULL) | (output == NULL)))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = InstallXorKey(first, second);
		if (ret == 0)
			ret = AesDecrypt(input, output, 0x1d);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_BC7D86EE(const uint32_t *key, void *wrapped, void *plain)
{
	uint32_t intermediate[4];
	int ret;

	if (((key == NULL) | (wrapped == NULL)) || plain == NULL)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = InstallDesKey(key);
		if (ret == 0) {
			ret = DesEncrypt(plain, intermediate, 0x1d);
			if (ret == 0)
				ret = AesEncrypt(intermediate, wrapped, 0x1e);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_4C82A466(const uint32_t *key, void *wrapped, void *plain)
{
	uint32_t intermediate[4];
	int ret;

	if (((key == NULL) | (wrapped == NULL)) || plain == NULL)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = InstallDesKey(key);
		if (ret == 0) {
			ret = AesDecrypt(wrapped, intermediate, 0x1e);
			if (ret == 0)
				ret = DesEncrypt(intermediate, plain, 0x1d);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_D37C3DAE(const uint32_t *key, void *wrapped, void *plain)
{
	uint32_t intermediate[4];
	int ret;

	if (((key == NULL) | (wrapped == NULL)) || plain == NULL)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = InstallDesKey(key);
		if (ret == 0) {
			ret = DesDecrypt(plain, intermediate, 0x1d);
			if (ret == 0)
				ret = AesEncrypt(intermediate, wrapped, 0x1e);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_8CFCAA52(const void *key, void *wrapped, void *plain)
{
	uint8_t intermediate[16];
	int ret;

	if (((key == NULL) | (wrapped == NULL)) || plain == NULL)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = AesDecrypt(key, (void *)s_keyring->slot[0x1d].words, 0x1e);
		if (ret == 0) {
			ret = AesEncrypt(plain, intermediate, 0x1d);
			if (ret == 0)
				ret = AesEncrypt(intermediate, wrapped, 0x1e);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_F8F856FC(const void *key, void *wrapped, void *plain)
{
	uint8_t intermediate[16];
	int ret;

	if (((key == NULL) | (wrapped == NULL)) || plain == NULL)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = AesDecrypt(key, (void *)s_keyring->slot[0x1d].words, 0x1e);
		if (ret == 0) {
			ret = AesDecrypt(wrapped, intermediate, 0x1e);
			if (ret == 0)
				ret = AesEncrypt(intermediate, plain, 0x1d);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_8ECFFD7A(const void *key, void *wrapped, void *plain)
{
	uint8_t intermediate[16];
	int ret;

	if (((key == NULL) | (wrapped == NULL)) || plain == NULL)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = AesDecrypt(key, (void *)s_keyring->slot[0x1d].words, 0x1e);
		if (ret == 0) {
			ret = AesDecrypt(plain, intermediate, 0x1d);
			if (ret == 0)
				ret = AesEncrypt(intermediate, wrapped, 0x1e);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_71FA16B0(const void *first, void *second)
{
	uint8_t left[16], right[16];
	int ret;

	if ((first == NULL) | (second == NULL))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetKeyslots();
	if (ret == 0) {
		ret = AesDecrypt(first, left, 0x1e);
		if (ret == 0) {
			ret = AesDecrypt(second, right, 0x1e);
			if (ret == 0) {
				for (unsigned i = 0; i < 16; i++)
					right[i] ^= left[i];
				ret = AesEncrypt(right, second, 0x1e);
			}
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

/* name unknown: mgkm_sm command 1 sets up keyslot 0x1f for the rewrap exports. */
int __attribute__((noinline)) SetRewrapKeyslot(void)
{
	return CallSecureModule(1);
}

int SceSblMgKeyMgrForDriver_1C5388D0(void *first, void *second)
{
	uint8_t intermediate[16];
	int ret;

	if ((first == NULL) | (second == NULL))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetRewrapKeyslot();
	if (ret == 0) {
		ret = SetKeyslots();
		if (ret == 0) {
			ret = AesDecrypt(first, intermediate, 0x1e);
			if (ret == 0)
				ret = AesEncrypt(intermediate, second, 0x1f);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_1263FB04(void *first, void *second)
{
	uint8_t intermediate[16];
	int ret;

	if ((first == NULL) | (second == NULL))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = SetRewrapKeyslot();
	if (ret == 0) {
		ret = SetKeyslots();
		if (ret == 0) {
			ret = AesDecrypt(second, intermediate, 0x1f);
			if (ret == 0)
				ret = AesEncrypt(intermediate, first, 0x1e);
		}
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}
