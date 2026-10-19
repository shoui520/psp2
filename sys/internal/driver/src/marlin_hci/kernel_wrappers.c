/* Descriptive source name: the original module has no debug records. */
#include "key.h"
#include "handles.h"
#include "kernel_wrappers.h"

int MarlinKernelLock(void)
{
	int context = 0;

	return MarlinLock(&context);
}

int MarlinKernelUnlock(void)
{
	return MarlinUnlock(0);
}

int MarlinKernelImportStoredKey(unsigned int kind, unsigned int *outputHandle)
{
	MarlinKey *key = 0;
	unsigned int handle = 0;
	int result;

	if (outputHandle != 0) {
		result = MarlinKeyImportStored(kind, &key);
		if (result == 0) {
			result = MarlinHandleCreate(1, 4, key, &handle);
			if (result == 0)
				*outputHandle = handle;
		}
	} else {
		result = 3;
	}
	return result;
}

unsigned int MarlinKernelGetKeyOperationsEnabled(unsigned int handle)
{
	unsigned int type = 0;
	unsigned int size = 0;
	void *object = 0;

	MarlinHandleGet(handle, &type, &size, &object);
	return MarlinKeyGetOperationsEnabled(object);
}

int MarlinKernelReleaseKey(unsigned int handle)
{
	unsigned int type = 0;
	unsigned int size = 0;
	void *object = 0;
	int result;
	int releaseResult;

	result = MarlinHandleGet(handle, &type, &size, &object);
	if (result == 0) {
		releaseResult = MarlinKeyRelease(object);
		if (releaseResult == 0)
			MarlinHandleRelease(handle);
		else
			result = releaseResult;
	}
	return result;
}

int MarlinKernelExportStoredList(MarlinStoredHandleList *output)
{
	unsigned int handle = 0;
	MarlinBuffer **buffers = 0;
	unsigned int count = 0;
	unsigned int index;
	int result;

	if (output == 0) {
		result = 3;
		goto cleanup;
	}
	result = MarlinExportStoredList(&buffers, &count);
	if (result != 0)
		goto cleanup;
	for (index = 0; index < count; index++) {
		result = MarlinHandleCreate(4, buffers[index]->size,
					  buffers[index], &handle);
		if (result != 0)
			goto cleanup;
		output->handles[index] = handle;
		buffers[index] = 0;
	}
	output->count = count;
	result = 0;
cleanup:
	for (index = 0; index < count; index++) {
		if (buffers[index] != 0)
			MarlinBufferDestroy(0, buffers[index]);
	}
	MarlinFreeMemory(0, buffers);
	return result;
}

int MarlinKernelImportWrappedKey(MarlinWrappedKeyImport *request)
{
	unsigned int type = 0;
	unsigned int size = 0;
	unsigned int handle = 0;
	void *object = 0;
	MarlinKey *key = 0;
	MarlinKey *wrappingKey;
	MarlinBuffer *label;
	MarlinBuffer *input;
	int result;

	if (request == 0)
		return 3;
	result = MarlinHandleGet(request->wrappingKeyHandle, &type, &size, &object);
	if (result != 0)
		return result;
	wrappingKey = object;
	if (request->labelSize > 0x10000)
		return 3;
	if (request->labelSize != 0 && request->labelData != 0) {
		label = MarlinBufferCreate(0, request->labelSize, request->labelData);
		if (label == 0)
			return 2;
	} else {
		label = 0;
	}
	if (request->inputSize > 0x10000) {
		result = 3;
		input = 0;
		goto cleanup;
	}
	if (request->inputSize != 0 && request->inputData != 0) {
		input = MarlinBufferCreate(0, request->inputSize, request->inputData);
		if (input == 0) {
			result = 2;
			goto cleanup;
		}
	} else {
		input = 0;
	}
	result = MarlinKeyImportWrapped(wrappingKey, request->algorithm, label,
					input, request->kind, &key);
	if (result == 0) {
		result = MarlinHandleCreate(1, 4, key, &handle);
		if (result == 0) {
			key = 0;
			request->outputHandle = handle;
		}
	}
cleanup:
	if (label != 0)
		MarlinBufferDestroy(0, label);
	if (input != 0)
		MarlinBufferDestroy(0, input);
	return result;
}

int MarlinKernelExportStoredInfo(unsigned int *outputHandle)
{
	MarlinBuffer *buffer = 0;
	unsigned int handle = 0;
	int result = 3;

	if (outputHandle != 0) {
		result = MarlinExportStoredInfo(&buffer);
		if (result == 0) {
			result = MarlinHandleCreate(4, buffer->size, buffer, &handle);
			if (result == 0)
				*outputHandle = handle;
		}
	}
	return result;
}

int MarlinKernelExportEncryptedKey(unsigned int keyHandle,
				    unsigned int *outputHandle)
{
	unsigned int type = 0;
	unsigned int size = 0;
	void *object = 0;
	MarlinBuffer *buffer = 0;
	unsigned int handle = 0;
	int result = 3;
	int exportResult;

	if (outputHandle == 0)
		goto finish;
	result = MarlinHandleGet(keyHandle, &type, &size, &object);
	if (result == 0) {
		exportResult = MarlinKeyExportEncrypted(object, &buffer);
		if (exportResult == 0) {
			result = MarlinHandleCreate(4, buffer->size, buffer, &handle);
			if (result == 0) {
				*outputHandle = handle;
				goto finish;
			}
		} else {
			result = exportResult;
		}
	}
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
finish:
	return result;
}

int MarlinKernelImportDerivedKey(MarlinDerivedKeyImport *request)
{
	unsigned int handle = 0;
	MarlinKey *key = 0;
	MarlinBuffer *wrappedSeed;
	MarlinBuffer *check = 0;
	MarlinBuffer *iv = 0;
	MarlinBuffer *encrypted = 0;
	int result;

	if (request == 0 || request->wrappedSeedSize > 0x10000)
		return 3;
	if (request->wrappedSeedSize != 0 && request->wrappedSeedData != 0) {
		wrappedSeed = MarlinBufferCreate(0, request->wrappedSeedSize,
					       request->wrappedSeedData);
		if (wrappedSeed == 0)
			return 2;
	} else {
		wrappedSeed = 0;
	}
	if (request->checkSize > 0x10000) {
		result = 3;
		goto cleanup;
	}
	if (request->checkSize != 0 && request->checkData != 0) {
		check = MarlinBufferCreate(0, request->checkSize, request->checkData);
		if (check == 0) {
			result = 2;
			goto cleanup;
		}
	}
	if (request->ivSize > 0x10000) {
		result = 3;
		goto cleanup;
	}
	if (request->ivSize != 0 && request->ivData != 0) {
		iv = MarlinBufferCreate(0, request->ivSize, request->ivData);
		if (iv == 0) {
			result = 2;
			goto cleanup;
		}
	}
	result = 3;
	if (request->encryptedSize > 0x10000)
		goto cleanup;
	if (request->encryptedSize != 0) {
		if (request->encryptedData == 0)
			goto cleanup;
		encrypted = MarlinBufferCreate(0, request->encryptedSize,
					      request->encryptedData);
		if (encrypted == 0) {
			result = 2;
			goto cleanup;
		}
	}
	result = MarlinKeyImportDerived(request->iterations, request->seedIndex,
					wrappedSeed, check, iv, encrypted, &key);
	if (result == 0) {
		result = MarlinHandleCreate(1, 4, key, &handle);
		if (result == 0) {
			key = 0;
			request->outputHandle = handle;
		}
	}
cleanup:
	if (wrappedSeed != 0)
		MarlinBufferDestroy(0, wrappedSeed);
	if (check != 0)
		MarlinBufferDestroy(0, check);
	if (iv != 0)
		MarlinBufferDestroy(0, iv);
	if (encrypted != 0)
		MarlinBufferDestroy(0, encrypted);
	return result;
}

int MarlinKernelSign(MarlinKeySignRequest *request)
{
	unsigned int type = 0;
	unsigned int size = 0;
	unsigned int handle = 0;
	void *object = 0;
	MarlinBuffer *signature = 0;
	MarlinKey *key;
	MarlinBuffer *data;
	int result = 3;

	if (request == 0)
		goto finish;
	result = MarlinHandleGet(request->keyHandle, &type, &size, &object);
	if (result != 0)
		goto cleanupSignature;
	key = object;
	if (request->dataSize > 0x10000) {
		result = 3;
		goto cleanupSignature;
	}
	if (request->dataSize != 0 && request->data != 0) {
		data = MarlinBufferCreate(0, request->dataSize, request->data);
		if (data == 0) {
			result = 2;
			goto cleanupSignature;
		}
	} else {
		data = 0;
	}
	result = MarlinKeySign(key, request->algorithm, data, &signature);
	if (result == 0) {
		result = MarlinHandleCreate(4, signature->size, signature, &handle);
		if (result == 0) {
			signature = 0;
			request->outputHandle = handle;
		}
	}
	if (data != 0)
		MarlinBufferDestroy(0, data);
cleanupSignature:
	if (signature != 0)
		MarlinBufferDestroy(0, signature);
finish:
	return result;
}

int MarlinKernelDecrypt(MarlinKeyDecryptRequest *request)
{
	unsigned int type = 0;
	unsigned int size = 0;
	unsigned int handle = 0;
	void *object = 0;
	MarlinBuffer *output = 0;
	MarlinKey *key;
	MarlinBuffer *label;
	MarlinBuffer *encrypted;
	int result = 3;

	if (request == 0)
		goto finish;
	result = MarlinHandleGet(request->keyHandle, &type, &size, &object);
	if (result != 0)
		goto cleanupOutput;
	key = object;
	if (request->labelSize > 0x10000) {
		result = 3;
		goto cleanupOutput;
	}
	if (request->labelSize != 0 && request->labelData != 0) {
		label = MarlinBufferCreate(0, request->labelSize, request->labelData);
		if (label == 0) {
			result = 2;
			goto cleanupOutput;
		}
	} else {
		label = 0;
	}
	if (request->encryptedSize > 0x10000) {
		result = 3;
		encrypted = 0;
		goto cleanupInputs;
	}
	if (request->encryptedSize != 0 && request->encryptedData != 0) {
		encrypted = MarlinBufferCreate(0, request->encryptedSize,
					      request->encryptedData);
		if (encrypted == 0) {
			result = 2;
			goto cleanupInputs;
		}
	} else {
		encrypted = 0;
	}
	result = MarlinKeyDecrypt(key, request->algorithm, request->mode,
				  request->counter, label, encrypted, &output);
	if (result == 0) {
		result = MarlinHandleCreate(4, output->size, output, &handle);
		if (result == 0) {
			output = 0;
			request->outputHandle = handle;
		}
	}
cleanupInputs:
	if (label != 0)
		MarlinBufferDestroy(0, label);
	if (encrypted != 0)
		MarlinBufferDestroy(0, encrypted);
cleanupOutput:
	if (output != 0)
		MarlinBufferDestroy(0, output);
finish:
	return result;
}

int MarlinKernelWrapRsa(MarlinKeyWrapRsaRequest *request)
{
	unsigned int type = 0;
	unsigned int size = 0;
	unsigned int handle = 0;
	void *object = 0;
	MarlinBuffer *output = 0;
	MarlinKey *key;
	MarlinBuffer *modulus;
	MarlinBuffer *publicExponent;
	int result = 3;

	if (request == 0)
		goto finish;
	result = MarlinHandleGet(request->keyHandle, &type, &size, &object);
	if (result != 0)
		goto cleanupOutput;
	key = object;
	if (request->modulusSize > 0x10000) {
		result = 3;
		goto cleanupOutput;
	}
	if (request->modulusSize != 0 && request->modulusData != 0) {
		modulus = MarlinBufferCreate(0, request->modulusSize, request->modulusData);
		if (modulus == 0) {
			result = 2;
			goto cleanupOutput;
		}
	} else {
		modulus = 0;
	}
	if (request->publicExponentSize > 0x10000) {
		result = 3;
		publicExponent = 0;
		goto cleanupInputs;
	}
	if (request->publicExponentSize != 0 && request->publicExponentData != 0) {
		publicExponent = MarlinBufferCreate(0, request->publicExponentSize,
					      request->publicExponentData);
		if (publicExponent == 0) {
			result = 2;
			goto cleanupInputs;
		}
	} else {
		publicExponent = 0;
	}
	result = MarlinKeyWrapRsa(key, modulus, publicExponent, &output);
	if (result == 0) {
		result = MarlinHandleCreate(4, output->size, output, &handle);
		if (result == 0) {
			output = 0;
			request->outputHandle = handle;
		}
	}
cleanupInputs:
	if (modulus != 0)
		MarlinBufferDestroy(0, modulus);
	if (publicExponent != 0)
		MarlinBufferDestroy(0, publicExponent);
cleanupOutput:
	if (output != 0)
		MarlinBufferDestroy(0, output);
finish:
	return result;
}

int MarlinKernelVerifyMac(MarlinKeyVerifyRequest *request)
{
	unsigned int type = 0;
	unsigned int size = 0;
	void *object = 0;
	unsigned int verified = 0;
	MarlinKey *key;
	MarlinBuffer *data;
	MarlinBuffer *expected;
	int result;

	if (request == 0)
		return 3;
	result = MarlinHandleGet(request->keyHandle, &type, &size, &object);
	if (result != 0)
		return result;
	key = object;
	if (request->dataSize > 0x10000)
		return 3;
	if (request->dataSize != 0 && request->data != 0) {
		data = MarlinBufferCreate(0, request->dataSize, request->data);
		if (data == 0)
			return 2;
	} else {
		data = 0;
	}
	if (request->expectedSize > 0x10000) {
		result = 3;
		expected = 0;
		goto cleanup;
	}
	if (request->expectedSize != 0 && request->expectedData != 0) {
		expected = MarlinBufferCreate(0, request->expectedSize,
					     request->expectedData);
		if (expected == 0) {
			result = 2;
			goto cleanup;
		}
	} else {
		expected = 0;
	}
	result = MarlinKeyVerifyMac(key, request->algorithm, data, expected, &verified);
	if (result == 0) {
		request->verified = verified;
		result = 0;
	}
cleanup:
	if (data != 0)
		MarlinBufferDestroy(0, data);
	if (expected != 0)
		MarlinBufferDestroy(0, expected);
	return result;
}

int MarlinKernelVerifyDigest(MarlinKeyVerifyRequest *request)
{
	unsigned int type = 0;
	unsigned int size = 0;
	void *object = 0;
	unsigned int verified = 0;
	MarlinKey *key;
	MarlinBuffer *data;
	MarlinBuffer *expected;
	int result;

	if (request == 0)
		return 3;
	result = MarlinHandleGet(request->keyHandle, &type, &size, &object);
	if (result != 0)
		return result;
	key = object;
	if (request->dataSize > 0x10000)
		return 3;
	if (request->dataSize != 0 && request->data != 0) {
		data = MarlinBufferCreate(0, request->dataSize, request->data);
		if (data == 0)
			return 2;
	} else {
		data = 0;
	}
	if (request->expectedSize > 0x10000) {
		result = 3;
		expected = 0;
		goto cleanup;
	}
	if (request->expectedSize != 0 && request->expectedData != 0) {
		expected = MarlinBufferCreate(0, request->expectedSize,
					     request->expectedData);
		if (expected == 0) {
			result = 2;
			goto cleanup;
		}
	} else {
		expected = 0;
	}
	result = MarlinKeyVerifyDigest(key, request->algorithm, data, expected, &verified);
	if (result == 0) {
		request->verified = verified;
		result = 0;
	}
cleanup:
	if (data != 0)
		MarlinBufferDestroy(0, data);
	if (expected != 0)
		MarlinBufferDestroy(0, expected);
	return result;
}

int MarlinKernelDisableKey(unsigned int keyHandle, unsigned int unused,
				    unsigned int *outputHandle)
{
	unsigned int type = 0;
	unsigned int size = 0;
	void *object = 0;
	MarlinBuffer *buffer = 0;
	unsigned int handle = 0;
	int result = 3;
	int disableResult;

	if (outputHandle == 0)
		goto finish;
	result = MarlinHandleGet(keyHandle, &type, &size, &object);
	if (result == 0) {
		disableResult = MarlinKeyDisable(object, unused, &buffer);
		if (disableResult == 0) {
			result = MarlinHandleCreate(4, buffer->size, buffer, &handle);
			if (result == 0) {
				*outputHandle = handle;
				goto finish;
			}
		} else {
			result = disableResult;
		}
	}
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
finish:
	return result;
}

int MarlinKernelStoreData(int selector, unsigned int size, const void *data)
{
	void *copy;
	int result = 3;

	if (size <= 0x10000) {
		if (size != 0) {
			copy = MarlinAllocateMemory(0, size);
			if (copy != 0) {
				MarlinCopyMemory(copy, data, size);
				result = MarlinStoreData(selector, size, copy);
				MarlinFreeMemory(0, copy);
			} else {
				result = 2;
			}
		} else {
			result = MarlinStoreData(selector, 0, 0);
		}
	}
	return result;
}

int MarlinKernelCheckStored(int selector)
{
	return MarlinCheckStored(selector);
}

int MarlinKernelGetHandleSize(unsigned int handle, unsigned int *outputSize)
{
	unsigned int type = 0;
	unsigned int size = 0;
	int result;

	if (outputSize != 0) {
		result = MarlinHandleGet(handle, &type, &size, 0);
		if (result == 0)
			*outputSize = size;
	} else {
		result = 3;
	}
	return result;
}

int MarlinKernelTakeHandleObject(unsigned int handle, unsigned int unused,
				     void **output)
{
	unsigned int type = 0;
	unsigned int size = 0;
	void *object = 0;
	int result = 3;

	if (output != 0) {
		result = MarlinHandleGet(handle, &type, &size, &object);
		if (result == 0) {
			*output = object;
			MarlinHandleRelease(handle);
		}
	}
	return result;
}
