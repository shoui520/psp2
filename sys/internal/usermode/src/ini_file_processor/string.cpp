/*
 * SceIniFileProcessor: the string type of the processor (its bytes come from the allocator it was given)
 * and the two path helpers that split device names and file names.
 */

#include "ini_internal.h"

char g_iniEmptyString[1];

/* gives the string a copy of another one */
IniString *iniStringAssign(IniString *string, const IniString *source)
{
	if (source != string) {
		if (string->data != g_iniEmptyString)
			string->ops->free(string->data, string->context);
		if (source->length != 0) {
			char *copy = (char *)string->ops->alloc(source->length + 1, string->context);

			string->data = copy;
			string->length = source->length;
			sceClibMemcpy_safe(copy, source->data, source->length + 1);
		} else {
			string->data = g_iniEmptyString;
			string->length = 0;
		}
	}
	return string;
}

/* gives the string a copy of a text of the length (nothing when the text is empty) */
IniString *iniStringAssignChars(IniString *string, const char *text, SceSize length)
{
	if (text != string->data) {
		if (string->data != g_iniEmptyString)
			string->ops->free(string->data, string->context);
		if (text != NULL && *text != 0) {
			char *copy = (char *)string->ops->alloc(length + 1, string->context);

			string->data = copy;
			string->length = length;
			sceClibMemcpy_safe(copy, text, length);
			string->data[length] = 0;
		} else {
			string->data = g_iniEmptyString;
			string->length = 0;
		}
	}
	return string;
}

IniString *iniStringDestroy(IniString *string)
{
	if (string->data != g_iniEmptyString)
		string->ops->free(string->data, string->context);
	return string;
}

/* puts a text into the string at a position */
IniString *iniStringInsert(IniString *string, SceSize position, const void *text, SceSize length)
{
	char *copy = (char *)string->ops->alloc(string->length + length + 1, string->context);
	SceSize newLength = string->length + length;

	sceClibMemcpy_safe(copy, string->data, position);
	sceClibMemcpy_safe(copy + position, text, length);
	sceClibMemcpy_safe(copy + position + length, string->data + position, string->length - position);
	copy[string->length + length] = 0;
	if (string->data != g_iniEmptyString)
		string->ops->free(string->data, string->context);
	string->data = copy;
	string->length = newLength;
	return string;
}

/* puts a text at the end of the string (nothing changes when there is no memory) */
IniString *iniStringAppend(IniString *string, const void *text, SceSize length)
{
	char *copy = (char *)string->ops->alloc(string->length + length + 1, string->context);
	SceSize newLength = string->length + length;

	if (copy == NULL)
		return string;
	sceClibMemcpy_safe(copy, string->data, string->length);
	sceClibMemcpy_safe(copy + string->length, text, length);
	copy[string->length + length] = 0;
	if (string->data != g_iniEmptyString)
		string->ops->free(string->data, string->context);
	string->data = copy;
	string->length = newLength;
	return string;
}

/* the part of a string from a position, of the length (to the end for -1 or when it is too long) */
IniString *iniStringSubstr(IniString *result, const IniString *source, SceSize position, SceSize length)
{
	SceSize size = source->length;

	if (position < size) {
		SceSize n = length;

		if (length == (SceSize)-1)
			n = size - position;
		else if (position + length > size)
			n = size - position;
		result->data = g_iniEmptyString;
		result->ops = source->ops;
		result->length = 0;
		result->context = source->context;
		return iniStringAssignChars(result, source->data + position, n);
	}
	result->ops = iniDefaultAllocator();
	result->context = iniDefaultAllocator();
	result->data = g_iniEmptyString;
	result->length = 0;
	return NULL;
}

/* where the device name of a path ends (its first ':' when no '/' comes before it), 0 when it has none */
SceSize iniPathDeviceLength(const char *path)
{
	const char *colon;
	const char *slash;
	SceSize length = 0;

	if (path == NULL)
		return 0;
	colon = sceClibStrchr(path, ':');
	if (colon != NULL) {
		length = colon - path;
		slash = sceClibStrchr(path, '/');
		if (slash != NULL && length > (SceSize)(slash - path))
			return 0;
	}
	return length;
}

/* the file name of a path: what follows the last '/', or the last ':' of a path without one */
const char *iniPathFileName(const char *path)
{
	const char *name;
	const char *slash;
	const char *colon;

	if (path == NULL)
		return NULL;
	name = path;
	slash = sceClibStrrchr(path, '/');
	colon = sceClibStrrchr(name, ':');
	if (slash != NULL && slash[1] != 0) {
		name = slash + 1;
		if (colon != NULL && slash + 1 < colon)
			return NULL;
	} else if (colon != NULL && colon[1] != 0) {
		return colon + 1;
	}
	return name;
}
