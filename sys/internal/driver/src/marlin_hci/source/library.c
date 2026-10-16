#include <psp2kern/kernel/threadmgr/mutex.h>

#include "key.h"
#include "storage.h"
#include "search.h"

extern MarlinBuffer *marlinStoredData;
int MarlinMemoryInitialize(void);
void MarlinMemoryTerminate(void);
int MarlinKeyLoadStoredPair(MarlinKey *key, unsigned int selector);
int MarlinKeyLoadStoredRaw(MarlinKey *key);
int MarlinRsaSign(MarlinRsaContext *context, int algorithm,
		  const MarlinBuffer *data, MarlinBuffer **signature);
int MarlinKeyDecryptRsa(MarlinKey *key, unsigned int mode,
		       const MarlinBuffer *label, const MarlinBuffer *encrypted,
		       MarlinBuffer **output);
int MarlinKeyTransformBuffer(MarlinKey *key, unsigned int algorithm,
			     unsigned int mode, const void *counter,
			     const MarlinBuffer *input, MarlinBuffer **output);
int MarlinKeyUnwrapRsa(MarlinKey *wrappingKey, unsigned int mode,
		       const MarlinBuffer *label, const MarlinBuffer *encrypted,
		       MarlinKey *key);
int MarlinKeyUnwrap(MarlinKey *wrappingKey, unsigned int mode,
		    const void *iv, unsigned int ivSize, const void *encrypted,
		    unsigned int encryptedSize, MarlinKey *key);
int MarlinSha1Digest(const void *data, unsigned int size, void *digest);
int MarlinSha256Digest(const void *data, unsigned int size, void *digest);
int MarlinCipherTransformBlock(const void *key, const void *input, void *output);
int MarlinDecryptData(const void *iv, const void *key, const void *encrypted,
		      unsigned int size, void *data, unsigned int *dataSize);
int MarlinBufferObfuscate(MarlinBuffer *buffer);
void MarlinEraseMemory(void *data, unsigned int size);
int MarlinHmacDigest(unsigned int algorithm, const MarlinBuffer *key,
		     const MarlinBuffer *data, MarlinBuffer **digest);
int MarlinRsaEncrypt(MarlinRsaContext *context, const MarlinBuffer *data,
		     MarlinBuffer **output);

SceUID marlinHciMutex = -1;

int MarlinInitialize(void)
{
	unsigned int result = 0;

	marlinHciMutex = ksceKernelCreateMutex("SceMarlinHciMutex", 0, 0, 0);
	if (marlinHciMutex < 0)
		result = marlinHciMutex;
	if (result == 0)
		result = MarlinMemoryInitialize();
	return result;
}

void MarlinTerminate(void)
{
	MarlinBufferDestroy(0, marlinStoredData);
	marlinStoredData = 0;
	MarlinMemoryTerminate();
	ksceKernelDeleteMutex(marlinHciMutex);
	marlinHciMutex = -1;
	return;
}

int MarlinLock(void *context)
{
	unsigned int result = 0;

	result = ksceKernelLockMutex(marlinHciMutex, 1, 0);
	return result;
}

int MarlinUnlock(void *context)
{
	int result = 0;

	MarlinBufferDestroy(0, marlinStoredData);
	marlinStoredData = 0;
	ksceKernelUnlockMutex(marlinHciMutex, 1);
	return result;
}

int MarlinKeyImportStored(unsigned int kind, MarlinKey **output)
{
	int result = 0;
	MarlinKey *key = 0;

	if (output == 0) {
		result = 3;
		goto out;
	}
	result = MarlinKeyCreate(kind, &key);
	if (result != 0)
		goto out;
	switch (kind) {
	case 3:
		result = MarlinKeyLoadStoredRaw(key);
		if (result != 0)
			goto out;
		break;
	case 2:
		result = MarlinKeyLoadStoredPair(key, kind);
		if (result != 0)
			goto out;
		break;
	case 0:
		result = MarlinKeyLoadStoredPair(key, kind);
		if (result != 0)
			goto out;
		break;
	case 1:
		result = MarlinKeyLoadStoredPair(key, kind);
		if (result != 0)
			goto out;
		break;
	default:
		result = 3;
		goto out;
	}
	*output = key;
	key = 0;
out:
	MarlinKeyDestroy(key);
	return result;
}

unsigned int MarlinKeyGetOperationsEnabled(const MarlinKey *key)
{
	const MarlinKey *target = key;

	if (target == 0)
		return 0;
	return target->operationsEnabled;
}

int MarlinKeyRelease(MarlinKey *key)
{
	int result = 0;
	MarlinKey *target = key;

	MarlinKeyDestroy(target);
	return result;
}

int MarlinKeySign(MarlinKey *key, unsigned int algorithm,
		  const MarlinBuffer *data, MarlinBuffer **signature)
{
	int result = 0;
	MarlinKey *target = key;
	MarlinBuffer *buffer = 0;
	MarlinBuffer *rawKey = 0;
	MarlinRsaKeyComponents *components = 0;
	MarlinRsaContext *context = 0;

	if (key == 0 || data == 0 || signature == 0) {
		result = 3;
		goto out;
	}
	if (target->kind != 1) {
		result = 3;
		goto out;
	}
	result = MarlinBufferDeobfuscateCopy(target->data, &rawKey);
	if (result == 0) {
		result = MarlinRsaParseKey(rawKey, &components);
		if (result != 0) {
			result = result == 2 ? 2 : 0x23110004;
		} else {
			result = MarlinRsaCreate(&context, &components->modulus,
					&components->publicExponent,
					&components->privateExponent, &components->primeP,
					&components->primeQ, &components->exponentP,
					&components->exponentQ, &components->coefficient);
			if (result == 0) {
				if (algorithm == 1) {
					if (MarlinRsaSign(context, 1, data, &buffer) != 0) {
						result = 0x23110001;
						goto out;
					}
				} else if (algorithm == 0) {
					if (MarlinRsaSign(context, 0, data, &buffer) != 0) {
						result = 0x23110001;
						goto out;
					}
				} else {
					result = 3;
					goto out;
				}
				*signature = buffer;
				buffer = 0;
			}
		}
	}
out:
	MarlinRsaDestroy(context);
	MarlinRsaFreeKey(components);
	MarlinBufferDestroy(0, buffer);
	MarlinBufferEraseAndDestroy(rawKey);
	return result;
}

int MarlinKeyDecrypt(MarlinKey *key, unsigned int algorithm,
		     unsigned int mode, const void *counter,
		     const MarlinBuffer *label, const MarlinBuffer *encrypted,
		     MarlinBuffer **output)
{
	int result = 0;
	MarlinKey *target = key;

	do {
		if (key == 0 || encrypted == 0 ||
		    (target->kind != 7 && target->kind != 8) || output == 0) {
			result = 3;
			break;
		}
		if (target->operationsEnabled == 0) {
			result = 0x23110002;
			break;
		}
		switch (algorithm) {
		case 0:
			result = MarlinKeyDecryptRsa(target, algorithm, 0, encrypted, output);
			if (result != 0)
				break;
			break;
		case 1:
			result = MarlinKeyDecryptRsa(target, algorithm, label, encrypted, output);
			if (result != 0)
				break;
			break;
		case 2:
		case 3:
			result = MarlinKeyTransformBuffer(target, algorithm, mode,
						 counter, encrypted, output);
			if (result != 0)
				break;
			break;
		default:
			result = 3;
			break;
		}
	} while (0);
	return result;
}

int MarlinKeyImportWrapped(MarlinKey *wrappingKey, unsigned int algorithm,
			   const MarlinBuffer *label, const MarlinBuffer *input,
			   unsigned int kind, MarlinKey **output)
{
	int result = 0;
	MarlinKey *target = wrappingKey;
	MarlinKey *key = 0;

	if (wrappingKey == 0 || input == 0) {
		result = 3;
		goto out;
	}
	if (kind == 0 || kind == 1 || kind == 2 || kind == 3 || kind == 4) {
		result = 3;
		goto out;
	}
	result = MarlinKeyCreate(kind, &key);
	if (result == 0) {
		if (algorithm == 1 || algorithm == 0) {
			result = MarlinKeyUnwrapRsa(target, algorithm, label, input, key);
			if (result != 0)
				goto out;
		} else if (algorithm == 2) {
			const void *iv = input->data;
			int ivSize = 16;
			const void *encrypted = 0;
			unsigned int encryptedSize = 0;

			if (input->size <= 47) {
				result = 3;
				goto out;
			}
			encrypted = input->data + 16;
			encryptedSize = input->size - 16;
			result = MarlinKeyUnwrap(target, algorithm, iv, ivSize,
						encrypted, encryptedSize, key);
			if (result != 0)
				goto out;
		} else {
			result = 3;
			goto out;
		}
		key->operationsEnabled = 1;
		*output = key;
		key = 0;
	}
out:
	MarlinKeyDestroy(key);
	return result;
}

int MarlinKeyImportDerived(unsigned int iterations, unsigned int seedIndex,
			   const MarlinBuffer *wrappedSeed, const MarlinBuffer *check,
			   const MarlinBuffer *iv, const MarlinBuffer *encrypted,
			   MarlinKey **output)
{
	int result;
	void *data;
	void *derived;
	MarlinBuffer *buffer;
	unsigned int index;
	const unsigned char *seeds;
	const void *source;
	MarlinKey *key;
	MarlinBuffer *stored;
	unsigned int size;
	unsigned char seed[16];
	unsigned char digest[20];

	result = 0;
	key = 0;
	seeds = 0;
	source = 0;
	data = 0;
	derived = 0;
	stored = 0;
	buffer = 0;
	size = 0;
	if (iterations > 16 || seedIndex > 0x1000 || wrappedSeed == 0 ||
	    wrappedSeed->size != 16 || check == 0 || iv == 0 || iv->size != 16 ||
	    encrypted == 0 || encrypted->size != 32 || output == 0) {
		result = 3;
		goto out;
	}
	result = MarlinKeyCreate(7, &key);
	if (result == 0) {
		result = MarlinGetStoredSections(2, 0, &stored);
		if (result == 0) {
			seeds = stored->data;
			source = seeds + 16 * seedIndex;
			MarlinCopyMemory(seed, source, 16);
			for (index = 0; index < iterations; index++) {
				MarlinSha1Digest(seed, 16, digest);
				MarlinCopyMemory(seed, digest, 16);
			}
			data = MarlinAllocateMemory(0, 16);
			if (data == 0) {
				result = 2;
				goto out;
			}
			MarlinFillMemory(data, 0, 16);
			size = 16;
			derived = MarlinAllocateMemory(0, 16);
			if (derived == 0) {
				result = 2;
				goto out;
			}
			MarlinFillMemory(derived, 0, 16);
			if (MarlinCipherTransformBlock(seed, wrappedSeed->data, derived) != 0) {
				result = 0x23110003;
				goto out;
			}
			MarlinSha1Digest(derived, 16, digest);
			if (MarlinCompareMemory(digest, check->data, 16) != 0) {
				result = 3;
				goto out;
			}
			if (MarlinDecryptData(iv->data, derived, encrypted->data,
					      encrypted->size, data, &size) != 0) {
				result = 0x23110003;
				goto out;
			}
			buffer = MarlinBufferCreate(0, size, data);
			if (buffer == 0) {
				result = 2;
				goto out;
			}
			result = MarlinBufferGetKeyFormat(buffer, &key->format);
			if (result == 0) {
				result = MarlinBufferObfuscate(buffer);
				if (result == 0) {
					key->data = buffer;
					buffer = 0;
					key->operationsEnabled = 1;
					*output = key;
					key = 0;
				}
			}
		}
	}
out:
	MarlinBufferEraseAndDestroy(buffer);
	MarlinBufferEraseAndDestroy(stored);
	if (data != 0) {
		MarlinEraseMemory(data, size);
		MarlinFreeMemory(0, data);
	}
	if (derived != 0) {
		MarlinEraseMemory(derived, 16);
		MarlinFreeMemory(0, derived);
	}
	MarlinKeyDestroy(key);
	return result;
}

int MarlinKeyVerifyMac(MarlinKey *key, unsigned int algorithm,
		       const MarlinBuffer *data, const MarlinBuffer *expected,
		       unsigned int *verified)
{
	int result;
	unsigned int selector;
	MarlinKey *target;
	MarlinBuffer *buffer;
	MarlinBuffer *rawKey;

	result = 0;
	buffer = 0;
	target = key;
	rawKey = 0;
	if (data == 0 || expected == 0 || target == 0 || verified == 0) {
		result = 3;
		goto out;
	}
	if (target->operationsEnabled == 0) {
		result = 0x23110002;
		goto out;
	}
	switch (algorithm) {
	case 0:
		selector = 1;
		break;
	case 1:
		selector = 2;
		break;
	default:
		result = 3;
		goto out;
	}
	result = MarlinBufferDeobfuscateCopy(target->data, &rawKey);
	if (result == 0) {
		result = MarlinHmacDigest(selector, rawKey, data, &buffer);
		if (result == 0) {
			if (MarlinBufferCompare(0, expected, buffer) == 0)
				*verified = 1;
			else
				*verified = 0;
		}
	}
out:
	MarlinBufferDestroy(0, buffer);
	MarlinBufferEraseAndDestroy(rawKey);
	return result;
}

int MarlinKeyVerifyDigest(MarlinKey *key, unsigned int algorithm,
			  const MarlinBuffer *prefix, const MarlinBuffer *expected,
			  unsigned int *verified)
{
	int result;
	unsigned char *data;
	unsigned int size;
	unsigned int digestSize;
	MarlinKey *target;
	unsigned char digest[32];
	MarlinBuffer *rawKey;

	result = 0;
	target = key;
	data = 0;
	rawKey = 0;
	size = 0;
	digestSize = 0;
	if (expected == 0 || target == 0 || verified == 0) {
		result = 3;
		goto out;
	}
	if (target->operationsEnabled == 0) {
		result = 0x23110002;
		goto out;
	}
	result = MarlinBufferDeobfuscateCopy(target->data, &rawKey);
	if (result == 0) {
		if (prefix != 0) {
			data = MarlinAllocateMemory(0, prefix->size + rawKey->size);
			if (data == 0) {
				result = 2;
				goto out;
			}
			MarlinCopyMemory(data, prefix->data, prefix->size);
			MarlinCopyMemory(data + prefix->size, rawKey->data, rawKey->size);
			size = prefix->size + rawKey->size;
		} else {
			data = rawKey->data;
			size = rawKey->size;
		}
		switch (algorithm) {
		case 0:
			MarlinSha1Digest(data, size, digest);
			digestSize = 20;
			break;
		case 1:
			MarlinSha256Digest(data, size, digest);
			digestSize = 32;
			break;
		default:
			result = 3;
			goto out;
		}
		if (MarlinCompareDataRanges(0, expected->size, expected->data,
					    digestSize, digest) == 0)
			*verified = 1;
		else
			*verified = 0;
	}
out:
	if (prefix != 0 && data != 0) {
		MarlinEraseMemory(data + prefix->size, rawKey->size);
		MarlinFreeMemory(0, data);
	}
	MarlinBufferEraseAndDestroy(rawKey);
	return result;
}

int MarlinKeyDisable(MarlinKey *key, unsigned int unused1, MarlinBuffer **unused2)
{
	int result = 0;
	MarlinKey *target = key;

	if (target == 0 || target->kind != 7) {
		result = 3;
	} else if (target->operationsEnabled == 0) {
		result = 0x23110002;
	} else {
		target->operationsEnabled = 0;
		MarlinBufferEraseAndDestroy(target->data);
		target->data = 0;
	}
	return result;
}

int MarlinKeyExportEncrypted(MarlinKey *key, MarlinBuffer **output)
{
	int result = 0;
	MarlinKey *target = key;
	MarlinBuffer *buffer = 0;

	if (key == 0 || output == 0) {
		result = 3;
	} else if (target->operationsEnabled == 0) {
		result = 0x23110002;
	} else if (target->format == 2) {
		result = MarlinBufferDeobfuscateCopy(target->data, &buffer);
		if (result == 0) {
			*output = buffer;
			buffer = 0;
		}
	} else {
		*output = 0;
	}
	MarlinBufferEraseAndDestroy(buffer);
	return result;
}

int MarlinKeyWrapRsa(MarlinKey *key, const MarlinBuffer *modulus,
		     const MarlinBuffer *publicExponent, MarlinBuffer **output)
{
	int result = 0;
	MarlinKey *target = key;
	MarlinRsaContext *context = 0;
	MarlinBuffer *buffer = 0;

	if (key == 0 || modulus == 0 || publicExponent == 0 ||
	    target->kind != 7 || output == 0) {
		result = 3;
	} else if (target->operationsEnabled == 0) {
		result = 0x23110002;
	} else {
		result = MarlinRsaCreate(&context, modulus, publicExponent,
					0, 0, 0, 0, 0, 0);
		if (result == 0) {
			result = MarlinBufferDeobfuscateCopy(target->data, &buffer);
			if (result == 0)
				result = MarlinRsaEncrypt(context, buffer, output);
		}
	}
	MarlinRsaDestroy(context);
	MarlinBufferEraseAndDestroy(buffer);
	return result;
}

int MarlinExportStoredList(MarlinBuffer ***output, unsigned int *count)
{
	int result = 0;
	int remaining = 2;
	int index = 0;
	MarlinBuffer **buffers = 0;

	if (output == 0 || count == 0) {
		result = 3;
	} else {
		buffers = MarlinAllocateMemory(0, remaining * sizeof(*buffers));
		if (buffers == 0) {
			result = 2;
		} else {
			MarlinFillMemory(buffers, 0, remaining * sizeof(*buffers));
			result = MarlinGetStoredSections(1, buffers, 0);
			if (result == 0) {
				result = MarlinGetStoredSections(0, buffers + 1, 0);
				if (result == 0) {
					*output = buffers;
					buffers = 0;
					*count = remaining;
					remaining = 0;
				}
			}
		}
	}
	if (buffers != 0) {
		for (index = 0; index < remaining; index++)
			MarlinBufferDestroy(0, buffers[index]);
		MarlinFreeMemory(0, buffers);
	}
	return result;
}

int MarlinExportStoredInfo(MarlinBuffer **output)
{
	int result = 0;
	MarlinBuffer *buffer = 0;

	if (output == 0) {
		result = 3;
	} else {
		result = MarlinGetStoredSections(2, &buffer, 0);
		if (result == 0) {
			*output = buffer;
			buffer = 0;
		}
	}
	MarlinBufferDestroy(0, buffer);
	return result;
}

int MarlinStoreData(int selector, unsigned int size, const void *data)
{
	int result = 0;

	result = MarlinUpdateStoredData(selector, size, data);
	return result;
}

int MarlinCheckStored(int selector)
{
	int result = 0;

	result = MarlinCheckStoredData(selector);
	return result;
}
