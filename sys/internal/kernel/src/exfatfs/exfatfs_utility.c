/* Utility routines whose Sony source-file records are absent. */
#include <psp2kern/types.h>
#include <threadmgr/threadmgr.h>
#include <string.h>
#include "exfatfs_internal.h"

extern void *allocateHeapBuffer(SceExfatfsDrive *unusedDrive);
extern void *allocateCommonHeapMemory(int unused, SceSize size);
extern void freeHeapBuffer(int unused, void *buffer);

void *allocateFatBuffer(SceSize size)
{
	if (size > 0x800)
		return allocateCommonHeapMemory(0, size);
	return allocateHeapBuffer(NULL);
}

void freeFatBuffer(void *buffer, SceSize size)
{
	(void)size;
	freeHeapBuffer(0, buffer);
}

unsigned int checksumUtf16Name(const SceUInt16 *name, unsigned int length)
{
	unsigned int checksum;
	SceUInt16 index;
	unsigned int character;
	SceUInt16 partial;

	if (!length)
		return length;
	character = *name;
	if (!character)
		return character;
	checksum = 0;
	index = 0;
	do {
		index++;
		partial = (character & 0xff) +
			(SceUInt16)((checksum << 15) | (checksum >> 1));
		checksum = (SceUInt16)((character >> 8) +
			(SceUInt16)((partial << 15) | (partial >> 1)));
		if (index >= length)
			break;
		character = name[index];
	} while (character);
	return checksum;
}

extern int utf16Length(const SceUInt16 *string);
extern SceUInt16 *utf16CopyBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int count);
extern int utf16Compare(const SceUInt16 *left, const SceUInt16 *right);
extern int codepageToUtf16(SceUInt16 *destination, int capacity,
	const unsigned char *source);

int fatNameLength(const SceUInt16 *name)
{
	return utf16Length(name);
}

SceUInt16 *fatNameCopyBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int count)
{
	return utf16CopyBounded(destination, source, count);
}

int fatNameCompare(const SceUInt16 *left, const SceUInt16 *right)
{
	return utf16Compare(left, right);
}

SceUInt16 *fatNameAppendBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int count)
{
	SceUInt16 *end = destination;
	SceUInt16 *next;
	unsigned int copied;
	SceUInt16 character;

	for (;;) {
		copied = *end;
		next = end + 1;
		if (!copied)
			break;
		end = next;
	}
	if (count) {
		character = *source;
		if (character) {
			do {
				copied++;
				*end = character;
				end = next++;
				if (copied == count)
					break;
				character = *++source;
			} while (character);
		}
	}
	*end = 0;
	return destination;
}

int fatCodepageToUtf16(SceUInt16 *destination, int capacity,
	const unsigned char *source)
{
	return codepageToUtf16(destination, capacity, source);
}

int isShortNameCharacter(unsigned int character, int allowExtended)
{
	SceUInt16 allowed[] = {
		'$', '%', '\'', '-', '_', '@', '~', '`', '!', '(', ')', '{', '}', '^', '#', '&', 0,
	};
	SceUInt16 extended[] = { '+', ',', ';', '=', '[', ']', 0 };
	const SceUInt16 *entry;
	int result;

	if (character > '~')
		return 0;
	if (((character & ~0x20U) - 'A') <= 25 || character - '0' <= 9)
		return 1;
	entry = allowed;
	if (*entry == character)
		return 1;
	if (*entry) {
		do {
			entry++;
			if (*entry == character)
				return 1;
		} while (*entry);
	}
	if (allowExtended) {
		entry = extended;
		result = *entry;
		if (result == character)
			return 1;
		if (result) {
			do {
				entry++;
				result = *entry;
				if (result == character)
					return 1;
			} while (result);
		}
		return result;
	}
	return 0;
}

int shortNameTimeCharacter(void)
{
	unsigned char value = SceThreadmgrForDriver_47F6DE49() % 36;

	if (value > 9)
		return (unsigned char)(value + 'A' - 10);
	return (unsigned char)(value + '0');
}

extern unsigned int lookupUpcaseCharacter(unsigned int unicode,
	SceExfatfsUpcaseTable *table);

int UVFAT_ToUpperBuf(SceUInt16 *destination, const SceUInt16 *source,
	int count, SceExfatfsFileSystem *fileSystem)
{
	unsigned int character;
	int index;
	int destinationIndex;

	if (fileSystem->fileSystemType == 2) {
		if (!count)
			return 0;
		character = *source;
		destinationIndex = 0;
		if (character) {
			index = 0;
			do {
				index++;
				destination[destinationIndex] = lookupUpcaseCharacter(character,
					&fileSystem->upcase);
				destinationIndex = index;
				if (index == count)
					return 0;
				character = *++source;
			} while (character);
		}
	} else {
		if (!count)
			return 0;
		character = *source;
		destinationIndex = 0;
		if (character) {
			index = 0;
			do {
				index++;
				if (character - 'a' <= 25)
					character = (SceUInt16)(character - 'a' + 'A');
				destination[destinationIndex] = character;
				destinationIndex = index;
				if (index == count)
					return 0;
				character = *++source;
			} while (character);
		}
	}
	destination[destinationIndex] = 0;
	return 0;
}

extern int _sceFatfsWriteModeFromStat(SceExfatfsDrive *drive, int attribute);

