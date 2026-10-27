/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"
#include <sysmem/utils.h>

int MarlinSha1Create(SceSha1Context **context)
{
	int result = 0;
	SceSha1Context *memory = 0;

	*context = 0;
	memory = MarlinAllocateMemory(0, sizeof(*memory));
	if (memory == 0) {
		result = 2;
		goto out;
	}
	sceSha1BlockInit(memory);
	*context = memory;
out:
	if (*context == 0)
		MarlinFreeMemory(0, memory);
	return result;
}

int MarlinSha1Finish(SceSha1Context *context, void *digest)
{
	int result = 0;
	SceSha1Context *memory = context;

	sceSha1BlockResult(memory, digest);
	MarlinFreeMemory(0, memory);
	return result;
}

int MarlinSha1Update(SceSha1Context *context, const void *data,
		     unsigned int size)
{
	int result = 0;
	SceSha1Context *sha = context;

	sceSha1BlockUpdate(sha, data, size);
	return result;
}

int MarlinSha1Digest(const void *data, unsigned int size, void *digest)
{
	int result = 0;

	sceSha1Digest(data, size, digest);
	return result;
}

int MarlinSha256Create(SceSha256Context **context)
{
	int result = 0;
	SceSha256Context *memory = 0;

	*context = 0;
	memory = MarlinAllocateMemory(0, sizeof(*memory));
	if (memory == 0) {
		result = 2;
		goto out;
	}
	sceSha256BlockInit(memory);
	*context = memory;
out:
	if (*context == 0)
		MarlinFreeMemory(0, memory);
	return result;
}

int MarlinSha256Finish(SceSha256Context *context, void *digest)
{
	int result = 0;
	SceSha256Context *memory = context;

	sceSha256BlockResult(memory, digest);
	MarlinFreeMemory(0, memory);
	return result;
}

int MarlinSha256Update(SceSha256Context *context, const void *data,
		       unsigned int size)
{
	int result = 0;
	SceSha256Context *sha = context;

	sceSha256BlockUpdate(sha, data, size);
	return result;
}

int MarlinSha256Digest(const void *data, unsigned int size, void *digest)
{
	int result = 0;

	sceSha256Digest(data, size, digest);
	return result;
}
