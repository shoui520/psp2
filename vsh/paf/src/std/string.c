/* String storage is reallocated for erase and append; there is no capacity field. */
#include "../compiler.h"
#include "string.h"
#include "ctype.h"
#include "stdlib.h"
#include <libkernel/libkernel.h>
#include "new.h"

char *sce_paf_strchr(const char *str, int ch)
{
	const signed char *cursor = (const signed char *)str;

	for (;;) {
		if (*cursor == ch)
			return (char *)cursor;
		if (!*cursor++)
			return NULL;
	}
}

char *sce_paf_strrchr(const char *str, int ch)
{
	const char *cursor;

	if (!str)
		return NULL;
	cursor = str;
	while (*(const signed char *)cursor)
		cursor++;
	for (;;) {
		if (*(const signed char *)cursor == (signed char)ch)
			return (char *)cursor;
		if (cursor == str)
			return NULL;
		cursor--;
	}
}

uint16_t *sce_paf_wcschr(const uint16_t *str, int ch)
{
	const uint16_t *cursor = (const uint16_t *)str;

	if (!str)
		return NULL;
	for (;;) {
		if (*cursor == (uint16_t)ch)
			return (uint16_t *)cursor;
		if (!*cursor++)
			return NULL;
	}
}

uint16_t *sce_paf_wcsrchr(const uint16_t *str, int ch)
{
	const uint16_t *cursor;
	const uint16_t *start = (const uint16_t *)str;

	if (!str)
		return NULL;
	cursor = start;
	while (*cursor)
		cursor++;
	for (;;) {
		if (*cursor == (uint16_t)ch)
			return (uint16_t *)cursor;
		if (cursor == start)
			return NULL;
		cursor--;
	}
}

int sce_paf_wcscmp(const uint16_t *left, const uint16_t *right)
{
	const uint16_t *leftByte;
	const uint16_t *rightByte;
	int leftCh;
	int rightCh;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	leftByte = (const uint16_t *)left;
	rightByte = (const uint16_t *)right;
	rightCh = *rightByte++;
	leftCh = *leftByte;
	for (;;) {
		if (leftCh != rightCh)
			return leftCh - rightByte[-1];
		/* Sony rereads the current left halfword before advancing. */
		if (!*leftByte++)
			return 0;
		rightCh = *rightByte++;
		leftCh = *leftByte;
	}
}

uint16_t *sce_paf_wcscpy(uint16_t *destination, const uint16_t *source)
{
	uint16_t *dest = (uint16_t *)destination;
	const uint16_t *src = (const uint16_t *)source;

	if (!destination || !source)
		return NULL;
	for (;;) {
		uint16_t ch = *src++;

		*dest++ = ch;
		if (!ch)
			return destination;
	}
}

uint16_t *sce_paf_wcscat(uint16_t *destination, const uint16_t *source)
{
	const uint16_t *src = (const uint16_t *)source;
	const uint16_t *sourceEnd;
	const uint16_t *destinationEnd;
	uint16_t *dest;

	if (!destination || !source)
		return NULL;
	destinationEnd = (const uint16_t *)destination;
	while (*destinationEnd)
		destinationEnd++;
	sourceEnd = src;
	while (*sourceEnd)
		sourceEnd++;
	/* The original rejects an append when both terminator addresses coincide. */
	if (destinationEnd == sourceEnd)
		return NULL;
	/* Sony scans the destination a second time before writing anything. */
	dest = (uint16_t *)destination;
	while (*dest)
		dest++;
	for (;;) {
		uint16_t ch = *src++;

		*dest++ = ch;
		if (!ch)
			return destination;
	}
}

uint16_t *sce_paf_wcsncpy(uint16_t *destination, const uint16_t *source,
			  size_t num)
{
	uint16_t *dest = (uint16_t *)destination;
	const uint16_t *src = (const uint16_t *)source;
	int32_t remaining = (int32_t)num;
	uint16_t ch;

	if (!destination || !source)
		return NULL;
	while (remaining > 0) {
		ch = *src++;
		*dest++ = ch;
		remaining--;
		if (!ch) {
			while (remaining > 0) {
				*dest++ = 0;
				remaining--;
			}
			break;
		}
	}
	return destination;
}

uint16_t *sce_paf_wcsncat(uint16_t *destination, const uint16_t *source,
			  size_t num)
{
	uint16_t *dest = (uint16_t *)destination;
	const uint16_t *src = (const uint16_t *)source;
	uint32_t remaining = num;
	uint16_t first;
	uint16_t ch;

	if (!destination || !source)
		return NULL;
	/* The original reads source[0] before searching for the terminator. */
	first = *src++;
	while (*dest)
		dest++;
	ch = first;
	*dest++ = ch;
	if (!ch)
		return destination;
	for (;;) {
		remaining--;
		if (remaining & 0x80000000U) {
			/* Sony first writes, then replaces the final halfword with NUL. */
			dest[-1] = 0;
			return destination;
		}
		ch = *src++;
		*dest++ = ch;
		if (!ch)
			return destination;
	}
}

int sce_paf_wcsncmp(const uint16_t *left, const uint16_t *right, size_t num)
{
	const uint16_t *leftByte = (const uint16_t *)left;
	const uint16_t *rightByte = (const uint16_t *)right;
	uint32_t remaining = num;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	for (;;) {
		int leftCh, rightCh;

		remaining--;
		if (remaining & 0x80000000U)
			return 0;
		rightCh = *rightByte++;
		leftCh = *leftByte;
		if (leftCh != rightCh)
			return *leftByte - rightByte[-1];
		if (!*leftByte++)
			return 0;
	}
}

int sce_paf_wcscasecmp(const uint16_t *left, const uint16_t *right)
{
	const uint16_t *leftByte = (const uint16_t *)left;
	const uint16_t *rightByte = (const uint16_t *)right;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	for (;;) {
		int leftCh = *leftByte;
		int rightCh;

		if (leftCh < 0x80)
			leftCh = (uint16_t)sce_paf_tolower(leftCh);
		rightCh = *rightByte;
		if (rightCh < 0x80)
			rightCh = (uint16_t)sce_paf_tolower(rightCh);
		if (leftCh != rightCh) {
			leftCh = *leftByte;
			if (leftCh < 0x80)
				leftCh = (uint16_t)sce_paf_tolower(leftCh);
			rightCh = *rightByte;
			if (rightCh < 0x80)
				rightCh = (uint16_t)sce_paf_tolower(rightCh);
			return leftCh - rightCh;
		}
		if (!*leftByte++)
			return 0;
		rightByte++;
	}
}

int sce_paf_wcsncasecmp(const uint16_t *left, const uint16_t *right,
			 size_t num)
{
	const uint16_t *leftByte = (const uint16_t *)left;
	const uint16_t *rightByte = (const uint16_t *)right;
	uint32_t remaining = num;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	for (;;) {
		int leftCh, rightCh;

		remaining--;
		if (remaining & 0x80000000U)
			return 0;
		leftCh = *leftByte;
		if (leftCh < 0x80)
			leftCh = (uint16_t)sce_paf_tolower(leftCh);
		rightCh = *rightByte;
		if (rightCh < 0x80)
			rightCh = (uint16_t)sce_paf_tolower(rightCh);
		if (leftCh != rightCh) {
			leftCh = *leftByte;
			if (leftCh < 0x80)
				leftCh = (uint16_t)sce_paf_tolower(leftCh);
			rightCh = *rightByte;
			if (rightCh < 0x80)
				rightCh = (uint16_t)sce_paf_tolower(rightCh);
			return leftCh - rightCh;
		}
		if (!*leftByte++)
			return 0;
		rightByte++;
	}
}

size_t sce_paf_wcsspn(const uint16_t *str, const uint16_t *accept)
{
	const uint16_t *cursor = (const uint16_t *)str;
	const uint16_t *start = cursor;
	const uint16_t *member;
	uint16_t ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return cursor - start;
		member = (const uint16_t *)accept;
		for (;;) {
			uint16_t candidate = *member;

			if (!candidate)
				return cursor - start;
			if (candidate == ch)
				break;
			member++;
		}
		cursor++;
	}
}

size_t sce_paf_wcscspn(const uint16_t *str, const uint16_t *reject)
{
	const uint16_t *cursor = (const uint16_t *)str;
	const uint16_t *start = cursor;
	const uint16_t *member;
	uint16_t ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return cursor - start;
		member = (const uint16_t *)reject;
		for (;;) {
			uint16_t candidate = *member;

			if (!candidate)
				break;
			if (candidate == ch)
				return cursor - start;
			member++;
		}
		cursor++;
	}
}

size_t sce_paf_wcsnlen(const uint16_t *str, size_t limit)
{
	const uint16_t *cursor = (const uint16_t *)str;
	uint32_t length = 0;

	if (!str)
		return 0;
	/* Sony reads the initial halfword even when limit is zero. */
	if (!*cursor++)
		return 0;
	for (;;) {
		length++;
		if (length >= limit)
			return length;
		if (!*cursor++)
			return length;
	}
}

uint16_t *sce_paf_wcspbrk(const uint16_t *str, const uint16_t *accept)
{
	const uint16_t *cursor = (const uint16_t *)str;
	const uint16_t *member;
	uint16_t ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return NULL;
		member = (const uint16_t *)accept;
		for (;;) {
			uint16_t candidate = *member;

			if (!candidate)
				break;
			if (candidate == ch)
				return (uint16_t *)cursor;
			member++;
		}
		cursor++;
	}
}

/*
 * Sony's private_strtok export is a substring search, not the stateful
 * delimiter tokenizer suggested by its name.
 */
char *sce_paf_private_strtok(const char *haystack, const char *needle)
{
	const signed char *pattern =
		(const signed char *)needle;
	const signed char *candidate =
		(const signed char *)haystack;

	if (!*pattern)
		return (char *)candidate;
	if (!*candidate)
		return NULL;
	for (;;) {
		const signed char *part = pattern;
		const signed char *cursor = candidate;
		int ch = *part;

		for (;;) {
			int current = *cursor;

			if (!current)
				return NULL;
			if (current != ch)
				break;
			ch = *++part;
			cursor++;
			if (!ch)
				return (char *)candidate;
		}
		candidate++;
		if (!*candidate)
			return NULL;
	}
}

/* The anonymous wide export implements the same substring search on 16-bit units. */
uint16_t *sce_paf_wcsstr(const uint16_t *haystack,
			      const uint16_t *needle)
{
	const uint16_t *pattern = (const uint16_t *)needle;
	const uint16_t *candidate = (const uint16_t *)haystack;

	if (!*pattern)
		return (uint16_t *)candidate;
	if (!*candidate)
		return NULL;
	for (;;) {
		const uint16_t *part = pattern;
		const uint16_t *cursor = candidate;
		uint16_t ch = *part;

		for (;;) {
			uint16_t current = *cursor;

			if (!current)
				return NULL;
			if (current != ch)
				break;
			ch = *++part;
			cursor++;
			if (!ch)
				return (uint16_t *)candidate;
		}
		candidate++;
		if (!*candidate)
			return NULL;
	}
}

/* Sony's reentrant tokenizer for 16-bit code units. */
uint16_t *sce_paf_wcstok(uint16_t *str, const uint16_t *delimiters,
			       uint16_t **save)
{
	uint16_t *cursor = (uint16_t *)str;
	const uint16_t *separator =
		(const uint16_t *)delimiters;
	uint16_t *token;
	uint16_t ch;

	if (!cursor) {
		cursor = (uint16_t *)*save;
		if (!cursor)
			return NULL;
	}
	for (;;) {
		const uint16_t *member;
		uint16_t sep;

		ch = *cursor++;
		if (!ch)
			break;
		member = separator;
		for (;;) {
			sep = *member++;
			if (!sep || sep == ch)
				break;
		}
		if (!sep)
			break;
	}
	if (!ch) {
		*save = NULL;
		return NULL;
	}
	token = (uint16_t *)(cursor - 1);
	for (;;) {
		const uint16_t *member;
		uint16_t sep;

		ch = *cursor++;
		if (!ch) {
			*save = NULL;
			return token;
		}
		member = separator;
		for (;;) {
			sep = *member++;
			if (!sep || sep == ch)
				break;
		}
		if (sep) {
			/* Unlike the byte tokenizer, Sony stores save first. */
			*(uint16_t **)save = (uint16_t *)cursor;
			cursor[-1] = 0;
			return token;
		}
	}
}

/* The classic tokenizer wrapper calls this export out of line. */
__attribute__((noinline))
char *sce_paf_strtok_r(char *str, const char *delimiters, char **save)
{
	signed char *cursor = (signed char *)str;
	const signed char *separator =
		(const signed char *)delimiters;
	char *token;
	int ch;

	if (!cursor) {
		cursor = (signed char *)*save;
		if (!cursor)
			return NULL;
	}

	/* Skip leading separators. Sony reads the input byte before the set. */
	for (;;) {
		const signed char *member;
		int sep;

		ch = *cursor++;
		if (!ch)
			break;
		member = separator;
		for (;;) {
			sep = *member++;
			if (!sep || sep == ch)
				break;
		}
		if (!sep)
			break;
	}
	if (!ch) {
		*save = NULL;
		return NULL;
	}

	token = (char *)(cursor - 1);
	/* The first token character was consumed by the separator skip loop. */
	for (;;) {
		const signed char *member;
		int sep;

		ch = *cursor++;
		if (!ch) {
			*save = NULL;
			return token;
		}
		member = separator;
		for (;;) {
			sep = *member++;
			if (!sep || sep == ch)
				break;
		}
		if (sep) {
			cursor[-1] = 0;
			*save = (char *)cursor;
			return token;
		}
	}
}

static char *paf_private_strtok_save;

char *sce_paf_strtok(char *str, const char *delimiters)
{
	return sce_paf_strtok_r(str, delimiters, &paf_private_strtok_save);
}

int sce_paf_strcmp(const char *left, const char *right)
{
	const signed char *leftByte;
	const signed char *rightByte;
	int leftCh;
	int rightCh;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	leftByte = (const signed char *)left;
	rightByte = (const signed char *)right;
	rightCh = *rightByte++;
	leftCh = *leftByte;
	for (;;) {
		if (leftCh != rightCh)
			return (unsigned char)leftCh -
				(unsigned char)rightByte[-1];
		/* Sony rereads the matched left byte before advancing. */
		if (!*leftByte++)
			return 0;
		rightCh = *rightByte++;
		leftCh = *leftByte;
	}
}

/* PafStringRfind calls this export out of line in the original. */
__attribute__((noinline))
int sce_paf_strncmp(const char *left, const char *right, size_t num)
{
	const signed char *leftByte = (const signed char *)left;
	const signed char *rightByte = (const signed char *)right;
	uint32_t remaining = num;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	for (;;) {
		int leftCh, rightCh;

		/* The native count is decremented and tested as signed. */
		remaining--;
		if (remaining & 0x80000000U)
			return 0;
		rightCh = *rightByte++;
		leftCh = *leftByte;
		if (leftCh != rightCh) {
			int leftAgain = *leftByte;
			int rightAgain = rightByte[-1];

			return (unsigned char)leftAgain - (unsigned char)rightAgain;
		}
		if (!*leftByte++)
			return 0;
	}
}

int sce_paf_strcasecmp(const char *left, const char *right)
{
	const signed char *leftByte = (const signed char *)left;
	const signed char *rightByte = (const signed char *)right;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	for (;;) {
		int leftFolded = sce_paf_tolower(*leftByte);
		int rightFolded = sce_paf_tolower(*rightByte);

		if (leftFolded != rightFolded) {
			int leftAgain = sce_paf_tolower(*leftByte);
			int rightAgain = sce_paf_tolower(*rightByte);

			return (unsigned char)leftAgain - (unsigned char)rightAgain;
		}
		if (!*leftByte++)
			return 0;
		rightByte++;
	}
}

int sce_paf_strncasecmp(const char *left, const char *right, size_t num)
{
	const signed char *leftByte = (const signed char *)left;
	const signed char *rightByte = (const signed char *)right;
	uint32_t remaining = num;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	for (;;) {
		int leftFolded, rightFolded;

		remaining--;
		if (remaining & 0x80000000U)
			return 0;
		leftFolded = sce_paf_tolower(*leftByte);
		rightFolded = sce_paf_tolower(*rightByte);
		if (leftFolded != rightFolded) {
			int leftAgain = sce_paf_tolower(*leftByte);
			int rightAgain = sce_paf_tolower(*rightByte);

			return (unsigned char)leftAgain - (unsigned char)rightAgain;
		}
		if (!*leftByte++)
			return 0;
		rightByte++;
	}
}

char *sce_paf_strcpy(char *destination, const char *source)
{
	char *result = destination;

	if (!destination || !source)
		return NULL;
	while ((*destination++ = *source++) != '\0')
		;
	return result;
}

char *sce_paf_strcat(char *destination, const char *source)
{
	char *cursor = destination;
	const signed char *src = (const signed char *)source;

	while (*(const signed char *)cursor)
		cursor++;
	for (;;) {
		signed char ch = *src++;

		*(char *)cursor = ch;
		cursor++;
		if (!ch)
			return destination;
	}
}

char *sce_paf_strncat(char *destination, const char *source, size_t num)
{
	char *cursor = destination;
	const signed char *src = (const signed char *)source;
	uint32_t remaining = num;
	signed char first;
	signed char ch;

	if (!destination || !source)
		return NULL;
	/* The first source read precedes the destination scan in Sony's code. */
	first = *src++;
	while (*(const signed char *)cursor)
		cursor++;
	ch = first;
	*(char *)cursor = ch;
	if (!ch)
		return destination;
	cursor++;
	for (;;) {
		/* Sony tests the count only after writing the preceding byte. */
		remaining--;
		if (remaining & 0x80000000U) {
			*(char *)(cursor - 1) = '\0';
			return destination;
		}
		ch = *src++;
		*(char *)cursor = ch;
		cursor++;
		if (!ch)
			return destination;
	}
}

size_t sce_paf_strspn(const char *str, const char *accept)
{
	const signed char *cursor = (const signed char *)str;
	const signed char *start = cursor;
	const signed char *member;
	int ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return cursor - start;
		member = (const signed char *)accept;
		for (;;) {
			int candidate = *member;

			if (!candidate)
				return cursor - start;
			if (candidate == ch)
				break;
			member++;
		}
		cursor++;
	}
}

size_t sce_paf_strcspn(const char *str, const char *reject)
{
	const signed char *cursor = (const signed char *)str;
	const signed char *start = cursor;
	const signed char *member;
	int ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return cursor - start;
		member = (const signed char *)reject;
		for (;;) {
			int candidate = *member;

			if (!candidate)
				break;
			if (candidate == ch)
				return cursor - start;
			member++;
		}
		cursor++;
	}
}

char *sce_paf_strpbrk(const char *str, const char *accept)
{
	const signed char *cursor = (const signed char *)str;
	const signed char *member;
	int ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return NULL;
		member = (const signed char *)accept;
		for (;;) {
			int candidate = *member;

			if (!candidate)
				break;
			if (candidate == ch)
				return (char *)cursor;
			member++;
		}
		cursor++;
	}
}

size_t sce_paf_strlcpy(char *destination, const char *source, size_t size)
{
	const signed char *cursor = (const signed char *)source;
	const signed char *begin = cursor;
	char *dest = destination;

	if (!destination || !source)
		return 0;
	while (size > 1) {
		const signed char *current = cursor;
		int ch = *cursor++;

		size--;
		*(char *)dest++ = ch;
		if (!ch) {
			/* Sony rereads the just-copied terminating byte. */
			cursor = current;
			goto measure;
		}
	}
	if (size == 1)
		*(char *)dest = 0;
measure:
	while (*cursor++)
		;
	return (size_t)(cursor - begin - 1);
}

size_t sce_paf_strlcat(char *destination, const char *source, size_t size)
{
	char *dest = destination;
	const signed char *cursor = (const signed char *)source;
	const signed char *begin = cursor;
	size_t prefix;

	if (!destination || !source)
		return 0;
	while (size) {
		if (!*(const signed char *)dest)
			break;
		size--;
		dest++;
	}
	prefix = (size_t)(dest - destination);
	while (size > 1) {
		const signed char *current = cursor;
		int ch = *cursor++;

		size--;
		*(char *)dest++ = ch;
		if (!ch) {
			cursor = current;
			goto measure;
		}
	}
	if (size == 1)
		*(char *)dest = 0;
measure:
	while (*cursor++)
		;
	return prefix + (size_t)(cursor - begin - 1);
}

char *sce_paf_strncpy(char *destination, const char *source, size_t num)
{
	char *result = destination;
	int32_t count = num;
	int32_t i = 0;

	if (!destination || !source)
		return NULL;
	while (i < count) {
		char value = *source++;

		*destination++ = value;
		if (!value) {
			while (++i < count)
				*destination++ = '\0';
			break;
		}
		i++;
	}
	return result;
}

void *memcpy(void *dst, const void *src, size_t count)
{
	return sce_paf_memcpy(dst, src, count);
}

void *memset(void *dst, int ch, size_t count)
{
	return sce_paf_memset(dst, ch, count);
}

void *sce_paf_memchr(const void *ptr, int ch, size_t count)
{
	const uint8_t *cursor = ptr;
	uint8_t value = ch;
	int32_t remaining = count;

	while (remaining > 0) {
		if (*cursor == value)
			return (void *)cursor;
		remaining--;
		cursor++;
	}
	return NULL;
}

uint16_t *sce_paf_wmemchr(const uint16_t *ptr, uint16_t ch, size_t count)
{
	int32_t remaining = count;

	while (remaining > 0) {
		if (*ptr == ch)
			return (uint16_t *)ptr;
		remaining--;
		ptr++;
	}
	return NULL;
}

size_t sce_paf_wcslen(const uint16_t *str)
{
	size_t length = 0;

	if (str) {
		while (*str++)
			length++;
	}
	return length;
}

size_t sce_paf_strlen(const char *str)
{
	if (!str)
		sce_paf_printf_dummy();
	return sceClibStrnlen(str, (size_t)-1);
}

extern int ScePafStdc_D608246A(void);

char PafStringEmpty;
uint16_t PafWStringEmpty;

