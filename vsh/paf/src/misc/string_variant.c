/* A copy of the string an object keeps 20 bytes in. */
#include <stddef.h>
#include <stdint.h>
#include "../compiler.h"
#include "../std/string.h"

typedef struct PafEmbeddedString {
	uint8_t reserved[20];
	PafString value;
} PafEmbeddedString;

_Static_assert(offsetof(PafEmbeddedString, value) == 20,
	       "embedded string offset");

int ScePafMisc_20814ED1(const PafEmbeddedString *self, PafString *output)
{
	PafStringCopy(output, &self->value);
	return 0;
}
