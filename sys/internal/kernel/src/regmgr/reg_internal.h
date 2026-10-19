/*
 * reg_internal.h: the reg library ("SceRegBlockAPI"): the registry files
 * (.ireg index, .dreg data) and the open registries and categories.
 * All names unknown unless noted.
 */

#ifndef REG_INTERNAL_H
#define REG_INTERNAL_H

#include <scetypes.h>
#include <sysmem/sysclib.h>
#include <string.h>
#include <sysmem/utils.h>
#include "reg.h"

/* 4.00 builds the reg library with _FORTIFY_SOURCE=1: whole-object sizes */
#if FW_VERSION >= 0x04000000
#define memcpy(dst, src, len)	__builtin___memcpy_chk(dst, src, len, __builtin_object_size(dst, 0))
#define memset(dst, c, len)	__builtin___memset_chk(dst, c, len, __builtin_object_size(dst, 0))
#define strncpy(dst, src, len)	__builtin___strncpy_chk(dst, src, len, __builtin_object_size(dst, 0))
#define strncat(dst, src, len)	__builtin___strncat_chk(dst, src, len, __builtin_object_size(dst, 0))
#endif

/* the reg library's errors (0x800cXXXX; names unknown) */
#define REG_ERROR_NO_MEMORY_HEAP	0x800c2712
#define REG_ERROR_NO_FILES		0x800c2714
#define REG_ERROR_NOT_FOUND		0x800c2715
#define REG_ERROR_BAD_CATEGORY_PATH	0x800c2716
#define REG_ERROR_NO_FREE_BLOCK		0x800c2717
#define REG_ERROR_CATEGORY_NOT_FOUND	0x800c2718
#define REG_ERROR_BAD_OPEN_MODE		0x800c2719
#define REG_ERROR_NO_BLOCKS		0x800c271a
#define REG_ERROR_CATEGORY_FULL		0x800c271b
#define REG_ERROR_BAD_KEY_TYPE		0x800c271c
#define REG_ERROR_KEY_NOT_FOUND		0x800c271d
#define REG_ERROR_KEY_IS_CATEGORY	0x800c2722
#define REG_ERROR_KEY_BUFFER_SIZE	0x800c2723
#define REG_ERROR_TOO_MANY_BLOCKS	0x800c271f
#define REG_ERROR_NO_SPACE		0x800c2720	/* the records are there already */
#define REG_ERROR_NO_RECORDS		0x800c2721
#define REG_ERROR_HAS_CATEGORIES	0x800c2724
#define REG_ERROR_NO_DATA		0x800c2725
#define REG_ERROR_KEY_EXISTS		0x800c2728
#define REG_ERROR_VALUE_TOO_BIG		0x800c2729
#define REG_ERROR_HEAP			0x800c272b
#define REG_ERROR_PARENT_NOT_FOUND	0x800c272d
#define REG_ERROR_CATEGORY_NOT_OPEN	0x800c272e
#define REG_ERROR_REGISTRY_CLEAN	0x800c272f
#define REG_ERROR_CATEGORY_CLEAN	0x800c2730
#define REG_ERROR_BAD_PATH		0x800c273b
#define REG_ERROR_MUTEX			0x800c273d
#define REG_ERROR_LOCK			0x800c273e
#define REG_ERROR_UNLOCK		0x800c273f
#define REG_ERROR_FLUSH			0x800c2715
#define REG_ERROR_COPY_WRITE		0x800c4e23
#define REG_ERROR_COPY_READ		0x800c4e24
#define REG_ERROR_BLOCK_SEEK		0x800c4e25
#define REG_ERROR_COPY_CLOSE		0x800c4e26
#define REG_ERROR_NO_CACHE		0x800c2735
#define REG_ERROR_NAME_TOO_LONG		0x800c2736
#define REG_ERROR_NO_NAME		0x800c2737
#define REG_ERROR_NO_PATH		0x800c2738
#define REG_ERROR_BAD_DIGEST		0x800c2739
#define REG_ERROR_BAD_CHECK		0x800c273a
#define REG_ERROR_CATEGORY_IN_USE	0x800c273c
#define REG_ERROR_NO_DIRECTORY		0x800c2731
#define REG_ERROR_INDEX_SIZE		0x800c4e29
#define REG_ERROR_BAD_EXPORT		0x800c4e2d
#define REG_ERROR_EXPORT_DIGEST		0x800c4e2e
#define REG_ERROR_CREATE		0x800c4e21
#define REG_ERROR_BAD_OPEN		0x800c4e27
#define REG_ERROR_RESTORE		0x800c4e2c

#define REG_FILE_INDEX		0	/* .ireg */
#define REG_FILE_DATA		1	/* .dreg */

#define REG_PATH_LEN		0x200
#define REG_CATEGORY_PATH_LEN	0x100
#define REG_BLOCK_SIZE		0x200	/* a block of the data file */

/* the image of a registry's index file: this header, a block bitmap and the
 * category table, each at its offset in the image */
typedef struct RegIndexImage {
	char magic[4];			/* "\0IRF" */
	SceUInt32 version;		/* REG_INDEX_VERSION_* */
	SceUInt32 digest[5];		/* SHA-1 of the image */
	SceUInt32 dataSize;		/* the data file */
	SceUInt32 blockSize;
	SceUInt32 categoryNum;		/* entries in the category table */
	SceUInt32 freeBlockNum;		/* data blocks free */
	SceUInt32 bitmapSize;
	SceUInt32 bitmapOffset;		/* the data blocks in use, one bit each */
	SceUInt32 categorySize;
	SceUInt32 categoryOffset;	/* the RegCategoryEntry table */
} RegIndexImage;

#define REG_CATEGORY_NAME_LEN	0x1a

/* a key of a category, in its data blocks */
typedef struct RegKeyRecord {
	SceUInt8 type;			/* REG_TYPE_* */
	char name[REG_CATEGORY_NAME_LEN];
	SceUInt8 info;			/* not yet traced */
	SceUInt32 value;		/* an int, or the size (bits 0-15) and block
					 * offset in 0x20 units (bits 24-31) of a
					 * string or binary */
} RegKeyRecord;

/* the types of a key (names unknown) */
#define REG_OPEN_CREATE		1	/* regOpenCategory: made when missing */
#define REG_OPEN_EXISTING	2

#define REG_TYPE_CATEGORY	1
#define REG_TYPE_INT		2
#define REG_TYPE_STR		3
#define REG_TYPE_BIN		4

#define REG_KEY_SIZE(value)	((value) & 0xffff)
#define REG_KEY_RECORDS(value)	(((value) >> 16) & 0xff)	/* data records */
#define REG_KEY_OFFSET(value)	((value) >> 24)

/* the first 0x200-byte block of a category's data: this header, then its keys */
typedef struct RegDataBlock {
	SceUInt16 recordMax;		/* not yet traced: blockNum * 16 - 1 when made */
	SceUInt16 blockNum;
	SceUInt16 info;			/* not yet traced: blockNum * 32 when made */
	SceUInt16 freeRecordNum;	/* the key records left in the blocks */
	SceUInt16 recordNum;		/* the next key record (counting this header) */
	SceUInt16 dataRecord;		/* the last record free for data, taken downwards */
	SceUInt16 keyNum;
	SceUInt8 check[4];		/* regComputeBlockCheck */
	SceUInt8 recordBitmap[0xe];	/* the key records in use, from bit 7 */
	RegKeyRecord key[];
} RegDataBlock;

/* the blocks of an open category, read from the data file */
typedef struct RegCategoryCache {
	SceInt16 openCount;		/* the opens of the category sharing it */
	SceUInt16 info;			/* not yet traced: 1 when read */
	RegDataBlock *data;		/* blockNum 0x200-byte blocks */
} RegCategoryCache;

#define REG_CATEGORY_BLOCK_MAX	5
#define REG_CATEGORY_NONE	0xffff

/* an entry of the category table; an open category's handle points to it */
typedef struct RegCategoryEntry {
	struct RegRegistry *registry;	/* set while the category is open */
	SceUInt16 parent;		/* REG_CATEGORY_NONE at the top level */
	SceUInt16 slot;			/* its hash slot, REG_CATEGORY_NONE when free */
	SceUInt16 id;			/* not yet traced: matched when a path is looked up */
	SceUInt16 keyNum;
	SceUInt16 blockNum;
	char name[REG_CATEGORY_NAME_LEN];
	SceUInt32 info2;		/* not yet traced */
	SceUInt16 block[REG_CATEGORY_BLOCK_MAX]; /* its data blocks, REG_CATEGORY_NONE unused */
	SceUInt16 padding;
	RegCategoryCache *cache;
} RegCategoryEntry;

/* index image versions (names unknown) */
#define REG_INDEX_VERSION_OLD	0x10000	/* top-level parents stored as 0 */
#define REG_INDEX_VERSION	0x10003

#define REG_INDEX_HEADER_SIZE	sizeof(RegIndexImage)

/* a run of consecutive data blocks, written in one go */
typedef struct RegBlockRun {
	SceUInt16 block;
	SceUInt16 num;
} RegBlockRun;

/* the header of an exported registry: the backup .ireg and .dreg follow it */
typedef struct RegExportHeader {
	char magic[12];			/* "*DATA-BASE*" */
	SceUInt32 headerSize;
	SceInt32 fileSize[2];
	SceUInt8 digest[0x14];		/* SHA-1 of the files */
	SceUInt8 reserved[0x14];
} RegExportHeader;

/* an open registry */
typedef struct RegRegistry {
	SceUInt32 state;		/* not yet traced: 1 once closed */
	int modified;			/* written since the last flush */
	int openCount;			/* the opens beyond the first */
	SceUInt32 id;			/* the registry it was opened as */
	SceUID fd[2];			/* REG_FILE_INDEX/DATA, -1 when closed */
	char path[REG_PATH_LEN];	/* the file path, no extension */
	RegIndexImage *image;
	struct RegRegistry *next;
	SceSize imageSize;
	SceUInt32 dataSize;		/* the image's dataSize */
} RegRegistry;

/* an open category */
typedef struct RegCategory {
	int registry;			/* the registry it was opened in */
	int modified;			/* written since the last flush */
	int handle;
	struct RegCategory *next;
} RegCategory;

extern SceUID g_regMutex;
extern SceSha1Context g_regSha1Context;
extern RegExportHeader g_regImportHeader;
extern RegExportHeader g_regExportHeader;
extern int g_regRegistryOpened;
extern int g_regInitialized;
extern RegRegistry *g_regRegistryList;
extern RegRegistry *g_regRegistryTail;
extern int g_regRegistryNum;
extern int g_regCategoryNum;
extern RegCategory *g_regCategoryList;
extern RegCategory *g_regCategoryTail;

/* reg_platform.c */
int regPlatformInit(void);
void regMakeDirectory(void);
SceUID regFileCreate(const char *path);
SceUID regFileCreateFullPath(const char *path);
SceUID regFileOpen(const char *path);
SceUID regFileOpenFullPath(const char *path);
int regFileClose(SceUID fd);
int regFileRemove(const char *path);
int regFileRename(const char *oldPath, const char *newPath);
int regFileWrite(const void *data, int size, int num, SceUID fd, int *written);
int regFileRead(void *data, int size, int num, SceUID fd, int *read);
int regFileSeek(SceUID fd, int offset, int whence);
int regFileGetSize(const char *path, SceUInt32 *size);

/* reg_memory.c */
int regMemInit(int unused, SceSize size);
void *regMemAlloc(SceSize size);
void regMemFree(void *ptr);
int regPlatformLock(void);
void regPlatformUnlock(int state);
const void *regPlatformCheckPointer(const void *ptr);
const void *regPlatformCheckBuffer(const void *buf, int size);
int regHashName(const SceChar8 *name, int size, int *hash);

/* reg_registry.c, reg_category.c, reg_key.c */
RegRegistry *regGetRegistry(int registry);
SceUID regOpenRegistryFile(RegRegistry *reg, int file);
void regCloseRegistryFile(RegRegistry *reg, SceUID fd, int file);
int regFindCategory(int handle);
int regSetRegistryModified(int registry);
int regClearRegistryModified(int registry);
int regClearCategoryModified(int handle);
int regIsRegistryModified(int registry);
int regIsCategoryModified(int handle);
int regFlushIndex(int registry);
int regLoadRegistry(const SceUInt32 *param, const char *indexPath, const char *dataPath,
		    int *registry);
int regFindCategoryEntry(int registry, RegIndexImage *image, SceUInt32 parent, SceUInt32 slot,
			 const char *name, RegCategoryEntry **entry);
int regReadCategoryBlocks(RegCategoryEntry *entry, SceUID fd, RegCategoryCache **cache);
int regComputeBlockCheck(RegDataBlock *block, SceSize size, SceUInt8 *check);
int regLoadCategoryCache(int create, RegRegistry *reg, RegCategoryEntry *entry);
void regFreeCategoryEntry(RegCategoryEntry *entry, RegIndexImage *image);
void regAddOpenCategory(int registry, int handle);
int regFindCategoryByPath(const char *path, RegIndexImage *image, int *index, const SceUInt32 *id);
int regFlushCategory(int handle);
int regReserveCategorySpace(RegCategoryEntry *entry, int size);
int regResizeCategoryCache(SceUInt16 *blockNum, RegCategoryCache **cache, RegDataBlock *data);
int regAddCategoryKey(RegCategoryEntry *entry, const char *name);
int regCloseCategoryUnlocked(int handle);
int regUpdateCategoryCheck(int handle);
int regDeleteKeyUnlocked(int handle, const char *name);

/* reg_category.c */
int regGetCategoryNum(int registry, int *num);
int regGetCategoryNames(int registry, char (*names)[REG_CATEGORY_NAME_LEN + 1], int num);

/* reg_key.c */
int regGetKeyInfo(int handle, const char *name, int *type, int *size);
int regGetKeyNum(int handle, int *num);
int regGetKeyNames(int handle, char (*names)[REG_CATEGORY_NAME_LEN + 1], int num);

#endif
