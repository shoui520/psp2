/*
 * SceSblMgKeyMgr: MagicGate keyslot transforms and secure-module setup.
 * All supported firmware originals are identical. Private names are inferred;
 * exported names retain their unresolved NIDs.
 */
#include <stdint.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <acmgr/acmgr.h>
#include <sm_comm/sm_comm.h>
#include <mgkeymgr/mgkeymgr.h>
#include <dmacmgr/dmac_regs.h>

#define MG_ERROR_ARGUMENT	((int)0x800f1616)
#define MG_ERROR_UNSUPPORTED	((int)0x800f1625)
#define MG_ERROR_OPERATION	((int)0x800f1628)
#define NOINLINE		__attribute__((noinline))

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

/* name unknown: map the DMAC5 key-register page. */
static int NOINLINE MapKeyRegisters(void)
{
	SceKernelAllocMemBlockKernelOpt opt;
	void *base;
	SceUID uid;

	memset(&opt, 0, sizeof(opt));
	opt.size = sizeof(opt);
	opt.attr = 2;
	opt.paddr = DMAC5_KEYRING_REG_BASE;
	uid = SceSysmemForDriver_C94850C9("SceSblDMAC5DmacKRBase", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_IO_RW,
		DMAC_REG_SIZE, &opt);
	if (uid < 0)
		return MG_ERROR_OPERATION;
	SceSysmemForDriver_A841EDDA(uid, &base);
	s_keyring = base;
	return 0;
}

/* name unknown: start mgkm_sm with the original authorization request. */
static int NOINLINE StartSecureModule(void)
{
	SceAuthInfo auth;
	int ret;

	memset(&auth, 0, sizeof(auth));
	memcpy(&auth.request, s_request, sizeof(s_request));
	ret = sceSblACMgrGetMediaType("os0:sm/mgkm_sm.self", &auth.media_type);
	if (ret < 0)
		return ret;
	auth.self_type = (auth.self_type & ~0xfU) | 2;
	return SceSblSmCommForKernel_7863A0CC(0, "os0:sm/mgkm_sm.self", 0,
		&auth, &s_smId);
}

/* The secure-module command payload is intentionally not initialized by Sony. */
static int NOINLINE call_set_key_command(unsigned command)
{
	SceSblSmCommPair stopResult;
	uint32_t result = 0;
	uint8_t data[0x30];
	int ret;

	ret = StartSecureModule();
	if (ret != 0)
		return ret;
	ret = SceSblSmCommForKernel_DB9FC204(s_smId, command, &result, data, sizeof(data));
	if (ret != 0)
		result = MG_ERROR_OPERATION;
	ret = SceSblSmCommForKernel_0631F8ED(s_smId, &stopResult);
	return ret != 0 ? MG_ERROR_OPERATION : (int)result;
}

/* name unknown: stage an aligned AES block before calling DMAC5. */
static int NOINLINE AesTransform(const void *input, void *output,
	unsigned size, unsigned slot, unsigned encrypt)
{
	uint8_t plain[32] __attribute__((aligned(16)));
	uint8_t cipher[32] __attribute__((aligned(16)));
	void *staged;
	int ret;

	if (size > 32 || encrypt > 1)
		return MG_ERROR_ARGUMENT;
	staged = __memcpy_chk(plain, input, size, sizeof(plain));
	if (encrypt)
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

/* name unknown: the DES stage copies words, without a memcpy import. */
static int NOINLINE DesTransform(const uint32_t *input, uint32_t *output,
	unsigned slot, unsigned encrypt)
{
	uint32_t plain[4] __attribute__((aligned(16)));
	uint32_t cipher[4] __attribute__((aligned(16)));
	int ret;

	if (encrypt > 1)
		return MG_ERROR_ARGUMENT;
	plain[0] = input[0];
	plain[1] = input[1];
	plain[2] = input[2];
	plain[3] = input[3];
	if (encrypt)
		ret = SceSblSsMgrForDriver_37DD5CBF(plain, cipher, 16,
			slot, 0xc0, 1);
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

static int ValidInputs(const void *first, const void *second, const void *third)
{
	return first && second && third;
}

/* Two identical halves form the 32-byte wrapped key. */
static int NOINLINE InstallDesKey(const uint32_t *key)
{
	uint32_t wrapped[8];

	for (unsigned i = 0; i < 4; i++) {
		wrapped[i] = key[i];
		wrapped[i + 4] = wrapped[i];
	}
	return AesTransform(wrapped, (void *)s_keyring->slot[0x1d].words, 32, 0x1e, 0);
}

/* name unknown: decode two keys, xor them, and install keyslot 0x1d. */
static int NOINLINE InstallXorKey(const void *first, const void *second)
{
	uint32_t left[4], right[4];
	int ret;

	ret = AesTransform(first, left, 16, 0x1e, 0);
	if (ret != 0)
		return ret;
	ret = AesTransform(second, right, 16, 0x1e, 0);
	if (ret != 0)
		return ret;
	for (unsigned i = 0; i < 4; i++)
		right[i] ^= left[i];
	for (unsigned i = 0; i < 4; i++)
		s_keyring->slot[0x1d].words[i] = right[i];
	return 0;
}

int SceSblMgKeyMgrForDriver_B6AC3AE1(const void *input, void *output)
{
	return (uintptr_t)input == 0 || !output ? MG_ERROR_ARGUMENT : MG_ERROR_UNSUPPORTED;
}

static int NOINLINE DesOperation(const uint32_t *key, const void *input,
	void *output, unsigned encrypt)
{
	int ret;

	if (!ValidInputs(key, input, output))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = call_set_key_command(2);
	if (ret == 0)
		ret = InstallDesKey(key);
	if (ret == 0)
		ret = DesTransform(input, output, 0x1d, encrypt);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_20B66ADA(const uint32_t *key, const void *input, void *output)
{
	return DesOperation(key, input, output, 1);
}

int SceSblMgKeyMgrForDriver_5FECCE03(const uint32_t *key, const void *input, void *output)
{
	return DesOperation(key, input, output, 0);
}

static int NOINLINE XorKeyOperation(const void *first, const void *second,
	const void *input, void *output, unsigned encrypt)
{
	int ret;

	if (!ValidInputs(first, second, output) || !input)
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = call_set_key_command(2);
	if (ret == 0)
		ret = InstallXorKey(first, second);
	if (ret == 0)
		ret = AesTransform(input, output, 16, 0x1d, encrypt);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_EA534495(const void *first, const void *second,
	const void *input, void *output)
{
	return XorKeyOperation(first, second, input, output, 1);
}

int SceSblMgKeyMgrForDriver_DF3B808D(const void *first, const void *second,
	const void *input, void *output)
{
	return XorKeyOperation(first, second, input, output, 0);
}

/* name unknown: translate between DES and the wrapping AES keyslot. */
static int NOINLINE TranslateDes(const uint32_t *key, void *wrapped,
	void *plain, unsigned decrypt)
{
	uint32_t intermediate[4];
	int ret;

	if (!ValidInputs(key, wrapped, plain))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = call_set_key_command(2);
	if (ret == 0)
		ret = InstallDesKey(key);
	if (ret == 0) {
		if (decrypt)
			ret = AesTransform(wrapped, intermediate, 16, 0x1e, 0);
		else
			ret = DesTransform(plain, intermediate, 0x1d, 1);
	}
	if (ret == 0) {
		if (decrypt)
			ret = DesTransform(intermediate, plain, 0x1d, 1);
		else
			ret = AesTransform(intermediate, wrapped, 16, 0x1e, 1);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_BC7D86EE(const uint32_t *key, void *wrapped, void *plain)
{
	return TranslateDes(key, wrapped, plain, 0);
}

int SceSblMgKeyMgrForDriver_D37C3DAE(const uint32_t *key, void *wrapped, void *plain)
{
	int ret;
	uint32_t intermediate[4];

	if (!ValidInputs(key, wrapped, plain))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = call_set_key_command(2);
	if (ret == 0)
		ret = InstallDesKey(key);
	if (ret == 0)
		ret = DesTransform(plain, intermediate, 0x1d, 0);
	if (ret == 0)
		ret = AesTransform(intermediate, wrapped, 16, 0x1e, 1);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_4C82A466(const uint32_t *key, void *wrapped, void *plain)
{
	return TranslateDes(key, wrapped, plain, 1);
}

/* name unknown: translate a block using a decoded 128-bit AES key. */
static int NOINLINE TranslateAes(const void *key, void *wrapped, void *plain,
	unsigned operation)
{
	uint8_t intermediate[16];
	int ret;

	if (!ValidInputs(key, wrapped, plain))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = call_set_key_command(2);
	if (ret == 0)
		ret = AesTransform(key, (void *)s_keyring->slot[0x1d].words, 16, 0x1e, 0);
	if (ret == 0) {
		if (operation == 1)
			ret = AesTransform(wrapped, intermediate, 16, 0x1e, 0);
		else
			ret = AesTransform(plain, intermediate, 16, 0x1d, operation == 0);
	}
	if (ret == 0) {
		if (operation == 1)
			ret = AesTransform(intermediate, plain, 16, 0x1d, 1);
		else
			ret = AesTransform(intermediate, wrapped, 16, 0x1e, 1);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_8CFCAA52(const void *key, void *wrapped, void *plain)
{
	return TranslateAes(key, wrapped, plain, 0);
}

int SceSblMgKeyMgrForDriver_F8F856FC(const void *key, void *wrapped, void *plain)
{
	return TranslateAes(key, wrapped, plain, 1);
}

int SceSblMgKeyMgrForDriver_8ECFFD7A(const void *key, void *wrapped, void *plain)
{
	return TranslateAes(key, wrapped, plain, 2);
}

int SceSblMgKeyMgrForDriver_71FA16B0(const void *first, void *second)
{
	uint8_t left[16], right[16];
	int ret;

	if (!ValidInputs(first, second, second))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = call_set_key_command(2);
	if (ret == 0)
		ret = AesTransform(first, left, 16, 0x1e, 0);
	if (ret == 0)
		ret = AesTransform(second, right, 16, 0x1e, 0);
	if (ret == 0) {
		for (unsigned i = 0; i < 16; i++)
			right[i] ^= left[i];
		ret = AesTransform(right, second, 16, 0x1e, 1);
	}
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

static int NOINLINE RewrapKey(void *first, void *second, unsigned decrypt)
{
	uint8_t intermediate[16];
	int ret;

	if (!ValidInputs(first, second, second))
		return MG_ERROR_ARGUMENT;
	SceKernelSuspendForDriver_4DF40893(0);
	ret = call_set_key_command(1);
	if (ret == 0)
		ret = call_set_key_command(2);
	if (ret == 0)
		ret = AesTransform(decrypt ? second : first, intermediate, 16,
			decrypt ? 0x1f : 0x1e, 0);
	if (ret == 0)
		ret = AesTransform(intermediate, decrypt ? first : second, 16,
			decrypt ? 0x1e : 0x1f, 1);
	SceKernelSuspendForDriver_2BB92967(0);
	return ret;
}

int SceSblMgKeyMgrForDriver_1C5388D0(void *first, void *second)
{
	return RewrapKey(first, second, 0);
}

int SceSblMgKeyMgrForDriver_1263FB04(void *first, void *second)
{
	return RewrapKey(first, second, 1);
}

int module_start(unsigned args, void *argp)
{
	MapKeyRegisters();
	return 0;
}

int module_stop(unsigned args, void *argp)
{
	return 0;
}
