/* Descriptive source name: the original library has no debug records. */
#include "asn1.h"
#include "key.h"

MarlinBuffer *MarlinBufferInitializeExternal(void *allocator,
		MarlinBuffer *buffer, unsigned int size, void *data);
MarlinBuffer *MarlinBufferCopyInto(void *allocator, MarlinBuffer *buffer,
		unsigned int size, const void *source);

void MarlinBufferReleaseData(void *allocator, MarlinBuffer *buffer);

int MarlinAsn1GetChildBuffer(MarlinAsn1Node *parent, unsigned int index,
			   int copy, MarlinBuffer *output)
{
	int result = 0;
	MarlinBuffer *buffer;
	MarlinAsn1Node *child;
	MarlinBuffer *destination;

	child = parent != 0 && parent->constructed != 0 &&
		parent->value.sequence.count > index ?
		parent->value.sequence.children[index] : 0;
	destination = output;
	if (child == 0 || destination == 0) {
		result = 1;
		goto out;
	}
	if (child->constructed != 0) {
		if (copy == 0)
			buffer = MarlinBufferInitializeExternal(0, destination,
				child->encodedSize, (void *)child->encodedData);
		else
			buffer = MarlinBufferCopyInto(0, destination,
				child->encodedSize, child->encodedData);
	} else {
		if (copy == 0)
			buffer = MarlinBufferInitializeExternal(0, destination,
				child->value.primitive.size,
				child->value.primitive.data);
		else
			buffer = MarlinBufferCopyInto(0, destination,
				child->value.primitive.size,
				child->value.primitive.data);
	}
	if (buffer == 0)
		result = 2;
out:
	return result;
}

int MarlinRsaParseKeyContents(const MarlinBuffer *input, int copy,
				     MarlinRsaKeyComponents **output)
{
	int result = 0;
	MarlinRsaKeyComponents *key = 0;
	MarlinAsn1Node *outer = 0;
	MarlinAsn1Node *inner = 0;
	MarlinAsn1Node *payload;

	if (input == 0 || output == 0) {
		result = 3;
		goto out;
	}
	result = MarlinAsn1Parse(0, input->data, input->size, 0, &outer);
	if (result != 0) {
		result = ((unsigned int)result >> 16) << 16 ? 0x22090001 : result;
		goto out;
	}
	payload = outer != 0 && outer->constructed != 0 &&
		outer->value.sequence.count > 2 ?
		outer->value.sequence.children[2] : 0;
	if (payload == 0 || payload->constructed != 0) {
		result = 0x22090001;
		goto out;
	}
	result = MarlinAsn1Parse(0, payload->value.primitive.data,
			       payload->value.primitive.size, 0, &inner);
	if (result != 0) {
		result = ((unsigned int)result >> 16) << 16 ? 0x22090001 : result;
		goto out;
	}
	key = MarlinAllocateMemory(0, sizeof(*key));
	if (key == 0) {
		result = 2;
		goto out;
	}
	MarlinFillMemory(key, 0, sizeof(*key));
	key->ownsData = copy;
	result = MarlinAsn1GetChildBuffer(inner, 1, copy, &key->modulus);
	if (result == 0) {
		result = MarlinAsn1GetChildBuffer(inner, 2, copy, &key->publicExponent);
		if (result == 0) {
			result = MarlinAsn1GetChildBuffer(inner, 3, copy, &key->privateExponent);
			if (result == 0) {
				result = MarlinAsn1GetChildBuffer(inner, 4, copy, &key->primeP);
				if (result == 0) {
					result = MarlinAsn1GetChildBuffer(inner, 5, copy, &key->primeQ);
					if (result == 0) {
						result = MarlinAsn1GetChildBuffer(inner, 6, copy, &key->exponentP);
						if (result == 0) {
							result = MarlinAsn1GetChildBuffer(inner, 7, copy, &key->exponentQ);
							if (result == 0) {
								result = MarlinAsn1GetChildBuffer(inner, 8, copy, &key->coefficient);
								if (result == 0) {
									*output = key;
								}
							}
						}
					}
				}
			}
		}
	}
out:
	if (result != 0)
		MarlinRsaFreeKey(key);
	MarlinAsn1Destroy(0, outer);
	MarlinAsn1Destroy(0, inner);
	return result;
}

int MarlinRsaParseKey(const MarlinBuffer *data, MarlinRsaKeyComponents **key)
{
	return MarlinRsaParseKeyContents(data, 0, key);
}

int MarlinRsaFreeKey(MarlinRsaKeyComponents *key)
{
	if (key != 0 && key->ownsData != 0) {
		MarlinBufferReleaseData(0, &key->modulus);
		MarlinBufferReleaseData(0, &key->publicExponent);
		MarlinBufferReleaseData(0, &key->privateExponent);
		MarlinBufferReleaseData(0, &key->primeP);
		MarlinBufferReleaseData(0, &key->primeQ);
		MarlinBufferReleaseData(0, &key->exponentP);
		MarlinBufferReleaseData(0, &key->exponentQ);
		MarlinBufferReleaseData(0, &key->coefficient);
	}
	MarlinFreeMemory(0, key);
	return 0;
}
