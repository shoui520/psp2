#ifndef APPMGR_PSF_INTERNAL_H
#define APPMGR_PSF_INTERNAL_H

#include <scetypes.h>
#include <stddef.h>

typedef struct AppmgrPsfDiskEntry {
	SceUInt16 keyOffset;
	SceUInt16 format;
	SceSize length;
	SceSize capacity;
	SceUInt32 valueOffset;
} AppmgrPsfDiskEntry;

typedef struct AppmgrPsfEntry {
	AppmgrPsfDiskEntry *diskEntry;
	unsigned char *key;
	void *value;
	SceUInt32 flags;
} AppmgrPsfEntry;

typedef struct AppmgrPsfSearchKey {
	const unsigned char *key;
	const unsigned char *keyTable;
} AppmgrPsfSearchKey;

typedef struct AppmgrPsfHeader {
	unsigned char magic[4];
	SceUInt32 version;
	SceUInt32 keyTableOffset;
	SceUInt32 valueTableOffset;
	SceSize entryCount;
} AppmgrPsfHeader;

/* The parsed form is a complete twenty-byte handle. */
typedef struct AppmgrPsfContext {
	SceUInt32 flags;
	AppmgrPsfHeader *header;
	AppmgrPsfDiskEntry *diskEntries;
	unsigned char *keyTable;
	union {
		unsigned char *valueTable;
		SceSize entryCapacity;
	} storage;
} AppmgrPsfContext;

typedef struct AppmgrPsfWriteContext {
	AppmgrPsfContext context;
	SceSize entryHighWater;
	SceSize diskEntryHighWater;
	SceSize keyHighWater;
	AppmgrPsfHeader embeddedHeader;
	AppmgrPsfEntry *entries;
	unsigned char (*keys)[32];
	AppmgrPsfEntry entryStorage[128];
	AppmgrPsfDiskEntry diskStorage[128];
	unsigned char keyStorage[128][32];
} AppmgrPsfWriteContext;

typedef struct AppmgrPsfBufferWriter {
	unsigned char *buffer;
	SceSize offset;
	SceSize remaining;
} AppmgrPsfBufferWriter;

typedef struct AppmgrPsfWriter {
	int (*write)(void *argument, const void *data, SceSize size);
	void *argument;
} AppmgrPsfWriter;

_Static_assert(sizeof(AppmgrPsfDiskEntry) == 16, "PSF disk entry");
_Static_assert(sizeof(AppmgrPsfEntry) == 16, "PSF runtime entry");
_Static_assert(sizeof(AppmgrPsfSearchKey) == 8, "PSF key search");
_Static_assert(offsetof(AppmgrPsfEntry, key) == 4, "PSF runtime key");
_Static_assert(offsetof(AppmgrPsfEntry, flags) == 12, "PSF entry flags");
_Static_assert(sizeof(AppmgrPsfHeader) == 20, "PSF file header");
_Static_assert(sizeof(AppmgrPsfContext) == 20, "PSF parsed handle");
_Static_assert(sizeof(AppmgrPsfWriteContext) == 8252, "PSF mutable object");
_Static_assert(offsetof(AppmgrPsfWriteContext, embeddedHeader) == 32,
	"PSF embedded header");
_Static_assert(offsetof(AppmgrPsfWriteContext, entries) == 52,
	"PSF runtime entry pointer");
_Static_assert(offsetof(AppmgrPsfWriteContext, keys) == 56, "PSF key slot pointer");
_Static_assert(offsetof(AppmgrPsfWriteContext, entryStorage) == 60,
	"PSF runtime entry storage");
_Static_assert(offsetof(AppmgrPsfWriteContext, diskStorage) == 2108,
	"PSF disk entry storage");
_Static_assert(offsetof(AppmgrPsfWriteContext, keyStorage) == 4156,
	"PSF key storage");
_Static_assert(sizeof(AppmgrPsfBufferWriter) == 12, "PSF buffer writer");
_Static_assert(sizeof(AppmgrPsfWriter) == 8, "PSF writer descriptor");

void *appmgrPsfAllocMemory(SceSize size);
int appmgrPsfFreeMemory(void *memory);
void *appmgrPsfCopyMemory(void *destination, const void *source, SceSize size);
void *appmgrPsfSetMemory(void *destination, int value, SceSize size);
int appmgrPsfCompareMemory(const signed char *left, const signed char *right,
	SceSize size);
SceSize appmgrPsfStringLength(const signed char *string);
int appmgrPsfCompareString(const unsigned char *left, const unsigned char *right);
void *appmgrPsfBinarySearch(const void *key, void *array, SceSize count,
	SceSize width, int (*compare)(const void *, const void *));
void appmgrPsfShellSort(void *array, SceSize count, SceSize width,
	int (*compare)(const void *, const void *));
void appmgrPsfSortEntries(void *object);
void appmgrPsfRemoveEntry(void *object, AppmgrPsfEntry *entry);
int appmgrPsfValidateEntries(const void *data, SceSize entryOffset,
	const AppmgrPsfHeader *header, SceSize size);
int appmgrPsfBufferWrite(void *argument, const void *data, SceSize size);
int appmgrPsfSerialize(AppmgrPsfWriter *writer, AppmgrPsfWriteContext *psf);
int appmgrPsfWrite(void *buffer, SceSize capacity, void *psf);
int appmgrPsfGetSize(void *object, SceSize *size);
int appmgrPsfDestroy(void *psf);
int appmgrPsfParse(const void *data, SceSize size, void **object);
int appmgrPsfCreate(void **object);
int appmgrPsfInitializeHeader(void *object, const void *magic,
	unsigned int majorVersion, unsigned int minorVersion);

#endif
