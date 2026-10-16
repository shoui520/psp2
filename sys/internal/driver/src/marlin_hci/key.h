#ifndef MARLIN_HCI_KEY_H
#define MARLIN_HCI_KEY_H

#include "buffer.h"
#include <sce/rsa.h>

typedef struct MarlinKey {
	MarlinBuffer *data;
	unsigned int kind;
	unsigned int format;
	unsigned int operationsEnabled;
} MarlinKey;

typedef struct MarlinRsaKeyComponents {
	unsigned int ownsData;
	MarlinBuffer modulus;
	MarlinBuffer publicExponent;
	MarlinBuffer privateExponent;
	MarlinBuffer primeP;
	MarlinBuffer primeQ;
	MarlinBuffer exponentP;
	MarlinBuffer exponentQ;
	MarlinBuffer coefficient;
} MarlinRsaKeyComponents;

typedef struct MarlinRsaContext {
	int bits;
	unsigned int primeSize;
	unsigned int modulusSize;
	int primeWords;
	int modulusWords;
	unsigned int *modulus;
	unsigned int *publicExponent;
	unsigned int *privateExponent;
	unsigned int *primeP;
	unsigned int *primeQ;
	unsigned int *exponentP;
	unsigned int *exponentQ;
	unsigned int *coefficient;
	MarlinRsaExponentKey publicKey;
	MarlinRsaPrivateKey privateKey;
} MarlinRsaContext;
typedef struct MarlinCipherContext MarlinCipherContext;

int MarlinKeyDestroy(MarlinKey *key);
int MarlinKeyCreate(unsigned int kind, MarlinKey **output);

int MarlinBufferDeobfuscateCopy(const MarlinBuffer *buffer, MarlinBuffer **copy);
int MarlinRsaParseKey(const MarlinBuffer *data, MarlinRsaKeyComponents **key);
int MarlinRsaFreeKey(MarlinRsaKeyComponents *key);
int MarlinRsaCreate(MarlinRsaContext **context, const MarlinBuffer *modulus,
		    const MarlinBuffer *publicExponent,
		    const MarlinBuffer *privateExponent, const MarlinBuffer *primeP,
		    const MarlinBuffer *primeQ, const MarlinBuffer *exponentP,
		    const MarlinBuffer *exponentQ, const MarlinBuffer *coefficient);
int MarlinRsaDestroy(MarlinRsaContext *context);
int MarlinRsaDecryptPkcs1(MarlinRsaContext *context, const MarlinBuffer *encrypted,
			 MarlinBuffer **output);
int MarlinRsaDecryptOaep(MarlinRsaContext *context, const MarlinBuffer *encrypted,
			const MarlinBuffer *label, MarlinBuffer **output);

#endif
