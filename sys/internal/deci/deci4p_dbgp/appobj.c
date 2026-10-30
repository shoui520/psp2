/*
 * The application's user-level objects: fibers, ULT runtimes and their ULTs,
 * and the ULT synchronisation objects, read through SceUlobjMgr's info array
 * (Sony's appobj.c).
 */

#include <kernel/error.h>
#include <threadmgr/threadmgr.h>
#include <threadmgr/threadmgr_debugger.h>
#include <sysmem/sysmem.h>
#include <sysmem/sysclib.h>
#include <sysmem/debug.h>
#include <ulobjmgr/ulobjmgr.h>
#include "dbgp_internal.h"

#define INLINE static inline __attribute__((always_inline))

/* the DBGP record of a fiber, of a ULT runtime and of a ULT */
#define DBGP_APPOBJ_FIBER		0x10
#define DBGP_APPOBJ_ULT_RUNTIME		0x20

/* ULT ID tables are read this many entries at a time */
#define ULT_TABLE_CHUNK			1024

/* where libult keeps a thread's current fiber context, below its TLS */
#define ULT_TLS_FIBER_CONTEXT		0x6fc

/* the fill value sceFiberStartContextSizeCheck writes into a context */
#define ULT_FIBER_CONTEXT_FILL		0xeeeeeeeeeeeeeeeeULL

/* a VFP register in a packet record: records are only 4-byte aligned */
typedef SceUInt64 DbgpVfpRegister __attribute__((aligned(4)));

typedef struct DbgpFiberRegisters {
	SceUInt32 fpscr;
	SceUInt32 reg[8];
	SceUInt32 sp;
	SceUInt32 lr;
	DbgpVfpRegister vfp[8];
} DbgpFiberRegisters;

/* the registers of the thread a running fiber is on */
typedef struct DbgpFiberThreadContext {
	SceSize size;
	DbgpFiberRegisters regs;
} DbgpFiberThreadContext;

typedef struct DbgpFiberInfo {
	SceSize size;			/* with the thread context, if any */
	SceUIntPtr address;
	char name[32];
	SceInt32 state;
	SceUIntPtr addrContext;
	SceUInt32 sizeContext;
	SceUInt32 entry;
	SceUInt32 threadId;
	DbgpFiberRegisters regs;
	DbgpFiberThreadContext thread[];
} DbgpFiberInfo;

typedef struct DbgpUltInfo {
	SceSize size;
	SceUIntPtr address;
	char name[32];
	SceInt32 state;
	SceUInt32 opaque0;
	SceUInt32 opaque1;
	SceUInt32 threadId;
	SceUInt32 opaque2;
	SceUInt32 reserved;
	SceUInt32 fiberIndex;		/* its fiber's place among the fibers sent */
} DbgpUltInfo;

typedef struct DbgpUltRuntimeInfo {
	SceSize size;			/* with its ULTs */
	SceUIntPtr address;
	char name[32];
	SceUInt32 opaque;
	SceUInt32 numUlts;
	SceUInt32 numUsedUlts;
	DbgpUltInfo ult[];
} DbgpUltRuntimeInfo;

typedef struct DbgpAppObjectSection {
	SceSize size;			/* with the header */
	SceUInt32 type;
	SceUInt32 count;
} DbgpAppObjectSection;

typedef struct DbgpGetAppObjectResponse {
	DbgpResponseHeader header;
	SceUInt32 numSections;
	SceUInt32 reserved0;
	DbgpAppObjectSection fibers;
	SceUInt32 reserved1;
	DbgpFiberInfo fiber[];		/* then the ULT runtime section */
} DbgpGetAppObjectResponse;

typedef struct DbgpFiberStackInfo {
	SceSize size;
	SceUIntPtr address;
	SceUInt32 maxUsed;		/* -1: no size check */
	SceUInt32 used;
} DbgpFiberStackInfo;

typedef struct DbgpGetFiberStackInfoResponse {
	DbgpResponseHeader header;
	SceUInt32 count;
	DbgpFiberStackInfo info[];
} DbgpGetFiberStackInfoResponse;

/* a fiber's name, decoded */
typedef union DbgpFiberName {
	SceUInt64 word[4];
	char c[32];
} DbgpFiberName;

/* where a fiber, then its saved context, is read to */
typedef union DbgpUltObjectBuffer {
	UltFiber fiber;
	UltFiberContext context;
} DbgpUltObjectBuffer;

/* the work buffer: a chunk of a ULT table, then one fiber or context */
typedef struct DbgpAppObjectWork {
	SceUIntPtr table[ULT_TABLE_CHUNK];
	DbgpUltObjectBuffer object;
} DbgpAppObjectWork;

_Static_assert(sizeof(DbgpFiberRegisters) == 108, "fiber registers record");
_Static_assert(sizeof(DbgpFiberInfo) == 168, "fiber record");
_Static_assert(sizeof(DbgpFiberThreadContext) == 112, "fiber thread record");
_Static_assert(sizeof(DbgpUltInfo) == 68, "ULT record");
_Static_assert(sizeof(DbgpUltRuntimeInfo) == 52, "ULT runtime record");
_Static_assert(offsetof(DbgpGetAppObjectResponse, fiber) == 52, "app object response");
_Static_assert(sizeof(DbgpAppObjectWork) == 0x1080, "app object work buffer");

/* Ult.state -> DbgpUltInfo.state */
static const SceUInt32 s_ultStateTable[] = { 0, 1, 4, 5, 3, 2 };

/*
 * the types summed when none are given: Sony's table holds SceUlobjMgr's
 * object types, not the info types _getUltSyncObjInfoSizeOfType takes, so
 * only ULOBJ_SYNC_OBJ_COND (ULOBJ_INFO_SEMA's value) is counted
 */
static const int s_ultSyncObjInfoTypes[] = {
	ULOBJ_SYNC_OBJ_SEMA,
	ULOBJ_SYNC_OBJ_MUTEX,
	ULOBJ_SYNC_OBJ_QDRP,
	ULOBJ_SYNC_OBJ_WQRP,
	ULOBJ_SYNC_OBJ_QUEUE,
	ULOBJ_SYNC_OBJ_RWLOCK,
	ULOBJ_SYNC_OBJ_COND,
};

INLINE int expectNumUlobjsUnderUltRuntime(SceUID pid, SceUIntPtr address,
					  SceUInt32 *num, SceUIntPtr *table)
{
	UltRuntime runtime;
	SceUIntPtr ults;
	SceUInt32 remaining, chunk, i;

	SCE_DBG_ASSERT(num != NULL, 381);
	SCE_DBG_ASSERT(table != NULL, 382);
	if (sceKernelCopyFromUserProc(pid, &runtime, (const void *)address,
				      sizeof(runtime)) == 0) {
		remaining = runtime.numUlts;
		ults = runtime.ultTable;
		while (remaining != 0) {
			chunk = remaining < ULT_TABLE_CHUNK ? remaining : ULT_TABLE_CHUNK;
			if (sceKernelCopyFromUserProc(pid, table, (const void *)ults,
						      chunk * sizeof(*table)) != 0)
				break;
			for (i = 0; i < chunk; i++) {
				if (table[i] != 0)
					++*num;
			}
			remaining -= chunk;
			ults += chunk * sizeof(*table);
		}
	}
	return 0;
}

/* The fibers and ULTs the fiber stack response lists. */
void expectNumFiberUlobjs(SceUID pid, const UlobjInfoArray *array,
			  const UlobjInfoArrayParam *param, SceUInt32 *num,
			  SceUIntPtr *table)
{
	SceUInt32 numEntries, value, n, i;

	SCE_DBG_ASSERT(num != NULL, 431);
	SCE_DBG_ASSERT(array != NULL, 432);
	SCE_DBG_ASSERT(param != NULL, 433);
	SCE_DBG_ASSERT(table != NULL, 434);
	*num = 0;
	numEntries = param->numEntries;
	for (i = 0; i != numEntries; i++) {
		value = array->entry[i];
		if ((value & ULOBJ_ENTRY_USED) == 0)
			continue;
		switch (value & ULOBJ_ENTRY_KIND_MASK) {
		case ULOBJ_KIND_FIBER:
			++*num;
			break;
		case ULOBJ_KIND_ULT_RUNTIME:
			n = 0;
			expectNumUlobjsUnderUltRuntime(pid, ULOBJ_ENTRY_ADDRESS(value),
						       &n, table);
			*num += n;
			break;
		}
	}
}

/* The TLS address of a thread of the process, 0 if it has none. */
static __attribute__((noinline)) SceUIntPtr getDbgpThreadTls(SceUID pid, SceUID thid)
{
	ThreadmgrThreadInfoForDebugger info;

	info.size = sizeof(info);
	if (SceThreadmgrForDriver_555CA6BB(scePUIDtoGUID(pid, thid), 0, &info) != 0)
		return 0;
	return info.tls;
}

/* The thread a fiber runs on, 0 if it is not running. */
INLINE SceUInt32 getDbgpFiberTid(const UltFiber *fiber)
{
	SCE_DBG_ASSERT(fiber != NULL, 237);
	if (fiber->magic != ULT_FIBER_MAGIC_RUNNING)
		return 0;
	return fiber->threadId;
}

INLINE int getNumUlobjsUnderUltRuntime(SceUID pid, SceUIntPtr address,
				       int *numRunning, int *numOthers,
				       SceUIntPtr *table)
{
	UltRuntime runtime;
	Ult ult;
	SceUIntPtr ults;
	SceUInt32 remaining, chunk, i;
	int running, others;

	SCE_DBG_ASSERT(numRunning != NULL, 252);
	SCE_DBG_ASSERT(numOthers != NULL, 253);
	SCE_DBG_ASSERT(table != NULL, 254);
	running = 0;
	others = 0;
	if (sceKernelCopyFromUserProc(pid, &runtime, (const void *)address,
				      sizeof(runtime)) == 0) {
		remaining = runtime.numUlts;
		ults = runtime.ultTable;
		while (remaining != 0) {
			chunk = remaining < ULT_TABLE_CHUNK ? remaining : ULT_TABLE_CHUNK;
			if (sceKernelCopyFromUserProc(pid, table, (const void *)ults,
						      chunk * sizeof(*table)) != 0)
				break;
			for (i = 0; i < chunk; i++) {
				if (table[i] == 0)
					continue;
				if (sceKernelCopyFromUserProc(pid, &ult,
						(const void *)table[i], sizeof(ult)) == 0 &&
				    getDbgpFiberTid(&ult.fiber) != 0)
					running++;
				else
					others++;
			}
			remaining -= chunk;
			ults += chunk * sizeof(*table);
		}
	}
	*numRunning = running;
	*numOthers = others;
	return running + others;
}

/* The records the application object response holds, by kind. */
void getNumUlobjs(SceUID pid, const UlobjInfoArray *array,
		  const UlobjInfoArrayParam *param, int *numRunning,
		  int *numOthers, int *numRuntimes, int *numUlts,
		  SceUIntPtr *table)
{
	SceUInt32 numEntries, value, i;
	int running, others, n;

	SCE_DBG_ASSERT(numRunning != NULL, 318);
	SCE_DBG_ASSERT(numOthers != NULL, 319);
	SCE_DBG_ASSERT(numRuntimes != NULL, 320);
	SCE_DBG_ASSERT(numUlts != NULL, 321);
	SCE_DBG_ASSERT(array != NULL, 322);
	SCE_DBG_ASSERT(param != NULL, 323);
	SCE_DBG_ASSERT(table != NULL, 324);
	*numRunning = 0;
	*numOthers = 0;
	*numRuntimes = 0;
	*numUlts = 0;
	numEntries = param->numEntries;
	for (i = 0; i != numEntries; i++) {
		value = array->entry[i];
		if ((value & ULOBJ_ENTRY_USED) == 0)
			continue;
		switch (value & ULOBJ_ENTRY_KIND_MASK) {
		case ULOBJ_KIND_FIBER: {
			UltFiber fiber;

			if (sceKernelCopyFromUserProc(pid, &fiber,
					(const void *)ULOBJ_ENTRY_ADDRESS(value),
					sizeof(fiber)) == 0 &&
			    getDbgpFiberTid(&fiber) != 0)
				++*numRunning;
			else
				++*numOthers;
			break;
		}
		case ULOBJ_KIND_ULT_RUNTIME:
			++*numRuntimes;
			n = getNumUlobjsUnderUltRuntime(pid, ULOBJ_ENTRY_ADDRESS(value),
							&running, &others, table);
			*numUlts += n;
			*numRunning += running;
			*numOthers += others;
			break;
		}
	}
}

/* 0 suspended without context, 1 running, 2 suspended; -1 not a fiber */
INLINE int getDbgpFiberState(const UltFiber *fiber)
{
	const SceUInt32 *word;
	SceUInt32 sum, i;

	SCE_DBG_ASSERT(fiber != NULL, 470);
	if (fiber->magic == ULT_FIBER_MAGIC_SAVED) {
		word = (const SceUInt32 *)fiber;
		sum = 0;
		for (i = 2; i < sizeof(*fiber) / sizeof(*word); i++)
			sum ^= word[i];
		if (sum != fiber->checksum)
			return -1;
		return fiber->context != 0 ? 2 : 0;
	}
	if (fiber->magic == ULT_FIBER_MAGIC_RUNNING)
		return 1;
	return -1;
}

INLINE int copyFiberRegisterIn(SceUID pid, UltFiberContext *context,
			       SceUIntPtr address)
{
	SCE_DBG_ASSERT(context != NULL, 493);
	return sceKernelCopyFromUserProc(pid, context, (const void *)address,
					 sizeof(*context));
}

INLINE void copyFiberGpr(DbgpFiberRegisters *regs, const UltFiberContext *context)
{
	SCE_DBG_ASSERT(context != NULL, 500);
	regs->reg[0] = context->reg[0];
	regs->reg[1] = context->reg[1];
	regs->reg[2] = context->reg[2];
	regs->reg[3] = context->reg[3];
	regs->reg[4] = context->reg[4];
	regs->reg[5] = context->reg[5];
	regs->reg[6] = context->reg[6];
	regs->reg[7] = context->reg[7];
	regs->sp = context->sp;
	regs->lr = context->lr;
	regs->vfp[0] = context->vfp[0];
	regs->vfp[1] = context->vfp[1];
	regs->vfp[2] = context->vfp[2];
	regs->vfp[3] = context->vfp[3];
	regs->vfp[4] = context->vfp[4];
	regs->vfp[5] = context->vfp[5];
	regs->vfp[6] = context->vfp[6];
	regs->vfp[7] = context->vfp[7];
}

INLINE void decodeFiberName(DbgpFiberName *name, const UltFiber *fiber)
{
	SCE_DBG_ASSERT(name != NULL, 629);
	name->word[0] = fiber->encodedName[0] ^ 0xa5aa55aaa555aaaaULL;
	name->word[1] = fiber->encodedName[1] ^ 0x5555c3cc33ccc333ULL;
	name->word[2] = fiber->encodedName[2] ^ 0xcccc333396996699ULL;
	name->word[3] = fiber->encodedName[3] ^ 0x9666999966665a5aULL;
	name->c[31] = '\0';
}

/* A fiber's record, with the registers of its thread if it runs. */
void fillFiberInfo(SceUID pid, SceUIntPtr address, DbgpFiberInfo *info,
		   DbgpUltObjectBuffer *buf)
{
	DbgpFiberName name;
	SceUIntPtr context, tls;
	SceUInt32 tid;
	int state, i;

	SCE_DBG_ASSERT(info != NULL, 647);
	SCE_DBG_ASSERT(buf != NULL, 648);
	if (sceKernelCopyFromUserProc(pid, &buf->fiber, (const void *)address,
				      sizeof(buf->fiber)) != 0) {
		info->size = sizeof(*info);
		info->address = address;
		info->state = -1;
		__builtin_strcpy(info->name, "-corrupt-");
		return;
	}
	state = getDbgpFiberState(&buf->fiber);
	tid = getDbgpFiberTid(&buf->fiber);
	info->address = address;
	info->size = sizeof(*info);
	decodeFiberName(&name, &buf->fiber);
	for (i = 0; name.c[i] != '\0'; i++)
		info->name[i] = name.c[i];
	if (i < 32)
		memset(&info->name[i], 0, 32 - i);
	info->state = state;
	info->addrContext = buf->fiber.addrContext;
	info->sizeContext = buf->fiber.sizeContext;
	info->entry = buf->fiber.entry ^ ULT_FIBER_ENTRY_KEY;
	context = buf->fiber.context;
	info->threadId = getDbgpFiberTid(&buf->fiber);
	if (state == 2 && copyFiberRegisterIn(pid, &buf->context, context) == 0) {
		info->regs.fpscr = buf->context.fpscr;
		copyFiberGpr(&info->regs, &buf->context);
	}
	if (tid == 0)
		return;
	tls = getDbgpThreadTls(pid, info->threadId);
	if (tls == 0)
		return;
	info->thread[0].size = sizeof(info->thread[0]);
	info->size += sizeof(info->thread[0]);
	if (sceKernelCopyFromUserProc(pid, &context,
			(const void *)(tls - ULT_TLS_FIBER_CONTEXT), sizeof(context)) != 0)
		return;
	if (sceKernelCopyFromUserProc(pid, &buf->context, (const void *)context,
				      sizeof(buf->context)) != 0)
		return;
	info->thread[0].regs.fpscr = buf->context.fpscr;
	copyFiberGpr(&info->thread[0].regs, &buf->context);
}

INLINE void fillUltInfo(SceUID pid, SceUIntPtr address, DbgpUltInfo *info,
			SceUInt32 threadId, SceUInt32 fiberIndex)
{
	Ult ult;

	SCE_DBG_ASSERT(info != NULL, 756);
	if (sceKernelCopyFromUserProc(pid, &ult, (const void *)address,
				      sizeof(ult)) != 0) {
		info->size = sizeof(*info);
		info->address = address;
		__builtin_strcpy(info->name, "-corrupt-");
		info->reserved = 0;
		info->fiberIndex = fiberIndex;
		info->state = -1;
		return;
	}
	info->size = sizeof(*info);
	info->address = address;
	__builtin_memcpy(info->name, ult.name, sizeof(info->name));
	if (ult.state < sizeof(s_ultStateTable) / sizeof(s_ultStateTable[0]))
		info->state = s_ultStateTable[ult.state];
	else
		info->state = 6;
	info->opaque0 = ult.opaque[0];
	info->opaque1 = ult.opaque[1];
	info->threadId = threadId;
	info->opaque2 = ult.opaque[2];
	info->reserved = 0;
	info->fiberIndex = fiberIndex;
}

/* A ULT runtime's record, its ULTs' records and their fibers' records. */
void fillAndTraverseUltRuntimeInfo(SceUID pid, SceUIntPtr address,
				   DbgpFiberInfo **pFiber, int *numFibers,
				   DbgpUltRuntimeInfo **pRuntime,
				   DbgpAppObjectWork *work)
{
	DbgpUltRuntimeInfo *info;
	UltRuntime runtime;
	SceUIntPtr ults, ult;
	SceUInt32 remaining, chunk, i;
	int n;

	SCE_DBG_ASSERT(pFiber != NULL, 798);
	SCE_DBG_ASSERT(*pFiber != NULL, 799);
	SCE_DBG_ASSERT(numFibers != NULL, 800);
	SCE_DBG_ASSERT(pRuntime != NULL, 801);
	SCE_DBG_ASSERT(*pRuntime != NULL, 802);
	SCE_DBG_ASSERT(work != NULL, 803);
	if (sceKernelCopyFromUserProc(pid, &runtime, (const void *)address,
				      sizeof(runtime)) != 0) {
		info = *pRuntime;
		info->size = sizeof(*info);
		info->address = address;
		__builtin_strcpy(info->name, "-corrupt-");
		info->opaque = 0;
		info->numUlts = 0;
		info->numUsedUlts = 0;
		*pRuntime = (DbgpUltRuntimeInfo *)((char *)*pRuntime + sizeof(*info));
		return;
	}
	info = *pRuntime;
	info->address = address;
	__builtin_memcpy(info->name, runtime.name, sizeof(info->name));
	info->opaque = runtime.opaque;
	info->numUlts = runtime.numUlts;
	n = 0;
	remaining = runtime.numUlts;
	ults = runtime.ultTable;
	while (remaining != 0) {
		chunk = remaining < ULT_TABLE_CHUNK ? remaining : ULT_TABLE_CHUNK;
		/* Sony's: a table that cannot be read is read again */
		if (sceKernelCopyFromUserProc(pid, work->table, (const void *)ults,
					      chunk * sizeof(work->table[0])) != 0)
			continue;
		for (i = 0; i < chunk; i++) {
			ult = work->table[i];
			if (ult == 0)
				continue;
			fillFiberInfo(pid, ult + offsetof(Ult, fiber), *pFiber,
				      &work->object);
			fillUltInfo(pid, ult, &info->ult[n], (*pFiber)->threadId,
				    *numFibers);
			n++;
			*pFiber = (DbgpFiberInfo *)((char *)*pFiber + (*pFiber)->size);
			(*numFibers)++;
		}
		remaining -= chunk;
		ults += chunk * sizeof(work->table[0]);
	}
	info->numUsedUlts = n;
	info->size = sizeof(*info) + n * sizeof(info->ult[0]);
	*pRuntime = (DbgpUltRuntimeInfo *)((char *)*pRuntime + info->size);
}

/* A fiber's stack use; 1 when a record was written. */
int fillFiberStackInfo(SceUID pid, SceUIntPtr address,
				     DbgpFiberStackInfo *info)
{
	static ThreadmgrThreadInfoForDebugger s_threadInfo;
	static ThreadmgrDebugRegisters s_registers[2];
	UltFiberContext context;
	UltFiber fiber;
	SceSSize unused;
	SceUInt32 sp;
	SceUID thid;

	SCE_DBG_ASSERT(info != NULL, 1510);
	if (sceKernelCopyFromUserProc(pid, &fiber, (const void *)address,
				      sizeof(fiber)) != 0)
		return 0;
	if ((fiber.flags & ULT_FIBER_FLAG_CONTEXT_SIZE_CHECK) == 0) {
		info->size = sizeof(*info);
		info->address = address;
		info->maxUsed = -1;
		info->used = -1;
		return 1;
	}
	if (fiber.addrContext == 0) {
		if (fiber.sizeContext != 0)
			return 0;
		info->size = sizeof(*info);
		info->address = address;
		info->maxUsed = 0;
		info->used = 0;
		return 1;
	}
	if ((fiber.addrContext & 7) != 0 || (fiber.sizeContext & 7) != 0)
		return 0;
	unused = sceKernelCountFillValue64FromUserProc(pid,
			(const void *)fiber.addrContext, ULT_FIBER_CONTEXT_FILL,
			fiber.sizeContext);
	if (unused < 0)
		return 0;
	if (fiber.magic == ULT_FIBER_MAGIC_RUNNING) {
		thid = scePUIDtoGUID(pid, fiber.threadId);
		s_threadInfo.size = sizeof(s_threadInfo);
		if (SceThreadmgrForDriver_555CA6BB(thid, 0, &s_threadInfo) < 0)
			return 0;
		if (SceThreadmgrForDriver_5022689D(thid, s_registers) != 0)
			return 0;
		if (s_threadInfo.userFrame != 0)
			sp = s_registers[1].sp;
		else
			sp = s_registers[0].sp;
	} else {
		if (fiber.context == 0) {
			info->size = sizeof(*info);
			info->address = address;
			info->maxUsed = 0;
			info->used = 0;
			return 1;
		}
		if (copyFiberRegisterIn(pid, &context, fiber.context) != 0)
			return 0;
		sp = context.sp;
	}
	info->size = sizeof(*info);
	info->address = address;
	info->maxUsed = fiber.sizeContext - unused;
	info->used = fiber.sizeContext + fiber.addrContext - sp;
	return 1;
}

/* The stack records of a ULT runtime's fibers; their number. */
int fillFiberStackInfoFromUltRuntime(SceUID pid, SceUIntPtr address,
				     DbgpFiberStackInfo *info, SceUIntPtr *table)
{
	UltRuntime runtime;
	SceUIntPtr ults;
	SceUInt32 remaining, chunk, i;
	int n;

	SCE_DBG_ASSERT(info != NULL, 1627);
	SCE_DBG_ASSERT(table != NULL, 1628);
	if (sceKernelCopyFromUserProc(pid, &runtime, (const void *)address,
				      sizeof(runtime)) != 0)
		return 0;
	n = 0;
	remaining = runtime.numUlts;
	ults = runtime.ultTable;
	while (remaining != 0) {
		chunk = remaining < ULT_TABLE_CHUNK ? remaining : ULT_TABLE_CHUNK;
		if (sceKernelCopyFromUserProc(pid, table, (const void *)ults,
					      chunk * sizeof(*table)) != 0)
			break;
		for (i = 0; i < chunk; i++) {
			if (table[i] != 0)
				n += fillFiberStackInfo(pid, table[i] + offsetof(Ult, fiber),
							&info[n]);
		}
		remaining -= chunk;
		ults += chunk * sizeof(*table);
	}
	return n;
}

INLINE int buildAndSendNullGetAppObjectInfoRes(DbgpProcCB *proc)
{
	DbgpGetAppObjectResponse *reply;

	SCE_DBG_ASSERT(proc != NULL, 1086);
	reply = _dbgpAllocSendBuf(proc, offsetof(DbgpGetAppObjectResponse, fibers));
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(reply, 0, offsetof(DbgpGetAppObjectResponse, fibers));
	reply->numSections = 0;
	_dbgpSetSendSize(proc, offsetof(DbgpGetAppObjectResponse, fibers));
	dbgpFillResponseHeader(proc, 0x30, 0x31, 0, 0);
	return 0;
}

INLINE int buildAndSendGetAppObjectInfoRes(DbgpProcCB *proc, SceUID pid,
					   const UlobjInfoArray *array,
					   const UlobjInfoArrayParam *param,
					   DbgpAppObjectWork *work)
{
	int numRunning, numOthers, numRuntimes, numUlts;
	DbgpGetAppObjectResponse *reply;
	DbgpAppObjectSection *runtimes;
	DbgpUltRuntimeInfo *runtime;
	DbgpFiberInfo *fiber;
	SceUInt32 value, i;
	SceSize fiberSize, runtimeSize, size;
	int numFibers;

	SCE_DBG_ASSERT(proc != NULL, 904);
	if (array == NULL)
		return buildAndSendNullGetAppObjectInfoRes(proc);
	getNumUlobjs(pid, array, param, &numRunning, &numOthers, &numRuntimes,
		     &numUlts, work->table);
	if (numRunning == 0 && numOthers == 0 && numRuntimes == 0 && numUlts == 0)
		return buildAndSendNullGetAppObjectInfoRes(proc);

	fiberSize = sizeof(DbgpAppObjectSection) + sizeof(SceUInt32) +
		    (numRunning + numOthers) * sizeof(DbgpFiberInfo) +
		    numRunning * sizeof(DbgpFiberThreadContext);
	if (numRuntimes != 0 || numUlts != 0)
		runtimeSize = sizeof(DbgpAppObjectSection) +
			      numRuntimes * sizeof(DbgpUltRuntimeInfo) +
			      numUlts * sizeof(DbgpUltInfo);
	else
		runtimeSize = 0;
	size = offsetof(DbgpGetAppObjectResponse, fibers) + fiberSize + runtimeSize;
	reply = _dbgpAllocSendBuf(proc, size);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(reply, 0, size);
	reply->fibers.size = fiberSize;
	reply->fibers.type = DBGP_APPOBJ_FIBER;
	reply->fibers.count = numRunning + numOthers;
	if (numRuntimes != 0) {
		runtimes = (DbgpAppObjectSection *)((char *)&reply->fibers + fiberSize);
		reply->numSections = 2;
		runtime = (DbgpUltRuntimeInfo *)(runtimes + 1);
		runtimes->size = runtimeSize;
		runtimes->type = DBGP_APPOBJ_ULT_RUNTIME;
		runtimes->count = numRuntimes;
	} else {
		runtime = NULL;
		reply->numSections = 1;
	}
	fiber = reply->fiber;
	numFibers = 0;
	for (i = 0; i < param->numEntries; i++) {
		value = array->entry[i];
		if ((value & ULOBJ_ENTRY_USED) == 0)
			continue;
		switch (value & ULOBJ_ENTRY_KIND_MASK) {
		case ULOBJ_KIND_FIBER:
			fillFiberInfo(pid, ULOBJ_ENTRY_ADDRESS(value), fiber,
				      &work->object);
			fiber = (DbgpFiberInfo *)((char *)fiber + fiber->size);
			numFibers++;
			break;
		case ULOBJ_KIND_ULT_RUNTIME:
			fillAndTraverseUltRuntimeInfo(pid, ULOBJ_ENTRY_ADDRESS(value),
						      &fiber, &numFibers, &runtime, work);
			break;
		}
	}
	_dbgpSetSendSize(proc, size);
	dbgpFillResponseHeader(proc, 0x30, 0x31, 0, 0);
	return 0;
}

int threadGetAppObjectCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpRequestHeader *request = (const DbgpRequestHeader *)packet;
	UlobjProcCB *procCB;
	UlobjInfoArray *array;
	UlobjInfoArrayParam *param;
	DbgpAppObjectWork *work;
	SceUInt32 error;
	SceUID pid;
	int result, ret;

	SCE_DBG_ASSERT(proc != NULL, 556);
	SCE_DBG_ASSERT(packet != NULL, 557);
	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	error = checkInvalidProcess(pid);
	if (error == 0)
		error = checkAttachedProcess(proc, pid);
	if (error != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, error);
		return (int)0x80080001;
	}
	error = checkIllegalProcessStatus(pid);
	if (error != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, error);
		return (int)0x80080001;
	}
	work = _dbgpAllocWorkBuf(proc, sizeof(*work));
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	pid = request->pid;
	if (sceUlobjMgrRetainInfoArray(pid, &array, &param, &procCB) != 0)
		return buildAndSendGetAppObjectInfoRes(proc, pid, NULL, NULL, work);
	result = buildAndSendGetAppObjectInfoRes(proc, pid, array, param, work);
	ret = sceUlobjMgrReleaseInfoArray(procCB);
	SCE_DBG_ASSERT(ret == 0, 599);
	(void)ret;
	return result;
}

int threadAppObjectInit(DbgpCB *dbgp)
{
	return 0;
}

/* 0 when the process uses libult's info array. */
int threadAppObjectIsSupported(SceUID pid)
{
	return SceUlobjMgrForDriver_6F868221(pid);
}

/* The size of the records of all objects of one type. */
int _getUltSyncObjInfoSizeOfType(SceUID pid, int type)
{
	SceUInt32 ids[128];
	int syncObjType, num, size, skip, n, i;

	switch (type) {
	case ULOBJ_INFO_SEMA:
		syncObjType = ULOBJ_SYNC_OBJ_SEMA;
		break;
	case ULOBJ_INFO_MUTEX:
		syncObjType = ULOBJ_SYNC_OBJ_MUTEX;
		break;
	case ULOBJ_INFO_QUEUE_DATA_RESOURCE_POOL:
		syncObjType = ULOBJ_SYNC_OBJ_QDRP;
		break;
	case ULOBJ_INFO_WAITING_QUEUE_RESOURCE_POOL:
		syncObjType = ULOBJ_SYNC_OBJ_WQRP;
		break;
	case ULOBJ_INFO_QUEUE:
		syncObjType = ULOBJ_SYNC_OBJ_QUEUE;
		break;
	case ULOBJ_INFO_RWLOCK:
		syncObjType = ULOBJ_SYNC_OBJ_RWLOCK;
		break;
	case ULOBJ_INFO_COND:
		syncObjType = ULOBJ_SYNC_OBJ_COND;
		break;
	default:
		return 0;
	}
	size = 0;
	skip = 0;
	do {
		num = sceUlobjMgrGetIdForFiber(pid, ids, 128, skip, syncObjType);
		if (num <= 0)
			break;
		for (i = 0; i != num; i++) {
			n = SceUlobjMgrForDriver_CDAD91AD(pid, ids[i], syncObjType);
			if (n > 0)
				size += n;
			size += 8;
		}
		skip += i;
	} while (i == 128);
	return size;
}

/* The records of all objects of one type, each behind its type and size. */
int _getUltSyncObjInfo(SceUID pid, int type, SceUInt32 *buf)
{
	SceUInt32 ids[128];
	int syncObjType, num, size, skip, n, i;

	SCE_DBG_ASSERT(buf != NULL, 1206);
	switch (type) {
	case ULOBJ_INFO_SEMA:
		syncObjType = ULOBJ_SYNC_OBJ_SEMA;
		break;
	case ULOBJ_INFO_MUTEX:
		syncObjType = ULOBJ_SYNC_OBJ_MUTEX;
		break;
	case ULOBJ_INFO_QUEUE_DATA_RESOURCE_POOL:
		syncObjType = ULOBJ_SYNC_OBJ_QDRP;
		break;
	case ULOBJ_INFO_WAITING_QUEUE_RESOURCE_POOL:
		syncObjType = ULOBJ_SYNC_OBJ_WQRP;
		break;
	case ULOBJ_INFO_QUEUE:
		syncObjType = ULOBJ_SYNC_OBJ_QUEUE;
		break;
	case ULOBJ_INFO_RWLOCK:
		syncObjType = ULOBJ_SYNC_OBJ_RWLOCK;
		break;
	case ULOBJ_INFO_COND:
		syncObjType = ULOBJ_SYNC_OBJ_COND;
		break;
	default:
		syncObjType = -1;
		break;
	}
	SCE_DBG_ASSERT(syncObjType != -1, 1208);
	size = 0;
	skip = 0;
	do {
		num = sceUlobjMgrGetIdForFiber(pid, ids, 128, skip, syncObjType);
		if (num <= 0)
			break;
		for (i = 0; i != num; i++) {
			n = SceUlobjMgrForDriver_B978DB70(pid, ids[i], syncObjType, buf + 2);
			if (n > 0) {
				buf[0] = type;
				buf[1] = n;
				size += n + 8;
				buf = (SceUInt32 *)((char *)buf + n + 8);
			}
		}
		skip += i;
	} while (i == 128);
	return size;
}

/* The size of the records of the given types; all types when there are none. */
int _getUltSyncObjInfoSize(SceUID pid, SceUInt32 num, const int *types)
{
	SceUInt32 i;
	int size;

	SCE_DBG_ASSERT(types != NULL, 1257);
	if (num == 0) {
		types = s_ultSyncObjInfoTypes;
		num = sizeof(s_ultSyncObjInfoTypes) / sizeof(s_ultSyncObjInfoTypes[0]);
	}
	size = 0;
	for (i = 0; i < num; i++)
		size += _getUltSyncObjInfoSizeOfType(pid, types[i]);
	return size;
}

INLINE int buildAndSendNullGetFiberStackInfoRes(DbgpProcCB *proc)
{
	DbgpGetFiberStackInfoResponse *reply;

	SCE_DBG_ASSERT(proc != NULL, 1473);
	reply = _dbgpAllocSendBuf(proc, sizeof(*reply));
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(reply, 0, sizeof(*reply));
	reply->count = 0;
	_dbgpSetSendSize(proc, sizeof(*reply));
	dbgpFillResponseHeader(proc, 0x30, 0x33, 0, 0);
	return 0;
}

/* Sony's response sizes count one word more than the records use. */
#define FIBER_STACK_RES_SIZE(n) \
	(sizeof(DbgpGetFiberStackInfoResponse) + sizeof(SceUInt32) + \
	 (n) * sizeof(DbgpFiberStackInfo))

INLINE int buildAndSendGetFiberStackInfoRes(DbgpProcCB *proc, SceUID pid,
					    const UlobjInfoArray *array,
					    const UlobjInfoArrayParam *param,
					    DbgpAppObjectWork *work)
{
	DbgpGetFiberStackInfoResponse *reply;
	SceUInt32 num, value, i;
	SceSize size;
	int n;

	SCE_DBG_ASSERT(proc != NULL, 1392);
	if (array == NULL)
		return buildAndSendNullGetFiberStackInfoRes(proc);
	expectNumFiberUlobjs(pid, array, param, &num, work->table);
	if (num == 0)
		return buildAndSendNullGetFiberStackInfoRes(proc);
	size = FIBER_STACK_RES_SIZE(num);
	reply = _dbgpAllocSendBuf(proc, size);
	if (reply == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(reply, 0, size);
	n = 0;
	for (i = 0; i < param->numEntries; i++) {
		value = array->entry[i];
		if ((value & ULOBJ_ENTRY_USED) == 0)
			continue;
		switch (value & ULOBJ_ENTRY_KIND_MASK) {
		case ULOBJ_KIND_FIBER:
			n += fillFiberStackInfo(pid, ULOBJ_ENTRY_ADDRESS(value),
						&reply->info[n]);
			break;
		case ULOBJ_KIND_ULT_RUNTIME:
			n += fillFiberStackInfoFromUltRuntime(pid,
					ULOBJ_ENTRY_ADDRESS(value), &reply->info[n],
					work->table);
			break;
		}
	}
	reply->count = n;
	_dbgpSetSendSize(proc, FIBER_STACK_RES_SIZE(n));
	dbgpFillResponseHeader(proc, 0x30, 0x33, 0, 0);
	return 0;
}

int threadGetFiberStackInfoCmd(DbgpProcCB *proc, const DbgpPrefix *packet)
{
	const DbgpRequestHeader *request = (const DbgpRequestHeader *)packet;
	UlobjProcCB *procCB;
	UlobjInfoArray *array;
	UlobjInfoArrayParam *param;
	DbgpAppObjectWork *work;
	SceUInt32 error;
	SceUID pid;
	int result, ret;

	SCE_DBG_ASSERT(proc != NULL, 1289);
	SCE_DBG_ASSERT(packet != NULL, 1290);
	if (_dbgpGetRecvSize(proc) < sizeof(*request)) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, 9);
		return (int)0x80080006;
	}
	pid = request->pid;
	error = checkInvalidProcess(pid);
	if (error == 0)
		error = checkAttachedProcess(proc, pid);
	if (error != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, error);
		return (int)0x80080001;
	}
	error = checkIllegalProcessStatus(pid);
	if (error != 0) {
		dbgpSendError(proc, packet->group, packet->type + 1, packet->code, error);
		return (int)0x80080001;
	}
	pid = request->pid;
	if (sceUlobjMgrRetainInfoArray(pid, &array, &param, &procCB) != 0)
		return buildAndSendGetFiberStackInfoRes(proc, pid, NULL, NULL, NULL);
	work = _dbgpAllocWorkBuf(proc, sizeof(*work));
	if (work == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	result = buildAndSendGetFiberStackInfoRes(proc, pid, array, param, work);
	ret = sceUlobjMgrReleaseInfoArray(procCB);
	SCE_DBG_ASSERT(ret == 0, 1333);
	(void)ret;
	return result;
}
