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

#endif
