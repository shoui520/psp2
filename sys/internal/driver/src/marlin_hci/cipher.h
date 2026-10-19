#ifndef MARLIN_HCI_CIPHER_H
#define MARLIN_HCI_CIPHER_H

#include "key.h"
#include <sysmem/utils.h>

struct MarlinCipherContext {
	int algorithm;
	int keyOperation;
	int blockMode;
	int paddingMode;
	unsigned int blockSize;
	unsigned char iv[16];
	unsigned int ivSize;
	unsigned char counter[8];
	SceAesFullContext *keySchedule;
};

int MarlinCipherValidate(const MarlinCipherContext *context);
int MarlinCipherGetPaddingSize(const MarlinCipherContext *context,
			      const unsigned char *data, unsigned int *size);
int MarlinCipherPreparePadding(const MarlinCipherContext *context,
			       const unsigned char *data, unsigned int size,
			       unsigned char *block, unsigned int *paddingSize);
int MarlinEncryptBlock(SceAesFullContext *key, const void *data, void *encrypted);
int MarlinDecryptBlock(SceAesFullContext *key, const void *encrypted, void *data);
int MarlinKeyScheduleCreate(SceAesFullContext **context, int operation,
			    unsigned int keySize, const void *key);
int MarlinKeyScheduleDestroy(SceAesFullContext *context);
int MarlinCipherEncryptEcb(const MarlinCipherContext *context,
			   SceAesFullContext *key, const unsigned char *data,
			   unsigned int size, unsigned char *encrypted,
			   unsigned int *encryptedSize,
			   const unsigned char *paddingBlock,
			   unsigned int paddingSize);
int MarlinCipherDecryptEcb(const MarlinCipherContext *context,
			   SceAesFullContext *key, const unsigned char *encrypted,
			   unsigned int size, unsigned char *data,
			   unsigned int *dataSize);
int MarlinCipherEncryptCbc(const MarlinCipherContext *context,
			   SceAesFullContext *key, const unsigned char *data,
			   unsigned int size, unsigned char *encrypted,
			   unsigned int *encryptedSize,
			   const unsigned char *paddingBlock,
			   unsigned int paddingSize);
int MarlinCipherDecryptCbc(const MarlinCipherContext *context,
			   SceAesFullContext *key, const unsigned char *encrypted,
			   unsigned int size, unsigned char *data,
			   unsigned int *dataSize);

/* cipher.c */
int MarlinCipherCreate(MarlinCipherContext **context, unsigned int selector,
		       const void *iv, unsigned int ivSize, const void *counter,
		       const void *key, unsigned int keySize);
int MarlinCipherDestroy(MarlinCipherContext *context);
int MarlinCipherProcess(MarlinCipherContext *context, const void *input,
			unsigned int size, void *output,
			unsigned int *outputSize);
int MarlinEncryptData(const void *iv, const void *key, const void *data,
		      unsigned int size, void *encrypted,
		      unsigned int *encryptedSize);
int MarlinDecryptData(const void *iv, const void *key, const void *encrypted,
		      unsigned int size, void *data, unsigned int *dataSize);
int MarlinCipherTransformBlock(const void *key, const void *input, void *output);

/* hash.c */
int MarlinHmacDigest(unsigned int algorithm, const MarlinBuffer *key,
		     const MarlinBuffer *data, MarlinBuffer **digest);

/* hash_context.c */
int MarlinSha1Digest(const void *data, unsigned int size, void *digest);
int MarlinSha256Digest(const void *data, unsigned int size, void *digest);

/* mask.c */
int MarlinGenerateSha1Mask(const void *seed, unsigned int seedSize,
			   int maskSize, unsigned char *mask);

/* random.c */
int MarlinRandomCreate(SceMt19937Context **context);
int MarlinRandomRead(SceMt19937Context *context, int size, void *data);
int MarlinRandomDestroy(SceMt19937Context *context);

/* random_fill.c */
int MarlinFillRandomBytes(unsigned char *data, int size);

#endif
