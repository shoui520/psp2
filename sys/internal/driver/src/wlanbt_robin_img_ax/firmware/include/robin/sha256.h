/*
 * SHA-256 and HMAC-SHA256 (Marvell's common/sha_256.h): the Mask ROM's
 * entry points (without the private-data argument Marvell's source passes) and the
 * two helpers of mrvl_sha256_crypto.c that give them their scratch memory.
 */
#ifndef ROBIN_SHA256_H
#define ROBIN_SHA256_H

#include <stdint.h>

#define SHA256_MAC_LEN			32
#define HMAC_SHA256_MIN_SCRATCH_BUF	500
#define SHA256_MIN_SCRATCH_BUF		400

/* num_elem data areas hashed in turn into mac; the scratch memory is SHA256_MIN_SCRATCH_BUF bytes. */
void sha256_vector(uint32_t num_elem, uint8_t *addr[], uint32_t *len, uint8_t *mac,
		   uint8_t *pScratchMem);						/* 0x03f059bd (veneer 0x531ec) */
/*
 * HMAC-SHA256 over a data vector (RFC 2104); element 0 of addr and len is left for the function
 * and counts in num_elem. The scratch memory is HMAC_SHA256_MIN_SCRATCH_BUF bytes.
 */
void hmac_sha256_vector(uint8_t *key, uint32_t key_len, uint32_t num_elem, uint8_t *addr[],
			uint32_t *len, uint8_t *mac, uint8_t *pScratchMem);	/* 0x03f05a1b (0x531f4) */

void mrvl_sha256_crypto_vector(int num_elem, uint8_t *addr[], int *len, uint8_t *mac);	/* 0x2e790 */
void mrvl_sha256_crypto_kdf(uint8_t *pKey, uint8_t key_len, char *label, uint8_t label_len,
			    uint8_t *pContext, uint16_t context_len, uint8_t *pOutput,
			    uint16_t output_len);					/* 0x2e7c8 */

#endif /* ROBIN_SHA256_H */
