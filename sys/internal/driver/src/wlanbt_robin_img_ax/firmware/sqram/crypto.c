/*
 * HostCmd 0x78 (HostCmd_CMD_802_11_CRYPTO, 0xc00078d6 .. 0xc000817c): the host hands the
 * firmware a key, the cipher's parameters and some data, and one of the firmware's ciphers runs
 * over the data in place: RC4, one AES block, the AES key wrap of RFC 3394, AES-CCM on the MAC's
 * CCM engine, and WAPI's SMS4 on the same engine, which runs a number of rounds over a test
 * pattern instead of the data. hostCmd0078Parse() checks the command, and hostCmdProcess() calls
 * the handler of the direction and the algorithm. Every function has a patch slot. The file
 * name is inferred.
 */
#include <stddef.h>
#include <stdint.h>
#include "robin/buffer.h"
#include "robin/btrom.h"
#include "robin/ccm.h"
#include "robin/esa.h"
#include "robin/hostcmd.h"
#include "robin/patch.h"
#include "robin/rom.h"
#include "robin/soc.h"
#include "robin/sqram.h"
#include "robin/wep.h"

static ROBIN_PATCH_SLOT(wlanWapiRunEngineOverride);		/* 0x55b54 */
static ROBIN_PATCH_SLOT(hostCmd0078WapiEncryptOverride);	/* 0x55b58 */
static ROBIN_PATCH_SLOT(hostCmd0078WapiDecryptOverride);	/* 0x55b5c */
static ROBIN_PATCH_SLOT(hostCmd0078AesEncryptOverride);	/* 0x55b60 */
static ROBIN_PATCH_SLOT(hostCmd0078AesDecryptOverride);	/* 0x55b64 */
static ROBIN_PATCH_SLOT(hostCmd0078AesWrapOverride);		/* 0x55b68 */
static ROBIN_PATCH_SLOT(hostCmd0078AesUnwrapOverride);		/* 0x55b6c */
static ROBIN_PATCH_SLOT(hostCmd0078AesCcmOverride);		/* 0x55b70 */
static ROBIN_PATCH_SLOT(hostCmd0078ParseOverride);		/* 0x55b78 */
static ROBIN_PATCH_SLOT(hostCmd0078Rc4Override);		/* 0x55b7c */

/*
 * What the WAPI test runs with, set by hostCmd0078HooksInit(): the pool of its data buffers,
 * the rounds it runs, and the functions that take the engine and give it back.
 */
struct OsaBufferPool *ramHook_cryptoTestPool __attribute__((section(".itcm.rw")));	/* 0x548ac */
uint32_t ramHook_cryptoTestRounds __attribute__((section(".itcm.rw")));		/* 0x548b0 */
int (*ramHook_cryptoTestEnter)(void) __attribute__((section(".itcm.rw")));		/* 0x548b4 */
void (*ramHook_cryptoTestLeave)(int lock) __attribute__((section(".itcm.rw")));		/* 0x548b8 */

/* Where the key wrap leaves its result before it goes back over the data (36 bytes, 0x55ff4). */
static uint8_t cryptoWrapScratch[36] __attribute__((section(".itcm.bss")));

static inline int aesKeyLengthValid(unsigned int length)
{
	return length == 16 || length == 24 || length == 32;
}

/*
 * Checks the command and fills in the request: the bits of status say what is wrong, and
 * lengthChange is how much longer the key wrap makes the data (-8 for the unwrap).
 */
int hostCmd0078Parse(uint32_t *status, uint8_t *body, struct HostCmdCryptoRequest *request,
		     int8_t *lengthChange)
{
	struct HostCmdCrypto *command = (struct HostCmdCrypto *)body;
	struct HostCmdCryptoAesCcm *ccm = (struct HostCmdCryptoAesCcm *)body;
	struct HostCmdCryptoWapi *wapi = (struct HostCmdCryptoWapi *)body;
	int result;

	if (hostCmd0078ParseOverride(status, body, request, lengthChange, &result) != 0)
		return result;
	if (status == NULL || body == NULL)
		return -1;
	if (command->encdec != HOSTCMD_CRYPTO_DECRYPT && command->encdec != HOSTCMD_CRYPTO_ENCRYPT)
		*status |= HOSTCMD_CRYPTO_BAD_DIRECTION;
	if (command->encdec == HOSTCMD_CRYPTO_DECRYPT)
		request->encdec = HOSTCMD_CRYPTO_DECRYPT;
	else if (command->encdec == HOSTCMD_CRYPTO_ENCRYPT)
		request->encdec = HOSTCMD_CRYPTO_ENCRYPT;

	switch (command->algorithm) {
	case HOSTCMD_CRYPTO_RC4:
		if ((unsigned int)(command->keyIvLength + command->keyLength - 1) > 0xff)
			*status |= HOSTCMD_CRYPTO_BAD_RC4_LENGTH;
		break;
	case HOSTCMD_CRYPTO_AES:
		if (!aesKeyLengthValid(command->keyLength))
			*status |= HOSTCMD_CRYPTO_BAD_KEY;
		if (command->data.length != 16)
			*status |= HOSTCMD_CRYPTO_BAD_DATA;
		break;
	case HOSTCMD_CRYPTO_AES_KEY_WRAP:
		if (command->keyIvLength != 8)
			*status |= HOSTCMD_CRYPTO_BAD_IV;
		if (!aesKeyLengthValid(command->keyLength))
			*status |= HOSTCMD_CRYPTO_BAD_KEY;
		if ((unsigned int)(command->data.length - 8) >= 0x3f1)
			*status |= HOSTCMD_CRYPTO_BAD_DATA;
		if (command->encdec == HOSTCMD_CRYPTO_ENCRYPT)
			*lengthChange = 8;
		else if (command->encdec == HOSTCMD_CRYPTO_DECRYPT)
			*lengthChange = -8;
		break;
	case HOSTCMD_CRYPTO_AES_CCM:
		if (!aesKeyLengthValid(ccm->keyLength))
			*status |= HOSTCMD_CRYPTO_BAD_KEY;
		if ((unsigned int)(ccm->nonceLength - 7) >= 7)
			*status |= HOSTCMD_CRYPTO_BAD_NONCE;
		if (ccm->aadLength > 30)
			*status |= HOSTCMD_CRYPTO_BAD_AAD;
		if (ccm->data.length > 40)
			*status |= HOSTCMD_CRYPTO_BAD_DATA;
		break;
	case HOSTCMD_CRYPTO_WAPI:
		if (wapi->keyLength != 32)
			*status |= HOSTCMD_CRYPTO_BAD_KEY;
		if (wapi->packetNumberLength != 16)
			*status |= HOSTCMD_CRYPTO_BAD_NONCE;
		if (wapi->aadLength != 32 && wapi->aadLength != 48)
			*status |= HOSTCMD_CRYPTO_BAD_AAD;
		break;
	default:
		*status |= HOSTCMD_CRYPTO_BAD_ALGORITHM;
		break;
	}
	request->body = body;
	return 0;
}

/* Encrypts the one AES block of the data in place (0xc0007abe). */
static void hostCmd0078AesEncrypt(struct HostCmdCryptoRequest *request, int *failed)
{
	struct HostCmdCrypto *command;

	if (hostCmd0078AesEncryptOverride(request, failed) != 0)
		return;
	if (failed == NULL || request == NULL) {
		*failed = -1;
		return;
	}
	*failed = 0;
	if (request->encdec != HOSTCMD_CRYPTO_ENCRYPT) {
		*failed = -1;
		return;
	}
	command = (struct HostCmdCrypto *)request->body;
	if (MRVL_AesEncrypt(command->key, (uint8_t)command->keyLength >> 3, command->data.data,
			    command->data.data) == -1)
		*failed = -1;
}

/* Decrypts the one AES block of the data in place, with the ROM's AES (0xc0007b0a). */
static void hostCmd0078AesDecrypt(struct HostCmdCryptoRequest *request, int *failed)
{
	struct HostCmdCrypto *command;
	unsigned int keyLength;
	OsaBuffer *buffer;
	uint8_t *context;

	if (hostCmd0078AesDecryptOverride(request, failed) != 0)
		return;
	if (failed == NULL || request == NULL || request->encdec != HOSTCMD_CRYPTO_DECRYPT) {
		*failed = -1;
		return;
	}
	command = (struct HostCmdCrypto *)request->body;
	keyLength = (uint8_t)command->keyLength;
	buffer = osa_BufferAlloc(ramHook_encrPoolConfig, 400, 0xffffffff);
	context = (uint8_t *)buffer + buffer->dataOffset;
	romAesKeySet(context, command->key, keyLength * 8, 0);
	romAesDecryptBlock(context, command->data.data, command->data.data);
	osa_BufferFree(buffer);
	*failed = 0;
}

/* Wraps the data with the key and the initial value of the command; the data grows by 8 (0xc0007b7e). */
static void hostCmd0078AesWrap(struct HostCmdCryptoRequest *request, int *failed)
{
	struct HostCmdCrypto *command;
	unsigned int length;

	if (hostCmd0078AesWrapOverride(request, failed) != 0)
		return;
	if (failed == NULL || request == NULL) {
		*failed = -1;
		return;
	}
	*failed = 0;
	if (request->encdec != HOSTCMD_CRYPTO_ENCRYPT) {
		*failed = -1;
		return;
	}
	command = (struct HostCmdCrypto *)request->body;
	length = command->data.length;
	command->data.length = length + 8;
	if (MRVL_AesWrap(command->key, (uint8_t)command->keyLength >> 3, length >> 3, command->data.data,
			 command->keyIv, cryptoWrapScratch) == -1) {
		*failed = -2;
		return;
	}
	sqramMemmove(command->data.data, cryptoWrapScratch, command->data.length);
}

/* Unwraps the data; it shrinks by 8 (0xc0007bfe). -2 when the initial value does not come out. */
static void hostCmd0078AesUnwrap(struct HostCmdCryptoRequest *request, int *failed)
{
	struct HostCmdCrypto *command;
	unsigned int length;

	if (hostCmd0078AesUnwrapOverride(request, failed) != 0)
		return;
	if (failed == NULL || request == NULL) {
		*failed = -1;
		return;
	}
	*failed = 0;
	if (request->encdec != HOSTCMD_CRYPTO_DECRYPT) {
		*failed = -1;
		return;
	}
	command = (struct HostCmdCrypto *)request->body;
	length = command->data.length;
	if (MRVL_AesUnWrap(command->key, (uint8_t)command->keyLength >> 3, (length >> 3) - 1,
			   command->data.data, command->keyIv, cryptoWrapScratch) == -1) {
		*failed = -2;
		return;
	}
	command->data.length = length - 8;
	sqramMemmove(command->data.data, cryptoWrapScratch, length - 8);
}

/* Copies a 16-byte block into the engine's registers (0xc000816a). */
static __attribute__((noinline)) void ccmCopyBlock(volatile uint32_t *destination, const uint32_t *source)
{
	destination[0] = source[0];
	destination[1] = source[1];
	destination[2] = source[2];
	destination[3] = source[3];
}

/*
 * Runs AES-CCM over the data in place on the engine, with an 8-byte MIC after the data: B0 from
 * the nonce and the length, the AAD in the next two blocks (0xc0007c7a). Decrypting, the data
 * comes with its MIC, and a MIC that does not check sets *failed; encrypting, the data grows
 * by the MIC.
 */
static void hostCmd0078AesCcm(struct HostCmdCryptoRequest *request, int *failed, int decrypt)
{
	struct HostCmdCryptoAesCcm *command = (struct HostCmdCryptoAesCcm *)request->body;
	uint32_t micLength = 0;
	uint32_t block[16];
	uint8_t *bytes = (uint8_t *)block;
	unsigned int length;
	uint32_t status;
	int lock;

	if (hostCmd0078AesCcmOverride(request, failed, decrypt) != 0)
		return;
	lock = ramHook_cryptoTestEnter();
	if (decrypt) {
		command->data.length -= 8;
		micLength = 8;
	}
	CRYPTO_MODE = 0;
	sqramMemmove(bytes, command->key, command->keyLength);
	ccmCopyBlock(CCM_KEY(0), block);
	CCM_MODE(0) = (command->data.length << 16) | micLength;
	wlanCcmHookStart();

	__aeabi_memclr4(block, sizeof(block));
	sqramMemmove(bytes + 1, command->nonce, command->nonceLength);
	bytes[0] = 0x59;
	length = command->data.length;
	*(uint16_t *)(bytes + 14) = (uint16_t)((length << 8) | (length >> 8));
	ccmCopyBlock(CCM_BLOCK0(0), block);

	__aeabi_memclr4(block, sizeof(block));
	bytes[0] = 0;
	bytes[1] = (uint8_t)command->aadLength;
	sqramMemmove(bytes + 2, command->aad, command->aadLength);
	ccmCopyBlock(CCM_BLOCK1(0), block);
	ccmCopyBlock((volatile uint32_t *)CCM_BLOCK2(0), block + 4);

	__aeabi_memclr4(block, sizeof(block));
	sqramMemmove(bytes + 1, command->nonce, command->nonceLength);
	CCM_SOURCE(0) = (uint32_t)(uintptr_t)command->data.data;
	CCM_DESTINATION(0) = (uint32_t)(uintptr_t)command->data.data;
	CRYPTO_MODE |= CRYPTO_MODE_CCM_ENGINE;
	CCM_CONTROL(0) |= CCM_CONTROL_START;
	status = wlanCcmWaitClear(0x3000, 0);
	CCM_CONTROL(0) &= ~CCM_CONTROL_INPUT_PENDING;
	if (!decrypt)
		command->data.length += 8;
	else if (status & CCM_CONTROL_FAILED)
		*failed = 1;
	ramHook_cryptoTestLeave(lock);
}

static void hostCmd0078AesCcmEncrypt(struct HostCmdCryptoRequest *request, int *failed)
{
	hostCmd0078AesCcm(request, failed, 0);
}

static void hostCmd0078AesCcmDecrypt(struct HostCmdCryptoRequest *request, int *failed)
{
	hostCmd0078AesCcm(request, failed, 1);
}

/* Runs RC4 over the data with the key and the IV of the command (0xc0007df6). */
static void hostCmd0078Rc4(struct HostCmdCryptoRequest *request, int *failed)
{
	struct HostCmdCrypto *command;

	if (hostCmd0078Rc4Override(request, failed) != 0)
		return;
	if (failed == NULL || request == NULL) {
		*failed = -1;
		return;
	}
	*failed = 0;
	if (request->encdec != HOSTCMD_CRYPTO_DECRYPT && request->encdec != HOSTCMD_CRYPTO_ENCRYPT) {
		*failed = -2;
		return;
	}
	command = (struct HostCmdCrypto *)request->body;
	RC4_Encrypt(command->key, command->keyIv, command->keyIvLength, command->data.data,
		    command->data.length, 0);
}

/*
 * Loads the engine for WPI-SMS4, starts it and waits until it is done: wlanWapiLoadEngine()
 * (wlan/wapi.c) without the interrupt, on block 0 (0xc0007e56).
 */
static int wlanWapiRunEngine(int decrypt, int qos, const uint8_t *packetNumber, const uint8_t *aad,
			     void *source, unsigned int length, const uint8_t *dataKey,
			     const uint8_t *micKey, void *destination)
{
	uint32_t mode = 0x40;
	int i;

	/* the hook's result and the loop's index are one variable (Sony's frame slot) */
	if (wlanWapiRunEngineOverride(decrypt, qos, packetNumber, aad, source, length, dataKey, micKey,
				      destination, &i) != 0)
		return i;
	for (i = 0; i < 16; i++) {
		((volatile uint8_t *)CCM_KEY(0))[i] = micKey[i];
		((volatile uint8_t *)CCM_KEY2(0))[i] = dataKey[i];
		((volatile uint8_t *)CCM_BLOCK0(0))[i] = packetNumber[i];
		((volatile uint8_t *)CCM_BLOCK1(0))[i] = aad[i];
		CCM_BLOCK2(0)[i] = aad[i + 16];
		if (qos)
			CCM_BLOCK3(0)[i] = aad[i + 32];
	}
	CCM_SOURCE(0) = (uint32_t)(uintptr_t)source;
	CCM_DESTINATION(0) = (uint32_t)(uintptr_t)destination;
	if (qos)
		mode |= 0x80;
	if (decrypt == 1)
		mode |= 8;
	CCM_MODE(0) = (length << 16) | mode;
	CRYPTO_MODE |= CRYPTO_MODE_CCM_ENGINE;
	CCM_CONTROL(0) |= CCM_CONTROL_START;
	wlanCcmWaitSet(0x3000, 0);
	return 0;
}

/* Encrypts a test pattern (1, 2, 3, ...) of the command's length, ramHook_cryptoTestRounds times (0xc0007f34). */
static void hostCmd0078WapiEncrypt(struct HostCmdCryptoRequest *request, int *failed)
{
	struct HostCmdCryptoWapi *command = (struct HostCmdCryptoWapi *)request->body;
	uint8_t key[32];
	uint8_t aad[48];
	uint8_t packetNumber[16];
	OsaBuffer *buffer;
	uint8_t *data;
	unsigned int round;
	int qos;
	int lock;
	int i;

	if (hostCmd0078WapiEncryptOverride(request, failed) != 0)
		return;
	buffer = osa_BufferAlloc(ramHook_cryptoTestPool, 0, 0);
	if (buffer == NULL)
		return;
	data = (uint8_t *)buffer + buffer->dataOffset;
	qos = command->aadLength == 48;
	for (i = 0; command->dataLength > i; i++)
		data[i] = i + 1;
	__aeabi_memcpy(packetNumber, command->packetNumber, command->packetNumberLength);
	__aeabi_memcpy(aad, command->aad, command->aadLength);
	__aeabi_memcpy(key, command->key, command->keyLength);
	lock = ramHook_cryptoTestEnter();
	SOC_IRQ_ENABLE_CLEAR = SOC_IRQ_CCM_ENGINE;
	wlanCcmInputRelease();
	for (round = 0; round < ramHook_cryptoTestRounds; round++)
		wlanWapiRunEngine(0, qos, packetNumber, aad, data, command->dataLength,
				  key, key + 16, data);
	wlanCcmInputHold();
	SOC_IRQ_ENABLE_SET = SOC_IRQ_CCM_ENGINE;
	ramHook_cryptoTestLeave(lock);
	osa_BufferFree(buffer);
}

/*
 * Encrypts the test pattern once, then decrypts it into a second buffer
 * ramHook_cryptoTestRounds times (0xc0008028).
 */
static void hostCmd0078WapiDecrypt(struct HostCmdCryptoRequest *request, int *failed)
{
	struct HostCmdCryptoWapi *command = (struct HostCmdCryptoWapi *)request->body;
	uint8_t key[32];
	uint8_t aad[48];
	uint8_t packetNumber[16];
	OsaBuffer *plainBuffer;
	OsaBuffer *outputBuffer;
	uint8_t *plain;
	uint8_t *output;
	unsigned int round;
	int qos;
	int lock;
	int i;

	if (hostCmd0078WapiDecryptOverride(request, failed) != 0)
		return;
	plainBuffer = osa_BufferAlloc(ramHook_cryptoTestPool, 0, 0);
	if (plainBuffer == NULL)
		return;
	outputBuffer = osa_BufferAlloc(ramHook_cryptoTestPool, 0, 0);
	if (outputBuffer == NULL) {
		osa_BufferFree(plainBuffer);
		return;
	}
	plain = (uint8_t *)plainBuffer + plainBuffer->dataOffset;
	output = (uint8_t *)outputBuffer + outputBuffer->dataOffset;
	qos = command->aadLength == 48;
	for (i = 0; command->dataLength > i; i++)
		plain[i] = i + 1;
	__aeabi_memcpy(packetNumber, command->packetNumber, command->packetNumberLength);
	__aeabi_memcpy(aad, command->aad, command->aadLength);
	__aeabi_memcpy(key, command->key, command->keyLength);
	lock = ramHook_cryptoTestEnter();
	SOC_IRQ_ENABLE_CLEAR = SOC_IRQ_CCM_ENGINE;
	wlanCcmInputRelease();
	wlanWapiRunEngine(0, qos, packetNumber, aad, plain, command->dataLength,
			  key, key + 16, plain);
	for (round = 0; round < ramHook_cryptoTestRounds; round++)
		wlanWapiRunEngine(1, qos, packetNumber, aad, plain, command->dataLength,
				  key, key + 16, output);
	wlanCcmInputHold();
	SOC_IRQ_ENABLE_SET = SOC_IRQ_CCM_ENGINE;
	ramHook_cryptoTestLeave(lock);
	osa_BufferFree(plainBuffer);
	osa_BufferFree(outputBuffer);
}

/* The handler of each direction and algorithm (0xc0011f34). */
struct HostCmdCryptoEntry hostCmd0078Handlers[2][HOSTCMD_CRYPTO_ALGORITHMS]
	__attribute__((section(".sqram1.rw"))) = {
	{
		{ HOSTCMD_CRYPTO_DECRYPT, NULL },
		{ HOSTCMD_CRYPTO_DECRYPT, hostCmd0078Rc4 },
		{ HOSTCMD_CRYPTO_DECRYPT, hostCmd0078AesDecrypt },
		{ HOSTCMD_CRYPTO_DECRYPT, hostCmd0078AesUnwrap },
		{ HOSTCMD_CRYPTO_DECRYPT, hostCmd0078AesCcmDecrypt },
		{ HOSTCMD_CRYPTO_DECRYPT, hostCmd0078WapiDecrypt },
	},
	{
		{ HOSTCMD_CRYPTO_ENCRYPT, NULL },
		{ HOSTCMD_CRYPTO_ENCRYPT, hostCmd0078Rc4 },
		{ HOSTCMD_CRYPTO_ENCRYPT, hostCmd0078AesEncrypt },
		{ HOSTCMD_CRYPTO_ENCRYPT, hostCmd0078AesWrap },
		{ HOSTCMD_CRYPTO_ENCRYPT, hostCmd0078AesCcmEncrypt },
		{ HOSTCMD_CRYPTO_ENCRYPT, hostCmd0078WapiEncrypt },
	},
};
