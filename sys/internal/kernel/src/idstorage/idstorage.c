/* SceIdStorage: sector index, cached leaves and copy-on-write block commits. */

#include <stddef.h>
#include <stdint.h>
#include <sysmem/cpu.h>
#include <psp2kern/kernel/iofilemgr.h>
#include <sysmem/pmmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <idstorage/idstorage.h>
#include <acmgr/acmgr.h>
#include <sysmem/suspend.h>
#include <threadmgr/threadmgr.h>
#include <sdstor/mbr.h>
#include <sysmem/sysevent.h>
#include <threadmgr/threadmgr.h>

enum {
	LEAF_SIZE = 512,
	CACHE_COUNT = 32,
	INDEX_FREE = 0xffff,
	INDEX_RESERVED = 0xfff5,
	INDEX_PENDING = 0xfff4,
	ID_INVALID_ARGUMENT = (int)0x80230000,
	ID_INVALID_LEAF = (int)0x80230001,
	ID_INVALID_COUNT = (int)0x80230002,
	ID_NOT_FORMATTED = (int)0x80230003,
	ID_NO_SPACE = (int)0x80230004,
	ID_NOT_FOUND = (int)0x80230005,
	ID_ALREADY_EXISTS = (int)0x80230006,
};

/* Names of the private records and internal helpers are unknown. */
typedef struct IdCacheEntry {
	uint16_t slot;
	uint8_t state;
	uint8_t padding;
} IdCacheEntry;

typedef struct IdStorageState {
	SceUID memblock;
	SceUID mutex;
	SceOff offset;
	SceUID fd;
	int blockLeaves;
	uint8_t formatted, readOnly, dirty, padding;
	uint32_t reserved;
	uint16_t *index;
	IdCacheEntry cache[CACHE_COUNT];
	uint8_t *data;
} IdStorageState;

_Static_assert(sizeof(IdStorageState) == 0xa8, "storage state layout");
_Static_assert(offsetof(IdStorageState, cache) == 0x24, "cache offset");

static IdStorageState storage;
static const char *device;
static int totalLeaves, indexLeaves;
static SceUID heap;
static int useEntire;
static struct {
	uint32_t owner;
	uint16_t mode;
} permission;
static SceSblACMgrValue accessAttr;
static const char entireDevice[] = "sdstor0:int-lp-act-entire";
static const char idDevice[] = "sdstor0:int-lp-ign-idstor";
static const char name[] = "SceIdStorage";

/* an open flag of the storage device paths (name unknown) */
#define IO_DEVICE_OPEN	0x04000000

/* name unknown: temporarily set the per-thread I/O mode. */
static int enterIo(void)
{
	int flags = SceThreadmgrForDriver_56C18B11();

	if (flags >= 0)
		flags = SceThreadmgrForDriver_84EB1EA4(flags | 1);
	return flags;
}

/* name unknown: one seek and transfer, preserving Sony's import order. */
static int transfer(SceUID fd, SceOff offset, void *buffer, SceSize size, int write)
{
	int flags = enterIo();
	SceOff position = ksceIoLseek(fd, offset, 0);
	int ret = (int)position;

	if (position >= 0) {
		ret = write ? ksceIoWrite(fd, buffer, size) : ksceIoRead(fd, buffer, size);
		if (write && ret >= 0 && device)
			ksceIoSync(device, 0);
	}
	SceThreadmgrForDriver_84EB1EA4(flags);
	return ret;
}

/* name unknown: the idstorage partition's owner and mode, for type-8 attributes */
static int loadPermissions(void)
{
	SceMbr mbr;
	int fd, ret, i;

	accessAttr.type = 8;
	accessAttr.value = (unsigned char *)&permission;
	accessAttr.size = 6;
	permission.owner = 0;
	permission.mode = 0;
	fd = ksceIoOpen(entireDevice, IO_DEVICE_OPEN | SCE_O_RDONLY, 0);
	if (fd < 0)
		return fd;
	ret = ksceIoRead(fd, &mbr, sizeof(mbr));
	if (ret < 0)
		goto close;
	if (memcmp(mbr.magic, SCE_MBR_MAGIC, sizeof(mbr.magic))) {
		ret = ID_NOT_FORMATTED;
		goto close;
	}
	for (i = 0; i < 16; i++) {
		if (mbr.partitions[i].code == SCE_MBR_PARTITION_IDSTORAGE) {
			permission.owner = mbr.partitions[i].flags;
			permission.mode = mbr.partitions[i].mode;
			ret = 0;
			break;
		}
	}
close:
	ksceIoClose(fd);
	return ret;
}

static int commitStorage(void);

/* name unknown: invalidate all cached physical slots. */
static void clearCache(void)
{
	int i;

	for (i = 0; i < CACHE_COUNT; i++)
		storage.cache[i].state = 0;
}

int ksceIdStorageIsDirty(void)
{
	int i;

	if (storage.dirty)
		return 1;
	for (i = 0; i < CACHE_COUNT; i++)
		if (storage.cache[i].state == 3)
			return 1;
	return 0;
}

int ksceIdStorageFlush(void)
{
	int ret;

	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (ksceIdStorageIsDirty()) {
		ret = commitStorage();
		if (ret < 0) {
			ksceKernelUnlockMutex(storage.mutex, 1);
			return ret;
		}
	}
	clearCache();
	ksceKernelUnlockMutex(storage.mutex, 1);
	return 0;
}

/* name unknown: suspend event callback. */
static int storageSysEvent(int resume, int event, void *args, void *opt)
{
	if (!resume && event == SYSEVENT_SUSPEND_PREPARE(0))
		ksceIdStorageFlush();
	return 0;
}

/* name unknown: allocate index/cache and register suspend handling. */
static int initializeStorage(void)
{
	SceKernelHeapCreateOpt createOpt;
	SceAllocOpt allocOpt = {0x14, 0, 0x1000, 0, 0};
	SceOff bytes;
	int fd, ret, attempts;

	memset(&storage, 0, sizeof(storage));
	storage.blockLeaves = 32;
	/* Sony only initializes the first two words of this option record. */
	createOpt.size = sizeof(createOpt);
	createOpt.attr = 1;
	heap = SceSysmemForDriver_9328E0E8(name, 0x8000, &createOpt);
	if (heap < 0)
		return ID_NO_SPACE;
	storage.offset = 0;
	fd = ksceIoOpen(idDevice, IO_DEVICE_OPEN | SCE_O_RDWR, 0);
	if (fd >= 0 && useEntire) {
		ksceIoClose(fd);
		fd = -1;
	}
	if (fd < 0) {
		fd = ksceIoOpen(entireDevice, IO_DEVICE_OPEN | SCE_O_RDWR, 0);
		if (fd < 0)
			return fd;
		device = entireDevice;
		storage.offset = 0x40000;
		storage.fd = fd;
		bytes = 0x80000;
	} else {
		device = idDevice;
		storage.fd = fd;
		bytes = ksceIoLseek(fd, 0, 2);
		if (bytes < 1)
			return (int)bytes;
	}
	if ((bytes >> 9) >> 32)
		return ID_INVALID_ARGUMENT;
	totalLeaves = indexLeaves = (int)(bytes >> 9);
	storage.index = SceSysmemForDriver_49D4DD9B(heap, indexLeaves << 1, &allocOpt);
	if (!storage.index)
		return ID_NO_SPACE;
	memset(storage.index, 0, indexLeaves << 1);
	ret = SceSysmemForDriver_C94850C9(name, SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_RW, 0x4000, NULL);
	if (ret < 0)
		return ret;
	storage.memblock = ret;
	ret = SceSysmemForDriver_A841EDDA(ret, (void **)&storage.data);
	if (ret < 0) {
		SceSysmemForDriver_009E1C61(storage.memblock);
		return ret;
	}
	ret = ksceKernelCreateMutex(name, 2, 0, NULL);
	if (ret < 0) {
		SceSysmemForDriver_009E1C61(storage.memblock);
		return ret;
	}
	storage.mutex = ret;
	for (attempts = 4; attempts; attempts--) {
		ret = transfer(storage.fd, storage.offset, storage.index, indexLeaves << 1, 0);
		if (ret >= 0) {
			storage.formatted = 1;
			break;
		}
	}
	ret = loadPermissions();
	if (ret >= 0) {
		ret = sceKernelRegisterSysEventHandler(name, storageSysEvent, NULL);
		if (ret >= 0)
			ret = 0;
	}
	return ret;
}

int module_start(SceSize args, void *argp)
{
	int ret = initializeStorage();

	return ret < 0 ? ret : 0;
}

int ksceIdStorageRestart(int flush)
{
	SceProductMode mode = 0;
	int ret = ScePmMgrForDriver_2AC815A2(&mode);

	if (ret < 0)
		return ret;
	if (!mode)
		return ID_INVALID_ARGUMENT;
	if (flush)
		ksceIdStorageFlush();
	ksceKernelDeleteMutex(storage.mutex);
	SceSysmemForDriver_009E1C61(storage.memblock);
	if (storage.index)
		sceKernelFreeHeapMemory(heap, storage.index);
	sceKernelDeleteHeap(heap);
	useEntire = 1;
	ret = initializeStorage();
	return ret < 0 ? ret : 0;
}

int ksceIdStorageGetLeafSize(void)
{
	return LEAF_SIZE;
}

int ksceIdStorageIsFormatted(void)
{
	return storage.formatted;
}

int ksceIdStorageIsReadOnly(void)
{
	return storage.readOnly;
}

/* name unknown: locate an ID in the physical-slot index. */
static int findLeaf(uint32_t leaf)
{
	int i;

	for (i = 0; i < indexLeaves; i++)
		if (storage.index[i] == leaf)
			return i;
	return -1;
}

/* name unknown: allocate a cached slot, flushing a full cache first. */
static int cacheSlot(uint32_t leaf, int *slot)
{
	int i, ret;

	*slot = findLeaf(leaf);
	if (*slot < 0)
		return ID_NOT_FOUND;
	for (i = 0; i < CACHE_COUNT; i++)
		if (storage.cache[i].slot == *slot && storage.cache[i].state)
			return i;
	for (i = 0; i < CACHE_COUNT; i++) {
		if (!storage.cache[i].state) {
			storage.cache[i].slot = *slot;
			storage.cache[i].state = 1;
			return i;
		}
	}
	ret = ksceIdStorageFlush();
	if (ret < 0) {
		*slot = -1;
		return ret;
	}
	*slot = findLeaf(leaf);
	if (*slot < 0)
		return ID_NOT_FOUND;
	for (i = 0; i < CACHE_COUNT; i++) {
		if (!storage.cache[i].state) {
			storage.cache[i].slot = *slot;
			storage.cache[i].state = 1;
			return i;
		}
	}
	/* Sony's nominally unreachable path uses cache entry -1. */
	return -1;
}

/* name unknown: read/write helpers accept caller-held mutexes. */
static int readLeaf(uint32_t leaf, void *buffer, int lock)
{
	int ret, cache, slot, attempts;

	if (leaf >= 0xfff0)
		return ID_INVALID_LEAF;
	if (!buffer)
		return ID_INVALID_ARGUMENT;
	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	ret = SceSblACMgrForDriver_BE5667C5(sceKernelGetProcessId(), &accessAttr, 1);
	if (ret < 0)
		return ret;
	if (lock) {
		ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
		if (ret < 0)
			return ret;
	}
	cache = cacheSlot(leaf, &slot);
	if (cache < 0 && slot < 0) {
		ret = cache;
		goto unlock;
	}
	if (storage.cache[cache].state == 1) {
		for (attempts = 4; attempts; attempts--) {
			ret = transfer(storage.fd, storage.offset + (SceOff)slot * LEAF_SIZE,
				       storage.data + cache * LEAF_SIZE, LEAF_SIZE, 0);
			if (ret >= 0)
				break;
		}
		if (ret < 0) {
			storage.cache[cache].state = 0;
			goto unlock;
		}
		storage.cache[cache].state = 2;
	}
	memcpy(buffer, storage.data + cache * LEAF_SIZE, LEAF_SIZE);
	ret = 0;
unlock:
	if (lock)
		ksceKernelUnlockMutex(storage.mutex, 1);
	return ret;
}

static int writeLeaf(uint32_t leaf, const void *buffer, int lock)
{
	int ret, cache, slot;

	if (leaf >= 0xfff0)
		return ID_INVALID_LEAF;
	if (!buffer)
		return ID_INVALID_ARGUMENT;
	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	if (storage.readOnly)
		return -1;
	ret = SceSblACMgrForDriver_BE5667C5(sceKernelGetProcessId(), &accessAttr, 2);
	if (ret < 0)
		return ret;
	if (lock) {
		ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
		if (ret < 0)
			return ret;
	}
	cache = cacheSlot(leaf, &slot);
	if (cache < 0 && slot < 0) {
		ret = cache;
		goto unlock;
	}
	memcpy(storage.data + cache * LEAF_SIZE, buffer, LEAF_SIZE);
	storage.cache[cache].state = 3;
	ret = 0;
unlock:
	if (lock)
		ksceKernelUnlockMutex(storage.mutex, 1);
	return ret;
}

int ksceIdStorageReadLeaf(SceSize leaf, void *buffer)
{
	return readLeaf(leaf, buffer, 1);
}

int ksceIdStorageWriteLeaf(SceSize leaf, const void *buffer)
{
	SceProductMode mode = 0;
	int ret = ScePmMgrForDriver_2AC815A2(&mode);

	if (ret < 0)
		return ret;
	return mode ? writeLeaf(leaf, buffer, 1) : ID_INVALID_ARGUMENT;
}

int ksceIdStorageLookup(SceUInt32 leaf, SceUInt32 offset, void *buffer, SceUInt32 size)
{
	uint8_t data[LEAF_SIZE];
	int ret;

	if (leaf >= 0xfff0)
		return ID_INVALID_LEAF;
	if (offset >= LEAF_SIZE || offset + size > LEAF_SIZE || size > LEAF_SIZE)
		return ID_INVALID_ARGUMENT;
	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	ret = readLeaf(leaf, size == LEAF_SIZE ? buffer : data, 0);
	if (!ret && size != LEAF_SIZE && size)
		memcpy(buffer, data + offset, size);
	ksceKernelUnlockMutex(storage.mutex, 1);
	return ret < 0 ? ret : 0;
}

int ksceIdStorageUpdate(uint32_t leaf, uint32_t offset, const void *buffer, uint32_t size)
{
	uint8_t data[LEAF_SIZE];
	SceProductMode mode = 0;
	int ret = ScePmMgrForDriver_2AC815A2(&mode);

	if (ret < 0)
		return ret;
	if (!mode)
		return ID_INVALID_ARGUMENT;
	if (leaf >= 0xfff0)
		return ID_INVALID_LEAF;
	if (offset >= LEAF_SIZE || offset + size > LEAF_SIZE || size > LEAF_SIZE)
		return ID_INVALID_ARGUMENT;
	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	if (storage.readOnly)
		return -1;
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (size == LEAF_SIZE) {
		ret = writeLeaf(leaf, buffer, 0);
	} else {
		ret = readLeaf(leaf, data, 0);
		if (!ret) {
			memcpy(data + offset, buffer, size);
			ret = writeLeaf(leaf, data, 0);
		}
	}
	ksceKernelUnlockMutex(storage.mutex, 1);
	return ret < 0 ? ret : 0;
}

int ksceIdStorageFormat(void)
{
	SceProductMode mode = 0;
	int ret = ScePmMgrForDriver_2AC815A2(&mode);
	int i;

	if (ret < 0)
		return ret;
	if (!mode)
		return ID_INVALID_ARGUMENT;
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	SceKernelSuspendForDriver_4DF40893(0);
	storage.dirty = 0;
	clearCache();
	if (indexLeaves > 0)
		memset(storage.index, -1, indexLeaves << 1);
	for (i = 0; i < storage.blockLeaves; i++)
		storage.index[i] = INDEX_RESERVED;
	ret = transfer(storage.fd, storage.offset, storage.index, indexLeaves << 1, 1);
	if (ret >= 0) {
		storage.readOnly = 0;
		storage.formatted = 1;
		ret = 0;
	}
	SceKernelSuspendForDriver_2BB92967(0);
	ksceKernelUnlockMutex(storage.mutex, 1);
	return ret;
}

int ksceIdStorageUnformat(void)
{
	uint8_t data[LEAF_SIZE];
	SceProductMode mode = 0;
	int ret = ScePmMgrForDriver_2AC815A2(&mode);
	unsigned int i;

	if (ret < 0)
		return ret;
	if (!mode)
		return ID_INVALID_ARGUMENT;
	memset(data, 0, sizeof(data));
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	SceKernelSuspendForDriver_4DF40893(0);
	for (i = 0; i < (unsigned int)totalLeaves; i += storage.blockLeaves)
		transfer(storage.fd, storage.offset + (SceOff)i * LEAF_SIZE, data, sizeof(data), 1);
	SceKernelSuspendForDriver_2BB92967(0);
	storage.formatted = 0;
	ksceKernelUnlockMutex(storage.mutex, 1);
	return 0;
}

int ksceIdStorageGetFreeLeaves(void)
{
	SceKernelIntrStatus intr;
	int ret, i;

	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_821FC0EE();
	ret = 0;
	for (i = 0; i < indexLeaves; i++)
		if (storage.index[i] == INDEX_FREE)
			ret++;
	sceKernelCpuResumeIntr(intr);
	ksceKernelUnlockMutex(storage.mutex, 1);
	return ret;
}

int ksceIdStorageEnumId(void)
{
	int ret = ksceKernelLockMutex(storage.mutex, 1, NULL);

	if (ret >= 0) {
		ksceKernelUnlockMutex(storage.mutex, 1);
		ret = 0;
	}
	return ret;
}

/* name unknown: count free slots in one physical block. */
static int blockFree(int block)
{
	int i, count = 0;

	for (i = 0; i < storage.blockLeaves; i++)
		if (storage.index[block + i] == INDEX_FREE)
			count++;
	return count;
}

int ksceIdStorageCreateLeaf(uint32_t leaf)
{
	SceProductMode mode = 0;
	SceKernelIntrStatus intr;
	int ret = ScePmMgrForDriver_2AC815A2(&mode);
	int previous, block, chosen, minimum, count, i;

	if (ret < 0)
		return ret;
	if (!mode)
		return ID_INVALID_ARGUMENT;
	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	if (storage.readOnly)
		return -1;
	if (leaf >= 0xfff0)
		return ID_INVALID_LEAF;
	ret = SceSblACMgrForDriver_BE5667C5(sceKernelGetProcessId(), &accessAttr, 2);
	if (ret < 0)
		return ret;
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_821FC0EE();
	if (findLeaf(leaf) >= 0) {
		ret = ID_ALREADY_EXISTS;
		goto done;
	}
	if (leaf) {
		previous = findLeaf((leaf - 1) & 0xffff);
		if (previous >= 0 && previous + 1 < indexLeaves && storage.index[previous + 1] == INDEX_FREE) {
			storage.index[previous + 1] = leaf;
			storage.dirty = 1;
			ret = 0;
			goto done;
		}
	}
	chosen = -1;
	minimum = storage.blockLeaves + 1;
	for (block = 0; block < indexLeaves; block += storage.blockLeaves) {
		count = blockFree(block);
		if (count > 0 && count < minimum) {
			chosen = block;
			minimum = count;
		}
	}
	if (chosen < 0) {
		ret = ID_NO_SPACE;
		goto done;
	}
	for (i = 0; i < storage.blockLeaves; i++) {
		if (storage.index[chosen + i] == INDEX_FREE) {
			storage.index[chosen + i] = leaf;
			break;
		}
	}
	storage.dirty = 1;
	ret = 0;
done:
	sceKernelCpuResumeIntr(intr);
	ksceKernelUnlockMutex(storage.mutex, 1);
	return ret;
}

int ksceIdStorageCreateAtomicLeaves(const uint16_t *leaves, int count)
{
	SceProductMode mode = 0;
	SceKernelIntrStatus intr;
	int ret = ScePmMgrForDriver_2AC815A2(&mode);
	int i, block, n;

	if (ret < 0)
		return ret;
	if (!mode)
		return ID_INVALID_ARGUMENT;
	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	if (storage.readOnly)
		return -1;
	if (count < 1 || count > storage.blockLeaves)
		return ID_INVALID_COUNT;
	ret = SceSblACMgrForDriver_BE5667C5(sceKernelGetProcessId(), &accessAttr, 2);
	if (ret < 0)
		return ret;
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_821FC0EE();
	for (i = 0; i < count; i++) {
		if (leaves[i] >= 0xfff0) {
			ret = ID_INVALID_LEAF;
			goto done;
		}
		if (findLeaf(leaves[i]) >= 0) {
			ret = ID_ALREADY_EXISTS;
			goto done;
		}
	}
	ret = ID_NO_SPACE;
	for (block = 0; block < indexLeaves; block += storage.blockLeaves) {
		if (blockFree(block) >= count) {
			n = 0;
			for (i = 0; i < storage.blockLeaves; i++) {
				if (storage.index[block + i] == INDEX_FREE) {
					storage.index[block + i] = leaves[n++];
					if (n == count)
						break;
				}
			}
			storage.dirty = 1;
			ret = 0;
			break;
		}
	}
done:
	sceKernelCpuResumeIntr(intr);
	ksceKernelUnlockMutex(storage.mutex, 1);
	return ret;
}

int ksceIdStorageDeleteLeaf(uint32_t leaf)
{
	SceProductMode mode = 0;
	SceKernelIntrStatus intr;
	int ret = ScePmMgrForDriver_2AC815A2(&mode);
	int slot;

	if (ret < 0)
		return ret;
	if (!mode)
		return ID_INVALID_ARGUMENT;
	if (!storage.formatted)
		return ID_NOT_FORMATTED;
	if (storage.readOnly)
		return -1;
	if (leaf >= 0xfff0)
		return ID_INVALID_LEAF;
	ret = SceSblACMgrForDriver_BE5667C5(sceKernelGetProcessId(), &accessAttr, 2);
	if (ret < 0)
		return ret;
	ret = ksceKernelLockMutex(storage.mutex, 1, NULL);
	if (ret < 0)
		return ret;
	intr = SceCpuForDriver_821FC0EE();
	slot = findLeaf(leaf);
	if (slot < 0) {
		ret = ID_NOT_FOUND;
	} else {
		storage.index[slot] = INDEX_FREE;
		storage.dirty = 1;
		ret = 0;
	}
	sceKernelCpuResumeIntr(intr);
	ksceKernelUnlockMutex(storage.mutex, 1);
	return ret;
}

/* name unknown: commit dirty blocks through a copy-on-write index. */
static int commitStorage(void)
{
	SceAllocOpt opt = {0x14, 0, 0x1000, 0, 0};
	uint16_t *index;
	uint8_t *blockData;
	int i, j, block, destination, ret = 0;
	int count = storage.blockLeaves;

	index = SceSysmemForDriver_49D4DD9B(heap, indexLeaves << 1, &opt);
	if (!index)
		return ID_NO_SPACE;
	blockData = SceSysmemForDriver_49D4DD9B(heap, 0x4000, &opt);
	/* Sony leaks the first allocation when the second fails. */
	if (!blockData)
		return ID_NO_SPACE;
	SceKernelSuspendForDriver_4DF40893(0);
	memcpy(index, storage.index, indexLeaves << 1);
	for (i = 0; i < CACHE_COUNT; i++) {
		if (storage.cache[i].state != 3)
			continue;
		block = storage.cache[i].slot & -storage.blockLeaves;
		/* Sony ignores the result of this initial block read. */
		transfer(storage.fd, storage.offset + (SceOff)block * LEAF_SIZE, blockData,
			 storage.blockLeaves * LEAF_SIZE, 0);
		for (j = 0; j < CACHE_COUNT; j++) {
			if (storage.cache[j].state == 3 && storage.cache[j].slot >= block &&
			    storage.cache[j].slot < block + storage.blockLeaves) {
				memcpy(blockData + (storage.cache[j].slot - block) * LEAF_SIZE,
				       storage.data + j * LEAF_SIZE, LEAF_SIZE);
				storage.cache[j].state = 4;
			}
		}
		for (;;) {
			destination = -1;
			for (j = 0; j < indexLeaves; j += count) {
				int k;

				for (k = 0; k < count; k++)
					if (storage.index[j + k] != INDEX_FREE)
						break;
				if (k == count) {
					destination = j;
					break;
				}
			}
			if (destination < 0) {
				ret = ID_NO_SPACE;
				break;
			}
			for (j = 0; j < count; j++) {
				index[destination + j] = storage.index[block + j];
				index[block + j] = INDEX_FREE;
				storage.index[destination + j] = INDEX_PENDING;
			}
			ret = transfer(storage.fd, storage.offset + (SceOff)destination * LEAF_SIZE,
				       blockData, count * LEAF_SIZE, 1);
			count = storage.blockLeaves;
			if (ret >= 0)
				break;
		}
		storage.dirty = 1;
	}
	if (ret < 0) {
		for (i = 0; i < indexLeaves; i++)
			if (storage.index[i] == INDEX_PENDING)
				storage.index[i] = INDEX_FREE;
		for (i = 0; i < CACHE_COUNT; i++)
			if (storage.cache[i].state == 4)
				storage.cache[i].state = 3;
	} else if (!storage.dirty) {
		clearCache();
	} else {
		do {
			ret = transfer(storage.fd, storage.offset, index, indexLeaves << 1, 1);
		} while (ret < 0);
		memcpy(storage.index, index, indexLeaves << 1);
		storage.dirty = 0;
		clearCache();
	}
	SceKernelSuspendForDriver_2BB92967(0);
	sceKernelFreeHeapMemory(heap, index);
	sceKernelFreeHeapMemory(heap, blockData);
	return ret;
}
