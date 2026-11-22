/*
 * The personality's keys, by the platform mode: a personality decrypted with
 * AES-128-CBC (its first 16 bytes the IV), and a block decrypted with AES-128-ECB
 * (0x8105D280-). The file's name is ours.
 */
#include "mln_internal.h"

/* the key of mode 1, which Sony writes byte by byte on the stack */
static inline void MlnPsnlDevelopmentKey(unsigned char *key)
{
	key[0] = 0x0C;
	key[1] = 0xA2;
	key[2] = 0x64;
	key[3] = 0x60;
	key[4] = 0xA9;
	key[5] = 0x30;
	key[6] = 0x66;
	key[7] = 0x5A;
	key[8] = 0x53;
	key[9] = 0xDC;
	key[10] = 0xB6;
	key[11] = 0x9A;
	key[12] = 0x6F;
	key[13] = 0x9C;
	key[14] = 0x6F;
	key[15] = 0x27;
}

/* the key of mode 2 */
static const unsigned char s_psnlKey[16] = {
	0x09, 0x94, 0x33, 0x60, 0x85, 0x35, 0xEC, 0x09,
	0xF0, 0x6B, 0xC0, 0x34, 0x36, 0xE1, 0xD1, 0x8F,
};

/* decrypt a mode 1 personality in place; size is its size, then the plaintext's */
unsigned int MlnImpl_8105D280(unsigned char *data, unsigned int *size)
{
	unsigned char key[16] __attribute__((aligned(16)));
	unsigned int length = *size;
	unsigned int ret;

	MlnPsnlDevelopmentKey(key);
	ret = MlnImpl_81054F84(data, key, data + 16, *size - 16, data, &length);
	if (ret == 0)
		*size = length;
	return ret;
}

/* decrypt a block of 16 bytes in place with the mode 1 key */
unsigned int MlnImpl_8105D34C(unsigned char *data)
{
	unsigned char key[16] __attribute__((aligned(16)));

	MlnPsnlDevelopmentKey(key);
	return MlnImpl_810550B0(key, data, data);
}

/* decrypt a mode 2 personality in place */
unsigned int MlnImpl_8105D3F4(unsigned char *data, unsigned int *size)
{
	return MlnImpl_81054F84(data, s_psnlKey, data + 16, *size - 16, data, size);
}

/* decrypt a block of 16 bytes in place with the mode 2 key */
unsigned int MlnImpl_8105D428(unsigned char *data)
{
	return MlnImpl_810550B0(s_psnlKey, data, data);
}
