/*
 * SceKernelModulemgr internals shared by the module's sources.
 *
 * Source files follow Sony's (from the 4.00 debug records): loadelf.c,
 * relocator.c, modulemgr.c, bootimage_fs.c, modulecb.c, libentcb.c,
 * libstubcb.c, librarydb.c, self_decrypter.c, loadcore.c, stub_linker.c,
 * syscallcb.c, backtrace.c and the syscall wrappers (syscall.c).
 */

#ifndef MODULEMGR_INTERNAL_H
#define MODULEMGR_INTERNAL_H

#include <scetypes.h>
#include <kernel/error.h>
#include <sysmem/spinlock.h>
#include <authmgr/sblauthmgr.h>
#include <threadmgr/threadmgr.h>
#include <sysmem/sysmem.h>
#include <processmgr/process.h>
#include <modulemgr/modulemgr_patch.h>
#include <modulemgr/modulemgr.h>
#include <iofilemgr/iofilemgr.h>
#include <modulemgr/module_library.h>
#include <modulemgr/elf.h>
#include <sysmem/debug.h>
#include <sysmem/dipsw.h>
#include <sysmem/utils.h>
#include <sysmem/qafmgr.h>
#include <sysmem/suspend.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysroot.h>
#include <processmgr/processmgr.h>
#include <acmgr/acmgr.h>
#include <intrmgr/intrmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/procevent.h>
#include <excpmgr/excpmgr.h>
#include <modulemgr/modulemgr_control.h>
#include <sysmem/sysroot_modulemgr.h>
#include <sysmem/uid_guid.h>

/* the kernel heap; the debug build calls the heap API directly */
#if FW_DEBUG
#define ModulemgrKernelAlloc(size)	SceSysmemForDriver_B415B5A8(0x1000b, (size), NULL, \
						    __builtin_return_address(0))
#define ModulemgrKernelFree(ptr)	sceKernelFreeHeapMemory(0x1000b, (ptr))
#else
#define ModulemgrKernelAlloc(size)	SceSysmemForKernel_85571907(size)
#define ModulemgrKernelFree(ptr)	SceSysmemForKernel_4233C16D(ptr)
#endif

/* Sony's debugger and tool entry points are laid out as rarely executed code: no loop
 * header copy, blocks in source order (measured on the originals, 2026-10-20) */
#define MODULEMGR_COLD			__attribute__((cold, section(".text")))

#define PAGE_TRUNC(x)			((x) / 0x1000 * 0x1000)
#define PAGE_ROUND(x)			PAGE_TRUNC((x) + 0xfff)

/* SceModuleCB.flags */
#define MODULE_FLAG_TYPE_MASK		0x000f	/* 1: user library?, 2: (name unknown) */
#define MODULE_FLAG_SHARED_COPY		0x0100	/* text shared with the original copy */
#define MODULE_FLAG_SHARED_ORIGINAL	0x0200	/* the original of a shared module */
#define MODULE_FLAG_SHARED_TOOL		0x0400	/* shared copy, tool mode (dipsw 0xd2) */
#define MODULE_FLAG_SHARED_MASK		0x0500	/* uses another module's image */
#define MODULE_FLAG_SHARED_ANY		0x0700
#define MODULE_FLAG_1000		0x1000	/* load option 0x8000 */
#define MODULE_FLAG_NO_STOP		0x2000	/* exports a library with attr 0x4000 */
#define MODULE_FLAG_PROCESS_IMAGE	0x4000
#define MODULE_FLAG_SELF_FIXED		0x8000	/* ELF type 0xfe04 */

/* SceModuleCB.status */
#define MODULE_STATUS_NONE		0
#define MODULE_STATUS_LOADED		1
#define MODULE_STATUS_STARTING		2
#define MODULE_STATUS_STARTED		3
#define MODULE_STATUS_STOPPING		4
#define MODULE_STATUS_STOPPED		5
#define MODULE_STATUS_START_SUSPENDED	0x10

/* SceModuleCB.segment[].perms */
#define SEGMENT_PERM_WRITE		0x2
#define SEGMENT_PERM_MASK		0x7
#define SEGMENT_PERM_TEXT		0x5	/* R-X */

/* LibraryDB.flags */
#define LIBDB_FLAG_SHARED		0x0001
#define LIBDB_FLAG_ASLR			0x0002	/* name unknown */
#define LIBDB_FLAG_INHIBIT_MASK		0x0030
#define LIBDB_FLAG_PCSG00307		0x8000

/* exported library attributes */
#define LIBRARY_ATTR_NOLINK		0x0004
#define LIBRARY_ATTR_NID_CHECKED	0x0004	/* LibEntCB.flags: entries checked */
#define LIBRARY_ATTR_SYSCALL		0x4000
#define LIBRARY_ATTR_8000		0x8000

/* loader option flags (ModulemgrLoadModule) */
#define LOAD_FLAG_SHARED		0x00000001
#define LOAD_FLAG_USE_PID		0x00000002
#define LOAD_FLAG_PROCESS_IMAGE		0x00000004
#define LOAD_FLAG_TYPE1			0x00000010
#define LOAD_FLAG_TYPE2			0x00000020
#define LOAD_FLAG_NO_ENTRY		0x00000100
#define LOAD_FLAG_NO_AUTH		0x00000800
#define LOAD_FLAG_1000			0x00001000
#define LOAD_FLAG_4000			0x00004000
#define LOAD_FLAG_8000			0x00008000
#define LOAD_FLAG_TIMING		0x00010000
#define LOAD_FLAG_FOR_PID		0x08000000	/* entry points get the arguments of a user call */

struct SceModuleCB;
struct LibEntCB;
struct SceKernelBacktraceFrame;
struct SceKernelBacktraceFrameForKernel;
struct LibStubCB;

/*
 * SCE module info (version 6), as found in the ELF; the field names are the labels
 * Sony's ModuleCBDumpInfo prints it with ("libent_top : ...", "modver[0]", ...)
 */
typedef struct SceModuleInfo {
	SceUInt16 modattr;
	SceUInt8 modver[2];
	char name[27];
	SceUInt8 infover;		/* 6 */
	SceUInt32 gp;
	SceUInt32 libent_top;		/* segment index << 30 | offset */
	SceUInt32 libent_btm;
	SceUInt32 libstub_top;
	SceUInt32 libstub_btm;
	SceUInt32 fingerprint;
	SceUInt32 tlsStart;
	SceUInt32 tlsFilesz;
	SceUInt32 tlsMemsz;
	SceUInt32 start_entry;
	SceUInt32 stop_entry;
	SceUInt32 exidx_top;
	SceUInt32 exidx_btm;
	SceUInt32 extab_top;
	SceUInt32 extab_btm;
} SceModuleInfo;

/* imported library entry, normalised 0x24-byte form */
typedef struct SceModuleImports {
	SceUInt8 size;
	SceUInt8 auxattr;
	SceUInt16 version;
	SceUInt16 attr;
	SceUInt16 nfunc;
	SceUInt16 nvar;
	SceUInt16 ntls;
	SceNID libnid;
	const char *libname;
	SceNID *funcnid;
	void **funcent;
	SceNID *varnid;
	void **varent;
} SceModuleImports;

/* imported library entry, 0x34-byte form */
typedef struct SceModuleImports34 {
	SceUInt8 size;
	SceUInt8 auxattr;
	SceUInt16 version;
	SceUInt16 attr;
	SceUInt16 nfunc;
	SceUInt16 nvar;
	SceUInt16 ntls;
	SceUInt32 reserved1;
	SceNID libnid;
	const char *libname;
	SceUInt32 reserved2;
	SceNID *funcnid;
	void **funcent;
	SceNID *varnid;
	void **varent;
	SceNID *tlsnid;
	void **tlsent;
} SceModuleImports34;

/* entry of the list of original shared modules (kernel heap) */
typedef struct SharedModuleEntry {
	struct SharedModuleEntry *next;
	struct SceModuleCB *cb;
	int refCount;
	void *dataCopy;			/* copy of the last (data) segment */
} SharedModuleEntry;

typedef struct SceModuleProbesInfo {
	SceUInt32 version;		/* 1 */
	SceUInt32 count;
} SceModuleProbesInfo;

typedef struct SceModuleObject {
	SceUInt32 base[2];		/* SceObjectBase */
	SceModuleCB cb;
} SceModuleObject;

typedef struct LibStubCB {
	struct LibStubCB *next;		/* client / lost client list */
	SceUID guid;
	SceModuleImports *imports;
	LibEntCB *linked;
	SceModuleCB *cb;
	void **toolCopy;		/* funcent, funcnid, varent, varnid (tool heap) */
} LibStubCB;

/* UID class objects */
typedef struct SceLibraryObject {
	SceUInt32 base[2];		/* SceObjectBase */
	LibEntCB *libEntCB;
	SceUID modid;
} SceLibraryObject;

typedef struct SceLibStubObject {
	SceUInt32 base[2];		/* SceObjectBase */
	SceUID modid;
	SceUInt16 index;
	SceUInt16 reserved;
} SceLibStubObject;

/* page -> module tables of a process (tool mode) */
typedef struct Addr2ModExidx {
	void *top;
	void *btm;
} Addr2ModExidx;

typedef struct Addr2ModTable {
	SceModuleCB **table;
	SceUID tableBlock;
	SceUID exidxBlock;
	SceUID exidxKernelBlock;
	Addr2ModExidx *exidx;		/* per page of the kernel range */
} Addr2ModTable;

typedef struct LibraryDB {
	SceUID pid;
	LibEntCB *libList;
	SceUInt16 libCount;
	SceUInt16 lostCount;
	LibStubCB *lostClients;
	SceModuleCB *modules;
	SceUID mainModid;
	SceUInt16 moduleCount;
	SceUInt16 flags;
	Addr2ModTable *addr2mod;
	SceKernelSpinlock lock;
} LibraryDB;

typedef struct ModulemgrPrivate {
	SceClass moduleClass;
	SceClass libraryClass;
	SceClass stubClass;
	LibraryDB kernelDB;
	SceUID kblModid;		/* stopped by sceKernelFinalizeKbl */
} ModulemgrPrivate;

/* loader state passed through loadelf.c */
typedef struct LoadelfSegment {
	SceUInt32 vaddr;		/* p_vaddr */
	SceUID tmpBlock;		/* kernel mapping of a user segment */
	void *kaddr;
} LoadelfSegment;

typedef struct LoadelfContext {
	SceModuleCB *cb;
	SceUInt32 entry;		/* e_entry: module info, segment << 30 | offset */
	SceSize linkInfoUnk;		/* name unknown; leading part of linkInfo */
	SceSize libentSize;		/* export table copy */
	SceSize libEntCBSize;
	SceSize libstubOffset;
	SceSize libstubSize;		/* normalised import table */
	SceSize libStubCBSize;
	LoadelfSegment seg[3];
} LoadelfContext;

typedef struct LoadelfTiming {
	SceUInt32 start;
	SceSize totalSize;
	SceSize loadedSize;
#if FW_DEBUG
	SceUInt32 loadUsec;
	SceUInt32 readUsec;
	SceUInt32 relocUsec;
#endif
	const char *path;
} LoadelfTiming;

/* the SELF/ELF parser (Sony's self_parser.h: SelfParserOpen, SelfParserGetPhdrOffset) */
typedef struct SelfParser {
	void *header;			/* the first page of the file */
	SceSize headerSize;
	void *ehdr;
	void *phdr;			/* Elf32_Phdr[] */
	SceUInt8 type;			/* 1: ELF, 2: SELF */
	SceUInt8 authenticated;		/* the SELF header went through the auth manager */
	SceUInt8 padding[2];
	void *segmentInfo;		/* SELF segment info table */
	void *heap;			/* allocation holding header */
	int authContext;
	SelfAuthRequest *auth;
	SceUID fd;
	SceUID pid;
	SceUInt32 reserved;
} SelfParser;

/* bootimage_fs.c */
typedef struct BootimageEntry {
	const char *name;
	void *data;
	SceSize size;
} BootimageEntry;

typedef struct BootimageTable {
	int count;
	BootimageEntry *entry;
} BootimageTable;

typedef struct BootimageFS {
	SceUID modid;
	int current;			/* the open entry, or -1 */
	BootimageTable table;
} BootimageFS;

#define BOOTIMAGE_FD		0x7f7f7f7f

/* the per-thread state of the module manager (TLS slot) */
typedef struct ModulemgrThreadState {
	SceUInt64 time;
	SceUID thid;
	SceUID modid;
	SceUInt32 flags;
	int result;
} ModulemgrThreadState;

#define THREAD_STATE_LOAD_RESUMED	0x001	/* the load finished; notify the debugger */
#define THREAD_STATE_LOAD_SUSPENDED	0x002	/* the load finished while suspended */
#define THREAD_STATE_RETRY_START	0x010	/* call the entry point again */
#define THREAD_STATE_RETRY_WAIT		0x020	/* wait for the entry thread again */
#define THREAD_STATE_START_RESULT	0x100	/* result holds the start result */
#define THREAD_STATE_START_SUSPEND	0x200	/* report the failed start first */

/* start_thread / stop_thread parameter of a user module */
typedef struct SceModuleThreadParam {
	SceUInt32 numParams;		/* 4 */
	SceInt32 priority;
	SceSize stackSize;
	SceUInt32 attr;
	SceInt32 cpuAffinityMask;
} SceModuleThreadParam;

/* SceProcessParam of a process image (up to 0x34 bytes) */
typedef struct SceProcessParam {
	SceSize size;
	SceUInt32 magic;		/* "PSP2" */
	SceUInt32 version;
	SceUInt32 sdkVersion;
	const char *mainThreadName;
	SceInt32 *mainThreadPriority;
	SceSize *mainThreadStackSize;
	SceUInt32 *mainThreadAttr;
	const char *processName;
	SceUInt32 *processPreloadDisabled;
	SceInt32 *mainThreadCpuAffinityMask;
	void *libcParam;
	SceKernelThreadOptParam *mainThreadOptParam;
} SceProcessParam;

#define SCE_PROCESS_PARAM_MAGIC		0x32505350

/* one module sceKernelLoadPreloadingModules loads into a process */
typedef struct PreloadModule {
	const char *depend;		/* needed library, or NULL */
	const char *hostPath[2];	/* tried first with host0: access */
	const char *path;
	const char *appPath;		/* loaded instead for (non)game programs */
	const char *sdPath;		/* tried first with external storage */
	const char *os0Path;
	SceUInt32 inhibitMask;		/* skipped when set in the process info flags */
	SceUInt32 flags;		/* PRELOAD_* */
} PreloadModule;

#define PRELOAD_SHARED			0x00000002
#define PRELOAD_TOOL_SHARED		0x00000008	/* load flag 0x20 with dipsw 0xd2 */
#define PRELOAD_STAGE1			0x00000010
#define PRELOAD_STAGE0			0x00000020
#define PRELOAD_DEVELOPMENT		0x00000040
#define PRELOAD_USER_PA_MAPPING		0x00000080
#define PRELOAD_ERROR_SHIFT		28

/* info of the library function a stub address calls (sysmem/sysroot_modulemgr.h) */
typedef struct ModulemgrStubInfo ModulemgrStubInfo;

/* globals */
extern SceUID g_mutex;
extern SceClass *g_moduleClass;
extern ModulemgrPrivate *g_private;
extern SceClass *g_libraryClass;
extern SceClass *g_stubClass;
extern int g_threadStateSlot;
extern void (*g_statusCallback)(SceModuleCB *cb);
extern int g_sharedFlag;
extern SceKernelSpinlock g_sharedLock;
extern SharedModuleEntry *g_sharedModules;
extern Addr2ModTable g_kernelAddr2Mod;
extern BootimageFS *g_bootimage;
extern LoadelfTiming *g_loadTiming;
extern SceUInt32 g_relocForms;

/* loadelf.c */
SceUInt32 Loadelf_GetAddrBias(LoadelfContext *ctx, const void *addr);
SceUInt32 LoadelfParsePhdrs(LoadelfContext *ctx, SceUID fd, SelfAuthRequest *auth, SceUInt32 flags);
int LoadelfFreeImage(SceModuleCB *cb);
int LoadelfMapImage(LoadelfContext *ctx);

/* relocator.c */
int RelocatorRelSegment(LoadelfContext *ctx, SceUInt32 *rel, SceSize size);
void relocatorFormCount(void);
void relocatorFormLogPrint(void);

/* modulemgr.c */
int ModulemgrLockLibDB(void);
int ModulemgrUnlockLibDB(void);
int ModulemgrCopyFromUser(SceModuleCB *cb, void *dst, const void *src, SceSize len);
int ModulemgrDestructor(SceModuleObject *obj);
int ModulemgrConstructor(SceModuleObject *obj);
SceModuleObject *ModulemgrReferModule(SceUID modid);
void ModulemgrReleaseModule(SceUID modid);
int ModulemgrReferProcess(SceUID pid);
int ModulemgrReleaseProcess(SceUID pid);
void ShowCallerInfo(SceUID pid, SceUInt32 addr);
int ModulemgrProcEventHandler(int event, int phase, void *param, void *common);
int StopUnloadModuleCommon(SceUID pid, SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			   const SceKernelUnloadModuleOpt *opt, int *status);
int loadModuleCommon(SceUID pid, const char *path, SceUInt32 flags, const SceKernelLoadModuleOpt *opt);
int unloadModuleCommon(SceUID pid, SceUID modid, SceUInt32 flags, const SceKernelUnloadModuleOpt *opt);
int startModuleCommon(SceUID pid, SceUID modid, SceSize args, void *argp, SceUInt32 flags,
		      const SceKernelStartModuleOpt *opt, int *status);
int stopModuleCommon(SceUID pid, SceUID modid, SceSize args, void *argp, SceUInt32 flags,
		     const SceKernelStopModuleOpt *opt, int *status);
int ModulemgrInhibitLoadingModule(SceUInt32 flags);
SceUID loadStartSharedModuleCommon(SceUID pid, const char *path, SceSize args, void *argp,
				   SceUInt32 flags, const SceKernelLoadModuleOpt *opt, int *status);

/* modulecb.c */
int ModuleObjNew(SceUID pid, SceUInt32 flags, SceModuleObject **out);
int ModuleCBCopyInfo(LoadelfContext *ctx);
void ModuleCBSetStatus(SceModuleCB *cb, SceUInt8 status);
int ModuleCBSetPath(SceModuleCB *cb, const char *path, SceUInt32 flags);
void ModuleCBDumpLoadAddress(SceModuleCB *cb);
void ModuleCBShowStartMessage(SceModuleCB *cb, int noResident, SceUInt32 usec);
int ModuleCBIsInRange(SceModuleCB *cb, SceUInt32 addr, SceSize size);
int ModuleCBGetSegmentIndex(SceModuleCB *cb, SceUInt32 addr);
int ModuleCBIsInText(SceModuleCB *cb, SceUInt32 addr, SceSize size);
int ModuleCBIsInReadOnly(SceModuleCB *cb, SceUInt32 addr, SceSize size);
int ModuleCBGetSegmentOffset(SceModuleCB *cb, SceUInt32 addr, int *seg, SceUInt32 *offset);
int ModulemgrCopyIn(SceUID pid, void *dst, const void *src, SceSize len);
void *ModulemgrAlloc(SceUID pid, SceSize size);
void ModulemgrFree(SceUID pid, void *ptr);

/* libentcb.c */
typedef struct LibEntNidTable {
	void *copy;			/* heap copy of the tables (user libraries) */
	void *hashTable;
	SceUInt32 funcHashBits;
	void *varHashTable;
	SceUInt32 varHashBits;
	SceUInt32 nfunc;
	SceUInt32 nvar;
	SceNID *nidtable;
	void **entrytable;
	LibEntCB *libEntCB;
} LibEntNidTable;

int LibEnt_checkEntTable(LoadelfContext *ctx);
int ModuleCBAllocLinkInfo(LoadelfContext *ctx);
int LibEntCBInit(LoadelfContext *ctx);
void LibEntFreeNames(SceModuleCB *cb);
void ModuleCBFreeLinkInfo(SceModuleCB *cb);
int LibEntGetIndex(SceModuleCB *cb, SceUInt32 addr);
void LibEntCBClear(LibEntCB *ent);
int LibEntGetNidTable(LibEntCB *ent, LibEntNidTable *info);
void LibEntReleaseNidTable(LibEntNidTable *info);
int LibEntForEachEntry(LibEntCB *ent, int (*func)(SceNID nid, void *entry, int type, void *arg), void *arg);

/* libstubcb.c */
typedef struct LibStubNidTable {
	void *copy;
	SceNID *funcnid;
	void **funcent;
	SceNID *varnid;
	void **varent;
	SceUInt32 nfunc;
	SceUInt32 nvar;
	LibStubCB *libStubCB;
} LibStubNidTable;

SceModuleImports *LibStubNormalize(SceModuleImports *dst, const void *src);
int LibStub_checkStubTable(LoadelfContext *ctx);
int LibStubCBInit(LoadelfContext *ctx);
void LibStubFree(SceModuleCB *cb);
int LibStubClassCreate(void *obj);
int LibStubClassDestroy(void *obj);
void LibStubCBClear(LibStubCB *stub);
int LibStubCBCreateUID(LibStubCB *stub, SceUInt16 index);
int LibStubGetNidTable(LibStubCB *stub, LibStubNidTable *info);
void LibStubReleaseNidTable(LibStubNidTable *info);

/* librarydb.c */
LibraryDB *LibraryDBLock(SceUID pid, int *intr);
void LibraryDBUnlock(LibraryDB *db, int intr);
LibEntCB *LibraryDBUnlinkLib(LibraryDB *db, SceNID libnid, const char *name);
SceModuleCB **Addr2ModGetEntry(SceModuleCB **table, Addr2ModExidx **exidxBase, SceUInt32 addr,
			       Addr2ModExidx **exidx);
void Addr2ModRegister(Addr2ModTable *tbl, SceModuleCB *cb);
int LibraryDBUsesPUID(SceUID pid);
SceLibraryObject *LibraryDBReferLibrary(SceUID pid, SceUID uid);
void LibraryDBDelete(SceUID pid);
LibraryDB *LibraryDBInitializeAddr2ModuleTable(SceUID pid);
int LibraryDBInitializeAddr2ModuleTableForKernel(LibraryDB *db);
SharedModuleEntry *SharedModuleFind(const char *path);
int SharedModuleRelease(SharedModuleEntry *ent);
void LibraryDBReleaseModule(SceModuleCB *cb);
void LibraryDBRegisterLostClient(LibStubCB *stub);
void LibraryDBReleaseLostClient(LibStubCB *stub);
SceUInt32 LibraryDBRegisterLib(SceUID pid, LibEntCB *ent);
int LibraryDBReleaseLib(LibraryDB *db, LibEntCB *ent);
LibEntCB *LibraryDBFindLib(LibStubCB *stub);
LibEntCB *LibraryDBFindSyscallLib(const char *name);
int LibraryDBGetLibraryInfoByNID(SceUID pid, SceUID modid, SceNID libnid,
				 SceKernelLibraryInfo *info);
LibStubCB *LibraryDBGetLostClientHead(SceUID pid);
int LibraryDBGetModuleInfoById(SceUID pid, SceUID modid, SceKernelModuleInfo *info);
int LibraryDBGetModuleCBList(SceUID pid, SceModuleCB **list, int *count);
int LibraryDBFindModuleByAddr(LibraryDB *db, SceUInt32 addr, SceModuleCB **out);
SceUID LibraryDBGetModuleUIDByAddr(SceUID pid, SceUInt32 addr);
SceUID LibraryDBGetModuleIdByAddr(SceUID pid, SceUInt32 addr);
SceUID LibraryDBFindModuleByName(SceUID pid, const char *name);
LibStubCB *LibraryDBGetStubCBByStubCodeAddrForTool(SceModuleCB *cb, SceUInt32 addr);
int LibraryDBIsAslr(SceUID pid);
void LibraryDBSetAslr(SceUID pid);
void LibraryDBRenameAddr2ModTbls(SceUID pid, const char *name);
int LibraryDBIsShared(SceUID pid);
void LibraryDBRegisterModule(SceModuleCB *cb);
void LibraryDBSetShared(SceUID pid);
int sceKernelGetModuleList(SceUID pid, SceUInt32 flags, SceUInt32 type, SceUID *list,
			   SceSize *num);

/* bootimage_fs.c */
extern BootimageTable sceKernelBootimageEntries;	/* SceKernelBootimage variable */
SceSize BootimageGetSize(void);
void *BootimageGetData(void);
SceUID BootimageOpen(const char *path);
int sceLoadcoreIoClose(SceUID fd);
int sceLoadcoreIoReadFrom(void *buf, SceSize size, SceOff offset);

/* self_decrypter.c */
int SelfParserCheckElfHeader(const void *ehdr, SceSize size);
int SelfParserOpen(SelfParser *reader, SceUID pid, SceUID fd, SelfAuthRequest *auth);
int SelfParserClose(SelfParser *reader);
int SelfParserGetType(SelfParser *reader);
SceUInt16 SelfParserGetPhnum(SelfParser *reader);
SceUInt32 SelfParserGetEntry(SelfParser *reader);
void *SelfParserGetSegmentInfo(SelfParser *reader, SceUInt32 index);
SceUInt32 SelfParserGetPhdrOffset(SelfParser *reader, SceUInt32 index);
SceUInt64 SelfParserGetProgramAuthId(SelfParser *reader);
SceUInt64 SelfParserGetProgramVersion(SelfParser *reader);
int SelfDecrypterLoadSegment(SelfParser *reader, int index, void *dst, LoadelfTiming *timing);
int SelfDecrypterInit(void);
int SelfDecrypterSuspend(SceUInt32 mask);
void SelfDecrypterLock(void);
void SelfDecrypterUnlock(void);

/* loadcore.c */

/* the tables of an import and of the library it links to */
typedef struct StubLinkerInfo {
	LibStubNidTable stub;
	LibEntNidTable ent;
} StubLinkerInfo;

int LoadcoreUnlinkClient(LibStubCB *stub);
int LoadcoreRegisterSyscallTableEntries(LibEntCB *ent);
void registerClient(LibEntCB *ent, LibStubCB *stub);
int LoadcoreLinkClient(LibEntCB *ent, LibStubCB *stub, const ModulemgrLibraryPatch *patch);
int LibraryClassCreate(SceLibraryObject *obj);
int LibraryClassDestroy(SceLibraryObject *obj);
int LoadcoreRegisterLibrary(SceModuleCB *cb, int index, SceUInt32 flags);
SceUInt32 LoadcoreReleaseLibrary(SceModuleCB *cb, int index, SceUInt32 flags);
int LoadcoreUnlinkLibraries(SceModuleCB *cb, SceUInt32 flags);
int LoadcoreLinkLibraryEntries(SceModuleCB *cb, SceUInt32 flags, const ModulemgrTitlePatch *patch);
int LoadcoreIsLibraryImported(SceUID pid, const char *name);
int LoadcoreExportModuleEntries(SceModuleCB *cb);
int LoadcoreRegisterModuleLibraries(SceModuleCB *cb);
int LoadcoreUnregisterModuleLibraries(SceModuleCB *cb, SceUInt32 flags);
int sceKernelLinkWeakLibraryByLibname(SceUID pid, const char *libname);

/* syscall.c */

/* the user argument block of the start/stop/load-start system calls */
typedef struct SceKernelModuleSyscallParam {
	SceUInt32 flags;
	const void *opt;
	int *status;
	SceUInt32 reserved;
} SceKernelModuleSyscallParam;

SceUID _sceKernelLoadModuleWithoutStartForUser(const char *path, SceUInt32 flags, const SceKernelLoadModuleOpt *opt);
int _sceKernelStartModuleForUser(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				const SceKernelStartModuleOpt *opt, int *status);
int _sceKernelStopModuleForUser(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
			       const SceKernelStopModuleOpt *opt, int *status);
int _sceKernelUnloadModuleForUser(SceUID modid, SceUInt32 flags, const SceKernelUnloadModuleOpt *opt);
SceUID _sceKernelLoadStartModuleForUser(const char *path, SceSize args, void *argp, SceUInt32 flags,
				       const SceKernelLoadModuleOpt *opt, int *status);
int _sceKernelStopUnloadModuleForUser(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				     const SceKernelUnloadModuleOpt *opt, int *status);
SceUID _sceKernelOpenModuleForUser(const char *path, SceSize args, void *argp, SceUInt32 flags,
				  const SceKernelLoadModuleOpt *opt, int *status);
int _sceKernelCloseModuleForUser(SceUID modid, SceSize args, void *argp, SceUInt32 flags,
				const SceKernelUnloadModuleOpt *opt, int *status);

/* stub_linker.c */
int apply_reftable(SceModuleCB *cb, const void *reftable, SceUInt32 value, int mode);
int link_variables(StubLinkerInfo *info, int mode);
int link_functions(StubLinkerInfo *info, int mode, const ModulemgrLibraryPatch *patch);
int StubLinkerLinkAll(StubLinkerInfo *info, const ModulemgrLibraryPatch *patch);
int StubLinkerUnlinkAll(StubLinkerInfo *info);
int StubLinkerUndefFunctions(StubLinkerInfo *info);

/* syscallcb.c */
int func_noentry(void);
int SyscallCBInit(void);
int SyscallCBAssignEntry(SceUInt32 count);
void SyscallCBSetEntry(LibEntCB *ent, SceUInt32 index);
void SyscallCBReleaseEntry(SceUInt32 index);
int SyscallCBIsInitialized(void);

/* backtrace.c */
int BacktraceInit(void);
int sceKernelBacktrace(SceUID thid, struct SceKernelBacktraceFrame *frames, SceSize size,
			SceSize *count, SceUInt32 flags);	/* vitasdk: ksceKernelBacktrace */
int SceBacktraceForDriver_888E99B8(SceUID thid, struct SceKernelBacktraceFrameForKernel *frames,
				   SceSize size, SceSize *count, SceUInt32 flags);
int SceBacktraceForDriver_7C878F90(SceUID pid, const struct SceKernelBacktraceFrameForKernel *frames,
				   SceSize count, SceUInt32 start);

/*
 * name unknown: system call entry and exit.  TPIDRURO holds the state of
 * the user call while in the kernel; Sony's syscall functions shift it up
 * for their duration and put it back before returning.
 */
static inline SceUInt32 ModulemgrSyscallEnter(void)
{
	SceUInt32 t;

	__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(t));
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(t << 16) : "memory");
	return t;
}

static inline SceUInt32 ModulemgrGetTPIDRURO(void)
{
	SceUInt32 t;

	__asm__ volatile("mrc p15, 0, %0, c13, c0, 3" : "=r"(t));
	return t;
}

static inline void ModulemgrSyscallLeave(SceUInt32 t)
{
	__asm__ volatile("mcr p15, 0, %0, c13, c0, 3" :: "r"(t) : "memory");
}

/* exidx.c */
SceUInt32 BacktraceFindExidx(SceUID pid, SceUInt32 addr, SceUInt32 *out, SceUInt32 flags);

#endif
