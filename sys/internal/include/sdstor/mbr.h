/*
 * The SCE master block: sector 0 of the internal storage ("sdstor0:int-lp-act-
 * entire"). Field meanings from psdevwiki's Partitions page; Sony's names are
 * unknown. Owned by SceSdstor; SceIdStorage reads its partition table.
 */
#pragma once

#include <scetypes.h>
#include <stddef.h>

#define SCE_MBR_MAGIC		"Sony Computer Entertainment Inc."

/* partition codes */
#define SCE_MBR_PARTITION_IDSTORAGE	1

typedef struct __attribute__((packed)) SceMbrPartitionEntry {
	SceUInt32 offset;		/* in sectors */
	SceUInt32 size;			/* in sectors */
	SceUInt8 code;
	SceUInt8 type;
	SceUInt8 active;
	SceUInt32 flags;		/* the filesystem owner of the partition */
	SceUInt16 mode;			/* its access mode */
} SceMbrPartitionEntry;

/* Standard disk partition records retained in the SCE master block. */
typedef struct SceSdstorMbrPartition {
	SceUInt8 bootFlag;
	SceUInt8 startChs[3];
	SceUInt8 systemId;
	SceUInt8 endChs[3];
	SceUInt8 startLba[4];
	SceUInt8 sectorCount[4];
} SceSdstorMbrPartition;

typedef struct SceSdstorMbr {
	SceUInt8 bootCode[446];
	SceSdstorMbrPartition partitions[4];
	SceUInt16 signature;
} SceSdstorMbr;

typedef struct SceMbr {
	char magic[0x20];
	SceUInt32 version;
	SceUInt32 size;			/* in sectors */
	SceUInt8 reserved0[8];
	SceUInt32 loader_start;
	SceUInt32 loader_count;
	SceUInt32 current_bl_lba;
	SceUInt32 bl_bank0_lba;
	SceUInt32 bl_bank1_lba;
	SceUInt32 current_os_lba;
	SceUInt8 reservedBoot[8];
	SceMbrPartitionEntry partitions[16];
	SceUInt8 reserved1[94];
	SceSdstorMbrPartition fdiskPartitions[4];
	SceUInt16 signature;		/* 0xaa55 */
} SceMbr;

_Static_assert(sizeof(SceMbrPartitionEntry) == 17, "SceMbrPartitionEntry");
_Static_assert(sizeof(SceSdstorMbrPartition) == 16, "SceSdstorMbrPartition");
_Static_assert(sizeof(SceSdstorMbr) == 512, "SceSdstorMbr");
_Static_assert(sizeof(SceMbr) == 0x200, "SceMbr");
_Static_assert(offsetof(SceMbr, fdiskPartitions) == 446 &&
	       offsetof(SceMbr, signature) == 510,
	       "SceMbr standard disk partition layout");
_Static_assert(offsetof(SceMbr, loader_start) == 0x30 &&
	       offsetof(SceMbr, current_os_lba) == 0x44 &&
	       offsetof(SceMbr, partitions) == 0x50,
	       "SceMbr boot-bank and partition layout");
