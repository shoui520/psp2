/* String storage is reallocated for erase and append; there is no capacity field. */
#include "../compiler.h"
#include "string.h"
#include "stdlib.h"
#include <libkernel/libkernel.h>

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
