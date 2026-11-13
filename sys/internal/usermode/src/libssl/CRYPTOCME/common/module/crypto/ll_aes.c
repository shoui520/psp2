/* Filename and descriptive names are ours: CRYPTOCME's AES in CBC mode on
 * the SDK's libaes.
 */
#include <libaes.h>
#include "ll_ciph.h"
#include "../../object.h"
#include "../../../../memory.h"

#define SSL_AES_BLOCK_SIZE 16

/* The state of AES in CBC mode: the key schedule, then the chaining
 * value, with room to align it
 */
/* The chaining value, indexed from the schedule */
#define SSL_AES_CBC_CHAIN(schedule, i) \
	(((unsigned char *)(schedule))[sizeof(SceAesContext) + (i)])
#define SSL_AES_CBC_STATE_SIZE \
	(sizeof(SceAesContext) + SSL_AES_BLOCK_SIZE + 16)

int sslAesEncryptBlock(void);
int sslAesDecryptBlock(void);
int sslSetAesCbcKey(SslCipherCore *core, const void *key,
		    const unsigned char *iv, int flags);
int sslProcessAesCbc(SslCipherCore *core, unsigned char *output,
		     const unsigned char *input, SceUInt32 length);

/* The CBC engine is never called: the process function does it all */
static const void *const sslAesCbcEngine[2] = {
	(const void *)sslAesEncryptBlock,
	(const void *)sslAesDecryptBlock
};

static const unsigned char sslAes128CbcOid[] = {
	0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x02
};

static const unsigned char sslAes256CbcOid[] = {
	0x06, 0x09, 0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x01, 0x2a
};

static const SceUInt32 sslAesCbcStateSchema[] = {
	0x00406042, 0x04003003, 0x04002003, 0x0040c0da,
	0x004080ca, 0x0000a004, 0x00000000
};

SslCipherDescriptor sslAes128CbcCipherDescriptor = {
	"sceAesCbc128",
	{ sizeof(sslAes128CbcOid), sslAes128CbcOid },
	32, 16, 16, SSL_AES_BLOCK_SIZE, SSL_AES_BLOCK_SIZE,
	SSL_AES_CBC_STATE_SIZE, 0, 1,
	sslSetAesCbcKey,
	sslProcessAesCbc,
	NULL,
	NULL,
	SSL_CIPHER_SAVABLE | SSL_CIPHER_CHAINED,
	sslAesCbcEngine,
	sslAesCbcStateSchema,
	NULL
};

SslCipherDescriptor sslAes256CbcCipherDescriptor = {
	"sceAesCbc256",
	{ sizeof(sslAes256CbcOid), sslAes256CbcOid },
	32, 32, 32, SSL_AES_BLOCK_SIZE, SSL_AES_BLOCK_SIZE,
	SSL_AES_CBC_STATE_SIZE, 0, 1,
	sslSetAesCbcKey,
	sslProcessAesCbc,
	NULL,
	NULL,
	SSL_CIPHER_SAVABLE | SSL_CIPHER_CHAINED,
	sslAesCbcEngine,
	sslAesCbcStateSchema,
	NULL
};

int sslAesEncryptBlock(void)
{
	return 0;
}

int sslAesDecryptBlock(void)
{
	return 0;
}

int sslSetAesCbcKey(SslCipherCore *core, const void *key,
		    const unsigned char *iv, int flags)
{
	SceUInt32 keyLength = core->keyLength;
	SceAesContext *schedule = core->state;
	unsigned char *chain;
	unsigned short encrypt;
	SceUInt32 i;
	int ret;

	if (keyLength != 16 && keyLength != 24 && keyLength != 32)
		return SSL_CRYPTO_ERROR_NOT_INITIALIZED;
	chain = (unsigned char *)(schedule + 1);
	core->schedule = schedule;
	core->iv = chain;
	encrypt = flags & SSL_CIPHER_ENCRYPT;
	if (key != NULL)
		core->encrypt = 1;
	if (core->encrypt != encrypt)
		core->encrypt = encrypt;
	ret = sceAesScheduleKey(schedule, 128, keyLength * 8, key);
	if (ret < 0)
		return ret;
	for (i = 0; i < schedule->blockWords * 4; i++)
		SSL_AES_CBC_CHAIN(schedule, i) = iv[i];
	return 0;
}

int sslProcessAesCbc(SslCipherCore *core, unsigned char *output,
		     const unsigned char *input, SceUInt32 length)
{
	unsigned char block[SSL_AES_BLOCK_SIZE] __attribute__((aligned(16)));
	unsigned char result[SSL_AES_BLOCK_SIZE] __attribute__((aligned(16)));
	unsigned char data[SSL_AES_BLOCK_SIZE] __attribute__((aligned(16)));
	unsigned char next[SSL_AES_BLOCK_SIZE] __attribute__((aligned(16)));
	SceAesContext *schedule;
	unsigned char *chain;
	SceUInt32 offset;
	SceUInt32 i;
	int ret = 0;

	if (core->encrypt) {
		for (offset = ret; offset < length; offset += 16) {
			sslCopyMemory(data, input, SSL_AES_BLOCK_SIZE);
			schedule = core->state;
			chain = (unsigned char *)(schedule + 1);
			for (i = ret; i < schedule->blockWords * 4; i++)
				block[i] = data[i] ^
					   SSL_AES_CBC_CHAIN(schedule, i);
			sceAesEncrypt(schedule, block, next);
			for (i = ret; i < schedule->blockWords * 4; i++)
				SSL_AES_CBC_CHAIN(schedule, i) = next[i];
			sslCopyMemory(output, next, SSL_AES_BLOCK_SIZE);
			input += 16;
			output += 16;
		}
	} else {
		for (offset = ret; offset < length; offset += 16) {
			sslCopyMemory(data, input, SSL_AES_BLOCK_SIZE);
			schedule = core->state;
			chain = (unsigned char *)(schedule + 1);
			sceAesDecrypt(schedule, data, result);
			for (i = 0; i < SSL_AES_BLOCK_SIZE; i++)
				next[i] = result[i] ^ chain[i];
			for (i = 0; i < SSL_AES_BLOCK_SIZE; i++)
				chain[i] = data[i];
			sslCopyMemory(output, next, SSL_AES_BLOCK_SIZE);
			input += 16;
			output += 16;
		}
	}
	return ret;
}

const SslCipherDescriptor *sslGetAes128CbcCipherDescriptor(void)
{
	return &sslAes128CbcCipherDescriptor;
}
