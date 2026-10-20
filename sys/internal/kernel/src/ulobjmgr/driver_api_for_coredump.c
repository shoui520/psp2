/*
 * SceUlobjMgrForDriver for the coredump: lists of fibers, user-level
 * threads (ULTs) and ULT runtimes, and their state
 * (Sony's driver_api_for_coredump.c).
 */
#include <string.h>
#include <sysmem/sysmem.h>
#include <ulobjmgr/ulobjmgr.h>
#include "ulobjmgr_internal.h"
#include "util.h"

#define ULT_FIBER_MAGIC_SAVED	0x5a5a5a5a	/* context saved, checksummed */
#define ULT_FIBER_MAGIC_RUNNING	0x5aa55aa5
#define ULT_FIBER_ENTRY_KEY	0x963c5aa5

/*
 * The registers a suspended fiber saved: r4-r11, d8-d15 and, by their
 * places around them, sp, lr and fpscr.
 */
typedef struct UltFiberContext {
	SceUInt32 sp;
	SceUInt32 reg[8];
	SceUInt32 lr;
	SceUInt64 vfp[8];
	SceUInt32 fpscr;
	SceUInt32 reserved;
} UltFiberContext;

typedef struct UltFiber {
	SceUInt32 checksum;		/* XOR of words 2..31 */
	SceUInt32 magic;
	/* as in SceFiberInfo */
	SceUInt32 entry;		/* stored XORed with ULT_FIBER_ENTRY_KEY */
	SceUInt32 argOnInitialize;
	SceUIntPtr addrContext;
	SceUInt32 sizeContext;
	SceUInt64 encodedName[4];	/* XORed with a fixed pattern */
	SceUIntPtr context;		/* UltFiberContext */
	SceUInt32 threadId;		/* while running */
	SceUInt32 reserved[16];
} UltFiber;

typedef struct Ult {
	char name[32];
	UltFiber fiber;
	SceUInt16 state;
	SceUInt8 reserved0[6];
	SceUInt32 opaque[3];		/* reported unchanged in UlobjUltInfo */
	SceUInt32 reserved1;
} Ult;

typedef struct UltRuntime {
	char name[32];
	SceUInt32 opaque;		/* reported unchanged in UlobjUltRuntimeInfo */
	SceUInt32 reserved[2];
	SceUInt32 numUlts;
	SceUIntPtr ultTable;		/* numUlts addresses, 0 for a free slot */
} UltRuntime;

_Static_assert(sizeof(UltFiberContext) == 0x70, "UltFiberContext");
_Static_assert(sizeof(UltFiber) == 0x80, "UltFiber");
_Static_assert(sizeof(Ult) == 0xb8, "Ult");
_Static_assert(sizeof(UltRuntime) == 0x34, "UltRuntime");

enum {
	TRAVERSE_FIBER = 1,
	TRAVERSE_ULT = 2,
};

#define ULT_ID_CHUNK	256

/*
 * Count (ids == NULL) or list the ULTs of a runtime, or their fibers; the
 * first *pSkip are skipped and *pCount continues from its current value.
 */
#if FW_DEBUG
__attribute__((noinline))
#endif
static int traverseUltRuntimeByUltRuntimeInstance(SceUID pid, const UltRuntime *runtime,
						  int mode, SceUInt32 maxIds, SceUInt32 *ids,
						  SceUInt32 *pSkip, SceUInt32 *pCount)
{
	SceUInt32 table[ULT_ID_CHUNK];
	SceUIntPtr address;
	SceUInt32 remaining, chunk, i;
	int ret;

	SCE_DBG_ASSERT(runtime != NULL, 55);
	SCE_DBG_ASSERT(ids == NULL || pSkip != NULL, 57);
	SCE_DBG_ASSERT(pCount != NULL, 58);
	SCE_DBG_ASSERT(mode == TRAVERSE_FIBER || mode == TRAVERSE_ULT, 60);

	if (ids != NULL && *pCount >= maxIds)
		return 0;
	address = runtime->ultTable;
	for (remaining = runtime->numUlts; remaining != 0; remaining -= chunk) {
		chunk = remaining < ULT_ID_CHUNK ? remaining : ULT_ID_CHUNK;
		ret = sceKernelCopyFromUserProc(pid, table, (const void *)address,
						 chunk * sizeof(table[0]));
		if (ret != 0)
			return ret;
		for (i = 0; i < chunk; i++) {
			if (table[i] == 0)
				continue;
			if (ids == NULL) {
				(*pCount)++;
			} else if (*pSkip != 0) {
				(*pSkip)--;
			} else {
				if (mode == TRAVERSE_FIBER)
					ids[*pCount] = table[i] + offsetof(Ult, fiber);
				else
					ids[*pCount] = table[i];
				if (++*pCount >= maxIds)
					return 0;
			}
		}
		address += chunk * sizeof(table[0]);
	}
	return 0;
}

static inline __attribute__((always_inline))
int traverseUltRuntimeByUltRuntimeId(SceUID pid, SceUIntPtr runtimeId, int mode,
				     SceUInt32 maxIds, SceUInt32 *ids, SceUInt32 *pSkip,
				     SceUInt32 *pCount)
{
	UltRuntime runtime;
	int ret;

	SCE_DBG_ASSERT(pSkip != NULL, 129);
	SCE_DBG_ASSERT(pCount != NULL, 130);
	SCE_DBG_ASSERT(mode == TRAVERSE_FIBER || mode == TRAVERSE_ULT, 132);
	ret = sceKernelCopyFromUserProc(pid, &runtime, (const void *)runtimeId,
					 sizeof(runtime));
	if (ret != 0)
		return ret;
	return traverseUltRuntimeByUltRuntimeInstance(pid, &runtime, mode, maxIds, ids, pSkip,
						      pCount);
}

/* fibers: those in the info array and those of every ULT */
static inline __attribute__((always_inline))
SceUInt32 sceUlobjMgrGetFiberId(SceUID pid, UlobjProcCB *procCB, SceUInt32 *ids,
				SceUInt32 maxIds, SceUInt32 skip)
{
	SceUInt32 i, entry, numEntries, n = 0;
	UlobjInfoArray *infoArray;

	SCE_DBG_ASSERT(procCB != NULL, 150);
	numEntries = procCB->numEntries;
	infoArray = procCB->infoArray;
	for (i = 0; i < numEntries; i++) {
		entry = infoArray->entry[i];
		if (!(entry & ULOBJ_ENTRY_USED))
			continue;
		if ((entry & ULOBJ_ENTRY_KIND_MASK) == ULOBJ_KIND_FIBER) {
			if (ids == NULL) {
				n++;
				continue;
			}
			if (skip != 0) {
				skip--;
				continue;
			}
			ids[n++] = ULOBJ_ENTRY_ADDRESS(entry);
			if (n >= maxIds)
				break;
		} else if ((entry & ULOBJ_ENTRY_KIND_MASK) == ULOBJ_KIND_ULT_RUNTIME) {
			traverseUltRuntimeByUltRuntimeId(pid, ULOBJ_ENTRY_ADDRESS(entry),
							 TRAVERSE_FIBER, maxIds, ids, &skip, &n);
			if (ids != NULL && n >= maxIds)
				break;
		}
	}
	return n;
}

static inline __attribute__((always_inline))
SceUInt32 sceUlobjMgrGetUltId(SceUID pid, UlobjProcCB *procCB, SceUInt32 *ids,
			      SceUInt32 maxIds, SceUInt32 skip)
{
	SceUInt32 i, entry, numEntries, n = 0;
	UlobjInfoArray *infoArray;

	SCE_DBG_ASSERT(procCB != NULL, 209);
	numEntries = procCB->numEntries;
	infoArray = procCB->infoArray;
	for (i = 0; i < numEntries; i++) {
		entry = infoArray->entry[i];
		if ((entry & ULOBJ_ENTRY_TAG_MASK) != (ULOBJ_ENTRY_USED | ULOBJ_KIND_ULT_RUNTIME))
			continue;
		traverseUltRuntimeByUltRuntimeId(pid, ULOBJ_ENTRY_ADDRESS(entry), TRAVERSE_ULT,
						 maxIds, ids, &skip, &n);
		if (ids != NULL && n >= maxIds)
			break;
	}
	return n;
}

static inline __attribute__((always_inline))
SceUInt32 sceUlobjMgrGetUltRuntimeId(UlobjProcCB *procCB, SceUInt32 *ids, SceUInt32 maxIds,
				     SceUInt32 skip)
{
	SceUInt32 i, entry, numEntries, n = 0;
	UlobjInfoArray *infoArray;

	SCE_DBG_ASSERT(procCB != NULL, 241);
	numEntries = procCB->numEntries;
	infoArray = procCB->infoArray;
	for (i = 0; i < numEntries; i++) {
		entry = infoArray->entry[i];
		if (!(entry & ULOBJ_ENTRY_USED) ||
		    (entry & ULOBJ_ENTRY_KIND_MASK) != ULOBJ_KIND_ULT_RUNTIME)
			continue;
		if (ids == NULL) {
			n++;
			continue;
		}
		if (skip != 0) {
			skip--;
			continue;
		}
		ids[n++] = ULOBJ_ENTRY_ADDRESS(entry);
		if (n >= maxIds)
			break;
	}
	return n;
}

/* the IDs of one kind of object: 1 fibers, 2 ULTs, 3 ULT runtimes, 0x11.. sync objects
 * (the 0.990 prototype's name, from when it took fibers only) */
int sceUlobjMgrGetIdForFiber(SceUID pid, SceUInt32 *ids, SceUInt32 maxIds,
				  SceUInt32 skip, int type)
{
	UlobjProcCB *procCB;
	int n;

	switch (type) {
	case 1:
		procCB = getAndLockUlobjInfoArrayForKernel(pid);
		if (procCB == NULL)
			return 0;
		n = sceUlobjMgrGetFiberId(pid, procCB, ids, maxIds, skip);
		break;
	case 2:
		procCB = getAndLockUlobjInfoArrayForKernel(pid);
		if (procCB == NULL)
			return 0;
		n = sceUlobjMgrGetUltId(pid, procCB, ids, maxIds, skip);
		break;
	case 3:
		procCB = getAndLockUlobjInfoArrayForKernel(pid);
		if (procCB == NULL)
			return 0;
		n = sceUlobjMgrGetUltRuntimeId(procCB, ids, maxIds, skip);
		break;
	case 0x11:
	case 0x12:
	case 0x13:
	case 0x14:
	case 0x15:
	case 0x16:
	case 0x17:
		procCB = getAndLockUlobjInfoArrayForKernel(pid);
		if (procCB == NULL)
			return 0;
		n = sceUlobjMgrGetUltSyncObjId(pid, ids, maxIds, skip, type, procCB);
		break;
	default:
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	unlockUlobjInfoArrayForKernelImpl(procCB);
	return n;
}

/* the ULTs of one runtime */
int SceUlobjMgrForDriver_3D6E2EF3(SceUID pid, SceUIntPtr runtimeId, SceUInt32 *ids,
				  SceUInt32 maxIds, SceUInt32 skip)
{
	SceUInt32 n = 0;
	int ret;

	ret = traverseUltRuntimeByUltRuntimeId(pid, runtimeId, TRAVERSE_ULT, maxIds, ids, &skip,
					       &n);
	if (ret == 0)
		ret = n;
	return ret;
}

/* name unknown */
static inline SceUInt32 calcFiberChecksum(const UltFiber *fiber)
{
	const SceUInt32 *word = (const SceUInt32 *)fiber;
	SceUInt32 i, sum = 0;

	for (i = 2; i < sizeof(*fiber) / sizeof(*word); i++)
		sum ^= word[i];
	return sum;
}

/* 0 suspended without context, 1 running, 2 suspended; -1 not a fiber */
static inline __attribute__((always_inline)) int getFiberState(const UltFiber *fiber)
{
	SCE_DBG_ASSERT(fiber != NULL, 376);
	if (fiber->magic == ULT_FIBER_MAGIC_SAVED) {
		if (calcFiberChecksum(fiber) != fiber->checksum)
			return -1;
		return fiber->context != 0 ? 2 : 0;
	}
	if (fiber->magic == ULT_FIBER_MAGIC_RUNNING)
		return 1;
	return -1;
}

static inline __attribute__((always_inline))
int getFiberRegister(SceUID pid, const UltFiber *fiber, UltFiberContext *context)
{
	SCE_DBG_ASSERT(fiber != NULL, 406);
	SCE_DBG_ASSERT(context != NULL, 407);
	if (getFiberState(fiber) == 2)
		return sceKernelCopyFromUserProc(pid, context, (const void *)fiber->context,
						  sizeof(*context));
	memset(context, 0, sizeof(*context));
	return 0;
}

static inline __attribute__((always_inline)) SceUInt32 getFiberTid(const UltFiber *fiber)
{
	SCE_DBG_ASSERT(fiber != NULL, 397);
	if (fiber->magic != ULT_FIBER_MAGIC_RUNNING)
		return 0;
	return fiber->threadId;
}

static inline __attribute__((always_inline))
void decodeFiberName(char *name, const UltFiber *fiber)
{
	union {
		SceUInt64 word[4];
		SceUInt8 c[32];
	} decoded;
	int i;

	SCE_DBG_ASSERT(name != NULL, 426);
	decoded.word[0] = fiber->encodedName[0] ^ 0xa5aa55aaa555aaaaULL;
	decoded.word[1] = fiber->encodedName[1] ^ 0x5555c3cc33ccc333ULL;
	decoded.word[2] = fiber->encodedName[2] ^ 0xcccc333396996699ULL;
	decoded.word[3] = fiber->encodedName[3] ^ 0x9666999966665a5aULL;
	decoded.c[31] = '\0';
	for (i = 0; decoded.c[i] != '\0'; i++)
		name[i] = decoded.c[i];
	if (i < 32)
		memset(&name[i], 0, 32 - i);
}

/* a fiber's state and saved registers */
int SceUlobjMgrForDriver_8E200387(SceUID pid, SceUIntPtr fiberId, UlobjFiberInfo *info)
{
	UltFiberContext context;
	UltFiber fiber;
	int state, ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = sceKernelCopyFromUserProc(pid, &fiber, (const void *)fiberId, sizeof(fiber));
	if (ret != 0)
		return ret;
	state = getFiberState(&fiber);
	if (state < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = getFiberRegister(pid, &fiber, &context);
	if (ret != 0)
		return ret;
	decodeFiberName(info->name, &fiber);
	info->address = fiberId;
	info->state = state;
	info->entry = fiber.entry ^ ULT_FIBER_ENTRY_KEY;
	info->addrContext = fiber.addrContext;
	info->sizeContext = fiber.sizeContext;
	info->threadId = getFiberTid(&fiber);
	info->fpscr = context.fpscr;
	info->reg[0] = context.reg[0];
	info->reg[1] = context.reg[1];
	info->reg[2] = context.reg[2];
	info->reg[3] = context.reg[3];
	info->reg[4] = context.reg[4];
	info->reg[5] = context.reg[5];
	info->reg[6] = context.reg[6];
	info->reg[7] = context.reg[7];
	info->sp = context.sp;
	info->lr = context.lr;
	copy64b(info->vfp, context.vfp);
	return 0;
}

/* Ult.state -> UlobjUltInfo.state */
static const SceUInt32 g_ultStateTable[] = { 0, 1, 4, 5, 3, 2 };

/* a ULT */
int SceUlobjMgrForDriver_FFC8C811(SceUID pid, SceUIntPtr ultId, SceUInt32 runtime,
				  UlobjUltInfo *info)
{
	Ult ult;
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = sceKernelCopyFromUserProc(pid, &ult, (const void *)ultId, sizeof(ult));
	if (ret != 0)
		return ret;
	copy32bWithNullTermination(info->name, ult.name);
	info->address = ultId;
	info->runtime = runtime;
	info->fiber = ultId + offsetof(Ult, fiber);
	if (ult.state < sizeof(g_ultStateTable) / sizeof(g_ultStateTable[0]))
		info->state = g_ultStateTable[ult.state];
	else
		info->state = 6;
	info->opaque0 = ult.opaque[0];
	info->threadId = getFiberTid(&ult.fiber);
	info->opaque1 = ult.opaque[1];
	info->opaque2 = ult.opaque[2];
	return 0;
}

/* a ULT runtime */
int SceUlobjMgrForDriver_525A9BDB(SceUID pid, SceUIntPtr runtimeId, UlobjUltRuntimeInfo *info)
{
	UltRuntime runtime;
	SceUInt32 count;
	int ret;

	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = sceKernelCopyFromUserProc(pid, &runtime, (const void *)runtimeId,
					 sizeof(runtime));
	if (ret != 0)
		return ret;
	count = 0;
	ret = traverseUltRuntimeByUltRuntimeInstance(pid, &runtime, TRAVERSE_ULT, 0, NULL, NULL,
						     &count);
	if (ret != 0)
		return ret;
	copy32bWithNullTermination(info->name, runtime.name);
	info->address = runtimeId;
	info->opaque = runtime.opaque;
	info->numUlts = runtime.numUlts;
	info->numUsedUlts = count;
	return ret;
}
