/* Descriptive source name: this original has no debug source records. */
#include <sce/syscall.h>
#include <sce/vshbridge_imports.h>
#include <sce/error.h>
#include "kernel_wrappers.h"
#include "buffer.h"

int module_start(void);
int module_stop(void);
int MarlinKernelLock(void);
int MarlinKernelUnlock(void);
int MarlinKernelImportStoredKey(unsigned int kind, unsigned int *outputHandle);
unsigned int MarlinKernelGetKeyOperationsEnabled(unsigned int handle);
int MarlinKernelReleaseKey(unsigned int handle);
int MarlinKernelExportStoredList(MarlinStoredHandleList *request);
int MarlinKernelImportWrappedKey(MarlinWrappedKeyImport *request);
int MarlinKernelImportDerivedKey(MarlinDerivedKeyImport *request);
int MarlinKernelSign(MarlinKeySignRequest *request);
int MarlinKernelDecrypt(MarlinKeyDecryptRequest *request);
int MarlinKernelWrapRsa(MarlinKeyWrapRsaRequest *request);
int MarlinKernelVerifyMac(MarlinKeyVerifyRequest *request);
int MarlinKernelVerifyDigest(MarlinKeyVerifyRequest *request);
int MarlinKernelDisableKey(unsigned int handle, unsigned int unused,
			 unsigned int *outputHandle);
int MarlinKernelStoreData(int selector, unsigned int size, const void *data);
int MarlinKernelCheckStored(int selector);
int MarlinKernelGetHandleSize(unsigned int handle, unsigned int *outputSize);
int MarlinKernelTakeHandleObject(unsigned int handle, unsigned int unused, void **output);
int MarlinKernelExportStoredInfo(unsigned int *outputHandle);
int MarlinKernelExportEncryptedKey(unsigned int handle, unsigned int *outputHandle);

int MarlinHciCheckAccess(void)
{
	return SceSblACMgrForDriver_F5AE24AC(0) != 0 ? 0 : SCE_ERROR_ERRNO_ENOSYS;
}

int MarlinHciInitialize(void)
{
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0)
		ret = module_start();
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciTerminate(void)
{
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0)
		ret = module_stop();
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciLock(void)
{
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0)
		ret = MarlinKernelLock();
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciUnlock(void)
{
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0)
		ret = MarlinKernelUnlock();
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciImportStoredKeyChecked(unsigned int kind, unsigned int *outputHandle)
{
	unsigned int handle = 0;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0) {
		ret = MarlinKernelImportStoredKey(kind, &handle);
		if (ret >= 0)
			ret = ksceKernelCopyToUser(outputHandle, &handle, sizeof(handle));
	}
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciGetKeyOperationsEnabledChecked(unsigned int handle)
{
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0)
		ret = MarlinKernelGetKeyOperationsEnabled(handle);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciReleaseKeyChecked(unsigned int handle)
{
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0)
		ret = MarlinKernelReleaseKey(handle);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciExportStoredListChecked(void *outputHandles, unsigned int *outputCount)
{
	MarlinStoredHandleList list;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0) {
		MarlinFillMemory(list.handles, 0, sizeof(list.handles));
		list.count = 0;
		ret = ksceKernelRunWithStack(0x4000,
					   (int (*)(void *))MarlinKernelExportStoredList, &list);
		if (ret >= 0) {
			ret = ksceKernelCopyToUser(outputCount, &list.count,
						  sizeof(list.count));
			if (ret >= 0)
				ret = ksceKernelCopyToUser(outputHandles, list.handles,
							  list.count * sizeof(*list.handles));
		}
	}
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciImportWrappedKeyChecked(MarlinWrappedKeyImport *userRequest)
{
	MarlinWrappedKeyImport input;
	MarlinWrappedKeyImport request;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret < 0)
		goto cleanup;
	ret = ksceKernelCopyFromUser(&input, userRequest, sizeof(input));
	if (ret < 0)
		goto cleanup;
	MarlinFillMemory(&request, 0, sizeof(request));
	request.wrappingKeyHandle = input.wrappingKeyHandle;
	request.algorithm = input.algorithm;
	request.labelSize = input.labelSize;
	if (input.labelSize != 0 && input.labelData != NULL && input.labelSize < 0x10000) {
		request.labelData = MarlinAllocateMemory(NULL, input.labelSize);
		ret = ksceKernelCopyFromUser((void *)request.labelData,
					    input.labelData, input.labelSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.labelData = NULL;
	}
	request.inputSize = input.inputSize;
	if (input.inputSize != 0 && input.inputData != NULL && input.inputSize < 0x10000) {
		request.inputData = MarlinAllocateMemory(NULL, input.inputSize);
		ret = ksceKernelCopyFromUser((void *)request.inputData,
					    input.inputData, input.inputSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.inputData = NULL;
	}
	request.kind = input.kind;
	request.outputHandle = 0;
	ret = ksceKernelRunWithStack(0x4000,
				   (int (*)(void *))MarlinKernelImportWrappedKey, &request);
	if (ret >= 0)
		ret = ksceKernelCopyToUser(&userRequest->outputHandle,
					  &request.outputHandle, sizeof(request.outputHandle));
cleanup:
	MarlinFreeMemory(NULL, (void *)request.labelData);
	MarlinFreeMemory(NULL, (void *)request.inputData);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciExportStoredInfoChecked(unsigned int *outputHandle)
{
	unsigned int handle = 0;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0) {
		ret = MarlinKernelExportStoredInfo(&handle);
		if (ret >= 0)
			ret = ksceKernelCopyToUser(outputHandle, &handle, sizeof(handle));
	}
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciExportEncryptedKeyChecked(unsigned int keyHandle,
				     unsigned int *outputHandle)
{
	unsigned int handle = 0;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0) {
		ret = MarlinKernelExportEncryptedKey(keyHandle, &handle);
		if (ret >= 0)
			ret = ksceKernelCopyToUser(outputHandle, &handle, sizeof(handle));
	}
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciImportDerivedKeyChecked(MarlinDerivedKeyImport *userRequest)
{
	MarlinDerivedKeyImport input;
	MarlinDerivedKeyImport request;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret < 0)
		goto cleanup;
	ret = ksceKernelCopyFromUser(&input, userRequest, sizeof(input));
	if (ret < 0)
		goto cleanup;
	MarlinFillMemory(&request, 0, sizeof(request));
	request.iterations = input.iterations;
	request.seedIndex = input.seedIndex;
	request.wrappedSeedSize = input.wrappedSeedSize;
	if (input.wrappedSeedSize != 0 && input.wrappedSeedData != NULL && input.wrappedSeedSize < 0x10000) {
		request.wrappedSeedData = MarlinAllocateMemory(NULL, input.wrappedSeedSize);
		ret = ksceKernelCopyFromUser((void *)request.wrappedSeedData,
					    input.wrappedSeedData, request.wrappedSeedSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.wrappedSeedData = NULL;
	}
	request.checkSize = input.checkSize;
	if (input.checkSize != 0 && input.checkData != NULL && input.checkSize < 0x10000) {
		request.checkData = MarlinAllocateMemory(NULL, input.checkSize);
		ret = ksceKernelCopyFromUser((void *)request.checkData,
					    input.checkData, request.checkSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.checkData = NULL;
	}
	request.ivSize = input.ivSize;
	if (input.ivSize != 0 && input.ivData != NULL && input.ivSize < 0x10000) {
		request.ivData = MarlinAllocateMemory(NULL, input.ivSize);
		ret = ksceKernelCopyFromUser((void *)request.ivData,
					    input.ivData, request.ivSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.ivData = NULL;
	}
	request.encryptedSize = input.encryptedSize;
	if (input.encryptedSize != 0 && input.encryptedData != NULL && input.encryptedSize < 0x10000) {
		request.encryptedData = MarlinAllocateMemory(NULL, input.encryptedSize);
		ret = ksceKernelCopyFromUser((void *)request.encryptedData,
					    input.encryptedData, request.encryptedSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.encryptedData = NULL;
	}
	request.outputHandle = 0;
	ret = MarlinKernelImportDerivedKey(&request);
	if (ret >= 0)
		ret = ksceKernelCopyToUser(&userRequest->outputHandle,
					  &request.outputHandle, sizeof(request.outputHandle));
cleanup:
	MarlinFreeMemory(NULL, (void *)request.wrappedSeedData);
	MarlinFreeMemory(NULL, (void *)request.checkData);
	MarlinFreeMemory(NULL, (void *)request.ivData);
	MarlinFreeMemory(NULL, (void *)request.encryptedData);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciSignChecked(unsigned int handle, unsigned int algorithm,
			 const MarlinUserDataInput *userInput,
			 unsigned int *outputHandle)
{
	MarlinUserDataInput input = { 0, NULL };
	MarlinKeySignRequest request;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret < 0)
		goto cleanup;
	MarlinFillMemory(&request, 0, sizeof(request));
	request.keyHandle = handle;
	request.algorithm = algorithm;
	if (userInput != NULL) {
		ret = ksceKernelCopyFromUser(&input, userInput, sizeof(input));
		if (ret < 0)
			goto cleanup;
		request.dataSize = input.size;
		if (input.size != 0 && input.data != NULL && input.size < 0x10000) {
			request.data = MarlinAllocateMemory(NULL, input.size);
			ret = ksceKernelCopyFromUser((void *)request.data,
						    input.data, input.size);
			if (ret < 0)
				goto cleanup;
		} else {
			request.data = NULL;
		}
	}
	request.outputHandle = 0;
	ret = ksceKernelRunWithStack(0x4000,
				   (int (*)(void *))MarlinKernelSign, &request);
	if (ret >= 0)
		ret = ksceKernelCopyToUser(outputHandle, &request.outputHandle,
					  sizeof(request.outputHandle));
cleanup:
	MarlinFreeMemory(NULL, (void *)request.data);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciDecryptChecked(MarlinKeyDecryptRequest *userRequest)
{
	MarlinKeyDecryptRequest input;
	MarlinKeyDecryptRequest request;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret < 0)
		goto cleanup;
	ret = ksceKernelCopyFromUser(&input, userRequest, sizeof(input));
	if (ret < 0)
		goto cleanup;
	request.keyHandle = input.keyHandle;
	request.algorithm = input.algorithm;
	request.mode = input.mode;
	if (input.counter != NULL) {
		request.counter = MarlinAllocateMemory(NULL, 8);
		ret = ksceKernelCopyFromUser((void *)request.counter, input.counter, 8);
		if (ret < 0)
			goto cleanup;
	} else {
		request.counter = NULL;
	}
	request.labelSize = input.labelSize;
	if (input.labelSize != 0 && input.labelData != NULL && input.labelSize < 0x10000) {
		request.labelData = MarlinAllocateMemory(NULL, input.labelSize);
		ret = ksceKernelCopyFromUser((void *)request.labelData,
					    input.labelData, request.labelSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.labelData = NULL;
	}
	request.encryptedSize = input.encryptedSize;
	if (input.encryptedSize != 0 && input.encryptedData != NULL &&
	    input.encryptedSize < 0x10000) {
		request.encryptedData = MarlinAllocateMemory(NULL, input.encryptedSize);
		ret = ksceKernelCopyFromUser((void *)request.encryptedData,
					    input.encryptedData, request.encryptedSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.encryptedData = NULL;
	}
	request.outputHandle = 0;
	ret = ksceKernelRunWithStack(0x4000,
				   (int (*)(void *))MarlinKernelDecrypt, &request);
	if (ret >= 0)
		ret = ksceKernelCopyToUser(&userRequest->outputHandle,
					  &request.outputHandle, sizeof(request.outputHandle));
cleanup:
	MarlinFreeMemory(NULL, (void *)request.counter);
	MarlinFreeMemory(NULL, (void *)request.labelData);
	MarlinFreeMemory(NULL, (void *)request.encryptedData);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciWrapRsaChecked(unsigned int handle, const MarlinUserDataInput *modulus,
			    const MarlinUserDataInput *publicExponent,
			    unsigned int *outputHandle)
{
	MarlinUserDataInput modulusInput = { 0, NULL };
	MarlinUserDataInput exponentInput = { 0, NULL };
	MarlinKeyWrapRsaRequest request;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret < 0)
		goto cleanup;
	request.keyHandle = handle;
	if (modulus != NULL) {
		ret = ksceKernelCopyFromUser(&modulusInput, modulus, sizeof(modulusInput));
		if (ret < 0)
			goto cleanup;
		request.modulusSize = modulusInput.size;
		if (modulusInput.size != 0 && modulusInput.data != NULL &&
		    modulusInput.size < 0x10000) {
			request.modulusData = MarlinAllocateMemory(NULL, modulusInput.size);
			ret = ksceKernelCopyFromUser((void *)request.modulusData,
						    modulusInput.data, modulusInput.size);
			if (ret < 0)
				goto cleanup;
		} else {
			request.modulusData = NULL;
		}
	}
	if (publicExponent != NULL) {
		ret = ksceKernelCopyFromUser(&exponentInput, publicExponent,
					    sizeof(exponentInput));
		if (ret < 0)
			goto cleanup;
		request.publicExponentSize = exponentInput.size;
		if (exponentInput.size != 0 && exponentInput.data != NULL &&
		    exponentInput.size < 0x10000) {
			request.publicExponentData = MarlinAllocateMemory(NULL, exponentInput.size);
			ret = ksceKernelCopyFromUser((void *)request.publicExponentData,
						    exponentInput.data, exponentInput.size);
			if (ret < 0)
				goto cleanup;
		} else {
			request.publicExponentData = NULL;
		}
	}
	request.outputHandle = 0;
	ret = ksceKernelRunWithStack(0x4000,
				   (int (*)(void *))MarlinKernelWrapRsa, &request);
	if (ret >= 0)
		ret = ksceKernelCopyToUser(outputHandle, &request.outputHandle,
					  sizeof(request.outputHandle));
cleanup:
	MarlinFreeMemory(NULL, (void *)request.modulusData);
	MarlinFreeMemory(NULL, (void *)request.publicExponentData);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciVerifyMacChecked(MarlinKeyVerifyRequest *userRequest)
{
	MarlinKeyVerifyRequest input;
	MarlinKeyVerifyRequest request;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret < 0)
		goto cleanup;
	ret = ksceKernelCopyFromUser(&input, userRequest, sizeof(input));
	if (ret < 0)
		goto cleanup;
	request.keyHandle = input.keyHandle;
	request.algorithm = input.algorithm;
	request.dataSize = input.dataSize;
	if (input.dataSize != 0 && input.data != NULL && input.dataSize < 0x10000) {
		request.data = MarlinAllocateMemory(NULL, input.dataSize);
		ret = ksceKernelCopyFromUser((void *)request.data, input.data, input.dataSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.data = NULL;
	}
	request.expectedSize = input.expectedSize;
	if (input.expectedSize != 0 && input.expectedData != NULL &&
	    input.expectedSize < 0x10000) {
		request.expectedData = MarlinAllocateMemory(NULL, input.expectedSize);
		ret = ksceKernelCopyFromUser((void *)request.expectedData,
					    input.expectedData, input.expectedSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.expectedData = NULL;
	}
	request.verified = 0;
	ret = ksceKernelRunWithStack(0x4000,
				   (int (*)(void *))MarlinKernelVerifyMac, &request);
	if (ret >= 0)
		ret = ksceKernelCopyToUser(&userRequest->verified, &request.verified,
					  sizeof(request.verified));
cleanup:
	MarlinFreeMemory(NULL, (void *)request.data);
	MarlinFreeMemory(NULL, (void *)request.expectedData);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciVerifyDigestChecked(MarlinKeyVerifyRequest *userRequest)
{
	MarlinKeyVerifyRequest request;
	MarlinKeyVerifyRequest input;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret < 0)
		goto cleanup;
	ret = ksceKernelCopyFromUser(&input, userRequest, sizeof(input));
	if (ret < 0)
		goto cleanup;
	request.keyHandle = input.keyHandle;
	request.algorithm = input.algorithm;
	request.dataSize = input.dataSize;
	if (input.dataSize != 0 && input.data != NULL && input.dataSize < 0x10000) {
		request.data = MarlinAllocateMemory(NULL, input.dataSize);
		ret = ksceKernelCopyFromUser((void *)request.data, input.data, input.dataSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.data = NULL;
	}
	request.expectedSize = input.expectedSize;
	if (input.expectedSize != 0 && input.expectedData != NULL &&
	    input.expectedSize < 0x10000) {
		request.expectedData = MarlinAllocateMemory(NULL, input.expectedSize);
		ret = ksceKernelCopyFromUser((void *)request.expectedData,
					    input.expectedData, input.expectedSize);
		if (ret < 0)
			goto cleanup;
	} else {
		request.expectedData = NULL;
	}
	request.verified = 0;
	ret = ksceKernelRunWithStack(0x4000,
				   (int (*)(void *))MarlinKernelVerifyDigest, &request);
	if (ret >= 0)
		ret = ksceKernelCopyToUser(&userRequest->verified, &request.verified,
					  sizeof(request.verified));
cleanup:
	MarlinFreeMemory(NULL, (void *)request.data);
	MarlinFreeMemory(NULL, (void *)request.expectedData);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciDisableKeyChecked(unsigned int handle, const MarlinUserDataInput *userInput,
			       unsigned int *outputHandle)
{
	MarlinBuffer *buffer = NULL;
	unsigned int output = 0;
	MarlinUserDataInput input = { 0, NULL };
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0) {
		ret = ksceKernelCopyFromUser(&input, userInput, sizeof(input));
		if (ret >= 0) {
			if (input.size < 0x10000) {
				buffer = MarlinBufferCreate(NULL, input.size, NULL);
				ret = ksceKernelCopyFromUser(buffer->data, input.data, input.size);
				if (ret < 0)
					goto cleanup;
			}
			ret = MarlinKernelDisableKey(handle, (unsigned int)buffer, &output);
			if (ret >= 0)
				ret = ksceKernelCopyToUser(outputHandle, &output, sizeof(output));
		}
	}
cleanup:
	MarlinBufferDestroy(NULL, buffer);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciStoreDataChecked(int selector, unsigned int size, const void *userData)
{
	void *memory = NULL;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret < 0)
		goto cleanup;
	if (userData != NULL && size != 0 && size < 0x10000) {
		memory = MarlinAllocateMemory(NULL, size);
		ret = ksceKernelCopyFromUser(memory, userData, size);
		if (ret < 0)
			goto cleanup;
	}
	ret = MarlinKernelStoreData(selector, size, memory);
cleanup:
	MarlinFreeMemory(NULL, memory);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciCheckStoredChecked(int selector)
{
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0)
		ret = MarlinKernelCheckStored(selector);
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciGetHandleSizeChecked(unsigned int handle, unsigned int *outputSize)
{
	unsigned int size = 0;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0) {
		ret = MarlinKernelGetHandleSize(handle, &size);
		if (ret >= 0)
			ret = ksceKernelCopyToUser(outputSize, &size, sizeof(size));
	}
	sceSyscallLeave(state);
	return ret;
}

int MarlinHciTakeHandleObjectChecked(unsigned int handle, unsigned int unused, void *output)
{
	MarlinBuffer *buffer = NULL;
	unsigned int state = sceSyscallEnter();
	int ret = MarlinHciCheckAccess();

	if (ret >= 0) {
		ret = MarlinKernelTakeHandleObject(handle, unused, (void **)&buffer);
		if (ret >= 0) {
			if (buffer != NULL) {
				ret = ksceKernelCopyToUser(output, buffer->data, buffer->size);
				if (ret < 0)
					goto done;
			}
			MarlinBufferDestroy(NULL, buffer);
		}
	}
done:
	sceSyscallLeave(state);
	return ret;
}
