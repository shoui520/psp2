/* FAT/exFAT drive information owned by vfs_exfatfs.c. */
#pragma once

#include <threadmgr/fast_mutex_object.h>
#include <threadmgr/threadmgr.h>
#include <iofilemgr/vfs.h>
#include <exfatfs/fat_boot.h>
#include <exfatfs/exfat_directory.h>

struct SceExfatfsFileSystem;
struct SceExfatfsDrive;

/* The legacy drive interface: only its initialization callback is read. */
struct ExfatfsLegacyOperations {
	void *reserved[10];
	int (*initialize)(struct SceExfatfsDrive *drive);
};

/* A legacy drive's file: only its cached entry is cleared on a forced unmount. */
struct ExfatfsLegacyFile {
	unsigned char reserved[0x684];
	SceUInt32 cachedEntry;
};

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
	SceUInt32 finalizeClearedWords[2];	/* only cleared, with assignName and fileSystem */
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

/* access_control.c */
int sceFatfsACSetFSAttributeByPermission(SceExfatfsDrive *drive,
				      SceUInt32 permission,
				      unsigned char *attribute);
int sceFatfsACSetFSAttributeByAttribute(SceExfatfsDrive *drive,
				     unsigned char attribute,
				     unsigned char *output);
int sceFatfsACGetFSAttributeByThreadAccessLevel(SceExfatfsDrive *drive,
					     int accessLevel, int permission,
					     unsigned char *attribute);
int sceFatfsACCheckRDirEntry(SceExfatfsDrive *drive, unsigned char attribute);
int sceFatfsACGetDirPermission(SceExfatfsDrive *drive, unsigned char attribute);
int sceFatfsACGetRootDirAttribute(SceExfatfsDrive *drive, unsigned char *attribute);

/* codepage.c */
int sceFatfsCpInitTable(void);
int utf16ToCodepage(unsigned char *destination, int capacity, const SceUInt16 *source);
int codepageToUtf16(SceUInt16 *destination, int capacity, const unsigned char *source);
int isCodepageDoubleByte(const unsigned char *source);
int isUnicodeDoubleByte(unsigned int unicode);

/* exfatfs_utility.c */
void *allocateFatBuffer(SceSize size);
void freeFatBuffer(void *buffer, SceSize size);	/* callers pass the size; it is ignored */
unsigned int checksumUtf16Name(const SceUInt16 *name, unsigned int length);
int fatNameLength(const SceUInt16 *name);
SceUInt16 *fatNameCopyBounded(SceUInt16 *destination, const SceUInt16 *source, unsigned int count);
int fatNameCompare(const SceUInt16 *left, const SceUInt16 *right);
SceUInt16 *fatNameAppendBounded(SceUInt16 *destination,
	const SceUInt16 *source, unsigned int count);
int fatCodepageToUtf16(SceUInt16 *destination, int capacity, const unsigned char *source);
int isShortNameCharacter(unsigned int character, int allowExtended);
int shortNameTimeCharacter(void);
int UVFAT_ToUpperBuf(SceUInt16 *destination, const SceUInt16 *source,
	int count, SceExfatfsFileSystem *fileSystem);

/* heap.c */
int sceFatfsInitializeHeap(int reuseHeap);
int sceFatfsFinalizeHeap(int reuseHeap);
void *allocateHeapBuffer(struct SceExfatfsDrive *unusedDrive);
void freeHeapBuffer(int unused, void *buffer);
void *allocateCommonHeapMemory(int unused, SceSize size);
void freeCommonHeapMemory(int unused, void *memory);

/* exfatfs.c */
int initializeLegacyDrive(SceExfatfsDrive *drive);

/* op_devctl_getdev.c */
int _exfatfs_get_devinf(SceExfatfsFileSystem *fileSystem, void *information,
	SceSize informationSize, SceExfatfsSharedResource *sharedResource);
int _exfatfs_set_fake_free(SceExfatfsFileSystem *fileSystem,
	SceExfatfsSharedResource *resource, SceUInt64 fakeFree,
	SceExfatfsSharedResource *sharedResource);

/* path_utility.c */
int utf16ToUtf8(unsigned char *destination, unsigned int capacity, const SceUInt16 *source);
void freePathBuffer(int unused, void *buffer);
int sceFatfsPrepareNameset(int unused, const unsigned char *path, SceFatfsNameset **nameset);
int validateDateTime(const SceExfatfsDateTime *dateTime);

/* utf16_utility.c */
int utf16Length(const SceUInt16 *string);
SceUInt16 *utf16Copy(SceUInt16 *destination, const SceUInt16 *source);
SceUInt16 *utf16FindChar(SceUInt16 *string, int character);
int utf16Compare(const SceUInt16 *left, const SceUInt16 *right);
SceUInt16 *utf16CopyBounded(SceUInt16 *destination, const SceUInt16 *source, unsigned int count);

/* uvexfat_allocbm.c */
int _check_allocation_bitmap(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem);
int UVFAT_readallocationbitmap(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem);
int updateallocbitmaps(SceExfatfsFileSystem *fileSystem);
int UVFAT_allocationbitmapentry(SceExfatfsSharedResource *sharedResource,
	int mode, SceExfatfsFileSystem *fileSystem, SceUInt32 cluster, SceUInt32 *value);

/* uvexfat_uptbl.c */
int UVFAT_readupcasetable(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem);
void freeUpcaseStorage(SceExfatfsFileSystem *fileSystem);
unsigned int lookupUpcaseCharacter(unsigned int unicode, SceExfatfsUpcaseTable *table);

/* uvexfat_util.c */
int UVFAT_InitLockMountPoint(SceExfatfsFileSystem *fileSystem);
int UVFAT_FinalizeLockMountPoint(SceExfatfsFileSystem *fileSystem);
int UVFAT_LockMountPoint(SceExfatfsFileSystem *fileSystem);
int UVFAT_UnlockMountPoint(SceExfatfsFileSystem *fileSystem);

/* uvexfatfs.c */
int _exfat_lock_shared_resource(const char *caller);
int _exfat_unlock_shared_resource(const char *caller);
void _print_fnode(SceExfatfsFnode *fnode, const char *caller);
int _deleted_entry(SceExfatfsFnode *fnode);
void clearClusterCacheStorage(SceExfatfsFnode *fnode);
void _invalidate_cluster_cache_element(SceExfatfsClusterCacheElement *element);
void _initialize_cluster_cache_data(SceExfatfsFnode *fnode);
void _invalidate_cluster_cache_data(SceExfatfsFnode *fnode);
int exfatfsErrorToIoError(int error);
int sceExfatfsInit(void);
int _sceFatfsWriteModeFromStat(SceExfatfsDrive *drive, int attribute);
int _cache_all_fat_data_drive(SceExfatfsDrive *drive, int unused,
		const char *devname, int unusedMode, int flags);
int _used_resource_count(SceVfsMount *mount);
void _free_all_cluster_cache(SceExfatfsDrive *drive);
int _exfatfs_renamed_file(const SceExfatfsFnode *vnode, const SceExfatfsFnode *file);
uint64_t fnodeToFileId(const SceExfatfsFnode *fnode);
SceSize getDriveClusterSize(SceExfatfsDrive *drive);
int _sceExfatfsVopSetRoot(SceExfatfsDrive *drive, SceExfatfsFnode **root);
int _sceExfatfsVopGetRoot(SceExfatfsDrive *drive, SceExfatfsFnode **root);
int sceExfatfsVopInactive(SceExfatfsFnode *fnode);
int sceExfatfsVopDecodePathElem(SceExfatfsDrive *drive, const unsigned char *path,
	SceSize *offset, SceSize *length, unsigned char *destination, SceSize size);
int _exfatfs_reference_count(SceExfatfsFnode *parent, const SceIoDirent *entry, int *error);
int sceExfatfsVopLookup(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, SceExfatfsFnode **output);
int probeWhiteoutDestination(SceExfatfsFnode *parent, const SceVfsPath *path);
int sceExfatfsVopGetstat(SceExfatfsFnode *fnode, SceIoStat *stat);
int sceExfatfsVopDclose(unsigned int fd);
int sceExfatfsVopClose(unsigned int fd);
SceOff _exfatfs_vnode_size(SceExfatfsFnode *fnode);
SceOff _exfatfs_file_size(unsigned int fd);
int sceExfatfsVopLseek(unsigned int fd, SceOff offset, int whence, SceOff *position);
int sceExfatfsVopPread(unsigned int fd, void *buffer, unsigned int size,
	SceOff offset, unsigned int *readSize);
int sceExfatfsVopPwrite(unsigned int fd, const void *buffer, unsigned int size,
	SceOff offset, unsigned int *writeSize);
SceExfatfsFile *_exfatfs_open_common(SceExfatfsFnode *fnode, int mode,
	SceExfatfsSharedResource *sharedResource, int *error);
int sceExfatfsVopDopen(SceExfatfsFnode *fnode, unsigned int *fd);
int sceExfatfsVopOpen(SceExfatfsFnode *fnode, int flags, int *fd);
int sceExfatfsVopDread(unsigned int fd, SceIoDirent *dirent);
int _chstat_setup(SceExfatfsFnode *fnode, SceIoStat *stat, unsigned int bits);
int checkDriveWritable(SceExfatfsDrive *drive);
void _exfatfs_set_whiteout_directory(SceExfatfsFnode *fnode);
void _clear_whiteout_directory(SceExfatfsFnode *fnode);
SceExfatfsFnode *getWhiteoutDirectory(SceExfatfsFnode *fnode);
SceExfatfsFnode *getDriveWhiteoutDirectory(SceExfatfsDrive *drive);
void _exfatfs_set_whiteout_bit(SceExfatfsFnode *fnode);
int _exfatfs_free_cluster_link(SceExfatfsFnode *fnode, SceUInt32 cluster);
int sceExfatfsVopRemove(SceExfatfsFnode *parent_fnode, SceExfatfsFnode *fnode);
int _check_empty_directory(SceExfatfsFnode *parent_fnode, SceExfatfsFnode *fnode);
int sceExfatfsVopMkdir(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, int mode, SceExfatfsFnode **output);
int sceExfatfsVopCreate(SceExfatfsFnode *parent, const void *suffix,
	int suffixLength, int mode, SceExfatfsFnode **output);
int sceExfatfsVopLink(SceExfatfsFnode *from, SceExfatfsFnode *to);
int sceExfatfsVopUnlink(SceExfatfsFnode *from, SceExfatfsFnode *to);
int sceExfatfsVopRename(SceExfatfsFnode *oldFnode, SceExfatfsFnode *parent,
	const void *suffix, int suffixLength, SceExfatfsFnode **output);
int _VopGetstatRootdir(SceExfatfsDrive *drive, SceIoStat *output);
int sceExfatfsVopSync(SceExfatfsFnode *fnode, SceUInt32 fd, int flags);
int sceExfatfsVopCleanup(SceExfatfsFnode *fnode, SceUInt32 fd);
int _exfatfs_flush_all_data(SceExfatfsDrive *drive);
int sceExfatfsVopUmount(SceExfatfsDrive *drive, int mode);
int exfatfs_get_drive_information(SceExfatfsDrive *drive, void *information,
	SceSize informationSize);
int exfatfs_get_file_size_limit(SceExfatfsDrive *drive, SceUInt64 *limit);
int exfatfs_set_fake_free(SceExfatfsDrive *drive, SceUInt64 fakeFree);
int exfatfs_clear_fake_free(SceExfatfsDrive *drive);

/* uvfat_fileio.c */
SceUInt32 _root_cluster(SceExfatfsFileSystem *fileSystem);
void _set_root_cluster_and_cluster_count(SceExfatfsFnode *fnode, SceUInt32 cluster, SceUInt64 size);
void _set_start_cluster_and_cluster_count(SceExfatfsFnode *fnode,
	SceUInt32 cluster, SceUInt64 size);
SceExfatfsFile *invalidateFileCachePosition(SceExfatfsFile *file);
SceUInt32 UVFAT_NextCluster(SceExfatfsSharedResource *sharedResource,
	SceUInt32 cluster, SceExfatfsFileSystem *fileSystem);
SceUInt32 nextContiguousCluster(SceExfatfsFileSystem *fileSystem,
	SceExfatfsStat *stat, SceUInt32 cluster);
SceUInt32 contiguousClusterAtIndex(SceExfatfsFileSystem *fileSystem,
	SceUInt32 start, SceUInt32 index, SceUInt64 size);
SceUInt32 UVFAT_SearchEmptyCluster(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceUInt32 FatEmptyClust);
int UVFAT_FreeClusterLink(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);
int UVFAT_FreeClusterLink_ex_NFC(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsStat *stat, SceUInt32 cluster);
void _flush_fd_cache(SceExfatfsSharedResource *sharedResource, SceExfatfsFnode *fnode);
int _has_parent_directory(SceExfatfsFnode *fnode);
void touchFnodeModificationTime(SceExfatfsFnode *fnode);
int _exfatfs_release_temporal_file(SceExfatfsFileSystem *fileSystem);
int UVFAT_InitDirEntry_ex(SceExfatfsSharedResource *sharedResource,
	int fd, SceUInt32 dirCluster, SceExfatfsFileSystem *fileSystem,
	unsigned char flags, SceUInt32 size);
int _exfatfs_flush_metadata(SceExfatfsSharedResource *sharedResource, SceExfatfsFnode *fnode);
int UVFAT_ReadData(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file,
	void *buffer, unsigned int size, unsigned char flags);
int _exfatfs_search_next_short_name(SceExfatfsSharedResource *sharedResource,
	int fd, SceExfatfsFileSystem *fileSystem, SceExfatfsFatDirectoryEntry *entry);
int _ReadDir_fat(SceExfatfsSharedResource *sharedResource, SceExfatfsFile *file,
	SceUInt16 *longName, SceExfatfsStat *stat, unsigned char *shortName, int fd);
int UVFAT_SearchGetFile_ex(int mode, SceExfatfsSharedResource *sharedResource,
	SceUInt16 *filename, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, int fd);
int _update_metadata(SceExfatfsSharedResource *sharedResource, int fd);
int UVFAT_WriteData(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, SceExfatfsFile *file,
	const void *buffer, unsigned int size, unsigned int flags,
	unsigned int *writeResult);
int UVFAT_SearchFileFromDir(SceExfatfsSharedResource *sharedResource,
	SceUInt16 *filename, SceExfatfsFileSystem *fileSystem, SceExfatfsFnode *fnode);
int UVFAT_SearchFile(SceExfatfsSharedResource *sharedResource,
	SceFatfsNameset *nameset, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, const SceExfatfsFnode *parentFnode);
int _UVFAT_SearchFile_from_root(SceExfatfsSharedResource *sharedResource,
	SceFatfsNameset *nameset, SceExfatfsFileSystem **fileSystem,
	SceExfatfsFnode *fnode, SceExfatfsFnode *parentFnode);
int UVFAT_LinkFromDir_ex(SceExfatfsSharedResource *sharedResource,
	const SceUInt16 *filename, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode, SceExfatfsFnode *parentFnode);
int UVFAT_UnLinkFromDir(SceExfatfsSharedResource *sharedResource,
	const SceUInt16 *filename, SceExfatfsFileSystem *fileSystem,
	SceExfatfsFnode *fnode);
int UVFAT_InitDirEntry(SceExfatfsSharedResource *sharedResource, int fd,
	SceUInt32 dirCluster, SceExfatfsFileSystem *fileSystem);
int UVFAT_InitFd(SceExfatfsSharedResource *sharedResource, int fd,
	SceExfatfsFileSystem *fileSystem);
int UVFAT_ReadDirEntry(SceExfatfsSharedResource *sharedResource, int fd,
	void *primary, void *secondary, SceExfatfsFileSystem *fileSystem);

/* uvfat_if.c */
SceUInt32 getInitializedFnodeCount(SceExfatfsSharedResource *sharedResource);
SceUInt32 getInitializedFileCount(SceExfatfsSharedResource *sharedResource);
int UVFAT_Initialize(SceExfatfsSharedResource *sharedResource);
int UVFAT_Mount(SceExfatfsSharedResource *sharedResource, const char *devname,
	int mode, const SceExfatfsMountParam *mountParam);
int UVFAT_Read(SceExfatfsSharedResource *sharedResource, unsigned int fd,
	void *buffer, unsigned int size);
int UVFAT_Write(SceExfatfsSharedResource *sharedResource, unsigned int fd,
	const void *buffer, unsigned int size);
int getFnodeReferenceCount(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path);
SceExfatfsFnode *UVFAT_GetFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path);
int UVFAT_Create(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *parent, SceFatfsNameset *nameset, int mode,
	SceExfatfsFnode **output);
int _change_path_element(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *oldPath,
	int keepLength, SceExfatfsFnode *fnode);
SceExfatfsFnode *UVFAT_GetParentFnode(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, const SceUInt16 *path);
int UVFAT_ReleaseFnode(SceExfatfsSharedResource *sharedResource, SceExfatfsFnode *fnode);
int initializeFnode(SceExfatfsFnode *fnode);
SceExfatfsFile *resetFilePosition(SceExfatfsFile *file);
int _AllocateFdBuffer(SceExfatfsFile *file);
void _FreeFpCache(SceExfatfsFile *file);
void _ReleaseFp(SceExfatfsSharedResource *sharedResource, SceExfatfsFile *file);
unsigned int _ReserveUnusedFd(SceExfatfsSharedResource *sharedResource, int mode);
int closeFileDescriptor(SceExfatfsSharedResource *sharedResource, unsigned int fd);

/* uvfat_rawio.c */
int hasCachedFreeClusterCount(SceExfatfsFileSystem *fileSystem);
void cacheFreeClusterCount(SceExfatfsFileSystem *fileSystem, SceUInt32 count);
void accountAllocatedCluster(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);
void accountFreedCluster(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);
SceUInt64 clusterToDeviceSector(SceUInt32 cluster, SceExfatfsFileSystem *fileSystem);
int getCurrentFatDateTime(SceExfatfsDateTime *dateTime);
int initializeFatDateTime(SceExfatfsDateTime *dateTime);
unsigned int UVFAT_TimeToFATDate(SceExfatfsDateTime *dateTime);
int exfatfs_cleanup_device_file_buffer(SceExfatfsDrive *drive);
int UVFAT_ReadDevice(SceExfatfsFileSystem *fileSystem, SceUInt64 sector,
	unsigned int size, void *buffer);
int UVFAT_ReadCblock(SceUInt32 cluster, SceUInt32 block,
	SceExfatfsFileSystem *fileSystem, void *buffer);
unsigned int fatDateTimeToTime(SceExfatfsDateTime *dateTime);
int copyFatEntryAttributeToStat(const SceExfatfsFatDirectoryEntry *entry,
	SceExfatfsStat *stat, SceExfatfsFileSystem *fileSystem);
int applyFatAttributeToStat(SceExfatfsDrive *drive, int attribute, SceExfatfsStat *stat);
int copyExfatEntryAttributeToStat(const SceExfatfsFileDirectoryEntry *entry,
	SceExfatfsStat *stat, SceExfatfsFileSystem *fileSystem);
int copyStatAttributeToFatEntry(const SceExfatfsStat *stat,
	SceExfatfsFatDirectoryEntry *entry, SceExfatfsFileSystem *fileSystem);
SceExfatfsDateTime *decodeFatDateTime(SceExfatfsDateTime *dateTime,
	unsigned int date, unsigned int time);
int decodeExfatUtcOffset(unsigned int offset);
void decodeExfatEntryDateTimes(SceExfatfsStat *stat, const SceExfatfsFileDirectoryEntry *entry);
int UVFAT_WriteDevice(SceExfatfsFileSystem *fileSystem, SceUInt64 sector,
	SceSize size, const void *buffer);
int UVFAT_WriteCblock(SceUInt32 cluster, SceUInt32 block,
	SceExfatfsFileSystem *fileSystem, const void *buffer);
int UVFAT_FlushFdBuf(SceExfatfsFile *file, SceExfatfsFileSystem *fileSystem);
int releaseMediaStorage(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, int mountMode);
int UVFAT_MediaInitialize(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFileSystem *fileSystem, int mountMode);
int flushFileSystemMetadata(SceExfatfsFileSystem *fileSystem);
SceUInt32 fileSystemClusterSize(SceExfatfsFileSystem *fileSystem);
int UVFAT_GetFreeClusterCount(SceExfatfsFileSystem *fileSystem, SceUInt32 *count);
int UVFAT_SetGetFAT(SceExfatfsFileSystem *fileSystem, int mode, SceUInt32 cluster, SceUInt32 *next);
int UVFAT_SetEOF(SceExfatfsFileSystem *fileSystem, SceUInt32 cluster);
SceUInt32 UVFAT_GetFatEmptyClust(SceExfatfsFileSystem *fileSystem, SceUInt32 startCluster);
int UVFAT_SetFatChain_ex_NFC(SceExfatfsFileSystem *fileSystem, int mode,
	SceUInt32 startCluster, SceUInt32 lastCluster);

/* uvfat_util.c */
int _Update_FAT_Size(SceExfatfsFnode *fnode, SceExfatfsStat *stat,
	unsigned int updateTime, SceExfatfsSharedResource *sharedResource);
int UVFAT_MakeDir_vop(SceExfatfsSharedResource *sharedResource,
	SceExfatfsFnode *parent, SceFatfsNameset *nameset, unsigned int mode,
	SceExfatfsFnode *fnode);

/* vfs_exfatfs.c */
void _exfatfs_notify_invalid_boot_signature(SceExfatfsDrive *drive);
void _exfatfs_notify_invalid_sector_shift(SceExfatfsDrive *drive, SceUInt32 sectorSize);
void _exfatfs_notify_unsupported_sector_size(SceExfatfsDrive *drive);
void _exfatfs_notify_broken_cluster_chain(SceExfatfsDrive *drive,
	SceUInt32 cluster, SceUInt32 next);
void _exfatfs_notify_invalid_directory_entry(SceExfatfsDrive *drive,
		const SceExfatfsFatDirectoryEntry *entry);
void _exfatfs_notify_directory_loop(SceExfatfsDrive *drive, SceUInt32 startCluster,
	SceOff offset, unsigned char attributes, SceOff fileSize);
int _register_exfatfs_vops(SceVfsInfo *info);
