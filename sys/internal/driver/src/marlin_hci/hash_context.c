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
	SceKernelUtilsForDriver_E4390FFA(memory);
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

	SceKernelUtilsForDriver_48F24106(memory, digest);
	MarlinFreeMemory(0, memory);
	return result;
}

int MarlinSha1Update(SceSha1Context *context, const void *data,
		     unsigned int size)
{
	int result = 0;
	SceSha1Context *sha = context;

	SceKernelUtilsForDriver_478A6F3C(sha, data, size);
	return result;
}

int MarlinSha1Digest(const void *data, unsigned int size, void *digest)
{
	int result = 0;

	SceKernelUtilsForDriver_87DC7F2F(data, size, digest);
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
	SceKernelUtilsForDriver_D909FA2C(memory);
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

	SceKernelUtilsForDriver_4899CD4B(memory, digest);
	MarlinFreeMemory(0, memory);
	return result;
}

int MarlinSha256Update(SceSha256Context *context, const void *data,
		       unsigned int size)
{
	int result = 0;
	SceSha256Context *sha = context;

	SceKernelUtilsForDriver_236A9097(sha, data, size);
	return result;
}

int MarlinSha256Digest(const void *data, unsigned int size, void *digest)
{
	int result = 0;

	SceKernelUtilsForDriver_A773A6A8(data, size, digest);
	return result;
}
