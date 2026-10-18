/* Standalone UTF16 utilities; Sony's source filename is not in debug records. */
#include <stddef.h>
#include <scetypes.h>
#include <psp2kern/kernel/sysclib.h>

int utf16Length(const SceUInt16 *string)
{
	const SceUInt16 *cursor;
	int length;

	if (!*string)
		return *string;
	cursor = string;
	length = 0;
	do {
		++cursor;
		++length;
	} while (*cursor);
	return length;
}

SceUInt16 *utf16Copy(SceUInt16 *destination, const SceUInt16 *source)
{
	SceUInt16 *cursor = destination;
	SceUInt16 character = *source;

	if (character) {
		do {
			*cursor++ = character;
			character = *++source;
		} while (character);
	}
	*cursor = 0;
	return cursor;
}

SceUInt16 *utf16FindChar(SceUInt16 *string, int character)
{
	while (*string != character && *string)
		++string;
	if (!*string)
		return NULL;
	return string;
}

int utf16Compare(const SceUInt16 *left, const SceUInt16 *right)
{
	while (*left && *right && *left == *right) {
		++left;
		++right;
	}
	return *left - *right;
}

SceUInt16 *utf16CopyBounded(SceUInt16 *destination, const SceUInt16 *source,
		unsigned int count)
{
	SceUInt16 *start = destination;
	int character;
	int copied;
	unsigned int size;

	if (!count)
		return start;
	character = *source;
	if (character) {
		copied = 0;
		for (;;) {
			++copied;
			*destination++ = character;
			if (copied == count)
				return start;
			character = *++source;
			if (!character)
				break;
		}
	} else {
		copied = character;
	}
	size = count - copied;
	++copied;
	size *= 2;
	if (count < copied)
		size = 2;
	memset(destination, 0, size);
	return start;
}
