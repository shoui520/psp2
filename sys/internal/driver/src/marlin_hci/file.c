/* Descriptive source name: the original library has no debug records. */
#include "buffer.h"
#include "file.h"
#include <stdint.h>
#include <iofilemgr/iofilemgr.h>
#include <kernel/error.h>
#include "search.h"

int MarlinFileRemove(void *allocator, const char *path)
{
	int result;
	int index;
	int length;
	int status;
	char devicePath[1024];

	length = MarlinStringLength(path);
	if (length > 1018) {
		result = 0x11060001;
		goto out;
	}
	for (index = 0; index < length; index++) {
		if (((const signed char *)path)[index] == ':')
			break;
	}
	if (index == length) {
		MarlinCopyMemory(devicePath, "host0:", 6);
		MarlinCopyMemory(devicePath + 6, path, length + 1);
	} else {
		MarlinCopyMemory(devicePath, path, length + 1);
	}
	status = sceIoRemove(devicePath);
	if (status != 0)
		result = status == SCE_ERROR_ERRNO_ENOENT ?
			0x11060002 : 0x11060001;
	else
		result = 0;
out:
	return result;
}

int MarlinFileOpen(void *allocator, const char *path, int mode,
		   MarlinFileContext **file)
{
	int result;
	MarlinFileContext *memory = 0;
	int descriptor;
	int index;
	int length;
	char devicePath[1024];

	if (file == 0) {
		result = 3;
		goto out;
	}
	memory = MarlinAllocateMemory(allocator, sizeof(*memory));
	if (memory == 0) {
		result = 2;
		goto out;
	}
	length = MarlinStringLength(path);
	if (length > 1018) {
		result = 0x11060001;
		goto out;
	}
	for (index = 0; index < length; index++) {
		if (((const signed char *)path)[index] == ':')
			break;
	}
	if (index == length) {
		MarlinCopyMemory(devicePath, "host0:", 6);
		MarlinCopyMemory(devicePath + 6, path, length + 1);
	} else {
		MarlinCopyMemory(devicePath, path, length + 1);
	}
	switch (mode) {
	case 1:
	case 9:
		descriptor = sceIoOpen(devicePath, SCE_O_RDONLY, 0666);
		break;
	case 2:
	case 10:
		descriptor = sceIoOpen(devicePath,
				       SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC,
				       0777);
		break;
	case 3:
	case 11:
		descriptor = sceIoOpen(devicePath, SCE_O_RDWR, 0777);
		break;
	case 4:
	case 12:
		descriptor = sceIoOpen(devicePath,
				       SCE_O_WRONLY | SCE_O_CREAT | SCE_O_APPEND,
				       0777);
		break;
	default:
		descriptor = -1;
		result = 0x11060008;
		goto out;
	}
	if (descriptor < 0) {
		if (descriptor == SCE_ERROR_ERRNO_ENOENT)
			result = 0x11060002;
		else
			result = 0x11060001;
		goto out;
	}
	memory->buffer = (unsigned char *)
		(((uintptr_t)&memory->storage[63] >> 6) << 6);
	memory->position = (mode & 1) != 0 ? 0 : -1;
	memory->cachedBlock = -1;
	memory->cachedSize = 0;
	memory->mode = mode;
	memory->descriptor = descriptor;
	*file = memory;
	memory = 0;
	result = 0;
out:
	if (memory != 0)
		MarlinFreeMemory(allocator, memory);
	return result;
}

int MarlinFileClose(void *allocator, MarlinFileContext *file)
{
	int result;
	MarlinFileContext *context = file;

	if (context == 0) {
		result = 4;
		goto out;
	}
	sceIoClose(context->descriptor);
	MarlinFreeMemory(allocator, file);
	result = 0;
out:
	return result;
}

int MarlinFileRead(void *allocator, MarlinFileContext *file,
		   unsigned int size, unsigned int *readSize, void *data)
{
	int result;
	unsigned int total;
	int count;
	MarlinFileContext *context = file;
	int descriptor;
	int block;
	int offset;

	if (readSize == 0 || data == 0 || size == 0) {
		result = 3;
		goto out;
	}
	if (context == 0 || (descriptor = context->descriptor) == 0 ||
	    (context->mode & 1) == 0) {
		result = 4;
		goto out;
	}
	block = context->position / 8192;
	if (context->cachedBlock != block) {
		sceIoLseek(descriptor, (long long)(block * 8192), SCE_SEEK_SET);
		count = sceIoRead(descriptor, context->buffer, 8192);
		if (count < 0) {
			result = 0x11060003;
			goto out;
		}
		context->cachedBlock = block;
		context->cachedSize = count;
	}
	offset = context->position % 8192;
	count = context->cachedSize - offset;
	if (count < 0) {
		result = 0x11060003;
		goto out;
	}
	if (count >= size) {
		MarlinCopyMemory(data, context->buffer + offset, size);
		total = size;
		size = 0;
		context->position += total;
	} else {
		MarlinCopyMemory(data, context->buffer + offset, count);
		total = count;
		size -= count;
		context->position += count;
		if (sceIoLseek(descriptor, 0, SCE_SEEK_END) > context->position) {
			sceIoLseek(descriptor, context->position, SCE_SEEK_SET);
			count = 1;
			while (size > 8192) {
				count = sceIoRead(descriptor,
					(unsigned char *)data + total, 8192);
				if (count < 0) {
					result = 0x11060003;
					goto out;
				}
				if (count == 0)
					break;
				total += count;
				size -= count;
				context->position += count;
			}
			if (count != 0 && size != 0) {
				count = sceIoRead(descriptor, context->buffer, 8192);
				if (count < 0) {
					result = 0x11060003;
					goto out;
				}
				context->cachedBlock = context->position / 8192;
				context->cachedSize = count;
				if (count >= size)
					count = size;
				MarlinCopyMemory((unsigned char *)data + total,
						 context->buffer, count);
				context->position += count;
				total += count;
			}
		}
	}
	*readSize = total;
	result = 0;
out:
	return result;
}

int MarlinFileWrite(void *allocator, MarlinFileContext *file,
		    unsigned int size, const void *data)
{
	int result;
	MarlinFileContext *context = file;
	int descriptor;
	int written;

	if (data == 0 || size == 0) {
		result = 3;
		goto out;
	}
	if (context == 0 || (descriptor = context->descriptor) == 0) {
		result = 4;
		goto out;
	}
	if ((context->mode & 6) == 0) {
		result = 4;
		goto out;
	}
	context->cachedBlock = -1;
	context->cachedSize = 0;
	written = sceIoWrite(descriptor, data, size);
	if (written < size)
		result = 0x11060004;
	else
		result = 0;
out:
	return result;
}

int MarlinFileGetSize(void *allocator, MarlinFileContext *file,
		      unsigned int *size)
{
	int result;
	MarlinFileContext *context = file;
	int descriptor;
	int position;
	int length;

	if (size == 0) {
		result = 3;
		goto out;
	}
	if (context == 0 || (descriptor = context->descriptor) == 0) {
		result = 4;
		goto out;
	}
	position = sceIoLseek(descriptor, 0, SCE_SEEK_CUR);
	if (position < 0) {
		result = 0x11060006;
		goto out;
	}
	length = sceIoLseek(descriptor, 0, SCE_SEEK_END);
	if (length < 0) {
		result = 0x11060005;
		goto out;
	}
	if (sceIoLseek(descriptor, position, SCE_SEEK_SET) < 0) {
		result = 0x11060005;
		goto out;
	}
	*size = length;
	result = 0;
out:
	return result;
}
