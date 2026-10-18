/*
 * SceDeci4pLoadp (Sony's loadp.c): see loadp.h.
 */
#include <sysmem/sysroot.h>
#include <appmgr/appmgr_launch.h>
#include <appmgr/appmgr.h>
#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <psp2kern/kernel/processmgr.h>
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/sblaimgr.h>
#include <regmgr/regmgr.h>
#include <stdio.h>
#include <string.h>
#include "deci4p_loadp_imports.h"
#include <modulemgr/modulemgr_control.h>
#include <processmgr/processmgr.h>
#include <sysmem/qafmgr.h>
#include <intrmgr/syscall.h>
#include <threadmgr/threadmgr_debugger.h>
#include "loadp.h"
#include <kernel/error.h>
#include <alloca.h>

/* SceAppMgr (henkaku wiki SceAppMgr, vitasdk psp2/appmgr.h) */
#define SCE_APPMGR_ERROR_INVALID_PARAM_SFO	0x80800015
#define SCE_APPMGR_ERROR_PARAM			0x80802002
#define SCE_APPMGR_ERROR_NOEXEC			0x8080201f

#define LOADP_DIPSW				0xc2
#define LOADP_DIPSW_NO_AUTO_ENABLE		0xc3


/* commands; the reply to each is cmd + 1 */
#define LOADP_CMD_SPAWN				0x00
#define LOADP_CMD_KILL				0x04
#define LOADP_CMD_LAUNCH_APP			0x06
#define LOADP_CMD_KILL_APP			0x08
#define LOADP_CMD_LOAD_MODULE			0x10
#define LOADP_CMD_START_MODULE			0x12
#define LOADP_CMD_STOP_MODULE			0x14
#define LOADP_CMD_UNLOAD_MODULE			0x16
#define LOADP_CMD_TOOL_APP_1			0x20
#define LOADP_CMD_TOOL_APP_2			0x22
#define LOADP_CMD_TOOL_APP_3			0x24
#define LOADP_CMD_TOOL_APP_4			0x26
#define LOADP_CMD_PACKAGE_INSTALL		0x30
#define LOADP_CMD_PACKAGE_UNINSTALL		0x32
#define LOADP_CMD_BT_PAIRING			0x34
#define LOADP_CMD_SAVEDATA_INSTALL		0x36
#define LOADP_CMD_SAVEDATA_FORMAT		0x38

/* notifications */
#define LOADP_NOTIFY_PROCESS_CREATE		0x80
#define LOADP_NOTIFY_PROCESS_EXIT		0x81
#define LOADP_NOTIFY_PROCESS_KILL		0x82
#define LOADP_NOTIFY_LOAD_PROGRESS		0x83
#define LOADP_NOTIFY_SPAWN			0x84
#define LOADP_NOTIFY_EXIT_SPAWN			0x85
#define LOADP_NOTIFY_MODULE_LOAD		0x90
#define LOADP_NOTIFY_MODULE_UNLOAD		0x91

/* results */
#define LOADP_RESULT_OK				0
#define LOADP_RESULT_BUSY			4	/* a tool app is running */
#define LOADP_RESULT_INVALID_ARG		7
#define LOADP_RESULT_UNKNOWN_CMD		8
#define LOADP_RESULT_INVALID_SIZE		9
#define LOADP_RESULT_BAD_PACKET			0x30002
#define LOADP_RESULT_NO_MEMORY			0x30008
#define LOADP_RESULT_AGAIN			0x30009
#define LOADP_RESULT_NOT_ALLOWED		0x3000a
#define LOADP_RESULT_AUTH_FAILED		0x3000b
#define LOADP_RESULT_NOT_EXECUTABLE		0x3000c
#define LOADP_RESULT_TOO_MANY_FILES		0x3000e
#define LOADP_RESULT_IS_DIRECTORY		0x3000f
#define LOADP_RESULT_TOO_LONG			0x30010
#define LOADP_RESULT_NO_PROCESS			0x30012
#define LOADP_RESULT_NO_MODULE			0x30014
#define LOADP_RESULT_MODULE_STARTED		0x30015
#define LOADP_RESULT_MODULE_STOPPED		0x30016
#define LOADP_RESULT_MODULE_NOT_STOPPED		0x30017
#define LOADP_RESULT_NO_NID			0x30018
#define LOADP_RESULT_DENIED			0x30026
#define LOADP_RESULT_BAD_ELF			0x30028
#define LOADP_RESULT_REVOKED			0x3002a
#define LOADP_RESULT_BAD_MEDIA_TYPE		0x3002b
#define LOADP_RESULT_PRELOAD_LIBC_FAILED	0x30031
#define LOADP_RESULT_PRELOAD_FIOS2_FAILED	0x30032
#define LOADP_RESULT_INVALID_PARAM_SFO		0x30036
#define LOADP_RESULT_UNKNOWN_ERROR		0x3003a
#define LOADP_RESULT_NOT_MAIN_MODULE		0x3003c
#define LOADP_RESULT_INVALID_PROC_PARAM		0x3003d
#define LOADP_RESULT_PROCESS_SUSPENDED		0x3003e
#define LOADP_RESULT_PROCESS_EXITING		0x3003f
#define LOADP_RESULT_NOT_DOLCE			0x30040
#define LOADP_RESULT_MODULE_NOT_STARTED		0x30080
#define LOADP_RESULT_START_FAILED		0x30081
#define LOADP_RESULT_STOP_FAILED		0x30082
#define LOADP_RESULT_UNSUPPORTED		0x30083
#define LOADP_RESULT_EXISTS			0x30084

#define LOADP_PATH_MAX				0x400
#define LOADP_ARGC_MAX				0x40
#define LOADP_TITLE_ID_SIZE			10

#define LOADP_SPAWN_DEBUG			1	/* LoadpHeader.arg of a spawn */
#define LOADP_SPAWN_SUSPEND			2

#define LOADP_LOAD_MODULE_FLAGS			0x40000
#define LOADP_LOAD_MODULE_FLAGS_EXT		0x50000

#define LOADP_LAUNCH_INFO_SIZE			0x814

#define LOADP_CUI_SETUPPER			"os0:ue/cui_setupper.self"
#define LOADP_CUI_SETUPPER_AUTH_ID		0x2800800000000003ULL
#define LOADP_SPAWN_SELF			"vs0:app/NPXS10082/spawn.self"
#define LOADP_DS4_PAIRING_SELF			"vs0:app/NPXS10103/ds4_pairing.self"
#define LOADP_TOOL_APP_DEFAULT			"NPXS10998"
#define LOADP_PROTECTED_TITLE			"NPXS19999"

/* Sony's 4.00 build is fortified */
#if FW_VERSION >= 0x04000000
#include <sysmem/fortify.h>
#endif

/* ===== packets ===== */

/* the ModuleCB part of a module's description: its segments */
typedef struct LoadpSegment {
	SceUInt32 attr;				/* perms | extraPages << 20 */
	void *vaddr;
	SceSize memsz;
	SceSize align;
} LoadpSegment;

typedef struct LoadpSegments {
	SceUInt32 count;			/* slots, always 4 */
	LoadpSegment segment[4];
} LoadpSegments;

typedef struct LoadpSpawnArgs {
	LoadpHeader head;
	SceUInt32 attr;
	SceUInt32 reserved;
	SceInt32 priority;			/* of the main thread */
	SceSize stackSize;
	int argc;
	SceSize pathSize;
	char path[];				/* then argc { size, string }, then a launch param */
} LoadpSpawnArgs;

typedef struct LoadpSpawnReply {
	LoadpReply reply;
	SceUID pid;
	SceUInt32 sdkVersion;
	SceUInt32 fingerprint;
	void *entry;
	SceUInt32 reserved0[5];			/* always 0 */
	SceInt32 reserved1;			/* always 0 */
	SceInt32 reserved2;			/* always -1 */
	SceUID mainThread;			/* user thread ID */
	int initPriority;			/* of the main thread */
	SceSize stackSize;			/* of the main thread */
	char name[26];
	SceUInt8 reserved[6];
	LoadpSegments segments;
	SceSize pathSize;
	char path[];				/* then 4 words of exidx/extab */
} LoadpSpawnReply;

typedef struct LoadpPidArgs {
	LoadpHeader head;
	SceUID pid;
} LoadpPidArgs;

typedef struct LoadpPidReply {
	LoadpReply reply;
	SceUID pid;
} LoadpPidReply;

typedef struct LoadpTitleArgs {
	LoadpHeader head;
	SceSize size;
	char titleId[];
} LoadpTitleArgs;

typedef struct LoadpTitleReply {
	LoadpReply reply;
	SceUID pid;
	SceSize size;
	char titleId[LOADP_TITLE_ID_SIZE];
} LoadpTitleReply;

typedef struct LoadpModuleLoadArgs {
	LoadpHeader head;
	SceUID pid;
	SceSize pathSize;
	char path[];
} LoadpModuleLoadArgs;

typedef struct LoadpModuleLoadReply {
	LoadpReply reply;
	SceUID pid;
	SceUID modid;
	SceUInt32 version;
	SceUInt32 sdkVersion;
	SceUInt32 fingerprint;
	SceUInt16 reserved[2];
	void *start;
	void *stop;
	void *exit;
	char name[26];
	SceUInt8 padding[6];
	LoadpSegments segments;
	SceSize pathSize;
	char path[];
} LoadpModuleLoadReply;

typedef struct LoadpModuleArgs {
	LoadpHeader head;
	SceUID pid;
	SceUID modid;
	int argc;
	char args[];				/* argc strings, each 4-aligned */
} LoadpModuleArgs;

typedef struct LoadpModuleReply {
	LoadpReply reply;
	SceUID pid;
	SceUID modid;
} LoadpModuleReply;

typedef struct LoadpPathArgs {
	LoadpHeader head;
	SceSize size;
	char path[];
} LoadpPathArgs;

typedef struct LoadpSaveDataArgs {
	LoadpHeader head;
	SceUInt32 mode;
	SceUInt8 slot;
	SceUInt8 reserved[3];
	SceSize size;
	char path[];				/* a directory, or a title ID */
} LoadpSaveDataArgs;

typedef struct LoadpNotify {
	LoadpHeader head;
	SceUInt32 a;
	SceUInt32 b;
	SceUInt64 time __attribute__((packed, aligned(4)));
} LoadpNotify;

typedef struct LoadpKillNotify {
	LoadpHeader head;
	SceUID pid;
	SceUInt64 time __attribute__((packed, aligned(4)));
} LoadpKillNotify;

typedef struct LoadpSpawnNotify {
	LoadpHeader head;
	SceUID pid;
	SceUInt64 time __attribute__((packed, aligned(4)));
	SceUID spawnPid;
	SceUInt32 error;
} LoadpSpawnNotify;

typedef struct LoadpProgressNotify {
	LoadpHeader head;
	SceUInt32 progress;
	SceSize pathSize;
	char path[];
} LoadpProgressNotify;

typedef struct LoadpModuleNotify {
	LoadpHeader head;
	SceUID pid;
	SceUID modid;
	SceUInt64 time __attribute__((packed, aligned(4)));
	SceUInt8 version[2];
	char name[26];
	SceUInt32 fingerprint;
	LoadpSegments segments;
	SceSize pathSize;
	char path[];				/* then the metadata size and data */
} LoadpModuleNotify;

/* SceSysroot process handler: the events the host hears about */
typedef struct LoadpProcessHandler {
	SceSize size;
	void (*create)(SceUID parentPid, SceUID pid, int flags, SceUInt64 time);
	void (*exit)(SceUID pid, int status, SceUInt64 time);
	void (*kill)(SceUID pid, SceUInt64 time);
	void (*progress)(SceUID pid, SceUID modid, SceUInt32 progress, const char *path);
	void (*moduleLoad)(SceUID pid, SceUID modid, SceUInt64 time);
	void (*moduleUnload)(SceUID pid, SceUID modid, SceUInt64 time);
	void (*appResult)(int result);
	void (*spawn)(SceUID pid, SceUID spawnPid, SceUInt64 time);
	void (*exitSpawn)(SceUID pid, SceUID spawnPid, int error, SceUInt64 time);
} LoadpProcessHandler;

/* the launch of a tool app sets this before signalling loadp.sema */
static int loadpAppResult;
static LoadpContext loadp;

static inline int isKernelPid(SceUID pid)
{
	return (SceUInt32)(pid + 1) < 2;
}

static inline void lockFlags(SceUInt32 set, SceUInt32 clear)
{
	SceKernelIntrStatus s = SceCpuForDriver_D32ACE9E(&loadp.lock);

	loadp.flags = (loadp.flags & ~clear) | set;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&loadp.lock, s);
}

/* ===== the send queue ===== */

static void loadpFreeSendBuf(LoadpSendBuf *buf)
{
	sceKernelFreeHeapMemory(loadp.heap, buf->mem);
	sceKernelFreeHeapMemory(loadp.heap, buf);
}

/* the size of a new packet, its header set */
static inline __attribute__((always_inline)) SceSize loadpInitPacketInline(void *packet,
								 SceSize size)
{
	__builtin_memset(packet, 0, size);
	SceDeci4pDfMgrForDebugger_6D26CC56(packet, size);
	return SceDeci4pDfMgrForDebugger_CACAB5F9(packet);
}

static __attribute__((noinline)) SceSize loadpInitPacket(void *packet, SceSize size)
{
	return loadpInitPacketInline(packet, size);
}

LoadpSendBuf *pdLoadpAllocSendBuf(void)
{
	LoadpSendBuf *buf = SceSysmemForDriver_7B4CB60A(loadp.heap, sizeof(*buf));
	void *mem;

	if (buf == NULL)
		SCE_PANIC(1048);
	memset(buf, 0, sizeof(*buf));
	mem = SceSysmemForDriver_7B4CB60A(loadp.heap, LOADP_SEND_BUF_SIZE);
	buf->mem = mem;
	if (mem == NULL) {
		sceKernelFreeHeapMemory(loadp.heap, buf);
		return NULL;
	}
	memset(mem, 0, LOADP_SEND_BUF_SIZE);
	buf->packet = (void *)(((SceUIntPtr)mem + 0x3f) & ~0x3f);
	return buf;
}

/* queue a packet; too many queued and the queue is dropped */
#if FW_VERSION >= 0x04000000
#define LOADP_CONTEXT_ARG
int pdLoadpReqSend(LoadpSendBuf *buf)
{
	LoadpContext *ctx = &loadp;
#else
#define LOADP_CONTEXT_ARG &loadp,
int pdLoadpReqSend(LoadpContext *ctx, LoadpSendBuf *buf)
{
#endif
	LoadpSendBuf *p, *next;
	int ret;

	if (!(ctx->flags & LOADP_FLAG_ENABLED))
		return DFMGR_ERROR_NOT_CONNECTED;
	ret = sceKernelLockMutex(ctx->mutex, 1, NULL);
	if (ret < 0)
		return ret;
	if (ctx->sendHead == NULL) {
		ctx->sendHead = buf;
	} else {
		for (p = ctx->sendHead; p->next != NULL; p = p->next)
			;
		p->next = buf;
	}
	buf->next = NULL;
	if (++ctx->sendCount > LOADP_SEND_QUEUE_MAX) {
		for (p = ctx->sendHead; p != NULL; p = next) {
			next = p->next;
			sceKernelFreeHeapMemory(loadp.heap, p->mem);
			sceKernelFreeHeapMemory(loadp.heap, p);
		}
		ctx->sendHead = NULL;
		ctx->sendCount = 0;
	}
	ret = sceKernelUnlockMutex(ctx->mutex, 1);
	if (ret < 0)
		return ret;
	ret = sceDfMgrReqSend(ctx->handle);
	if (ret >= 0)
		return 0;
	if (ret == (int)DFMGR_ERROR_BUSY)
		return 0;
	if (ret != (int)DFMGR_ERROR_NOT_CONNECTED) {
		SCE_PRINTF_WITH_INFO(0xf, 979, "0x%08x\n", ret);
		return ret;
	}
	/* disconnected: the queue is given up (Sony does not free it) */
	do {
		if (ctx->sendCount == 0)
			return DFMGR_ERROR_NOT_CONNECTED;
		ret = sceKernelLockMutex(ctx->mutex, 1, NULL);
		if (ret < 0)
			return ret;
		if (ctx->sendHead != NULL) {
			ctx->sendHead = ctx->sendHead->next;
			ctx->sendCount--;
		}
		ret = sceKernelUnlockMutex(ctx->mutex, 1);
	} while (ret >= 0);
	return ret;
}

/* the reply to a command that failed, or has nothing to report */
static void loadpSendReply(SceUInt8 cmd, SceUInt8 reqCmd, SceUInt32 result)
{
	LoadpSendBuf *buf = pdLoadpAllocSendBuf();
	LoadpReply *reply;

	if (buf == NULL)
		return;
	reply = buf->packet;
	buf->size = loadpInitPacketInline(reply, sizeof(*reply));
	reply->head.cmd = cmd;
	reply->head.arg = reqCmd;
	reply->result = result;
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0)
		loadpFreeSendBuf(buf);
}

/* a kernel error as the host knows it */
static SceUInt32 pdLoadpKernelErrToLoadpErr(int error)
{
	switch (error) {
	case 0:
		return LOADP_RESULT_OK;
	case 0x80802021:
		return LOADP_RESULT_BUSY;
	case SCE_ERROR_ERRNO_ENOMEM:
	case SCE_KERNEL_ERROR_MODULEMGR_NOMEM_LIB:
	case SCE_KERNEL_ERROR_MODULEMGR_NOMEM_STUB:
	case SCE_KERNEL_ERROR_MODULEMGR_NOMEM_SELF:
	case SCE_KERNEL_ERROR_MODULEMGR_NOMEM:
		return LOADP_RESULT_NO_MEMORY;
	case SCE_ERROR_ERRNO_EAGAIN:
		return LOADP_RESULT_AGAIN;
	case SCE_ERROR_ERRNO_ENOENT:
	case SCE_ERROR_ERRNO_EINVAL:
	case SCE_KERNEL_ERROR_MODULEMGR_NOENT:
	case 0x8010390c:
	case 0x80800007:
		return LOADP_RESULT_NOT_ALLOWED;
	case SCE_KERNEL_ERROR_AUTHFAIL:
		return LOADP_RESULT_AUTH_FAILED;
	case SCE_ERROR_ERRNO_ENOEXEC:
	case SCE_KERNEL_ERROR_ILLEGAL_SELF_HEADER:
	case SCE_KERNEL_ERROR_MODULEMGR_NOEXEC:
		return LOADP_RESULT_NOT_EXECUTABLE;
	case SCE_ERROR_ERRNO_EMFILE:
		return LOADP_RESULT_TOO_MANY_FILES;
	case SCE_ERROR_ERRNO_EISDIR:
		return LOADP_RESULT_IS_DIRECTORY;
	case SCE_ERROR_ERRNO_ENAMETOOLONG:
	case SCE_KERNEL_ERROR_MODULEMGR_NAMETOOLONG:
		return LOADP_RESULT_TOO_LONG;
	case SCE_KERNEL_ERROR_INVALID_PID:
	case SCE_KERNEL_ERROR_MODULEMGR_NO_PROCESS:
	case SCE_APPMGR_ERROR_PARAM:
		return LOADP_RESULT_NO_PROCESS;
	case SCE_KERNEL_ERROR_MODULEMGR_NO_MOD:
		return LOADP_RESULT_NO_MODULE;
	case SCE_KERNEL_ERROR_MODULEMGR_STARTED:
		return LOADP_RESULT_MODULE_STARTED;
	case SCE_KERNEL_ERROR_MODULEMGR_STOPPED:
		return LOADP_RESULT_MODULE_STOPPED;
	case SCE_KERNEL_ERROR_MODULEMGR_NOT_STOPPED:
		return LOADP_RESULT_MODULE_NOT_STOPPED;
	case SCE_KERNEL_ERROR_MODULEMGR_NO_FUNC_NID:
	case SCE_KERNEL_ERROR_MODULEMGR_NO_VAR_NID:
		return LOADP_RESULT_NO_NID;
	case 0x800f0b3a:
		return 0x30019;
	case 0x80800020:
		return 0x30022;
	case 0x80800021:
		return 0x30023;
	case 0x80800022:
		return 0x30024;
	case 0x80800023:
		return 0x30025;
	case SCE_KERNEL_ERROR_ILLEGAL_ELF_HEADER:
	case 0x8080001a:
		return LOADP_RESULT_BAD_ELF;
	case 0x800f0627:
		return 0x30029;
	case 0x800f0b30:
		return LOADP_RESULT_REVOKED;
	case 0x800f0b32:
		return LOADP_RESULT_BAD_MEDIA_TYPE;
	case 0x800f0537:
	case 0x800f0b37:
		return 0x30030;
	case SCE_KERNEL_ERROR_PRELOAD_LIBC_FAILED:
		return LOADP_RESULT_PRELOAD_LIBC_FAILED;
	case SCE_KERNEL_ERROR_PRELOAD_FIOS2_FAILED:
		return LOADP_RESULT_PRELOAD_FIOS2_FAILED;
	case 0x80800005:
		return 0x30035;
	case SCE_APPMGR_ERROR_INVALID_PARAM_SFO:
		return LOADP_RESULT_INVALID_PARAM_SFO;
	case 0x80800016:
		return 0x30037;
	case 0x80800017:
		return 0x30038;
	case 0x80800019:
		return 0x30039;
	case 0x80802025:
		return 0x3003b;
	case SCE_KERNEL_ERROR_MODULEMGR_NOT_MAIN_MODULE:
		return LOADP_RESULT_NOT_MAIN_MODULE;
	case SCE_KERNEL_ERROR_MODULEMGR_INVALID_PROC_PARAM:
		return LOADP_RESULT_INVALID_PROC_PARAM;
	case SCE_KERNEL_ERROR_MODULEMGR_NOT_STARTED:
		return LOADP_RESULT_MODULE_NOT_STARTED;
	case SCE_KERNEL_ERROR_MODULEMGR_START_FAILED:
		return LOADP_RESULT_START_FAILED;
	case SCE_KERNEL_ERROR_MODULEMGR_STOP_FAIL:
		return LOADP_RESULT_STOP_FAILED;
	case SCE_ERROR_ERRNO_ENOSYS:
		return LOADP_RESULT_UNSUPPORTED;
	case SCE_ERROR_ERRNO_EEXIST:
		return LOADP_RESULT_EXISTS;
	default:
		SceDebugForDriver_391B74B7("*** load error happened: 0x%08x ***\n", error);
		return LOADP_RESULT_UNKNOWN_ERROR;
	}
}

/* ===== process and module events ===== */

static inline void setSegments(LoadpSegments *s, const SceModuleCB *cb)
{
	int i;

	s->count = 4;
	for (i = 0; i < cb->nSegments && i < 3; i++) {
		s->segment[i].attr = cb->segment[i].perms | cb->segment[i].extraPages << 20;
		s->segment[i].vaddr = cb->segment[i].vaddr;
		s->segment[i].memsz = cb->segment[i].memsz;
		s->segment[i].align = 1 << cb->segment[i].alignLog2;
	}
}

/* two values and a time */
static inline __attribute__((always_inline)) int loadpNotify(SceUInt8 cmd, SceUInt32 a, SceUInt32 b,
							     SceUInt64 time)
{
	LoadpSendBuf *buf = pdLoadpAllocSendBuf();
	LoadpNotify *n;
	int ret;

	if (buf == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	n = buf->packet;
	buf->size = loadpInitPacketInline(n, sizeof(*n));
	n->a = a;
	n->b = b;
	n->head.arg = 0;
	n->time = time;
	n->head.cmd = cmd;
	ret = pdLoadpReqSend(LOADP_CONTEXT_ARG buf);
	if (ret < 0) {
		loadpFreeSendBuf(buf);
		return ret;
	}
	return 0;
}

static int loadpNotifyProcessCreate(SceUID parentPid, SceUID pid, SceUInt64 time)
{
	return loadpNotify(LOADP_NOTIFY_PROCESS_CREATE, parentPid, pid, time);
}

static int loadpNotifyModuleUnload(SceUID pid, SceUID modid, SceUInt64 time)
{
	return loadpNotify(LOADP_NOTIFY_MODULE_UNLOAD, pid, modid, time);
}

static void loadpOnProcessCreate(SceUID parentPid, SceUID pid, int flags, SceUInt64 time)
{
	(void)flags;
	loadpNotifyProcessCreate(parentPid, pid, time);
	sceDfMgrReqSend(loadp.handle);
}

static void loadpOnProcessExit(SceUID pid, int status, SceUInt64 time)
{
	if (pid == loadp.appPid) {
		loadp.appPid = -1;
		loadp.appCmd = 0;
		sceDfMgrDcmpReqSendForLoadp(status);
	}
	loadpNotify(LOADP_NOTIFY_PROCESS_EXIT, pid, status, time);
}

/* the tool app of a DCMP request was killed: its result goes to DCMP */
static void loadpOnProcessKill(SceUID pid, SceUInt64 time)
{
	LoadpSendBuf *buf;
	LoadpKillNotify *n;
	SceUInt32 result;

	if (pid == loadp.appPid) {
		switch (loadp.appCmd) {
		case LOADP_CMD_PACKAGE_INSTALL:
			SceSysrootForDriver_D03741B8(4, "", 1);
			result = 0;
			break;
		case LOADP_CMD_PACKAGE_UNINSTALL:
			SceSysrootForDriver_0F59B369(4, "", 1);
			result = 0;
			break;
		case LOADP_CMD_BT_PAIRING:
			result = 4;
			break;
		case LOADP_CMD_SAVEDATA_INSTALL:
			SceSysrootForDriver_E384DD93(4, "", 1);
			result = 0;
			break;
		case LOADP_CMD_SAVEDATA_FORMAT:
			SceSysrootForDriver_D04C7723(4, "", 1);
			result = 0;
			break;
		default:
			result = 0;
			break;
		}
		loadp.appPid = -1;
		loadp.appCmd = 0;
		sceDfMgrDcmpReqSendForLoadp(result);
	}
	buf = pdLoadpAllocSendBuf();
	if (buf == NULL)
		return;
	n = buf->packet;
	buf->size = loadpInitPacketInline(n, sizeof(*n));
	n->head.arg = 0;
	n->pid = pid;
	n->time = time;
	n->head.cmd = LOADP_NOTIFY_PROCESS_KILL;
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0)
		loadpFreeSendBuf(buf);
}

static void loadpOnProgress(SceUID pid, SceUID modid, SceUInt32 progress, const char *path)
{
	LoadpSendBuf *buf;
	LoadpProgressNotify *n;
	SceSize len, padded, size;

	(void)pid;
	(void)modid;
	len = strnlen(path, 0x100);
	if (len > 0xff)
		len = 0xff;
	buf = pdLoadpAllocSendBuf();
	if (buf == NULL)
		return;
	padded = (len + 4) & ~3;
	n = buf->packet;
	size = sizeof(*n) + padded;
	buf->size = loadpInitPacketInline(n, size);
	n->progress = progress;
	n->pathSize = len + 1;
	n->head.cmd = LOADP_NOTIFY_LOAD_PROGRESS;
	n->head.arg = 0;
	memcpy(n->path, path, len);
	memset(n->path + len, 0, padded - len);
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0)
		loadpFreeSendBuf(buf);
}

/* a module's description, from its ModuleCB */
static int loadpNotifyModuleLoad(SceUID pid, SceUID modid, SceUInt64 time)
{
	LoadpSendBuf *buf;
	LoadpModuleNotify *n;
	SceModuleCB *cb;
	SceSize len, padded, metaSize, size;
	char *p;
	int ret, tooLarge;

	buf = pdLoadpAllocSendBuf();
	if (buf == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	ret = SceModulemgrForKernel_37512E29(modid, &cb);
	if (ret < 0 || cb == NULL || cb->path == NULL) {
		loadpFreeSendBuf(buf);
		return ret;
	}
	len = strnlen(cb->path, 0x100);
	metaSize = cb->metadataSize;
	if (len > 0xff)
		len = 0xff;
	padded = (len + 4) & ~3;
	size = ((metaSize + 3) & ~3) + sizeof(*n) + 4 + padded;
	tooLarge = size < metaSize;
	if (size > LOADP_RECV_BUF_SIZE)
		tooLarge = 1;
	if (tooLarge) {
		size = LOADP_RECV_BUF_SIZE;
		metaSize = LOADP_RECV_BUF_SIZE - (sizeof(*n) + 4) - padded;
	}
	n = buf->packet;
	buf->size = loadpInitPacketInline(n, size);
	n->head.cmd = LOADP_NOTIFY_MODULE_LOAD;
	n->head.arg = 0;
	n->pid = pid;
	n->modid = (pid == SCE_KERNEL_PROCESS_ID || isKernelPid(pid)) ? modid : cb->puid;
	n->time = time;
	n->version[0] = cb->version[0];
	n->version[1] = cb->version[1];
	__builtin_memcpy(n->name, cb->name, sizeof(n->name));
	n->fingerprint = cb->fingerprint;
	setSegments(&n->segments, cb);
	n->pathSize = len + 1;
	memcpy(n->path, cb->path, len);
	memset(n->path + len, 0, padded - len);
	p = n->path + padded;
	*(SceSize *)p = metaSize;
	if (metaSize != 0) {
		memcpy(p + 4, cb->metadata, metaSize);
		memset(p + 4 + metaSize, 0, ((metaSize + 3) & ~3) - metaSize);
	}
	ret = pdLoadpReqSend(LOADP_CONTEXT_ARG buf);
	if (ret < 0) {
		loadpFreeSendBuf(buf);
		return ret;
	}
	return 0;
}

static void loadpOnModuleLoad(SceUID pid, SceUID modid, SceUInt64 time)
{
	loadpNotifyModuleLoad(pid, modid, time);
	sceDfMgrReqSend(loadp.handle);
}

static void loadpOnModuleUnload(SceUID pid, SceUID modid, SceUInt64 time)
{
	loadpNotifyModuleUnload(pid, modid, time);
	sceDfMgrReqSend(loadp.handle);
}

/* a launched tool app's result, for the command waiting on it */
static void loadpOnAppResult(int result)
{
	loadpAppResult = result;
	sceKernelSignalSema(loadp.sema, 1);
}

static void loadpOnSpawn(SceUID pid, SceUID spawnPid, SceUInt64 time)
{
	LoadpSendBuf *buf = pdLoadpAllocSendBuf();
	LoadpSpawnNotify *n;

	if (buf != NULL) {
		n = buf->packet;
		buf->size = loadpInitPacketInline(n, sizeof(*n) - 4);
		n->pid = pid;
		n->time = time;
		n->head.arg = 0;
		n->spawnPid = spawnPid;
		n->head.cmd = LOADP_NOTIFY_SPAWN;
		if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0)
			loadpFreeSendBuf(buf);
	}
	sceDfMgrReqSend(loadp.handle);
}

static void loadpOnExitSpawn(SceUID pid, SceUID spawnPid, int error, SceUInt64 time)
{
	LoadpSendBuf *buf = pdLoadpAllocSendBuf();
	LoadpSpawnNotify *n;

	if (buf != NULL) {
		n = buf->packet;
		buf->size = loadpInitPacketInline(n, sizeof(*n));
		n->pid = pid;
		n->time = time;
		n->head.cmd = LOADP_NOTIFY_EXIT_SPAWN;
		n->head.arg = 0;
		n->spawnPid = spawnPid;
		n->error = pdLoadpKernelErrToLoadpErr(error);
		if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0)
			loadpFreeSendBuf(buf);
	}
	sceDfMgrReqSend(loadp.handle);
}

static const LoadpProcessHandler loadpProcessHandler = {
	.size = sizeof(LoadpProcessHandler),
	.create = loadpOnProcessCreate,
	.exit = loadpOnProcessExit,
	.kill = loadpOnProcessKill,
	.progress = loadpOnProgress,
	.moduleLoad = loadpOnModuleLoad,
	.moduleUnload = loadpOnModuleUnload,
	.appResult = loadpOnAppResult,
	.spawn = loadpOnSpawn,
	.exitSpawn = loadpOnExitSpawn,
};

/* ===== SceDeci4pDfMgr ===== */

/* the next command, into the receive buffer */
static inline __attribute__((always_inline)) int pdLoadpGetCmd(LoadpContext *ctx)
{
	LoadpHeader *packet;
	SceSize size;
	int ret;

	ret = sceDfMgrRecv(ctx->handle, ctx->recvBuf, sizeof(LoadpHeader));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 658, "0x%08x\n", ret);
		goto failed;
	}
	packet = ctx->recvBuf;
	size = SceDeci4pDfMgrForDebugger_CACAB5F9(&packet->hdr);
	if (size - sizeof(LoadpHeader) >= LOADP_RECV_BUF_SIZE - sizeof(LoadpHeader) + 1)
		goto failed;
	ctx->packetSize = size;
	ctx->argSize = size - sizeof(LoadpHeader);
	ret = sceDfMgrRecv(ctx->handle, packet + 1, size - sizeof(LoadpHeader));
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 686, "0x%08x\n", ret);
		goto failed;
	}
	loadp.pending = ctx->recvBuf;
	if (loadp.pending == NULL)
		return DFMGR_ERROR_INVALID_ARGUMENT;
	return sceKernelSetEventFlag(loadp.evf, 1);

failed:
	loadp.pending = NULL;
	return DFMGR_ERROR_INVALID_ARGUMENT;
}

static inline void pdLoadpPutSendDoneMsgQueue(LoadpSendBuf *buf)
{
	if (sceKernelLockMutex(loadp.mutex, 1, NULL) < 0)
		SCE_PANIC(1091);
	buf->next = loadp.doneList;
	loadp.doneList = buf;
	if (sceKernelUnlockMutex(loadp.mutex, 1) < 0)
		SCE_PANIC(1100);
}

static inline void pdLoadpCleanSendDoneMsgQueue(void)
{
	LoadpSendBuf *buf, *next;

	if (sceKernelLockMutex(loadp.mutex, 1, NULL) < 0)
		SCE_PANIC(1114);
	for (buf = loadp.doneList; buf != NULL; buf = next) {
		next = buf->next;
		sceKernelFreeHeapMemory(loadp.heap, buf->mem);
		sceKernelFreeHeapMemory(loadp.heap, buf);
	}
	loadp.doneList = NULL;
	if (sceKernelUnlockMutex(loadp.mutex, 1) < 0)
		SCE_PANIC(1129);
}

/* send the first queued packet */
static inline __attribute__((always_inline)) int pdLoadpSend(void)
{
	LoadpSendBuf *buf;
	int ret;

	if (sceKernelLockMutex(loadp.mutex, 1, NULL) < 0)
		return 0;
	buf = loadp.sendHead;
	if (buf == NULL) {
		sceKernelUnlockMutex(loadp.mutex, 1);
		return 0;
	}
	loadp.sendHead = buf->next;
	loadp.sendCount--;
	if (sceKernelUnlockMutex(loadp.mutex, 1) < 0)
		return 0;
	ret = sceDfMgrSend(loadp.handle, buf->packet, buf->size);
	if (ret < 0)
		SCE_PRINTF_WITH_INFO(0xf, 543, "0x%08x\n", ret);
	else
		pdLoadpPutSendDoneMsgQueue(buf);
	return ret;
}

static int pdLoadpHandler(int event, void *arg, void *ctxp)
{
	LoadpContext *ctx = ctxp;
	const SceDfMgrStatus *status;
	LoadpSendBuf *head;
	int value, ret;

	switch (event) {
	case SCE_DFMGR_EVENT_RECV:
		if (loadp.pending != NULL) {
			ret = DFMGR_ERROR_INVALID_ARGUMENT;
			break;
		}
		ret = pdLoadpGetCmd(ctx);
		if (ret >= 0)
			return ret;
		break;
	case SCE_DFMGR_EVENT_REQ_DONE:
	case 6:
		return 0;
	case SCE_DFMGR_EVENT_SEND:
		ret = pdLoadpSend();
		if (ret >= 0)
			return ret;
		break;
	case SCE_DFMGR_EVENT_SENT:
		if (sceKernelLockMutex(loadp.mutex, 1, NULL) < 0 ||
		    (head = loadp.sendHead, sceKernelUnlockMutex(loadp.mutex, 1) < 0) ||
		    head == NULL)
			pdLoadpCleanSendDoneMsgQueue();
		else
			sceDfMgrReqSend(loadp.handle);
		return 0;
	case SCE_DFMGR_EVENT_STATUS:
		status = arg;
		if (status->kind == 0) {
			value = status->connected;
			if (value == 0) {
				lockFlags(0, LOADP_FLAG_ENABLED);
				return 0;
			}
			if (value == 1) {
				lockFlags(LOADP_FLAG_ENABLED, 0);
				return 1;
			}
		} else if (status->kind == 1) {
			value = status->enabled;
			if (value == 0) {
				lockFlags(0, LOADP_FLAG_ENABLED);
				return 0;
			}
			if (value == 1) {
				lockFlags(LOADP_FLAG_ENABLED, 0);
				return 0;
			}
		}
		ret = DFMGR_ERROR_INVALID_ARGUMENT;
		break;
	default:
		SCE_PRINTF_WITH_INFO(0xf, 522, "0x%08x\n", event);
		return 0;
	}
	SCE_PRINTF_WITH_INFO(0xf, 526, "0x%08x:0x%08x\n", event, ret);
	return ret;
}

/* ===== commands ===== */

/* whether a program may come from where the path says */
static inline int pathAllowed(const char *path)
{
	return SceSysrootForDriver_55392965() || SceQafMgrForDriver_B9770A13() ||
	       SceQafMgrForDriver_082A4FC2() || strncmp("host0:", path, 6) == 0 ||
	       strncmp("ux0:data/", path, 9) == 0 || strncmp("sd0:", path, 4) == 0;
}

/* an old cui_setupper on host0: is replaced with the system's */
static inline int useSystemSetupper(const char *path, SceSize pathSize)
{
	SceUInt64 authId, authId2, authId3, version, sysVersion;

	if (strnlen(path, 0x100) != pathSize - 1 || strncmp(path, "host0:", 6) != 0)
		return 0;
	if (sceKernelGetProgramIdentificationInfo(path, &authId, &sysVersion) < 0 ||
	    authId != LOADP_CUI_SETUPPER_AUTH_ID)
		return 0;
	if (sceKernelGetProgramIdentificationInfo(path, &authId2, &version) < 0 ||
	    sceKernelGetProgramIdentificationInfo(LOADP_CUI_SETUPPER, &authId3,
						   &sysVersion) < 0)
		return 0;
	if (authId2 != authId || authId3 != authId2)
		return 0;
	return sysVersion > version;
}

/* the spawn reply: the new process's main module (part of pdLoadpProcSpawnCmd) */
static inline __attribute__((always_inline)) int sendSpawnReply(SceUID pid, const char *path,
								 SceSize pathSize, SceSize padded,
								 const void *launchParam,
								 SceUInt32 *result)
{
	LoadpSendBuf *buf;
	LoadpSpawnReply *r;
	ThreadmgrThreadInfoForDebugger *info;
	SceModuleCB *cb;
	SceUInt32 *tail;
	int ret;

	buf = pdLoadpAllocSendBuf();
	if (buf == NULL) {
		*result = 0;
		return -1;
	}
	r = buf->packet;
	buf->size = loadpInitPacketInline(r, sizeof(*r) + padded + 0x10);
	r->reply.head.cmd = LOADP_CMD_SPAWN + 1;
	r->reply.head.arg = LOADP_CMD_SPAWN;
	r->reply.result = 0;
	ret = SceModulemgrForKernel_37512E29(sceKernelGetModuleIdByPid(pid), &cb);
	if (ret < 0 || cb == NULL) {
		*result = pdLoadpKernelErrToLoadpErr(ret);
		goto free_buf;
	}
	r->pid = pid;
	r->sdkVersion = cb->sdkVersion;
	r->fingerprint = cb->fingerprint;
	r->entry = cb->moduleStart;
	r->reserved0[0] = 0;
	r->reserved0[1] = 0;
	r->reserved0[2] = 0;
	r->reserved0[3] = 0;
	r->reserved0[4] = 0;
	r->reserved1 = 0;
	r->reserved2 = -1;
	r->mainThread = SceThreadmgrForDriver_F311808F(sceKernelGetProcessMainThreadForDebugger(pid));
	info = SceSysmemForDriver_7B4CB60A(loadp.heap, sizeof(*info));
	if (info == NULL) {
		*result = LOADP_RESULT_NO_MEMORY;
		goto free_buf;
	}
	info->size = sizeof(*info);
	ret = SceThreadmgrForDriver_555CA6BB(sceKernelGetProcessMainThreadForDebugger(pid), 0, info);
	if (ret < 0) {
		*result = pdLoadpKernelErrToLoadpErr(ret);
		goto free_info;
	}
	r->initPriority = info->initPriority;
	r->stackSize = info->userStackSize;
	__builtin_memcpy(r->name, cb->name, sizeof(r->name));
	setSegments(&r->segments, cb);
	r->pathSize = pathSize;
	memcpy(r->path, path, pathSize);
	if (padded != pathSize)
		memset(r->path + pathSize, 0, padded - pathSize);
	tail = (SceUInt32 *)(r->path + padded);
	tail[0] = (SceUInt32)cb->exidxTop;
	tail[1] = (SceUInt32)cb->exidxBtm;
	tail[2] = (SceUInt32)cb->extabStart;
	tail[3] = (SceUInt32)cb->extabEnd;
	if (launchParam != NULL) {
		ret = sceKernelWaitSema(loadp.sema, 1, NULL);
		if (ret < 0)
			SceDebugForDriver_02B04343(0xf, SCE_DBGINFO_FUNC(3948, "pdLoadpProcSpawnCmd"),
						 "0x%08x\n", ret);
		if (loadpAppResult != 0) {
			ret = sceAppMgrKillAppForDebugger(pid);
#if FW_DEBUG
			if (ret < 0)
				SceDebugForDriver_D9703808(1, 0xf,
							      SCE_DBGINFO_FUNC(3956, "pdLoadpProcSpawnCmd"),
							      "sceAppMgrKillAppForDebugger failed 0x%08x\n", ret);
#endif
			*result = pdLoadpKernelErrToLoadpErr(loadpAppResult);
			goto free_info;
		}
	}
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) >= 0) {
		sceKernelFreeHeapMemory(loadp.heap, info);
		return 0;
	}
	*result = 0;
free_info:
	sceKernelFreeHeapMemory(loadp.heap, info);
free_buf:
	sceKernelFreeHeapMemory(loadp.heap, buf->mem);
	sceKernelFreeHeapMemory(loadp.heap, buf);
	return -1;
}

/* spawn a process, or launch an app with a launch param */
static __attribute__((noinline)) void pdLoadpProcSpawnCmd(SceUInt8 flags)
{
	SceSize argSize = loadp.argSize;
	LoadpSpawnArgs *req = (LoadpSpawnArgs *)loadp.recvBuf;
	SceSize pathSize, padded, total, size, n, stackSize;
	const char *argv[LOADP_ARGC_MAX];
	const char *base = (const char *)req + sizeof(LoadpHeader);
	SceUInt32 attr, appFlags, result;
	SceKernelProcessOpt param;
	const int *launchParam;
	int argc, i, setupper, gxmDebug, debug, value, ret, priority;
	char *path, *argBuf, *p;
	SceUInt64 time;
	SceUID pid;

	loadpAppResult = 0;
	if (argSize < 0x18)
		goto bad_packet;
	pathSize = req->pathSize;
	attr = req->attr;
	priority = req->priority;
	stackSize = req->stackSize;
	argc = req->argc;
	if (pathSize > LOADP_PATH_MAX) {
		result = LOADP_RESULT_TOO_LONG;
		goto reply;
	}
	padded = (pathSize + 3) & ~3;
	if (argSize < padded + 0x18)
		goto bad_packet;
	path = req->path;
	path[pathSize - 1] = '\0';
	if (!pathAllowed(path)) {
		result = LOADP_RESULT_NOT_ALLOWED;
		goto reply;
	}
	setupper = useSystemSetupper(path, pathSize);
	if (argc > 0x3f)
		argc = LOADP_ARGC_MAX;

	launchParam = (const int *)(path + padded);
	total = 0;
	for (i = 0; i < argc; i++) {
		if ((SceSize)((const char *)launchParam + 4 - base) > argSize)
			goto bad_packet;
		n = launchParam[0];
		p = (char *)(launchParam + 1);
		if ((SceSize)(p + n - base) > argSize)
			goto bad_packet;
		argv[i] = p;
		total += n;
		launchParam = (const int *)(p + ((n + 3) & ~3));
		p[n - 1] = '\0';
	}
	if ((SceSize)((const char *)launchParam + 4 - base) < argSize) {
		if (argSize < (SceSize)((const char *)launchParam + launchParam[0] - base))
			goto bad_packet;
	} else {
		launchParam = NULL;
	}

	if (setupper) {
		path = SceSysmemForDriver_7B4CB60A(loadp.heap, sizeof(LOADP_CUI_SETUPPER));
		if (path == NULL) {
			result = LOADP_RESULT_NO_MEMORY;
			goto reply;
		}
		memset(path, 0, sizeof(LOADP_CUI_SETUPPER));
		__builtin_memcpy(path, LOADP_CUI_SETUPPER, sizeof(LOADP_CUI_SETUPPER));
		padded = 0x1c;
		pathSize = sizeof(LOADP_CUI_SETUPPER);
	}
	size = pathSize + total;
	argBuf = SceSysmemForDriver_7B4CB60A(loadp.heap, size);
	if (argBuf == NULL) {
		result = LOADP_RESULT_NO_MEMORY;
		goto free_path;
	}
	memcpy(argBuf, path, pathSize);
	p = argBuf + pathSize;
	for (i = 0; i < argc; i++) {
		n = strnlen(argv[i], total) + 1;
		memcpy(p, argv[i], n);
		total -= n;
		p += n;
	}

	debug = flags & LOADP_SPAWN_DEBUG;
	appFlags = debug ? 0x80000000 : 0;
	memset(&param, 0, sizeof(param));
	param.size = sizeof(param);
	if (flags & LOADP_SPAWN_SUSPEND) {
		value = 0x40000000;
		if (attr & (1 << 9))
			value = 0x50000000;
	} else {
		value = 0;
		if (attr & (1 << 9))
			value = 0x10000000;
	}
	if (attr & (1 << 10))
		value |= 0x20000000;
	param.flags = value | 0x80000000;
	param.priority = priority;
	param.stackSize = stackSize;

	if (launchParam == NULL) {
		if (!SceSysrootForDriver_55392965() && !SceQafMgrForDriver_B9770A13() &&
		    !SceQafMgrForDriver_082A4FC2()) {
			result = pdLoadpKernelErrToLoadpErr(SCE_ERROR_ERRNO_ENOSYS);
			goto free_args;
		}
		gxmDebug = 0;
		if (sceRegMgrGetKeyInt("/DEVENV/TOOL", "game_uses_gxmdbg", &value) == 0 && value != 0)
			gxmDebug = 1;
		pid = sceKernelSpawnProcessExt("main", debug ? 0x81000000 : 0x01000000, path, size,
					       argBuf, &param, gxmDebug);
		if (pid < 0) {
			result = pdLoadpKernelErrToLoadpErr(pid);
			goto free_args;
		}
		if (debug && !SceSblACMgrForDriver_4CBD6156(pid)) {
			ret = sceKernelKillProcess(pid, 1);
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 3847, "sceKernelKillProcess failed 0x%08x\n", ret);
			result = LOADP_RESULT_DENIED;
			goto free_args;
		}
	} else {
		if (SceModulemgrForDriver_BBE1771C("SceAppMgr") < 0) {
			result = pdLoadpKernelErrToLoadpErr(SCE_ERROR_ERRNO_ENOSYS);
			goto free_args;
		}
		/* appmgr passes the parameter on to process creation as is */
		pid = sceAppMgrLaunchAppForDebugger(path, argBuf, size, appFlags,
						(const SceAppMgrLaunchParam *)&param, (void *)launchParam);
		if (pid < 0) {
			result = pdLoadpKernelErrToLoadpErr(pid);
			goto free_args;
		}
		if (debug && !SceSblACMgrForDriver_4CBD6156(pid)) {
			ret = sceAppMgrKillAppForDebugger(pid);
			if (ret < 0)
				SCE_DBG_PRINTF(0xf, 3802, "sceAppMgrKillAppForDebugger failed 0x%08x\n", ret);
			result = LOADP_RESULT_DENIED;
			goto free_args;
		}
	}

	if (sendSpawnReply(pid, path, pathSize, padded, launchParam, &result) == 0) {
		time = sceKernelGetSystemTimeWide();
		loadpNotifyProcessCreate(-1, pid, time);
		if (debug)
			SceSysrootForDriver_F4340469(-1, pid, time);
		sceKernelFreeHeapMemory(loadp.heap, argBuf);
		if (setupper)
			sceKernelFreeHeapMemory(loadp.heap, path);
		return;
	}

free_args:
	sceKernelFreeHeapMemory(loadp.heap, argBuf);
free_path:
	if (setupper)
		sceKernelFreeHeapMemory(loadp.heap, path);
	goto reply;
bad_packet:
	result = LOADP_RESULT_BAD_PACKET;
reply:
	loadpSendReply(LOADP_CMD_SPAWN + 1, LOADP_CMD_SPAWN, result);
}

/* kill a process the host may debug */
static inline __attribute__((always_inline)) void loadpKillCmd(void)
{
	LoadpPidArgs *req = (LoadpPidArgs *)loadp.recvBuf;
	LoadpSendBuf *buf;
	LoadpPidReply *r;
	SceUInt32 result;
	SceUID pids[16];
	int count, found, i, ret;
	SceUID pid;

	if (loadp.argSize < 4) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	pid = req->pid;
	if (isKernelPid(pid)) {
		result = pdLoadpKernelErrToLoadpErr(SCE_ERROR_ERRNO_EINVAL);
		goto reply;
	}
	count = 0;
	memset(pids, 0, sizeof(pids));
	ret = SceProcessmgrForKernel_EFFF2975(3, pids, sizeof(pids), &count);
	if (ret >= 0) {
		found = 0;
		for (i = 0; i < count; i++) {
			if (pids[i] != SCE_KERNEL_PROCESS_ID && pids[i] == pid) {
				found = 1;
				break;
			}
		}
		if (!found) {
			result = LOADP_RESULT_NO_PROCESS;
			goto reply;
		}
	}
	if (SceSblACMgrForDriver_4CBD6156(pid) != 1) {
		result = LOADP_RESULT_DENIED;
		goto reply;
	}
	ret = sceAppMgrKillAppForDebugger(pid);
	if (ret < 0) {
		result = pdLoadpKernelErrToLoadpErr(ret);
		goto reply;
	}
	buf = pdLoadpAllocSendBuf();
	if (buf == NULL)
		return;
	r = buf->packet;
	buf->size = loadpInitPacketInline(r, sizeof(*r));
	r->pid = pid;
	r->reply.head.cmd = LOADP_CMD_KILL + 1;
	r->reply.head.arg = LOADP_CMD_KILL;
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0)
		loadpFreeSendBuf(buf);
	return;
reply:
	loadpSendReply(LOADP_CMD_KILL + 1, LOADP_CMD_KILL, result);
}

/* the reply to a command about a title: its process and ID */
static inline void sendTitleReply(SceUInt8 cmd, SceUID pid, const char *titleId)
{
	LoadpSendBuf *buf = pdLoadpAllocSendBuf();
	LoadpTitleReply *r;

	if (buf == NULL)
		return;
	r = buf->packet;
	buf->size = loadpInitPacketInline(r, sizeof(*r));
	r->reply.result = 0;
	r->reply.head.cmd = cmd + 1;
	r->reply.head.arg = cmd;
	r->pid = pid;
	r->size = LOADP_TITLE_ID_SIZE;
	__builtin_memcpy(r->titleId, titleId, LOADP_TITLE_ID_SIZE);
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0)
		loadpFreeSendBuf(buf);
}

static inline __attribute__((always_inline)) void loadpLaunchAppCmd(void)
{
	LoadpTitleArgs *req = (LoadpTitleArgs *)loadp.recvBuf;
	char titleId[LOADP_TITLE_ID_SIZE] = { 0 };
	SceUInt32 result;
	SceUID pid;
	int ret;

	if (req->head.arg != 0 || loadp.argSize < 0x10) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	memcpy(titleId, req->titleId, req->size);
	if (strncmp(titleId, LOADP_PROTECTED_TITLE, LOADP_TITLE_ID_SIZE) == 0) {
		result = LOADP_RESULT_DENIED;
		goto reply;
	}
	ret = sceAppMgrLaunchAppByNameForDebugger(titleId, 0);
	if (ret >= 0)
		ret = sceAppMgrGetPidByNameForDebugger(titleId, &pid);
	if (ret >= 0) {
		sendTitleReply(LOADP_CMD_LAUNCH_APP, pid, titleId);
		return;
	}
	result = pdLoadpKernelErrToLoadpErr(ret);
reply:
	loadpSendReply(LOADP_CMD_LAUNCH_APP + 1, LOADP_CMD_LAUNCH_APP, result);
}

static inline __attribute__((always_inline)) void loadpKillAppCmd(void)
{
	LoadpTitleArgs *req = (LoadpTitleArgs *)loadp.recvBuf;
	char titleId[LOADP_TITLE_ID_SIZE];
	SceUInt32 result;
	SceUID pid;
	int ret;

	if (req->head.arg != 0 || loadp.argSize < 0x10) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	memcpy(titleId, req->titleId, req->size);
	ret = sceAppMgrGetPidByNameForDebugger(titleId, &pid);
	if (ret >= 0) {
		if (strncmp(titleId, LOADP_PROTECTED_TITLE, LOADP_TITLE_ID_SIZE) == 0 &&
		    SceSblACMgrForDriver_4CBD6156(pid) == 0) {
			result = LOADP_RESULT_DENIED;
			goto reply;
		}
		ret = sceAppMgrKillAppByNameForDebugger(titleId);
		if (ret >= 0) {
			sendTitleReply(LOADP_CMD_KILL_APP, pid, titleId);
			return;
		}
	}
	result = pdLoadpKernelErrToLoadpErr(ret);
reply:
	loadpSendReply(LOADP_CMD_KILL_APP + 1, LOADP_CMD_KILL_APP, result);
}

/* the process status that allows module commands */
static inline int checkProcess(SceUID pid, SceUInt32 *result)
{
	int status = 0;

	if (sceKernelGetProcessStatus(pid, &status) < 0) {
		*result = LOADP_RESULT_NO_PROCESS;
		return 0;
	}
	if (status & 0x10) {
		*result = LOADP_RESULT_PROCESS_SUSPENDED;
		return 0;
	}
	if ((status & 0xff) == 3) {
		*result = LOADP_RESULT_PROCESS_EXITING;
		return 0;
	}
	return 1;
}

static inline __attribute__((always_inline)) void loadpLoadModuleCmd(void)
{
	LoadpModuleLoadArgs *req = (LoadpModuleLoadArgs *)loadp.recvBuf;
	LoadpModuleLoadReply *r;
	LoadpSendBuf *buf;
	SceModuleCB *cb = NULL;
	SceSize pathSize, padded;
	SceUInt32 result, *tail;
	SceUID pid, target, modid;
	SceUInt64 time;
	int flags, kernel, ret;

	if (loadp.argSize < 8) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	pathSize = req->pathSize;
	flags = (req->head.arg & 2) ? LOADP_LOAD_MODULE_FLAGS_EXT : LOADP_LOAD_MODULE_FLAGS;
	pid = req->pid;
	if (pathSize > LOADP_PATH_MAX) {
		result = LOADP_RESULT_TOO_LONG;
		goto reply;
	}
	padded = (pathSize + 3) & ~3;
	if (loadp.argSize < padded + 8) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	kernel = isKernelPid(pid);
	target = kernel ? SCE_KERNEL_PROCESS_ID : pid;
	if (!checkProcess(target, &result))
		goto reply;
	req->path[pathSize - 1] = '\0';
	if (kernel) {
		if (!SceQafMgrForDriver_382C71E8())
			modid = SCE_ERROR_ERRNO_ENOENT;
		else
			modid = sceKernelLoadModule(req->path, flags, NULL);
	} else if (pathAllowed(req->path)) {
		modid = SceModulemgrForKernel_4E85022D(target, req->path, flags, NULL);
	} else {
		modid = SCE_ERROR_ERRNO_ENOENT;
	}
	if (modid < 0) {
		result = pdLoadpKernelErrToLoadpErr(modid);
		goto reply;
	}
	buf = pdLoadpAllocSendBuf();
	if (buf == NULL)
		return;
	r = buf->packet;
	buf->size = loadpInitPacketInline(r, sizeof(*r) + padded + 0x10);
	r->reply.head.cmd = LOADP_CMD_LOAD_MODULE + 1;
	r->reply.head.arg = LOADP_CMD_LOAD_MODULE;
	r->reply.result = 0;
	ret = SceModulemgrForKernel_37512E29(modid, &cb);
	if (ret < 0 || cb == NULL) {
		result = pdLoadpKernelErrToLoadpErr(ret);
		loadpFreeSendBuf(buf);
		goto reply;
	}
	r->pid = pid;
	r->modid = kernel ? modid : cb->puid;
	r->version = *(SceUInt16 *)cb->version;
	r->sdkVersion = cb->sdkVersion;
	r->fingerprint = cb->fingerprint;
	r->start = cb->moduleStart;
	r->stop = cb->moduleStop;
	r->exit = cb->moduleExit;
	r->reserved[0] = 0;
	__builtin_memcpy(r->name, cb->name, sizeof(r->name));
	setSegments(&r->segments, cb);
	r->pathSize = pathSize;
	memcpy(r->path, req->path, pathSize);
	memset(r->path + pathSize, 0, padded - pathSize);
	tail = (SceUInt32 *)(r->path + padded);
	tail[0] = (SceUInt32)cb->exidxTop;
	tail[1] = (SceUInt32)cb->exidxBtm;
	tail[2] = (SceUInt32)cb->extabStart;
	tail[3] = (SceUInt32)cb->extabEnd;
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0) {
		loadpFreeSendBuf(buf);
		return;
	}
	time = sceKernelGetSystemTimeWide();
	if (SceSysrootForDriver_6050A467(target) == 0) {
		if (kernel)
			loadpNotifyModuleLoad(pid, modid, time);
	} else {
		loadpNotifyModuleLoad(target, modid, time);
		SceSysrootForDriver_CA497324(target, modid, time);
	}
	return;
reply:
	loadpSendReply(LOADP_CMD_LOAD_MODULE + 1, LOADP_CMD_LOAD_MODULE, result);
}

/* the arguments of a start or stop: the strings packed together */
static inline const char *moduleArgs(LoadpModuleArgs *req, SceSize *size)
{
	SceSize avail, i = 0;
	char *dst;
	int n;

	*size = 0;
	if (loadp.argSize < 0xc)
		return NULL;
	n = req->argc;
	if (n == 0)
		return NULL;
	avail = loadp.argSize - 0xc;
	dst = req->args;
	do {
		for (;;) {
			if (i >= avail)
				goto done;
			if ((*dst++ = req->args[i]) == '\0')
				break;
			i++;
		}
		i = (i + 4) & ~3;
	} while (--n != 0);
done:
	*size = dst - req->args;
	return req->args;
}

static inline void sendModuleReply(SceUInt8 cmd, SceUID pid, SceUID modid, int *sent)
{
	LoadpSendBuf *buf = pdLoadpAllocSendBuf();
	LoadpModuleReply *r;

	*sent = 0;
	if (buf == NULL)
		return;
	r = buf->packet;
	buf->size = loadpInitPacket(r, sizeof(*r));
	r->pid = pid;
	r->reply.head.cmd = cmd + 1;
	r->modid = modid;
	r->reply.head.arg = cmd;
	r->reply.result = 0;
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0) {
		loadpFreeSendBuf(buf);
		return;
	}
	*sent = 1;
}

/* the module's GUID in the process */
static inline SceUID moduleUid(SceUID target, SceUID modid, int kernel, SceUInt32 *result)
{
	SceUID uid = kernel ? modid : SceSysmemForDriver_45D22597(target, modid);

	if (uid == (SceUID)SCE_KERNEL_ERROR_INVALID_PID)
		*result = pdLoadpKernelErrToLoadpErr(SCE_KERNEL_ERROR_INVALID_PID);
	else if (uid < 0)
		*result = LOADP_RESULT_NO_MODULE;
	return uid;
}

static inline __attribute__((always_inline)) void loadpStartModuleCmd(void)
{
	LoadpModuleArgs *req = (LoadpModuleArgs *)loadp.recvBuf;
	SceUID pid, modid, target, uid;
	SceUInt32 result;
	const char *args;
	SceSize size;
	SceUInt64 time;
	int kernel, status = 0, sent, ret;

	if (loadp.argSize < 8) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	args = moduleArgs(req, &size);
	pid = req->pid;
	modid = req->modid;
	kernel = isKernelPid(pid);
	target = kernel ? SCE_KERNEL_PROCESS_ID : pid;
	if (!checkProcess(target, &result))
		goto reply;
	uid = moduleUid(target, modid, kernel, &result);
	if (uid < 0)
		goto reply;
	if (kernel) {
		if (!SceQafMgrForDriver_382C71E8())
			ret = SCE_ERROR_ERRNO_ENOENT;
		else
			ret = sceKernelStartModule(uid, size, (void *)args, 0, NULL, &status);
	} else {
		ret = SceModulemgrForKernel_3FE47DDF(target, uid, size, (void *)args, 0, NULL, &status);
	}
	if (ret < 0) {
		result = pdLoadpKernelErrToLoadpErr(ret);
		goto reply;
	}
	sendModuleReply(LOADP_CMD_START_MODULE, pid, modid, &sent);
	if (!sent)
		return;
	time = sceKernelGetSystemTimeWide();
	if (SceSysrootForDriver_6050A467(target) == 0) {
		if (kernel && status == SCE_KERNEL_START_NO_RESIDENT)
			loadpNotifyModuleUnload(pid, modid, time);
	} else if (status == SCE_KERNEL_START_NO_RESIDENT) {
		loadpNotifyModuleUnload(target, modid, time);
		if (status == SCE_KERNEL_START_NO_RESIDENT)
			SceSysrootForDriver_AA8D34EE(target, modid, time);
	}
	return;
reply:
	loadpSendReply(LOADP_CMD_START_MODULE + 1, LOADP_CMD_START_MODULE, result);
}

static inline __attribute__((always_inline)) void loadpStopModuleCmd(void)
{
	LoadpModuleArgs *req = (LoadpModuleArgs *)loadp.recvBuf;
	SceUID pid, modid, target, uid;
	SceUInt32 result;
	const char *args;
	SceSize size;
	int kernel, status, sent, ret;

	if (loadp.argSize < 8) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	args = moduleArgs(req, &size);
	pid = req->pid;
	modid = req->modid;
	kernel = isKernelPid(pid);
	target = kernel ? SCE_KERNEL_PROCESS_ID : pid;
	if (!checkProcess(target, &result))
		goto reply;
	uid = moduleUid(target, modid, kernel, &result);
	if (uid < 0)
		goto reply;
	if (!kernel) {
		ret = sceKernelStopRemoteModule(target, uid, size, (void *)args, 0, NULL, &status);
	} else if (!SceQafMgrForDriver_382C71E8()) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		ret = SceModulemgrForDriver_100DAEB9(uid, size, (void *)args, 0, NULL, &status);
	}
	if (ret < 0) {
		result = pdLoadpKernelErrToLoadpErr(ret);
		goto reply;
	}
	sendModuleReply(LOADP_CMD_STOP_MODULE, pid, modid, &sent);
	return;
reply:
	loadpSendReply(LOADP_CMD_STOP_MODULE + 1, LOADP_CMD_STOP_MODULE, result);
}

static inline __attribute__((always_inline)) void loadpUnloadModuleCmd(void)
{
	LoadpModuleArgs *req = (LoadpModuleArgs *)loadp.recvBuf;
	SceUID pid, modid, target, uid;
	SceUInt32 result;
	SceUInt64 time;
	int kernel, sent, ret;

	if (loadp.argSize < 8) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	pid = req->pid;
	modid = req->modid;
	kernel = isKernelPid(pid);
	target = kernel ? SCE_KERNEL_PROCESS_ID : pid;
	if (!checkProcess(target, &result))
		goto reply;
	uid = moduleUid(target, modid, kernel, &result);
	if (uid < 0)
		goto reply;
	if (!kernel) {
		ret = SceModulemgrForKernel_FCA9FDB1(target, uid, 0x40000000, NULL);
	} else if (!SceQafMgrForDriver_382C71E8()) {
		ret = SCE_ERROR_ERRNO_ENOENT;
	} else {
		ret = sceKernelUnloadModule(uid, 0, NULL);
	}
	if (ret < 0) {
		result = pdLoadpKernelErrToLoadpErr(ret);
		goto reply;
	}
	sendModuleReply(LOADP_CMD_UNLOAD_MODULE, pid, modid, &sent);
	if (!sent)
		return;
	time = sceKernelGetSystemTimeWide();
	if (SceSysrootForDriver_6050A467(target) == 0) {
		if (kernel)
			loadpNotifyModuleUnload(pid, modid, time);
	} else {
		loadpNotifyModuleUnload(target, modid, time);
		SceSysrootForDriver_AA8D34EE(target, modid, time);
	}
	return;
reply:
	loadpSendReply(LOADP_CMD_UNLOAD_MODULE + 1, LOADP_CMD_UNLOAD_MODULE, result);
}

/* a tool app control command (1-4) for a title, NPXS10998 by default */
static inline __attribute__((always_inline)) void loadpToolAppCmd(SceUInt8 cmd, int kind)
{
	LoadpTitleArgs *req = (LoadpTitleArgs *)loadp.recvBuf;
	char titleId[LOADP_TITLE_ID_SIZE];
	LoadpSendBuf *buf;
	LoadpReply *r;
	SceUInt32 result;
	int ret;

	if (req->head.arg != 0) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	if (loadp.packetSize != sizeof(LoadpHeader) && loadp.packetSize <= 0x23) {
		result = LOADP_RESULT_INVALID_SIZE;
		goto reply;
	}
	if (loadp.packetSize == sizeof(LoadpHeader)) {
		__builtin_memcpy(titleId, LOADP_TOOL_APP_DEFAULT, sizeof(titleId));
	} else {
		if (loadp.argSize < 0x10) {
			result = LOADP_RESULT_BAD_PACKET;
			goto reply;
		}
		__builtin_memset(titleId, 0, sizeof(titleId));
		if (memcmp(titleId, req->titleId, sizeof(titleId)) == 0)
			__builtin_memcpy(titleId, LOADP_TOOL_APP_DEFAULT, sizeof(titleId));
		else
			memcpy(titleId, req->titleId, req->size);
	}
	ret = SceAppMgrForDriver_3B4F305B(kind, titleId);
	if (ret < 0) {
		result = pdLoadpKernelErrToLoadpErr(ret);
		goto reply;
	}
	buf = pdLoadpAllocSendBuf();
	if (buf == NULL) {
		result = 0;
		goto reply;
	}
	r = buf->packet;
	buf->size = loadpInitPacketInline(r, sizeof(*r));
	r->head.cmd = cmd + 1;
	r->head.arg = cmd;
	if (pdLoadpReqSend(LOADP_CONTEXT_ARG buf) < 0)
		loadpFreeSendBuf(buf);
	return;
reply:
	loadpSendReply(cmd + 1, cmd, result);
}

/*
 * Launch a system tool app for the host and wait for its launch result; the
 * host hears of it, and of its end (loadpOnProcessExit / Kill), through the
 * process events. In each spawn command; the debug records name the command.
 */
#define LOADP_RUN_TOOL_APP(_cmd, _self, _args, _argsSize, _lWait, _lKill, _onSendFail,	\
			   _lFreeInfo1, _lFreeArgs1, _lFreeInfo2, _lFreeArgs2) do {	\
	SceKernelProcessOpt param;								\
	LoadpSendBuf *buf;								\
	LoadpPidReply *r;								\
	AppmgrLaunchOptions *info;						\
	SceUID pid;									\
	int ret;									\
											\
	memset(&param, 0, sizeof(param));						\
	param.size = sizeof(param);							\
	param.flags = 0x80000000;							\
	info = SceSysmemForDriver_7B4CB60A(loadp.heap, LOADP_LAUNCH_INFO_SIZE);		\
	if (info == NULL) {								\
		result = LOADP_RESULT_NO_MEMORY;					\
		goto free_args;								\
	}										\
	memset(info, 0, LOADP_LAUNCH_INFO_SIZE);					\
	info->size = LOADP_LAUNCH_INFO_SIZE;					\
	pid = sceAppMgrLaunchAppForDebugger(_self, _args, _argsSize, 0,			\
					(const SceAppMgrLaunchParam *)&param, info);	\
	if (pid < 0) {									\
		result = pdLoadpKernelErrToLoadpErr(pid);					\
		goto free_info;								\
	}										\
	ret = sceKernelWaitSema(loadp.sema, 1, NULL);					\
	if (ret < 0)									\
		SCE_PRINTF_WITH_INFO(0xf, _lWait, "0x%08x\n", ret);			\
	if (loadpAppResult != 0) {							\
		ret = sceAppMgrKillAppForDebugger(pid);					\
		if (ret < 0)								\
			SCE_DBG_PRINTF(0xf, _lKill,					\
				       "sceAppMgrKillAppForDebugger failed 0x%08x\n", ret);	\
		result = pdLoadpKernelErrToLoadpErr(loadpAppResult);				\
		goto free_info;								\
	}										\
	buf = pdLoadpAllocSendBuf();							\
	if (buf == NULL) {								\
		result = 0;								\
		goto free_info;								\
	}										\
	r = buf->packet;								\
	if ((_cmd) == LOADP_CMD_PACKAGE_INSTALL)					\
		buf->size = loadpInitPacketInline(r, sizeof(*r));			\
	else									\
		buf->size = loadpInitPacket(r, sizeof(*r));			\
	r->reply.result = 0;								\
	r->pid = pid;									\
	loadp.appPid = pid;								\
	r->reply.head.cmd = (_cmd) + 1;							\
	r->reply.head.arg = (_cmd);							\
	loadp.appCmd = (_cmd);								\
	ret = pdLoadpReqSend(LOADP_CONTEXT_ARG buf);							\
	if (ret >= 0) {									\
		loadpNotifyProcessCreate(-1, pid, sceKernelGetSystemTimeWide());	\
		ret = sceKernelFreeHeapMemory(loadp.heap, info);			\
		if (ret < 0)								\
			SCE_PRINTF_WITH_INFO(0xf, _lFreeInfo1, "0x%08x\n", ret);	\
		ret = sceKernelFreeHeapMemory(loadp.heap, _args);			\
		if (ret < 0)								\
			SCE_PRINTF_WITH_INFO(0xf, _lFreeArgs1, "0x%08x\n", ret);	\
		return;									\
	}										\
	_onSendFail;									\
	loadpFreeSendBuf(buf);								\
	result = 0;									\
free_info:										\
	ret = sceKernelFreeHeapMemory(loadp.heap, info);				\
	if (ret < 0)									\
		SCE_PRINTF_WITH_INFO(0xf, _lFreeInfo2, "0x%08x\n", ret);		\
free_args:										\
	ret = sceKernelFreeHeapMemory(loadp.heap, _args);				\
	if (ret < 0)									\
		SCE_PRINTF_WITH_INFO(0xf, _lFreeArgs2, "0x%08x\n", ret);		\
} while (0)

/* "spawn.self -budget mini" */
static inline void spawnSelfArgs(char *args)
{
	__builtin_memcpy(args, LOADP_SPAWN_SELF "\0-budget\0mini", 0x2a);
}

static inline __attribute__((always_inline)) void pdLoadpPackageInstallerSpawnCmd(void)
{
	LoadpPathArgs *req = (LoadpPathArgs *)loadp.recvBuf;
	SceUInt32 result;
	SceSize size;
	char *args;

	if (req->head.arg != 0 || (loadpAppResult = 0, loadp.argSize < 4)) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	if (loadp.appPid != -1) {
		result = LOADP_RESULT_BUSY;
		goto reply;
	}
	if (req->size > LOADP_PATH_MAX) {
		result = LOADP_RESULT_TOO_LONG;
		goto reply;
	}
	req->path[req->size - 1] = '\0';
	size = req->size + 0x2a;
	args = SceSysmemForDriver_7B4CB60A(loadp.heap, size);
	if (args == NULL) {
		result = LOADP_RESULT_NO_MEMORY;
		goto reply;
	}
	memset(args, 0, size);
	spawnSelfArgs(args);
	memcpy(args + 0x2a, req->path, req->size);
	LOADP_RUN_TOOL_APP(LOADP_CMD_PACKAGE_INSTALL, LOADP_SPAWN_SELF, args, size, 2311, 2319,
			   SCE_PRINTF_WITH_INFO(0xf, 2348, "0x%08x\n", ret), 2359, 2364, 2378, 2384);
reply:
	loadpSendReply(LOADP_CMD_PACKAGE_INSTALL + 1, LOADP_CMD_PACKAGE_INSTALL, result);
}

static inline __attribute__((always_inline)) void pdLoadpPackageUninstallerSpawnCmd(void)
{
	LoadpPathArgs *req = (LoadpPathArgs *)loadp.recvBuf;
	SceUInt32 result;
	SceSize size;
	char *args;

	if (req->head.arg != 0 || (loadpAppResult = 0, loadp.argSize < 4)) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	if (loadp.appPid != -1) {
		result = LOADP_RESULT_BUSY;
		goto reply;
	}
	req->path[req->size - 1] = '\0';
	size = req->size + 0x35;
	args = SceSysmemForDriver_7B4CB60A(loadp.heap, size);
	if (args == NULL) {
		result = LOADP_RESULT_NO_MEMORY;
		goto reply;
	}
	memset(args, 0, size);
	spawnSelfArgs(args);
	__builtin_memcpy(args + 0x2a, "-uninstall", 11);
	memcpy(args + 0x35, req->path, req->size);
	LOADP_RUN_TOOL_APP(LOADP_CMD_PACKAGE_UNINSTALL, LOADP_SPAWN_SELF, args, size, 2492, 2500,
			   (void)0, 2539, 2544, 2558, 2564);
reply:
	loadpSendReply(LOADP_CMD_PACKAGE_UNINSTALL + 1, LOADP_CMD_PACKAGE_UNINSTALL, result);
}

static inline __attribute__((always_inline)) void pdLoadpBtPairingAppSpawnCmd(void)
{
	SceUInt32 result;
	char *args;

	if (loadp.recvBuf->arg != 0) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	if (!SceSblAIMgrForDriver_71608CA3()) {
		result = LOADP_RESULT_NOT_DOLCE;
		goto reply;
	}
	loadpAppResult = 0;
	if (loadp.appPid != -1) {
		result = LOADP_RESULT_BUSY;
		goto reply;
	}
	args = SceSysmemForDriver_7B4CB60A(loadp.heap, sizeof(LOADP_DS4_PAIRING_SELF));
	if (args == NULL) {
		result = LOADP_RESULT_NO_MEMORY;
		goto reply;
	}
	memset(args, 0, sizeof(LOADP_DS4_PAIRING_SELF));
	__builtin_memcpy(args, LOADP_DS4_PAIRING_SELF, sizeof(LOADP_DS4_PAIRING_SELF));
	LOADP_RUN_TOOL_APP(LOADP_CMD_BT_PAIRING, LOADP_DS4_PAIRING_SELF, args,
			   sizeof(LOADP_DS4_PAIRING_SELF), 2655, 2663, (void)0, 2702, 2707, 2721, 2727);
reply:
	loadpSendReply(LOADP_CMD_BT_PAIRING + 1, LOADP_CMD_BT_PAIRING, result);
}

static inline __attribute__((always_inline)) void pdLoadpSaveDataInstallerSpawnCmd(void)
{
	LoadpSaveDataArgs *req = (LoadpSaveDataArgs *)loadp.recvBuf;
	SceSize len, size;
	SceUInt32 result;
	SceUInt8 slot;
	char *args, *p;
	int kind;

	if (req->head.arg != 0 || (loadpAppResult = 0, loadp.argSize < 0xc)) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	if (loadp.appPid != -1) {
		result = LOADP_RESULT_BUSY;
		goto reply;
	}
	len = req->size;
	if (len > LOADP_PATH_MAX) {
		result = LOADP_RESULT_TOO_LONG;
		goto reply;
	}
	req->path[len - 1] = '\0';
	slot = req->slot;
	switch (req->mode & 3) {
	case 2:
		size = len + 0x35;
		kind = 1;
		break;
	case 0:
		if (slot >= 0x40) {
			result = LOADP_RESULT_INVALID_ARG;
			goto reply;
		}
		size = len + 0x3d;
		kind = 2;
		break;
	case 1:
		size = len + 0x38;
		kind = 3;
		break;
	default:
		result = LOADP_RESULT_INVALID_ARG;
		goto reply;
	}
	args = SceSysmemForDriver_7B4CB60A(loadp.heap, size);
	if (args == NULL) {
		result = LOADP_RESULT_NO_MEMORY;
		goto reply;
	}
	memset(args, 0, size);
	spawnSelfArgs(args);
	if (kind == 2) {
		p = args + 0x35;
		__builtin_memcpy(args + 0x2a, "-sd-import", 11);
		snprintf(p, len, "%s", req->path);
		__builtin_memcpy(p + len, "-u", 3);
		snprintf(p + len + 3, 5, "0x%02x", slot);
	} else if (kind == 3) {
		__builtin_memcpy(args + 0x2a, "-rawsd-import", 14);
		snprintf(args + 0x38, len, "%s", req->path);
	} else {
		__builtin_memcpy(args + 0x2a, "-sd-import", 11);
		snprintf(args + 0x35, len, "%s", req->path);
	}
	LOADP_RUN_TOOL_APP(LOADP_CMD_SAVEDATA_INSTALL, LOADP_SPAWN_SELF, args, size, 2911, 2919,
			   (void)0, 2958, 2963, 2977, 2983);
reply:
	loadpSendReply(LOADP_CMD_SAVEDATA_INSTALL + 1, LOADP_CMD_SAVEDATA_INSTALL, result);
}

static inline __attribute__((always_inline)) void pdLoadpSaveDataFormatterSpawnCmd(void)
{
	LoadpSaveDataArgs *req = (LoadpSaveDataArgs *)loadp.recvBuf;
	const char *titleId = NULL;
	SceUInt32 result;
	SceUInt8 slot;
	SceSize size;
	char *args;
	int kind;

	if (req->head.arg != 0 || (loadpAppResult = 0, loadp.argSize < 0xc)) {
		result = LOADP_RESULT_BAD_PACKET;
		goto reply;
	}
	if (loadp.appPid != -1) {
		result = LOADP_RESULT_BUSY;
		goto reply;
	}
	slot = req->slot;
	switch (req->mode & 7) {
	case 2:
		kind = 1;
		size = 0x3f;
		break;
	case 0:
		if (slot < 0x40) {
			kind = 2;
			size = 0x47;
		} else {
			kind = 1;
			size = 0;
		}
		break;
	case 4:
		kind = 3;
		size = 0x42;
		break;
	case 1:
		kind = 4;
		size = 0x42;
		break;
	default:
		kind = 1;
		size = 0;
		break;
	}
	if (kind != 4) {
		if (req->size != LOADP_TITLE_ID_SIZE) {
			result = LOADP_RESULT_INVALID_ARG;
			goto reply;
		}
		titleId = req->path;
		req->path[9] = '\0';
	}
	args = SceSysmemForDriver_7B4CB60A(loadp.heap, size);
	if (args == NULL) {
		result = LOADP_RESULT_NO_MEMORY;
		goto reply;
	}
	memset(args, 0, size);
	spawnSelfArgs(args);
	if (kind == 3) {
		__builtin_memcpy(args + 0x2a, "-sd-format", 11);
		if (titleId != NULL)
			snprintf(args + 0x35, 10, "%s", titleId);
		__builtin_memcpy(args + 0x3f, "-a", 3);
	} else if (kind == 4) {
		__builtin_memcpy(args + 0x2a, "-rawsd-format", 14);
	} else {
		__builtin_memcpy(args + 0x2a, "-sd-format", 11);
		if (kind == 2) {
			if (titleId != NULL)
				snprintf(args + 0x35, 10, "%s", titleId);
			__builtin_memcpy(args + 0x3f, "-u", 3);
			snprintf(args + 0x42, 5, "0x%02x", slot);
		} else if (titleId != NULL) {
			snprintf(args + 0x35, 10, "%s", titleId);
		}
	}
	LOADP_RUN_TOOL_APP(LOADP_CMD_SAVEDATA_FORMAT, LOADP_SPAWN_SELF, args, size, 3187, 3195,
			   (void)0, 3234, 3239, 3253, 3259);
reply:
	loadpSendReply(LOADP_CMD_SAVEDATA_FORMAT + 1, LOADP_CMD_SAVEDATA_FORMAT, result);
}

static int _loadpThread(SceSize args, void *argp)
{
	SceUInt32 bits;
	SceUInt8 cmd;
	int ret;

	(void)args;
	(void)argp;

	for (;;) {
		bits = 0;
		ret = sceKernelWaitEventFlag(loadp.evf, 1,
					      SCE_KERNEL_EVF_WAITMODE_OR |
					      SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
					      &bits, NULL);
		if (ret < 0) {
			SCE_PRINTF_WITH_INFO(0xf, 4604, "0x%08x\n", ret);
			return ret;
		}
		cmd = loadp.pending->cmd;
		switch (cmd) {
		case LOADP_CMD_SPAWN:
			pdLoadpProcSpawnCmd(loadp.pending->arg);
			break;
		case LOADP_CMD_KILL:
			loadpKillCmd();
			break;
		case LOADP_CMD_LAUNCH_APP:
			loadpLaunchAppCmd();
			break;
		case LOADP_CMD_KILL_APP:
			loadpKillAppCmd();
			break;
		case LOADP_CMD_LOAD_MODULE:
			loadpLoadModuleCmd();
			break;
		case LOADP_CMD_START_MODULE:
			loadpStartModuleCmd();
			break;
		case LOADP_CMD_STOP_MODULE:
			loadpStopModuleCmd();
			break;
		case LOADP_CMD_UNLOAD_MODULE:
			loadpUnloadModuleCmd();
			break;
		case LOADP_CMD_TOOL_APP_1:
			loadpToolAppCmd(LOADP_CMD_TOOL_APP_1, 1);
			break;
		case LOADP_CMD_TOOL_APP_2:
			loadpToolAppCmd(LOADP_CMD_TOOL_APP_2, 2);
			break;
		case LOADP_CMD_TOOL_APP_3:
			loadpToolAppCmd(LOADP_CMD_TOOL_APP_3, 3);
			break;
		case LOADP_CMD_TOOL_APP_4:
			loadpToolAppCmd(LOADP_CMD_TOOL_APP_4, 4);
			break;
		case LOADP_CMD_PACKAGE_INSTALL:
			pdLoadpPackageInstallerSpawnCmd();
			break;
		case LOADP_CMD_PACKAGE_UNINSTALL:
			pdLoadpPackageUninstallerSpawnCmd();
			break;
		case LOADP_CMD_BT_PAIRING:
			pdLoadpBtPairingAppSpawnCmd();
			break;
		case LOADP_CMD_SAVEDATA_INSTALL:
			pdLoadpSaveDataInstallerSpawnCmd();
			break;
		case LOADP_CMD_SAVEDATA_FORMAT:
			pdLoadpSaveDataFormatterSpawnCmd();
			break;
		default:
			loadpSendReply(cmd + 1, cmd, LOADP_RESULT_UNKNOWN_CMD);
			break;
		}
		loadp.pending = NULL;
	}
}

/* ===== start ===== */

/* Sony's LoadpModuleStart */
int module_start(SceSize args, void *argp)
{
	SceKernelHeapCreateOpt heapOpt;
	SceDfMgrProtocol protocol;
	SceKernelIntrStatus s;
	int ret;

	(void)args;
	(void)argp;

	if (!SceDipswForDriver_A98FC2FD(LOADP_DIPSW)) {
#if FW_DEBUG
		SceDebugForDriver_D9703808(1, 0xb, SCE_DBGINFO_FUNC(274, "LoadpModuleStart"),
					      "disabled by dipsw\n");
#endif
		return SCE_KERNEL_START_NO_RESIDENT;
	}
	memset(&loadp, 0, sizeof(loadp));
	ret = sceKernelCreateMutex("SceDeci4pLoadp", 0x2000, 0, NULL);
	if (ret < 0)
		return SCE_KERNEL_START_FAILED;
	loadp.mutex = ret;
	ret = sceKernelCreateEventFlag("SceDeci4pLoadp", 0, 0, NULL);
	if (ret < 0)
		return SCE_KERNEL_START_FAILED;
	loadp.evf = ret;
	{
		SceKernelThreadOptParam threadOpt;

		threadOpt.size = sizeof(threadOpt);
		threadOpt.attr = 1;
		threadOpt.kStackMemType = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
		ret = sceKernelCreateThread("SceDeci4pLoadp", _loadpThread, 0x1e, 0x4000,
					    0, 0xf, &threadOpt);
	}
	if (ret < 0)
		return SCE_KERNEL_START_FAILED;
	loadp.thread = ret;
	ret = sceKernelStartThread(loadp.thread, 0, NULL);
	if (ret < 0)
		return SCE_KERNEL_START_FAILED;
	heapOpt.size = sizeof(heapOpt);
	heapOpt.attr = 0x401;
	heapOpt.memtype = SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_TOOL_RW;
	ret = SceSysmemForDriver_9328E0E8("SceDeci4pLoadp", 0x20000, &heapOpt);
	if (ret < 0)
		return SCE_KERNEL_START_FAILED;
	loadp.heap = ret;
	loadp.recvBuf = SceSysmemForDriver_7B4CB60A(loadp.heap, LOADP_RECV_BUF_SIZE);
	if (loadp.recvBuf == NULL)
		return SCE_KERNEL_START_FAILED;
	memset(loadp.recvBuf, 0, LOADP_RECV_BUF_SIZE);
	loadp.sendHead = NULL;
	loadp.sendCount = 0;
	loadp.doneList = NULL;

	protocol.size = sizeof(protocol);
	protocol.protocol = 0x24000;
	protocol.alias = 0;
	protocol.version = 0x1010000;
	protocol.subVersion = 0x20000;
	protocol.destination = 0;
	protocol.flags = DFMGR_PROTOCOL_ENABLE;
	strncpy(protocol.name, "LOADP", sizeof(protocol.name));
	if (!SceDipswForDriver_A98FC2FD(LOADP_DIPSW_NO_AUTO_ENABLE))
		protocol.flags |= DFMGR_PROTOCOL_USB;
	else
		protocol.flags |= DFMGR_PROTOCOL_SDIO;
	ret = sceDfMgrOpen(pdLoadpHandler, &protocol, &loadp);
	if (ret < 0)
		return SCE_KERNEL_START_FAILED;
	loadp.handle = ret;
	ret = sceKernelCreateSema("SceDeci4pLoadp", 0, 0, 1, NULL);
	if (ret < 0)
		return SCE_KERNEL_START_FAILED;
	loadp.appPid = -1;
	loadp.appCmd = 0;
	loadp.sema = ret;
	SceSysrootForDriver_0F07C3FC((const SysrootProcessEventOps *)&loadpProcessHandler);
	s = SceCpuForDriver_D32ACE9E(&loadp.lock);
	loadp.flags |= LOADP_FLAG_ENABLED;
	sceKernelSpinlockLowUnlockCpuResumeIntr(&loadp.lock, s);
	return SCE_KERNEL_START_SUCCESS;
}

/* ===== syscalls (names unknown) ===== */

/* the launch parameter of SceDeci4pLoadp_9A46C1BA, from the user */
typedef struct LoadpTestLaunchParam {
	int flags;
	const char *args;			/* up to 0x400 */
	int option1;				/* passed to appmgr as is */
	int option2;
	const void *data;			/* 0x80 bytes */
	SceUInt32 reserved[5];
} LoadpTestLaunchParam;

__attribute__((noinline)) int launchAppForTest2(const char *path, int option0, const char *titleId,
							int flags, const char *args, int option1,
							int option2, const void *data)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (!SceSblACMgrForDriver_AD717E7A(0)) {
		sceSyscallLeave(t);
		return SCE_APPMGR_ERROR_NOEXEC;
	}
	ret = sceAppMgrLaunchAppForTest2(path, titleId, flags, args, option1, option2, option0, data);
	sceSyscallLeave(t);
	return ret;
}

__attribute__((noinline)) int destroyOtherApp(SceUID pid)
{
	SceUInt32 t = sceSyscallEnter();
	int ret;

	if (SceSblACMgrForDriver_AD717E7A(0)) {
		ret = sceAppMgrDestroyAppForTest(pid);
		sceSyscallLeave(t);
		return ret;
	}
	sceSyscallLeave(t);
	return SCE_APPMGR_ERROR_NOEXEC;
}

__attribute__((noinline)) int launchAppForTest(const char *path, int option0, const char *titleId,
						       int flags, const char *args, int option1, int option2,
						       const void *data)
{
	return launchAppForTest2(path, option0, titleId, flags, args, option1, option2, data);
}

__attribute__((noinline)) int destroyOtherAppSyscall(SceUID pid)
{
	return destroyOtherApp(pid);
}

int SceDeci4pLoadp_9A46C1BA(const char *path, int option0, const char *titleId,
			    const LoadpTestLaunchParam *param)
{
	LoadpTestLaunchParam p;
	char *kpath = NULL, *ktitleId = NULL, *kargs = NULL;
	void *kdata = NULL;
	const char *args;
	const void *data;
	int flags, option1, option2;
	int ret;

	ret = SceSysmemForDriver_BC996A7A(&p, param, sizeof(p));
	if (ret < 0)
		return ret;
	flags = p.flags;
	args = p.args;
	option1 = p.option1;
	option2 = p.option2;
	data = p.data;
	if (path != NULL) {
		kpath = alloca(0x124);
		ret = SceSysmemForDriver_DB3EC244(kpath, path, 0x124);
		if (ret < 0)
			return ret;
		if (ret == 0x124 && ((signed char *)kpath)[0x123] != '\0')
			return SCE_KERNEL_ERROR_NO_MEMORY;	/* Sony's code for an unterminated string here */
	}
	if (titleId != NULL) {
		ktitleId = alloca(0x20);
		ret = SceSysmemForDriver_DB3EC244(ktitleId, titleId, 0x20);
		if (ret < 0)
			return ret;
		if (ret == 0x20 && ((signed char *)ktitleId)[0x1f] != '\0')
			return SCE_KERNEL_ERROR_NO_MEMORY;	/* Sony's code for an unterminated string here */
	}
	if (args != NULL) {
		kargs = alloca(0x400);
		ret = SceSysmemForDriver_DB3EC244(kargs, args, 0x400);
		if (ret < 0)
			return ret;
		if (ret == 0x400 && ((signed char *)kargs)[0x3ff] != '\0')
			return SCE_KERNEL_ERROR_NO_MEMORY;	/* Sony's code for an unterminated string here */
	}
	if (data != NULL) {
		kdata = alloca(0x80);
		ret = SceSysmemForDriver_BC996A7A(kdata, data, 0x80);
		if (ret < 0)
			return ret;
	}
	return launchAppForTest(kpath, option0, ktitleId, flags, kargs, option1, option2, kdata);
}

int SceDeci4pLoadp_AA103B3F(SceUID pid)
{
	return destroyOtherAppSyscall(pid);
}
