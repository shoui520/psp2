/* SceKernelBootimage is a data container, with no executable entry point. */
#include <stdint.h>

/* Field names are descriptive; original type names are unknown. */
struct BootimageModuleEntry {
	const char *path;
	const void *image;
	uint32_t size;
};

struct BootimageEntries {
	uint32_t count;
	const struct BootimageModuleEntry *entries;
};

_Static_assert(sizeof(struct BootimageModuleEntry) == 12,
	       "bootimage entry layout");
_Static_assert(sizeof(struct BootimageEntries) == 8,
	       "bootimage header layout");

extern const struct BootimageEntries sceKernelBootimageEntries;

/* PLACEHOLDER: container fidelity requires every embedded child to be verified. */
