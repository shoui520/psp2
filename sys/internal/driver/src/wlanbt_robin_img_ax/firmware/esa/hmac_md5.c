/*
 * HMAC-MD5 (Marvell's common/hmac_md5.c, 0x2da70), as this firmware has it: the
 * working buffer comes from the encryption pool, the MD5 itself from the Mask ROM.
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/esa.h"
#include "robin/md5.h"
#include "robin/rom.h"

void Mrvl_hmac_md5(uint8_t *text_data, int text_len, uint8_t *key, int key_len, uint8_t *digest)
{
	OsaBuffer *pDesc;
	uint8_t *k_pad;			/* padding - key XORd with i/opad */
	Mrvl_MD5_CTX *context;
	int i;

	/* Wait forever ensures a buffer */
	pDesc = osa_BufferAlloc(ramHook_encrPoolConfig, 400, 0xffffffff);
	/* WLAN buffers are aligned, so k_pad starts at a uint32_t boundary */
	k_pad = (uint8_t *)pDesc + pDesc->dataOffset;
	context = (Mrvl_MD5_CTX *)(k_pad + 64);

	/* if key is longer than 64 bytes reset it to key=MD5(key) */
	if (key_len > 64) {
		Mrvl_MD5_CTX tctx;

		wpa_MD5Init(&tctx);
		wpa_MD5Update(&tctx, key, key_len);
		wpa_MD5Final(context->buffer, &tctx);
		key = context->buffer;
		key_len = 16;
	}

	/*
	 * the HMAC_MD5 transform looks like:
	 *
	 *  MD5(K XOR opad, MD5(K XOR ipad, text))
	 *
	 * where K is an n byte key
	 * ipad is the byte 0x36 repeated 64 times
	 * opad is the byte 0x5c repeated 64 times
	 * and text is the data being protected
	 */

	/* start out by storing key in pads */
	__aeabi_memclr(k_pad, 64);
	__aeabi_memcpy(k_pad, key, key_len);

	/* XOR key with ipad and opad values */
	for (i = 0; i < 16; i++)
		((uint32_t *)k_pad)[i] ^= 0x36363636;

	/* perform inner MD5 */
	wpa_MD5Init(context);			/* init context for 1st pass */
	wpa_MD5Update(context, k_pad, 64);	/* start with inner pad */
	wpa_MD5Update(context, text_data, text_len);	/* then text of datagram */
	wpa_MD5Final(digest, context);		/* finish up 1st pass */

	/* start out by storing key in pads */
	__aeabi_memclr(k_pad, 64);
	__aeabi_memcpy(k_pad, key, key_len);

	for (i = 0; i < 16; i++)
		((uint32_t *)k_pad)[i] ^= 0x5c5c5c5c;

	/* perform outer MD5 */
	wpa_MD5Init(context);			/* init context for 2nd pass */
	wpa_MD5Update(context, k_pad, 64);	/* start with outer pad */
	wpa_MD5Update(context, digest, 16);	/* then results of 1st hash */
	wpa_MD5Final(digest, context);		/* finish up 2nd pass */

	osa_BufferFree(pDesc);
}
