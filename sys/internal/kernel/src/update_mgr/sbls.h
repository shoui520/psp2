#ifndef UPDATE_MGR_SBLS_H
#define UPDATE_MGR_SBLS_H

#include <stdint.h>
#include <stddef.h>

#define SCE_SLB2_MAGIC 0x32424c53u

typedef struct SceSlb2Entry {
	uint32_t fileStartSector;
	uint32_t fileSizeInByte;
	uint32_t reserved[2];
	char m_file_name[32];
} SceSlb2Entry;

typedef struct SceSlb2Header {
	uint32_t magic;
	uint32_t version;
	uint32_t flags;
	uint32_t entryNum;
	uint32_t sizeInSector;
	uint32_t reserved[3];
	SceSlb2Entry entryList[];
} SceSlb2Header;

_Static_assert(sizeof(SceSlb2Entry) == 48 &&
	       offsetof(SceSlb2Entry, m_file_name) == 16,
	       "SLB2 entry layout");
_Static_assert(sizeof(SceSlb2Header) == 32 &&
	       offsetof(SceSlb2Header, entryList) == 32,
	       "SLB2 header layout");

/* storage_sbls.c */
int StorageUtilSblsSearchEntry(const SceSlb2Header *header,
		const SceSlb2Entry *entry, const char *name, uint32_t *index,
		uint32_t *sector, uint32_t *size);
int StorageUtilSblsRename(const SceSlb2Header *header, SceSlb2Entry *entries,
		const char *oldName, const char *newName);

#endif
