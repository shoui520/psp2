/* SceSdstor partition management (Sony source: sdpart.c). */
#include <sysmem/sysroot.h>
#include "sdstor_internal.h"
#include <sysmem/debug.h>
#include <stdio.h>
#include <string.h>
#include <sdstor/mbr.h>
#include <sdif/sdif.h>
#include <stdlib.h>
#include <sysmem/sysclib.h>

#if FW_DEBUG
static uint8_t sdpartXmcPort = 5;
static uint8_t sdpartMcPort = 5;
#else
static uint8_t sdpartMcPort = 5;
static uint8_t sdpartXmcPort = 5;
#endif

static const SceSdstorPartitionAccess sdpartInternalAccess = {
	.flags = 0xf4f,
	.mode = 0,
};

static const SceSdstorPartitionAccess sdpartDefaultAccess = {
	.flags = 0xfff,
	.mode = 0,
};

static const SceSdstorPartitionAccess sdpartGameCardAccess = {
	.flags = 0xf4f,
	.mode = 0,
};

static const SceSdstorPartitionAccess sdpartSdAccess = {
	.flags = 0xfff,
	.mode = 0,
};

static const SceSdstorPartitionAccess sdpartUsbAccess = {
	.flags = 0xfff,
	.mode = 0,
};

static const char *const sdpartKindNames[2] = { "lp-", "pp-" };

static const char *const sdpartPartitionNames[16] = {
	"unused", "idstor", "sloader", "os", "vsh", "vshdata", "vtrm",
	"user", "userext", "gamero", "gamerw", "updater", "sysdata",
	"mediaid", "pidata", "entire",
};

static const uint8_t sdpartNamePorts[7] = { 0, 1, 1, 2, 3, 4, 255 };
static const char *const sdpartPortNames[7];
static const char *const sdpartStatusNames[3];

static const SceSdstorNameComponent sdpartNameComponents[4] = {
	{ 7, sdpartPortNames },
	{ 2, sdpartKindNames },
	{ 3, sdpartStatusNames },
	{ 16, sdpartPartitionNames },
};

static const char *const sdpartPortNames[7] = {
	"int-", "ext-", "gcd-", "mcd-", "uma-", "usd-", "xmc-",
};

static const char *const sdpartStatusNames[3] = { "ina-", "act-", "ign-" };

static SceSdstorSdpart sdpartTables[5];
static uint8_t sdpartMbrBuffers[5][512];

int sdpart_init_entire_entry(unsigned int port, SceSdstorSdpart *table,
			     uint32_t sectorCount,
			     const SceSdstorPartitionAccess *access)
{
	SceSdstorPartitionEntry *entry = &table->pentry[16];
	int result;

	SCE_DBG_ASSERT(table != NULL, 836);
	SCE_DBG_ASSERT(access != NULL, 837);
	entry->start_lba = 0;
	entry->partition_entry = NULL;
	entry->n_sectors = sectorCount;
	entry->sdpart = table;
	result = snprintf(entry->numeric_name, sizeof(entry->numeric_name),
			  "%01d%02d", port, 16);
	entry->flags = access->flags;
	entry->mode = access->mode;
	return result;
}

#if FW_DEBUG
void
#else
static inline void
#endif
sdpart_transfer_lba_to_chs(uint32_t capacity, uint32_t lba, uint8_t chs[3])
{
	uint32_t heads, sectorsPerTrack, sectorsPerCylinder, remainder;
	uint16_t cylinder;
	uint8_t head, sector;

	if (capacity <= 0x1000) {
		heads = 2;
		sectorsPerTrack = 16;
	} else if (capacity <= 0x8000) {
		heads = 2;
		sectorsPerTrack = 32;
	} else if (capacity <= 0x10000) {
		heads = 4;
		sectorsPerTrack = 32;
	} else if (capacity <= 0x40000) {
		heads = 8;
		sectorsPerTrack = 32;
	} else if (capacity <= 0x80000) {
		heads = 16;
		sectorsPerTrack = 32;
	} else {
		sectorsPerTrack = 63;
		if (capacity <= 0xfc000)
			heads = 16;
		else if (capacity <= 0x1f8000)
			heads = 32;
		else if (capacity <= 0x3f0000)
			heads = 64;
		else if (capacity <= 0x7e0000)
			heads = 128;
		else
			heads = 255;
	}
	sectorsPerCylinder = heads * sectorsPerTrack;
	SCE_DBG_PRINTF_LEVEL(2, 6, 1006,
		"capa=%#x lba=%#x, heads=%d sec_per_track=%d\n",
		capacity, lba, heads, sectorsPerTrack);
	cylinder = lba / sectorsPerCylinder;
	remainder = lba % sectorsPerCylinder;
	head = remainder / sectorsPerTrack;
	sector = remainder % sectorsPerTrack + 1;
	SCE_DBG_PRINTF_LEVEL(2, 6, 1011, "head=%d sect=%d  cyl=%d\n",
			     head, sector, cylinder);
	SCE_DBG_ASSERT_FUNC(1, 972, "sdpart_encode_chs");
	SCE_DBG_ASSERT_FUNC(cylinder < 1024, 973, "sdpart_encode_chs");
	chs[0] = head;
	chs[2] = cylinder;
	chs[1] = sector | ((cylinder & 0x300) >> 2);
}

int sdpart_parse_intpar(unsigned int port, SceSdstorSdpart *table,
		       SceSdstorSddev *device)
{
	SceMbr *mbr;
	SceMbrPartitionEntry *source;
	SceSdstorPartitionEntry *entry;
	unsigned int index;

	SCE_DBG_ASSERT(port <= 4, 786);
	SCE_DBG_ASSERT(table != NULL, 787);
	SCE_DBG_ASSERT(device != NULL, 788);
	mbr = table->mbr_ptr;
	SCE_DBG_ASSERT_FUNC(mbr != NULL, 1350, "sdpart_valid_internal_mbr");
	if (strncmp(mbr->magic, SCE_MBR_MAGIC, sizeof(mbr->magic)) != 0 ||
	    mbr->signature != 0xaa55)
		return 0x8001002f;
	SCE_DBG_ASSERT_FUNC(table != NULL, 811, "sdpart_init_internal_entry");
	source = mbr->partitions;
	entry = table->pentry;
	index = 0;
	do {
		__builtin_prefetch(&entry[4].n_sectors);
		if (!source->code) {
			memset(entry, 0, sizeof(*entry));
		} else {
			entry->start_lba = source->offset;
			entry->n_sectors = source->size;
			entry->sdpart = table;
			entry->partition_entry = source;
			snprintf(entry->numeric_name, sizeof(entry->numeric_name),
				 "%01d%02d", port, index);
			entry->flags = source->flags;
			entry->mode = source->mode;
		}
		++source;
		++entry;
	} while (++index != 16);
	table->externalFormat = 0;
	return 0;
}

int sdpart_init(void)
{
	SceSdstorSdpart *table = sdpartTables;
	unsigned int port = 0;
	int result;

	for (;;) {
		SCE_DBG_ASSERT(1, 207);
		SCE_DBG_ASSERT_FUNC(1, 192, "sdpart_get_table");
		result = sceKernelInitializeFastMutex(&table->fastmutex,
				"SceSdStorPartitionTable", 0, 0);
		if (result != 0)
			break;
		table->externalFormat = 0;
		table->stat = 0;
		table->prev_stat = 0;
		table->sddev = NULL;
		table->mediaInfo.blockSize = 0;
		table->mediaInfo.sectorCount = 0;
		table->mediaInfo.eraseBlockCount = 0;
		table->mbr_ptr = sdpartMbrBuffers[port];
		memset(table->pentry, 0, sizeof(table->pentry));
		++table;
		if (++port == 5)
			return 0;
	}
	SCE_DBG_PRINTF(15, 216, "0x%08x\n", result);
	return result;
}

int sdpart_close(unsigned int port)
{
	SceSdstorSdpart *table;
	int result;

	SCE_DBG_ASSERT(port <= 4, 321);
	SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
	table = port <= 4 ? &sdpartTables[port] : NULL;
	result = lockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	if (table->stat) {
		table->externalFormat = 0;
		table->stat = 0;
		table->sddev = NULL;
		table->mbr_ptr = sdpartMbrBuffers[port];
		table->mediaInfo.blockSize = 0;
		table->mediaInfo.sectorCount = 0;
		table->mediaInfo.eraseBlockCount = 0;
		memset(table->pentry, 0, sizeof(table->pentry));
	}
	result = sceKernelUnlockFastMutex(&table->fastmutex);
	return result < 0 ? result : 0;
}

int sdpart_remove(unsigned int port)
{
	SceSdstorSdpart *table;
	uint8_t previous;
	int result;

	SCE_DBG_ASSERT_FUNC(port <= 4, 258, "sdpart_suspend_stat");
	SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
	table = port <= 4 ? &sdpartTables[port] : NULL;
	result = lockFastMutex(&table->fastmutex);
	if (result >= 0) {
		previous = table->stat;
		table->stat = 2;
		table->prev_stat = previous;
		result = sceKernelUnlockFastMutex(&table->fastmutex);
		if (result >= 0)
			return 0;
	}
	SCE_DBG_PRINTF_LEVEL(2, 15, 352,
			     "sdpart_suspend_stat() returns 0x%08x\n", result);
	return result;
}

int sdpart_insert(unsigned int port)
{
	SceSdstorSdpart *table;
	int result;

	SCE_DBG_ASSERT_FUNC(port <= 4, 321, "sdpart_close");
	SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
	table = port <= 4 ? &sdpartTables[port] : NULL;
	result = lockFastMutex(&table->fastmutex);
	if (result >= 0) {
		if (table->stat) {
			table->externalFormat = 0;
			table->stat = 0;
			table->sddev = NULL;
			table->mbr_ptr = sdpartMbrBuffers[port];
			table->mediaInfo.blockSize = 0;
			table->mediaInfo.sectorCount = 0;
			table->mediaInfo.eraseBlockCount = 0;
			memset(table->pentry, 0, sizeof(table->pentry));
		}
		result = sceKernelUnlockFastMutex(&table->fastmutex);
		if (result >= 0)
			return 0;
	}
	SCE_DBG_PRINTF_LEVEL(2, 15, 366, "sdpart_close() returns 0x%08x\n", result);
	return result;
}

int sdpart_suspend(unsigned int port)
{
	SceSdstorSdpart *table;
	uint8_t previous;
	int result;

	SCE_DBG_ASSERT_FUNC(port <= 4, 258, "sdpart_suspend_stat");
	SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
	table = port <= 4 ? &sdpartTables[port] : NULL;
	result = lockFastMutex(&table->fastmutex);
	if (result >= 0) {
		previous = table->stat;
		table->stat = 2;
		table->prev_stat = previous;
		result = sceKernelUnlockFastMutex(&table->fastmutex);
		if (result >= 0)
			return 0;
	}
	SCE_DBG_PRINTF_LEVEL(2, 15, 380,
			     "sdpart_suspend_stat() returns 0x%08x\n", result);
	return result;
}

int sdpart_resume_stat(unsigned int port)
{
	SceSdstorSdpart *table;
	int result;

	SCE_DBG_ASSERT(port <= 4, 275);
	SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
	table = port <= 4 ? &sdpartTables[port] : NULL;
	result = lockFastMutex(&table->fastmutex);
	if (result >= 0) {
		table->stat = table->prev_stat;
		result = sceKernelUnlockFastMutex(&table->fastmutex);
		if (result >= 0)
			return 0;
	}
	SCE_DBG_PRINTF_LEVEL_FUNC(2, 15, 394, "sdpart_resume",
				  "sdpart_resume_stat() returns 0x%08x\n", result);
	return result;
}

int sdpart_valid_sd_mbr(SceSdstorMbr *mbr, uint32_t capacity)
{
	SceSdstorMbrPartition empty = { 0 };
	SceSdstorMbrPartition *partition = &mbr->partitions[0];
	uint32_t start, end, last, alignment;
	unsigned int systemId;
	uint8_t chs[3];

	SCE_DBG_ASSERT(mbr != NULL, 1159);
	if (memcmp(&empty, &mbr->partitions[1], sizeof(empty)) != 0) {
		SCE_DBG_PRINTF(6, 1166, "partition #%d is not empty\n", 1);
		return 0x8001002f;
	}
	if (memcmp(&empty, &mbr->partitions[2], sizeof(empty)) != 0) {
		SCE_DBG_PRINTF(6, 1166, "partition #%d is not empty\n", 2);
		return 0x8001002f;
	}
	if (memcmp(&empty, &mbr->partitions[3], sizeof(empty)) != 0) {
		SCE_DBG_PRINTF(6, 1166, "partition #%d is not empty\n", 3);
		return 0x8001002f;
	}
	if (mbr->signature != 0xaa55) {
		SCE_DBG_PRINTF(6, 1172, "MBR Signature wrong %#x\n", mbr->signature);
		return 0x8001002f;
	}
	start = ((uint32_t)partition->startLba[2] << 16) |
		((uint32_t)partition->startLba[1] << 8) |
		partition->startLba[0] | ((uint32_t)partition->startLba[3] << 24);
	if (partition->bootFlag & 0xf7) {
		SCE_DBG_PRINTF(6, 1181, "Boot flag wrong %#x\n", partition->bootFlag);
		return 0x8001002f;
	}
	end = (((uint32_t)partition->sectorCount[2] << 16) |
		((uint32_t)partition->sectorCount[1] << 8) |
		partition->sectorCount[0] |
		((uint32_t)partition->sectorCount[3] << 24)) + start;
	last = end - 1;
	if (capacity >= 0x400000) {
		if (capacity < 0x4000000)
			systemId = last <= 0xfb03ff ? 11 : 12;
		else
			systemId = 7;
	} else if (capacity > 0x7fa7) {
		systemId = capacity < 0x10000 ? 4 : 6;
	} else {
		systemId = 1;
	}
	if (partition->systemId != systemId) {
		SCE_DBG_PRINTF(6, 1188, "System ID wrong %#x %#x\n",
			       partition->systemId, systemId);
		return 0x8001002f;
	}
	if (start <= 0xfb03ff) {
		sdpart_transfer_lba_to_chs(capacity, start, chs);
	} else {
		chs[0] = 0xfe;
		chs[2] = 0xff;
		chs[1] = 0xff;
	}
	if (memcmp(chs, partition->startChs, sizeof(chs)) != 0) {
		SCE_DBG_PRINTF(6, 1197,
			"start CHS is wrong: expected=%02x%02x%02x actual=%02x%02x%02x\n",
			chs[0], chs[1], chs[2], partition->startChs[0],
			partition->startChs[1], partition->startChs[2]);
		return 0x8001002f;
	}
	if (last <= 0xfb03ff) {
		sdpart_transfer_lba_to_chs(capacity, last, chs);
	} else {
		chs[0] = 0xfe;
		chs[2] = 0xff;
		chs[1] = 0xff;
	}
	if (memcmp(chs, partition->endChs, sizeof(chs)) != 0) {
		SCE_DBG_PRINTF(6, 1206,
			"ending CHS is wrong: expected=%02x%02x%02x actual=%02x%02x%02x\n",
			chs[0], chs[1], chs[2], partition->endChs[0],
			partition->endChs[1], partition->endChs[2]);
		return 0x8001002f;
	}
	if (start >= capacity || end > capacity) {
		SCE_DBG_PRINTF(6, 1212, "start LBA is too large\n");
		return 0x8001002f;
	}
	if (capacity >= 0x400000) {
		if (capacity == 0x400000)
			alignment = 128;
		else if (capacity <= 0x4000000)
			alignment = 0x2000;
		else if (capacity <= 0x10000000)
			alignment = 0x8000;
		else if (capacity <= 0x40000000)
			alignment = 0x10000;
		else
			alignment = 0x20000;
		if (start % alignment) {
			SCE_DBG_PRINTF(6, 1220, "start LBA is not aligned\n");
			return 0x8001002f;
		}
	}
	SCE_DBG_PRINTF(6, 1225, "MBR seems conforming SD spec\n");
	return 0;
}

int sdpart_parse_extpar(unsigned int port, SceSdstorSdpart *table,
#if FW_DEBUG
		       SceSdstorSddev *device,
#else
		       const uint8_t *hport,
#endif
		       int mediaType)
{
	SceSdstorMbr *mbr;
	SceSdstorMbrPartition partition;
	SceSdstorPartitionEntry *entry;
	SdifContextPart *context;
	unsigned int index;

	SCE_DBG_ASSERT(port <= 4, 733);
	SCE_DBG_ASSERT(table != NULL, 734);
#if FW_DEBUG
	SCE_DBG_ASSERT(device != NULL, 735);
#endif
	if (mediaType != 1) {
		const uint8_t *signature;

		mbr = table->mbr_ptr;
		SCE_DBG_ASSERT_FUNC(mbr != NULL, 1334, "sdpart_valid_external_mbr");
		signature = (const uint8_t *)&mbr->signature;
		if (signature[0] != 0x55 || signature[1] != 0xaa)
			return 0;
		if (mbr->bootCode[0] == 0xeb) {
			if (mbr->bootCode[2] == 0x90)
				return 0;
		} else if (mbr->bootCode[0] == 0xe9) {
			return 0;
		}
	} else {
#if FW_DEBUG
		context = SceSdifForDriver_B9EA5B1E(device->hport);
#else
		context = SceSdifForDriver_B9EA5B1E(*hport);
#endif
		if (!context) {
			SCE_DBG_PRINTF_LEVEL(2, 6, 745, "Card not found\n");
			return 0x80010013;
		}
		if (sdpart_valid_sd_mbr(table->mbr_ptr,
				      context->parameters.capacity >> 9) != 0)
			return 0;
	}
	mbr = table->mbr_ptr;
	partition = mbr->partitions[0];
	entry = &table->pentry[0];
	entry->sdpart = table;
	entry->partition_entry = &mbr->partitions[0];
	__builtin_memcpy(&entry->start_lba, partition.startLba, sizeof(entry->start_lba));
	__builtin_memcpy(&entry->n_sectors, partition.sectorCount, sizeof(entry->n_sectors));
	snprintf(entry->numeric_name, sizeof(entry->numeric_name), "%01d%02d", port, 0);
	entry->mode = sdpartDefaultAccess.mode;
	entry->flags = sdpartDefaultAccess.flags;
	for (index = 1; index != 16; ++index)
		memset(&table->pentry[index], 0, sizeof(table->pentry[index]));
	table->externalFormat = 1;
	return 0;
}

int sdpart_write_sd_mbr(unsigned int port)
{
	SceSdstorSdpart *table;
	SceSdstorSddev *device;
	SdifContextPart *context;
	SceSdstorMbr *mbr;
	SceSdstorMbrPartition *partition;
	uint32_t capacity, start, length;
	uint32_t sectorsPerCluster, bufferUnit, candidate, reserved, remaining;
	uint32_t clusters, fatSectors, bufferMultiple;
	int fat16, result;

	SCE_DBG_ASSERT(port <= 4, 1250);
	SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
	table = port <= 4 ? &sdpartTables[port] : NULL;
	device = sddev_get_object(port);
	if (!device) {
		SCE_DBG_PRINTF(6, 1255, "no sddev\n");
		return 0x80010013;
	}
	context = SceSdifForDriver_B9EA5B1E(device->hport);
	if (!context) {
		SCE_DBG_PRINTF(6, 1261, "no card\n");
		return 0x80010013;
	}
	mbr = table->mbr_ptr;
	capacity = context->parameters.capacity >> 9;
	memset(mbr, 0, sizeof(*mbr));
	mbr->signature = 0xaa55;
	if (capacity >= 0x400000) {
		if (capacity == 0x400000)
			start = 128;
		else if (capacity <= 0x4000000)
			start = 0x2000;
		else if (capacity <= 0x10000000)
			start = 0x8000;
		else if (capacity <= 0x40000000)
			start = 0x10000;
		else
			start = 0x20000;
	} else {
		if (capacity <= 0x4000) {
			sectorsPerCluster = 16;
			bufferUnit = 16;
		} else if (capacity <= 0x20000) {
			sectorsPerCluster = 32;
			bufferUnit = 32;
		} else if (capacity <= 0x200000) {
			sectorsPerCluster = 32;
			bufferUnit = capacity <= 0x80000 ? 64 : 128;
		} else {
			sectorsPerCluster = 64;
			bufferUnit = 128;
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 6, 1126, "sdpart_calc_sd_fat1612_start",
			"sec_per_clus=%d(%#x) bu_size=%d(%#x)\n",
			sectorsPerCluster, sectorsPerCluster, bufferUnit, bufferUnit);
		start = bufferUnit;
		if (capacity > bufferUnit) {
			candidate = 2 * bufferUnit;
			remaining = capacity - candidate;
			reserved = bufferUnit;
			bufferMultiple = 2;
			fat16 = capacity >= 0x20000;
			for (;;) {
				clusters = remaining / sectorsPerCluster;
				if (fat16)
					fatSectors = (2 * clusters + 511) >> 9;
				else
					fatSectors = (((3 * clusters + 1) >> 1) + 511) >> 9;
				remaining -= bufferUnit;
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 6, 1102, "sdpart_calc_fat_sectors",
					"fs=%d cluster_count=%d(%#x) fat size=%d(%#x)\n",
					fat16, clusters, clusters, fatSectors, fatSectors);
				SCE_DBG_PRINTF_LEVEL_FUNC(2, 6, 1135, "sdpart_calc_sd_fat1612_start",
					"bu_mult=%d clusters=%d(%#x) fat_size=%d(%#x)\n",
					bufferMultiple++, clusters, clusters, fatSectors, fatSectors);
				if (2 * fatSectors + 33 <= reserved) {
					start = candidate - 33 - 2 * fatSectors;
					break;
				}
				reserved = candidate;
				if (capacity <= candidate)
					break;
				candidate += bufferUnit;
			}
		}
		SCE_DBG_PRINTF_LEVEL_FUNC(2, 6, 1142, "sdpart_calc_sd_fat1612_start",
					 "partition start sec=%d(%#x)\n", start, start);
	}
	partition = &mbr->partitions[0];
	length = capacity - start;
	partition->startLba[0] = start;
	partition->sectorCount[0] = length;
	partition->sectorCount[1] = length >> 8;
	partition->sectorCount[2] = length >> 16;
	partition->sectorCount[3] = length >> 24;
	partition->startLba[1] = start >> 8;
	partition->startLba[2] = start >> 16;
	partition->startLba[3] = start >> 24;
	if (start <= 0xfb03ff) {
		sdpart_transfer_lba_to_chs(capacity, start, partition->startChs);
	} else {
		partition->startChs[0] = 0xfe;
		partition->startChs[2] = 0xff;
		partition->startChs[1] = 0xff;
	}
	if (capacity - 1 <= 0xfb03ff) {
		sdpart_transfer_lba_to_chs(capacity, capacity - 1, partition->endChs);
	} else {
		partition->endChs[0] = 0xfe;
		partition->endChs[2] = 0xff;
		partition->endChs[1] = 0xff;
	}
	if (capacity >= 0x400000) {
		if (capacity < 0x4000000)
			partition->systemId = capacity - 1 <= 0xfb03ff ? 11 : 12;
		else
			partition->systemId = 7;
	} else if (capacity > 0x7fa7) {
		partition->systemId = capacity < 0x10000 ? 4 : 6;
	} else {
		partition->systemId = 1;
	}
	device = sddev_get_object(port);
	if (!device) {
		SCE_DBG_PRINTF(6, 1306, "no sddev\n");
		return 0x80010013;
	}
	result = sddev_power_on(device);
	if (result != 0) {
		SCE_DBG_PRINTF(6, 1312, "sddev open fail %#x\n", result);
		return result;
	}
	result = sddev_write(device, mbr, 0, 1);
	if (result != 0) {
		SCE_DBG_PRINTF(6, 1318, "sddev write fail %#x\n", result);
		return result;
	}
	result = sddev_close(device);
	if (result != 0)
		SCE_DBG_PRINTF(6, 1324, "sddev write fail %#x\n", result);
	return result;
}

int sdpart_valid_internal_mbr(const SceMbr *mbr)
{
	int result;

	SCE_DBG_ASSERT(mbr != NULL, 1350);
	result = strncmp(mbr->magic, SCE_MBR_MAGIC, sizeof(mbr->magic));
	if (result != 0)
		return 0x8001002f;
	if (mbr->signature != 0xaa55)
		return 0x8001002f;
	return result;
}

unsigned int sdpart_open(unsigned int port)
{
	SceSdstorSdpart *table;
	SceSdstorSddev *device;
	SceSdstorMediaInfo info;
	SceMbr *mbr;
	unsigned int index, count;
	int result, unlockResult;

	SCE_DBG_ASSERT(port <= 4, 291);
	SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
	table = port <= 4 ? &sdpartTables[port] : NULL;
	result = lockFastMutex(&table->fastmutex);
	if (result < 0)
		return result;
	if (table->stat == 2) {
		result = 0x80010013;
		goto unlock;
	}
	if (table->stat == 1) {
		result = 0;
		goto unlock;
	}
	SCE_DBG_ASSERT_FUNC(port <= 4, 631, "sdpart_read");
	if (port > 4) {
		result = 0x80010013;
		goto unlock;
	}
	SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
	device = sddev_get_object(port);
	if (!device) {
		result = 0x80010013;
		goto unlock;
	}
	result = sddev_power_on(device);
	if (result != 0)
		goto unlock;
	result = sddev_read(device, table->mbr_ptr, 0, 1);
	if (result != 0)
		goto unlock;
	result = sddev_get_info(device, &info);
	if (result != 0)
		goto unlock;
	table->externalFormat = 0;
	table->sddev = device;
	table->stat = 1;
	table->blockSize = info.blockSize;
	sddev_get_info(device, &table->mediaInfo);
	mbr = table->mbr_ptr;
	if (port == 0) {
		sdpart_init_entire_entry(port, table, info.sectorCount,
					 &sdpartInternalAccess);
		result = sdpart_parse_intpar(port, table, device);
	} else if (port == 1) {
		result = SceSysrootForDriver_F804F761();
		if (result != 0) {
			if (SceSysrootForDriver_F804F761() != 0)
				goto sd;
			result = 0x80010013;
			goto unlock;
		}
		sdpart_init_entire_entry(port, table, info.sectorCount,
					 &sdpartGameCardAccess);
		if (sdpart_valid_internal_mbr(mbr) == 0 &&
		    sdpart_parse_intpar(port, table, device) == 0) {
			SCE_DBG_ASSERT_FUNC(table != NULL, 854,
					    "sdpart_get_valid_internal_entry_count");
			count = 0;
			for (index = 0; index != 16; ++index)
				if (table->pentry[index].sdpart &&
				    table->pentry[index].partition_entry)
					++count;
			if (count != 0)
				goto unlock;
		}
		if (!SceSysrootForDriver_55392965())
			goto unlock;
		sdpart_init_entire_entry(port, table, info.sectorCount,
					 &sdpartDefaultAccess);
#if FW_DEBUG
		sdpart_parse_extpar(port, table, device, 0);
#else
		sdpart_parse_extpar(port, table, &device->hport, 0);
#endif
	} else if (port == 2) {
		sdpart_init_entire_entry(port, table, info.sectorCount,
					 &sdpartSdAccess);
		if (sdpart_valid_internal_mbr(mbr) == 0)
			result = sdpart_parse_intpar(port, table, device);
	} else if (port == 3) {
		sdpart_init_entire_entry(port, table, info.sectorCount,
					 &sdpartUsbAccess);
#if FW_DEBUG
		sdpart_parse_extpar(port, table, device, 0);
#else
		sdpart_parse_extpar(port, table, &device->hport, 0);
#endif
	} else {
sd:
		sdpart_init_entire_entry(port, table, info.sectorCount,
					 &sdpartSdAccess);
#if FW_DEBUG
		sdpart_parse_extpar(port, table, device, 1);
#else
		sdpart_parse_extpar(port, table, &device->hport, 1);
#endif
		result = 0;
	}
unlock:
	unlockResult = sceKernelUnlockFastMutex(&table->fastmutex);
	return unlockResult < 0 ? unlockResult : result;
}

SceSdstorPartitionEntry *sdpart_get_object(const char *name, int length)
{
	SceSdstorSdinst sdinst;
	SceSdstorSdpart *table;
	SceSdstorPartitionEntry *entry;
	SceMbrPartitionEntry *partition;
	SceSdstorSddev *device;
	uint8_t portsWithSd[3], portsWithoutSd[2];
	const uint8_t *ports;
	uint8_t port, index;
	int number, partitionIndex;

	SCE_DBG_ASSERT(name != NULL, 404);
	SCE_DBG_ASSERT_FUNC(name != NULL, 611, "sdpart_is_internal_format");
	if (length == 3 && (look_ctype_table(name[0]) & 4) &&
	    (look_ctype_table(name[1]) & 4) && (look_ctype_table(name[2]) & 4)) {
		SCE_DBG_ASSERT_FUNC(name != NULL, 560, "sdpart_find_by_str");
		number = strtol(name, NULL, 10);
		if (number < 0)
			return NULL;
		port = number / 100;
		partitionIndex = number % 100;
		if (port > 4 || (uint8_t)partitionIndex > 16)
			return NULL;
		if ((int)sdpart_open(port) < 0)
			return NULL;
		SCE_DBG_ASSERT_FUNC(port <= 4, 192, "sdpart_get_table");
		table = &sdpartTables[port];
		if (lockFastMutex(&table->fastmutex) < 0)
			return NULL;
		entry = NULL;
		if (table->stat == 1) {
			entry = &table->pentry[partitionIndex];
			if (entry->sdpart && strncmp(entry->numeric_name, name, 3))
				entry = NULL;
		}
		if (sceKernelUnlockFastMutex(&table->fastmutex) < 0)
			return NULL;
		return entry;
	}
	if (sdpart_decode_name(name, length, &sdinst) < 0)
		return NULL;
	if (sdinst.port == 255) {
		SCE_DBG_PRINTF_LEVEL(2, 0, 422,
			"%s: psudo MC port=%d kind=%d id=%d stat=%d\n",
			"sdpart_get_object", sdinst.port, sdinst.kind,
			sdinst.id, sdinst.stat);
		if (!SceSysrootForDriver_55392965() &&
		    SceSysrootForDriver_50FE3B4D())
			goto resolveXmc;
		port = sdpartMcPort;
		if (port == 5) {
			portsWithSd[0] = 2;
			portsWithSd[1] = 4;
			portsWithSd[2] = 5;
			portsWithoutSd[0] = 2;
			portsWithoutSd[1] = 5;
			port = 2;
			if (SceSysrootForDriver_55392965()) {
				ports = SceSysrootForDriver_8AA268D6(11) ?
					portsWithSd : portsWithoutSd;
				for (; *ports != 5; ++ports) {
					device = sddev_get_object(*ports);
					if (device && sddev_power_on(device) >= 0) {
						port = *ports;
						break;
					}
				}
			}
		}
		if (port != 0)
			goto resolved;
resolveXmc:
		port = sdpartXmcPort;
		if (port == 5) {
			portsWithSd[0] = 2;
			portsWithSd[1] = 4;
			portsWithSd[2] = 5;
			portsWithoutSd[0] = 2;
			portsWithoutSd[1] = 5;
			port = 2;
			if (SceSysrootForDriver_55392965()) {
				ports = SceSysrootForDriver_8AA268D6(11) ?
					portsWithSd : portsWithoutSd;
				for (; *ports != 5; ++ports) {
					device = sddev_get_object(*ports);
					if (device && sddev_power_on(device) >= 0) {
						port = *ports;
						break;
					}
				}
			}
		}
resolved:
		sdinst.port = port;
		if (sdinst.id == 8 && (port == 4 || port == 1)) {
			sdinst.kind = 1;
			sdinst.id = 0;
		}
	}
	SCE_DBG_ASSERT_FUNC(&sdinst != NULL, 449, "sdpart_find_by_sdinst");
	sdpart_trace_sdinst(&sdinst);
	if ((int)sdpart_open(sdinst.port) < 0)
		return NULL;
	SCE_DBG_ASSERT_FUNC(sdinst.port <= 4, 192, "sdpart_get_table");
	table = sdinst.port <= 4 ? &sdpartTables[sdinst.port] : NULL;
	if (lockFastMutex(&table->fastmutex) < 0)
		return NULL;
	entry = NULL;
	if (table->stat != 1)
		goto unlock;
	if (table->externalFormat == 1) {
		SCE_DBG_ASSERT_FUNC(&sdinst != NULL, 487, "sdpart_find_extpar");
		SCE_DBG_ASSERT_FUNC(sdinst.port <= 4, 192, "sdpart_get_table");
		if (sdinst.kind == 1 && sdinst.id != 17)
			entry = &table->pentry[sdinst.id];
		else if (sdinst.kind == 0 && sdinst.id == 15)
			entry = &table->pentry[16];
		if (entry && !entry->sdpart)
			entry = NULL;
	} else if (table->externalFormat == 0) {
		SCE_DBG_ASSERT_FUNC(&sdinst != NULL, 516, "sdpart_find_intpar");
		SCE_DBG_ASSERT_FUNC(sdinst.port <= 4, 192, "sdpart_get_table");
		if (sdinst.kind == 1 && sdinst.id != 17) {
			entry = &table->pentry[sdinst.id];
			if (!entry->sdpart)
				entry = NULL;
		} else if (sdinst.kind == 0 && sdinst.id == 15) {
			entry = &table->pentry[16];
			if (!entry->sdpart)
				entry = NULL;
		} else {
			for (index = 0; index != 16; ++index) {
				entry = &table->pentry[index];
				partition = entry->partition_entry;
				if (entry->sdpart && partition &&
				    sdinst.id == sdpart_partition_code_to_id(partition->code) &&
				    (sdinst.stat == 2 || partition->active == sdinst.stat))
					goto unlock;
			}
			entry = NULL;
		}
	}
unlock:
	if (sceKernelUnlockFastMutex(&table->fastmutex) < 0)
		return NULL;
	return entry;
}

unsigned int sdpart_grab_mc(unsigned int port)
{
	uint8_t portsWithSd[3] = { 2, 4, 5 };
	uint8_t portsWithoutSd[2] = { 2, 5 };
	const uint8_t *ports;
	SceSdstorSddev *device;
	int manufacturing;

	SCE_DBG_ASSERT(port <= 4, 1369);
	if (port != 0 && port != 3 &&
	    (port != 1 || SceSysrootForDriver_F804F761() != 0)) {
		port = (signed char)port;
		if (SceCpuForDriver_3627F4E0((SceInt8 *)&sdpartMcPort,
						 5, port) == 5)
			return port;
	}
	manufacturing = SceSysrootForDriver_55392965();
	if (!manufacturing && SceSysrootForDriver_50FE3B4D())
		return manufacturing;
	port = sdpartMcPort;
	if (port != 5)
		return port;
	if (!SceSysrootForDriver_55392965())
		return 2;
	ports = SceSysrootForDriver_8AA268D6(11) ?
		portsWithSd : portsWithoutSd;
	for (port = *ports; port != 5; port = *++ports) {
		device = sddev_get_object(port);
		if (device && sddev_power_on(device) >= 0)
			return port;
	}
	return 2;
}

int sdpart_release_mc(unsigned int port)
{
	SCE_DBG_ASSERT(port <= 4, 1394);
	if (port == 0 || port == 3 ||
	    (port == 1 && SceSysrootForDriver_F804F761() == 0))
		return 0;
	return SceCpuForDriver_3627F4E0((SceInt8 *)&sdpartMcPort,
					    (signed char)port, 5);
}

unsigned int sdpart_get_mc_port(void)
{
	const uint8_t *ports;
	SceSdstorSddev *device;
	unsigned int port;
	int manufacturing;

	manufacturing = SceSysrootForDriver_55392965();
	if (!manufacturing && SceSysrootForDriver_50FE3B4D())
		return manufacturing;
	port = sdpartMcPort;
	if (port != 5)
		return port;
	uint8_t portsWithSd[3] = { 2, 4, 5 };
	uint8_t portsWithoutSd[2] = { 2, 5 };
	if (!SceSysrootForDriver_55392965())
		return 2;
	ports = SceSysrootForDriver_8AA268D6(11) ?
		portsWithSd : portsWithoutSd;
	for (port = *ports; port != 5; port = *++ports) {
		device = sddev_get_object(port);
		if (device && sddev_power_on(device) >= 0)
			return port;
	}
	return 2;
}

int sdpart_grab_xmc(unsigned int port)
{
	uint8_t portsWithSd[3], portsWithoutSd[2];
	const uint8_t *ports;
	SceSdstorSddev *device;

	SCE_DBG_ASSERT(port <= 4, 1476);
	if (port != 0 && port != 3 &&
	    (port != 1 || SceSysrootForDriver_F804F761() != 0)) {
		port = (signed char)port;
		if (SceCpuForDriver_3627F4E0((SceInt8 *)&sdpartXmcPort,
						 5, port) == 5)
			return port;
	}
	port = sdpartXmcPort;
	if (port != 5)
		return port;
	portsWithSd[0] = 2;
	portsWithSd[1] = 4;
	portsWithSd[2] = 5;
	portsWithoutSd[0] = 2;
	portsWithoutSd[1] = 5;
	if (!SceSysrootForDriver_55392965())
		return 2;
	ports = SceSysrootForDriver_8AA268D6(11) ?
		portsWithSd : portsWithoutSd;
	for (port = *ports; port != 5; port = *++ports) {
		device = sddev_get_object(port);
		if (device && sddev_power_on(device) >= 0)
			return port;
	}
	return 2;
}

int sdpart_release_xmc(unsigned int port)
{
	SCE_DBG_ASSERT(port <= 4, 1501);
	if (port == 0 || port == 3 ||
	    (port == 1 && SceSysrootForDriver_F804F761() == 0))
		return 0;
	return SceCpuForDriver_3627F4E0((SceInt8 *)&sdpartXmcPort,
					    (signed char)port, 5);
}

unsigned int sdpart_get_xmc_port(void)
{
	const uint8_t *ports;
	SceSdstorSddev *device;
	unsigned int port;

	port = sdpartXmcPort;
	if (port != 5)
		return port;
	uint8_t portsWithSd[3] = { 2, 4, 5 };
	uint8_t portsWithoutSd[2] = { 2, 5 };
	if (!SceSysrootForDriver_55392965())
		return 2;
	ports = SceSysrootForDriver_8AA268D6(11) ?
		portsWithSd : portsWithoutSd;
	for (port = *ports; port != 5; port = *++ports) {
		device = sddev_get_object(port);
		if (device && sddev_power_on(device) >= 0)
			return port;
	}
	return 2;
}

int sdpart_decode_name(const char *name, unsigned int length,
		       SceSdstorSdinst *sdinst)
{
	uint8_t indices[4] = { 7, 2, 0, 2 };
	const SceSdstorNameComponent *component = sdpartNameComponents;
	const char *remaining = name;
	unsigned int offset = 0, group = 0;
	unsigned int tokenLength;
	int index;

	if (!sdinst || !name || length > 20)
		return 0x80010013;
	for (;;) {
		index = 0;
		if (component->count > 0) {
			for (;;) {
				tokenLength = strnlen(component->names[index], 20 - offset);
				if (strncmp(remaining, component->names[index], tokenLength) == 0) {
					if (remaining[tokenLength - 1] == '-') {
						offset += tokenLength;
						break;
					}
					if (tokenLength + offset == length) {
						offset = length;
						break;
					}
				}
				if (++index == component->count)
					return 0x80010013;
			}
		} else if (component->count == 0) {
			return 0x80010013;
		}
		indices[group] = index;
		if (group == 3)
			break;
		++component;
		remaining = name + offset;
		if (group == 2 && indices[1] == 1)
			goto physical;
		++group;
	}
	if (indices[1] == 1) {
physical:
		if (strnlen(remaining, 20 - offset) > 1 ||
		    (uint8_t)(*remaining - 'a') > 15)
			return 0x80010013;
		++offset;
		indices[3] = *remaining - 'a';
	}
	if (length > offset)
		return 0x80010013;
	sdinst->kind = indices[1];
	sdinst->id = indices[3];
	sdinst->stat = indices[2];
	sdinst->port = sdpartNamePorts[indices[0]];
	return 0;
}

unsigned int sdpart_partition_code_to_id(unsigned int code)
{
	if (code == 0)
		return 0;
	if (code == 1)
		return 1;
	if (code == 2)
		return 2;
	if (code == 3)
		return 3;
	if (code == 4)
		return 4;
	if (code == 5)
		return 5;
	if (code == 6)
		return 6;
	if (code == 7)
		return 7;
	if (code == 8)
		return 8;
	if (code == 9)
		return 9;
	if (code == 10)
		return 10;
	if (code == 11)
		return 11;
	if (code == 12)
		return 12;
	if (code == 13)
		return 13;
	if (code == 14)
		return 14;
	if (code == 255)
		return 15;
	return 0;
}

void sdpart_trace_sdinst(const SceSdstorSdinst *sdinst)
{
	(void)sdinst;
}
