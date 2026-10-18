/*
 * ScePfsMgr: the protected file system. Each mount ("pmi", PFS mount info)
 * puts one of three pseudo drives over a directory of an underlying file
 * system: gamedata/savedata (GDSD), application content (AC) or a plain
 * redirect. File data is encrypted and its integrity kept in icv/unicv
 * databases; the crypt engine thread does the work.
 *
 * Cryptographic routines are deferred.
 */

#ifndef PFSMGR_INTERNAL_H
#define PFSMGR_INTERNAL_H

#include <scetypes.h>

#include <sysmem/sysclib.h>
#include <string.h>
#include <iofilemgr/vfs.h>

/* vitasdk: ksceVfsOpDecodePathElem */
int SceIofilemgrForDriver_F7DAC0F5(SceVfsMount *mnt, const char *path, const char **path2,
	const char **path3, char *buf, SceSize size, SceSize *decodeLen);

#include <kernel/error.h>
#include <iofilemgr/iofilemgr.h>

/* PFS's own errors; vfs operations turn them into errno codes (pfsErrorToErrno) */
#define SCE_PFS_ERROR_BAD_MODE		0x80142109	/* name unknown */
#define SCE_PFS_ERROR_NOT_IN_LIST	0x80142409	/* name unknown */
#define SCE_PFS_ERROR_NO_MEMORY		0x8014200b	/* name unknown */
#define SCE_PFS_ERROR_NOT_FOUND		0x80140206	/* name unknown */

#define PFS_MOUNT_NAME_MAX		0x20	/* the mount point's name, without "0:" */
#define PFS_PATH_MAX			0x3f
#define PFS_APPROVED_MAX		16

/* mode indices (ScePfsMgrForKernel_2D48AEA2), grouped by pseudo drive */
#define PFS_MODE_GDSD_MASK	0x26ec	/* 2, 3, 5, 6, 7, 9, 10, 13 */
#define PFS_MODE_AC_MASK	0x1110	/* 4, 8, 12 */
#define PFS_MODE_REDIRECT_MASK	0xf00001	/* 0, 20 to 23 */

/* ScePfsPmi.flags */
#define PFS_PMI_NO_KLICENSEE	0x0001
#define PFS_PMI_READ_ONLY	0x0002	/* name unknown */
#define PFS_PMI_NOT_APPROVED	0x0004
#define PFS_PMI_NOT_DEVELOPMENT	0x0008
#define PFS_PMI_NOT_MOUNTED	0x0010
#define PFS_PMI_MODE_3		0x0020	/* name unknown */
#define PFS_PMI_MODE_7_9_10	0x0040	/* name unknown */

/* a fixed pool of equal elements in one memory block (the "...BufferList"s) */
#define PFS_MUTEX_ATTR		0x2000	/* name unknown */

typedef struct ScePfsBufferList {
	SceUID mutex;
	SceUID memBlock;
	const char *name;
	SceUInt32 count;
	SceSize elementSize;
	void *freeList;
} ScePfsBufferList;

typedef struct ScePfsListNode {
	struct ScePfsListNode *next;
	struct ScePfsListNode *prev;
} ScePfsListNode;

/* a cached page of an underlying file */
typedef struct ScePfsLruEntry {
	struct ScePfsLruEntry *prev;		/* in its hash bucket */
	struct ScePfsLruEntry *next;
	struct ScePfsLruEntry *lruPrev;		/* on the LRU list, while not busy */
	struct ScePfsLruEntry *lruNext;
	SceUInt32 cookie;			/* the file's ScePfsLowerFile.cookie */
	SceUInt32 reserved;
	SceOff offset;
	void *data;
	SceUInt32 flags;			/* PFS_LRU_* */
} ScePfsLruEntry;

#define PFS_LRU_VALID		0x1
#define PFS_LRU_BUSY		0x2
#define PFS_LRU_DIRTY		0x4

typedef struct ScePfsLru {
	SceUInt32 bucketCount;
	SceSize pageSize;
	SceUInt32 entryCount;
	ScePfsLruEntry *head;			/* least recently used */
	ScePfsLruEntry *tail;
	ScePfsLruEntry **buckets;
	ScePfsLruEntry *entries;		/* after the buckets */
	void *pool;
	void *poolStart;
	void *poolEnd;
} ScePfsLru;

/* a file open in the underlying file system */
typedef struct ScePfsLowerFile {		/* name unknown */
	SceVfsVnode *vp;
	SceVfsFile *file;
	SceUID fd;
	SceUInt32 cookie;			/* name unknown: set by pfsLowerOpen */
} ScePfsLowerFile;

/* hands out the pages of a file kept in an ScePfsLru (name unknown) */
typedef struct ScePfsPageAllocator {
	ScePfsLru *lru;
	ScePfsLowerFile *lf;
	SceOff freeHead;			/* name unknown: a free run, -1 = none */
	SceOff end;				/* name unknown: first page never handed out */
	SceOff base;				/* name unknown: file offset of page 0 */
} ScePfsPageAllocator;

#define PFS_UNICV_MAGIC		0x42444f5249454353ULL	/* "SCEIRODB" */

/* the first 0x20 bytes of "unicv.db" (name unknown) */
typedef struct ScePfsUnicvHeader {
	SceUInt64 magic;			/* PFS_UNICV_MAGIC */
	SceUInt32 version;			/* pfsVersionUnicvParam */
	SceSize pageSize;
	SceUInt64 freeHead;			/* ScePfsPageAllocator's */
	SceUInt64 end;
} ScePfsUnicvHeader;

/* the icv trees of all files, in "unicv.db" (name unknown) */
typedef struct ScePfsUnicv {
	ScePfsUnicvHeader header;
	ScePfsPageAllocator pages;
} ScePfsUnicv;

#define PFS_HASH_SIZE	0x14

/* buffers the lower file system reads into by DMA start on 64 bytes (names unknown) */
#define PFS_DMA_ALIGN		0x40
#define PFS_DMA_ALIGNED(buf)	((void *)(((SceUIntPtr)(buf) + PFS_DMA_ALIGN - 1) & ~(PFS_DMA_ALIGN - 1)))

/* a memcpy() into an object of known size: 4.00 checks it (_FORTIFY_SOURCE), 3.65 does not */
#if FW_VERSION >= 0x04000000
#define pfsMemcpyChecked(dst, src, len, size)	__memcpy_chk(dst, src, len, size)
#else
#define pfsMemcpyChecked(dst, src, len, size)	memcpy(dst, src, len)
#endif

/* a SHA-1 sized hash or key (name unknown) */
typedef struct ScePfsHash {
	SceUInt32 words[PFS_HASH_SIZE / 4];
} ScePfsHash;

/* PFS mount info: one per mounted pseudo drive (in "ScePfsPmiBufferList") */
typedef struct ScePfsPmi {
	ScePfsListNode link;			/* in g_pfsPmiList */
	SceUID mutex;				/* "ScePfsPmi" */
	char path[0x40];			/* the directory mounted */
	char rootPath[0x22];			/* "/<name>" */
	char assignName[0x23];			/* "<name>0:" */
	SceUInt16 mode;
	SceUInt16 flags;			/* PFS_PMI_* */
	char klicensee[0x10];
	SceUInt16 a6;				/* name unknown */
	SceUInt32 a8;				/* name unknown */
	SceUInt32 quota;			/* name unknown: bytes the mount may use, 0 = any */
	SceUInt32 version;			/* name unknown: selects the icv formats */
	SceVfsVnode *sceDirVp;			/* "sce_pfs" */
	SceUID filesDbMutex;			/* "ScePfsFilesDb" */
	ScePfsLru filesDbLru;
	ScePfsLowerFile filesDb;		/* "files.db" */
	ScePfsLru icvLru;
	SceVfsVnode *icvDirVp;			/* "icv.db", holds "%08x.icv" */
	ScePfsLowerFile unicvDb;		/* "unicv.db" */
	ScePfsUnicv unicv;
	union {
		ScePfsListNode acidDirs;	/* AC modes: the acid directories */
		struct {			/* acid directory modes 0x1f to 0x21 */
			struct ScePfsPmi *parent;
			SceInt32 openCount;	/* name unknown: files and directories open */
		};
	};
	SceUInt64 approved[PFS_APPROVED_MAX];	/* program authority IDs, -1 = free */
	SceVfsMountData mountData;
	SceInt32 approvedMax;
	ScePfsHash filesDbHash;			/* name unknown: the hmac of files.db's header */
	SceUInt32 clusterSize;			/* name unknown: of the underlying mount */
} ScePfsPmi;

/* a PFS vnode's nodeData (in "ScePfsVdBufferList"): the vnode it covers */
typedef struct ScePfsVd {
	SceVfsVnode *lowerVp;			/* in the underlying file system */
	SceVfsVnode *icvVp;			/* its "%08x.icv" file, with per-file icv */
	SceUInt32 slot;				/* name unknown; -1 = none */
	SceUInt32 parentIndex;			/* its directory's files.db index */
	SceUInt32 index;			/* its files.db index */
	SceUInt16 attr;				/* PFS file attributes */
	ScePfsListNode pages;			/* name unknown */
	SceUInt8 hash[PFS_HASH_SIZE];		/* of its icv tree */
	SceUInt32 reserved;
} ScePfsVd;

#define PFS_FI_FILE	0
#define PFS_FI_DIR	1

/* a PFS file's state, kept in SceVfsFile.fd (in "ScePfsFiBufferList") */
typedef struct ScePfsFi {
	SceUInt32 type;				/* PFS_FI_* */
	ScePfsLowerFile lower;
	SceVfsVnode *vp;
	ScePfsLowerFile icv;			/* name unknown: the file's own icv file */
	SceOff position;
} ScePfsFi;

#define PFS_ICV_UNICV	0	/* name unknown: all in "unicv.db" */
#define PFS_ICV_FILES	1	/* name unknown: one "%08x.icv" per file in "icv.db" */
#define PFS_ICV_NONE	2	/* name unknown */

/* what each mount mode does (pfsModeInfo) */
typedef struct ScePfsModeInfo {			/* name unknown */
	SceUInt32 type;				/* name unknown: 0, 1 or 2 */
	SceUInt32 icvType;			/* PFS_ICV_*: where the icv trees are */
	SceUInt32 reserved8;			/* not yet traced */
	SceUInt32 icvOpenFlags;			/* name unknown */
	SceUInt32 unicvOpenFlags;		/* name unknown */
	SceUInt32 fileMode;			/* name unknown: of files PFS creates */
	SceUInt32 dirMode;			/* name unknown: of directories PFS creates */
	SceUInt32 noCreate;			/* name unknown: never creates files.db/icv */
	SceUInt32 noSync;			/* name unknown */
	SceUInt32 readOnly;			/* name unknown: sets PFS_PMI_READ_ONLY */
} ScePfsModeInfo;

/* a vnode and the name it was looked up by */
typedef struct ScePfsNode {			/* name unknown */
	SceVfsVnode *vp;
	const char *name;
} ScePfsNode;

/* operations checked by pfsAccessBegin */
enum {
	PFS_OP_MOUNT		= 0,
	PFS_OP_UMOUNT		= 1,
	PFS_OP_DEVCTL		= 4,
	PFS_OP_SYNC		= 7,
	PFS_OP_DECODE_PATH_ELEM	= 8,
	PFS_OP_OPEN		= 9,
	PFS_OP_CREATE		= 10,
	PFS_OP_CLOSE		= 11,
	PFS_OP_LOOKUP		= 12,
	PFS_OP_READ		= 13,
	PFS_OP_WRITE		= 14,
	PFS_OP_PREAD		= 15,
	PFS_OP_PWRITE		= 16,
	PFS_OP_LSEEK		= 17,
	PFS_OP_IOCTL		= 18,
	PFS_OP_MKDIR		= 19,
	PFS_OP_RMDIR		= 20,
	PFS_OP_REMOVE		= 21,
	PFS_OP_GETSTAT		= 22,
	PFS_OP_CHSTAT		= 23,
	PFS_OP_RENAME		= 24,
	PFS_OP_DOPEN		= 25,
	PFS_OP_DREAD		= 26,
	PFS_OP_DCLOSE		= 27,
	PFS_OP_VNODE_SYNC	= 30,
	PFS_OP_WHITEOUT		= 31,
	PFS_OP_CLEANUP		= 32,
};

#define PFS_LRU_COUNT		0x78
#define PFS_LRU_TBL_HDR_SIZE	0xdc	/* "ScePfsLruTblHdrBufferList" elements */
#define PFS_LRU_POOL_SIZE	0x1400	/* "ScePfsLruPoolBufferList" elements */

/* a files.db entry's key: a name in a directory */
typedef struct ScePfsDbKey {			/* name unknown */
	SceUInt32 parentIndex;
	const char *name;
	SceVfsMount *mnt;			/* of the underlying file system */
} ScePfsDbKey;

/* bufmgr/page_acc.c: a page of a file in an ScePfsLru, held while in use (name unknown) */
typedef struct ScePfsPageAcc {
	ScePfsLru *lru;
	ScePfsLowerFile *lf;
	ScePfsLruEntry *entry;			/* the page held, NULL = none */
	SceUInt32 reserved;
	SceOff offset;				/* of the page held, from base */
	SceOff base;				/* file offset of page 0 */
} ScePfsPageAcc;

#define SCE_PFS_ERROR_PAGE_OFFSET	0x80140b09	/* name unknown: negative offset */

void page_acc_init(ScePfsPageAcc *acc, ScePfsLru *lru, ScePfsLowerFile *lf, SceOff base);
void page_acc_fini(ScePfsPageAcc *acc);
int page_acc_unlock(ScePfsPageAcc *acc);
int page_acc_lock(ScePfsPageAcc *acc, SceOff offset);

/* heads a run of free pages; free runs form a ring in offset order (name unknown) */
typedef struct ScePfsFreeRun {
	SceOff offset;				/* of the run itself */
	SceOff prev;
	SceOff next;
	SceUInt32 count;			/* pages */
} ScePfsFreeRun;

typedef struct ScePfsHashTree ScePfsHashTree;

/* a counted array of icvs at the start of a page (names unknown) */
typedef struct ScePfsHashArrayHeader {
	SceSize size;				/* bytes the array may use */
	SceSize itemSize;			/* PFS_HASH_SIZE */
	SceUInt32 count;
	SceUInt32 reserved;
} ScePfsHashArrayHeader;

typedef struct ScePfsHashArray {
	ScePfsHashArrayHeader *header;
	ScePfsHash *items;
} ScePfsHashArray;

/* a page of icvs (name unknown) */
typedef struct ScePfsHashList {
	void *data;
	ScePfsHashArray hashes;
} ScePfsHashList;

/* a node of an icv tree: the icvs and the pages of its children (name unknown) */
typedef struct ScePfsHashNodeView {
	SceUInt32 *level;			/* 0 = leaf */
	ScePfsHashArray hashes;
	SceUInt32 *children;
} ScePfsHashNodeView;

/* a page from an ScePfsPageAllocator, held while in use (names unknown) */
typedef struct ScePfsHashPage {
	ScePfsPageAllocator *pages;
	SceUInt32 reserved;
	SceOff offset;				/* of the page held, -1 = none */
	ScePfsPageAcc acc;
	ScePfsHashList list;
	SceUInt32 reserved2;
} ScePfsHashPage;

typedef struct ScePfsHashNodePage {
	ScePfsPageAllocator *pages;
	SceUInt32 reserved;
	SceOff offset;				/* of the page held, -1 = none */
	ScePfsPageAcc acc;
	ScePfsHashNodeView view;
} ScePfsHashNodePage;

#define PFS_HASH_NODE_DATA		0x10	/* name unknown: the level, before the array */
#define PFS_HASH_NODE_CHILDREN		0x3a4	/* name unknown: the child pages, after the array */
#define PFS_HASH_NODE_ARRAY_SIZE	0x394	/* name unknown: bytes the array may use */

#define SCE_PFS_ERROR_HASH_ARRAY_FULL	0x80140703	/* name unknown */
#define SCE_PFS_ERROR_NODE_PAGE		0x8014040a	/* name unknown: not a node */

static inline SceUInt32 pfsHashNodeChildren(ScePfsHashNodeView *view)	/* name unknown */
{
	return (view->hashes.header->count + 1) / 2;
}

static inline void *pfsHashArrayItem(ScePfsHashArray *array, SceUInt32 index, SceSize itemSize)	/* name unknown */
{
	return (char *)array->items + itemSize * index;
}

#define SCE_PFS_ERROR_UNICV_PAGE_SIZE	0x80140309	/* name unknown */
#define SCE_PFS_ERROR_HASH_LIST_INDEX	0x80140701	/* name unknown */
#define SCE_PFS_ERROR_NODES_LEVEL	0x80140401	/* name unknown */

void pfsHashArrayAttach(ScePfsHashArray *array, void *data);
void pfsHashArrayInit(ScePfsHashArray *array, SceSize itemSize, void *data, SceSize size);
void pfsHashLog2(SceUInt32 x, SceUInt32 *depth, SceUInt32 *rest);
void pfsHashArrayDepth(ScePfsHashArray *array, SceUInt32 *depth, SceUInt32 *rest);
int pfsHashArrayGrow(ScePfsHashArray *array, SceSize size, SceSize itemSize);
int pfsHashArrayShrink(ScePfsHashArray *array);
SceUInt32 pfsHashArrayLeaf(ScePfsHashArray *array, SceUInt32 slot);
void pfsHashArrayRehash(ScePfsHashArray *array, const ScePfsHash *key, SceUInt32 index,
			SceSize itemSize);
int pfsHashArrayCheck(ScePfsHashArray *array, const ScePfsHash *key, SceUInt32 index,
		      SceSize itemSize);
int pfsHashArraySet(ScePfsHashArray *array, const ScePfsHash *key, SceUInt32 slot,
		    const ScePfsHash *hash, SceSize itemSize, int *changed);
int pfsHashArrayGet(ScePfsHashArray *array, const ScePfsHash *key, SceUInt32 slot,
		    ScePfsHash *hash, SceSize itemSize);
int pfsHashArrayPeek(ScePfsHashArray *array, SceUInt32 slot, ScePfsHash *hash, SceSize itemSize);
int pfsHashArrayAppend(ScePfsHashArray *array, const ScePfsHash *key, const ScePfsHash *hash,
		       SceSize size, SceSize itemSize);
int pfsHashArrayRemoveLast(ScePfsHashArray *array, const ScePfsHash *key, SceSize itemSize);
void pfsHashNodeViewAttach(ScePfsHashNodeView *view, void *data);
void pfsHashNodeViewInit(ScePfsHashNodeView *view, void *data, SceSize size);
int pfsHashArrayPush(ScePfsHashArray *array, SceSize size, SceSize itemSize);
int pfsHashArrayPop(ScePfsHashArray *array);
void pfsHashListAttach(ScePfsHashList *list, void *data);
void pfsHashListInit(ScePfsHashList *list, void *data, SceSize size);
int pfsHashListGet(ScePfsHashList *list, SceUInt32 index, ScePfsHash *icv);
int pfsHashListSet(ScePfsHashList *list, SceUInt32 index, const ScePfsHash *icv);
int pfsHashListAppend(ScePfsHashList *list, const ScePfsHash *icv);
int pfsHashListRemoveLast(ScePfsHashList *list);
void pfsHashPageInit(ScePfsHashPage *page, ScePfsPageAllocator *pages);
int pfsHashPageAlloc(ScePfsHashPage *page);
int pfsHashPageLoad(ScePfsHashPage *page, SceOff offset, int raw);
int pfsHashPageRelease(ScePfsHashPage *page);
int pfsHashPageTerm(ScePfsHashPage *page);
int pfsHashPageFree(ScePfsHashPage *page);
int pfsHashNodeViewGetChild(ScePfsHashNodeView *view, const ScePfsHash *key, SceUInt32 slot,
			    ScePfsHash *hash, SceUInt32 *child);
int pfsHashNodePageLoad(ScePfsHashNodePage *page, SceOff offset);
int pfsHashNodeViewGet(ScePfsHashNodeView *view, const ScePfsHash *key, SceUInt32 slot,
		       ScePfsHash *icv);
int pfsHashNodeViewSet(ScePfsHashNodeView *view, const ScePfsHash *key, SceUInt32 slot,
		       const ScePfsHash *icv, int *changed);
int pfsHashNodeViewSetChild(ScePfsHashNodeView *view, const ScePfsHash *key, SceUInt32 slot,
			    const ScePfsHash *hash, SceUInt32 child, int *changed);
int pfsHashNodeViewAppend(ScePfsHashNodeView *view, const ScePfsHash *key,
			  const ScePfsHash *hash, SceUInt32 child);
int pfsHashNodeViewRemoveLast(ScePfsHashNodeView *view, const ScePfsHash *key);
SceUInt32 pfsHashNodeFanout(SceSize pageSize);
int pfsHashNodePageCheck(ScePfsHashNodePage *page);
int pfsHashNodePageAlloc(ScePfsHashNodePage *page);
int pfsHashNodePageFree(ScePfsHashNodePage *page);

/* the record at the start of a file's pages in "unicv.db" (name unknown) */
typedef struct ScePfsUnicvRecord {
	SceUInt64 magic;
	SceUInt32 version;
	SceSize pageSize;
	SceUInt32 hashesPerPage;		/* (pageSize - 0x10) / PFS_HASH_SIZE */
	SceUInt32 count;			/* blocks */
	SceSize blockSize;
	SceUInt8 reserved1C[0x18];		/* not yet traced */
	ScePfsHash seed;			/* name unknown: for the file's keys, version 2 on */
} ScePfsUnicvRecord;

/* a file's chain of pages in "unicv.db" (name unknown) */
typedef struct ScePfsUnicvEntry {
	SceOff offset;
	ScePfsUnicvRecord record;
	SceUInt32 pageHeld;			/* name unknown: page holds a reference */
	SceUInt32 dirty;			/* name unknown: record to be written */
	SceUInt32 pageIndex;			/* name unknown: first icv of the page held */
	ScePfsHash key;
	ScePfsHashPage page;			/* the page of hashes held */
} ScePfsUnicvEntry;

#define PFS_HASH_TREE_MAGIC	0x4244564349454353ULL	/* "SCEICVDB" */
#define PFS_DB_MAGIC		0x534650474e454353ULL	/* "SCENGPFS" */

#define SCE_PFS_ERROR_ICV_HASH		0x80140e02	/* name unknown: an icv tree does not hash */
#define SCE_PFS_ERROR_ICV_EMPTY		0x80140e04	/* name unknown */
#define SCE_PFS_ERROR_ICV_RANGE		0x80140e06	/* name unknown */
#define SCE_PFS_ERROR_ICV_NODE_SIZE	0x80140e09	/* name unknown */
#define SCE_PFS_ERROR_ICV_BROKEN	0x80140e0a	/* name unknown */
#define SCE_PFS_ERROR_ICV_VERSION	0x80140e15	/* name unknown */

/* what the node tree under a hash tree returns (names unknown) */
#define SCE_PFS_ERROR_NODES_HASH	0x80140402
#define SCE_PFS_ERROR_NODES_RANGE	0x80140404
#define SCE_PFS_ERROR_NODE_HASH		0x80140702
#define SCE_PFS_ERROR_NODE_EMPTY	0x80140704
#define SCE_PFS_ERROR_NODE_RANGE	0x80140706

/* the first 0x40 bytes of an icv tree's file (name unknown) */
typedef struct ScePfsHashTreeHeader {
	SceUInt64 magic;			/* PFS_HASH_TREE_MAGIC */
	SceUInt32 version;
	SceSize blockSize;
	SceSize nodeSize;
	SceUInt32 root;				/* of its node tree */
	SceOff freeHead;			/* of its page allocator */
	SceOff end;
	SceUInt32 count;			/* blocks */
	ScePfsHash rootHash;
} ScePfsHashTreeHeader;

/* the nodes of a hash tree, kept in pages (0x8101318c+, name unknown) */
typedef struct ScePfsHashNodes {
	ScePfsPageAllocator *pages;
	SceUInt32 height;			/* name unknown: levels */
	ScePfsHash rootHash;
	SceUInt32 root;				/* page of the root, -1 = none */
	SceUInt32 fanout;			/* icvs per node */
	SceUInt32 held;				/* name unknown: page holds a node */
	SceUInt32 dirty;			/* name unknown: the node held changed */
	SceUInt32 reserved;
	ScePfsHashNodePage page;
	SceUInt32 tail;				/* name unknown: page holds the last leaf */
	SceUInt32 base;				/* name unknown: first icv of the leaf held */
} ScePfsHashNodes;

/* a file's own icv tree, in its "%08x.icv" file (name unknown) */
struct ScePfsHashTree {
	ScePfsHash key;
	ScePfsLowerFile *lf;
	ScePfsLru *lru;
	SceUInt32 dirty;			/* the header is to be written */
	ScePfsPageAllocator pages;
	ScePfsHashNodes nodes;
	ScePfsHashTreeHeader header;
	SceUInt32 reserved;			/* not yet traced */
};

void pfsHashNodesInit(ScePfsHashNodes *nodes, ScePfsPageAllocator *pages);
int pfsHashNodesCreate(ScePfsHashNodes *nodes);
int pfsHashNodesOpen(ScePfsHashNodes *nodes, SceUInt32 root);
int pfsHashNodesFlush(ScePfsHashNodes *nodes, const ScePfsHash *key);
int pfsHashNodesRootHash(ScePfsHashNodes *nodes, const ScePfsHash *key, ScePfsHash *hash);
int pfsHashNodesGet(ScePfsHashNodes *nodes, const ScePfsHash *key, SceUInt32 index,
		    ScePfsHash *icv);
int pfsHashNodesSet(ScePfsHashNodes *nodes, const ScePfsHash *key, SceUInt32 index,
		    const ScePfsHash *icv, int *changed);
int pfsHashNodesAppend(ScePfsHashNodes *nodes, const ScePfsHash *key, const ScePfsHash *icv);
int pfsHashNodesRemoveLast(ScePfsHashNodes *nodes, const ScePfsHash *key);
int pfsHashNodesVisit(ScePfsHashNodes *nodes, SceUInt32 page);
int pfsHashNodesUpdateLast(ScePfsHashNodes *nodes, const ScePfsHash *key, ScePfsHashNodePage *page);
int pfsHashNodesGetLeaf(ScePfsHashNodes *nodes, const ScePfsHash *key, SceUInt32 index,
			ScePfsHashNodePage **page, SceUInt32 *slot);
int pfsHashNodesGetLast(ScePfsHashNodes *nodes, const ScePfsHash *key, ScePfsHashNodePage **page);
int pfsHashNodesWalk(ScePfsHashNodes *nodes, const ScePfsHash *key, SceUInt32 index,
		     SceUInt32 level, ScePfsHashNodePage *page, SceUInt32 *slot, int last);

#define PFS_ICV_FLAT_MAGIC	0x4c4c554e49454353ULL	/* "SCEINULL" */

/* what an "%08x.icv" file without icvs holds (name unknown) */
typedef struct ScePfsIcvFlatRecord {
	SceUInt64 magic;			/* PFS_ICV_FLAT_MAGIC */
	SceUInt32 version;			/* pfsVersionHashParam */
	SceSize blockSize;
	SceUInt32 count;			/* blocks */
	SceUInt32 reserved;
} ScePfsIcvFlatRecord;

typedef struct ScePfsIcvFlat {
	ScePfsHash key;
	ScePfsLowerFile *lf;
	ScePfsIcvFlatRecord record;
	SceUInt32 reserved78[2];
	SceUInt32 dirty;			/* name unknown: record to be written */
} ScePfsIcvFlat;

struct ScePfsCrypt;

/* a file's icv tree and its integrity checks, set up by pfsCryptInit */
typedef struct ScePfsCrypt {			/* name unknown */
	int (*fini)(struct ScePfsCrypt *ctx);
	int (*getIcv)(struct ScePfsCrypt *ctx, SceUInt32 index, ScePfsHash *icv);
	int (*setIcv)(struct ScePfsCrypt *ctx, SceUInt32 index, const ScePfsHash *icv);
	int (*getCount)(struct ScePfsCrypt *ctx, SceUInt32 *count);
	int (*resize)(struct ScePfsCrypt *ctx, SceUInt32 count);
	SceSize (*blockSize)(struct ScePfsCrypt *ctx);
	int (*getHash)(struct ScePfsCrypt *ctx, void *hash);
	void (*hashBlock)(struct ScePfsCrypt *ctx, ScePfsHash *icv, SceUInt32 index,
			  const void *data, SceSize len);
	SceOff (*icvSize)(struct ScePfsCrypt *ctx, SceOff size);
	ScePfsLowerFile *lf;
	ScePfsLru *lru;
	ScePfsHash key;
	SceUInt32 kind;				/* name unknown: 0 to 3 */
	ScePfsUnicv *unicv;			/* kinds 0 and 3 */
	union {
		SceUInt8 state[0xfc];		/* not yet traced */
		ScePfsUnicvEntry entry;		/* kinds 0 and 3 */
		ScePfsHashTree tree;		/* kind 1 */
		ScePfsIcvFlat flat;		/* kind 2 */
	};
} ScePfsCrypt;

/* what a file's blocks are read and written with (name unknown) */
typedef struct ScePfsFileKey {
	const char *klicensee;			/* the pmi's */
	SceUInt32 keyParam;			/* the pmi's a8 */
	SceUInt32 index;			/* the file's files.db index */
	SceUInt16 mode;
	SceUInt16 flags;
	SceUInt16 a6;				/* the pmi's */
	SceUInt16 attr;
	SceInt32 ioPriority;			/* the calling thread's, 0xf by default */
	SceInt32 ioWeight;			/* the calling thread's, 0x100 by default */
	SceUInt32 type;				/* ScePfsModeInfo.type */
	SceOff size;
	SceSize blockSize;
	SceUInt8 dataKey[0x10];
	SceUInt8 tweakKey[0x10];
	ScePfsHash icvKey;
} ScePfsFileKey;

#define PFS_BLOCK_NONE		0	/* names unknown */
#define PFS_BLOCK_WRITE		2
#define PFS_BLOCK_READ		3
#define PFS_BLOCK_RESIZE	4

/* a run of a file's blocks on its way through the crypt engine (name unknown) */
typedef struct ScePfsBlockRequest {
	ScePfsListNode link;			/* in ScePfsVd.pages while mapped */
	SceInt32 op;				/* PFS_BLOCK_*, -1 when freed */
	ScePfsFileKey *key;
	void *dst;				/* reads: the caller's buffer */
	const void *src;			/* writes: the caller's data */
	SceUInt32 headPartial;			/* the first block is partly covered */
	SceUInt32 fullBlocks;			/* blocks covered completely */
	SceOff offset;
	SceSize length;
	SceUInt32 count;			/* blocks */
	SceUInt32 allocated;			/* blocks with buffers */
	SceUInt32 first;			/* first block */
	SceSize offsetInBlock;
	SceSize headLen;			/* of the first block, read before a write */
	SceSize tailLen;			/* of the last block */
	SceSize lastLen;
	SceSize padLen;
	ScePfsHash *icvs;
	void *plain;
	void *disk;
} ScePfsBlockRequest;

/* a request queued for the crypt engine thread (name unknown) */
typedef struct ScePfsCryptJob {
	ScePfsListNode link;
	ScePfsBlockRequest *req;
	int result;
	SceUID thread;
	SceUInt32 waiting;			/* the thread waits on the done list */
} ScePfsCryptJob;

/* PFS ioctls 0x4420 and 0x4421: map file blocks (names unknown) */
typedef struct ScePfsMapArg {
	SceUID fd;				/* the caller's */
	void *buf;				/* where full blocks are decrypted to */
	SceSize length;
	SceUInt32 count;			/* blocks the buffers hold */
	SceOff offset;
	ScePfsHash *icvs;			/* the caller's crypt buffers */
	void *plain;
	void *disk;
	ScePfsFileKey *key;			/* set up on the first map */
} ScePfsMapArg;

/* what ScePfsMgrForKernel_4C148288 reports about a PFS mount */
typedef struct ScePfsMountInfo {
	SceVfsMount *mount;
	const char *path;
	const char *rootPath;
	const char *assignName;
	SceUInt16 mode;
	SceUInt32 quota;
	SceUInt32 version;
	SceInt32 approvedMax;
	SceUInt64 *approved;
	SceInt32 acidDirCount;
	void *acidDirs[16];
} ScePfsMountInfo;

extern const SceVfsOpTable g_pfsRedirectVfsOps;
extern const SceVopTable g_pfsRedirectVops;
extern const SceVfsOpTable g_pfsAcVfsOps;
extern const SceVopTable g_pfsAcVops;
extern SceVfsInfo g_pfsRedirectVfsInfo;
extern SceVfsInfo g_pfsGdsdVfsInfo;
extern SceVfsInfo g_pfsAcVfsInfo;
extern SceUID g_pfsPmiListMutex;
extern ScePfsListNode g_pfsPmiList;
extern ScePfsBufferList g_pfsPmiBufferList;
extern SceInt32 g_pfsBusy;
extern ScePfsBufferList g_pfsFiBufferList;
extern ScePfsBufferList g_pfsVdBufferList;
extern ScePfsBufferList g_pfsLruPoolBufferList;
extern ScePfsBufferList g_pfsLruTblHdrBufferList;

/* pmi.c */
int pfsPmiFind(ScePfsListNode *list, const char *name, ScePfsPmi **pmi);
int pfsPmiListCount(ScePfsListNode *list, int *count);
int pfsPmiCreate(ScePfsPmi **outPmi, const char *path, const char *name, const void *klicensee,
		 SceUInt32 mode);
int pfsPmiDestroy(ScePfsPmi *pmi);
int pfsPmiApprove(ScePfsPmi *pmi, SceUInt64 authId);
int pfsPmiDisapprove(ScePfsPmi *pmi, SceUInt64 authId);
int pfsPmiIsApproved(ScePfsPmi *pmi, SceUInt64 authId);
int pfsPmiInit(void);
void pfsPmiTerm(void);

/* buffer lists */
int pfsBufferListInit(ScePfsBufferList *list, const char *name, SceSize elementSize,
		      SceSize alignment, SceUInt32 count);
void pfsBufferListTerm(ScePfsBufferList *list);
void *pfsBufferListAlloc(ScePfsBufferList *list);
void pfsBufferListFree(ScePfsBufferList *list, void *element);

/* the other parts' setup, in module_start's order */
int pfsGlobalInit(void);			/* name unknown */
void pfsGlobalTerm(void);		/* name unknown */
void pfsRandomInit(SceUInt32 seed);
int pfsFiInit(void);
void pfsFiTerm(void);
int pfsVdInit(void);
void pfsVdTerm(void);
int pfsLruInit(void);
void pfsLruTerm(void);

int pfsErrorToErrno(int error);

/* access checks around every operation (names unknown) */
int pfsModeToAttr(SceUInt32 mode, SceUInt16 *attr);
int pfsAttrToMode(SceUInt16 attr, SceUInt32 *mode);
SceUInt32 pfsAdjustAttr(ScePfsPmi *pmi, SceUInt32 attr);
SceUInt32 pfsCreateAttr(ScePfsPmi *pmi, SceUInt32 mode);
SceUInt32 pfsLowerMode(SceUInt32 mode);
int pfsAccessBegin(ScePfsPmi *pmi, int op, SceUInt32 arg, int *oldPermission);
void pfsAccessEnd(int oldPermission);
const ScePfsModeInfo *pfsModeInfo(SceUInt32 mode);
SceUInt16 pfsModeFsType(SceUInt32 mode);
int pfsModeCheckFsType(SceUInt32 mode, SceUInt32 fsType);
int pfsFsTypeIsX(int fsType);

/* lower.c: the underlying file system */
void pfsDiskSize(ScePfsPmi *pmi, SceUInt32 attr, SceUInt32 icvIndex, SceUInt32 unused,
		 SceUInt64 size, SceUInt64 *diskSize);
void pfsVnodeDiskSize(ScePfsPmi *pmi, SceVfsVnode *vp, SceUInt64 *diskSize);
int pfsPmiDiskSize(ScePfsPmi *pmi, SceUInt64 *diskSize);
int pfsNodeInactive(ScePfsNode *node);
int pfsNodeRelease(ScePfsNode *node);
int pfsNodeLookup(ScePfsNode *node);
int pfsNodeCreate(ScePfsNode *node, int mode);
int pfsLowerOpen(ScePfsLowerFile *lf, ScePfsNode *node, SceUInt32 cookie, int flags);
int pfsLowerClose(ScePfsLowerFile *lf);
int pfsLowerCloseForce(ScePfsLowerFile *lf);
int pfsLowerCloseLocked(ScePfsLowerFile *lf);
int pfsLowerCleanup(ScePfsLowerFile *lf, SceUInt32 a2, SceUInt32 a3);
int pfsLowerPread(ScePfsLowerFile *lf, void *buf, SceSize nbyte, SceOff offset, SceSize *read);
int pfsLowerPwrite(ScePfsLowerFile *lf, const void *buf, SceSize nbyte, SceOff offset,
		   SceSize *written);
int pfsLowerGetSize(ScePfsLowerFile *lf, SceUInt64 *size);
int pfsLowerSetSize(ScePfsLowerFile *lf, SceUInt64 size);

/* write size zero bytes at the start of the file (name unknown) */
static inline int pfsLowerZero(ScePfsLowerFile *lf, SceSize size)
{
	char zero[0x40];
	SceOff offset = 0;
	SceSize n;
	int ret;

	memset(zero, 0, sizeof(zero));
	for (; size != 0; size -= n) {
		ret = pfsLowerPwrite(lf, zero, size < sizeof(zero) ? size : sizeof(zero), offset, &n);
		if (ret != 0)
			return ret;
		offset += n;
	}
	return 0;
}
int pfsNodeRemove(ScePfsNode *dir, ScePfsNode *node);
int pfsNodeMkdir(ScePfsNode *node, int mode);
int pfsNodeRmdir(ScePfsNode *dir, ScePfsNode *node);
int pfsNodeRename(ScePfsNode *oldDir, ScePfsNode *oldNode, ScePfsNode *newNode);
int pfsLowerSync(ScePfsLowerFile *lf);
SceUInt32 pfsLowerCookie(ScePfsLowerFile *lf);

/* lru.c */
void pfsLruInitTable(ScePfsLru *lru, SceUInt32 bucketCount, SceSize pageSize,
		     SceUInt32 entryCount, ScePfsLruEntry **buckets, void *pool);
void pfsLruTermTable(ScePfsLru *lru, ScePfsLruEntry ***buckets, void **pool);
int pfsLruRelease(ScePfsLru *lru, ScePfsLruEntry *e);
int pfsLruGet(ScePfsLru *lru, ScePfsLruEntry **out, ScePfsLowerFile *lf, SceUInt32 unused,
	      SceOff base, SceOff offset);
int pfsLruFlush(ScePfsLru *lru, ScePfsLowerFile *lf, int invalidate);

#define PFS_KERNEL_PID		0x10005		/* name unknown */
#define PFS_OPEN_KERNEL		0x10000000	/* name unknown */

#define SCE_PFS_ERROR_HASH		0x80142002	/* name unknown */
#define SCE_PFS_ERROR_DB_TYPE		0x8014091c	/* name unknown: files.db type too new */
#define SCE_PFS_ERROR_DB_KEY_PARAM	0x80140901	/* name unknown: key params the type lacks */
#define SCE_PFS_ERROR_DB_END		0x80140204	/* name unknown: no more entries */

/* files.db's header, its first 0x160 bytes (name unknown) */
typedef struct ScePfsDbHeader {
	SceUInt64 magic;			/* PFS_DB_MAGIC */
	SceUInt32 type;				/* name unknown: 3, or 5 for some fs types */
	SceUInt16 fsType;
	SceUInt16 a6;				/* the pmi's a6 */
	SceSize pageSize;
	SceUInt32 order;			/* name unknown: of the tree */
	SceUInt32 root;				/* name unknown: the tree's root page */
	SceUInt32 keyParam;			/* the pmi's a8 */
	SceUInt64 freeHead;			/* of its page allocator */
	SceUInt64 end;
	SceUInt64 usage;			/* name unknown: disk bytes the files take */
	ScePfsHash rootHash;			/* name unknown: of the tree */
	ScePfsHash hmac;			/* name unknown: of the header */
	union {					/* not covered by the hmac */
		SceUInt32 dirty;		/* name unknown: set while being changed */
		SceUInt8 signature[0x100];	/* name unknown: RSA-2048, read-only modes */
	};
} ScePfsDbHeader;

#define PFS_DB_HEADER_SECTOR	0x200	/* name unknown: what the header is read in */
#define PFS_DB_NAME_MAX		0x40	/* name unknown */

/* the tree that holds files.db's entries (0x810167a8+, name unknown) */
typedef struct ScePfsDbTable ScePfsDbTable;

typedef struct ScePfsDbTree {
	ScePfsPageAllocator *pages;
	SceUInt32 root;				/* page of the root, -1 = empty */
	SceUInt32 order;			/* name unknown: children of a node */
	ScePfsHash hash;			/* of the root */
	SceVfsMount *mnt;			/* of the underlying file system */
	SceUInt32 type;				/* of files.db */
} ScePfsDbTree;

/* an open files.db (name unknown) */
struct ScePfsDbTable {
	ScePfsHash key;
	ScePfsLowerFile *lf;
	ScePfsLru *lru;
	SceUInt32 reserved;			/* not yet traced */
	ScePfsPageAllocator pages;
	ScePfsDbTree tree;
	SceUInt32 dirty;
	SceUInt32 reserved2;			/* not yet traced */
	ScePfsDbHeader header;
};

/* a files.db entry as its tree keeps it (names unknown) */
typedef struct ScePfsDbEntryName {
	SceUInt32 parentIndex;
	char name[0x40];
} ScePfsDbEntryName;

typedef struct ScePfsDbRecordKey {
	ScePfsDbEntryName id;
	SceUInt32 reserved;
} ScePfsDbRecordKey;

typedef struct ScePfsDbRecord {
	SceUInt32 index;			/* child page in an inner node */
	SceUInt16 attr;
	SceUInt64 size;
} ScePfsDbRecord;

/*
 * A node of the files.db tree, as bpt.c lays one out: order - 1 keys, then
 * order records (the children of an inner node; the last of a leaf links the
 * next leaf), then the hashes of the records (name unknown).
 */
typedef struct ScePfsDbNode {
	SceUInt32 parent;			/* page, -1 = the root */
	SceUInt32 internal;			/* name unknown: 1 = inner node, 0 = leaf */
	SceUInt32 count;			/* keys */
	SceUInt32 reserved;
	ScePfsDbRecordKey keys[];
} ScePfsDbNode;

#define PFS_DB_ENTRY_SIZE	(sizeof(ScePfsDbRecordKey) + sizeof(ScePfsDbRecord) + sizeof(ScePfsHash))

static inline SceSize pfsDbNodeSize(SceUInt32 order)	/* name unknown */
{
	return sizeof(ScePfsDbNode) + order * PFS_DB_ENTRY_SIZE - sizeof(ScePfsDbRecordKey);
}

static inline ScePfsDbRecord *pfsDbNodeRecords(ScePfsDbNode *node, SceUInt32 order)	/* name unknown */
{
	return (ScePfsDbRecord *)((char *)node + offsetof(ScePfsDbNode, keys) +
				  (order - 1) * sizeof(ScePfsDbRecordKey));
}

static inline ScePfsHash *pfsDbNodeHashes(ScePfsDbNode *node, SceUInt32 order)	/* name unknown */
{
	return (ScePfsHash *)((char *)node + offsetof(ScePfsDbNode, keys) +
			      (order - 1) * sizeof(ScePfsDbRecordKey) + order * sizeof(ScePfsDbRecord));
}

/* a page of the files.db tree, held while in use (name unknown) */
typedef struct ScePfsDbNodePage {
	ScePfsPageAllocator *pages;
	SceUInt32 reserved;
	SceOff offset;				/* of the page held, -1 = none */
	ScePfsPageAcc acc;
} ScePfsDbNodePage;

#define SCE_PFS_ERROR_DB_TREE_HASH	0x80140202	/* name unknown */
#define SCE_PFS_ERROR_DB_EXISTS		0x80140205	/* name unknown */
#define SCE_PFS_ERROR_DB_INVALID_ARG	0x80140209	/* name unknown */
#define SCE_PFS_ERROR_DB_NODE		0x8014020a	/* name unknown: not a node */
#define SCE_PFS_ERROR_DB_NAME_TOO_LONG	0x8014021d	/* name unknown */
#define SCE_PFS_ERROR_PAGE_UNUSED	0x80140c09	/* name unknown */

/* sector_crypt.c */
int pfsSectorCryptXtsEncrypt(void *dst, const void *src, const void *key, const void *tweakKey,
			     SceUInt32 keySize, SceUInt32 tweakKeySize, SceUInt32 sectorLow,
			     SceUInt32 sectorHigh, SceUInt32 sectorSize, SceSize size,
			     SceUInt16 flags);
int pfsSectorCryptXtsDecrypt(void *dst, const void *src, const void *key, const void *tweakKey,
			     SceUInt32 keySize, SceUInt32 tweakKeySize, SceUInt32 sectorLow,
			     SceUInt32 sectorHigh, SceUInt32 sectorSize, SceSize size,
			     SceUInt16 flags);
int pfsSectorCryptXtsSwEncrypt(void *dst, const void *src, SceUInt32 keySize,
			       SceUInt32 tweakKeySize, SceUInt32 sectorLow, SceUInt32 sectorHigh,
			       const void *key, const void *tweakKey, SceUInt16 flags);
int pfsSectorCryptXtsSwDecrypt(void *dst, const void *src, SceUInt32 keySize,
			       SceUInt32 tweakKeySize, SceUInt32 sectorLow, SceUInt32 sectorHigh,
			       const void *key, const void *tweakKey, SceUInt16 flags);
void pfsSectorTweakEncrypt(SceUInt32 *dst, const SceUInt32 *src, const SceUInt32 *tweak,
			   SceSize size);
void pfsSectorTweakDecrypt(SceUInt32 *dst, const SceUInt32 *src, const SceUInt32 *tweak,
			   SceSize size);
int pfsSectorCryptCbcEncrypt(void *dst, const void *src, const void *key, const void *iv,
			     SceUInt32 keySize, SceSize size, SceUInt32 flags);
int pfsSectorCryptCbcDecrypt(void *dst, const void *src, const void *key, const void *iv,
			     SceUInt32 keySize, SceSize size, SceUInt32 flags);
int pfsSectorCryptCbcSwEncrypt(void *dst, const void *src, const void *key, const void *iv,
			       SceUInt32 keySize, SceSize size, SceUInt32 flags);
int pfsSectorCryptCbcSwDecrypt(void *dst, const void *src, const void *key, const void *iv,
			       SceUInt32 keySize, SceSize size, SceUInt32 flags);
int pfsSectorCryptEcbEncrypt(void *dst, const void *src, SceSize size, const void *key,
			     SceUInt32 keySize);
int pfsSectorCryptEcbDecrypt(void *dst, const void *src, SceSize size, const void *key,
			     SceUInt32 keySize);
int pfsSectorCryptEcbSlotEncrypt(void *dst, const void *src, SceSize size, const void *key,
				 SceUInt32 keySize, SceUInt16 slot);
int pfsSectorCryptEcbSlotDecrypt(void *dst, const void *src, SceSize size, const void *key,
				 SceUInt32 keySize, SceUInt16 slot);
int pfsSectorCryptCtrEncrypt(void *dst, const void *src, SceSize size, const void *key,
			     SceUInt32 keySize);
int pfsSectorCryptCtrDecrypt(void *dst, const void *src, SceSize size, const void *key,
			     SceUInt32 keySize);
int pfsSectorCryptCtrSlotEncrypt(void *dst, const void *src, SceSize size, const void *key,
				 SceUInt32 keySize, SceUInt16 slot);
int pfsSectorCryptCtrSlotDecrypt(void *dst, const void *src, SceSize size, const void *key,
				 SceUInt32 keySize, SceUInt16 slot);

void pfsDbNodePageInit(ScePfsDbNodePage *page, ScePfsPageAllocator *pages);
int pfsDbNodePageAlloc(ScePfsDbNodePage *page);
int pfsDbNodePageLoad(ScePfsDbNodePage *page, SceOff offset, SceUInt32 order);
int pfsDbNodePageRelease(ScePfsDbNodePage *page);
int pfsDbNodePageFree(ScePfsDbNodePage *page);

int pfsDbTreeDestroy(ScePfsDbTree *tree, SceUInt32 page);
void pfsDbNodeHash(ScePfsDbNode *node, const ScePfsHash *key, ScePfsHash *hash, ScePfsDbTree *tree);
int pfsDbTreeUpdateHashes(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbNodePage *page);
int pfsDbTreeCheckHashes(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbNodePage *page,
			 int *valid);
int pfsDbNodeCreate(const SceUInt32 *order, ScePfsDbNodePage *page, SceUInt32 internal);
int pfsDbTreeInsertIntoParent(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbNodePage *left,
			      ScePfsDbRecordKey *rkey, ScePfsDbNodePage *right);
int pfsDbNodeFind(ScePfsDbTree *tree, ScePfsDbNodePage *page, ScePfsDbRecordKey *rkey,
		  SceUInt32 *slot, int *found);
int pfsDbTreeRemoveEntry(ScePfsDbTree *tree, ScePfsDbNodePage *page, ScePfsDbRecordKey *rkey,
			 int inner);
int pfsDbTreeDeleteEntry(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbNodePage *page);
int pfsDbTreeFindLeaf(ScePfsDbTree *tree, ScePfsDbRecordKey *rkey, ScePfsDbNodePage *page);
int pfsDbTreeHeight(ScePfsDbTree *tree, SceUInt32 *height);
int pfsDbTreeClear(ScePfsDbTree *tree);
int pfsDbTreeFind(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbRecordKey *rkey,
		  ScePfsDbRecord *record, ScePfsHash *hash);
int pfsDbTreeNext(ScePfsDbTree *tree, ScePfsDbRecordKey *after, ScePfsDbRecordKey *rkey,
		  ScePfsDbRecord *record);
int pfsDbTreeInit(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsPageAllocator *pages,
		  SceUInt32 root, SceUInt32 type);
int pfsDbTreeSet(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbRecordKey *rkey,
		 ScePfsDbRecord *record, const ScePfsHash *hash, int mode);
int pfsDbTreeRemove(ScePfsDbTree *tree, const ScePfsHash *key, ScePfsDbRecordKey *rkey);
void pfsHashNodePageInit(ScePfsHashNodePage *page, ScePfsPageAllocator *pages);
int pfsHashNodePageRelease(ScePfsHashNodePage *page);
int pfsHashNodeViewChild(ScePfsHashNodeView *view, SceUInt32 slot, ScePfsHash *hash,
			 SceUInt32 *child);

#define PFS_DB_TREE_INSERT	1	/* name unknown */
#define PFS_DB_TREE_UPDATE	2	/* name unknown */

int pfsDbTableReadInfo(ScePfsLowerFile *lf, SceUInt32 *type, SceUInt32 *keyParam,
		       SceUInt16 *a6);
int pfsDbTableWriteHeader(ScePfsDbTable *table);
int pfsDbTableWriteDirty(ScePfsDbTable *table);
void pfsDbTableInit(ScePfsDbTable *table, ScePfsLru *lru);
void pfsDbTableTerm(ScePfsDbTable *table);
int pfsDbTableOpen(ScePfsDbTable *table, ScePfsLowerFile *lf, const ScePfsHash *key);
int pfsDbTableCreate(ScePfsDbTable *table, ScePfsLowerFile *lf, const ScePfsHash *key,
		     SceUInt16 fsType, SceUInt16 a6, SceUInt32 type, SceUInt32 keyParam);
int pfsDbTableFlush(ScePfsDbTable *table);
int pfsDbTableFind(ScePfsDbTable *table, ScePfsDbKey *key, void *hash, SceUInt32 *index,
		   SceUInt16 *attr, SceUInt64 *size);
int pfsDbTableInsert(ScePfsDbTable *table, ScePfsDbKey *key, const void *hash, SceUInt32 index,
		     SceUInt16 attr, SceUInt64 size);
int pfsDbTableUpdate(ScePfsDbTable *table, ScePfsDbKey *key, const void *hash, SceUInt32 *index,
		     SceUInt16 *attr, SceUInt64 *size);
int pfsDbTableRemove(ScePfsDbTable *table, ScePfsDbKey *key);
int pfsDbTableGetSize(ScePfsDbTable *table, SceUInt64 *size);
int pfsDbTableFirst(ScePfsDbTable *table, SceUInt32 *parentIndex, char *name, SceUInt32 *index,
		    SceUInt16 *attr, SceUInt64 *size);
int pfsDbTableVerify(ScePfsDbTable *table);
int pfsDbTableNext(ScePfsDbTable *table, SceUInt32 *parentIndex, char *name, SceUInt32 *index,
		   SceUInt16 *attr, SceUInt64 *size);

/* files.db (names unknown) */
int pfsDbOpen(ScePfsPmi *pmi, ScePfsNode *node);
int pfsDbCreate(ScePfsPmi *pmi, ScePfsNode *node);
int pfsDbClose(ScePfsPmi *pmi);
int pfsDbGetUsage(ScePfsPmi *pmi, SceUInt64 *usage);
int pfsDbAddUsage(ScePfsPmi *pmi, SceInt64 delta);
int pfsDbGetSize(ScePfsPmi *pmi, SceUInt64 *size);
int pfsDbFind(ScePfsPmi *pmi, ScePfsDbKey *key, void *hash, SceUInt32 *index,
	      SceUInt16 *attr, SceUInt64 *size);
int pfsDbInsert(ScePfsPmi *pmi, ScePfsDbKey *key, const void *hash, SceUInt32 index,
		SceUInt16 attr, SceUInt64 size);
int pfsDbUpdate(ScePfsPmi *pmi, ScePfsDbKey *key, const void *hash, SceUInt16 *attr,
		SceUInt64 *size, SceUInt64 *unused, SceInt64 usageDelta);
int pfsDbRemove(ScePfsPmi *pmi, ScePfsDbKey *key);
int pfsDbCheck(ScePfsPmi *pmi);
int pfsDbDump(ScePfsPmi *pmi, void *buf, SceSize len);

/* the integrity layer (names unknown) */
void pfsHashCopy(void *dst, const void *src);
int pfsSha1(ScePfsHash *digest, const void *data, SceSize len);
int pfsHmacSha1(ScePfsHash *digest, const ScePfsHash *key, const void *data, SceSize len);
int pfsHmacSha1Dma(ScePfsHash *digest, const ScePfsHash *key, const void *data, SceSize len);
int pfsHashEqual(const void *a, const void *b);
int pfsCryptInit(ScePfsCrypt *ctx, ScePfsPmi *pmi, ScePfsLowerFile *icv, SceUInt32 index,
		 SceUInt16 attr, SceUInt64 size, const void *hash);
void pfsCryptClear(ScePfsCrypt *ctx);
int pfsUnicvOpen(ScePfsPmi *pmi, ScePfsNode *node, int create);
int pfsUnicvClose(ScePfsPmi *pmi);
int pfsIndexAlloc(ScePfsPmi *pmi, SceUInt32 *index);
int pfsIndexFree(ScePfsPmi *pmi, SceUInt32 index);
int pfsIcvCreate(ScePfsPmi *pmi, SceUInt32 index, SceUInt16 attr, int mode, void *hash);
int pfsFileRead(ScePfsLowerFile *lf, SceUInt32 index, SceOff size, SceUInt16 attr,
		ScePfsPmi *pmi, ScePfsCrypt *ctx, void *buf, SceSize nbyte, SceOff offset,
		SceSize *read);
int pfsFileWrite(ScePfsLowerFile *lf, SceUInt32 index, SceOff size, SceUInt16 attr,
		 ScePfsPmi *pmi, ScePfsCrypt *ctx, const void *buf, SceSize nbyte, SceOff offset,
		 SceSize *written);
int pfsFileResize(ScePfsLowerFile *lf, SceUInt32 index, SceOff size, SceUInt16 attr,
		  ScePfsPmi *pmi, ScePfsCrypt *ctx, SceOff newSize);
SceUInt32 pfsVersionUnicvParam(SceUInt32 version);
SceUInt32 pfsVersionIcvParam(SceUInt32 version);
SceUInt32 pfsVersionHashParam(SceUInt32 version);
SceUInt32 pfsRandom(void);
int pfsThreadMappingInit(void);
int pfsThreadMappingTerm(void);
ScePfsBlockRequest **pfsThreadMapping(void);
int pfsUnicvInit(ScePfsUnicv *unicv, ScePfsLru *lru, ScePfsLowerFile *lf, SceUInt32 param);
int pfsUnicvTerm(ScePfsUnicv *unicv);
int pfsUnicvAllocIndex(ScePfsUnicv *unicv, SceUInt32 *index);
int pfsUnicvTreeCreate(ScePfsCrypt *ctx, ScePfsUnicv *unicv, SceUInt32 index, SceSize blockSize,
		       const void *key);
int pfsUnicvTreeOpen(ScePfsCrypt *ctx, ScePfsUnicv *unicv, SceUInt32 index, const void *key,
		     SceUInt64 size);
void pfsUnicvTreeTerm(ScePfsCrypt *ctx);
int pfsUnicvTree3Create(ScePfsCrypt *ctx, ScePfsUnicv *unicv, SceUInt32 index, SceSize blockSize);
int pfsUnicvTree3Open(ScePfsCrypt *ctx, ScePfsUnicv *unicv, SceUInt32 index, const void *key,
		      SceUInt64 size);
void pfsUnicvTree3Term(ScePfsCrypt *ctx);

int pfsUnicvReadHeader(ScePfsLowerFile *lf, ScePfsUnicvHeader *header);
int pfsUnicvWriteHeader(ScePfsLowerFile *lf, const ScePfsUnicvHeader *header);
int pfsUnicvSync(ScePfsUnicv *unicv);
int pfsUnicvOpenEntry(ScePfsUnicvEntry *entry, ScePfsUnicv *unicv, SceOff offset);
int pfsUnicvCloseEntry(ScePfsUnicvEntry *entry);

/* page allocator (0x10f00) */
void pfsPageAllocatorGet(ScePfsPageAllocator *pages, SceUInt64 *freeHead, SceUInt64 *end);
void pfsPageAllocatorSet(ScePfsPageAllocator *pages, SceUInt64 freeHead, SceUInt64 end);
void pfsPageAllocatorInit(ScePfsPageAllocator *pages, ScePfsLru *lru, ScePfsLowerFile *lf,
			  SceOff base);
void pfsPageAllocatorTerm(ScePfsPageAllocator *pages);
int pfsFreeRunUnlink(SceOff *other, ScePfsPageAcc *acc);
int pfsFreeRunInsertBefore(ScePfsPageAcc *acc, ScePfsPageAcc *before);
int pfsFreeRunMerge(ScePfsPageAcc *acc, ScePfsPageAcc *next);
int pfsPageAllocatorIsUsed(ScePfsPageAllocator *pages, int *used, SceOff offset);
int pfsPageAllocatorAlloc(ScePfsPageAllocator *pages, SceOff *offset, SceUInt32 count);
int pfsPageAllocatorFree(ScePfsPageAllocator *pages, SceOff offset, SceUInt32 count);

/* unicv entries (0x12168) */
int pfsUnicvEntryWriteRecord(ScePfsUnicvEntry *entry);
int pfsUnicvEntryGetPage(ScePfsUnicvEntry *entry, SceUInt32 index, ScePfsHashPage **page);
int pfsUnicvEntryCreate(ScePfsUnicvEntry *entry, ScePfsPageAllocator *pages, SceUInt32 version);
int pfsUnicvEntryOpen(ScePfsUnicvEntry *entry, ScePfsPageAllocator *pages, SceOff offset,
		      SceUInt32 version);
int pfsUnicvEntryFlush(ScePfsUnicvEntry *entry);
int pfsUnicvEntrySetBlockSize(ScePfsUnicvEntry *entry, SceSize blockSize);
int pfsUnicvEntryClose(ScePfsUnicvEntry *entry);
int pfsUnicvEntryGet(ScePfsUnicvEntry *entry, SceUInt32 index, ScePfsHash *icv);
int pfsUnicvEntrySet(ScePfsUnicvEntry *entry, SceUInt32 index, const ScePfsHash *icv);
int pfsUnicvEntryAppend(ScePfsUnicvEntry *entry, const ScePfsHash *icv);
int pfsUnicvEntryRemoveLast(ScePfsUnicvEntry *entry);

/* hash trees (0x11cb8) */
int pfsHashTreeReadHeader(ScePfsHashTree *tree);
int pfsHashTreeWriteHeader(ScePfsHashTree *tree);
void pfsHashTreeInit(ScePfsHashTree *tree, ScePfsLru *lru);
int pfsHashTreeCreate(ScePfsHashTree *tree, ScePfsLowerFile *lf, const void *key, SceSize blockSize,
		      SceUInt32 version);
int pfsHashTreeOpen(ScePfsHashTree *tree, ScePfsLowerFile *lf, const void *key, SceUInt32 version);
int pfsHashTreeFlush(ScePfsHashTree *tree);
int pfsHashTreeGet(ScePfsHashTree *tree, SceUInt32 index, ScePfsHash *icv);
int pfsHashTreeSet(ScePfsHashTree *tree, SceUInt32 index, const ScePfsHash *icv);
int pfsHashTreeAppend(ScePfsHashTree *tree, const ScePfsHash *icv);
int pfsHashTreeRemoveLast(ScePfsHashTree *tree);
int pfsHashTreeRootHash(ScePfsHashTree *tree, void *hash);

/* blocks (block.c, block_request.c) */
static inline int pfsModeHasFileKeys(SceUInt16 mode)	/* name unknown: modes 2, 3, 10, 11, 13, 32, 33 */
{
	SceUInt16 bit = mode - 2;

	return bit < 0x20 && ((1 << bit) & 0xc0000b03);
}

int pfsBlockWrite(ScePfsBlockRequest *req, ScePfsLowerFile *lf, ScePfsCrypt *ctx);
int pfsBlockReadEdges(ScePfsBlockRequest *req, ScePfsLowerFile *lf, ScePfsCrypt *ctx);
int pfsFileWriteChunk(ScePfsLowerFile *lf, SceUInt32 index, SceOff size, SceUInt16 attr,
		      ScePfsPmi *pmi, ScePfsCrypt *ctx, const void *buf, SceSize nbyte,
		      SceOff offset, SceSize *written);
int pfsBlockRead(ScePfsBlockRequest *req, ScePfsLowerFile *lf, ScePfsCrypt *ctx);
int pfsBlockCopyOut(ScePfsBlockRequest *req, void *buf, SceSize nbyte, SceOff offset,
		    SceSize *read);
void pfsFileKeyInit(ScePfsFileKey *key, SceUInt32 index, SceOff size, SceUInt16 attr,
		    SceUInt32 type, ScePfsCrypt *ctx, ScePfsPmi *pmi);
void pfsFileKeyClear(ScePfsFileKey *key);
void pfsBlockRequestInitRead(ScePfsBlockRequest *req, ScePfsFileKey *key, SceOff offset,
			     SceSize nbyte, void *dst);
void pfsBlockRequestInitWrite(ScePfsBlockRequest *req, ScePfsFileKey *key, SceOff offset,
			      SceSize nbyte, const void *src);
void pfsBlockRequestInitResize(ScePfsBlockRequest *req, ScePfsFileKey *key, SceOff newSize,
			       SceOff *reached);
int pfsBlockRequestAlloc(ScePfsBlockRequest *req);
int pfsBlockRequestFree(ScePfsBlockRequest *req);

/* crypt engine (0xbca0) */
int pfsCryptEngineInit(void);
void pfsCryptEngineTerm(void);
int pfsCryptEngineSubmit(ScePfsCryptJob *job);
int pfsCryptEngineCancel(ScePfsCryptJob *job);
int pfsCryptEngineWait(ScePfsCryptJob *job);
void pfsCryptEngineProcess(ScePfsCryptJob *job);

/* crypt buffers (0x11614) */
int pfsCryptBufferInit(void);
void pfsCryptBufferTerm(void);
#define PFS_CRYPT_BUF_EXACT	0	/* name unknown: fail when count are not free */
#define PFS_CRYPT_BUF_WAIT	1	/* name unknown: wait until they are */
#define PFS_CRYPT_BUF_PARTIAL	2	/* name unknown: take as many as are free */

int pfsCryptBufferAlloc(SceInt32 weight, SceInt32 priority, SceUInt32 count, void **plain,
			void **disk, ScePfsHash **icvs, int mode);
int pfsCryptBufferFree(SceInt32 weight, SceInt32 priority, SceUInt32 count, void *plain,
		       void *disk, ScePfsHash *icvs);
SceUInt32 pfsCryptBufferMaxBlocks(SceInt32 weight, SceInt32 priority);

/* file keys (crypto.c): deferred */
void pfsDeriveFileKeys(void *dataKey, void *tweakKey, const char *klicensee, SceUInt32 keyParam,
		       SceUInt16 flags, SceUInt32 index, SceUInt16 a6);
void pfsDeriveFileKeysSeeded(void *dataKey, void *tweakKey, const char *klicensee,
			     SceUInt16 flags, SceUInt16 a6, const ScePfsHash *seed, SceSize seedSize);
void pfsDeriveSimpleKeys(void *dataKey, void *tweakKey, const char *klicensee,
			 SceUInt32 keyParam, SceUInt32 index);

/* hashes (0xdf28) */
void pfsHashZero(ScePfsHash *hash);
int pfsIcvTreeCreate(ScePfsCrypt *ctx, ScePfsLru *lru, ScePfsLowerFile *lf, SceSize blockSize,
		     const void *key, SceUInt32 version);
int pfsIcvTreeOpen(ScePfsCrypt *ctx, ScePfsLru *lru, ScePfsLowerFile *lf, const void *key,
		   SceUInt64 size, SceUInt32 param);
int pfsIcvFlatCreate(ScePfsCrypt *ctx, ScePfsLowerFile *lf, SceSize blockSize, const void *key,
		     SceUInt32 version);
int pfsIcvFlatOpen(ScePfsCrypt *ctx, ScePfsLowerFile *lf, const void *key, SceUInt64 size,
		   SceUInt32 version);
int pfsPathResolve(ScePfsPmi *pmi, ScePfsVd *vd, const char *path);
/* the vfs operations */
#define SCE_PFS_ERROR_PASS_THROUGH	0x80142301	/* name unknown: returned as it is */

static inline ScePfsPmi *pfsVnodePmi(SceVfsVnode *vp)	/* name unknown */
{
	return (ScePfsPmi *)vp->core.mnt->pData;
}

static inline int pfsPmiIsMounted(ScePfsPmi *pmi)	/* name unknown */
{
	return pmi != NULL && !(pmi->flags & PFS_PMI_NOT_MOUNTED);
}

/* ac_vfs.c */
#define PFS_MODE_ACID_DIR	0x1f	/* name unknown: an acid directory not mounted on its own */
#define PFS_ATTR_ACID_DIR	0x1000	/* name unknown: a directory that holds one acid */
#define PFS_ATTR_DIR		0x8000
#define PFS_MODE_ACID		0x20	/* name unknown: a mounted acid directory */
#define PFS_MODE_ACID_SEALED	0x21	/* name unknown */
#define SCE_PFS_ERROR_ACID_TOO_MANY	0x80142203	/* name unknown */
#define SCE_PFS_ERROR_ACID_EXISTS	0x80142205	/* name unknown */
#define SCE_PFS_ERROR_ACID_SEALED	0x8014220c	/* name unknown: already sealed */
#define SCE_PFS_ERROR_ACID_NOT_DIR	0x80142211	/* name unknown */
#define SCE_PFS_ERROR_ACID_BUSY		0x80142218	/* name unknown: files still open */
#define SCE_PFS_ERROR_NO_ACID	0x80142204	/* name unknown */
#define SCE_PFS_ERROR_INVALID_ARG	0x80142209	/* name unknown */
#define SCE_PFS_ERROR_ACID_CROSS	0x80142217	/* name unknown: rename out of an acid directory */
#define PFS_DEVCTL_GET_ACID_DIRS	0x4413	/* name unknown */
#define PFS_IOCTL_ACID_DIR_SET		0x4401	/* name unknown */
#define PFS_IOCTL_ACID_DIR_MOUNT	0x4402	/* name unknown */
#define PFS_IOCTL_ACID_DIR_SEAL		0x4403	/* name unknown */
#define PFS_IOCTL_ACID_DIR_UMOUNT	0x4404	/* name unknown */

/* what PFS_DEVCTL_GET_ACID_DIRS returns */
typedef struct ScePfsAcidDirs {
	SceUInt32 count;
	char path[16][0x40];
} ScePfsAcidDirs;

/* acid directories (0x1088c+) */
int pfsAcidFindLocked(ScePfsPmi **acid, ScePfsPmi *pmi, SceVfsVnode *vp);
int pfsAcidFind(ScePfsPmi **acid, ScePfsPmi *pmi, SceVfsVnode *vp);
int pfsAcidDirSet(ScePfsPmi *pmi, SceVfsVnode *vp);
int pfsAcidDirMount(ScePfsPmi *pmi, SceVfsVnode *vp, const void *klicensee, SceSize len);
int pfsAcidGetDirs(ScePfsPmi *pmi, ScePfsAcidDirs *dirs);
int pfsAcidDirSeal(ScePfsPmi *pmi, SceVfsVnode *vp);
int pfsAcidDirUmount(ScePfsPmi *pmi, SceVfsVnode *vp);

/* redirect layer (0xeae8+): a pmi without files.db of its own */
int pfsRedirectVnodeCreate(SceVfsVnode **vpp, SceVfsVnode *dvp, SceVfsVnode *lowerVp,
			   SceUInt16 attr);
int pfsRedirectSetRoot(ScePfsPmi *pmi, SceVfsMount *mnt, int unk, SceVfsVnode *vp);
int pfsRedirectDevctl(ScePfsPmi *pmi, SceVfsMount *mnt, const char *dev, unsigned int cmd,
		      const void *arg, SceSize argLen, void *buf, SceSize bufLen);
int pfsRedirectLookup(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		      int flags);
int pfsRedirectOpen(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceVfsPath *path,
		    int flags);
int pfsRedirectCreate(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		      int flags, int mode);
int pfsRedirectClose(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file);
int pfsRedirectRead(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, void *buf, SceSize nbyte);
int pfsRedirectWrite(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, const void *buf,
		     SceSize nbyte);
int pfsRedirectPread(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, void *buf, SceSize nbyte,
		     SceOff offset);
int pfsRedirectPwrite(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, const void *buf,
		      SceSize nbyte, SceOff offset);
SceOff pfsRedirectLseek(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceOff offset,
			int whence);
int pfsRedirectIoctl(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, int cmd, const void *in,
		     SceSize inLen, void *out, SceSize outLen);
int pfsRedirectInactive(SceVfsVnode *vp);
int pfsRedirectRemove(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path,
		      int flags);
int pfsRedirectMkdir(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		     int mode);
int pfsRedirectRmdir(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path);
int pfsRedirectDopen(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceVfsFile *file);
int pfsRedirectDread(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoDirent *dir);
int pfsRedirectDclose(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file);
int pfsRedirectGetstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat);
int pfsRedirectChstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat, int bit);
int pfsRedirectFgetstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat);
int pfsRedirectFchstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat,
		       int bit);
int pfsRedirectRename(ScePfsPmi *pmi, SceVfsVnode *odvp, SceVfsVnode *ovp, SceVfsPath *oldPath,
		      SceVfsVnode *ndvp, SceVfsVnode **nvpp, SceVfsPath *newPath);
int pfsRedirectVnodeSync(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, int flags);
int pfsRedirectWhiteout(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *oldPath,
			SceVfsPath *newPath, SceUInt32 a4, SceUInt32 a5);
int pfsRedirectCleanup(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceUInt32 a2,
		       SceUInt32 a3);

/* crypto.c: deferred */
void pfsDeriveKey(ScePfsHash *key, const void *klicensee, SceUInt32 keyParam, SceUInt32 flags,
		  SceUInt32 index, SceUInt32 keyParam2);

/* the file system proper (names unknown) */
int pfsCoreFind(ScePfsPmi *pmi, ScePfsNode *node, SceUInt32 *index, SceUInt16 *attr,
		SceUInt64 *size);
int pfsVnodeCreate(SceVfsVnode **vpp, SceVfsVnode *dvp, SceVfsVnode *lowerVp,
		   SceVfsVnode *icvVp, SceUInt32 index, SceUInt32 parentIndex, SceUInt16 attr,
		   SceUInt32 unused, SceUInt64 size);
int pfsCoreCreateEntry(ScePfsPmi *pmi, ScePfsNode *node, int mode, int icvMode,
		       SceUInt32 *index, SceUInt16 attrFlags, SceUInt16 *attr);
int pfsCoreMap(ScePfsPmi *pmi, SceVfsVnode *vp, ScePfsFi *fi, int cmd, ScePfsMapArg *arg,
	       ScePfsBlockRequest *map);
int pfsCoreReadData(ScePfsFi *fi, ScePfsPmi *pmi, void *buf, SceSize nbyte, SceOff offset,
		    SceSize *read);
int pfsCoreUpdateSize(ScePfsPmi *pmi, SceVfsVnode *vp);
int pfsCoreRemoveEntry(ScePfsPmi *pmi, SceVfsVnode *dvp, ScePfsNode *node, SceUInt32 index,
		       SceUInt16 attr);
int pfsCoreMount(ScePfsPmi *pmi, ScePfsNode *root);
int pfsCoreUmount(ScePfsPmi *pmi, int flags);
int pfsCoreSync(ScePfsPmi *pmi, int flags);
int pfsCoreLookup(ScePfsPmi *pmi, ScePfsNode *node, SceUInt32 flags);
int pfsCoreCreate(ScePfsPmi *pmi, ScePfsNode *node, int mode, SceUInt32 icvMode);
int pfsCoreOpen(ScePfsFi *fi, ScePfsPmi *pmi, ScePfsNode *node, int flags);
int pfsCoreClose(ScePfsFi *fi, ScePfsPmi *pmi);
int pfsCoreWrite(ScePfsFi *fi, ScePfsPmi *pmi, const void *buf, SceSize nbyte, SceOff offset,
		 SceSize *written);
int pfsCoreLseek(ScePfsFi *fi, ScePfsPmi *pmi, SceOff offset, int whence, SceOff *position);
int pfsCoreRemove(ScePfsPmi *pmi, ScePfsNode *dir, ScePfsNode *node);
int pfsCoreMkdir(ScePfsPmi *pmi, ScePfsNode *node, int mode);
int pfsCoreRmdir(ScePfsPmi *pmi, ScePfsNode *dir, ScePfsNode *node);
int pfsCoreRename(ScePfsPmi *pmi, ScePfsNode *oldDir, ScePfsNode *oldNode, ScePfsNode *newNode);
int pfsCoreGetAttr(ScePfsPmi *pmi, ScePfsNode *node, SceUInt16 *attr, SceUInt64 *size);
int pfsCoreReadInternal(ScePfsFi *fi, ScePfsPmi *pmi, void *buf, SceSize nbyte, SceOff offset,
			SceSize *read);
int pfsCoreTruncate(ScePfsFi *fi, ScePfsPmi *pmi, SceOff size);
int pfsCoreSetAttr(ScePfsPmi *pmi, ScePfsNode *node, SceUInt32 attr);
int pfsCoreVnodeSync(ScePfsFi *fi, ScePfsPmi *pmi, int flags);
int pfsCoreDopen(ScePfsFi *fi, ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path);
int pfsCoreDread(ScePfsFi *fi, ScePfsPmi *pmi, SceIoDirent *dir, int a3);
int pfsCoreDclose(ScePfsFi *fi, ScePfsPmi *pmi);
int pfsCoreDevctl(ScePfsPmi *pmi, const char *dev, unsigned int cmd, const void *arg,
		  SceSize argLen, void *buf, SceSize bufLen);
int pfsCoreWhiteout(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *oldPath,
		    SceVfsPath *newPath, SceUInt32 a4, SceUInt32 a5);
int pfsCoreIoctl(ScePfsPmi *pmi, SceVfsVnode *vp, ScePfsFi *fi, int cmd, const void *in,
		 SceSize inLen, void *out, SceSize outLen);
int pfsCoreCleanup(ScePfsFi *fi, ScePfsPmi *pmi, SceUInt32 a2, SceUInt32 a3);
int pfsCoreRead(ScePfsFi *fi, ScePfsPmi *pmi, void *buf, SceSize nbyte, SceOff offset,
		SceSize *read);

/* gdsd.c: the gamedata/savedata pseudo drive */
extern const SceVfsOpTable g_pfsGdsdVfsOps;
extern const SceVopTable g_pfsGdsdVops;
int pfsGdsdDevctl(ScePfsPmi *pmi, SceVfsMount *mnt, const char *dev, unsigned int cmd,
		  const void *arg, SceSize argLen, void *buf, SceSize bufLen);
int pfsGdsdMount(ScePfsPmi *pmi, SceVfsMount *mnt, SceVfsPath *devFilePath);
int pfsGdsdUmount(ScePfsPmi *pmi, SceVfsMount *mnt, int flags);
int pfsGdsdSync(ScePfsPmi *pmi, SceVfsMount *mnt, int flags);
int pfsGdsdSetRoot(ScePfsPmi *pmi, SceVfsMount *mnt, int unk, SceVfsVnode *vp);
int pfsGdsdDecodePathElem(ScePfsPmi *pmi, SceVfsMount *mnt, const char *path, const char **path2,
			  const char **path3, char *buf, SceSize bufLen, SceSize *decodeLen);
int pfsGdsdLookup(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		  SceUInt32 flags);
int pfsGdsdOpen(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceVfsPath *path, int flags);
int pfsGdsdCreate(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path,
		  int flags, int mode);
int pfsGdsdClose(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file);
int pfsGdsdRead(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, void *buf, SceSize nbyte);
int pfsGdsdWrite(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, const void *buf,
		 SceSize nbyte);
int pfsGdsdPread(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, void *buf, SceSize nbyte,
		 SceOff offset);
int pfsGdsdPwrite(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, const void *buf,
		  SceSize nbyte, SceOff offset);
SceOff pfsGdsdLseek(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceOff offset, int whence);
int pfsGdsdIoctl(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, int cmd, const void *in,
		 SceSize inLen, void *out, SceSize outLen);
int pfsGdsdInactive(SceVfsVnode *vp);
int pfsGdsdRemove(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path,
		  SceUInt32 flags);
int pfsGdsdMkdir(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode **vpp, SceVfsPath *path, int mode);
int pfsGdsdRmdir(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *path);
int pfsGdsdDopen(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceVfsFile *file);
int pfsGdsdDread(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoDirent *dir);
int pfsGdsdDclose(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file);
int pfsGdsdGetstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat);
int pfsGdsdChstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsPath *path, SceIoStat *stat, int bit);
int pfsGdsdFgetstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat);
int pfsGdsdFchstat(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceIoStat *stat,
		   SceUInt32 bit);
int pfsGdsdRename(ScePfsPmi *pmi, SceVfsVnode *odvp, SceVfsVnode *ovp, SceVfsPath *oldPath,
		  SceVfsVnode *ndvp, SceVfsVnode **nvpp, SceVfsPath *newPath);
int pfsGdsdVnodeSync(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, int flags);
int pfsGdsdWhiteout(ScePfsPmi *pmi, SceVfsVnode *dvp, SceVfsVnode *vp, SceVfsPath *oldPath,
		    SceVfsPath *newPath, SceUInt32 a4, SceUInt32 a5);
int pfsGdsdCleanup(ScePfsPmi *pmi, SceVfsVnode *vp, SceVfsFile *file, SceUInt32 a2,
		   SceUInt32 a3);

#endif
