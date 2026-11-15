/*
 * HMAC-SHA1 and the WPA PRF built on it (Marvell's common/hmac_sha1.c, 0x2db58 and
 * 0x2dc5c), as this firmware has them: the working buffer comes from the encryption
 * pool; the SHA-1 is the firmware's own (esa/sha1.c).
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/esa.h"
#include "robin/rom.h"
#include "robin/sha1.h"

#define MIN(a, b)	((a) < (b) ? (a) : (b))

void Mrvl_hmac_sha1(uint8_t **ppText, int *pTextLen, int textNum, uint8_t *key,
		    int key_len, uint8_t *output, int outputLen)
{
	OsaBuffer *pDesc;
	Mrvl_SHA1_CTX *context;
	uint8_t *k_pad;			/* padding - key XORd with i/opad */
	uint8_t *digest;
	int i;

	pDesc = osa_BufferAlloc(ramHook_encrPoolConfig, 400, 0xffffffff);
	k_pad = (uint8_t *)pDesc + pDesc->dataOffset;
	digest = k_pad + 64;
	context = (Mrvl_SHA1_CTX *)(k_pad + 64 + 20);

	/* if key is longer than 64 bytes reset it to key=SHA1(key) */
	if (key_len > 64) {
		Mrvl_SHA1Init(context);
		Mrvl_SHA1Update(context, key, key_len);
		Mrvl_SHA1Final(context, key);
		key_len = 20;
	}

	/*
	 * the HMAC_SHA1 transform looks like:
	 *
	 * SHA1(K XOR opad, SHA1(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* perform inner SHA1: start out by storing key in pads */
	__aeabi_memclr(k_pad, 64);
	__aeabi_memcpy(k_pad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 8; i++)
		((uint64_t *)k_pad)[i] ^= 0x3636363636363636ULL;

	Mrvl_SHA1Init(context);			/* init context for 1st pass */
	Mrvl_SHA1Update(context, k_pad, 64);	/* start with inner pad */
	for (i = 0; i < textNum; i++)
		Mrvl_SHA1Update(context, ppText[i], pTextLen[i]);	/* then text of datagram */
	Mrvl_SHA1Final(context, digest);	/* finish up 1st pass */

	/* perform outer SHA1: start out by storing key in pads */
	__aeabi_memclr(k_pad, 64);
	__aeabi_memcpy(k_pad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 8; i++)
		((uint64_t *)k_pad)[i] ^= 0x5c5c5c5c5c5c5c5cULL;

	Mrvl_SHA1Init(context);			/* init context for 2nd pass */
	Mrvl_SHA1Update(context, k_pad, 64);	/* start with outer pad */
	Mrvl_SHA1Update(context, digest, 20);	/* then results of 1st hash */
	Mrvl_SHA1Final(context, digest);	/* finish up 2nd pass */
	__aeabi_memcpy(output, digest, outputLen);

	osa_BufferFree(pDesc);
}

/*
 * PRF -- Length of output is in octets rather than bits
 *     since length is always a multiple of 8 output array is
 *     organized so first N octets starting from 0 contains PRF output
 *
 *     supported inputs are 16, 32, 48, 64
 *     output array must be 80 octets to allow for sha1 overflow
 */
void Mrvl_PRF(uint8_t *key, int key_len, uint8_t *prefix, int prefix_len,
	      uint8_t *data, int data_len, uint8_t *output, int len)
{
	int i;
	int currentindex = 0;
	int total_len;
	uint8_t prf_input[120];		/* concatenated input */
	uint8_t *pText = prf_input;
	int8_t remain = len;

	__aeabi_memclr4(prf_input, sizeof(prf_input));

	if (prefix) {
		__aeabi_memcpy(prf_input, prefix, prefix_len);
		prf_input[prefix_len] = 0;	/* single octet 0 */
		__aeabi_memcpy(&prf_input[prefix_len + 1], data, data_len);
		total_len = prefix_len + 1 + data_len;
	} else {
		__aeabi_memcpy(prf_input, data, data_len);
		total_len = data_len;
	}

	prf_input[total_len] = 0;		/* single octet count, starts at 0 */
	total_len++;
	for (i = 0; i < (len + 19) / 20; i++) {
		Mrvl_hmac_sha1(&pText, &total_len, 1, key, key_len, &output[currentindex],
			       MIN(20, remain));
		currentindex += MIN(20, remain);	/* next concatenation location */
		remain -= 20;
		prf_input[total_len - 1]++;	/* increment octet count */
	}
}
