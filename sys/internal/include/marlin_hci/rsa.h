/* Shared RSA arithmetic descriptors; original internal names are unknown. */
#pragma once

#include <stddef.h>

typedef struct MarlinRsaData {
	unsigned char *data;
	int size;
} MarlinRsaData;

typedef struct MarlinRsaExponentKey {
	unsigned int *modulus;
	unsigned int *exponent;
} MarlinRsaExponentKey;

typedef struct MarlinRsaPrivateKey {
	unsigned int reserved;
	unsigned int *modulus;
	unsigned int reservedExponent;
	unsigned int *privateExponent;
	unsigned int *primeP;
	unsigned int *primeQ;
	unsigned int *exponentP;
	unsigned int *exponentQ;
	unsigned int *coefficient;
} MarlinRsaPrivateKey;

_Static_assert(sizeof(MarlinRsaData) == 8, "RSA data descriptor ABI");
_Static_assert(offsetof(MarlinRsaData, size) == 4, "RSA data size offset");
_Static_assert(sizeof(MarlinRsaExponentKey) == 8, "RSA exponent key ABI");
_Static_assert(offsetof(MarlinRsaExponentKey, exponent) == 4,
	       "RSA exponent pointer offset");

/* rsa_encrypt.c */
int MarlinRsaEncrypt512(MarlinRsaData *output, const MarlinRsaData *input,
			const MarlinRsaExponentKey *key);
int MarlinRsaEncrypt1024(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key);
int MarlinRsaEncrypt2048(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key);

/* rsa_private.c */
int MarlinRsaPrivateOperation(unsigned int *output, const unsigned int *input,
			      const MarlinRsaPrivateKey *key, int count);

/* rsa_public_operation.c */
int MarlinRsaExponentOperation(unsigned int *output, const unsigned int *input,
			       const MarlinRsaExponentKey *key, int count);

/* rsa_random.c */
int MarlinRsaHasRandomGenerator(void);
void MarlinRsaFillNonzeroRandom(unsigned char *data, int size);
int MarlinRsaSetRandomGenerator(int (*generator)(unsigned char *data, int size));
