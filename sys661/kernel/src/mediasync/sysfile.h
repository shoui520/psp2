#ifndef MEDIASYNC_SYSFILE_H
#define MEDIASYNC_SYSFILE_H

#include <stdint.h>
#include <stddef.h>

typedef int32_t SceUID;
typedef uint32_t SceSize;

#define SCE_SYSFILE_ERROR_NOMEM         ((int)0x80120001)
#define SCE_SYSFILE_ERROR_IO            ((int)0x80120002)
#define SCE_SYSFILE_ERROR_EINVAL        ((int)0x80120003)
#define SCE_SYSFILE_ERROR_BAD_MAGIC     ((int)0x80120004)
#define SCE_SYSFILE_ERROR_NOT_FOUND     ((int)0x80120005)
#define SCE_SYSFILE_ERROR_BAD_VERSION   ((int)0x80120007)
#define SCE_SYSFILE_ERROR_BUF_TOO_SMALL ((int)0x80120008)

typedef struct __attribute__((packed)) SceSysfileHeader {
	char magic[4];
	uint32_t version;
	uint32_t keyTableOffset;
	uint32_t dataTableOffset;
	uint32_t numEntries;
} SceSysfileHeader;

typedef struct __attribute__((packed)) SceSysfileIndexEntry {
	uint16_t keyOffset;
	uint16_t dataFmt;
	uint32_t usedSize;
	uint32_t totalSize;
	uint32_t dataOffset;
} SceSysfileIndexEntry;

typedef struct __attribute__((packed)) SceSysfileRwSlot {
	SceSysfileIndexEntry *entry;
	char *key;
	void *dataBuf;
	uint32_t slotFlags;
} SceSysfileRwSlot;

typedef struct __attribute__((packed)) SceSysfileContext {
	uint32_t flags;
	SceSysfileHeader *header;
	SceSysfileIndexEntry *indexTable;
	union {
		char *keyTable;
		SceUID fileDesc;
	};
	union {
		uint8_t *dataTable;
		uint32_t maxEntries;
	};
	uint32_t sortedCount;
	uint32_t reservedA;
	uint32_t reservedB;
	SceSysfileHeader inlineHeader;
	SceSysfileRwSlot *slots;
	char *keyStorage;
} SceSysfileContext;

typedef struct SceSysfileRoSearchKey {
	const char *key;
	const char *keyTable;
} SceSysfileRoSearchKey;

typedef void *(*SceSysfileMallocFn)(SceSize size);
typedef int (*SceSysfileFreeFn)(void *ptr);

int sceSystemFileGetValue(SceSysfileContext *ctx, const void *entryHandle, void *outBuf);
int sceSystemFileGetString(SceSysfileContext *ctx, const void *entryHandle, void *outBuf, uint32_t outSize);
int sceSystemFileLoadReadOnly(void *buf, SceSysfileContext **outCtx);
int sceSystemFileLoadAll2(const void *buf, uint32_t size, SceSysfileContext **outCtx);
int sceSystemFileExit(SceSysfileContext *ctx);
SceSysfileContext *sysfileAllocContext(int readOnly, uint32_t maxEntries);
void sysfileInitSlotTable(SceSysfileContext *ctx);
int sysfileCheckHeader(const SceSysfileHeader *hdr, uint32_t size);
int sysfileCheckKeyTable(const uint8_t *buf, uint32_t keyOff, const SceSysfileHeader *hdr, uint32_t size);
int sysfileCheckIndexTable(const uint8_t *buf, uint32_t indexOff, const SceSysfileHeader *hdr, uint32_t size);
int sceSystemFileGetIndex(SceSysfileContext *ctx, const char *key, void **outEntry);
void sysfileClearRwSlot(SceSysfileContext *ctx, SceSysfileRwSlot *slot);
int sysfileCompareRwSlot(const void *lhs, const void *rhs);
int sysfileCompareKeyToRwSlot(const void *key, const void *slot);
int sysfileCompareKeyToRoEntry(const void *searchKey, const void *entry);
int sceSystemFileSetAllocator(SceSysfileMallocFn mallocFn, SceSysfileFreeFn freeFn);
int sysfileMemcmp(const char *lhs, const char *rhs, int len);
int sysfileStrcmp(const uint8_t *lhs, const uint8_t *rhs);
void *sysfileBsearch(const void *key, const void *base, uint32_t count, uint32_t elemSize, int (*cmp)(const void *, const void *));
void sysfileShellSort(void *base, uint32_t count, uint32_t elemSize, int (*cmp)(const void *, const void *));
char *sysfileStrcpy(char *dst, const char *src);

#endif /* MEDIASYNC_SYSFILE_H */
