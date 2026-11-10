/*
 * The AS runtime's utilities: the configuration strings every AS object is created from
 * ("numVoices=8 numReverbs=0"), the linear allocator that lays an object out in its
 * buffer, and the few C library routines the audio system needs, over SceLibKernel's sceClib.
 */

#include <stdarg.h>
#include <stddef.h>
#include <libkernel/libkernel.h>
#include <kernel/libkernel.h>

#include "as_ssp.h"
#include "as_util.h"

/* a configuration's separators: blanks, line feeds, commas, semicolons and carriage returns */
static inline int sceSasAsConfigIsSeparator(int c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == ',' || c == ';' || c == '\r';
}

static inline int sceSasAsIsDigit(int c)
{
	return (unsigned int)(c - '0') <= 9;
}

/* a token's last character: a separator, '=' or the end */
static inline int sceSasAsConfigIsDelimiter(int c)
{
	return c == '\0' || sceSasAsConfigIsSeparator(c) || c == '=';
}

/* appends c to the token while it has room; count counts every character */
static inline void sceSasAsConfigPutChar(char *token, SceSize size, SceSize *count, char c)
{
	(*count)++;
	if (token != NULL && size > *count)
		token[*count - 1] = c;
}

/* a token in quotes: what lies between them */
static inline const char *sceSasAsConfigReadQuoted(const char *string, char *token, SceSize size,
						   SceSize *count, char quote)
{
	char c;

	c = *++string;
	while (c != quote && c != '\0') {
		sceSasAsConfigPutChar(token, size, count, c);
		c = *++string;
	}
	if (c == quote)
		string++;
	return string;
}

/*
 * Reads the next token of a configuration: '=', a quoted string, a number or a word.
 * It is copied into token (size bytes) and its length, the terminator included, goes to
 * *length; the result points past it and the separators after it.
 */
static inline const char *sceSasAsConfigReadToken(const char *string, char *token, SceSize size,
						  SceSize *length)
{
	SceSize count;
	char c;

	while (*string != '\0' && sceSasAsConfigIsSeparator(*string))
		string++;
	count = 0;
	c = *string;
	switch (c) {
	case '=':
		sceSasAsConfigPutChar(token, size, &count, c);
		string++;
		break;
	case '\'':
		string = sceSasAsConfigReadQuoted(string, token, size, &count, '\'');
		break;
	case '"':
		string = sceSasAsConfigReadQuoted(string, token, size, &count, '"');
		break;
	default:
		if (sceSasAsIsDigit(c) || c == '+' || c == '-') {
			if (c == '+' || c == '-') {
				sceSasAsConfigPutChar(token, size, &count, c);
				string++;
			}
			while (sceSasAsIsDigit(c = *string)) {
				sceSasAsConfigPutChar(token, size, &count, c);
				string++;
			}
			if (c == '.') {
				sceSasAsConfigPutChar(token, size, &count, c);
				string++;
				while (sceSasAsIsDigit(c = *string)) {
					sceSasAsConfigPutChar(token, size, &count, c);
					string++;
				}
			}
		} else {
			while (!sceSasAsConfigIsDelimiter(c = *string)) {
				sceSasAsConfigPutChar(token, size, &count, c);
				string++;
			}
		}
		break;
	}
	while (*string != '\0' && sceSasAsConfigIsSeparator(*string))
		string++;
	if (length != NULL)
		*length = count + 1;
	if (token != NULL && size != 0)
		token[size - 1] = '\0';
	return string;
}

/* the length of the next token, its terminator included */
__attribute__((noinline)) const char *sceSasAsConfigGetTokenLength(const char *string, SceSize *length)
{
	return sceSasAsConfigReadToken(string, NULL, 0, length);
}

/* copies the next token into token (size bytes, from sceSasAsConfigGetTokenLength) */
__attribute__((noinline)) const char *sceSasAsConfigGetToken(const char *string, char *token, SceSize size)
{
	return sceSasAsConfigReadToken(string, token, size, NULL);
}

static inline int sceSasAsStrlenInline(const char *string)
{
	int length;

	for (length = 0; *string != '\0'; string++)
		length++;
	return length;
}

static inline void *sceSasAsAllocatorAllocInline(SceSasAsAllocator *allocator, SceSize size, int clear)
{
	char *memory;
	SceSize offset;

	if (allocator == NULL)
		return NULL;
	offset = (allocator->used + SCE_SAS_AS_ALLOCATOR_ALIGNMENT - 1) & -SCE_SAS_AS_ALLOCATOR_ALIGNMENT;
	memory = allocator->cursor;
	if (memory != NULL) {
		memory = (char *)(((SceUIntPtr)memory + SCE_SAS_AS_ALLOCATOR_ALIGNMENT - 1) &
				  -SCE_SAS_AS_ALLOCATOR_ALIGNMENT);
		allocator->cursor = memory + size;
		if (clear)
			sceClibMemset(memory, 0, size);
	}
	allocator->used = offset + size;
	if (allocator->alignment < SCE_SAS_AS_ALLOCATOR_ALIGNMENT)
		allocator->alignment = SCE_SAS_AS_ALLOCATOR_ALIGNMENT;
	return memory;
}

static inline char *sceSasAsAllocatorStrdupInline(SceSasAsAllocator *allocator, const char *string)
{
	char *copy;
	SceSize size;

	if (allocator == NULL)
		return NULL;
	size = sceSasAsStrlenInline(string) + 1;
	copy = allocator->cursor;
	if (copy != NULL)
		allocator->cursor = copy + size;
	allocator->used += size;
	if (allocator->alignment == 0)
		allocator->alignment = 1;
	if (copy != NULL)
		sceClibStrlcpy(copy, string, size);
	return copy;
}

/*
 * Parses "key=value ..." into a list of entries allocated from allocator (sizes only while
 * the allocator has no buffer); *list gets the list's head.
 */
int sceSasAsConfigParse(SceSasAsAllocator *allocator, const char *config, SceSasAsConfigEntry ***list)
{
	SceSasAsConfigEntry **head;
	SceSasAsConfigEntry *entry;
	const char *next;
	char *keyToken;
	char *valueToken;
	char *key;
	char *value;
	SceSize length;

	head = sceSasAsAllocatorAllocInline(allocator, sizeof(*head), 0);
	if (head != NULL)
		*head = NULL;
	if (config == NULL)
		config = "";
	while (*config != '\0' && sceSasAsConfigIsSeparator(*config))
		config++;
	while (*config != '\0') {
		sceSasAsConfigGetTokenLength(config, &length);
		keyToken = __builtin_alloca(length);
		next = sceSasAsConfigGetToken(config, keyToken, length);
		key = sceSasAsAllocatorStrdupInline(allocator, keyToken);
		if (*next++ != '=')
			return SCE_SAS_AS_ERROR_CONFIG;
		sceSasAsConfigGetTokenLength(next, &length);
		valueToken = __builtin_alloca(length);
		config = sceSasAsConfigGetToken(next, valueToken, length);
		if (allocator != NULL) {
			value = sceSasAsAllocatorStrdupInline(allocator, valueToken);
			entry = sceSasAsAllocatorAllocInline(allocator, sizeof(*entry), 0);
			if (head != NULL && entry != NULL) {
				entry->key = key;
				entry->value = value;
				entry->referenceCount = 0;
				entry->next = *head;
				*head = entry;
			}
		}
	}
	if (list != NULL)
		*list = head;
	return 0;
}

void *sceSasAsMemcpy(void *destination, const void *source, SceSize size)
{
	return sceClibMemcpy_safe(destination, source, size);
}

void *sceSasAsMemZero(void *buffer, SceSize size)
{
	return sceClibMemset(buffer, 0, size);
}

char *sceSasAsStrlcpy(char *destination, SceSize capacity, const char *source)
{
	sceClibStrlcpy(destination, source, capacity);
	return destination;
}

char *sceSasAsStrlcat(char *destination, SceSize capacity, const char *source)
{
	sceClibStrlcat(destination, source, capacity);
	return destination;
}

int sceSasAsStrcmp(const char *left, const char *right)
{
	return sceClibStrcmp(left, right);
}

int sceSasAsStrlen(const char *string)
{
	return sceSasAsStrlenInline(string);
}

static inline void sceSasAsLowerString(char *string, SceSize count)
{
	unsigned char c;

	while (*string != '\0' && count != 0) {
		c = *string;
		count--;
		if ((unsigned int)(c - 'A') <= 'Z' - 'A')
			*string = c + ('a' - 'A');
		string++;
	}
}

static inline int sceSasAsStrcasecmpInline(const char *left, const char *right)
{
	char lowerRight[SCE_SAS_AS_NAME_LENGTH];
	char lowerLeft[SCE_SAS_AS_NAME_LENGTH];

	sceClibStrlcpy(lowerLeft, left, sizeof(lowerLeft));
	sceClibStrlcpy(lowerRight, right, sizeof(lowerRight));
	sceSasAsLowerString(lowerLeft, sizeof(lowerLeft));
	sceSasAsLowerString(lowerRight, sizeof(lowerRight));
	return sceClibStrcmp(lowerLeft, lowerRight);
}

int sceSasAsStrcasecmp(const char *left, const char *right)
{
	return sceSasAsStrcasecmpInline(left, right);
}

char *sceSasAsStrchr(const char *string, int value)
{
	for (;;) {
		if (*string == '\0')
			return NULL;
		if (*string == value)
			return (char *)string;
		string++;
	}
}

char *sceSasAsStrlwr(char *string, SceSize count)
{
	sceSasAsLowerString(string, count);
	return string;
}

static inline int sceSasAsAtoiInline(const char *string)
{
	char c;
	int sign;
	int value;

	c = *string;
	while (c == ' ' || c == '\t')
		c = *++string;
	if (c == '-') {
		sign = -1;
		c = *++string;
	} else {
		sign = 1;
		if (c == '+')
			c = *++string;
	}
	if ((unsigned char)(c - '0') > 9)
		return 0;
	value = 0;
	do {
		value = value * 10 + (c - '0');
		c = *++string;
	} while ((unsigned char)(c - '0') <= 9);
	return value * sign;
}

int sceSasAsAtoi(const char *string)
{
	return sceSasAsAtoiInline(string);
}

static inline float sceSasAsAtofInline(const char *string)
{
	char c;
	float sign;
	float value;
	float scale;

	c = *string;
	while (c == ' ' || c == '\t')
		c = *++string;
	if (c == '-') {
		string++;
		sign = -1.0f;
	} else {
		sign = 1.0f;
		if (c == '+')
			string++;
	}
	value = 0.0f;
	c = *string;
	while ((unsigned char)(c - '0') <= 9) {
		value = value * 10.0f + (float)(c - '0');
		c = *++string;
	}
	if (c == '.') {
		scale = 0.1f;
		c = *++string;
		while ((unsigned char)(c - '0') <= 9) {
			value += (float)(c - '0') * scale;
			scale *= 0.1f;
			c = *++string;
		}
	}
	return sign * value;
}

float sceSasAsAtof(const char *string)
{
	return sceSasAsAtofInline(string);
}

static inline int sceSasAsIsCharIn(int value, const char *set)
{
	while (*set != '\0') {
		if (*set == value)
			return 1;
		set++;
	}
	return 0;
}

const char *sceSasAsGetToken(const char *string, char *token, SceSize size,
			     const char *delimiters, const char *spaces)
{
	char *end;

	if (delimiters == NULL)
		delimiters = ",";
	if (spaces == NULL)
		spaces = " \t";
	while (*string != '\0' && sceSasAsIsCharIn(*string, spaces))
		string++;
	end = token;
	if (token != NULL) {
		while (*string != '\0' && !sceSasAsIsCharIn(*string, delimiters) && size > 1) {
			*end++ = *string++;
			size--;
		}
	}
	while (*string != '\0' && !sceSasAsIsCharIn(*string, delimiters))
		string++;
	if (end != NULL) {
		while (end != token && sceSasAsIsCharIn(end[-1], spaces))
			end--;
		*end = '\0';
	}
	if (*string != '\0' && sceSasAsIsCharIn(*string, delimiters))
		string++;
	while (*string != '\0' && sceSasAsIsCharIn(*string, spaces))
		string++;
	return string;
}

int sceSasAsVsnprintf(char *buffer, SceSize size, const char *format, va_list arguments)
{
	int length;

	length = sceClibVsnprintf(buffer, size - 1, format, arguments);
	buffer[size - 1] = '\0';
	return length;
}

int sceSasAsSnprintf(char *buffer, SceSize size, const char *format, ...)
{
	va_list arguments;
	int length;

	va_start(arguments, format);
	length = sceClibVsnprintf(buffer, size - 1, format, arguments);
	buffer[size - 1] = '\0';
	va_end(arguments);
	return length;
}

void sceSasAsAllocatorInit(SceSasAsAllocator *allocator, void *buffer)
{
	if (allocator != NULL) {
		allocator->buffer = buffer;
		allocator->cursor = buffer;
		allocator->used = 0;
		allocator->alignment = 0;
	}
}

void *sceSasAsAllocatorAllocAligned(SceSasAsAllocator *allocator, SceSize size,
				    SceSize alignment, int clear)
{
	char *memory;
	SceSize offset;

	if (allocator == NULL)
		return NULL;
	if (alignment == 0)
		alignment = SCE_SAS_AS_ALLOCATOR_DEFAULT_ALIGNMENT;
	else if ((alignment & -alignment) != alignment)
		return NULL;
	offset = (allocator->used + alignment - 1) & -alignment;
	memory = allocator->cursor;
	if (memory != NULL) {
		memory = (char *)(((SceUIntPtr)memory + alignment - 1) & -alignment);
		allocator->cursor = memory + size;
		if (clear)
			sceClibMemset(memory, 0, size);
	}
	allocator->used = offset + size;
	if (alignment > allocator->alignment)
		allocator->alignment = alignment;
	return memory;
}

void *sceSasAsAllocatorAlloc(SceSasAsAllocator *allocator, SceSize size, int clear)
{
	return sceSasAsAllocatorAllocInline(allocator, size, clear);
}

char *sceSasAsAllocatorStrdup(SceSasAsAllocator *allocator, const char *string)
{
	return sceSasAsAllocatorStrdupInline(allocator, string);
}

void *sceSasAsAllocatorGetBuffer(const SceSasAsAllocator *allocator)
{
	if (allocator == NULL)
		return NULL;
	return allocator->buffer;
}

/* the offset from base of the allocator's cursor, aligned up */
SceSize sceSasAsAllocatorGetAlignedOffset(const SceSasAsAllocator *allocator, SceUIntPtr base,
					  SceSize alignment)
{
	if (allocator == NULL)
		return 0;
	if (allocator->cursor == NULL)
		return 0;
	return (((SceUIntPtr)allocator->cursor + alignment - 1) & -alignment) - base;
}

SceSize sceSasAsAllocatorGetOffset(const SceSasAsAllocator *allocator, SceUIntPtr base)
{
	if (allocator == NULL)
		return 0;
	if (allocator->cursor == NULL)
		return 0;
	return (((SceUIntPtr)allocator->cursor + SCE_SAS_AS_ALLOCATOR_ALIGNMENT - 1) &
		-SCE_SAS_AS_ALLOCATOR_ALIGNMENT) - base;
}

/* the buffer size everything allocated so far needs, with room to align its start */
SceSize sceSasAsAllocatorGetSize(const SceSasAsAllocator *allocator)
{
	if (allocator == NULL)
		return 0;
	return allocator->used + allocator->alignment;
}

int sceSasAsConfigGetSize(const char *config, SceSize *size)
{
	SceSasAsAllocator allocator;
	int ret;

	allocator.buffer = NULL;
	allocator.cursor = NULL;
	allocator.used = 0;
	allocator.alignment = 0;
	ret = sceSasAsConfigParse(&allocator, config, NULL);
	if (ret < 0)
		return ret;
	if (size != NULL)
		*size = allocator.used + allocator.alignment;
	return 0;
}

int sceSasAsConfigCreate(const char *config, void *buffer, SceSize size, SceSasAsConfigEntry ***list)
{
	SceSasAsAllocator allocator;
	int ret;

	allocator.buffer = NULL;
	allocator.cursor = NULL;
	allocator.used = 0;
	allocator.alignment = 0;
	ret = sceSasAsConfigParse(&allocator, config, NULL);
	if (ret < 0)
		return ret;
	if (size != allocator.used + allocator.alignment)
		return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	allocator.buffer = buffer;
	allocator.cursor = buffer;
	allocator.used = 0;
	allocator.alignment = 0;
	ret = sceSasAsConfigParse(&allocator, config, list);
	return ret < 0 ? ret : 0;
}

/* SCE_SAS_AS_ERROR_INVALID_ARGUMENT while an entry was never read: a key nobody knows */
int sceSasAsConfigCheckUnused(SceSasAsConfigEntry **list)
{
	SceSasAsConfigEntry *entry;

	if (list == NULL)
		return 0;
	for (entry = *list; entry != NULL; entry = entry->next) {
		if (entry->referenceCount <= 0)
			return SCE_SAS_AS_ERROR_INVALID_ARGUMENT;
	}
	return 0;
}

static inline SceSasAsConfigEntry *sceSasAsConfigFind(SceSasAsConfigEntry **list, const char *key)
{
	SceSasAsConfigEntry *entry;

	if (list == NULL)
		return NULL;
	for (entry = *list; entry != NULL; entry = entry->next) {
		if (sceClibStrcmp(entry->key, key) == 0)
			return entry;
	}
	return NULL;
}

/* a whole decimal integer, an optional sign first */
static inline int sceSasAsConfigIsInteger(const char *value)
{
	if (*value == '\0')
		return 0;
	if (*value == '-' || *value == '+') {
		value++;
		if (*value == '\0')
			return 1;
	}
	if (!sceSasAsIsDigit((unsigned char)*value))
		return 0;
	while (*++value != '\0') {
		if (!sceSasAsIsDigit((unsigned char)*value))
			return 0;
	}
	return 1;
}

int sceSasAsConfigGetUInt(SceSasAsConfigEntry **list, const char *key, SceUInt32 defaultValue,
			  SceUInt32 *value)
{
	SceSasAsConfigEntry *entry;

	entry = sceSasAsConfigFind(list, key);
	if (entry == NULL) {
		if (value != NULL)
			*value = defaultValue;
		return 0;
	}
	entry->referenceCount++;
	if (!sceSasAsConfigIsInteger(entry->value))
		return SCE_SAS_AS_ERROR_CONFIG;
	if (value != NULL)
		*value = sceSasAsAtoiInline(entry->value);
	return 0;
}

int sceSasAsConfigGetInt(SceSasAsConfigEntry **list, const char *key, SceInt32 defaultValue,
			 SceInt32 *value)
{
	SceSasAsConfigEntry *entry;

	entry = sceSasAsConfigFind(list, key);
	if (entry == NULL) {
		if (value != NULL)
			*value = defaultValue;
		return 0;
	}
	entry->referenceCount++;
	if (!sceSasAsConfigIsInteger(entry->value))
		return SCE_SAS_AS_ERROR_CONFIG;
	if (value != NULL)
		*value = sceSasAsAtoiInline(entry->value);
	return 0;
}

/* a decimal number: an optional sign, digits, and a fraction after a point */
static inline int sceSasAsConfigIsNumber(const char *value)
{
	if (*value == '\0')
		return 0;
	if (*value == '-' || *value == '+') {
		value++;
		if (*value == '\0')
			return 1;
	}
	while (sceSasAsIsDigit((unsigned char)*value)) {
		value++;
		if (*value == '\0')
			return 1;
	}
	if (*value == '.')
		value++;
	while (*value != '\0') {
		if (!sceSasAsIsDigit((unsigned char)*value))
			return 0;
		value++;
	}
	return 1;
}

int sceSasAsConfigGetFloat(SceSasAsConfigEntry **list, const char *key, float defaultValue,
			   float *value)
{
	SceSasAsConfigEntry *entry;

	entry = sceSasAsConfigFind(list, key);
	if (entry == NULL) {
		if (value != NULL)
			*value = defaultValue;
		return 0;
	}
	entry->referenceCount++;
	if (!sceSasAsConfigIsNumber(entry->value))
		return SCE_SAS_AS_ERROR_CONFIG;
	if (value != NULL)
		*value = sceSasAsAtofInline(entry->value);
	return 0;
}

/* "true", "false" (any case) or an integer, nonzero for true */
int sceSasAsConfigGetBool(SceSasAsConfigEntry **list, const char *key, SceInt8 defaultValue,
			  SceInt8 *value)
{
	SceSasAsConfigEntry *entry;

	entry = sceSasAsConfigFind(list, key);
	if (entry == NULL) {
		if (value != NULL)
			*value = defaultValue;
		return 0;
	}
	entry->referenceCount++;
	if (sceSasAsStrcasecmpInline(entry->value, "true") == 0) {
		if (value != NULL)
			*value = 1;
		return 0;
	}
	if (sceSasAsStrcasecmpInline(entry->value, "false") == 0) {
		if (value != NULL)
			*value = 0;
		return 0;
	}
	if (!sceSasAsConfigIsInteger(entry->value))
		return SCE_SAS_AS_ERROR_CONFIG;
	if (value != NULL)
		*value = sceSasAsAtoiInline(entry->value) != 0;
	return 0;
}

int sceSasAsConfigGetString(SceSasAsConfigEntry **list, const char *key, const char *defaultValue,
			    char *value, SceSize size)
{
	SceSasAsConfigEntry *entry;

	entry = sceSasAsConfigFind(list, key);
	if (entry == NULL) {
		sceClibStrlcpy(value, defaultValue, size);
		return 0;
	}
	entry->referenceCount++;
	sceClibStrlcpy(value, entry->value, size);
	return 0;
}
