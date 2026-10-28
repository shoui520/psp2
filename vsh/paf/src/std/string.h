/* C layouts of paf::basic_string<char> and paf::basic_string<wchar_t>. */
#ifndef PAF_STD_STRING_H
#define PAF_STD_STRING_H

#include <stddef.h>
#include <stdint.h>

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

typedef char PafStringSizeCheck[sizeof(PafString) == 12 ? 1 : -1];
typedef char PafWStringSizeCheck[sizeof(PafWString) == 12 ? 1 : -1];

extern char PafStringEmpty;
extern uint16_t PafWStringEmpty;

PafWString *PafWStringErase(PafWString *self, size_t pos, size_t len);
PafWString *PafWStringAppend(PafWString *self, size_t len, uint16_t value);
PafWString *PafWStringAssign(PafWString *self, const uint16_t *src, size_t len);
PafString *PafStringErase(PafString *self, size_t pos, size_t len);
PafString *PafStringAppend(PafString *self, size_t len, char value);
PafString *PafStringAssign(PafString *self, const char *src, size_t len);

#endif
