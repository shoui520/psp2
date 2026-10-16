#ifndef MARLIN_HCI_KERNEL_WRAPPERS_H
#define MARLIN_HCI_KERNEL_WRAPPERS_H

/* Stored-list output: sixteen handle slots followed by their count. */
typedef struct MarlinStoredHandleList {
	unsigned int handles[16];
	unsigned int count;
} MarlinStoredHandleList;

typedef struct MarlinWrappedKeyImport {
	unsigned int wrappingKeyHandle;
	unsigned int algorithm;
	unsigned int labelSize;
	const void *labelData;
	unsigned int inputSize;
	const void *inputData;
	unsigned int kind;
	unsigned int outputHandle;
} MarlinWrappedKeyImport;

typedef struct MarlinDerivedKeyImport {
	unsigned int iterations;
	unsigned int seedIndex;
	unsigned int wrappedSeedSize;
	const void *wrappedSeedData;
	unsigned int checkSize;
	const void *checkData;
	unsigned int ivSize;
	const void *ivData;
	unsigned int encryptedSize;
	const void *encryptedData;
	unsigned int outputHandle;
} MarlinDerivedKeyImport;

typedef struct MarlinUserDataInput {
	unsigned int size;
	const void *data;
} MarlinUserDataInput;

typedef struct MarlinKeySignRequest {
	unsigned int keyHandle;
	unsigned int algorithm;
	unsigned int dataSize;
	const void *data;
	unsigned int outputHandle;
} MarlinKeySignRequest;

typedef struct MarlinKeyDecryptRequest {
	unsigned int keyHandle;
	unsigned int algorithm;
	unsigned int mode;
	const void *counter;
	unsigned int labelSize;
	const void *labelData;
	unsigned int encryptedSize;
	const void *encryptedData;
	unsigned int outputHandle;
} MarlinKeyDecryptRequest;

typedef struct MarlinKeyWrapRsaRequest {
	unsigned int keyHandle;
	unsigned int modulusSize;
	const void *modulusData;
	unsigned int publicExponentSize;
	const void *publicExponentData;
	unsigned int outputHandle;
} MarlinKeyWrapRsaRequest;

typedef struct MarlinKeyVerifyRequest {
	unsigned int keyHandle;
	unsigned int algorithm;
	unsigned int dataSize;
	const void *data;
	unsigned int expectedSize;
	const void *expectedData;
	unsigned int verified;
} MarlinKeyVerifyRequest;

#endif
