#ifndef MARLIN_HCI_KERNEL_WRAPPERS_H
#define MARLIN_HCI_KERNEL_WRAPPERS_H

#include <marlin_hci/hci.h>

/* Stored-list output: sixteen handle slots followed by their count. */
typedef struct MarlinStoredHandleList {
	unsigned int handles[16];
	unsigned int count;
} MarlinStoredHandleList;

typedef struct MarlinKeySignRequest {
	unsigned int keyHandle;
	unsigned int algorithm;
	unsigned int dataSize;
	const void *data;
	unsigned int outputHandle;
	unsigned int reserved;
} MarlinKeySignRequest;

typedef struct MarlinKeyWrapRsaRequest {
	unsigned int keyHandle;
	unsigned int modulusSize;
	const void *modulusData;
	unsigned int publicExponentSize;
	const void *publicExponentData;
	unsigned int outputHandle;
} MarlinKeyWrapRsaRequest;

/* kernel_wrappers.c */
int MarlinKernelLock(void);
int MarlinKernelUnlock(void);
int MarlinKernelImportStoredKey(unsigned int kind, unsigned int *outputHandle);
unsigned int MarlinKernelGetKeyOperationsEnabled(unsigned int handle);
int MarlinKernelReleaseKey(unsigned int handle);
int MarlinKernelExportStoredList(MarlinStoredHandleList *output);
int MarlinKernelImportWrappedKey(MarlinWrappedKeyImport *request);
int MarlinKernelExportStoredInfo(unsigned int *outputHandle);
int MarlinKernelExportEncryptedKey(unsigned int keyHandle, unsigned int *outputHandle);
int MarlinKernelImportDerivedKey(MarlinDerivedKeyImport *request);
int MarlinKernelSign(MarlinKeySignRequest *request);
int MarlinKernelDecrypt(MarlinKeyDecryptRequest *request);
int MarlinKernelWrapRsa(MarlinKeyWrapRsaRequest *request);
int MarlinKernelVerifyMac(MarlinKeyVerifyRequest *request);
int MarlinKernelVerifyDigest(MarlinKeyVerifyRequest *request);
int MarlinKernelDisableKey(unsigned int keyHandle, unsigned int unused, unsigned int *outputHandle);
int MarlinKernelStoreData(int selector, unsigned int size, const void *data);
int MarlinKernelCheckStored(int selector);
int MarlinKernelGetHandleSize(unsigned int handle, unsigned int *outputSize);
int MarlinKernelTakeHandleObject(unsigned int handle, unsigned int unused, void **output);

/* protected_exports.c */
int MarlinHciInitialize(void);
int MarlinHciTerminate(void);
int MarlinHciLock(void);
int MarlinHciUnlock(void);
int MarlinHciImportStoredKeyChecked(unsigned int kind, unsigned int *outputHandle);
int MarlinHciGetKeyOperationsEnabledChecked(unsigned int handle);
int MarlinHciReleaseKeyChecked(unsigned int handle);
int MarlinHciExportStoredListChecked(void *outputHandles, unsigned int *outputCount);
int MarlinHciImportWrappedKeyChecked(MarlinWrappedKeyImport *userRequest);
int MarlinHciExportStoredInfoChecked(unsigned int *outputHandle);
int MarlinHciExportEncryptedKeyChecked(unsigned int keyHandle, unsigned int *outputHandle);
int MarlinHciImportDerivedKeyChecked(MarlinDerivedKeyImport *userRequest);
int MarlinHciSignChecked(unsigned int handle, unsigned int algorithm,
			 const MarlinUserDataInput *userInput,
			 unsigned int *outputHandle);
int MarlinHciDecryptChecked(MarlinKeyDecryptRequest *userRequest);
int MarlinHciWrapRsaChecked(unsigned int handle, const MarlinUserDataInput *modulus,
			    const MarlinUserDataInput *publicExponent,
			    unsigned int *outputHandle);
int MarlinHciVerifyMacChecked(MarlinKeyVerifyRequest *userRequest);
int MarlinHciVerifyDigestChecked(MarlinKeyVerifyRequest *userRequest);
int MarlinHciDisableKeyChecked(unsigned int handle, const MarlinUserDataInput *userInput,
			       unsigned int *outputHandle);
int MarlinHciStoreDataChecked(int selector, unsigned int size, const void *userData);
int MarlinHciCheckStoredChecked(int selector);
int MarlinHciGetHandleSizeChecked(unsigned int handle, unsigned int *outputSize);
int MarlinHciTakeHandleObjectChecked(unsigned int handle, unsigned int unused, void *output);

#endif
