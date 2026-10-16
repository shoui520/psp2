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
