/*
 * The queue of module events in a debugged process, and the module load and
 * unload notifications sent for them (Sony's module_info.c).
 */

#include <fw.h>
#include <kernel/error.h>
#include <modulemgr/modulemgr.h>
#include <modulemgr/modulemgr_control.h>
#include <sysmem/cpu.h>
#include <sysmem/debug.h>
#include <sysmem/sysclib.h>
#include <threadmgr/threadmgr.h>
#include "dbgp_internal.h"

#define MODULE_INFO_MAX		8

/* the event flag bits that wake the worker */
#define MODULE_EVENT_START	0x100
#define MODULE_EVENT_STOP	0x200

/* DBGP group 0x10: the module notifications */
#define DBGP_MODULE_GROUP	0x10
#define DBGP_MODULE_LOAD_TYPE	0xcc
#define DBGP_MODULE_UNLOAD_TYPE	0xce

/* the segments a load notification always reports */
#define DBGP_MODULE_SEGMENT_MAX	4

/* the largest notification, the size of the worker's buffers */
#define DBGP_MODULE_LOAD_MAX	0x11000

/* the longest module path a notification carries */
#define DBGP_MODULE_PATH_MAX	256

#define ALIGN4(x)		(((x) + 3) & ~3)

typedef struct DbgpModuleSegment {
	SceSize size;			/* sizeof(DbgpModuleSegment) */
	SceUInt32 attr;			/* perms | extraPages << 20 */
	void *vaddr;
	SceSize memsz;
	SceSize align;
} DbgpModuleSegment;

/* a load notification after its header; the path follows, then the metadata */
typedef struct DbgpModuleLoadInfo {
	SceUID pid;
	SceUID modid;
	DbgpTime time;
	SceUInt8 version[2];
	char name[26];
	SceUInt32 fingerprint;
	SceUInt32 segmentCount;
	DbgpModuleSegment segment[DBGP_MODULE_SEGMENT_MAX];
	SceSize pathSize;
	char path[];
} DbgpModuleLoadInfo;

typedef struct DbgpModuleLoadNotification {
	DbgpResponseHeader header;
	DbgpModuleLoadInfo info;
} DbgpModuleLoadNotification;

typedef struct DbgpModuleUnloadNotification {
	DbgpResponseHeader header;
	SceUID pid;
	SceUID modid;
	DbgpTime time;
} DbgpModuleUnloadNotification;

_Static_assert(sizeof(DbgpModuleLoadInfo) == 0x88, "module load notification body");
_Static_assert(sizeof(DbgpModuleUnloadNotification) == 0x2c, "module unload notification");

static SceKernelSpinlock s_moduleInfoLock;

/* a function of its own in 4.00; the 3.6x builds inline it into both callers */
#if FW_SINCE(0x04000000)
static void
#else
static inline __attribute__((always_inline)) void
#endif
moduleInfoFillLoadNotification(const SceModuleCB *module, SceSize pathSize,
			       DbgpModuleLoadInfo *body, SceSize metadataSize)
{
	const SceModuleSegment *segment;
	char *p;
	SceSize pathAligned;
	int i;

	body->modid = module->puid;
	body->version[0] = module->version[0];
	body->version[1] = module->version[1];
	__builtin_memcpy(body->name, module->name, sizeof(body->name));
	body->fingerprint = module->fingerprint;
	body->segmentCount = DBGP_MODULE_SEGMENT_MAX;
	/* the debugger reads a fourth record past SceModuleCB.segment */
	segment = module->segment;
	for (i = 0; i < DBGP_MODULE_SEGMENT_MAX; i++) {
		body->segment[i].size = sizeof(DbgpModuleSegment);
		if (i < module->nSegments) {
			body->segment[i].attr = segment[i].perms |
						segment[i].extraPages << 20;
			body->segment[i].vaddr = segment[i].vaddr;
			body->segment[i].memsz = segment[i].memsz;
			body->segment[i].align = 1 << segment[i].alignLog2;
		}
	}
	body->pathSize = pathSize;
	p = body->path;
	memcpy(p, module->path, pathSize - 1);
	pathAligned = ALIGN4(pathSize);
	memset(&body->path[pathSize - 1], 0, pathAligned - pathSize + 1);
	*(SceSize *)(p + pathAligned) = metadataSize;
	p += pathAligned;
	if (metadataSize != 0) {
		p += sizeof(SceSize);
		memcpy(p, module->metadata, metadataSize);
		memset(p + metadataSize, 0, ALIGN4(metadataSize) - metadataSize);
	}
}

int moduleInfoSave(DbgpProcCB *proc, const ModuleInfo *info)
{
	ModuleInfoBuf *buf;
	SceKernelIntrStatus intr;
	SceUInt32 event;
	int write, result;

	buf = &proc->prxInfoBuf;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_moduleInfoLock);
	if (buf->count > MODULE_INFO_MAX - 1) {
		sceKernelSpinlockLowUnlockCpuResumeIntr(&s_moduleInfoLock, intr);
		return (int)0x8008000b;
	}
	write = buf->write;
	buf->count++;
	buf->info[write] = *info;
	if (info->type != MODULE_INFO_START)
		event = MODULE_EVENT_STOP;
	else
		event = MODULE_EVENT_START;
	write++;
	if (write > MODULE_INFO_MAX - 1)
		write = 0;
	buf->write = write;
	if (buf->count > 0) {
		result = sceKernelSetEventFlag(proc->eventFlag, event);
		if (result < 0)
			SCE_PRINTF_WITH_INFO(15, 91, "0x%08x\n", result);
	} else {
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_moduleInfoLock, intr);
	return result;
}

int moduleInfoLoad(DbgpProcCB *proc, ModuleInfo *info)
{
	ModuleInfoBuf *buf;
	SceKernelIntrStatus intr;
	int read, count, result;

	buf = &proc->prxInfoBuf;
	intr = sceKernelSpinlockLowLockCpuSuspendIntr(&s_moduleInfoLock);
	count = buf->count;
	if (count <= 0) {
		result = (int)0x80080008;
	} else {
		read = buf->read;
		*info = buf->info[read];
		read++;
		if (read > MODULE_INFO_MAX - 1)
			read = 0;
		buf->read = read;
		buf->count = count - 1;
		result = 0;
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&s_moduleInfoLock, intr);
	return result;
}

int moduleInfoGetCount(DbgpProcCB *proc)
{
	return proc->prxInfoBuf.count;
}

int _createModuleLoadNotification(DbgpProcCB *proc, void *packet,
				  const ModuleInfo *info)
{
	DbgpModuleLoadNotification *load;
	DbgpModuleUnloadNotification *unload;
	SceModuleCB *module, *object;
	DbgpTime time;
	SceUID pid, modid;
	SceSize pathLen, pathAligned, metadataSize, size;
	int result;

	if (packet == NULL || info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->type == MODULE_INFO_START) {
		result = SceModulemgrForKernel_37512E29(info->modid, &object);
		if (result < 0)
			return result;
		SCE_DBG_ASSERT(object != NULL, 192);
		module = object;
		pathLen = strnlen(module->path, DBGP_MODULE_PATH_MAX);
		metadataSize = module->metadataSize;
		if (pathLen >= DBGP_MODULE_PATH_MAX - 1)
			pathLen = DBGP_MODULE_PATH_MAX - 1;
		pathAligned = ALIGN4(pathLen + 1);
		size = ALIGN4(metadataSize) + sizeof(DbgpModuleLoadNotification) +
		       sizeof(SceSize) + pathAligned;
		if (metadataSize > size || size > DBGP_MODULE_LOAD_MAX) {
			size = DBGP_MODULE_LOAD_MAX;
			metadataSize = DBGP_MODULE_LOAD_MAX -
				       sizeof(DbgpModuleLoadNotification) -
				       sizeof(SceSize) - pathAligned;
		}
		load = packet;
		memset(load, 0, size);
		_dbgpSetSendSize(proc, size);
		dbgpFillResponseHeader(proc, DBGP_MODULE_GROUP,
				       DBGP_MODULE_LOAD_TYPE, 0, 0);
		time = info->time;
		load->info.pid = info->pid;
		load->info.time = time;
		moduleInfoFillLoadNotification(object, pathLen + 1, &load->info,
					       metadataSize);
		return 0;
	}
	if (info->type == MODULE_INFO_STOP) {
		unload = packet;
		memset(unload, 0, sizeof(*unload));
		_dbgpSetSendSize(proc, sizeof(*unload));
		dbgpFillResponseHeader(proc, DBGP_MODULE_GROUP,
				       DBGP_MODULE_UNLOAD_TYPE, 0, 0);
		pid = info->pid;
		modid = info->modid;
		time = info->time;
		unload->pid = pid;
		unload->modid = modid;
		unload->time = time;
		return 0;
	}
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}

int moduleInfoSendPrxLoadNotificationFake(DbgpProcCB *proc)
{
	DbgpModuleLoadNotification *load;
	SceModuleCB *module;
	SceUID *threads;
	SceUID pid;
	SceSize pathLen, pathAligned, metadataSize, size;
	int result;

	pid = proc->pid;
	result = SceModulemgrForKernel_37512E29(proc->prxModid, &module);
	if (result < 0)
		return result;
	SCE_DBG_ASSERT(module != NULL, 287);
	pathLen = strnlen(module->path, DBGP_MODULE_PATH_MAX);
	if (pathLen >= DBGP_MODULE_PATH_MAX - 1)
		pathLen = DBGP_MODULE_PATH_MAX - 1;
	threads = _dbgpAllocWorkBuf(proc, DBGP_MODULE_LOAD_MAX);
	if (threads == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	if (processWaitThreadSuspendForProcess(pid, threads, 1) < 0)
		return (int)0x80080001;
	pathAligned = ALIGN4(pathLen + 1);
	metadataSize = module->metadataSize;
	size = ALIGN4(metadataSize) + sizeof(DbgpModuleLoadNotification) +
	       sizeof(SceSize) + pathAligned;
	if (size < metadataSize || size > DBGP_MODULE_LOAD_MAX) {
		size = DBGP_MODULE_LOAD_MAX;
		metadataSize = DBGP_MODULE_LOAD_MAX -
			       sizeof(DbgpModuleLoadNotification) -
			       sizeof(SceSize) - pathAligned;
	}
	load = _dbgpAllocSendBuf(proc, size);
	if (load == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	memset(load, 0, size);
	_dbgpSetSendSize(proc, size);
	dbgpFillResponseHeader(proc, DBGP_MODULE_GROUP, DBGP_MODULE_LOAD_TYPE,
			       0, 0);
	load->info.pid = pid;
	load->info.time = proc->time;
	moduleInfoFillLoadNotification(module, pathLen + 1, &load->info,
				       metadataSize);
	return 0;
}
