/*
 * The connection's encryption: a random AES-128 session key, sent to the PS4 under its RSA
 * public key (OAEP with SHA-1), and AES-CBC in both directions with an IV for each, which the
 * PS4's hello sets.
 */
#include <libkernel/libkernel.h>
#include <libaes.h>
#include "companion_util_internal.h"
#include "rsa_oaep.h"

namespace SceCompanionUtil {

#define AES_BLOCK_SIZE	16

/* the PS4's public key, RSA-2048, least significant word first */
static unsigned int s_modulus[64] = {
	0xd1d0a6b3, 0xc34bed46, 0x4ddabf43, 0x738d2a82, 0xf9564630, 0x841a315d, 0x8cf633d5, 0xfac459b4,
	0x80a2c4c3, 0x88a3f57d, 0x02a549b7, 0xbeefe74c, 0x5ae72ea1, 0x18ba3a4b, 0x76110f34, 0x359d2e60,
	0x49e75dd7, 0x598a412d, 0xb53f577d, 0x542795f5, 0x0f1c9513, 0x3678f8bd, 0x625c8c75, 0x93d28b77,
	0xed233e53, 0x6e56106f, 0xa6ff4d05, 0x36aa196c, 0xef6f730e, 0x3d24d73a, 0xc787e598, 0xe79559b5,
	0xfd9313d9, 0xa3b309bd, 0xdc2c9798, 0x2f470899, 0x256a9ccf, 0x32c94fed, 0xfe9e997b, 0xa57b610e,
	0x41d57ba2, 0xffee75d9, 0x225ddf53, 0x3ec18d73, 0xa82d43f7, 0xf1a1c4b2, 0x7a13b5e1, 0x68406dca,
	0x9210cb70, 0x1c59d4f2, 0x88a0a711, 0xc10dd30a, 0x2ff35ee9, 0x3232cccd, 0xaf2925a5, 0x36b45f5b,
	0x66e1b9bd, 0x05b3ade8, 0x33b40cfe, 0xa7384a0e, 0x946f4924, 0xd9a69ef1, 0xc0e4e68b, 0xc5f00efc,
};
static unsigned int s_exponent[64] = { 0x00010001 };

bool g_sessionReady;
unsigned char g_sessionKey[16];
unsigned char g_decryptIv[16];
unsigned char g_encryptIv[16];
static SceAesContext s_decryptContext;
static SceAesContext s_encryptContext;

int CreateSession(void)
{
	if (g_sessionReady)
		return -1;
	sceClibMemset(g_encryptIv, 0, sizeof(g_encryptIv));
	sceClibMemset(g_decryptIv, 0, sizeof(g_decryptIv));
	sceKernelGetRandomNumber(g_sessionKey, sizeof(g_sessionKey));
	sceAesScheduleKey(&s_encryptContext, 128, 128, g_sessionKey);
	sceAesScheduleKey(&s_decryptContext, 128, 128, g_sessionKey);
	g_sessionReady = true;
	return 0;
}

int DestroySession(void)
{
	if (!g_sessionReady)
		return -1;
	g_sessionReady = false;
	return 0;
}

int EncryptSessionKey(unsigned char *out)
{
	MarlinRsaExponentKey key = { s_modulus, s_exponent };
	MarlinRsaData input = { g_sessionKey, sizeof(g_sessionKey) };
	MarlinRsaData output = { out, 0 };
	MarlinRsaData label = { NULL, 0 };
	MarlinRsaHash hash = { Sha1Digest, 20 };

	MarlinRsaSetRandomGenerator(GenerateRandom);
	if (MarlinRsaEncryptOaep2048(&output, &input, &key, &hash, &label) != 0)
		return -1;
	return 0;
}

int SetEncryptIv(const void *iv)
{
	if (!g_sessionReady)
		return -1;
	sceClibMemcpy_safe(g_encryptIv, iv, sizeof(g_encryptIv));
	return 0;
}

int SetDecryptIv(const void *iv)
{
	if (!g_sessionReady)
		return -1;
	sceClibMemcpy_safe(g_decryptIv, iv, sizeof(g_decryptIv));
	return 0;
}

/* a size rounded up to whole AES blocks */
int GetPaddedSize(int size)
{
	return (size - 1) / AES_BLOCK_SIZE * AES_BLOCK_SIZE + AES_BLOCK_SIZE;
}

/* the text of a text packet, after its three arguments */
int GetTextSize(const CompanionUtilPacket *header)
{
	return header->size - 20;
}

/* the text of a text update, after its five arguments */
int GetBodySize(const CompanionUtilPacket *header)
{
	return header->size - 28;
}

int Encrypt(const unsigned char *in, unsigned char *out, SceSize size)
{
	unsigned char block[AES_BLOCK_SIZE];
	const unsigned char *p;
	int i, ret;

	if (!g_sessionReady)
		return -1;
	for (p = in; (SceSize)(p - in) < size; p += AES_BLOCK_SIZE) {
		for (i = 0; i < AES_BLOCK_SIZE; i++)
			block[i] = p[i] ^ g_encryptIv[i];
		ret = sceAesEncrypt(&s_encryptContext, block, out);
		if (ret < 0)
			return ret;
		sceClibMemcpy_safe(g_encryptIv, out, sizeof(g_encryptIv));
		out += AES_BLOCK_SIZE;
	}
	return 0;
}

int Decrypt(unsigned char *data, SceSize size)
{
	unsigned char iv[AES_BLOCK_SIZE];
	unsigned char *p;
	int i, ret;

	if (!g_sessionReady)
		return -1;
	for (p = data; (SceSize)(p - data) < size; p += AES_BLOCK_SIZE) {
		sceClibMemcpy_safe(iv, g_decryptIv, sizeof(iv));
		sceClibMemcpy_safe(g_decryptIv, p, sizeof(g_decryptIv));
		ret = sceAesDecrypt(&s_decryptContext, p, p);
		if (ret < 0)
			return ret;
		for (i = 0; i < AES_BLOCK_SIZE; i++)
			p[i] ^= iv[i];
	}
	return 0;
}

int GenerateRandom(unsigned char *data, int size)
{
	sceKernelGetRandomNumber(data, size);
	return 0;
}

} /* namespace SceCompanionUtil */
