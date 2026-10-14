/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"
#include "handles.h"

MarlinHandleEntry marlinHandles[32];

void MarlinHandlesInitialize(void)
{
	int status = 0;

	MarlinFillMemory(marlinHandles, 0, sizeof(marlinHandles));
}

int MarlinHandleCreate(unsigned int type, unsigned int size, void *object,
		       unsigned int *handle)
{
	int result = 0;
	unsigned int index = 0;

	if (handle == 0) {
		result = 3;
		goto out;
	}
	for (index = 0; index < 32; index++) {
		if (marlinHandles[index].type == 0 &&
		    marlinHandles[index].id == 0 &&
		    marlinHandles[index].size == 0 &&
		    marlinHandles[index].object == 0)
			break;
	}
	if (index == 32) {
		result = 3;
		goto out;
	}
	marlinHandles[index].type = type;
	marlinHandles[index].size = size;
	marlinHandles[index].object = object;
	marlinHandles[index].id = index | 0x1000;
	if (handle != 0)
		*handle = marlinHandles[index].id;
out:
	return result;
}

int MarlinHandleGet(unsigned int handle, unsigned int *type, unsigned int *size,
		    void **object)
{
	int result = 0;
	unsigned int index = 0;

	if (type == 0 || size == 0) {
		result = 3;
		goto out;
	}
	for (index = 0; index < 32; index++) {
		if (marlinHandles[index].id == handle)
			break;
	}
	if (index == 32) {
		result = 3;
		goto out;
	}
	*type = marlinHandles[index].type;
	*size = marlinHandles[index].size;
	if (object != 0)
		*object = marlinHandles[index].object;
out:
	return result;
}

int MarlinHandleRelease(unsigned int handle)
{
	int result = 0;
	unsigned int index;
	unsigned int type = 0;
	unsigned int size = 0;
	void *object = 0;

	index = 0;
	for (index = 0; index < 32; index++) {
		if (marlinHandles[index].id == handle)
			break;
	}
	if (index == 32) {
		result = 3;
		goto out;
	}
	marlinHandles[index].type = 0;
	marlinHandles[index].size = 0;
	marlinHandles[index].object = 0;
	marlinHandles[index].id = 0;
out:
	return result;
}

void MarlinHandlesTerminate(void)
{
	unsigned int index;
	int status = 0;

	index = 0;
	for (index = 0; index < 32; index++) {
		if (marlinHandles[index].id != 0)
			MarlinHandleRelease(marlinHandles[index].id);
		marlinHandles[index].type = 0;
		marlinHandles[index].size = 0;
		marlinHandles[index].object = 0;
		marlinHandles[index].id = 0;
	}
}
