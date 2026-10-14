/* Descriptive names: original export NIDs have no SDK identifiers. */
#include <sce/syscall_copy.h>
#include "kernel_wrappers.h"
int MarlinHciInitialize(void);
int MarlinHciTerminate(void);
int MarlinHciLock(void);
int MarlinHciUnlock(void);
int MarlinHciImportStoredKeyChecked(unsigned int kind, unsigned int *outputHandle);
int MarlinHciGetKeyOperationsEnabledChecked(unsigned int handle);
int MarlinHciReleaseKeyChecked(unsigned int handle);
int MarlinHciExportStoredListChecked(void *handles, unsigned int *count);
int MarlinHciImportWrappedKeyChecked(MarlinWrappedKeyImport *request);
int MarlinHciExportStoredInfoChecked(unsigned int *outputHandle);
int MarlinHciExportEncryptedKeyChecked(unsigned int handle, unsigned int *outputHandle);
int MarlinHciImportDerivedKeyChecked(MarlinDerivedKeyImport *request);
int MarlinHciDecryptChecked(MarlinKeyDecryptRequest *request);
int MarlinHciVerifyMacChecked(MarlinKeyVerifyRequest *request);
int MarlinHciVerifyDigestChecked(MarlinKeyVerifyRequest *request);
int MarlinHciStoreDataChecked(int selector, unsigned int size, const void *data);
int MarlinHciCheckStoredChecked(int selector);
int MarlinHciGetHandleSizeChecked(unsigned int handle, unsigned int *outputSize);
int MarlinHciTakeHandleObjectChecked(unsigned int handle, unsigned int unused, void *output);
int MarlinHciDisableKeyChecked(unsigned int handle, const MarlinUserDataInput *input,
			unsigned int *outputHandle);
int MarlinHciWrapRsaChecked(unsigned int handle, const MarlinUserDataInput *modulus,
			const MarlinUserDataInput *publicExponent, unsigned int *outputHandle);
int MarlinHciSignChecked(unsigned int handle, unsigned int algorithm,
			const MarlinUserDataInput *input, unsigned int *outputHandle);

int MarlinHciInitializeAlias(void)
{
	return MarlinHciInitialize();
}

int MarlinHciTerminateAlias(void)
{
	return MarlinHciTerminate();
}

int MarlinHciLockAlias(void)
{
	return MarlinHciLock();
}

int MarlinHciUnlockAlias(void)
{
	return MarlinHciUnlock();
}

int MarlinHciImportStoredKey(unsigned int kind, unsigned int *outputHandle)
{
	int ret;

	BLOCK_OUT(outputHandle, sizeof(*outputHandle),
		  MarlinHciImportStoredKeyChecked(kind, 0),
		  MarlinHciImportStoredKeyChecked(kind, (unsigned int *)k));
}

int MarlinHciGetKeyOperationsEnabled(unsigned int handle)
{
	return MarlinHciGetKeyOperationsEnabledChecked(handle);
}

int MarlinHciReleaseKey(unsigned int handle)
{
	return MarlinHciReleaseKeyChecked(handle);
}

int MarlinHciExportStoredList(void *outputHandles, unsigned int *outputCount,
			      const SyscallSizeArgs *userArgs)
{
	SyscallSizeArgs args;
	void *handles = NULL;
	unsigned int *count = outputCount;
	int ret;
	int copy;

	ARGS_IN(args, userArgs);
	if (outputHandles != NULL) {
		if (args.size > 64)
			return SCE_KERNEL_ERROR_NO_MEMORY;
		handles = __builtin_alloca(64);
		__builtin_memset(handles, 0, 64);
	}
	if (outputCount != NULL) {
		count = __builtin_alloca(sizeof(*count));
		*count = 0;
	}
	ret = MarlinHciExportStoredListChecked(handles, count);
	if (outputHandles != NULL) {
		copy = ksceKernelCopyToUser(outputHandles, handles, args.size);
		if (copy < 0)
			return copy;
	}
	if (outputCount != NULL)
		return copyResult(ret, ksceKernelCopyToUser(outputCount, count,
							 sizeof(*count)));
	return ret;
}

int MarlinHciImportWrappedKey(MarlinWrappedKeyImport *userRequest)
{
	MarlinWrappedKeyImport *request;
	int ret;

	if (userRequest == NULL)
		return MarlinHciImportWrappedKeyChecked(NULL);
	request = __builtin_alloca(sizeof(*request));
	__builtin_memset(request, 0, sizeof(*request));
	ret = ksceKernelCopyFromUser(request, userRequest, sizeof(*request));
	if (ret < 0)
		return ret;
	ret = MarlinHciImportWrappedKeyChecked(request);
	return copyResult(ret, ksceKernelCopyToUser(userRequest, request,
						 sizeof(*request)));
}

int MarlinHciExportStoredInfo(unsigned int *outputHandle)
{
	int ret;

	BLOCK_OUT(outputHandle, sizeof(*outputHandle),
		  MarlinHciExportStoredInfoChecked(NULL),
		  MarlinHciExportStoredInfoChecked((unsigned int *)k));
}

int MarlinHciExportEncryptedKey(unsigned int handle, unsigned int *outputHandle)
{
	int ret;

	BLOCK_OUT(outputHandle, sizeof(*outputHandle),
		  MarlinHciExportEncryptedKeyChecked(handle, NULL),
		  MarlinHciExportEncryptedKeyChecked(handle, (unsigned int *)k));
}

int MarlinHciImportDerivedKey(MarlinDerivedKeyImport *request)
{
	return MarlinHciImportDerivedKeyChecked(request);
}

int MarlinHciSign(unsigned int handle, unsigned int algorithm,
		  const MarlinUserDataInput *input, unsigned int *outputHandle)
{
	int ret;

	BLOCK_OUT(outputHandle, sizeof(*outputHandle),
		  MarlinHciSignChecked(handle, algorithm, input, NULL),
		  MarlinHciSignChecked(handle, algorithm, input,
				       (unsigned int *)k));
}

int MarlinHciDecrypt(MarlinKeyDecryptRequest *request)
{
	return MarlinHciDecryptChecked(request);
}

int MarlinHciWrapRsa(unsigned int handle, const MarlinUserDataInput *modulus,
		     const MarlinUserDataInput *publicExponent,
		     unsigned int *outputHandle)
{
	int ret;

	BLOCK_OUT(outputHandle, sizeof(*outputHandle),
		  MarlinHciWrapRsaChecked(handle, modulus, publicExponent, NULL),
		  MarlinHciWrapRsaChecked(handle, modulus, publicExponent,
				       (unsigned int *)k));
}

int MarlinHciVerifyMac(MarlinKeyVerifyRequest *request)
{
	return MarlinHciVerifyMacChecked(request);
}

int MarlinHciVerifyDigest(MarlinKeyVerifyRequest *request)
{
	return MarlinHciVerifyDigestChecked(request);
}

int MarlinHciDisableKey(unsigned int handle, const MarlinUserDataInput *input,
			unsigned int *outputHandle)
{
	int ret;

	BLOCK_OUT(outputHandle, sizeof(*outputHandle),
		  MarlinHciDisableKeyChecked(handle, input, NULL),
		  MarlinHciDisableKeyChecked(handle, input, (unsigned int *)k));
}

int MarlinHciStoreData(int selector, unsigned int size, const void *data)
{
	return MarlinHciStoreDataChecked(selector, size, data);
}

int MarlinHciCheckStored(int selector)
{
	return MarlinHciCheckStoredChecked(selector);
}

int MarlinHciGetHandleSize(unsigned int handle, unsigned int *outputSize)
{
	int ret;

	BLOCK_OUT(outputSize, sizeof(*outputSize),
		  MarlinHciGetHandleSizeChecked(handle, NULL),
		  MarlinHciGetHandleSizeChecked(handle, (unsigned int *)k));
}

int MarlinHciTakeHandleObject(unsigned int handle, unsigned int unused, void *output)
{
	return MarlinHciTakeHandleObjectChecked(handle, unused, output);
}
