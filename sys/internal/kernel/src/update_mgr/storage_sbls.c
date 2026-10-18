#include <sysmem/sysclib.h>
#include "update_internal.h"
#include "sbls.h"

int StorageUtilSblsSearchEntry(const SceSlb2Header *header,
		const SceSlb2Entry *entry, const char *name, uint32_t *index,
		uint32_t *sector, uint32_t *size)
{
	uint32_t current, count;
	size_t length;
	int result;

	if (!header || !entry)
		return (int)0x800f0216;
	if (!name || !index)
		return (int)0x800f0216;
	if (!sector || !size)
		return (int)0x800f0216;
	current = 0;
	if (header->magic != SCE_SLB2_MAGIC)
		return (int)0x800f0224;
	length = strnlen(name, 32);
	count = header->entryNum;
	if (length == 32)
		return (int)0x800f0216;
	if (!count)
		return (int)0x800f0202;
	do {
		result = memcmp(name, entry->m_file_name, length);
		if (!result) {
			*sector = entry->fileStartSector;
			*size = entry->fileSizeInByte;
			*index = current;
			return result;
		}
		current++;
		entry++;
	} while (current != count);
	return (int)0x800f0202;
}

int StorageUtilSblsRename(const SceSlb2Header *header, SceSlb2Entry *entries,
		const char *oldName, const char *newName)
{
	uint32_t index, sector, size;
	char *name;
	size_t length;
	int result;

	if (!header || !entries || !oldName || !newName)
		return (int)0x800f0216;
	if (strnlen(oldName, 32) == 32)
		return (int)0x800f0216;
	length = strnlen(newName, 32);
	if (length == 32)
		return (int)0x800f0216;
	result = StorageUtilSblsSearchEntry(header, entries, oldName,
		&index, &sector, &size);
	if (!result) {
		name = entries[index].m_file_name;
		memset(name, 0, 32);
		memcpy(name, newName, length);
		result = 0;
	}
	return result;
}
