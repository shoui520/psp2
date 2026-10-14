/* Complete on-disk exFAT directory records. Numeric fields use little-endian bytes. */
#pragma once

typedef struct SceExfatfsAllocationBitmapEntry {
	unsigned char entryType;
	unsigned char bitmapFlags;
	unsigned char reserved[18];
	unsigned char firstCluster[4];
	unsigned char dataLength[8];
} SceExfatfsAllocationBitmapEntry;

typedef struct SceExfatfsUpcaseTableEntry {
	unsigned char entryType;
	unsigned char reserved[3];
	unsigned char tableChecksum[4];
	unsigned char reservedTable[12];
	unsigned char firstCluster[4];
	unsigned char dataLength[8];
} SceExfatfsUpcaseTableEntry;

typedef struct SceExfatfsVolumeLabelEntry {
	unsigned char entryType;
	unsigned char characterCount;
	unsigned char volumeLabel[11][2];
	unsigned char reserved[8];
} SceExfatfsVolumeLabelEntry;

_Static_assert(sizeof(SceExfatfsAllocationBitmapEntry) == 32, "allocation bitmap entry");
_Static_assert(sizeof(SceExfatfsUpcaseTableEntry) == 32, "upcase table entry");
_Static_assert(sizeof(SceExfatfsVolumeLabelEntry) == 32, "volume label entry");
