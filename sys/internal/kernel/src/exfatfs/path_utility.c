/* Sony: path_utility.c */
#include <psp2kern/types.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/debug.h>
#include "exfatfs_internal.h"

extern void *allocateHeapBuffer(SceExfatfsDrive *unusedDrive);
extern void freeHeapBuffer(int unused, void *buffer);
extern int utf16Length(const SceUInt16 *string);
extern SceUInt16 *utf16CopyBounded(SceUInt16 *destination,
		const SceUInt16 *source, unsigned int count);
extern SceUInt16 *utf16Copy(SceUInt16 *destination, const SceUInt16 *source);
extern SceUInt16 *utf16FindChar(SceUInt16 *string, int character);
extern int utf16ToCodepage(unsigned char *destination, int capacity,
		const SceUInt16 *source);
extern int isUnicodeDoubleByte(unsigned int unicode);
extern int isCodepageDoubleByte(const unsigned char *source);

int utf8ToUtf16(void *destination, unsigned int capacity,
		const unsigned char *source)
{
	SceUInt16 units[2];
	SceUInt16 *end;
	unsigned char *output = destination;
	unsigned int remaining;
	unsigned int unicode;
	unsigned int marker;
	unsigned int size;
	SceUInt16 firstUnit;
	int count = 0;

	if (capacity <= 1)
		return 0;
	remaining = capacity - 2;
	for (;;) {
		unicode = *source++;
		if (unicode & 128) {
			if ((unsigned char)(unicode + 62) > 50)
				break;
			unicode &= 127;
			marker = 64;
			do {
				unsigned int continuation = *source++;

				unicode &= marker - 1;
				marker <<= 5;
				unicode = (continuation & 63) | (unicode << 6);
			} while (marker <= unicode);
		}
		if (!unicode)
			break;
		end = units + 1;
		if (unicode > 0x10ffff || unicode - 0xd800 < 0x800) {
			firstUnit = 0;
		} else if (unicode < 0x10000) {
			firstUnit = unicode;
		} else {
			unicode -= 0x10000;
			*end++ = (unicode & 1023) - 0x2400;
			firstUnit = (unicode >> 10) - 0x2800;
		}
		units[0] = firstUnit;
		size = (end - units) * 2;
		if (remaining < size)
			break;
		remaining -= size;
		if (end != units) {
			void *copyDestination = output;

			output += size;
			memcpy(copyDestination, units, size);
		}
		++count;
	}
	*(SceUInt16 *)output = 0;
	return count;
}

int utf16ToUtf8(unsigned char *destination, unsigned int capacity,
		const SceUInt16 *source)
{
	unsigned char bytes[4];
	unsigned char *end;
	unsigned char *cursor;
	unsigned int unicode;
	unsigned int limit;
	unsigned int size;
	unsigned int remaining = capacity - 1;
	int continuationCount;
	unsigned int mask;
	unsigned char prefix;
	int count = 0;

	if (!capacity)
		return 0;
	for (;;) {
		unicode = *source;
		if ((SceUInt16)(unicode + 0x2800) < 0x400) {
			SceUInt16 low = source[1];

			if ((SceUInt16)(low + 0x2400) >= 0x400)
				break;
			source += 2;
			unicode = ((low & 1023) | ((unicode & 1023) << 10))
				+ 0x10000;
		} else {
			if ((SceUInt16)(unicode + 0x2400) < 0x400 || !unicode)
				break;
			++source;
		}
		if (unicode - 0xd800 < 0x800)
			unicode = 0;
		if (unicode <= 127) {
			bytes[0] = unicode;
			end = bytes + 1;
		} else {
			if (unicode < 0x800) {
				mask = 31;
				prefix = 0xc0;
				continuationCount = 0;
				end = bytes + 2;
			} else {
				limit = 0x800;
				continuationCount = 1;
				for (;;) {
					limit <<= 5;
					if (unicode < limit)
						break;
					++continuationCount;
				}
				end = bytes + continuationCount + 2;
				mask = (unsigned char)((1 << (6 - continuationCount)) - 1);
				prefix = ~mask;
				mask >>= 1;
			}
			cursor = end;
			do {
				unsigned char continuation = unicode & 63;

				unicode >>= 6;
				*--cursor = continuation | 128;
			} while (cursor != end - continuationCount - 1);
			*(end - continuationCount - 2) = prefix | (mask & unicode);
		}
		size = end - bytes;
		if (remaining < size)
			break;
		remaining -= size;
		if (size) {
			unsigned char *output = destination;

			destination += size;
			memcpy(output, bytes, size);
		}
		++count;
	}
	*destination = 0;
	return count;
}

void freePathBuffer(int unused, void *buffer)
{
	freeHeapBuffer(unused, buffer);
}

int sceFatfsParsePath(int unused, SceFatfsNameset *nameset, SceUInt16 *path)
{
	SceUInt16 *name;
	unsigned char *encoded;
	const SceUInt16 *input;
	unsigned int nameLength;
	unsigned int baseWidth;
	unsigned int extensionWidth;
	unsigned int unicode;
	unsigned int index;
	int state;
	int requiresLongName;
	int specialCount;
	int consumed;
	int containsSpace;
	int depth;
	int length;
	int result;

	name = allocateHeapBuffer((SceExfatfsDrive *)unused);
	if (!name)
		return 0x8001000c;
	encoded = allocateHeapBuffer((SceExfatfsDrive *)unused);
	if (!encoded) {
		result = 0x8001000c;
		goto freeName;
	}
	length = utf16Length(path);
	if (!length)
		goto invalidPath;
	if (length > 0 && path[length - 1] == '.') {
		SceUInt16 *tail = path + length - 1;

		do {
			*tail = 0;
			if (tail == path)
				break;
			--tail;
		} while (*tail == '.');
	}
	memset(name, 0, 0x200);
	memset(nameset, 0, sizeof(*nameset));
	nameset->shortName[0] = 0;
	nameset->requiresLongName = 1;
	nameset->isRoot = 1;
	input = path;
	state = 0;
	nameLength = 0;
	requiresLongName = 0;
	consumed = 0;
	specialCount = 0;
	baseWidth = 0;
	extensionWidth = 0;
	containsSpace = 0;
	depth = 0;
	do {
		unicode = *input++;
		if (unicode == '.' || unicode == ' ') {
			if (unicode == ' ')
				containsSpace = 1;
			if (!baseWidth || nameLength == specialCount) {
				name[nameLength++] = unicode;
				++baseWidth;
				if (specialCount == 2) {
					requiresLongName = 1;
					nameset->isRoot = 0;
				}
			} else {
				if (!state || baseWidth > 510)
					goto invalidPath;
				name[nameLength++] = unicode;
				if (state == 2)
					requiresLongName = 1;
				else
					state = 2;
			}
			++specialCount;
		} else if (unicode == '/' || unicode == '\\') {
			if (nameLength) {
				if (specialCount == nameLength) {
					if (specialCount != 1) {
						if (specialCount != 2)
							goto invalidPath;
						if (depth && !--depth)
							nameset->isRoot = 1;
					}
				} else {
					++depth;
				}
			}
			if ((!baseWidth && state == 1) ||
			    (!extensionWidth && state == 2))
				goto invalidPath;
			requiresLongName = 0;
			state = 1;
			extensionWidth = 0;
			containsSpace = 0;
			baseWidth = 0;
			specialCount = 0;
			nameLength = 0;
		} else if (!unicode) {
			if (specialCount == nameLength && nameLength)
				goto invalidPath;
			if (((!baseWidth && state == 1) ||
			     (!extensionWidth && state == 2)) &&
			    (!nameset->isRoot || baseWidth))
				goto invalidPath;
			if (nameLength > 255)
				goto invalidPath;
			name[nameLength] = 0;
			goto classifyName;
		} else {
			const char *invalid = "\\\"./:<>|*?";
			int character;

			if ((unicode <= 31 && unicode != 5) || unicode == '\\')
				goto invalidPath;
			while ((character = *++invalid)) {
				if ((SceUInt16)character == unicode)
					goto invalidPath;
			}
			nameset->isRoot = 0;
			if (specialCount && specialCount == nameLength)
				requiresLongName = 1;
			if (state == 1) {
				name[nameLength++] = unicode;
				if (isUnicodeDoubleByte(unicode))
					++baseWidth;
				++baseWidth;
			} else if (state == 2) {
				unicode = input[-1];
				name[nameLength++] = unicode;
				if (isUnicodeDoubleByte(unicode))
					++extensionWidth;
				++extensionWidth;
			}
		}
		++consumed;
	} while (consumed != 261);
	if (nameLength > 255) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 15, 439, "file name is too long");
#endif
		goto invalidPath;
	}

classifyName:
	memset(encoded, 0, 0x100);
	length = consumed - nameLength;
	if (nameset->isRoot || baseWidth > 8 || extensionWidth > 3 ||
	    requiresLongName || nameLength - 1 > baseWidth + extensionWidth)
		goto copyPath;
	input = name;
	while ((unicode = *input++)) {
		if (unicode > 127 || unicode - 'a' <= 'z' - 'a' ||
		    unicode == '[' || unicode == '=' || unicode == ']' ||
		    (unicode & 0xffef) == '+' || unicode == ',')
			goto copyPath;
	}
	if (containsSpace)
		goto copyPath;
	result = utf16ToCodepage(encoded, 256, name);
	if (result < 0)
		goto freeEncoded;
	if (baseWidth)
		memcpy(nameset->shortName, encoded, baseWidth);
	nameset->shortName[baseWidth] = '.';
	if (extensionWidth)
		memcpy(nameset->shortName + baseWidth + 1,
			encoded + baseWidth + 1, extensionWidth);
	nameset->shortName[baseWidth + extensionWidth + 2] = 0;
	nameset->requiresLongName = 0;
	index = 0;
	do {
		if (isCodepageDoubleByte(nameset->shortName + index)) {
			++index;
		} else {
			unsigned int character = nameset->shortName[index];

			if (character == '[' || character == '=' || character == ';' ||
			    character == ']' || character - '+' <= 1)
				nameset->shortName[index] = '_';
		}
		++index;
	} while (index <= 11);
copyPath:
	result = 0;
	utf16CopyBounded(nameset->filePath, path, 515);
	nameset->fileName = nameset->filePath + length;
	goto freeEncoded;
invalidPath:
	result = 0x80010016;
freeEncoded:
	freeHeapBuffer(unused, encoded);
freeName:
	freeHeapBuffer(unused, name);
	return result;
}

int sceFatfsPrepareNameset(int unused, const unsigned char *path,
		SceFatfsNameset **nameset)
{
	SceUInt16 *decoded;
	SceUInt16 *expanded;
	SceUInt16 *cursor;
	SceUInt16 *output;
	SceUInt16 *previous;
	SceUInt16 *replacement;
	unsigned int character;
	int length;
	int dotCount;
	int componentStart;
	int removed;
	int result;
	int atEnd;
	int atSeparator;

	if (!path)
		return 0x80010016;
	decoded = allocateHeapBuffer((SceExfatfsDrive *)unused);
	if (!decoded)
		return 0x8001000c;
	utf8ToUtf16(decoded, 0x800, path);
	for (length = 0; length < 259; ++length) {
		character = decoded[length];
		if (!character)
			break;
		if (character == '\\')
			decoded[length] = '/';
	}
	if (length > 1 && decoded[length - 1] == '/')
		decoded[length - 1] = 0;
	expanded = allocateHeapBuffer((SceExfatfsDrive *)unused);
	if (!expanded) {
		result = 0x8001000c;
		goto freeDecoded;
	}
	output = expanded;
	if (*decoded != '/')
		*output++ = '/';
	utf16Copy(output, decoded);
	if (utf16FindChar(expanded, '.')) {
		cursor = expanded;
		dotCount = 0;
		componentStart = 1;
		for (;;) {
			character = *cursor;
			if (character == '.') {
				++dotCount;
				if (!componentStart)
					dotCount = 0;
				++cursor;
				continue;
			}
			atEnd = character == 0;
			atSeparator = atEnd;
			if (character == '/')
				atSeparator |= 1;
			if (!atSeparator) {
				componentStart = atSeparator;
				dotCount = atSeparator;
				++cursor;
				continue;
			}
			if (atEnd) {
				if (!componentStart) {
					while (cursor[-1] == '.')
						*--cursor = 0;
					break;
				}
				if (cursor[-1] == '.') {
					character = '/';
					*cursor = '/';
					cursor[1] = 0;
				}
			}
			if (dotCount) {
				replacement = cursor;
				removed = 0;
				for (;;) {
					previous = replacement - 1;
					while (replacement[-1] != '/' && expanded < previous) {
						replacement = previous;
						--previous;
					}
					++removed;
					if (expanded > previous) {
						result = 0x80010002;
						goto freeExpanded;
					}
					if (dotCount <= removed) {
						utf16Copy(replacement, cursor + 1);
						character = replacement[-1];
						cursor = previous;
						break;
					}
					replacement = previous;
				}
			}
			if (!character)
				break;
			componentStart = 1;
			dotCount = 0;
			++cursor;
		}
	}
	if (utf16Length(expanded) >= 260) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 701, "_expandPath", "path is too long");
#endif
		result = 0x80010016;
		goto freeExpanded;
	}
	utf16Copy(decoded, expanded);
	freeHeapBuffer(unused, expanded);
	if ((unsigned int)utf16Length(decoded) >= 260) {
#if FW_VERSION >= 0x04000000
		SCE_DBG_PRINTF_LEVEL(2, 15, 112, "path is too long\n");
#endif
		result = 0x80010016;
		goto freeDecoded;
	}
	*nameset = allocateHeapBuffer((SceExfatfsDrive *)unused);
	if (!*nameset) {
		result = 0x8001000c;
		goto freeDecoded;
	}
	result = sceFatfsParsePath(unused, *nameset, decoded);
	if (result >= 0) {
		freeHeapBuffer(unused, decoded);
		return 0;
	}
	freeHeapBuffer(unused, *nameset);
	goto freeDecoded;
freeExpanded:
	freeHeapBuffer(unused, expanded);
freeDecoded:
	freeHeapBuffer(unused, decoded);
	*nameset = NULL;
	return result;
}

int validateDateTime(const SceExfatfsDateTime *dateTime)
{
	if ((unsigned int)dateTime->year - 1980 > 127 ||
	    (unsigned int)dateTime->month - 1 > 11 ||
	    (unsigned int)dateTime->day - 1 > 30 ||
	    dateTime->hour > 23 || dateTime->minute > 59)
		return 0x80010016;
	if (dateTime->second <= 59)
		return 0;
	return 0x80010016;
}

