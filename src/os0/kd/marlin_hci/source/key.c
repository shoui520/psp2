#include "key.h"

int MarlinKeyDestroy(MarlinKey *key)
{
	if (key != 0) {
		if (key->data != 0) {
			MarlinBufferEraseAndDestroy(key->data);
			key->data = 0;
		}
		MarlinFreeMemory(0, key);
		key = 0;
	}
	return 0;
}

int MarlinKeyCreate(unsigned int kind, MarlinKey **output)
{
	int result = 0;
	MarlinKey *key = 0;

	key = MarlinAllocateMemory(0, sizeof(*key));
	if (key == 0) {
		result = 2;
	} else {
		MarlinFillMemory(key, 0, sizeof(*key));
		key->data = 0;
		key->format = 3;
		key->kind = kind;
		key->operationsEnabled = 0;
		*output = key;
		key = 0;
	}
	MarlinKeyDestroy(key);
	return result;
}
