#ifndef SCE_CERTIFIED_FILE_H
#define SCE_CERTIFIED_FILE_H

#include <stdint.h>
#include <stddef.h>

/* Version-three certified-file envelope, shared by SELF and update packages. */
typedef struct cf_header {
	uint32_t magic;
	uint32_t version;
	uint16_t attribute;
	uint16_t category;
	uint32_t ext_header_size;
	uint64_t file_offset;
	uint64_t file_size;
	uint64_t cf_file_size;
	uint8_t versionExtension[8];
} cf_header;

_Static_assert(sizeof(cf_header) == 48, "certified-file header size");
_Static_assert(offsetof(cf_header, category) == 10 &&
	       offsetof(cf_header, file_offset) == 16 &&
	       offsetof(cf_header, versionExtension) == 40,
	       "certified-file header layout");

#endif
