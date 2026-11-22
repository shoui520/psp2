/*
 * SceMarlinHci's interface to user mode: the request records its syscalls take, and the
 * syscalls by their user-mode link names (each comment names the kernel function behind it,
 * sys/internal/driver/src/marlin_hci/protected_exports.c).
 */
#ifndef _MARLIN_HCI_HCI_H
#define _MARLIN_HCI_HCI_H

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

typedef struct MarlinKeyVerifyRequest {
	unsigned int keyHandle;
	unsigned int algorithm;
	unsigned int dataSize;
	const void *data;
	unsigned int expectedSize;
	const void *expectedData;
	unsigned int verified;
} MarlinKeyVerifyRequest;

/* the syscalls SceLibMarlin imports */
/* MarlinHciLock */
int SceMarlinHci_4406594F(void);
/* MarlinHciUnlock */
int SceMarlinHci_457F6683(void);
/* MarlinHciImportStoredKeyChecked */
int SceMarlinHci_C3E201D5(unsigned int kind, unsigned int *outputHandle);
/* MarlinHciGetKeyOperationsEnabledChecked */
int SceMarlinHci_A626EBBC(unsigned int handle);
/* MarlinHciReleaseKeyChecked */
int SceMarlinHci_9EA124CA(unsigned int handle);
/* MarlinHciSignChecked */
int SceMarlinHci_B7FB96EC(unsigned int handle, unsigned int algorithm,
			 const MarlinUserDataInput *userInput, unsigned int *outputHandle);
/* MarlinHciGetHandleSizeChecked */
int SceMarlinHci_527EB697(unsigned int handle, unsigned int *outputSize);
/* MarlinHciTakeHandleObjectChecked */
int SceMarlinHci_C889F85B(unsigned int handle, unsigned int size, void *output);
/* MarlinHciDecryptChecked */
int SceMarlinHci_AE0FBC3F(MarlinKeyDecryptRequest *userRequest);
/* MarlinHciImportWrappedKeyChecked */
int SceMarlinHci_24D94277(MarlinWrappedKeyImport *userRequest);
/* MarlinHciImportDerivedKeyChecked */
int SceMarlinHci_00F544DF(MarlinDerivedKeyImport *userRequest);
/* MarlinHciVerifyMacChecked */
int SceMarlinHci_CB99E399(MarlinKeyVerifyRequest *userRequest);
/* MarlinHciVerifyDigestChecked */
int SceMarlinHci_44390C58(MarlinKeyVerifyRequest *userRequest);
/* MarlinHciDisableKeyChecked */
int SceMarlinHci_EAD075E3(unsigned int handle, const MarlinUserDataInput *userInput,
			 unsigned int *outputHandle);
/* MarlinHciExportEncryptedKeyChecked */
int SceMarlinHci_68315912(unsigned int keyHandle, unsigned int *outputHandle);
/* MarlinHciWrapRsaChecked */
int SceMarlinHci_F7303D28(unsigned int handle, const MarlinUserDataInput *modulus,
			 const MarlinUserDataInput *publicExponent, unsigned int *outputHandle);
/* MarlinHciExportStoredListChecked */
int SceMarlinHci_CC2BA3B5(void *outputHandles, unsigned int *outputCount);
/* MarlinHciExportStoredInfoChecked */
int SceMarlinHci_4DADE2FD(unsigned int *outputHandle);
/* MarlinHciStoreDataChecked */
int SceMarlinHci_BFBA3A79(int selector, unsigned int size, const void *userData);
/* MarlinHciCheckStoredChecked */
int SceMarlinHci_E8A352C2(int selector);

#endif
