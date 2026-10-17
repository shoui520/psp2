/* FAT/exFAT drive information owned by vfs_exfatfs.c. */
#pragma once

#include <threadmgr/fast_mutex_object.h>
#include <iofilemgr/vfs.h>
#include <exfatfs/fat_boot.h>
#include <exfatfs/exfat_directory.h>

struct SceExfatfsFileSystem;
struct ExfatfsLegacyOperations;
struct ExfatfsLegacyFile;

typedef struct SceExfatfsDeviceFileInfo {
	SceVfsMountData *mount_misc;
	SceVfsVnode *device_vnode;
	SceVfsFile *device_file;
	SceUID device_fd;
} SceExfatfsDeviceFileInfo;

typedef struct SceExfatfsDrive {
	SceUInt32 state;
	unsigned char reserved[188];
	const char *assignName;
	struct SceExfatfsFileSystem *fileSystem;
	SceUInt32 reservedWords[2];
	const struct ExfatfsLegacyOperations *legacyOperations;
	struct ExfatfsLegacyFile *legacyFiles[64];
	unsigned char reservedTail[12];
	struct ThreadmgrFastMutex fastmutex;
	SceExfatfsDeviceFileInfo deviceInfo;
} SceExfatfsDrive;

_Static_assert(sizeof(SceExfatfsDrive) == 0x230, "drive information size");

typedef struct SceFatfsNameset {
	int requiresLongName;
	SceUInt16 *fileName;
	SceUInt16 filePath[515];
	unsigned char shortName[14];
	int isRoot;
} SceFatfsNameset;

_Static_assert(sizeof(SceFatfsNameset) == 0x420, "nameset size");

typedef struct SceExfatfsDateTime {
	SceUInt16 year;
	SceUInt16 month;
	SceUInt16 day;
	SceUInt16 hour;
	SceUInt16 minute;
	SceUInt16 second;
} SceExfatfsDateTime;

typedef struct SceExfatfsStat {
	SceUInt16 st_mode;
	SceUInt16 st_type;
	SceUInt16 st_dev;
	SceUInt16 st_uid;
	SceOff Size;
	SceExfatfsDateTime ctime;
	SceExfatfsDateTime atime;
	SceExfatfsDateTime mtime;
	SceUInt32 StartCluster;
	SceUInt8 generalSecondaryFlags;
	SceUInt8 st_attr;
} SceExfatfsStat;

_Static_assert(sizeof(SceExfatfsStat) == 0x40, "filesystem stat size");

typedef struct SceExfatfsClusterCacheElement {
	SceUInt32 cluster;
	SceUInt32 index;
	SceUInt32 count;
} SceExfatfsClusterCacheElement;

typedef struct SceExfatfsFnode {
	SceUInt16 FilePath[260];
	SceUInt16 *FileName;
	struct SceExfatfsFileSystem *fileSystem;
	SceExfatfsStat Stat;
	SceUInt32 DirCluster;
	SceOff DirOffset;
	SceUInt32 DirOffClust;
	unsigned char reservedDirectory[4];
	SceSize dirSize;
	SceUInt8 dirGeneralSecondaryFlags;
	SceUInt8 flags;
	unsigned char reservedFlags[2];
	SceInt16 refCount;
	SceInt16 fdCount;
	unsigned char reservedCache[4];
	SceUInt32 cache_element_count;
	SceExfatfsClusterCacheElement *cluster_cache_array;
	SceUInt32 tailCluster;
	SceUInt32 tailIndex;
	SceUInt32 clusterCount;
	SceUInt32 chunkCount;
} SceExfatfsFnode;

_Static_assert(sizeof(SceExfatfsFnode) == 0x290, "filesystem node size");

typedef struct SceExfatfsMountParam {
	SceExfatfsDrive *drive;
	SceUInt32 mount_mode;
} SceExfatfsMountParam;

typedef struct SceExfatfsFile {
	SceExfatfsFnode *fnode;
	int mode;
	SceOff Position;
	void *Buf;
	void *BufTop;
	SceUInt32 Cluster;
	SceUInt32 clusterIndex;
	SceUInt32 cacheFlags;
	SceUInt64 cacheIndex;
	SceUInt32 cacheCluster;
	SceUInt32 clustBlockNo;
} SceExfatfsFile;

_Static_assert(sizeof(SceExfatfsFile) == 0x38, "filesystem file size");

typedef struct SceExfatfsFatCache {
	SceUInt32 firstCluster;
	SceUInt32 clusterCount;
	void *buffer;
	void *allocation;
	SceUInt32 deviceSector;
	int dirty;
	struct SceExfatfsFatCache *next;
} SceExfatfsFatCache;

typedef struct SceExfatfsUpcaseRun {
	SceUInt16 tableIndex;
	SceUInt16 skipCount;
	struct SceExfatfsUpcaseRun *next;
} SceExfatfsUpcaseRun;

typedef struct SceExfatfsUpcaseBlock {
	struct SceExfatfsUpcaseBlock *next;
	SceSize allocationSize;
	SceExfatfsUpcaseRun *nextEntry;
	SceExfatfsUpcaseRun entries[254];
	unsigned char reserved[4];
} SceExfatfsUpcaseBlock;

_Static_assert(sizeof(SceExfatfsUpcaseRun) == 8, "upcase run");
_Static_assert(sizeof(SceExfatfsUpcaseBlock) == 2048, "upcase block");

typedef struct SceExfatfsUpcaseTable {
	SceUInt16 *table;
	unsigned char reserved[4];
	SceUInt64 allocationSize __attribute__((packed, aligned(4)));
	SceExfatfsUpcaseRun *runHead;
	SceExfatfsUpcaseRun **runTailLink;
	SceExfatfsUpcaseBlock *blockList;
} SceExfatfsUpcaseTable;

_Static_assert(sizeof(SceExfatfsUpcaseTable) == 28, "upcase table owner");

typedef struct SceExfatfsFileSystem {
	const char *devname;
	int mode;
	int state;
	SceUInt32 dataAreaStartSector;
	SceUInt16 rootDirectorySectors;
	SceUInt32 clusterCount;
	void *PBRBuf;
	int PBRDirty;
	SceUInt32 freeClusterCount;
	int freeClusterCountValid;
	SceExfatfsFatCache *fatCacheHead;
	SceExfatfsFatCache fatCache[4];
	SceSize fatBufSize;
	int fatDirty;
	SceUInt32 FATBits;
	SceUInt32 ClusterMAX;
	SceExfatfsDrive *drive;
	SceUInt32 mount_mode;
	SceUInt32 fileSystemType;
	SceUInt32 BytePerSectorShift;
	SceSize sectorSize;
	SceUInt32 clusterSectorCount;
	SceUInt32 firstClusterOfRoot;
	SceUInt64 devSectors;
	SceUInt8 bitmapIndex;
	SceUInt8 bitmapDirty;
	SceUInt32 bitmapFirstCluster;
	SceUInt64 bitmapSize;
	void *bitmapBuffer;
	void *bitmapAllocation;
	SceUInt64 bitmapDeviceSector;
	SceUInt32 bitmapNextCluster;
	SceUInt32 bitmapFreeClusterCount;
	SceExfatfsUpcaseTable upcase;
	SceExfatfsFnode *whiteout_directory;
	SceExfatfsFile file;
	SceExfatfsFnode fnode;
	struct ThreadmgrFastMutex rootFastMutex;
	SceSize fileBufferSize;
	SceUInt32 nextFreeCluster;
	SceUInt32 fakeFreeClusterCount;
} SceExfatfsFileSystem;

_Static_assert(sizeof(SceExfatfsFileSystem) == 0x430, "filesystem size");

typedef struct SceExfatfsMountActivity {
	SceUInt32 active;
	SceExfatfsFileSystem *fileSystem;
} SceExfatfsMountActivity;

/* FAT short directory entry, including the creation/access timestamps. */
typedef struct SceExfatfsFatDirectoryEntry {
	unsigned char name[8];
	unsigned char extension[3];
	unsigned char attribute;
	unsigned char nameCase;
	unsigned char creationTenths;
	SceUInt16 creationTime;
	SceUInt16 creationDate;
	SceUInt16 accessDate;
	SceUInt16 firstClusterHigh;
	SceUInt16 modificationTime;
	SceUInt16 modificationDate;
	SceUInt16 firstClusterLow;
	SceUInt32 size;
} SceExfatfsFatDirectoryEntry;

/* FAT long-name entry; each character is an unaligned UTF-16LE pair. */
typedef struct SceExfatfsFatLongDirectoryEntry {
	unsigned char sequence;
	unsigned char namePrefix[5][2];
	unsigned char attribute;
	unsigned char type;
	unsigned char checksum;
	unsigned char nameMiddle[6][2];
	SceUInt16 firstClusterLow;
	unsigned char nameSuffix[2][2];
} SceExfatfsFatLongDirectoryEntry;

typedef union SceExfatfsFatDirectoryRecord {
	SceExfatfsFatDirectoryEntry shortEntry;
	SceExfatfsFatLongDirectoryEntry longEntry;
} SceExfatfsFatDirectoryRecord;

_Static_assert(sizeof(SceExfatfsFatDirectoryRecord) == 32, "FAT directory record size");

typedef struct SceExfatfsFileDirectoryEntry {
	unsigned char entryType;
	unsigned char secondaryCount;
	unsigned char setChecksum[2];
	unsigned char fileAttributes[2];
	unsigned char reserved[2];
	unsigned char creationTime[4];
	unsigned char modificationTime[4];
	unsigned char accessTime[4];
	unsigned char creationTenths;
	unsigned char modificationTenths;
	unsigned char creationUtcOffset;
	unsigned char modificationUtcOffset;
	unsigned char accessUtcOffset;
	unsigned char reservedTail[7];
} SceExfatfsFileDirectoryEntry;

_Static_assert(sizeof(SceExfatfsFileDirectoryEntry) == 32, "exFAT file entry");

typedef struct SceExfatfsStreamDirectoryEntry {
	unsigned char entryType;
	unsigned char generalSecondaryFlags;
	unsigned char reserved;
	unsigned char nameLength;
	unsigned char nameHash[2];
	unsigned char reservedHash[2];
	unsigned char validDataLength[8];
	unsigned char reservedLength[4];
	unsigned char firstCluster[4];
	unsigned char dataLength[8];
} SceExfatfsStreamDirectoryEntry;

typedef struct SceExfatfsNameDirectoryEntry {
	unsigned char entryType;
	unsigned char generalSecondaryFlags;
	unsigned char name[15][2];
} SceExfatfsNameDirectoryEntry;

typedef union SceExfatfsDirectoryRecord {
	SceExfatfsFileDirectoryEntry file;
	SceExfatfsStreamDirectoryEntry stream;
	SceExfatfsNameDirectoryEntry filename;
} SceExfatfsDirectoryRecord;

_Static_assert(sizeof(SceExfatfsDirectoryRecord) == 32, "exFAT directory record");

typedef struct SceExfatfsSharedResource {
	SceExfatfsFileSystem fileSystems[15];
	unsigned char bootSectors[15][512];
	SceExfatfsFile *files;
	SceExfatfsFnode fnodes[4096];
	SceExfatfsMountActivity mountActivity[15];
	SceUInt32 fnodeCapacity;
	SceUInt32 initializedFnodeCount;
	SceUInt32 fileCapacity;
	SceUInt32 initializedFileCount;
} SceExfatfsSharedResource;

_Static_assert(sizeof(SceExfatfsSharedResource) == 0x295d60, "shared resource size");
