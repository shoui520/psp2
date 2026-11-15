/*
 * SHA-256 and the 802.11r/w key derivation on top of it (Marvell's
 * common/mrvl_sha256_crypto.c, 0x2e790 and 0x2e7c8): the hashing is in the Mask ROM;
 * these give it its scratch memory from the encryption pool.
 *
 * Written from Marvell's source (Copyright (C) 2014-2017, Marvell International
 * Ltd., GPL-2.0) as this firmware has it.
 */
#include <stdint.h>
#include "robin/btrom.h"
#include "robin/buffer.h"
#include "robin/esa.h"
#include "robin/rom.h"
#include "robin/sha256.h"

/* Helper function to allocate scratch memory and call sha256_vector() */
void mrvl_sha256_crypto_vector(int num_elem, uint8_t *addr[], int *len, uint8_t *mac)
{
	OsaBuffer *pBufDesc = osa_BufferAlloc(ramHook_encrPoolConfig, SHA256_MIN_SCRATCH_BUF, 0xffffffff);

	sha256_vector(num_elem, addr, (uint32_t *)len, mac, (uint8_t *)pBufDesc + pBufDesc->dataOffset);
	osa_BufferFree(pBufDesc);
}

void mrvl_sha256_crypto_kdf(uint8_t *pKey, uint8_t key_len, char *label, uint8_t label_len,
			    uint8_t *pContext, uint16_t context_len, uint8_t *pOutput,
			    uint16_t output_len)
{
	uint8_t *vectors[4 + 1];
	uint32_t vectLen[4 + 1];
	uint8_t *pResult;
	uint8_t *pLoopResult;
	uint8_t iterations;
	uint16_t i;
	OsaBuffer *pBufDesc;

	pBufDesc = osa_BufferAlloc(ramHook_encrPoolConfig, HMAC_SHA256_MIN_SCRATCH_BUF, 0xffffffff);
	pResult = pContext + context_len;

	/*
	 * Working memory layout:
	 *            | KDF-Len output --- >
	 *            |
	 * [KDF Input][HMAC output#1][HMAC output#2][...]
	 */

	/* Number of SHA256 digests needed to meet the bit length output_len */
	iterations = (output_len + 255) / 256;

	pLoopResult = pResult;

	for (i = 1; i <= iterations; i++) {
		/* Skip vectors[0]; Used internally in hmac_sha256_vector function */
		vectors[1] = (uint8_t *)&i;
		vectLen[1] = sizeof(i);

		vectors[2] = (uint8_t *)label;
		vectLen[2] = label_len;

		vectors[3] = pContext;
		vectLen[3] = context_len;

		vectors[4] = (uint8_t *)&output_len;
		vectLen[4] = sizeof(output_len);

		/* KDF input = (K, i || label || Context || Length) */
		hmac_sha256_vector(pKey, key_len, 5, vectors, vectLen, pLoopResult,
				   (uint8_t *)pBufDesc + pBufDesc->dataOffset);

		/*
		 * Move the hmac output pointer so another digest can be appended
		 * if more loop iterations are needed to get the output_len key
		 * bit total
		 */
		pLoopResult += SHA256_MAC_LEN;
	}

	osa_BufferFree(pBufDesc);
	__aeabi_memcpy(pOutput, pResult, output_len / 8);
}
