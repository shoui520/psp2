/* String storage is reallocated for erase and append; there is no capacity field. */
#include "string.h"

extern void *ScePafStdc_FC5CD359(size_t size);
extern void ScePafStdc_1B77082E(void *ptr);
extern int ScePafStdc_D608246A(void);
extern void *sce_paf_memcpy(void *dst, const void *src, size_t len);

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
