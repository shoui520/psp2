/* C layouts of paf::basic_string<char> and paf::basic_string<wchar_t>. */
#ifndef PAF_STD_STRING_H
#define PAF_STD_STRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

typedef struct PafString {
	char *ptr;
	size_t count;
	unsigned char alloc;
} PafString;

typedef struct PafWString {
	uint16_t *ptr;
	size_t count;
	unsigned char alloc;
} PafWString;

typedef struct PafStringVector {
	PafString *_data;
	size_t _size;
	size_t _capacity;
	unsigned char _allocator;
} PafStringVector;

typedef char PafStringVectorSizeCheck[sizeof(PafStringVector) == 16 ? 1 : -1];

int PafStringVectorReallocate(PafStringVector *self, size_t capacity);
PafStringVector *PafStringVectorConstructCopy(PafStringVector *self,
					   const PafStringVector *src);

typedef char PafStringSizeCheck[sizeof(PafString) == 12 ? 1 : -1];
typedef char PafWStringSizeCheck[sizeof(PafWString) == 12 ? 1 : -1];

extern char PafStringEmpty;
extern uint16_t PafWStringEmpty;

void *sce_paf_memset(void *dst, int ch, size_t len);
size_t sce_paf_strlen(const char *str);
size_t sce_paf_wcslen(const uint16_t *str);
int sce_paf_swprintf(uint16_t *buffer, size_t bufsz, const uint16_t *format, ...);
#define sce_paf_vswprintf ScePafStdc_FA5C2193
int sce_paf_vswprintf(uint16_t *buffer, size_t bufsz, const uint16_t *format,
		     va_list args);
PafWString *PafWStringConstruct(PafWString *self, const void *allocator);
PafWString *PafWStringDestruct(PafWString *self);

PafWString *PafWStringErase(PafWString *self, size_t pos, size_t len);
PafWString *PafWStringAppend(PafWString *self, size_t len, uint16_t value);
PafWString *PafWStringAssign(PafWString *self, const uint16_t *src, size_t len);
PafWString *PafWStringCopy(PafWString *self, const PafWString *src);
PafString *PafStringErase(PafString *self, size_t pos, size_t len);
PafString *PafStringAppend(PafString *self, size_t len, char value);
PafString *PafStringAssign(PafString *self, const char *src, size_t len);
PafString *PafStringCopy(PafString *self, const PafString *src);
PafString *PafStringAssignChar(PafString *self, char value);
PafString *PafStringSubstring(PafString *result, const PafString *src,
			      size_t pos, size_t len);
size_t PafStringRfind(const PafString *self, const char *str,
		      size_t pos, size_t len);

extern void *ScePafStdc_FC5CD359(size_t size);
extern void ScePafStdc_1B77082E(void *ptr);
extern int ScePafStdc_D608246A(void);
extern void *sce_paf_memcpy(void *dst, const void *src, size_t len);

static inline __attribute__((always_inline)) void PafStringInitialize(PafString *self)
{
	self->ptr = &PafStringEmpty;
	self->count = 0;
}

static inline __attribute__((always_inline)) void PafStringDestroy(PafString *self)
{
	if (self->ptr != &PafStringEmpty)
		ScePafStdc_1B77082E(self->ptr);
	self->ptr = (char *)-1;
	self->count = (size_t)-1;
}

static inline __attribute__((always_inline)) void PafWStringInitialize(PafWString *self)
{
	self->ptr = &PafWStringEmpty;
	self->count = 0;
}

static inline __attribute__((always_inline)) void PafWStringDestroy(PafWString *self)
{
	if (self->ptr != &PafWStringEmpty)
		ScePafStdc_1B77082E(self->ptr);
	self->ptr = (uint16_t *)-1;
	self->count = (size_t)-1;
}

/* The byte-span append overload is inlined at the queue-name call sites. */
static inline __attribute__((always_inline))
void PafStringAppendBytes(PafString *self, const char *src, size_t len)
{
	char *dest = ScePafStdc_FC5CD359(self->count + len + 1);

	if (dest) {
		size_t total = self->count + len;

		sce_paf_memcpy(dest, self->ptr, self->count);
		sce_paf_memcpy(dest + self->count, src, len);
		dest[self->count + len] = 0;
		if (self->ptr != &PafStringEmpty)
			ScePafStdc_1B77082E(self->ptr);
		self->ptr = dest;
		self->count = total;
	} else {
		ScePafStdc_D608246A();
	}
}

#endif
