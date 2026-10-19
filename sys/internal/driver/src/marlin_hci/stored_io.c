/* Descriptive source name: the original library has no debug records. */
#include "storage.h"
#include "file.h"
#include "path.h"
#include "cipher.h"

int MarlinGetStoredPath(int selector, int createDirectories,
			unsigned int capacity, char *path)
{
	int result = 0;
	const char *filename = 0;
	unsigned int size = capacity;

	result = MarlinGetStorageRoot(0, &size, path);
	if (result == 0) {
		size = capacity;
		result = MarlinAppendPath(&size, path, "sdata/P0000000");
		if (result == 0) {
			if (createDirectories != 0)
				MarlinCreateDirectories(0, path);
			switch (selector) {
			case 1:
				filename = "P0000004.DAT";
				break;
			case 0:
				filename = "P0000001.DAT";
				break;
			default:
				result = 5;
				goto out;
			}
			size = capacity;
			result = MarlinAppendPath(&size, path, filename);
		}
	}
out:
	return result;
}

int MarlinWriteStoredData(int selector, const MarlinBuffer *data)
{
	int result;
	MarlinBuffer *buffer;
	int cleanupStatus;
	unsigned int size;
	MarlinFileContext *file;
	char path[64];
	MarlinBuffer hashInput;

	result = 0;
	cleanupStatus = 0;
	file = 0;
	buffer = 0;
	hashInput.size = 0;
	hashInput.data = 0;
	size = 0;
	MarlinGetStoredPath(selector, 1, 64, path);
	if (data != 0) {
		size += ((data->size + 16) / 16) * 16;
		size += 16;
		size += 20;
		buffer = MarlinBufferCreate(0, size, 0);
		if (buffer == 0) {
			result = 2;
			goto out;
		}
		result = MarlinEncryptRecord(data, buffer->data + 20);
		if (result == 0) {
			hashInput.data = buffer->data + 20;
			hashInput.size = buffer->size - 20;
			result = MarlinSha1Digest(hashInput.data, hashInput.size,
						 buffer->data);
			if (result == 0) {
				result = MarlinFileOpen(0, path, 2, &file);
				if (result != 0) {
					cleanupStatus = MarlinFileRemove(0, path);
					goto out;
				}
				result = MarlinFileWrite(0, file, buffer->size,
							 buffer->data);
				if (result != 0) {
					MarlinFileClose(0, file);
					file = 0;
					cleanupStatus = MarlinFileRemove(0, path);
				}
			}
		}
	} else {
		result = MarlinFileRemove(0, path);
		if (result == 0x11060002)
			result = 0;
	}
out:
	if (buffer != 0)
		MarlinBufferDestroy(0, buffer);
	if (file != 0)
		MarlinFileClose(0, file);
	return result;
}

unsigned int MarlinLoadStoredData(int kind, MarlinBuffer **data)
{
	int result;
	MarlinBuffer *buffer;
	unsigned char *cursor;
	MarlinFileContext *file;
	unsigned int size;
	char path[64];
	MarlinBuffer record;
	unsigned char digest[20];
	MarlinBuffer *output;

	result = 0;
	file = 0;
	buffer = 0;
	cursor = 0;
	record.size = 0;
	record.data = 0;
	output = 0;
	MarlinGetStoredPath(kind, 0, 64, path);
	if (data != 0) {
		result = MarlinFileOpen(0, path, 1, &file);
		if (result == 0) {
			result = MarlinFileGetSize(0, file, &size);
			if (result == 0) {
				buffer = MarlinBufferCreate(0, size, 0);
				if (buffer == 0) {
					result = 2;
					goto out;
				}
				if (size != 0) {
					cursor = buffer->data;
					result = MarlinFileRead(0, file, size,
							&size, cursor);
					if (result != 0)
						goto out;
					cursor += size;
				}
				record.data = buffer->data + 20;
				record.size = buffer->size - 20;
				result = MarlinSha1Digest(record.data, record.size,
							 digest);
				if (result == 0) {
					if (MarlinCompareMemory(digest, buffer->data,
								20) != 0) {
						result = 6;
						goto out;
					}
					result = MarlinDecryptRecord(&record, &output);
					if (result == 0) {
						*data = output;
						output = 0;
					}
				}
			}
		}
	}
out:
	if (file != 0)
		MarlinFileClose(0, file);
	MarlinBufferDestroy(0, buffer);
	MarlinBufferDestroy(0, output);
	return result;
}
