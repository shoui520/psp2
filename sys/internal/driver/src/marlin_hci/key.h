#ifndef MARLIN_HCI_KEY_H
#define MARLIN_HCI_KEY_H

#include "buffer.h"
#include <marlin_hci/rsa.h>

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

/* key_load.c */
int MarlinKeyLoadStoredPair(MarlinKey *key, unsigned int selector);
int MarlinKeyLoadStoredRaw(MarlinKey *key);
int MarlinKeyUnwrap(MarlinKey *wrappingKey, unsigned int mode,
		    const void *iv, unsigned int ivSize, const void *encrypted,
		    unsigned int encryptedSize, MarlinKey *key);
int MarlinKeyUnwrapRsa(MarlinKey *wrappingKey, unsigned int mode,
		       const MarlinBuffer *label, const MarlinBuffer *encrypted,
		       MarlinKey *key);
int MarlinKeyDecryptRsa(MarlinKey *key, unsigned int mode,
		       const MarlinBuffer *label, const MarlinBuffer *encrypted,
		       MarlinBuffer **output);
int MarlinKeyTransformBuffer(MarlinKey *key, unsigned int algorithm,
			     unsigned int mode, const void *counter,
			     const MarlinBuffer *input, MarlinBuffer **output);

/* library.c */
int MarlinInitialize(void);
void MarlinTerminate(void);
int MarlinLock(void *context);
int MarlinUnlock(void *context);
int MarlinKeyImportStored(unsigned int kind, MarlinKey **output);
unsigned int MarlinKeyGetOperationsEnabled(const MarlinKey *key);
int MarlinKeyRelease(MarlinKey *key);
int MarlinKeySign(MarlinKey *key, unsigned int algorithm,
		  const MarlinBuffer *data, MarlinBuffer **signature);
int MarlinKeyDecrypt(MarlinKey *key, unsigned int algorithm,
		     unsigned int mode, const void *counter,
		     const MarlinBuffer *label, const MarlinBuffer *encrypted,
		     MarlinBuffer **output);
int MarlinKeyImportWrapped(MarlinKey *wrappingKey, unsigned int algorithm,
			   const MarlinBuffer *label, const MarlinBuffer *input,
			   unsigned int kind, MarlinKey **output);
int MarlinKeyImportDerived(unsigned int iterations, unsigned int seedIndex,
			   const MarlinBuffer *wrappedSeed, const MarlinBuffer *check,
			   const MarlinBuffer *iv, const MarlinBuffer *encrypted,
			   MarlinKey **output);
int MarlinKeyVerifyMac(MarlinKey *key, unsigned int algorithm,
		       const MarlinBuffer *data, const MarlinBuffer *expected,
		       unsigned int *verified);
int MarlinKeyVerifyDigest(MarlinKey *key, unsigned int algorithm,
			  const MarlinBuffer *prefix, const MarlinBuffer *expected,
			  unsigned int *verified);
int MarlinKeyDisable(MarlinKey *key, unsigned int unused1, MarlinBuffer **unused2);
int MarlinKeyExportEncrypted(MarlinKey *key, MarlinBuffer **output);
int MarlinKeyWrapRsa(MarlinKey *key, const MarlinBuffer *modulus,
		     const MarlinBuffer *publicExponent, MarlinBuffer **output);
int MarlinExportStoredList(MarlinBuffer ***output, unsigned int *count);
int MarlinExportStoredInfo(MarlinBuffer **output);
int MarlinStoreData(int selector, unsigned int size, const void *data);
int MarlinCheckStored(int selector);

/* rsa.c */
int MarlinRsaPrivateTransform(MarlinRsaContext *context,
			      const unsigned int *input, unsigned int *output);
int MarlinRsaSignTransform(MarlinRsaContext *context,
			      const unsigned int *input, unsigned int *output);

/* rsa_crt.c */
int MarlinRsaPrivate512(unsigned int *output, const unsigned int *input,
			const MarlinRsaPrivateKey *key);
int MarlinRsaPrivate1024(unsigned int *output, const unsigned int *input,
			 const MarlinRsaPrivateKey *key);
int MarlinRsaPrivate2048(unsigned int *output, const unsigned int *input,
			 const MarlinRsaPrivateKey *key);

/* rsa_encrypt.c */
int MarlinRsaEncrypt512(MarlinRsaData *output, const MarlinRsaData *input,
			const MarlinRsaExponentKey *key);
int MarlinRsaEncrypt1024(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key);
int MarlinRsaEncrypt2048(MarlinRsaData *output, const MarlinRsaData *input,
			 const MarlinRsaExponentKey *key);

/* rsa_exponent.c */
int MarlinRsaExponent512(unsigned int *output, const unsigned int *input,
			 const MarlinRsaExponentKey *key);
int MarlinRsaExponent1024(unsigned int *output, const unsigned int *input,
			  const MarlinRsaExponentKey *key);
int MarlinRsaExponent2048(unsigned int *output, const unsigned int *input,
			  const MarlinRsaExponentKey *key);

/* rsa_operations.c */
int MarlinRsaEncrypt(MarlinRsaContext *context, const MarlinBuffer *data, MarlinBuffer **output);
int MarlinRsaSign(MarlinRsaContext *context, int algorithm,
		  const MarlinBuffer *data, MarlinBuffer **signature);

/* rsa_padding.c */
int MarlinRemoveOaepPadding(const MarlinBuffer *buffer,
			   const MarlinBuffer *label, MarlinBuffer **output);
int MarlinRemovePkcs1Padding(const MarlinBuffer *buffer, MarlinBuffer **output);
int MarlinCreateSignaturePadding(const MarlinBuffer *data, int algorithm,
				unsigned int size, MarlinBuffer **output);

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

/* rsa_sign.c */
int MarlinRsaSignPrivate512(unsigned int *output, const unsigned int *input,
			    const MarlinRsaPrivateKey *key);
int MarlinRsaSignPrivate1024(unsigned int *output, const unsigned int *input,
			     const MarlinRsaPrivateKey *key);
int MarlinRsaSignPrivate2048(unsigned int *output, const unsigned int *input,
			     const MarlinRsaPrivateKey *key);
int MarlinRsaSignExponent512(unsigned int *output, const unsigned int *input,
			     const MarlinRsaExponentKey *key);
int MarlinRsaSignExponent1024(unsigned int *output, const unsigned int *input,
			     const MarlinRsaExponentKey *key);
int MarlinRsaSignExponent2048(unsigned int *output, const unsigned int *input,
			     const MarlinRsaExponentKey *key);

#endif
