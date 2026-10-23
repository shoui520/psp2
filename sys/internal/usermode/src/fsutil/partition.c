/* Storage partition selection and formatting. */

#include <sysmem/debug.h>
#include <sdstor/mbr.h>
#include <sdstor/sdstor.h>
#include <iofilemgr/iofilemgr.h>
#include <stdint.h>
#include <string.h>
#include "partition.h"

/* Sony's debug records retain the source path rather than its basename. */
#undef __FILE_NAME__
#define __FILE_NAME__ "../../../usermode/src/fsutil/partition.c"

typedef struct AppmgrPartitionConf {
	uint8_t code;
	uint8_t type;
	uint8_t reserved[2];
	uint32_t clusterSize;
	uint32_t reservedTail;
} AppmgrPartitionConf;

static const uint32_t partitionFormatTypes[14] = {
	1, 0, 0, 2, 0, 2, 4, 0, 0, 0, 3, 3, 0, 2,
};

static const AppmgrPartitionConf partitionConfiguration[3] = {
	{ .code = 9, .type = 7, .clusterSize = 0x8000 },
	{ .code = 10, .type = 7, .clusterSize = 0x8000 },
	{ .code = 13, .type = 0xda },
};

_Static_assert(sizeof(AppmgrPartitionConf) == 12, "AppmgrPartitionConf");

/* Each configuration contains sixteen SCE partition entries. */
static const SceMbrPartitionEntry gameCardPartitionTables[8][16] = {
	{
		{ 0x40, 0x34ffc0, 9, 7, 1, 0xfff, 0 },
		{ 0x350000, 0x30000, 10, 7, 1, 0xfff, 0 },
		{ 0x380000, 0x8000, 13, 218, 1, 0xfff, 0 },
	},
	{
		{ 0x40, 0x2effc0, 9, 7, 1, 0xfff, 0 },
		{ 0x2f0000, 0x90000, 10, 7, 1, 0xfff, 0 },
		{ 0x380000, 0x8000, 13, 218, 1, 0xfff, 0 },
	},
	{
		{ 0x40, 0x28ffc0, 9, 7, 1, 0xfff, 0 },
		{ 0x290000, 0xf0000, 10, 7, 1, 0xfff, 0 },
		{ 0x380000, 0x8000, 13, 218, 1, 0xfff, 0 },
	},
	{
		{ 0x40, 0x37ffc0, 9, 7, 1, 0xfff, 0 },
		{ 0x380000, 0x8000, 13, 218, 1, 0xfff, 0 },
	},
	{
		{ 0x40, 0x69ffc0, 9, 7, 1, 0xfff, 0 },
		{ 0x6a0000, 0x60000, 10, 7, 1, 0xfff, 0 },
		{ 0x700000, 0x10000, 13, 218, 1, 0xfff, 0 },
	},
	{
		{ 0x40, 0x5dffc0, 9, 7, 1, 0xfff, 0 },
		{ 0x5e0000, 0x120000, 10, 7, 1, 0xfff, 0 },
		{ 0x700000, 0x10000, 13, 218, 1, 0xfff, 0 },
	},
	{
		{ 0x40, 0x51ffc0, 9, 7, 1, 0xfff, 0 },
		{ 0x520000, 0x1e0000, 10, 7, 1, 0xfff, 0 },
		{ 0x700000, 0x10000, 13, 218, 1, 0xfff, 0 },
	},
	{
		{ 0x40, 0x6fffc0, 9, 7, 1, 0xfff, 0 },
		{ 0x700000, 0x10000, 13, 218, 1, 0xfff, 0 },
	},
};

static const SceMbrPartitionEntry removablePartitionTables[4][16] = {
	{
		{ 0x8000, 0x8000, 13, 218, 1, 0xfff, 0 },
		{ 0x10000, 0, 8, 7, 1, 0xfff, 0 },
	},
	{
		{ 0x8000, 0x8000, 13, 218, 1, 0xfff, 0 },
		{ 0x10000, 0, 8, 7, 1, 0xfff, 0 },
	},
	{
		{ 0x8000, 0x8000, 13, 218, 1, 0xfff, 0 },
		{ 0x10000, 0, 8, 7, 1, 0xfff, 0 },
	},
	{
		{ 0x8000, 0x8000, 13, 218, 1, 0xfff, 0 },
		{ 0x10000, 0, 8, 7, 1, 0xfff, 0 },
	},
};

int _select_partition_table(int id, uint64_t volumeSize,
			    const uint32_t *sectorCount,
			    const SceMbrPartitionEntry *table,
			    unsigned int defaultTableSize)
{
	uint64_t volumeSectors = volumeSize >> 9;
	uint64_t selectedSize = 0;
	int selected = (int)0x80010016;
	int configuration = 0;
	unsigned int remaining;
	const SceMbrPartitionEntry *end;
	int found;

	if (id != 2 && id != 3)
		SCE_DBG_PRINTF_LEVEL(1, 0, 265,
			"%s: unsupported id(%d), default_table_size(%u), configuration_count(%u)\n",
			__func__, id, defaultTableSize, defaultTableSize / 272);
	for (remaining = defaultTableSize; remaining >= 272; remaining -= 272) {
		found = 0;
		for (end = table + 16; table != end; table++) {
			if (id == 2) {
				if (table->code != 9)
					continue;
				if (volumeSize != 0) {
					if (table->size == volumeSectors) {
						selected = configuration;
						found = 1;
					}
				} else if (table->offset + table->size <= *sectorCount) {
					selected = configuration;
					found = 1;
				} else {
					SCE_DBG_PRINTF_LEVEL(1, 0, 296,
						"%s: request is any configuration but device size is too small, necessary lba[%#x] > sectorCount[%#x]\n",
						__func__, table->offset + table->size,
						*sectorCount);
				}
			} else if (id == 3 && table->code == 8) {
				if (table->size == 0) {
					selected = configuration;
					found = 1;
				} else if (table->offset + table->size <= *sectorCount &&
					   (volumeSize == 0 || volumeSectors >= table->size) &&
					   selectedSize < table->size) {
					selectedSize = table->size;
					selected = configuration;
				}
			}
		}
		configuration++;
		if (found)
			break;
	}
	return selected;
}

/* The device every block device name must start with. */
static const char blockDevicePrefix[] = "sdstor0:";

int _fs_util_get_device_info(const char *blockDeviceName, void *buffer,
			     unsigned int bufferSize)
{
	const char *prefix;
	const char *cursor;
	const char *device;
	unsigned int length;
	char character;

	if (!blockDeviceName || blockDeviceName[0] != 's')
		goto unsupported;
	prefix = blockDevicePrefix;
	cursor = blockDeviceName;
	while (*++prefix) {
		if (*prefix != *++cursor)
			goto unsupported;
	}
	if (bufferSize != 16) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 178,
			"%s: buf_size(%u) != expected(%u)\n",
			__func__, bufferSize, 16);
		return (int)0x80010016;
	}
	device = blockDeviceName + 8;
	length = 1;
	if (device[0] != '\0') {
		for (;;) {
			character = device[length++];
			if (character == '\0')
				break;
			if (length == 32)
				return (int)0x80010016;
		}
	}
	return sceIoDevctl("sdstor0:", 1, (void *)device, length, buffer, 16);
unsupported:
	SCE_DBG_PRINTF_LEVEL(1, 0, 175,
		"%s: unsupported block_device_name(%s)\n", __func__, blockDeviceName);
	return (int)0x80010016;
}

int _fs_util_device_format(const char *blockDeviceName)
{
	const char *prefix;
	const char *cursor;
	const char *device;
	unsigned int length;
	char character;

	if (!blockDeviceName || blockDeviceName[0] != 's')
		goto unsupported;
	prefix = blockDevicePrefix;
	cursor = blockDeviceName;
	while (*++prefix) {
		if (*prefix != *++cursor)
			goto unsupported;
	}
	device = blockDeviceName + 8;
	length = 1;
	if (device[0] != '\0') {
		for (;;) {
			character = device[length++];
			if (character == '\0')
				break;
			if (length == 32)
				return (int)0x80010016;
		}
	}
	return sceIoDevctl("sdstor0:", 2, (void *)device, length, NULL, 0);
unsupported:
	SCE_DBG_PRINTF_LEVEL(1, 0, 196,
		"%s: unsupported block_device_name(%s)\n", __func__, blockDeviceName);
	return (int)0x80010016;
}

uint64_t appmgrMbrGetFdiskOffset(const SceSdstorMbr *mbr, unsigned int index)
{
	const SceSdstorMbrPartition *partition;

	if (mbr == NULL || (index & ~3U) != 0)
		return 0;
	partition = &mbr->partitions[index];
	return (uint32_t)partition->startLba[2] << 16 |
	       (uint32_t)partition->startLba[1] << 8 |
	       partition->startLba[0] | (uint32_t)partition->startLba[3] << 24;
}

uint64_t appmgrMbrGetFdiskSize(const SceSdstorMbr *mbr, unsigned int index)
{
	const uint8_t *size;

	if (mbr == NULL || (index & ~3U) != 0)
		return 0;
	size = mbr->partitions[index].sectorCount;
	return (uint32_t)size[2] << 16 | (uint32_t)size[1] << 8 |
	       size[0] | (uint32_t)size[3] << 24;
}

uint64_t appmgrMbrGetPartitionOffset(const SceMbr *mbr, int index)
{
	int valid = mbr != NULL;

	if (index > 15)
		valid = 0;
	if (valid)
		return mbr->partitions[index].offset;
	return 0;
}

uint64_t appmgrMbrGetPartitionSize(const SceMbr *mbr, int index)
{
	int valid = mbr != NULL;

	if (index > 15)
		valid = 0;
	if (valid)
		return mbr->partitions[index].size;
	return 0;
}

unsigned int appmgrMbrGetFormatType(const SceMbr *mbr, int index)
{
	int valid = mbr != NULL;
	uint8_t type;

	if (index > 15)
		valid = 0;
	if (!valid)
		return 0;
	type = mbr->partitions[index].type - 1;
	if (type > 13)
		return 0;
	return partitionFormatTypes[type];
}

unsigned int appmgrMbrGetClusterSize(const SceMbr *mbr, int index)
{
	int valid = mbr != NULL;
	uint8_t code;
	const AppmgrPartitionConf *configuration;
	const AppmgrPartitionConf *end;

	if (index > 15)
		valid = 0;
	if (!valid)
		return 0;
	code = mbr->partitions[index].code;
	if (code == 8)
		return 0x8000;
	if (code >= 8 && code <= 10) {
		for (configuration = partitionConfiguration, end = configuration + 3;
		     configuration != end; configuration++) {
			if (configuration->code == code)
				return configuration->clusterSize;
		}
	}
	return 0;
}

int _fs_util_prepare_rm_mbr(const char *blockDeviceName, SceUID file,
			    uint64_t volumeSize, unsigned int sectorSize,
			    SceMbr *buffer, unsigned int bufferSize)
{
	SceSdstorPartitionInfo info;
	SceSdstorPartitionInfo externalInfo;
	SceMbr *scratch;
	SceMbrPartitionEntry *partition;
	SceSdstorMbrPartition *fdisk;
	const char *prefix;
	const char *cursor;
	const char *device;
	unsigned int index;
	unsigned int alignment;
	unsigned int offset;
	unsigned int size;
	int id;
	int configuration;
	int count;
	int result;
	unsigned int length;
	char character;

	if (!blockDeviceName || blockDeviceName[0] != 's')
		goto unsupported_device;
	prefix = blockDevicePrefix;
	cursor = blockDeviceName;
	while (*++prefix) {
		if (*prefix != *++cursor)
			goto unsupported_device;
	}
	device = blockDeviceName + 8;
	length = 1;
	if (device[0] != '\0') {
		for (;;) {
			character = device[length++];
			if (character == '\0')
				break;
			if (length == 32)
				return (int)0x80010016;
		}
	}
	result = sceIoDevctl("sdstor0:", 1, (void *)device, length, &info, sizeof(info));
	if (result != 0)
		return result;
	scratch = bufferSize >= 1024 ? buffer + 1 : buffer;
	result = sceIoPread(file, scratch, sizeof(*scratch), 0);
	if (result < 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 783,
			"%s: sceIoPread failed[%#x]\n", __func__, result);
		return result;
	}
	if (scratch->signature != 0xaa55)
		goto invalid;
	for (index = 0; index < sizeof(scratch->magic); ++index) {
		if (scratch->magic[index] != SCE_MBR_MAGIC[index])
			goto invalid;
	}
	if (scratch->version != 3)
		goto invalid;
	for (index = 0; index < sizeof(scratch->reserved1); ++index) {
		if (scratch->reserved1[index] != 0)
			goto invalid;
	}
	if (info.eraseBlockCount % sectorSize != 0 ||
	    scratch->size > info.sectorCount)
		goto invalid;
	alignment = info.eraseBlockCount / sectorSize;
	partition = scratch->partitions;
	do {
		if (partition->size != 0) {
			if ((uint64_t)partition->offset + partition->size > info.sectorCount)
				goto invalid;
			if ((partition->offset + info.startSector) % alignment != 0)
				goto invalid;
		}
		++partition;
	} while (partition != scratch->partitions + 16);
	if (bufferSize >= 1024)
		memcpy(buffer, scratch, sizeof(*buffer));
	return 0;
invalid:
	if (bufferSize >= 1024)
		return 0;
	switch ((signed char)blockDeviceName[8]) {
	case 'e':
		id = 1;
		break;
	case 'g':
		id = 2;
		break;
	case 'i':
		id = 0;
		break;
	case 'x':
		result = _fs_util_get_device_info("sdstor0:xmc-lp-act-entire",
						 &externalInfo, sizeof(externalInfo));
		if (result == 0) {
			id = 3;
		} else {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 231, "_entire_partition_id",
				"%s: block_device(%s) is emulation\n",
				"_entire_partition_id", blockDeviceName);
			id = -1;
		}
		break;
	default:
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 235, "_entire_partition_id",
			"%s: unexpected (%c), block_device_name(%s)\n",
			"_entire_partition_id", (signed char)blockDeviceName[8], blockDeviceName);
		id = -1;
		break;
	}
	configuration = _select_partition_table(id, volumeSize, &info.sectorCount,
						removablePartitionTables[0],
						sizeof(removablePartitionTables));
	if (configuration < 0)
		return configuration;
	memset(buffer, 0, sizeof(*buffer));
	buffer->size = info.sectorCount;
	buffer->version = 3;
	memcpy(buffer->magic, SCE_MBR_MAGIC, sizeof(buffer->magic));
	memcpy(buffer->partitions, removablePartitionTables[configuration],
	       sizeof(buffer->partitions));
	partition = buffer->partitions;
	while (partition != buffer->partitions + 16 && partition->offset != 0) {
		if (partition->size == 0)
			partition->size = info.sectorCount - partition->offset;
		++partition;
	}
	buffer->signature = 0xaa55;
	count = 0;
	partition = buffer->partitions;
	while (partition != buffer->partitions + 16 && partition->offset != 0) {
		if ((unsigned int)(partition->code - 8) <= 2 && partition->type == 7) {
			if (count > 3)
				return (int)0x8001001c;
			fdisk = &buffer->fdiskPartitions[count++];
			fdisk->bootFlag = 0;
			fdisk->systemId = partition->type;
			offset = partition->offset;
			fdisk->startLba[0] = offset;
			fdisk->startLba[1] = offset >> 8;
			fdisk->startLba[2] = offset >> 16;
			fdisk->startLba[3] = offset >> 24;
			size = partition->size;
			fdisk->sectorCount[0] = size;
			fdisk->sectorCount[1] = size >> 8;
			fdisk->sectorCount[2] = size >> 16;
			fdisk->sectorCount[3] = size >> 24;
		}
		++partition;
	}
	return 0;
unsupported_device:
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 175, "_fs_util_get_device_info",
		"%s: unsupported block_device_name(%s)\n",
		"_fs_util_get_device_info", blockDeviceName);
	return (int)0x80010016;
}

int _fs_util_prepare_mbr(const char *blockDeviceName, unsigned char systemId,
			 SceMbr *buffer, unsigned int bufferSize,
			 uint64_t volumeSize)
{
	SceSdstorPartitionInfo info;
	SceSdstorPartitionInfo externalInfo;
	const SceMbrPartitionEntry *table;
	SceMbrPartitionEntry *partition;
	SceSdstorMbrPartition *fdisk;
	const char *prefix;
	const char *cursor;
	const char *device;
	uint64_t necessarySectors;
	unsigned int offset;
	unsigned int size;
	int configuration;
	int count;
	int result;
	int gameCard;
	unsigned int length;
	char character;

	if (!blockDeviceName || blockDeviceName[0] != 's')
		goto unsupported_device;
	prefix = blockDevicePrefix;
	cursor = blockDeviceName;
	while (*++prefix) {
		if (*prefix != *++cursor)
			goto unsupported_device;
	}
	device = blockDeviceName + 8;
	length = 1;
	if (device[0] != '\0') {
		for (;;) {
			character = device[length++];
			if (character == '\0')
				break;
			if (length == 32) {
				result = (int)0x80010016;
				goto device_info_result;
			}
		}
	}
	result = sceIoDevctl("sdstor0:", 1, (void *)device, length, &info, sizeof(info));
device_info_result:
	if (result != 0) {
		SCE_DBG_PRINTF_LEVEL(1, 0, 908,
			"%s: _fs_util_get_device_info for (%s) failed[%#x]\n",
			__func__, blockDeviceName, result);
		return result;
	}
	switch ((signed char)blockDeviceName[8]) {
	case 'e':
		if (volumeSize != 0) {
			necessarySectors = (volumeSize >> 9) + (info.eraseBlockCount >> 9);
			if (info.sectorCount < necessarySectors) {
				SCE_DBG_PRINTF_LEVEL(1, 0, 857,
					"%s: _prepare_mbr for SD failed[%#x], volume_size[%#llx], device_sectorCount[%#x], device_blockSize[%#x], necessary_sectors[%#llx]\n",
					__func__, (int)0x8001001c, volumeSize,
					info.sectorCount, info.eraseBlockCount,
					necessarySectors);
				return (int)0x8001001c;
			}
			info.sectorCount = necessarySectors;
		}
		if (bufferSize != sizeof(*buffer)) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 617, "_prepare_mbr",
				"%s: buf_size(%u) != expected(%u)\n",
				"_prepare_mbr", bufferSize, 512);
			if (volumeSize != 0)
				SCE_DBG_PRINTF_LEVEL(1, 0, 853,
					"%s: _prepare_mbr for SD failed[%#x], volume_size[%#llx], device_sectorCount[%#x], device_blockSize[%#x]\n",
					__func__, (int)0x80010016, volumeSize,
					info.sectorCount, info.eraseBlockCount);
			return (int)0x80010016;
		}
		offset = info.eraseBlockCount >> 9;
		size = info.sectorCount - offset;
		memset(buffer, 0, sizeof(*buffer));
		fdisk = &buffer->fdiskPartitions[0];
		fdisk->systemId = systemId;
		fdisk->startLba[0] = offset;
		fdisk->startLba[1] = offset >> 8;
		fdisk->startLba[2] = offset >> 16;
		fdisk->startLba[3] = 0;
		fdisk->sectorCount[0] = size;
		fdisk->sectorCount[1] = size >> 8;
		fdisk->sectorCount[2] = size >> 16;
		fdisk->sectorCount[3] = size >> 24;
		fdisk->bootFlag = 0;
		buffer->signature = 0xaa55;
		return 0;
	case 'g':
		gameCard = 1;
		configuration = _select_partition_table(2, volumeSize, &info.sectorCount,
							gameCardPartitionTables[0],
							sizeof(gameCardPartitionTables));
		if (configuration < 0) {
			SCE_DBG_PRINTF_LEVEL(1, 0, 868,
				"%s: _select_partition_table for game card failed[%#x], volume_size[%#llx], device_sectorCount[%#x], device_blockSize[%#x]\n",
				__func__, configuration, volumeSize,
				info.sectorCount, info.eraseBlockCount);
			return configuration;
		}
		table = gameCardPartitionTables[configuration];
		break;
	case 'i':
		return (int)0x80010086;
	case 'x':
		result = _fs_util_get_device_info("sdstor0:xmc-lp-act-entire",
						 &externalInfo, sizeof(externalInfo));
		if (result != 0) {
			SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 231, "_entire_partition_id",
				"%s: block_device(%s) is emulation\n",
				"_entire_partition_id", blockDeviceName);
			goto invalid_id;
		}
		gameCard = 0;
		configuration = _select_partition_table(3, volumeSize, &info.sectorCount,
							removablePartitionTables[0],
							sizeof(removablePartitionTables));
		if (configuration < 0)
			return configuration;
		table = removablePartitionTables[configuration];
		break;
	default:
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 235, "_entire_partition_id",
			"%s: unexpected (%c), block_device_name(%s)\n",
			"_entire_partition_id", (signed char)blockDeviceName[8], blockDeviceName);
invalid_id:
		SCE_DBG_PRINTF_LEVEL(1, 0, 890,
			"%s: invalid id(%d)\n", __func__, -1);
		return (int)0x80010016;
	}
	if (bufferSize != sizeof(*buffer)) {
		SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 641, "_prepare_ngp_mbr",
			"%s: buf_size(%u) != expected(%u)\n",
			"_prepare_ngp_mbr", bufferSize, 512);
		if (gameCard)
			SCE_DBG_PRINTF_LEVEL(1, 0, 872,
				"%s: _prepare_ngp_mbr for game card failed[%#x], volume_size[%#llx], device_sectorCount[%#x], device_blockSize[%#x]\n",
				__func__, (int)0x80010016, volumeSize,
				info.sectorCount, info.eraseBlockCount);
		return (int)0x80010016;
	}
	memset(buffer, 0, sizeof(*buffer));
	buffer->version = 3;
	memcpy(buffer->magic, SCE_MBR_MAGIC, sizeof(buffer->magic));
	buffer->size = info.sectorCount;
	memcpy(buffer->partitions, table, sizeof(buffer->partitions));
	partition = buffer->partitions;
	while (partition != buffer->partitions + 16 && partition->offset != 0) {
		if (partition->size == 0)
			partition->size = info.sectorCount - partition->offset;
		++partition;
	}
	buffer->signature = 0xaa55;
	count = 0;
	partition = buffer->partitions;
	while (partition != buffer->partitions + 16 && partition->offset != 0) {
		if ((unsigned int)(partition->code - 8) <= 2 && partition->type == 7) {
			if (count > 3) {
				if (gameCard)
					SCE_DBG_PRINTF_LEVEL(1, 0, 878,
						"%s: _set_fdisk_partitions for game card failed[%#x], volume_size[%#llx], device_sectorCount[%#x], device_blockSize[%#x]\n",
						__func__, (int)0x8001001c, volumeSize,
						info.sectorCount, info.eraseBlockCount);
				return (int)0x8001001c;
			}
			fdisk = &buffer->fdiskPartitions[count++];
			fdisk->bootFlag = 0;
			fdisk->systemId = partition->type;
			offset = partition->offset;
			fdisk->startLba[0] = offset;
			fdisk->startLba[1] = offset >> 8;
			fdisk->startLba[2] = offset >> 16;
			fdisk->startLba[3] = offset >> 24;
			size = partition->size;
			fdisk->sectorCount[0] = size;
			fdisk->sectorCount[1] = size >> 8;
			fdisk->sectorCount[2] = size >> 16;
			fdisk->sectorCount[3] = size >> 24;
		}
		++partition;
	}
	return 0;
unsupported_device:
	SCE_DBG_PRINTF_LEVEL_FUNC(1, 0, 175, "_fs_util_get_device_info",
		"%s: unsupported block_device_name(%s)\n",
		"_fs_util_get_device_info", blockDeviceName);
	result = (int)0x80010016;
	goto device_info_result;
}
