/* String storage is reallocated for erase and append; there is no capacity field. */
#include "../compiler.h"
#include "string.h"
#include "ctype.h"
#include "stdlib.h"
#include <libkernel/libkernel.h>

char *sce_paf_strchr(const char *str, int ch)
{
	const volatile signed char *cursor = (const volatile signed char *)str;

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
	while (*(const volatile signed char *)cursor)
		cursor++;
	for (;;) {
		if (*(const volatile signed char *)cursor == (signed char)ch)
			return (char *)cursor;
		if (cursor == str)
			return NULL;
		cursor--;
	}
}

int sce_paf_strcmp(const char *left, const char *right)
{
	const volatile signed char *leftByte;
	const volatile signed char *rightByte;
	int leftCh;
	int rightCh;

	if (!left || !right) {
		if (left == right)
			return 0;
		return left ? 1 : -1;
	}
	leftByte = (const volatile signed char *)left;
	rightByte = (const volatile signed char *)right;
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
	const volatile signed char *leftByte = (const volatile signed char *)left;
	const volatile signed char *rightByte = (const volatile signed char *)right;
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
	const volatile signed char *leftByte = (const volatile signed char *)left;
	const volatile signed char *rightByte = (const volatile signed char *)right;

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
	const volatile signed char *leftByte = (const volatile signed char *)left;
	const volatile signed char *rightByte = (const volatile signed char *)right;
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
	const volatile signed char *src = (const volatile signed char *)source;

	while (*(const volatile signed char *)cursor)
		cursor++;
	for (;;) {
		signed char ch = *src++;

		*(volatile char *)cursor = ch;
		cursor++;
		if (!ch)
			return destination;
	}
}

char *sce_paf_strncat(char *destination, const char *source, size_t num)
{
	char *cursor = destination;
	const volatile signed char *src = (const volatile signed char *)source;
	uint32_t remaining = num;
	volatile signed char first;
	signed char ch;

	if (!destination || !source)
		return NULL;
	/* The first source read precedes the destination scan in Sony's code. */
	first = *src++;
	while (*(const volatile signed char *)cursor)
		cursor++;
	ch = first;
	*(volatile char *)cursor = ch;
	if (!ch)
		return destination;
	cursor++;
	for (;;) {
		/* Sony tests the count only after writing the preceding byte. */
		remaining--;
		if (remaining & 0x80000000U) {
			*(volatile char *)(cursor - 1) = '\0';
			return destination;
		}
		ch = *src++;
		*(volatile char *)cursor = ch;
		cursor++;
		if (!ch)
			return destination;
	}
}

size_t sce_paf_strspn(const char *str, const char *accept)
{
	const volatile signed char *cursor = (const volatile signed char *)str;
	const volatile signed char *start = cursor;
	const volatile signed char *member;
	int ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return cursor - start;
		member = (const volatile signed char *)accept;
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
	const volatile signed char *cursor = (const volatile signed char *)str;
	const volatile signed char *start = cursor;
	const volatile signed char *member;
	int ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return cursor - start;
		member = (const volatile signed char *)reject;
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
	const volatile signed char *cursor = (const volatile signed char *)str;
	const volatile signed char *member;
	int ch;

	for (;;) {
		ch = *cursor;
		if (!ch)
			return NULL;
		member = (const volatile signed char *)accept;
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
	const volatile signed char *cursor = (const volatile signed char *)source;
	const volatile signed char *begin = cursor;
	char *dest = destination;

	if (!destination || !source)
		return 0;
	while (size > 1) {
		const volatile signed char *current = cursor;
		int ch = *cursor++;

		size--;
		*(volatile char *)dest++ = ch;
		if (!ch) {
			/* Sony rereads the just-copied terminating byte. */
			cursor = current;
			goto measure;
		}
	}
	if (size == 1)
		*(volatile char *)dest = 0;
measure:
	while (*cursor++)
		;
	return (size_t)(cursor - begin - 1);
}

size_t sce_paf_strlcat(char *destination, const char *source, size_t size)
{
	char *dest = destination;
	const volatile signed char *cursor = (const volatile signed char *)source;
	const volatile signed char *begin = cursor;
	size_t prefix;

	if (!destination || !source)
		return 0;
	while (size) {
		if (!*(const volatile signed char *)dest)
			break;
		size--;
		dest++;
	}
	prefix = (size_t)(dest - destination);
	while (size > 1) {
		const volatile signed char *current = cursor;
		int ch = *cursor++;

		size--;
		*(volatile char *)dest++ = ch;
		if (!ch) {
			cursor = current;
			goto measure;
		}
	}
	if (size == 1)
		*(volatile char *)dest = 0;
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

void *ScePafStdc_5883A6E3(void *dst, const void *src, size_t count)
{
	return sce_paf_memcpy(dst, src, count);
}

void *ScePafStdc_75ECC54E(void *dst, int ch, size_t count)
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

PafString *PafStringConstruct(PafString *self, const void *allocator)
{
	PafStringInitialize(self);
	return self;
}

PafString *PafStringDestruct(PafString *self)
{
	PafStringDestroy(self);
	return self;
}

PafWString *PafWStringConstruct(PafWString *self, const void *allocator)
{
	PafWStringInitialize(self);
	return self;
}

PafWString *PafWStringDestruct(PafWString *self)
{
	PafWStringDestroy(self);
	return self;
}

size_t sce_paf_strlen(const char *str)
{
	if (!str)
		ScePafStdc_D41FD265();
	return sceClibStrnlen(str, (size_t)-1);
}

extern void *ScePafStdc_FC5CD359(size_t size);
extern void ScePafStdc_1B77082E(void *ptr);
extern int ScePafStdc_D608246A(void);

char PafStringEmpty;
uint16_t PafWStringEmpty;

PafWString *PafWStringErase(PafWString *self, size_t pos, size_t len)
{
	size_t count = self->count;
	uint16_t *dest;

	if (pos > count) {
		ScePafStdc_D608246A();
		count = self->count;
	}
	if (len > count - pos)
		len = count - pos;
	dest = ScePafStdc_FC5CD359(2 * (count - len + 1));
	if (dest) {
		count = self->count;
		sce_paf_memcpy(dest, self->ptr, 2 * pos);
		sce_paf_memcpy(dest + pos, self->ptr + pos + len,
			       2 * (count - pos - len));
		dest[count - len] = 0;
		if (self->ptr != &PafWStringEmpty)
			ScePafStdc_1B77082E(self->ptr);
		self->ptr = dest;
		self->count = count - len;
	} else {
		ScePafStdc_D608246A();
	}
	return self;
}

PafWString *PafWStringAppend(PafWString *self, size_t len, uint16_t value)
{
	uint16_t *dest = ScePafStdc_FC5CD359(2 * (self->count + len + 1));

	if (dest) {
		size_t count = self->count;
		size_t total = count + len;
		size_t i;

		sce_paf_memcpy(dest, self->ptr, 2 * count);
		count = self->count;
		for (i = 0; i < len; count = self->count)
			dest[count + i++] = value;
		dest[count + len] = 0;
		if (self->ptr != &PafWStringEmpty)
			ScePafStdc_1B77082E(self->ptr);
		self->ptr = dest;
		self->count = total;
	} else {
		ScePafStdc_D608246A();
	}
	return self;
}

PafWString *PafWStringAssign(PafWString *self, const uint16_t *src, size_t len)
{
	uint16_t *old = self->ptr;

	if (old == src)
		return self;
	if (old != &PafWStringEmpty)
		ScePafStdc_1B77082E(old);
	if (len && src && *src) {
		uint16_t *dest = ScePafStdc_FC5CD359(2 * (len + 1));

		if (dest) {
			self->ptr = dest;
			self->count = len;
			sce_paf_memcpy(dest, src, 2 * len);
			self->ptr[len] = 0;
			return self;
		}
		ScePafStdc_D608246A();
	}
	self->ptr = &PafWStringEmpty;
	self->count = 0;
	return self;
}

PafString *PafStringErase(PafString *self, size_t pos, size_t len)
{
	size_t count = self->count;
	char *dest;

	if (pos > count) {
		ScePafStdc_D608246A();
		count = self->count;
	}
	if (len > count - pos)
		len = count - pos;
	dest = ScePafStdc_FC5CD359(count - len + 1);
	if (dest) {
		count = self->count;
		sce_paf_memcpy(dest, self->ptr, pos);
		sce_paf_memcpy(dest + pos, self->ptr + pos + len,
			       count - pos - len);
		dest[count - len] = 0;
		if (self->ptr != &PafStringEmpty)
			ScePafStdc_1B77082E(self->ptr);
		self->ptr = dest;
		self->count = count - len;
	} else {
		ScePafStdc_D608246A();
	}
	return self;
}

PafString *PafStringAppend(PafString *self, size_t len, char value)
{
	char *dest = ScePafStdc_FC5CD359(self->count + len + 1);

	if (dest) {
		size_t count = self->count;
		size_t total = count + len;
		size_t i;

		sce_paf_memcpy(dest, self->ptr, count);
		count = self->count;
		for (i = 0; i < len; count = self->count)
			dest[count + i++] = value;
		dest[count + len] = 0;
		if (self->ptr != &PafStringEmpty)
			ScePafStdc_1B77082E(self->ptr);
		self->ptr = dest;
		self->count = total;
	} else {
		ScePafStdc_D608246A();
	}
	return self;
}

PafString *PafStringAssign(PafString *self, const char *src, size_t len)
{
	char *old = self->ptr;

	if (old == src)
		return self;
	if (old != &PafStringEmpty)
		ScePafStdc_1B77082E(old);
	if (len && src && *src) {
		char *dest = ScePafStdc_FC5CD359(len + 1);

		if (dest) {
			self->ptr = dest;
			self->count = len;
			sce_paf_memcpy(dest, src, len);
			self->ptr[len] = 0;
			return self;
		}
		ScePafStdc_D608246A();
	}
	self->ptr = &PafStringEmpty;
	self->count = 0;
	return self;
}

PafString *PafStringCopy(PafString *self, const PafString *src)
{
	if (self == src)
		return self;
	if (self->ptr != &PafStringEmpty)
		ScePafStdc_1B77082E(self->ptr);
	if (src->count) {
		char *dest = ScePafStdc_FC5CD359(src->count + 1);

		if (dest) {
			self->ptr = dest;
			self->count = src->count;
			sce_paf_memcpy(dest, src->ptr, src->count + 1);
			return self;
		}
		ScePafStdc_D608246A();
	}
	self->ptr = &PafStringEmpty;
	self->count = 0;
	return self;
}

PafString *PafStringAssignChar(PafString *self, char value)
{
	if (self->ptr != &PafStringEmpty)
		ScePafStdc_1B77082E(self->ptr);
	if (value) {
		char *dest = ScePafStdc_FC5CD359(2);

		if (dest) {
			self->ptr = dest;
			self->count = 1;
			*dest = value;
			self->ptr[1] = 0;
			return self;
		}
		ScePafStdc_D608246A();
	}
	self->ptr = &PafStringEmpty;
	self->count = 0;
	return self;
}

PafString *PafStringSubstring(PafString *result, const PafString *src,
			      size_t pos, size_t len)
{
	size_t count = src->count;
	const char *text;

	if (pos > count) {
		ScePafStdc_D608246A();
		count = src->count;
	}
	if (pos >= count) {
		PafStringInitialize(result);
		return NULL;
	}
	if (len == (size_t)-1 || pos + len > count)
		len = count - pos;
	text = src->ptr + pos;
	PafStringInitialize(result);
	return PafStringAssign(result, text, len);
}

size_t PafStringRfind(const PafString *self, const char *str,
		      size_t pos, size_t len)
{
	extern int sce_paf_strncmp(const char *left, const char *right, size_t len);
	size_t count = self->count;
	const char *begin, *current;

	if (!len)
		return pos < count ? pos : count;
	if (len > count)
		return (size_t)-1;
	begin = self->ptr;
	if (pos >= count - len)
		pos = count - len;
	current = begin + pos;
	for (;;) {
		if (*str == *current) {
			int comparison = sce_paf_strncmp(current, str, len);

			begin = self->ptr;
			if (!comparison)
				return current - begin;
		}
		if (current == begin)
			return (size_t)-1;
		--current;
	}
}

PafWString *PafWStringCopy(PafWString *self, const PafWString *src)
{
	if (src != self) {
		if (self->ptr != &PafWStringEmpty)
			ScePafStdc_1B77082E(self->ptr);
		if (src->count) {
			uint16_t *dest = ScePafStdc_FC5CD359(2 * (src->count + 1));

			if (dest) {
				size_t count = src->count;

				self->ptr = dest;
				self->count = count;
				sce_paf_memcpy(dest, src->ptr, 2 * count + 2);
				return self;
			}
			ScePafStdc_D608246A();
		}
		self->ptr = &PafWStringEmpty;
		self->count = 0;
	}
	return self;
}
