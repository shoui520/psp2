#include "key.h"
#include "storage.h"
#include "search.h"
#include "cipher.h"

int MarlinKeyLoadStoredPair(MarlinKey *key, unsigned int selector)
{
	int result = 0;
	MarlinKey *target;
	MarlinBuffer *stored = 0;
	MarlinBuffer *selected = 0;
	MarlinBuffer *first = 0;
	MarlinBuffer *second = 0;

	target = key;
	do {
		if (target == 0) {
			result = 3;
			break;
		}
		if (selector == 2) {
			result = MarlinGetStoredSections(1, 0, &selected);
			if (result != 0)
				break;
		} else if (selector == 0 || selector == 1) {
			result = MarlinLoadStoredData(1, &stored);
			if (result != 0) {
				result = result != 1 ? result : 0x23110005;
				break;
			}
			result = MarlinExtractStoredKeyPair(stored, &first, &second);
			if (result != 0)
				break;
			if (selector == 0) {
				selected = second;
				second = 0;
			} else {
				selected = first;
				first = 0;
			}
		} else {
			result = 3;
			break;
		}
		result = MarlinBufferObfuscate(selected);
		if (result == 0) {
			target->data = selected;
			selected = 0;
			target->format = 1;
		}
	} while (0);
	MarlinBufferDestroy(0, stored);
	MarlinBufferEraseAndDestroy(selected);
	MarlinBufferEraseAndDestroy(first);
	MarlinBufferEraseAndDestroy(second);
	return result;
}

int MarlinKeyLoadStoredRaw(MarlinKey *key)
{
	int result = 0;
	MarlinKey *target = key;
	MarlinBuffer *data = 0;

	if (target == 0) {
		result = 3;
	} else {
		result = MarlinGetStoredSections(0, 0, &data);
		if (result == 0) {
			result = MarlinBufferObfuscate(data);
			if (result == 0) {
				target->data = data;
				data = 0;
				target->format = 0;
			}
		}
	}
	MarlinBufferEraseAndDestroy(data);
	return result;
}

int MarlinKeyUnwrap(MarlinKey *wrappingKey, unsigned int mode,
		    const void *iv, unsigned int ivSize, const void *encrypted,
		    unsigned int encryptedSize, MarlinKey *key)
{
	int result;
	void *data;
	MarlinBuffer *buffer;
	int capacity;
	const unsigned char *keyData;
	unsigned int size;
	MarlinBuffer *rawKey;

	result = 0;
	data = 0;
	size = 0;
	capacity = 0;
	rawKey = 0;
	buffer = 0;
	keyData = 0;
	if (wrappingKey == 0) {
		result = 3;
		goto out;
	}
	if (iv == 0) {
		result = 3;
		goto out;
	}
	if (encrypted == 0) {
		result = 3;
		goto out;
	}
	if (ivSize != 16) {
		result = 3;
		goto out;
	}
	capacity = encryptedSize;
	size = capacity;
	data = MarlinAllocateMemory(0, capacity);
	if (data == 0) {
		result = 2;
		goto out;
	}
	MarlinFillMemory(data, 0, capacity);
	result = MarlinBufferDeobfuscateCopy(wrappingKey->data, &rawKey);
	if (result == 0) {
		keyData = rawKey->data;
		if (MarlinDecryptData(iv, keyData, encrypted, capacity, data, &size) != 0) {
			result = 0x23110003;
		} else {
			buffer = MarlinBufferCreate(0, size, data);
			if (buffer == 0) {
				result = 2;
			} else {
				result = MarlinBufferGetKeyFormat(buffer, &key->format);
				if (result == 0) {
					result = MarlinBufferObfuscate(buffer);
					if (result == 0) {
						key->data = buffer;
						buffer = 0;
					}
				}
			}
		}
	}
out:
	MarlinBufferEraseAndDestroy(buffer);
	if (data != 0) {
		MarlinEraseMemory(data, size);
		MarlinFreeMemory(0, data);
	}
	MarlinBufferEraseAndDestroy(rawKey);
	return result;
}

int MarlinKeyUnwrapRsa(MarlinKey *wrappingKey, unsigned int mode,
		       const MarlinBuffer *label, const MarlinBuffer *encrypted,
		       MarlinKey *key)
{
	int result = 0;
	MarlinKey *target = wrappingKey;
	MarlinBuffer *buffer = 0;
	MarlinRsaContext *context = 0;
	MarlinBuffer labelData;
	MarlinBuffer *rawKey;
	MarlinRsaKeyComponents *components;

	labelData.size = 0;
	labelData.data = 0;
	rawKey = 0;
	components = 0;
	if (target == 0) {
		result = 3;
		goto out;
	}
	if (encrypted == 0) {
		result = 3;
		goto out;
	}
	result = MarlinBufferDeobfuscateCopy(wrappingKey->data, &rawKey);
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
				if (mode == 0) {
					result = MarlinRsaDecryptPkcs1(context, encrypted, &buffer);
					if (result != 0)
						goto out;
				} else if (mode == 1) {
					if (label != 0) {
						labelData.data = label->data;
						labelData.size = label->size;
					}
					result = MarlinRsaDecryptOaep(context, encrypted,
								     &labelData, &buffer);
					if (result != 0)
						goto out;
				} else {
					result = 3;
					goto out;
				}
				result = MarlinBufferGetKeyFormat(buffer, &key->format);
				if (result == 0) {
					result = MarlinBufferObfuscate(buffer);
					if (result == 0) {
						key->data = buffer;
						buffer = 0;
					}
				}
			}
		}
	}
out:
	MarlinRsaDestroy(context);
	MarlinRsaFreeKey(components);
	MarlinBufferEraseAndDestroy(buffer);
	MarlinBufferEraseAndDestroy(rawKey);
	return result;
}

int MarlinKeyDecryptRsa(MarlinKey *key, unsigned int mode,
		       const MarlinBuffer *label, const MarlinBuffer *encrypted,
		       MarlinBuffer **output)
{
	int result = 0;
	MarlinRsaContext *context = 0;
	MarlinRsaKeyComponents *components = 0;
	MarlinBuffer *rawKey = 0;
	MarlinBuffer labelData;

	labelData.size = 0;
	labelData.data = 0;
	result = MarlinBufferDeobfuscateCopy(key->data, &rawKey);
	if (result != 0)
		goto out;
	result = MarlinRsaParseKey(rawKey, &components);
	if (result != 0) {
		result = result == 2 ? 2 : 0x23110004;
		goto out;
	}
	result = MarlinRsaCreate(&context, &components->modulus,
				&components->publicExponent,
				&components->privateExponent, &components->primeP,
				&components->primeQ, &components->exponentP,
				&components->exponentQ, &components->coefficient);
	if (result != 0)
		goto out;
	switch (mode) {
	case 0:
		result = MarlinRsaDecryptPkcs1(context, encrypted, output);
		if (result != 0)
			goto out;
		break;
	case 1:
		if (label != 0) {
			labelData.data = label->data;
			labelData.size = label->size;
		}
		result = MarlinRsaDecryptOaep(context, encrypted, &labelData, output);
		if (result != 0)
			goto out;
		break;
	default:
		result = 3;
		break;
	}
out:
	MarlinBufferEraseAndDestroy(rawKey);
	if (components != 0)
		MarlinRsaFreeKey(components);
	MarlinRsaDestroy(context);
	return result;
}

int MarlinKeyTransformBuffer(MarlinKey *key, unsigned int algorithm,
			     unsigned int mode, const void *counter,
			     const MarlinBuffer *input, MarlinBuffer **output)
{
	int result;
	MarlinBuffer *buffer;
	int selector;
	const unsigned char *data;
	unsigned int size;
	MarlinBuffer *rawKey;
	MarlinCipherContext *context;

	result = 0;
	buffer = 0;
	rawKey = 0;
	data = 0;
	size = 0;
	selector = 0;
	context = 0;
	size = input->size - 16;
	data = input->data + 16;
	buffer = MarlinBufferCreate(0, size, 0);
	if (buffer == 0) {
		result = 2;
		goto out;
	}
	if (algorithm == 2) {
		selector = 0x20201;
		if (mode == 1) {
			selector |= 0x2000000;
		} else if (mode == 0) {
			selector |= 0x1000000;
		} else {
			result = 3;
			goto out;
		}
	} else if (algorithm == 3) {
		selector = 0x30201;
		if (mode == 0) {
			selector |= 0x1000000;
		} else {
			result = 3;
			goto out;
		}
	} else {
		result = 3;
		goto out;
	}
	result = MarlinBufferDeobfuscateCopy(key->data, &rawKey);
	if (result == 0) {
		result = MarlinCipherCreate(&context, selector, input->data, 16,
					    counter, rawKey->data, 16);
		if (result == 0) {
			result = MarlinCipherProcess(context, data, size, buffer->data,
						    &buffer->size);
			if (result == 0) {
				*output = buffer;
				buffer = 0;
			}
		}
	}
out:
	MarlinBufferEraseAndDestroy(rawKey);
	MarlinBufferDestroy(0, buffer);
	MarlinCipherDestroy(context);
	return result;
}
