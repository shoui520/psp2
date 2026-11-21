/*
 * The module's RSA-OAEP encryption (SHA-1 for the hash and the mask), on GcAuthMgr's RSA
 * arithmetic, and its SHA-1.
 */
#pragma once

#include <scetypes.h>

#if defined(__cplusplus)
extern "C" {
#endif

#include <marlin_hci/rsa.h>

/* the hash OAEP uses: digest(out, data, size) and its digest's size */
typedef struct MarlinRsaHash {
	int (*digest)(unsigned char *out, const unsigned char *data, int size);
	int size;
} MarlinRsaHash;

/* sha1.c */
int Sha1Digest(unsigned char *out, const unsigned char *data, int size);

/* rsa_oaep.c */
int MarlinRsaEncryptOaep(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key, int count, int size,
			 const MarlinRsaHash *hash, const MarlinRsaData *label);
int MarlinRsaEncryptOaep2048(MarlinRsaData *output, const MarlinRsaData *input,
			     const MarlinRsaExponentKey *key, const MarlinRsaHash *hash,
			     const MarlinRsaData *label);

/* rsa_mgf1.c */
void MarlinRsaMgf1(unsigned char *mask, int maskSize, const unsigned char *seed, int seedSize,
		   const MarlinRsaHash *hash);

#if defined(__cplusplus)
}
#endif
