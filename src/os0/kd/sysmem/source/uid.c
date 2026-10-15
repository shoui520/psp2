/*
 * SceSysmem: UIDs. Every kernel object has a GUID entry in the GUID entry
 * heap; opening a GUID adds a link entry next to it, and a process sees its
 * objects through PUID entries in its own UID entry heap. Entries count their
 * links (lcount) and their users (ucount); the last close frees the object.
 *
 * The helpers that lock an entry heap pass Sony's function name and line,
 * which the lock helpers ignore.
 */

#include <psp2kern/kernel/sysclib.h>
#include "sysmem_internal.h"
#include "memblock_obj_module.h"

#define UID_WRITE_LOCK(heap, line)	FixedHeapWriteLock(&(heap)->fixedHeap, __func__, (line))
#define UID_WRITE_UNLOCK(heap, state)	FixedHeapWriteUnlock(&(heap)->fixedHeap, (state))
#define UID_READ_LOCK(heap, line)	FixedHeapReadLock(&(heap)->fixedHeap, __func__, (line))
#define UID_READ_UNLOCK(heap, state)	FixedHeapReadUnlock(&(heap)->fixedHeap, (state))

/* the GUID entry heap */
#define GUID_ENTRY_HEAP()		((UIDEntryHeap *)g_uidManager->guidEntryHeap)

#define UID_ERROR_INVALID_UID		SCE_KERNEL_ERROR_INVALID_UID
#define UID_ERROR_NO_OBJECT		SCE_KERNEL_ERROR_UID_ERROR
#define UID_ERROR_LEVEL			SCE_KERNEL_ERROR_UID_NOT_VISIBLE
#define UID_ERROR_LINK_COUNT		SCE_KERNEL_ERROR_UID_MAX_OPEN
#define UID_ERROR_OBJECT_LIMIT		SCE_KERNEL_ERROR_UID_RL_OVERFLOW
#define UID_ERROR_CLASS			SCE_KERNEL_ERROR_INVALID_UID_CLASS
#define UID_ERROR_SUBCLASS		SCE_KERNEL_ERROR_INVALID_UID_SUBCLASS
#define UID_ERROR_NAME_NOT_FOUND	SCE_KERNEL_ERROR_UID_CANNOT_FIND_BY_NAME
#define UID_ERROR_NAME_TOO_LONG		SCE_KERNEL_ERROR_UID_NAME_TOO_LONG
/* names unknown */

/* the class magic (SceClass.magic) */
#define SCE_CLASS_MAGIC			0xabce9da5

/* SceUIDDLinkClass, SceUIDEntryHeapClass and the default "heap" of registered objects */
static SceClass *s_dlinkClass;
static FixedHeapClass *s_entryHeapClass;
static void *s_registerHeap;
/* the last mismatching UID UIDEntryCheckInvalid warned about */
static SceUID s_lastWarnedUID;

/* name unknown: SceUIDDLinkClass's constructor and destructor */
static int UIDDLinkCreate(void *object)
{
	void **link = object;

	link[0] = object;
	link[2] = object;
	return 0;
}

static int UIDDLinkDestroy(void *object)
{
	void **link = object;

	link[2] = NULL;
	return 0;
}

/* SceSysmemForKernel_7CDB7655 */
int SceSysmemForKernel_7CDB7655(SceClass *cls, const char *name, void *uidclass, SceSize itemsize,
				SceClassCallback create, SceClassCallback destroy)
{
	return ksceUIDClassInitClass(cls, name, uidclass, itemsize, create, destroy);
}

/* name unknown */
void UIDClassSetCallbacks(SceClass *cls, const char *name, SceClassCallback create,
			  SceClassCallback destroy)
{
	cls->name = name;
	cls->create_cb = create;
	cls->destroy_cb = destroy;
}

SceClass *ksceKernelGetUIDDLinkClass(void)
{
	return s_dlinkClass;
}

/* name unknown */
int UIDDLinkClassInit(UIDManager *manager)
{
	SceClass *cls = manager->dlinkClass;

	s_dlinkClass = cls;
	cls->name = "SceUIDDLinkClass";
	cls->create_cb = UIDDLinkCreate;
	cls->destroy_cb = UIDDLinkDestroy;
	return 0;
}

/* name unknown: SceUIDEntryHeapClass's onCarve, a new entry's first UID */
static void UIDEntryHeapClassOnCarve(void *object, SceUInt32 index)
{
	UIDEntry *entry = object;

	entry->attr = 0;
	entry->linkCount = 0;
	entry->useCount = 0;
	entry->uid = (SceUInt16)(index << 1) | UID_GENERATION_ONE | 1;
}

/* name unknown: vector match, the entries of a process */
static SceUInt32 UIDFuncMatchPID(UIDEntry *entry, UIDFuncVector *vector)
{
	if (!(entry->attr & UID_ENTRY_ATTR_ACTIVE))
		return UID_WALK_CONTINUE | UID_WALK_SKIP;
	return vector->key == (SceUInt32)entry->pid ? UID_WALK_CONTINUE | UID_WALK_MATCH :
						     UID_WALK_CONTINUE | UID_WALK_SKIP;
}

/* name unknown: vector match, the PUIDs of a GUID */
static SceUInt32 UIDFuncMatchGUID(UIDEntry *entry, UIDFuncVector *vector)
{
	SceUID guid;
	SceUInt32 key;

	if (!(entry->attr & UID_ENTRY_ATTR_ACTIVE))
		return UID_WALK_CONTINUE | UID_WALK_SKIP;
	key = vector->key;
	if ((entry->attr & UID_ENTRY_ATTR_PUID) && (entry->attr & UID_ENTRY_ATTR_LINK_ANY))
		guid = entry->guid;
	else
		guid = UID_ERROR_INVALID_UID;
	return key == (SceUInt32)guid ? UID_WALK_CONTINUE | UID_WALK_MATCH :
						UID_WALK_CONTINUE | UID_WALK_SKIP;
}

/* name unknown: vector add, the UID */
static void UIDFuncAddUID(UIDFuncVector *vector, UIDEntry *entry)
{
	SceUID *uids = vector->buffer;

	vector->total++;
	if (uids != NULL && vector->count * sizeof(SceUID) < vector->size) {
		uids[vector->count] = entry->uid;
		vector->count++;
	}
}

/* name unknown: vector match by state: key bit 0 the active ones, bit 1 the closed ones */
static SceUInt32 UIDFuncMatchState(UIDEntry *entry, UIDFuncVector *vector)
{
	SceUInt32 result;

	if ((vector->key & 1) && (entry->attr & UID_ENTRY_ATTR_ACTIVE))
		result = UID_WALK_MATCH;
	else
		result = UID_WALK_SKIP;
	if ((vector->key & 2) && (entry->attr & UID_ENTRY_ATTR_CLOSED))
		return UID_WALK_CONTINUE | UID_WALK_MATCH;
	return result | UID_WALK_CONTINUE;
}

/* a fixed heap walk looking for an entry */
typedef struct UIDEntryFind {
	SceUInt32 key;
	UIDEntry *entry;
	SceUInt32 index;
} UIDEntryFind;

/* name unknown: walk callback, the entry of an object */
static int UIDEntryFindByObject(void *object, SceUInt32 index, void *arg)
{
	UIDEntry *entry = object;
	UIDEntryFind *find = arg;
	SceUInt32 key = find->key;
	SceUInt8 linkCount;
	SceObjectBase *entryObject;

	if (!(entry->attr & UID_ENTRY_ATTR_ACTIVE))
		return 0;
	linkCount = entry->linkCount;
	entryObject = entry->object;
	if (linkCount > UID_ENTRY_MAX_LCOUNT || entry->useCount > UID_ENTRY_MAX_UCOUNT ||
	    key != (SceUInt32)entryObject)
		return 0;
	find->entry = entry;
	find->index = index;
	return 1;
}

#if !FW_DEBUG
static inline
#endif
int UIDEntryIncLinkCountRaw(UIDEntry *entry)
{
	SceUInt8 limit = entry->attr & UID_ENTRY_ATTR_LINK_LIMIT;
	SceUInt8 linkCount = entry->linkCount;

	if (limit != 0) {
		if (linkCount >= limit)
			return 0;
	}
	if (linkCount < UID_ENTRY_MAX_LCOUNT) {
		entry->linkCount = linkCount + 1;
		return 1;
	}
	SCE_DBG_PRINTF(0xb, 886, "Error:linkCount=0x%08x >= SCE_UID_ENTRY_MAX_LCOUNT\n", linkCount);
	return 0;
}

#if !FW_DEBUG
static inline
#endif
int UIDEntryIsValidWithUIDRaw(const UIDEntry *entry, SceUID uid)
{
	if (!(entry->attr & UID_ENTRY_ATTR_ALL)) {
		SCE_DBG_PRINTF(0xb, 670, "UIDEntryIsAllRaw failed, attr=0x%08x, ALL=0x%08X\n",
			       entry->attr, UID_ENTRY_ATTR_ALL);
		return 0;
	}
	if (uid == entry->uid)
		return 1;
	SCE_DBG_PRINTF(0xb, 665, "uid 0x%08x mismatch pEntry->uid=0x%08x\n", uid, entry->uid);
	return 0;
}

/* walk callback, the PUID of a GUID */
static int UIDEntryFindByBaseGUID(void *object, SceUInt32 index, void *arg)
{
	UIDEntry *entry = object;
	UIDEntryFind *find = arg;
	SceUInt32 attr = entry->attr;
	SceUInt32 key = find->key;

	if (!(attr & UID_ENTRY_ATTR_LINK))
		return 0;
	if (entry->object == NULL || entry->linkCount > UID_ENTRY_MAX_LCOUNT ||
	    entry->useCount > UID_ENTRY_MAX_UCOUNT) {
		SCE_DBG_PRINTF_LEVEL(1, 0xf, 1462, "pEntry=%p[0x%08x] is not valid\n", entry,
				     entry->guid);
		return 0;
	}
	if (key != (SceUInt32)entry->guid)
		return 0;
	find->entry = entry;
	find->index = index;
	return 1;
}

/* the entry of a UID, if the UID is its current one */
int UIDEntryCheckInvalid(UIDEntryHeap *heap, SceUID uid, SceUInt32 flags, UIDEntry **out)
{
	UIDEntry *entry = NULL;
	SceUID expect;
	int ret;

	ret = FixedHeapGetObjectByIndexRaw(&heap->fixedHeap, UIDGetIndex(uid), (void **)&entry);
	if (ret < 0)
		return UID_ERROR_INVALID_UID;
	if (entry != NULL && uid != (expect = entry->uid)) {
		if (!(flags & 1) && uid != s_lastWarnedUID) {
#if FW_DEBUG
			const char *name = entry->name, *p;

			if (name != NULL) {
				p = name;
				do {
					if (*p == '\0')
						break;
					if (!(look_ctype_table(*p) & 0x97)) {
						name = "<NOT PRINTABLE>";
						break;
					}
				} while (p++ != name + 0x1e);
			}
			SCE_DBG_PRINTF(6, 1428,
				       "Warning:uid=0x%08x!=0x%08x[expect:pEntry->uid],pEnrtry=%p[%s]\n",
				       uid, expect, entry, name);
#endif
			s_lastWarnedUID = uid;
		}
		*out = NULL;
		return UID_ERROR_INVALID_UID;
	}
	*out = entry;
	return 0;
}

void UIDEntryDestructorRaw(UIDEntry *entry)
{
	const char *name = entry->name;
	SceUInt32 uid;
#if FW_SINCE(0x04000000)
	SceUIntPtr pa;

	if (ksceKernelMMUVAtoPA((void *)name, &pa) < 0) {
		SCE_DBG_PRINTF(0xb, 798, "pEntry=%p, pName:%p is in unmapped area\n", entry, name);
		entry->name = NULL;
		SCE_DBG_ASSERT_FAIL(801);
	} else if (name != NULL) {
		NameHeapDelete(name, NAME_HEAP_FLAG_BY_LINK | ((entry->attr & UID_ENTRY_ATTR_NAME_KIND) ?
							       NAME_HEAP_FLAG_SECOND : 1));
	}
#else
	if (name != NULL) {
		NameHeapDelete(name, NAME_HEAP_FLAG_BY_LINK | ((entry->attr & UID_ENTRY_ATTR_NAME_KIND) ?
							       NAME_HEAP_FLAG_SECOND : 1));
		entry->name = NULL;
	}
#endif
	/* the next generation of the UID */
	uid = entry->uid;
	entry->object = NULL;
	entry->linkCount = 0;
	entry->useCount = 0;
	if ((uid & UID_GENERATION_MASK) == UID_GENERATION_MASK)
		uid = (uid & ~UID_GENERATION_MASK) | UID_GENERATION_ONE;
	else
		uid += UID_GENERATION_ONE;
	entry->guid = 0;
	entry->uid = uid;
	entry->attr &= ~UID_ENTRY_ATTR_ALL;
}

/* the entry a GUID's object is referred through, counted as a user */
static inline int UIDEntryReferObjectCheckClassLevelWithGUID(UIDEntry *entry, SceUID guid,
							     int mode, SceClass *cls,
							     SceUInt32 level, SceUInt32 flags,
							     SceObjectBase **out)
{
	SceClass *c;
	SceUInt32 attr = entry->attr;

	if (!(attr & UID_ENTRY_ATTR_ACTIVE) || guid != entry->uid)
		return UID_ERROR_INVALID_UID;
	if ((flags & 1) && entry->useCount != 0) {
		SCE_DBG_PRINTF(0xb, 2741, "guid 0x%08x[%s] has ucount=%d\n", guid, entry->name,
			       entry->useCount);
		return SCE_KERNEL_ERROR_UID_REFER_NOT_ZERO;
	}
	switch (mode) {
	case 0:
		break;
	case 1:
		if (cls != entry->object->sce_class)
			return UID_ERROR_CLASS;
		break;
	case 2:
		for (c = entry->object->sce_class; c != NULL; c = c->uidclass) {
			if (c == cls)
				goto found;
		}
		if (out != NULL)
			*out = NULL;
		return UID_ERROR_SUBCLASS;
	default:
		return UID_ERROR_INVALID_UID;
	}
found:
	if (((attr & UID_ENTRY_ATTR_LEVEL_MASK) >> UID_ENTRY_ATTR_LEVEL_SHIFT) > level)
		return UID_ERROR_LEVEL;
	entry->useCount++;
	if (out != NULL)
		*out = entry->object;
	return 0;
}

int GUIDReferObjectClassCheckCommon(SceUID guid, int mode, SceClass *cls, SceUInt32 level,
					   SceUInt32 flags, SceObjectBase **out)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	int state, ret;

	state = UID_WRITE_LOCK(heap, 5667);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0)
		ret = UIDEntryReferObjectCheckClassLevelWithGUID(entry, guid, mode, cls, level,
								 flags, out);
	UID_WRITE_UNLOCK(heap, state);
	if (ret < 0) {
		if (out != NULL)
			*out = NULL;
	}
	return ret;
}

int GUIDGetObjectWithClass(SceUID guid, SceClass *cls, SceObjectBase **out)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	int state, ret;

	state = UID_READ_LOCK(heap, 5188);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0) {
		if (UIDEntryIsValidWithUIDRaw(entry, guid) == 1) {
			SceObjectBase *object = entry->object;
			SceClass *objectClass = object->sce_class;

			if (cls == NULL || cls == objectClass) {
				*out = object;
			} else {
				ret = UID_ERROR_CLASS;
				*out = NULL;
			}
		} else {
			SCE_DBG_PRINTF(0xb, 5196, "UIDEntryIsvalidWithUIDRaw(guid 0x%08x) failed\n",
				       guid);
			ret = UID_ERROR_INVALID_UID;
		}
	}
	UID_READ_UNLOCK(heap, state);
	return ret;
}

int GUIDGetName(SceUID guid, const char **name)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	int state;

	state = UID_READ_LOCK(heap, 5252);
	if (UIDEntryCheckInvalid(heap, guid, 0, &entry) >= 0) {
		UIDEntry *current = entry;

		*name = UIDEntryIsValidWithUIDRaw(current, guid) == 1 ? current->name : NULL;
	}
	UID_READ_UNLOCK(heap, state);
	return 0;
}

int GUIDGetVisibilityLevel(SceUID guid)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	unsigned int level = 7;
	int state;

	state = UID_READ_LOCK(heap, 5324);
	if (UIDEntryCheckInvalid(heap, guid, 0, &entry) >= 0) {
		UIDEntry *current = entry;

		if (UIDEntryIsValidWithUIDRaw(current, guid) == 1)
			level = UIDEntryGetLevel(current);
	}
	UID_READ_UNLOCK(heap, state);
	return level;
}

/* walk callback: the first active entry with a name (the name is not compared) */
static int UIDEntryFindNamed(void *object, SceUInt32 index, void *arg)
{
	UIDEntry *entry = object;
	UIDEntryFind *find = arg;

	if (!(entry->attr & UID_ENTRY_ATTR_ACTIVE) || entry->object == NULL)
		return 0;
	if (entry->linkCount > UID_ENTRY_MAX_LCOUNT || entry->useCount > UID_ENTRY_MAX_UCOUNT)
		return 0;
	if (entry->name == NULL)
		return 0;
	find->entry = entry;
	find->index = index;
	return 1;
}

int GUIDEntryHeapFindByNameRaw(UIDEntryHeap *heap, const char *name, int all,
				      UIDEntry **out)
{
	NameEntry *nameEntry;
	UIDEntryFind find;
	int ret;

	ret = NameHeapFind(name, NAME_HEAP_FLAG_SECOND, &nameEntry);
	if (ret >= 0) {
		if ((nameEntry->attr & NAME_ENTRY_ATTR_KIND_MASK) == NAME_ENTRY_KIND_UID_ENTRY) {
			UIDEntry *entry = nameEntry->owner;
			SceUInt32 attr = entry->attr;

			*out = entry;
			ret = attr;
			goto check;
		}
		if (ret == 0)
			SCE_DBG_PRINTF(0xb, 4641, "NameHeapFind(<%s>) attr=0x%08x failed 0x%08x\n",
				       name, nameEntry->attr, 0);
	}
	if (all != 1)
		return UID_ERROR_NAME_NOT_FOUND;
	find.key = (SceUInt32)name;
	ret = FixedHeapWalk(&heap->fixedHeap, UIDEntryFindNamed, &find);
	if (ret == 0) {
		ret = UID_ERROR_NAME_NOT_FOUND;
		if (find.entry != NULL) {
			*out = find.entry;
			ret = find.index;
			goto walked;
		}
	} else {
		*out = NULL;
walked:
		if (ret == SCE_KERNEL_ERROR_FIXEDHEAP_ILLEGAL_INDEX) {
			*out = NULL;
			return UID_ERROR_NAME_NOT_FOUND;
		}
		if (ret >= 0) {
			ret = (*out)->attr;
			goto check;
		}
	}
	*out = NULL;
	return ret;
check:
	if (ret & UID_ENTRY_ATTR_NAME_PUBLIC)
		return 0;
	return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
}

/* name unknown: walk callback of a vector */
static int UIDFuncVectorApply(void *object, SceUInt32 index, void *arg)
{
	UIDEntry *entry = object;
	UIDFuncVector *vector = arg;
	SceUInt32 result;

	if (UIDEntryGetLevel(entry) > vector->level || vector->match == NULL)
		return 0;
	result = vector->match(entry, vector);
	if ((result & 0xf) == UID_WALK_MATCH)
		vector->add(vector, entry);
	if ((result & 0xf0) == UID_WALK_STOP)
		return 1;
	return 0;
}

/* sixteen entries at a time, unlocking in between */
int UIDFuncVectorGetUIDVectorWeak(UIDFuncVector *vector)
{
	SceUInt32 start = 0, end;
	int state, ret;

	do {
		state = UID_READ_LOCK(vector->heap, 3596);
		ret = FixedHeapApplyRangeWithIndexArgRaw(&vector->heap->fixedHeap, start, 0x10,
							 UIDFuncVectorApply, vector, &end);
		UID_READ_UNLOCK(vector->heap, state);
		if (ret < 0)
			break;
		start += 0x10;
	} while (start <= end);
	return vector->total;
}

/* name unknown: SceUIDEntryHeapClass's setPage */
static int UIDEntryHeapClassSetPage(FixedHeap *heap, void *page)
{
	return FixedHeapSetPage(heap, page);
}

/* name unknown: no links and no users left (one link with UID_ENTRY_ATTR_SINGLE_LINK) */
__attribute__((noinline)) int UIDEntryIsUnused(const UIDEntry *entry)
{
	if (entry->attr & UID_ENTRY_ATTR_PERMANENT)
		return 0;
	if (entry->attr & UID_ENTRY_ATTR_SINGLE_LINK) {
		if (entry->linkCount > 1)
			return 0;
	} else if (entry->linkCount != 0) {
		return 0;
	}
	return entry->useCount == 0;
}

/* name unknown: the kernel fixed heap for objects of a size */
void *UIDGetKernelHeapForSize(SceSize size)
{
	FixedHeap *heap;

	if (size <= 0x20)
		return g_uidManager->fixedHeap32B;
	if (size <= 0x40)
		return g_uidManager->fixedHeap64B;
	if (size <= 0x80) {
		heap = g_uidManager->fixedHeap128B;
		if (heap != NULL)
			return heap;
		goto fallback;
	}
	if (size <= 0x100) {
		heap = g_uidManager->fixedHeap256B;
		if (heap != NULL)
			return heap;
		goto fallback;
	}
	if (size <= 0x200) {
		heap = g_uidManager->fixedHeap512B;
		if (heap != NULL)
			return heap;
	}
fallback:
	return g_uidManager->heapDefault;
}

int UIDEntrySetNameWithNameHeapRaw(UIDEntry *entry, const char *name)
{
	NameEntry *nameEntry;
	int ret;

	if (entry->name != NULL)
		NameHeapDelete(entry->name, NAME_HEAP_FLAG_BY_LINK | 1);
	ret = NameHeapInsert(name, 1, &nameEntry);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 976, "NameHeapInsert([%s], default) failed 0x%08x\n", name, ret);
		return ret;
	}
	entry->name = nameEntry->name;
	return 0;
}

static inline void UIDEntrySet(UIDEntry *entry, SceClass *cls, const char *name,
			       SceObjectBase *object)
{
	if (name != NULL)
		UIDEntrySetNameWithNameHeapRaw(entry, name);
	if (object == NULL) {
		if (entry->object == NULL)
			SCE_DBG_PRINTF(0xb, 1995, "Warning: There is no pObject\n");
	} else {
		entry->object = object;
		if (cls != NULL)
			object->sce_class = cls;
	}
}

int GUIDSet(SceUID guid, SceClass *cls, const char *name, SceObjectBase *object)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	heap = GUID_ENTRY_HEAP();
	state = UID_WRITE_LOCK(heap, 4873);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0)
		UIDEntrySet(entry, cls, name, object);
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

/* a kernel address that is mapped; 3.6x keeps only GCC's split tail (IsSafe.part) */
#if !FW_SINCE(0x04000000)
static
#endif
int IsSafe(const void *address)
{
	SceUIntPtr pa;

	if ((SceUIntPtr)address >= 0x10000000) {
		SCE_DBG_PRINTF(0xb, 176, "Invalid vbase=%p\n", address);
		return 0;
	}
	return ksceKernelVAtoPA(address, &pa) >= 0;
}

/* name unknown: vector add, an entry's info */
static void UIDFuncAddEntryInfo(UIDFuncVector *vector, UIDEntry *entry)
{
	SceKernelUIDEntryInfo *info;
	SceObjectBase *object;
	void *buffer = vector->buffer;

	vector->total++;
	if (buffer == NULL || vector->count * sizeof(*info) >= vector->size)
		return;
	object = entry->object;
	info = (SceKernelUIDEntryInfo *)buffer + vector->count;
	info->uid = entry->uid;
	if (!IsSafe(object)) {
		info->object = NULL;
		info->cls = NULL;
	} else {
		SceClass *objectClass = object->sce_class;

		info->object = object;
		info->cls = objectClass;
	}
	info->guid = entry->guid;
	info->linkCount = entry->linkCount;
	info->useCount = entry->useCount;
	info->attr = entry->attr;
	info->name = entry->name;
	info->pid = entry->pid;
	vector->count++;
}

/* walk callback of the UID vectors by class */
typedef struct UIDVectorByClass {
	UIDEntryHeap *heap;
	SceClass *cls;
	SceUInt32 level;
	int size;				/* of uids, in UIDs */
	SceUID *uids;
	int count;
	SceUInt32 total;
} UIDVectorByClass;

static int UIDEntryCheckClass(void *object, SceUInt32 index, void *arg)
{
	UIDEntry *entry = object;
	UIDVectorByClass *vector = arg;
	SceObjectBase *base;
	SceClass *cls;
	SceUID uid;
	SceUID *uids;

	if (!(entry->attr & UID_ENTRY_ATTR_ACTIVE))
		return 0;
	base = entry->object;
	uid = entry->uid;
	if (!IsSafe(base)) {
		SCE_DBG_PRINTF(0xb, 1508, "Error: pObject=%p, ucount=%d, linkCount=%d, [%s]\n", base,
			       entry->useCount, entry->linkCount, entry->name);
		return 0;
	}
	if ((SceUIntPtr)base & 3) {
		SCE_DBG_PRINTF(0xb, 1515, "Error: pObject=%p, ucount=%d, linkCount=%d, [%s]\n", base,
			       entry->useCount, entry->linkCount, entry->name);
		return 0;
	}
	cls = base->sce_class;
	if (!IsSafe(cls) || entry->object == NULL || entry->linkCount > UID_ENTRY_MAX_LCOUNT ||
	    entry->useCount > UID_ENTRY_MAX_UCOUNT || UIDEntryGetLevel(entry) > vector->level ||
	    cls == NULL || cls->magic != SCE_CLASS_MAGIC || vector->cls == NULL)
		return 0;
	for (; cls != vector->cls; cls = cls->uidclass) {
		if (cls->uidclass == NULL)
			return 0;
	}
	uids = vector->uids;
	vector->total++;
	if (uids == NULL || vector->count >= vector->size)
		return 0;
	uids[vector->count] = uid;
	vector->count++;
	return 0;
}

/* walk callback of the entry infos (by class, or all with cls NULL) */
typedef struct UIDEntryInfoWalk {
	SceClass *cls;
	SceUInt32 level;
	SceUInt32 reserved;
	SceKernelUIDEntryInfo *infos;
	int size;				/* of infos, in infos */
	int count;
	SceUInt32 total;
} UIDEntryInfoWalk;

static int UIDEntryFuncForGetInfo(void *object, SceUInt32 index, void *arg)
{
	UIDEntry *entry = object;
	UIDEntryInfoWalk *walk = arg;
	SceKernelUIDEntryInfo *info;
	SceObjectBase *base = entry->object;
	SceClass *cls;

	if (base == NULL || entry->linkCount > UID_ENTRY_MAX_LCOUNT ||
	    entry->useCount > UID_ENTRY_MAX_UCOUNT || !(entry->attr & UID_ENTRY_ATTR_ACTIVE) ||
	    UIDEntryGetLevel(entry) > walk->level)
		return 0;
	if (walk->cls != NULL) {
		if (!IsSafe(base)) {
			SCE_DBG_PRINTF(0xb, 1729, "Illegal pObject: name [%s] %p\n", entry->name,
				       base);
			return 0;
		}
		for (cls = base->sce_class; cls != NULL; cls = cls->uidclass) {
			if (cls == walk->cls)
				break;
		}
		if (cls == NULL)
			return 0;
	}
	if (walk->count < walk->size && walk->infos != NULL) {
		info = &walk->infos[walk->count];
		info->size = sizeof(*info);
		info->uid = entry->uid;
		if (IsSafe(base)) {
			SceClass *objectClass = base->sce_class;

			info->object = base;
			info->cls = objectClass;
		} else {
			SCE_DBG_PRINTF(0xb, 1751, "Illegal pObject: name [%s] %p\n", entry->name,
				       base);
		}
		info->guid = entry->guid;
		info->useCount = entry->useCount;
		info->linkCount = entry->linkCount;
		info->attr = entry->attr;
		info->name = entry->name;
		walk->count++;
	}
	walk->total++;
	return 0;
}

int GetPUIDEntryHeap(SceUID pid, UIDEntryHeap **heap)
{
	int ret;

	ret = ksceKernelSysrootGetPUIDEntryHeap(pid, (void **)heap);
	if (ret >= 0 && *heap == GUID_ENTRY_HEAP()) {
		SCE_DBG_PRINTF(0xb, 6883, "Cannot return GUIDEntryHeap\n");
		*heap = NULL;
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	return ret;
}

static inline int UIDEntryReferObjectCheckClassLevel(UIDEntry *entry, SceClass *cls,
						     SceUInt32 level, SceObjectBase **out)
{
	SceObjectBase *object;
	SceClass *objectClass;

	if (UIDEntryGetLevel(entry) > level)
		return UID_ERROR_LEVEL;
	object = entry->object;
#if !FW_DEBUG
	objectClass = object->sce_class;
#endif
	entry->useCount++;
#if FW_DEBUG
	if (object == NULL) {
		SCE_DBG_PRINTF(0xb, 2692, "Warning:pEntry[%p]->pObject is SCE_NULL\n", entry);
		object = entry->object;
	}
	objectClass = object->sce_class;
#endif
	if (cls == NULL || cls == objectClass) {
		*out = object;
		return 0;
	}
	*out = NULL;
	return UID_ERROR_CLASS;
}

int PUIDReferObject(SceUID pid, SceUID puid, SceClass *cls, SceUInt32 level,
			   SceObjectBase **out)
{
	UIDEntryHeap *heap = NULL;
	UIDEntry *entry;
	int state, ret;

	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = UID_WRITE_LOCK(heap, 7160);
	/* an invalid PUID returns the PUID itself, as in the original */
	if (UIDEntryCheckInvalid(heap, puid, 0, &entry) >= 0)
		puid = UIDEntryReferObjectCheckClassLevel(entry, cls, level, out);
	UID_WRITE_UNLOCK(heap, state);
	return puid;
}

/* one user more of a GUID */
static inline void GUIDRefer(SceUID guid)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	int state;

	state = UID_WRITE_LOCK(heap, 5748);
	UIDEntryCheckInvalid(heap, guid, 0, &entry);
	entry->useCount++;
	UID_WRITE_UNLOCK(heap, state);
}

/* the GUID of a PUID; flags bit 4: one user more of it */
int PUID2GUID(SceUID pid, SceUID puid, SceClass *cls, SceUInt32 flags)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	SceUID guid;
	int state, ret;

	if (!UIDIsPUID(puid))
		return UID_ERROR_INVALID_UID;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = UID_READ_LOCK(heap, 6971);
	ret = UIDEntryCheckInvalid(heap, puid, flags, &entry);
	if (ret < 0) {
		UID_READ_UNLOCK(heap, state);
		return ret;
	}
	if (cls != NULL) {
		if (entry->object == NULL)
			ret = UID_ERROR_INVALID_UID;
		else if (cls == entry->object->sce_class)
			goto found;
		else
			ret = UID_ERROR_CLASS;
		UID_READ_UNLOCK(heap, state);
		return ret;
	}
found:
	if ((entry->attr & UID_ENTRY_ATTR_PUID) && (entry->attr & UID_ENTRY_ATTR_LINK_ANY))
		guid = entry->guid;
	else
		guid = UID_ERROR_INVALID_UID;
	if (flags & 0x10)
		GUIDRefer(guid);
	UID_READ_UNLOCK(heap, state);
#if FW_DEBUG
	if (guid >= 0 && ksceKernelGetAssertLevel() > 0) {
		const char *name;

		if (GUIDGetName(guid, &name) < 0)
			name = NULL;
		ksceKernelPrintfAssertLevel(1, guid > 0x1005f, SCE_DBGINFO(6987),
					    __builtin_return_address(0),
					    "error: PUID2GUID(pid=0x%08X, puid=0x%08X), guid=0x%08X, name=[\"%s\"]\n",
					    pid, puid, guid, name);
	}
#endif
	return guid;
}

/* mode 0: any class, 1: cls exactly, 2: cls or a subclass */
int UIDEntryCheckClassRaw(const UIDEntry *entry, SceClass *cls, int mode)
{
	SceClass *c;

	if (mode == 1) {
		if (cls != entry->object->sce_class) {
			SCE_DBG_PRINTF(0xb, 2514, "SCE_KERNEL_ERROR_INVALID_UID_CLASS\n");
			return UID_ERROR_CLASS;
		}
	} else if (mode != 0) {
		if (mode != 2)
			return UID_ERROR_INVALID_UID;
		for (c = entry->object->sce_class; c != NULL; c = c->uidclass) {
			if (c == cls)
				return 0;
		}
		SCE_DBG_PRINTF(0xb, 2521, "SCE_KERNEL_ERROR_INVALID_UID_SUBCLASS\n");
		return UID_ERROR_SUBCLASS;
	}
	return 0;
}

/* an entry about to be freed must be destructed and in the heap */
static inline void UIDEntryHeapCheckFreeUIDEntry(UIDEntryHeap *heap, UIDEntry *entry)
{
#if FW_DEBUG
	if (entry->attr & UID_ENTRY_ATTR_ALL) {
		SCE_DBG_PRINTF(0xb, 4309, "(%p,%p)\n", heap, entry);
		SCE_DBG_ASSERT_FAIL(4310);
	}
	SCE_DBG_ASSERT(FixedHeapGetIndexRaw(&heap->fixedHeap, entry) >= 0, 4313);
#endif
}

/* name from Sony's messages: a free entry of the heap, checked */
static inline int UIDEntryHeapAllocateObject(UIDEntryHeap *heap, UIDEntry **entry)
{
	int ret = FixedHeapAllocObjectRaw(&heap->fixedHeap, 0, (void **)entry);

	if (ret >= 0)
		UIDEntryHeapCheckFreeUIDEntry(heap, *entry);
	return ret;
}

/* name from Sony's messages */
static inline int UIDEntryHeapFreeObject(UIDEntryHeap *heap, UIDEntry *entry)
{
	UIDEntryHeapCheckFreeUIDEntry(heap, entry);
	return FixedHeapFreeObject(&heap->fixedHeap, entry);
}

/* the object of a closed entry, freed by DeleteParamClean after the lock */
static inline int DeleteParamAdd(DeleteParam *param, SceObjectBase *object, void *heap,
				 SceUInt32 type)
{
	int n = param->nObject;

	if (n >= DELETE_PARAM_OBJECTS) {
		SCE_DBG_ASSERT_FAIL(4185);
		return SCE_KERNEL_ERROR_ERROR;
	}
	param->objects[n].type = type;
	param->objects[n].object = object;
	param->objects[n].heap = heap;
	param->nObject = n + 1;
	return 0;
}

int UIDEntryClose(UIDEntryHeap *heap, UIDEntry *entry, DeleteParam *param)
{
	SceUInt32 attr = entry->attr;
	SceObjectBase *object;
	void *objectHeap;
	int ret;

	if (attr & UID_ENTRY_ATTR_GUID_ANY) {
		object = entry->object;
		objectHeap = (attr & UID_ENTRY_ATTR_PUID) ? NULL : entry->heap;
		if (object == NULL) {
			SCE_DBG_PRINTF(0xb, 1931, "Warning:There is no pObject\n");
			SCE_DBG_ASSERT_FAIL(1932);
		}
	} else {
		/* a link: its base is closed next */
		object = NULL;
		objectHeap = NULL;
		if (attr & UID_ENTRY_ATTR_CLOSED) {
			UIDEntry *base = NULL;

			if (!(attr & UID_ENTRY_ATTR_PUID) && (attr & UID_ENTRY_ATTR_LINK_ANY))
				base = entry->base;
			if (param->next == NULL)
				param->next = base;
		}
	}
	UIDEntryDestructorRaw(entry);
	UIDEntryHeapCheckFreeUIDEntry(heap, entry);
	ret = FixedHeapFreeObjectRaw(&heap->fixedHeap, entry);
	if (ret < 0 || (objectHeap == NULL && !(attr & UID_ENTRY_ATTR_DESTRUCT)))
		return ret;
	/* GUIDEntryHeapCloseRaw */
	if (object == NULL) {
		SCE_DBG_PRINTF_FUNC(0xb, 4419, "GUIDEntryHeapCloseRaw",
				    "UIDEntryClose returns pObject==SCE_NULL\n");
		return ret;
	}
	return DeleteParamAdd(param, object, objectHeap,
			      (attr & UID_ENTRY_ATTR_TYPE_MASK) >> UID_ENTRY_ATTR_TYPE_SHIFT);
}

/* one link less, closing what is left unused down the chain to the GUID entry */
int UIDEntryCloseLink(UIDEntryHeap *heap, UIDEntry *entry, DeleteParam *param)
{
	UIDEntry *base;
	SceUInt32 attr;
	int ret;

	if (entry->object == NULL)
		goto warn;
	for (;;) {
		attr = entry->attr;
		if (attr & UID_ENTRY_ATTR_GUID_ANY) {
			if (entry->linkCount == 0)
				return UID_ERROR_INVALID_UID;
			entry->linkCount--;
			if (!UIDEntryIsUnused(entry))
				return 0;
			goto close;
		}
		if (!(attr & UID_ENTRY_ATTR_LINK_ANY) || entry->linkCount == 0)
			return UID_ERROR_INVALID_UID;
		entry->linkCount--;
		if (!UIDEntryIsUnused(entry))
			return 0;
		if (attr & UID_ENTRY_ATTR_PUID)
			goto close;
		base = entry->base;
		ret = UIDEntryClose(heap, entry, param);
		if (ret < 0)
			return ret;
		if (base == NULL)
			return 0;
		entry = base;
		if (entry->object == NULL) {
warn:
			SCE_DBG_PRINTF(0xb, 2142, "Warning:pEntry[%p]->pObject is SCE_NULL\n", entry);
		}
	}
close:
	ret = UIDEntryClose(heap, entry, param);
	if (ret < 0)
		return ret;
	return 0;
}

/* the UID an entry has, from its index; puid: UID_PUID for a PUID */
#if FW_DEBUG
static inline SceUID UIDEntry2UID(UIDEntryHeap *heap, const UIDEntry *entry, SceUInt32 puid)
{
	int index;
	SceUID uid;

	index = FixedHeapGetIndexRaw(&heap->fixedHeap, entry);
	if (index < 0) {
		SCE_DBG_PRINTF(0xb, 1280, "FixedHeapGetIndexRaw(%p) failed 0x%08x\n", entry, index);
		return index;
	}
	if ((unsigned int)index != UIDGetIndex(entry->uid)) {
		SCE_DBG_PRINTF(0xb, 1286, "eindex=0x%08x != 0x%08x\n", index,
			       UIDGetIndex(entry->uid));
		SCE_PANIC(1287);
	}
	uid = (entry->uid & UID_GENERATION_MASK) | puid | 1 | index << 1;
	if ((uid ^ entry->uid) & ~UID_PUID)
		SCE_DBG_PRINTF(0xb, 1293, "Error:uid=0x%08x != pEntry->uid=0x%08x\n", uid,
			       entry->uid);
	return uid;
}

#endif

/* the entry's UID, checked against its index in debug builds */
static inline SceUID UIDEntryHeapCheckIndex(UIDEntryHeap *heap, const UIDEntry *entry,
					    SceUInt32 puid)
{
#if FW_DEBUG
	SceUID uid;

	uid = UIDEntry2UID(heap, entry, puid);
	if (uid < 0) {
		SCE_DBG_PRINTF(0xb, 4455, "UIDEntry2UID(pEntry %p) failed 0x%08x\n", entry, uid);
		SCE_DBG_ASSERT_FAIL(4456);
	}
	if (uid != entry->uid) {
		SCE_DBG_PRINTF(0xb, 4460, "Error: uid check failed 0x%08x != 0x%08x\n", uid,
			       entry->uid);
		SCE_DBG_ASSERT_FAIL(4461);
	}
#endif
	return entry->uid;
}

/* a link of the name of its base */
static inline int UIDEntryConstructNameLinkRaw(UIDEntry *entry, const char *name, SceUInt32 attr)
{
	NameEntry *nameEntry;
	SceUInt32 flags;
	int ret;

	if (name == NULL) {
		entry->name = NULL;
		SCE_DBG_PRINTF(0xb, 780, "Warning:name is SCE_NULL\n");
		return 0;
	}
	flags = NAME_HEAP_FLAG_BY_LINK | ((attr & UID_ENTRY_ATTR_NAME_KIND) ? NAME_HEAP_FLAG_SECOND :
										 1);
	ret = NameHeapLink(name, flags, &nameEntry);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 772, "NameHeapLink([%s],0x%08x) failed 0x%08x\n", name, flags,
			       ret);
		entry->name = NULL;
		entry->attr &= ~UID_ENTRY_ATTR_ALL;
		return ret;
	}
	entry->name = nameEntry->name;
	return 0;
}

/* what a PUID or a link copies from its GUID entry */
typedef struct GUIDBaseInfo {
	SceUID guid;
	SceUInt32 attr;
	SceObjectBase *object;
	SceClass *cls;
	const char *name;			/* 0x10 */
	SceUInt32 reserved;
	void *heap;
	UIDEntry *base;
	SceUID baseGuid;			/* 0x20 */
} GUIDBaseInfo;

int UIDEntryHeapSetPUIDEntryChainRaw(UIDEntryHeap *heap, UIDEntry *entry, SceUID guid,
					    const GUIDBaseInfo *info, SceUInt32 attr)
{
	SceUInt16 used;
	SceObjectBase *object;
	SceClass *cls;
	SceUID pid;
	const char *name;
	SceUInt32 entryAttr;
	int ret;

	used = heap->fixedHeap.totalCount - heap->fixedHeap.freeCount;
	if (used > 0x1800) {
		SCE_DBG_PRINTF(0xb, 4579, "PUID overflow: 0x%08x < 0x%08x\n", 0x1800, used);
		return SCE_KERNEL_ERROR_UID_FOR_PRODCESS_OVERFLOW;
	}
	attr |= info->attr;
	entryAttr = (attr & ~(UID_ENTRY_ATTR_PUID | UID_ENTRY_ATTR_ALL)) | UID_ENTRY_ATTR_PUID |
		    UID_ENTRY_ATTR_LINK;
	if (!(attr & UID_ENTRY_ATTR_LEVEL_MASK))
		return SCE_KERNEL_ERROR_ERROR;
	/* UIDEntryConstructPChain */
	object = info->object;
	cls = object != NULL ? object->sce_class : NULL;
	pid = heap->pid;
	name = info->name;
	entry->reserved = 0;
	if (object == NULL)
		SCE_DBG_PRINTF_FUNC(0xb, 1075, "UIDEntryConstructPChain",
				    "Warning:pObject is SCE_NULL\n");
	entry->object = object;
	entry->pid = pid;
	entry->attr = entryAttr;
	if (cls != NULL)
		entry->classAttr = cls->attributes;
	else
		entry->classAttr = 0;
	entry->useCount = 0;
	entry->linkCount = 1;
	entry->guid = guid;
	if (!(entry->uid & UID_PUID))
		entry->uid |= UID_PUID;
	ret = UIDEntryConstructNameLinkRaw(entry, name, attr);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 4602, "UIDEntryConstructPChain failed 0x%08x\n", ret);
		return ret;
	}
	return UIDEntryHeapCheckIndex(heap, entry, UID_PUID);
}

/* a link entry of a GUID entry, in the GUID entry heap */
int UIDEntryConstructByBase(UIDEntryHeap *heap, UIDEntry *entry, SceUID pid, UIDEntry *base)
{
	SceUInt32 attr = base->attr;
	SceObjectBase *object = base->object;
	const char *name = base->name;
	SceClass *cls = object != NULL ? object->sce_class : NULL;
	int ret;

	if (!(attr & UID_ENTRY_ATTR_LEVEL_MASK))
		return SCE_KERNEL_ERROR_ERROR;
	/* UIDEntryConstructGChain */
	entry->reserved = 0;
	if (object == NULL)
		SCE_DBG_PRINTF_FUNC(0xb, 1042, "UIDEntryConstructGChain",
				    "Warning:pObject is SCE_NULL\n");
	entry->object = object;
	entry->pid = pid;
	entry->attr = (attr & ~(UID_ENTRY_ATTR_PUID | UID_ENTRY_ATTR_ALL)) | UID_ENTRY_ATTR_LINK;
	if (cls != NULL)
		entry->classAttr = cls->attributes;
	else
		entry->classAttr = 0;
	entry->useCount = 0;
	entry->linkCount = 1;
	entry->base = base;
	if (entry->uid & UID_PUID)
		entry->uid &= ~UID_PUID;
	ret = UIDEntryConstructNameLinkRaw(entry, name, attr);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 1819, "UIDEntryConstructChain([%s]) failed 0x%08x\n", name, ret);
		UIDEntryDestructorRaw(entry);
		return ret;
	}
	return UIDEntryHeapCheckIndex(heap, entry, 0);
}

/* a GUID entry's class, if it has an object */
static inline int UIDEntryGetClass(const UIDEntry *entry, SceClass **cls)
{
	if (entry->object == NULL) {
		SCE_DBG_PRINTF(0xb, 1141, "pEntry[%p]->pObject is SCE_NULL\n", entry);
		*cls = NULL;
		return UID_ERROR_INVALID_UID;
	}
	*cls = entry->object->sce_class;
	return 0;
}

/* the GUID of a registered object goes; the object stays */
int GUIDUnregister(SceUID guid)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	heap = GUID_ENTRY_HEAP();
	state = UID_WRITE_LOCK(heap, 4809);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0) {
		if (entry->linkCount != 0)
			entry->linkCount--;
#if FW_DEBUG
		SceClass *cls, *memblockClass;
		SceUInt8 linkCount;
		SceUInt16 useCount;

		if (UIDEntryIsUnused(entry) != 1 && UIDEntryGetClass(entry, &cls) >= 0) {
			memblockClass = ksceKernelGetUIDMemBlockClass();
			for (; cls != NULL; cls = cls->uidclass) {
				if (cls == memblockClass)
					break;
			}
			linkCount = entry->linkCount;
			useCount = entry->useCount;
			if (cls == NULL || linkCount != 1 || useCount != 0)
				SCE_DBG_PRINTF(0xb, 4830, "Warning: Unregister 0x%08x[%s]%d:%d\n", guid,
					       entry->name, linkCount, useCount);
		}
#endif
		entry->object = NULL;
		UIDEntryDestructorRaw(entry);
		UIDEntryHeapCheckFreeUIDEntry(heap, entry);
		ret = FixedHeapFreeObjectRaw(&heap->fixedHeap, entry);
		if (ret < 0)
			SCE_DBG_PRINTF(0xb, 4844, "UIDEntryHeapFreeObjectRaw failed 0x%08x\n", ret);
	}
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

/* the entry's name, inserted into the name heap its attr selects */
static inline int UIDEntryConstructNameInsertRaw(UIDEntry *entry, const char *name, SceUInt32 attr)
{
	NameEntry *nameEntry;
	SceUInt32 flags;
	int ret;

	if (name == NULL) {
		entry->name = NULL;
		return 0;
	}
	flags = NAME_HEAP_FLAG_BY_LINK | ((attr & UID_ENTRY_ATTR_NAME_KIND) ?
					  NAME_HEAP_FLAG_SECOND : 1);
	ret = NameHeapInsert(name, flags, &nameEntry);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 746, "NameHeapInsert([%s], 0x%08x) failed 0x%08x\n", name, flags,
			       ret);
		return ret;
	}
	if ((flags & 0xf) == NAME_HEAP_FLAG_SECOND)
		NameEntrySetUIDEntry(nameEntry, entry);
	entry->name = nameEntry->name;
	return ret;
}

/* a free entry made the GUID entry of object */
static inline int UIDEntryConstructObjectRaw(UIDEntry *entry, SceClass *cls, const char *name,
					     SceObjectBase *object, SceUID pid, void *objectHeap,
					     SceUInt32 attr)
{
	int ret;

	object->sce_class = cls;
	entry->object = object;
	entry->pid = pid;
	entry->reserved = 0;
	entry->attr = (attr & ~(UID_ENTRY_ATTR_PUID | UID_ENTRY_ATTR_ALL)) | UID_ENTRY_ATTR_GUID;
	if (cls != NULL)
		entry->classAttr = cls->attributes;
	else
		entry->classAttr = 0;
	entry->useCount = 0;
	entry->linkCount = 1;
	if (entry->uid & UID_PUID)
		entry->uid &= ~UID_PUID;
	entry->heap = objectHeap;
	ret = UIDEntryConstructNameInsertRaw(entry, name, attr);
	if (ret < 0) {
		entry->name = NULL;
		entry->attr &= ~UID_ENTRY_ATTR_ALL;
	}
	return ret;
}

/* a GUID entry for an object; returns the GUID */
int UIDEntryHeapAllocateGUIDEntryObject(UIDEntryHeap *heap, SceClass *cls, const char *name,
					       SceObjectBase *object, SceUID pid, void *objectHeap,
					       SceUInt32 attr)
{
	UIDEntry *entry;
	int state, ret;

	state = UID_WRITE_LOCK(heap, 4494);
	ret = FixedHeapAllocObjectRaw(&heap->fixedHeap, 1, (void **)&entry);
	UIDEntryHeapCheckFreeUIDEntry(heap, entry);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 4500, "UIDEntryHeapAllocateObjectRaw failed 0x%08x\n", ret);
		goto out;
	}
	if (!(attr & UID_ENTRY_ATTR_LEVEL_MASK)) {
		UIDEntryHeapCheckFreeUIDEntry(heap, entry);
		ret = SCE_KERNEL_ERROR_ERROR;
		FixedHeapFreeObjectRaw(&heap->fixedHeap, entry);
		goto out;
	}
	ret = UIDEntryConstructObjectRaw(entry, cls, name, object, pid, objectHeap, attr);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 4526, "UIDEntryConstructObjectRaw failed 0x%08x\n", ret);
		UIDEntryHeapCheckFreeUIDEntry(heap, entry);
		FixedHeapFreeObjectRaw(&heap->fixedHeap, entry);
		goto out;
	}
	UIDEntryHeapCheckIndex(heap, entry, 0);
	ret = entry->uid;
out:
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

/* the base info of a GUID entry, one link more */
static inline void GUIDBaseInfoSetLinks(GUIDBaseInfo *info, const UIDEntry *base)
{
	SceUInt32 attr = base->attr;

	if (attr & UID_ENTRY_ATTR_PUID) {
		info->heap = NULL;
		info->base = NULL;
		info->baseGuid = (attr & UID_ENTRY_ATTR_LINK_ANY) ? base->guid :
								     UID_ERROR_INVALID_UID;
	} else {
		info->heap = (attr & UID_ENTRY_ATTR_GUID_ANY) ? base->heap : NULL;
		info->base = (attr & UID_ENTRY_ATTR_LINK_ANY) ? base->base : NULL;
		info->baseGuid = UID_ERROR_INVALID_UID;
	}
}

static inline int UIDEntryIncLinkCountAndGetInfoForByName(UIDEntry *base, SceClass *cls, int mode,
							  GUIDBaseInfo *info)
{
	int ret;

	ret = UIDEntryCheckClassRaw(base, cls, mode);
	if (ret < 0)
		return ret;
	if (UIDEntryIncLinkCountRaw(base) != 1)
		return UID_ERROR_LINK_COUNT;
	info->object = base->object;
	info->guid = base->uid;
	info->attr = base->attr;
	info->name = base->name;
	if (info->name == NULL)
		SCE_DBG_PRINTF(0xb, 2640, "ERROR: There is no name.\n");
	GUIDBaseInfoSetLinks(info, base);
	return ret;
}

static inline int GUIDLinkBaseInfoByName(const char *name, SceClass *cls, int mode,
					 GUIDBaseInfo *info)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *base;
	int state, ret;

	info->baseGuid = -1;
	info->object = NULL;
	info->cls = NULL;
	info->name = NULL;
	info->heap = NULL;
	state = UID_WRITE_LOCK(heap, 5604);
	ret = GUIDEntryHeapFindByNameRaw(heap, name, 0, &base);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 5608, "GUIDEntryHeapFindByNameRaw([%s]) failed 0x%08x\n", name,
			       ret);
		goto out;
	}
	ret = UIDEntryIncLinkCountAndGetInfoForByName(base, cls, mode, info);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 5614, "UIDEntryIncLinkCountAndGetInfoForByName failed 0x%08x\n",
			       ret);
out:
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

static inline void GUIDUnlinkBaseByName(const char *name)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *base;
	int state;

	state = UID_WRITE_LOCK(heap, 5635);
	if (GUIDEntryHeapFindByNameRaw(heap, name, 0, &base) >= 0) {
		/* UIDEntryDecLinkCount */
		if (base->linkCount == 0)
			SCE_DBG_PRINTF(0xb, 5645, "UIDEntryDecLinkCount failed 0x%08x\n", SCE_KERNEL_ERROR_UID_LINK_COUNT_UNDERFLOW);
		else
			base->linkCount--;
	}
	UID_WRITE_UNLOCK(heap, state);
}

/* a PUID of pid for the GUID named name */
int PUIDOpenByName(SceUID pid, const char *name, SceClass *cls, int mode, SceUInt32 attr)
{
	UIDEntryHeap *heap = NULL;
	UIDEntry *entry;
	GUIDBaseInfo info;
	int state, ret;

	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	ret = UIDEntryHeapAllocateObject(heap, &entry);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 7355, "UIDEntryHeapAllocateObject failed 0x%08x\n", ret);
		return ret;
	}
	ret = GUIDLinkBaseInfoByName(name, cls, mode, &info);
	if (ret < 0)
		goto free;
	state = UID_WRITE_LOCK(heap, 7368);
	ret = UIDEntryHeapSetPUIDEntryChainRaw(heap, entry, info.guid, &info, attr);
	UID_WRITE_UNLOCK(heap, state);
	if (ret >= 0 || entry == NULL)
		return ret;
	SCE_DBG_PRINTF(0xb, 7383, "puid=0x%08x\n", ret);
	GUIDUnlinkBaseByName(name);
free:
	UIDEntryHeapFreeObject(heap, entry);
	return ret;
}

static inline int GUIDLinkBaseInfoByGUID(SceUID guid, SceClass *cls, int mode,
					 GUIDBaseInfo *info)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry, *base;
	int state, ret;

	state = UID_WRITE_LOCK(heap, 5529);
	info->object = NULL;
	info->cls = NULL;
	info->name = NULL;
	info->heap = NULL;
	info->baseGuid = -1;
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret < 0) {
		UID_WRITE_UNLOCK(heap, state);
		return ret;
	}
	base = entry;
	ret = UIDEntryCheckClassRaw(base, cls, mode);
	if (ret >= 0) {
		if (UIDEntryIncLinkCountRaw(base) == 1) {
			info->attr = base->attr;
			info->object = base->object;
			info->cls = info->object != NULL ? info->object->sce_class : NULL;
			info->name = base->name;
			info->guid = guid;
			GUIDBaseInfoSetLinks(info, base);
		} else {
			ret = UID_ERROR_LINK_COUNT;
		}
	}
	if (info->name == NULL)
		SCE_DBG_PRINTF(0xb, 5548, "There is no name for guid=0x%08x\n", guid);
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

static inline void GUIDSetBitAttr(SceUID guid, SceUInt32 bits)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	int state;

	state = UID_WRITE_LOCK(heap, 5399);
	if (UIDEntryCheckInvalid(heap, guid, 0, &entry) >= 0 &&
	    (entry->attr & UID_ENTRY_ATTR_ACTIVE) && guid == entry->uid)
		entry->attr |= bits;
	UID_WRITE_UNLOCK(heap, state);
}

static inline void GUIDUnlinkBaseByGUID(SceUID guid)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *base;
	int state;

	state = UID_WRITE_LOCK(heap, 5568);
	if (UIDEntryCheckInvalid(heap, guid, 0, &base) >= 0 && base->linkCount != 0)
		base->linkCount--;
	UID_WRITE_UNLOCK(heap, state);
}

/* a PUID of pid for guid; flags bit 0: the GUID is freed with one link left, bit 1: weak */
int PUIDOpenByGUID(SceUID pid, SceUID guid, SceUInt32 flags, SceClass *cls, int mode,
			  SceUInt32 attr)
{
	UIDEntryHeap *heap = NULL;
	UIDEntry *entry;
	GUIDBaseInfo info;
	int state, ret;

	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 7277, "GetPUIDEntryHeap failed 0x%08x\n", ret);
		return ret;
	}
	ret = UIDEntryHeapAllocateObject(heap, &entry);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 7283, "UIDEntryHeapAllocateObject failed 0x%08x\n", ret);
		return ret;
	}
	ret = GUIDLinkBaseInfoByGUID(guid, cls, mode, &info);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 7290, "GUIDLinkBaseInfoByGUID(0x%08x) failed 0x%08x\n", guid,
			       ret);
		SCE_DBG_PRINTF(0xb, 7291, "->UIDEntryHeapFreeObject(%p,%p)\n", heap, entry);
		goto free;
	}
	state = UID_WRITE_LOCK(heap, 7296);
	ret = UIDEntryHeapSetPUIDEntryChainRaw(heap, entry, guid, &info, attr);
	if (flags & 1)
		GUIDSetBitAttr(guid, UID_ENTRY_ATTR_SINGLE_LINK);
	if (flags & 2)
		entry->attr |= UID_ENTRY_ATTR_WEAK;
	UID_WRITE_UNLOCK(heap, state);
	if (ret >= 0)
		return ret;
	SCE_DBG_PRINTF(0xb, 7321, "error 0x%08x clean up %p\n", ret, entry);
	GUIDUnlinkBaseByGUID(guid);
free:
	UIDEntryHeapFreeObject(heap, entry);
	return ret;
}

/* the constructors of cls and its base classes, the base class first */
int sceUIDCallConstructor(SceClass *cls, SceObjectBase *object)
{
	SceClass *chain[16];
	int n = 0, ret;

	object->sce_class = cls;
	for (;;) {
		chain[n] = cls;
		if (cls == ksceKernelGetUIDClass() || cls->uidclass == NULL)
			break;
		n++;
		cls = cls->uidclass;
		if (n == 16) {
			SCE_DBG_ASSERT_FAIL(380);
			return UID_ERROR_NO_OBJECT;
		}
	}
	for (;;) {
		if (cls->create_cb != NULL) {
			ret = cls->create_cb(object);
			if (ret < 0)
				return ret;
		}
		if (n-- == 0)
			return 0;
		cls = chain[n];
	}
}

/* an object of cls (from heap, or object) with a GUID */
int GUIDCreate(SceClass *cls, const char *name, SceUID pid, FixedHeap *heap,
		      SceObjectBase *object, SceUInt32 attr, SceObjectBase **out)
{
	SceObjectBase *newObject = object;
	SceObjectBase *constructedObject;
	ObjectHeap *objectHeap;
	SceUInt32 type;
	int allocated, limited, ret;

	type = (attr & UID_ENTRY_ATTR_TYPE_MASK) >> UID_ENTRY_ATTR_TYPE_SHIFT;
	if (type == 0)
		type = cls->defaultObjectType;
	if (newObject != NULL) {
		limited = 0;
		allocated = 0;
		objectHeap = NULL;
		goto construct;
	}
	objectHeap = heap->owner;
	limited = 0;
	if (objectHeap != NULL) {
		if (ObjectHeapAllocLimit(objectHeap, type, (void **)&newObject) != 1) {
			ksceKernelPrintf("%s:ObjectHeapAllocLimit(%p,%d) failed\n", "UIDCreateObject",
					 objectHeap, type);
			return UID_ERROR_OBJECT_LIMIT;
		}
		limited = 1;
		if (newObject != NULL) {
			allocated = 0;
			goto construct;
		}
	}
	ret = UIDHeapCallAlloc(heap, cls->itemsize_aligned, (void **)&newObject);
	if (ret < 0) {
		allocated = 0;
		goto failed;
	}
	allocated = 1;
construct:
	ret = sceUIDCallConstructor(cls, newObject);
	if (ret >= 0) {
		constructedObject = newObject;
		ret = UIDEntryHeapAllocateGUIDEntryObject(GUID_ENTRY_HEAP(), cls, name, constructedObject, pid,
							  heap, attr);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 4726, "UIDEntryHeapAllocateGUIDEntryObject failed 0x%08x\n",
				       ret);
		} else if (ret != 0) {
			*out = constructedObject;
			SCE_DBG_ASSERT_PRINTF(ret > 0x1005f, 4735,
					      "error: GUIDCreate(pClass=%p, name=[\"%s\"], pHeap=%p, pObject=%p, attr=0x%08X, ppObject=%p), guid=0x%08X, class name=[\"%s\"]\n",
					      cls, name, heap, constructedObject, attr, out, ret, cls->name);
		}
		return ret;
	}
failed:
	if (limited == 1)
		ObjectHeapFreeLimit(objectHeap, type, newObject);
	if (allocated)
		UIDHeapCallFree(heap, newObject);
	return ret;
}

/* the destructors of the object's class and its base classes, the class first */
int sceUIDCallDestructor(SceObjectBase *object)
{
	SceClass *cls = object->sce_class;
	int ret = 0;

	if (cls == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	for (;;) {
		if (cls->destroy_cb != NULL) {
			ret = cls->destroy_cb(object);
			if (ret < 0)
				break;
		}
		if (cls == ksceKernelGetUIDClass())
			break;
		cls = cls->uidclass;
	}
	return ret;
}

/* destruct and free what UIDEntryClose left */
void DeleteParamClean(DeleteParam *param)
{
	SceObjectBase *object;
	FixedHeap *heap;
	SceUInt32 type;
	int i;

	SCE_DBG_ASSERT_CHECK(param->nObject < 3, 4206);
	for (i = 0; i < param->nObject; i++) {
		object = param->objects[i].object;
		heap = param->objects[i].heap;
		type = param->objects[i].type;
		if (object == NULL) {
			SCE_DBG_PRINTF(0xb, 4218, "Error!: pObject=%p. pHeap=%p, pParam->nObject=%d\n",
				       NULL, heap, param->nObject);
			SCE_DBG_ASSERT_FAIL(4219);
			continue;
		}
		sceUIDCallDestructor(object);
		if (heap != s_registerHeap && heap != NULL &&
		    (heap->owner == NULL || ObjectHeapFreeLimit(heap->owner, type, object) != 1))
			UIDHeapCallFree(heap, object);
	}
}

/* one link less of a GUID, closing it when it was the last */
int GUIDCloseLink(SceUID guid)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	DeleteParam param;
	UIDEntry *entry;
	int state, ret;

	state = UID_WRITE_LOCK(heap, 5081);
	param = (DeleteParam){0};
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0)
		ret = UIDEntryCloseLink(heap, entry, &param);
	UID_WRITE_UNLOCK(heap, state);
	DeleteParamClean(&param);
	return ret;
}

/* close a PUID entry; all: the process is going, close its GUID too unless weak */
int UIDEntryCloseEntryForPUID(UIDEntryHeap *heap, UIDEntry *entry, int all, SceUInt32 flags)
{
	SceUInt32 attr;
	SceObjectBase *object;
	SceClass *cls, *memblockClass;
	SceUID guid;
#if FW_DEBUG
	SceUID puid;
#endif
	int ret;

	if (entry->object == NULL)
		SCE_DBG_PRINTF(0xb, 2993, "Warning:pEntry[%p]->pObject is SCE_NULL\n", entry);
	attr = entry->attr;
	if (!(attr & UID_ENTRY_ATTR_LINK_ANY))
		return UID_ERROR_INVALID_UID;
	if (entry->linkCount == 0)
		return SCE_KERNEL_ERROR_ERROR;
	entry->linkCount--;
	if (!UIDEntryIsUnused(entry)) {
		entry->attr = (attr & ~UID_ENTRY_ATTR_ALL) | UID_ENTRY_ATTR_LINK_CLOSED;
		return 0;
	}
	if (!(attr & UID_ENTRY_ATTR_PUID))
		return UID_ERROR_INVALID_UID;
	guid = entry->guid;
	if (guid < 0)
		return guid;
	object = entry->object;
#if FW_DEBUG
	puid = entry->uid;
#endif
	if (flags & 1) {
		UIDEntryDestructorRaw(entry);
		goto free;
	}
	if (all != 0 && !(attr & UID_ENTRY_ATTR_WEAK)) {
		if (object != NULL) {
			cls = object->sce_class;
			memblockClass = ksceKernelGetUIDMemBlockClass();
			if (cls != NULL && SceSysmemForKernel_7F7D5C4A(cls, memblockClass)) {
				ret = FreeMemBlockByGUID(guid);
				goto closed;
			}
		}
		SCE_DBG_PRINTF_LEVEL(2, 0, 3056,
				     "[DeleteProcess] Force Close PUID 0x%08x [%s], Close  guid=0x%08x\n",
				     puid, entry->name, guid);
		ret = GUIDCloseLink(guid);
	} else {
		ret = GUIDCloseLink(entry->guid);
	}
closed:
	UIDEntryDestructorRaw(entry);
	if (ret == 0x10000)
		return 0;
	if (ret < 0)
		return ret;
free:
	UIDEntryHeapCheckFreeUIDEntry(heap, entry);
	return FixedHeapFreeObjectRaw(&heap->fixedHeap, entry);
}

/* walk callback: close every PUID of a process */
static int UIDEntryIsActiveForCloseAll(void *object, SceUInt32 index, void *arg)
{
	UIDEntry *entry = object;
	UIDEntryHeap **heap = arg;

	if (!(entry->attr & UID_ENTRY_ATTR_ALL))
		return 0;
	if (entry->object == NULL) {
		SCE_DBG_PRINTF(0xb, 1844, "Warning: SCE_NULL\n");
	} else if (entry->linkCount > UID_ENTRY_MAX_LCOUNT) {
		SCE_DBG_PRINTF(0xb, 1849, "Warning: lcount=0x%04x\n", entry->linkCount);
	} else if (entry->useCount > UID_ENTRY_MAX_UCOUNT) {
		SCE_DBG_PRINTF(0xb, 1854, "Warning: ucount=0x%04x\n", entry->useCount);
	} else {
		UIDEntryCloseEntryForPUID(*heap, entry, 1, 0);
	}
	return 0;
}

int PUIDClose(SceUID pid, SceUID puid, SceUInt32 flags)
{
	UIDEntryHeap *heap = NULL;
	UIDEntry *entry;
	int state, ret;

	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = UID_WRITE_LOCK(heap, 7458);
	ret = UIDEntryCheckInvalid(heap, puid, 0, &entry);
	if (ret >= 0)
		ret = UIDEntryCloseEntryForPUID(heap, entry, 0, flags);
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

/* the fields of an entry */
SceUInt32 SceSysmemForKernel_17F12DCE(UIDEntry *entry)
{
	return entry->attr;
}

SceUID SceSysmemForKernel_03A2CAF0(UIDEntry *entry)
{
	return entry->uid;
}

SceUInt8 SceSysmemForKernel_F3DB2D71(UIDEntry *entry)
{
	return entry->linkCount;
}

SceUInt16 SceSysmemForKernel_797C858B(UIDEntry *entry)
{
	return entry->useCount;
}

const char *SceSysmemForKernel_5FD15C6E(UIDEntry *entry)
{
	return entry->name;
}

/* cls is base or a subclass of it */
int SceSysmemForKernel_7F7D5C4A(SceClass *cls, SceClass *base)
{
	if (cls == base)
		return 1;
	while ((cls = cls->uidclass) != NULL) {
		if (base == cls)
			return 1;
	}
	return 0;
}

/* a GUID for an object the caller made */
int GUIDRegister(SceClass *cls, const char *name, SceObjectBase *object, SceUInt32 attr)
{
	UIDEntryHeap *heap;

	if ((SceInt32)attr < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if ((attr & UID_ENTRY_ATTR_NAME_KIND) && name != NULL &&
	    strnlen(name, NAME_ENTRY_SHORT_NAME) >= NAME_ENTRY_SHORT_NAME)
		return UID_ERROR_NAME_TOO_LONG;
	heap = GUID_ENTRY_HEAP();
	object->sce_class = cls;
	return UIDEntryHeapAllocateGUIDEntryObject(heap, cls, name, object,
						   SceSysrootForKernel_7385CADE(), s_registerHeap,
						   attr);
}

static inline int UIDEntrySetCNOA(UIDEntry *entry, SceClass *cls, const char *name,
				  SceObjectBase *object, SceUInt32 attr)
{
	SceClass *memblockClass;
	int ret;

	if (name != NULL) {
		ret = UIDEntrySetNameWithNameHeapRaw(entry, name);
		if (ret < 0) {
			SCE_DBG_PRINTF(0xb, 2438, "UIDEntrySetNameWithNameHeapRaw([%s]) failed 0x%08x\n",
				       name, ret);
			entry->name = NULL;
		}
	} else {
		SCE_DBG_PRINTF(0xb, 2442, "name is NULL\n");
		entry->name = NULL;
		ret = 0;
	}
	if (object == NULL) {
		if (entry->object == NULL) {
			SCE_DBG_PRINTF(0xb, 2464, "pObject is SCE_NULL\n");
			SCE_DBG_ASSERT_FAIL(2465);
		}
	} else {
		entry->object = object;
		if (cls != NULL) {
			object->sce_class = cls;
			memblockClass = ksceKernelGetUIDMemBlockClass();
			if (SceSysmemForKernel_7F7D5C4A(cls, memblockClass)) {
				MemBlockObject *block = (MemBlockObject *)object;
				const char *blockName;
#if FW_DEBUG
				int assertLevel = ksceKernelGetAssertLevel();

				blockName = entry->name;
#pragma push_macro("__FILE_NAME__")
#undef __FILE_NAME__
#define __FILE_NAME__ "memblock_obj_module.h"
				if (assertLevel > 0)
					ksceKernelAssertLevel(1, blockName != NULL,
						SCE_DBGINFO_FUNC(653, "MemBlockSetNameRaw"),
						__builtin_return_address(0));
#pragma pop_macro("__FILE_NAME__")
#else
				blockName = entry->name;
#endif
				block->name = blockName;
				block->flags |= MEMBLOCK_FLAG_NAMED;
			}
		}
	}
	entry->attr = attr;
	return ret;
}

/* class, name, object and attributes of a GUID made before the UID manager */
int GUIDSetCNOA(SceUID guid, void *cls, const char *name, void *object, SceUInt32 attr)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	int state, ret;

	state = UID_WRITE_LOCK(heap, 5375);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0) {
		if (attr & UID_ENTRY_ATTR_PERMANENT)
			ret = UIDEntrySetCNOA(entry, cls, name, object, attr);
		else
			ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

int GUIDFindByName(const char *name)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	int state, ret;

	state = UID_READ_LOCK(heap, 5426);
	ret = GUIDEntryHeapFindByNameRaw(heap, name, 0, &entry);
	UID_READ_UNLOCK(heap, state);
	if (ret >= 0)
		ret = entry->uid;
	return ret;
}

/* one user less of a GUID */
/* one use less: the last use of an entry closed meanwhile closes it */
static inline int UIDEntryReleaseObjectWithGUID(UIDEntryHeap *heap, UIDEntry **pEntry,
						SceUID guid, DeleteParam *param)
{
	UIDEntry *entry = *pEntry;
	UIDEntry *base;
	int ret;

	if (entry->attr & UID_ENTRY_ATTR_GUID_ANY) {
		if (entry->useCount == 0) {
			SCE_DBG_ASSERT_FAIL(2813);
			return UID_ERROR_INVALID_UID;
		}
		entry->useCount--;
		if (!UIDEntryIsUnused(entry))
			return 0;
		base = NULL;
	} else if (entry->attr & UID_ENTRY_ATTR_LINK_ANY) {
		if (entry->useCount == 0) {
			SCE_DBG_ASSERT_FAIL(2828);
			return UID_ERROR_INVALID_UID;
		}
		entry->useCount--;
		if (!UIDEntryIsUnused(entry))
			return 0;
		base = (entry->attr & UID_ENTRY_ATTR_PUID) ? NULL : entry->base;
	} else {
		return UID_ERROR_INVALID_UID;
	}
	SCE_DBG_ASSERT_PRINTF(guid - 0x10001U > 0x5e, 2849,
			      "error: GUIDReleaseObject(guid=0x%08X), pEntry->ucount=0x%08X\n", guid,
			      entry->useCount);
	ret = UIDEntryClose(heap, *pEntry, param);
	if (ret < 0)
		return ret;
	if (base != NULL)
		ret = UIDEntryCloseLink(heap, base, param);
	return ret;
}

int GUIDReleaseObject(SceUID guid)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	DeleteParam param = { 0 };
	UIDEntry *entry;
	int state, ret;

	state = UID_WRITE_LOCK(heap, 5771);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0)
		ret = UIDEntryReleaseObjectWithGUID(heap, &entry, guid, &param);
	UID_WRITE_UNLOCK(heap, state);
	if (param.nObject > DELETE_PARAM_OBJECTS) {
		SCE_DBG_PRINTF(0xb, 5810, "nObject=%d[0x%08x]/%d, guid=0x%08x\n", param.nObject,
			       param.nObject, DELETE_PARAM_OBJECTS, guid);
		SCE_DBG_ASSERT_FAIL(5811);
	}
	DeleteParamClean(&param);
	SCE_DBG_ASSERT_PRINTF(ret >= 0, 5820,
			      "*** fatal error: GUIDReleaseObject(0x%08x), res=0x%08x\n", guid, ret);
	return ret;
}

/* attr: a visibility level, else 3; UID_ENTRY_ATTR_NAME_PUBLIC names go to table 0 */
static inline SceUInt32 UIDCreateAttr(SceUInt32 attr)
{
	if (attr & UID_ENTRY_ATTR_NAME_PUBLIC) {
		if (!(attr & UID_ENTRY_ATTR_NAME_KIND))
			attr |= UID_ENTRY_ATTR_NAME_KIND;
	}
	return attr;
}

int ksceGUIDKernelCreateWithAttr(SceClass *pClass, const char *name, SceUInt32 attr,
				 SceObjectBase **ppEntry)
{
	void *heap;
	SceUInt32 type;

	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!(attr & UID_ENTRY_ATTR_LEVEL_MASK))
		attr |= UID_ENTRY_ATTR_LEVEL_DEFAULT;
	if (ppEntry == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	attr = UIDCreateAttr(attr);
	heap = UIDGetKernelHeapForSize(pClass->itemsize_aligned);
	type = pClass->defaultObjectType;
	return GUIDCreate(pClass, name, SceSysrootForKernel_7385CADE(), heap, NULL,
			  (attr & ~UID_ENTRY_ATTR_TYPE_MASK) |
			  ((type << UID_ENTRY_ATTR_TYPE_SHIFT) & UID_ENTRY_ATTR_TYPE_MASK), ppEntry);
}

SceUID ksceGUIDKernelCreate(SceClass *cls, const char *name, SceObjectBase **obj)
{
	return ksceGUIDKernelCreateWithAttr(cls, name, UID_ENTRY_ATTR_LEVEL_DEFAULT, obj);
}

/* sceGUIDRegister */
SceUID SceSysmemForKernel_6DB44A42(SceClass *cls, const char *name, void *object)
{
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return GUIDRegister(cls, name, object, UID_ENTRY_ATTR_LEVEL_DEFAULT);
}

/* name unknown */
int GUIDUnregisterChecked(SceUID guid)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	return GUIDUnregister(guid);
}

/* sceGUIDSet */
int SceSysmemForKernel_BF275C5A(SceUID guid, SceClass *cls, const char *name,
				SceObjectBase *object)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	return GUIDSet(guid, cls, name, object);
}

/* sceGUIDSetCNOA */
int SceSysmemForKernel_5EDDEEB5(SceUID guid, SceClass *cls, const char *name,
				SceObjectBase *object, SceUInt32 attr)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	return GUIDSetCNOA(guid, cls, name, object, attr);
}

/* links newEntry to the GUID entry it opens */
static inline int UIDEntryIncLinkCountForOpen(UIDEntryHeap *heap, UIDEntry *entry, UIDEntry *newEntry,
					      SceUID guid)
{
	int ret;

	if (entry->object == NULL)
		SCE_DBG_PRINTF(0xb, 2048, "Warning:pEntry[%p]->pObject is SCE_NULL\n", entry);
	if (!(entry->attr & UID_ENTRY_ATTR_ACTIVE) || guid != entry->uid) {
		ret = UID_ERROR_INVALID_UID;
		SCE_DBG_PRINTF(0xb, 2053, "guid 0x%08x is not active\n", guid);
		return ret;
	}
	if (UIDEntryIncLinkCountRaw(entry) != 1) {
		SCE_DBG_PRINTF(0xb, 2060, "UIDEntryIncLinkCountRaw failed\n");
		return UID_ERROR_LINK_COUNT;
	}
	ret = UIDEntryConstructByBase(heap, newEntry, SceSysrootForKernel_7385CADE(), entry);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 2071, "UIDEntryHeapSetGUIDEntryChainRaw failed 0x%08x\n", ret);
	return ret;
}

/* a new link to a GUID: another GUID for the same object */
static inline int GUIDOpenByGUID(SceUID guid)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *base, *entry = NULL;
	int state, ret;

	ret = UIDEntryHeapAllocateObject(heap, &entry);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 4940, "UIDEntryHeapAllocateObject failed 0x%08x\n", ret);
		return ret;
	}
	state = UID_WRITE_LOCK(heap, 4944);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &base);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 4949, "UIDEntryHeapGetByUIDRaw(guid 0x%08x) failed 0x%08x\n", guid,
			       ret);
		ret = UID_ERROR_INVALID_UID;
	} else {
		ret = UIDEntryIncLinkCountForOpen(heap, base, entry, guid);
		if (ret >= 0) {
			UID_WRITE_UNLOCK(heap, state);
			return ret;
		}
		SCE_DBG_PRINTF(0xb, 4956, "UIDEntryIncLinkCountForOpen failed\n");
	}
	UID_WRITE_UNLOCK(heap, state);
	SCE_DBG_PRINTF(0xb, 4985, "guidNew=0x%08x\n", ret);
	if (entry != NULL) {
		UIDEntryHeapFreeObject(heap, entry);
	}
	return ret;
}

SceUID ksceGUIDOpenByGUID(SceUID guid)
{
	if (!UIDIsGUID(guid)) {
		SCE_DBG_PRINTF_FUNC(0xb, 6278, "sceGUIDOpenByGUID",
				    "GUIDIsValid(0x%08x) failed 0x%08x\n", guid, UID_ERROR_INVALID_UID);
		return UID_ERROR_INVALID_UID;
	}
	return GUIDOpenByGUID(guid);
}

/* a new link to the GUID named name */
/* links newEntry to the named GUID entry it opens */
static inline int UIDEntryIncLinkCountForOpenByName(UIDEntryHeap *heap, UIDEntry *entry,
						    UIDEntry *newEntry)
{
	int ret;

	if (entry->object == NULL)
		SCE_DBG_PRINTF(0xb, 2094, "Warning:pEntry[%p]->pObject is SCE_NULL\n", entry);
	if (!(entry->attr & UID_ENTRY_ATTR_ALL))
		return UID_ERROR_INVALID_UID;
	if (UIDEntryIncLinkCountRaw(entry) != 1) {
		ret = UID_ERROR_LINK_COUNT;
		SCE_DBG_PRINTF(0xb, 2106, "UIDEntryIncLinkCountRaw failed\n");
		return ret;
	}
	ret = UIDEntryConstructByBase(heap, newEntry, SceSysrootForKernel_7385CADE(), entry);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 2116, "UIDEntryHeapSetGUIDEntryChainRaw failed 0x%08x\n", ret);
	return ret;
}

SceUID ksceGUIDOpenByName(const char *name)
{
	UIDEntryHeap *heap;
	UIDEntry *base, *entry;
	int state, ret;

	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	heap = GUID_ENTRY_HEAP();
	ret = UIDEntryHeapAllocateObject(heap, &entry);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 5010, "GUIDOpenByName",
				    "UIDEntryHeapAllocateObject failed 0x%08x\n", ret);
		return ret;
	}
	state = FixedHeapWriteLock(&heap->fixedHeap, "GUIDOpenByName", 5014);
	ret = GUIDEntryHeapFindByNameRaw(heap, name, 0, &base);
	if (ret < 0) {
		SCE_DBG_PRINTF_FUNC(0xb, 5018, "GUIDOpenByName",
				    "GUIDEntryHeapFindByNameRaw([%s]) failed 0x%08x\n", name,
				    ret);
	} else {
		ret = UIDEntryIncLinkCountForOpenByName(heap, base, entry);
		if (ret >= 0) {
			UID_WRITE_UNLOCK(heap, state);
			return ret;
		}
		SCE_DBG_PRINTF_FUNC(0xb, 5027, "GUIDOpenByName",
				    "UIDEntryIncLinkCountForOpenByName failed 0x%08x\n", ret);
	}
	UID_WRITE_UNLOCK(heap, state);
	SCE_DBG_PRINTF_FUNC(0xb, 5034, "GUIDOpenByName", "guid=0x%08x\n", ret);
	UIDEntryHeapFreeObject(heap, entry);
	return ret;
}

/* one link less: the last one closes the GUID */
static inline int UIDEntryCloseAPI(UIDEntryHeap *heap, UIDEntry **pEntry, DeleteParam *param)
{
	UIDEntry *entry = *pEntry;
	SceUInt32 attr;
	UIDEntry *base;
	int ret;

	if (entry->object == NULL)
		SCE_DBG_PRINTF(0xb, 2212, "Warning:pEntry[%p]->pObject is SCE_NULL\n", entry);
	attr = entry->attr;
	if (attr & UID_ENTRY_ATTR_GUID) {
		if (entry->linkCount == 0)
			return UID_ERROR_INVALID_UID;
		entry->linkCount--;
		if (!UIDEntryIsUnused(entry)) {
			entry->attr = (attr & ~UID_ENTRY_ATTR_ALL) | UID_ENTRY_ATTR_GUID_CLOSED;
			return 0;
		}
		return UIDEntryClose(heap, *pEntry, param);
	}
	if (!(attr & UID_ENTRY_ATTR_LINK) || entry->linkCount == 0)
		return UID_ERROR_INVALID_UID;
	entry->linkCount--;
	if (!UIDEntryIsUnused(entry)) {
		entry->attr = (attr & ~UID_ENTRY_ATTR_ALL) | UID_ENTRY_ATTR_LINK_CLOSED;
		return 0;
	}
	if ((attr & UID_ENTRY_ATTR_PUID) || !(attr & UID_ENTRY_ATTR_LINK_ANY))
		return UIDEntryClose(heap, *pEntry, param);
	base = entry->base;
	ret = UIDEntryClose(heap, *pEntry, param);
	if (ret < 0)
		return ret;
	if (base != NULL)
		ret = UIDEntryCloseLink(heap, base, param);
	return ret;
}

static inline int GUIDClose(SceUID guid)
{
	UIDEntryHeap *heap;
	DeleteParam param;
	UIDEntry *entry;
	int state, ret;

	SCE_DBG_ASSERT_PRINTF(guid - 0x10001U > 0x5e, 5118, "error: GUIDClose(guid=0x%08X)\n",
			      guid);
	param = (DeleteParam){0};
	heap = GUID_ENTRY_HEAP();
	state = FixedHeapWriteLock(&heap->fixedHeap, "GUIDClose", 5130);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0)
		ret = UIDEntryCloseAPI(heap, &entry, &param);
	UID_WRITE_UNLOCK(heap, state);
	DeleteParamClean(&param);
	return ret;
}

int ksceGUIDClose(SceUID guid)
{
	if (!UIDIsGUID(guid)) {
		SCE_DBG_PRINTF_FUNC(0xb, 6301, "sceGUIDClose",
				    "GUIDIsValid(0x%08x) failed 0x%08x\n",
				    guid, UID_ERROR_INVALID_UID);
		return UID_ERROR_INVALID_UID;
	}
	return GUIDClose(guid);
}

int ksceGUIDGetObjectWithClass(SceUID uid, SceClass *pClass, SceObjectBase **obj)
{
	if (!UIDIsGUID(uid))
		return UID_ERROR_INVALID_UID;
	return GUIDGetObjectWithClass(uid, pClass, obj);
}

int ksceGUIDGetObject(SceUID guid, void *object)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	if (object == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return GUIDGetObjectWithClass(guid, NULL, object);
}

/* sceUIDEntryHeapCreate: the PUID entry heap of a process, in the heap heapGuid */
SceUID SceSysmemForKernel_89BEF058(SceUID pid, SceUID heapGuid)
{
	void *addressSpace, *heap, *partition;
	UIDEntryHeap *entryHeap;
	int ret;

	ret = SysrootPIDtoAddressSpaceCB(pid, &addressSpace);
	if (ret < 0)
		return ret;
	ret = ksceGUIDGetObject(heapGuid, &heap);
	if (ret < 0)
		return ret;
	entryHeap = HeapAllocLocked(heap, sizeof(*entryHeap));
	entryHeap->fixedHeap.uidHeap.cls = s_entryHeapClass;
	ret = AddressSpaceGetDefaultPhyMemPart(addressSpace, 0, &partition);
	if (ret >= 0) {
		entryHeap->fixedHeap.hook = NULL;
		entryHeap->fixedHeap.heap = heap;
		ret = FixedHeapInitForProcess(&entryHeap->fixedHeap, pid, "UID",
					      FIXED_HEAP_ATTR_UID_ENTRIES, sizeof(UIDEntry), partition,
					      0x1000, 0x40);
		if (ret >= 0) {
			entryHeap->reserved = -1;
			FixedHeapInitLock(&entryHeap->fixedHeap);
			entryHeap->pid = pid;
			entryHeap->heap = heap;
			return GUIDRegister(&s_entryHeapClass->heapClass.cls, "SceUIDEntryHeap",
					    (SceObjectBase *)entryHeap,
					    UID_ENTRY_ATTR_LEVEL_DEFAULT);
		}
	}
	HeapFreeLocked(heap, entryHeap);
	return ret;
}

/* sceUIDEntryHeapCloseAllPUID */
int SceSysmemForKernel_133024A5(SceUID guid)
{
	UIDEntryHeap *heap = NULL;
	UIDEntryHeap *entryHeap;
	UIDEntryHeap *walk;
	int state, ret;

	ret = ksceGUIDGetObject(guid, &heap);
	if (ret < 0)
		return ret;
	entryHeap = heap;
	walk = heap;
	state = FixedHeapWriteLock(&entryHeap->fixedHeap, "UIDEntryHeapCloseAllPUID", 3820);
	FixedHeapWalk(&entryHeap->fixedHeap, UIDEntryIsActiveForCloseAll, &walk);
	UID_WRITE_UNLOCK(entryHeap, state);
	return 0;
}

/* sceUIDEntryHeapDelete */
int SceSysmemForKernel_F6D09BD9(SceUID guid)
{
	UIDEntryHeap *heap = NULL;
	void *kernelHeap;
	int ret;

	ret = ksceGUIDGetObject(guid, &heap);
	if (ret < 0)
		return ret;
	ret = GUIDUnregister(guid);
	if (ret < 0)
		return ret;
	FixedHeapFinish(&heap->fixedHeap);
	kernelHeap = heap->heap;
	if (kernelHeap == NULL)
		kernelHeap = UIDManagerGetKernelHeap();
	if (heap->fixedHeap.pages != NULL) {
		HeapFreeLocked(kernelHeap, heap->fixedHeap.pages);
		heap->fixedHeap.pages = NULL;
	}
	if (heap->fixedHeap.blocks != NULL) {
		HeapFreeLocked(kernelHeap, heap->fixedHeap.blocks);
		heap->fixedHeap.blocks = NULL;
	}
	HeapFreeLocked(kernelHeap, heap);
	return 0;
}

SceInt32 ksceKernelUIDEntryHeapGetInfo(SceUID entryHeapId, void *pInfo)
{
	SceKernelUIDEntryHeapInfo *info = pInfo;
	SceKernelFixedHeapInfo heapInfo;
	UIDEntryHeap *heap = NULL;
	int state, ret;

	if (entryHeapId == SCE_KERNEL_GUID_ENTRY_HEAP_ID) {
		heap = GUID_ENTRY_HEAP();
	} else {
		ret = ksceGUIDGetObject(entryHeapId, &heap);
		if (ret < 0)
			return ret;
	}
	if (info == NULL || info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	state = FixedHeapReadLock(&heap->fixedHeap, "sceUIDEntryHeapGetInfo", 4101);
	heapInfo.size = sizeof(heapInfo);
	ret = FixedHeapGetInfo(&heap->fixedHeap, &heapInfo);
	if (ret >= 0) {
		info->itemSize = heapInfo.itemSize;
		info->totalSize = heapInfo.totalSize;
		info->perPage = heapInfo.perPage;
		info->freeCount = heapInfo.freeCount;
		info->peakCount = heapInfo.peakCount;
	}
	UID_READ_UNLOCK(heap, state);
	return ret;
}

SceUID ksceGUIDKernelCreateWithOpt(SceClass *sce_class, const char *name,
				   SceGUIDKernelCreateOpt *createOpt, SceObjectBase **obj)
{
	const UIDCreateOpt *opt = (const UIDCreateOpt *)createOpt;
	SceUID pid = SceSysrootForKernel_7385CADE();
	SceObjectBase *object = NULL;
	void *heap, *processHeap;
	SceUInt32 type, attr, flags;

	if (name == NULL)
		return SCE_KERNEL_ERROR_ILLEGAL_ADDR;
	type = sce_class->defaultObjectType;
	if (opt == NULL) {
		heap = UIDGetKernelHeapForSize(sce_class->itemsize_aligned);
		attr = UID_ENTRY_ATTR_LEVEL_DEFAULT;
		goto create;
	}
	flags = opt->flags;
	if (flags & UID_CREATE_OPT_CLASS) {
		SCE_DBG_PRINTF_FUNC(0xb, 6106, "sceGUIDKernelCreateWithOpt", "Duplicate pClass\n");
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	if (flags & UID_CREATE_OPT_OBJECT) {
		heap = NULL;
		object = opt->object;
	} else if (flags & UID_CREATE_OPT_HEAP_GUID) {
		if (ksceGUIDGetObject(opt->heapGuid, &heap) < 0)
			heap = NULL;
		flags = opt->flags;
	} else if (flags & UID_CREATE_OPT_PROCESS_HEAP) {
		processHeap = SysrootGetProcessObjectHeap(opt->pid, opt->heapKind);
		if (processHeap == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		heap = ObjectHeapGetHeap(processHeap, sce_class->itemsize_aligned);
		if (heap == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
		flags = opt->flags;
	} else {
		heap = UIDGetKernelHeapForSize(sce_class->itemsize_aligned);
	}
	if (flags & UID_CREATE_OPT_TYPE)
		type = opt->type;
	attr = (flags & UID_CREATE_OPT_ATTR) ? opt->attr : UID_ENTRY_ATTR_LEVEL_DEFAULT;
	if (flags & UID_CREATE_OPT_OWNER)
		pid = opt->ownerPid;
create:
	if (obj == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!(attr & UID_ENTRY_ATTR_LEVEL_MASK))
		attr |= UID_ENTRY_ATTR_LEVEL_DEFAULT;
	attr = UIDCreateAttr(attr);
	return GUIDCreate(sce_class, name, pid, heap, object,
			  (attr & ~UID_ENTRY_ATTR_TYPE_MASK) |
			  (type << UID_ENTRY_ATTR_TYPE_SHIFT & UID_ENTRY_ATTR_TYPE_MASK), obj);
}

/* an object of cls from the heap of heapGuid (the kernel heap if none) */
SceUID ksceGUIDCreate(SceClass *cls, const char *name, SceUID heapGuid, SceObjectBase **obj)
{
	SceUInt32 attr;
	SceUID pid;
	void *heap;

	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (obj == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	attr = ((cls->defaultObjectType << UID_ENTRY_ATTR_TYPE_SHIFT) & UID_ENTRY_ATTR_TYPE_MASK) |
	       UID_ENTRY_ATTR_LEVEL_DEFAULT;
	pid = SceSysrootForKernel_7385CADE();
	return GUIDCreate(cls, name, pid, ksceGUIDGetObject(heapGuid, &heap) < 0 ? NULL : heap, NULL,
			  attr, obj);
}

static inline int UIDEntryGetClassWithGUID(const UIDEntry *entry, SceUID guid, SceClass **cls)
{
	if (UIDEntryIsValidWithUIDRaw(entry, guid) != 1) {
		SCE_DBG_PRINTF(0xb, 2287, "guid 0x%08x is invalid\n", guid);
		*cls = NULL;
		return UID_ERROR_INVALID_UID;
	}
	if (entry->object == NULL) {
		*cls = NULL;
		return UID_ERROR_INVALID_UID;
	}
	*cls = entry->object->sce_class;
	return 0;
}

/* sceGUIDGetClass */
int ksceGUIDGetClass(SceUID guid, SceClass **cls)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	heap = GUID_ENTRY_HEAP();
	state = FixedHeapReadLock(&heap->fixedHeap, "GUIDGetClass", 5228);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0)
		ret = UIDEntryGetClassWithGUID(entry, guid, cls);
	UID_READ_UNLOCK(heap, state);
	return ret;
}

/* sceUIDtoObject */
void *SceSysmemForKernel_FA6C098C(SceUID uid)
{
	void *object;

	if (ksceGUIDGetObject(uid, &object) < 0)
		object = NULL;
	return object;
}

/* sceUIDtoClass */
SceClass *SceSysmemForKernel_368F1C36(SceUID uid)
{
	SceClass *cls;

	if (ksceGUIDGetClass(uid, &cls) < 0)
		cls = NULL;
	return cls;
}

const char *ksceGUIDName(SceUID guid)
{
	const char *name;

	if (GUIDGetName(guid, &name) < 0)
		name = NULL;
	return name;
}

/* sceGUIDGetVisibilityLevel */
int SceSysmemForKernel_4E923101(SceUID guid)
{
	return GUIDGetVisibilityLevel(guid);
}

/* sceGUIDSetVisibilityLevel */
int SceSysmemForKernel_E8CC2166(SceUID guid, SceUInt32 level)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntry *entry;
	int state, ret;

	state = FixedHeapWriteLock(&heap->fixedHeap, "GUIDSetVisibilityLevel", 5348);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0) {
		UIDEntry *validEntry = entry;

		if (UIDEntryIsValidWithUIDRaw(validEntry, guid) == 1)
			validEntry->attr = (level << UID_ENTRY_ATTR_LEVEL_SHIFT &
					    UID_ENTRY_ATTR_LEVEL_MASK) |
					   (validEntry->attr & ~UID_ENTRY_ATTR_LEVEL_MASK);
	}
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

int ksceGUIDGetName(SceUID guid, const char **name)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return GUIDGetName(guid, name);
}

SceUID sceGUIDGetPID(SceUID guid)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	SceUID pid;
	int state;

	if (!UIDIsGUID(guid)) {
		SCE_DBG_PRINTF(0xb, 6445, "GUIDIsValid(0x%08x) returns 0x%08x\n", guid,
			       UID_ERROR_INVALID_UID);
		return UID_ERROR_INVALID_UID;
	}
	heap = GUID_ENTRY_HEAP();
	state = FixedHeapReadLock(&heap->fixedHeap, "GUIDGetPID", 5276);
	pid = UID_ERROR_INVALID_UID;
	if (UIDEntryCheckInvalid(heap, guid, 0, &entry) >= 0) {
		UIDEntry *validEntry = entry;

		if (UIDEntryIsValidWithUIDRaw(validEntry, guid) == 1)
			pid = validEntry->pid;
	}
	UID_READ_UNLOCK(heap, state);
	return pid;
}

/* a GUID of the kernel (or of none) given to a process */
int sceGUIDSetPID(SceUID guid, SceUID pid)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (!UIDIsGUID(guid)) {
		SCE_DBG_PRINTF(0xb, 6460, "GUIDIsValid(0x%08x) returns 0x%08x\n", guid,
			       UID_ERROR_INVALID_UID);
		return UID_ERROR_INVALID_UID;
	}
	heap = GUID_ENTRY_HEAP();
	state = FixedHeapWriteLock(&heap->fixedHeap, "GUIDSetPID", 5300);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0) {
		UIDEntry *validEntry = entry;

		if (UIDEntryIsValidWithUIDRaw(validEntry, guid) == 1) {
			SceUID ownerPid = validEntry->pid;

			if (ownerPid == SCE_KERNEL_PROCESS_ID || (SceUInt32)ownerPid + 1U == 0)
				validEntry->pid = pid;
		}
	}
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

int ksceGUIDSetName(SceUID guid, const char *name)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return GUIDSet(guid, NULL, name, NULL);
}

/* name unknown */
int GUIDApplyWithClass(SceUID guid, SceClass *cls, int (*func)(SceObjectBase *object, void *argument),
		       GUIDApplyArgs *args)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	heap = GUID_ENTRY_HEAP();
	state = UID_WRITE_LOCK(heap, 4909);
	ret = UIDEntryCheckInvalid(heap, guid, 0, &entry);
	if (ret >= 0) {
		if (cls == entry->object->sce_class) {
			args->result = func(entry->object, args->argument);
			ret = 0;
		} else {
			ret = UID_ERROR_CLASS;
		}
	}
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

int ksceGUIDReferObject(SceUID guid, SceObjectBase **object)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	return GUIDReferObjectClassCheckCommon(guid, 0, NULL, 7, 0, object);
}

int ksceGUIDReferObjectWithClass(SceUID guid, SceClass *sce_class, SceObjectBase **object)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	return GUIDReferObjectClassCheckCommon(guid, 1, sce_class, 7, 0, object);
}

/* name unknown: only while no one else refers to it */
int GUIDReferObjectWithClassExclusive(SceUID guid, SceClass *cls, SceObjectBase **object)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	return GUIDReferObjectClassCheckCommon(guid, 1, cls, 7, 1, object);
}

int ksceGUIDReferObjectWithLevel(SceUID guid, SceUInt32 level, SceObjectBase **object)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	if (level >= 8)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return GUIDReferObjectClassCheckCommon(guid, 0, NULL, level, 0, object);
}

int ksceGUIDReferObjectWithClassLevel(SceUID guid, SceClass *pClass, SceUInt32 level,
				      SceObjectBase **entry)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	if (level >= 8)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return GUIDReferObjectClassCheckCommon(guid, 1, pClass, level, 0, entry);
}

int ksceGUIDReferObjectWithSubclass(SceUID guid, SceClass *cls, SceObjectBase **object)
{
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	return GUIDReferObjectClassCheckCommon(guid, 2, cls, 7, 0, object);
}

int ksceGUIDReleaseObject(SceUID guid)
{
	int ret;

	if (!UIDIsGUID(guid)) {
		SCE_DBG_PRINTF_FUNC(0xb, 6627, "sceGUIDReleaseObject",
				    "GUIDIsValid(0x%08x) failed 0x%08x\n", guid, UID_ERROR_INVALID_UID);
		return UID_ERROR_INVALID_UID;
	}
	ret = GUIDReleaseObject(guid);
	if (ret < 0)
		SCE_DBG_ASSERT_CHECK_FUNC(0, 6632, "sceGUIDReleaseObject");
	return ret;
}

/* sceGUIDFindByName */
SceUID SceSysmemForKernel_BDE00106(const char *name)
{
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return GUIDFindByName(name);
}

/* sceGUIDFindByNameAll: by its name in the name heap, else by any name */
SceUID SceSysmemForKernel_F49C0BE8(const char *name)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	heap = GUID_ENTRY_HEAP();
	state = FixedHeapReadLock(&heap->fixedHeap, "GUIDFindByNameAll", 5456);
	ret = GUIDEntryHeapFindByNameRaw(heap, name, 0, &entry);
	UID_READ_UNLOCK(heap, state);
	if (ret < 0) {
		state = FixedHeapReadLock(&heap->fixedHeap, "GUIDFindByNameAll", 5466);
		ret = GUIDEntryHeapFindByNameRaw(heap, name, 1, &entry);
		UID_READ_UNLOCK(heap, state);
		if (ret < 0)
			return ret;
	}
	return entry->uid;
}

/* sceGUIDFindByObject */
SceUID SceSysmemForKernel_EC90AA11(SceObjectBase *object)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDEntryFind find;
	UIDEntry *entry;
	int state, ret;

	state = FixedHeapReadLock(&heap->fixedHeap, "GUIDFindByObject", 5498);
	find.key = (SceUInt32)object;
	ret = FixedHeapWalk(&heap->fixedHeap, UIDEntryFindByObject, &find);
	if (ret == 0) {
		entry = find.entry;
		if (entry == NULL) {
			ret = UID_ERROR_INVALID_UID;
			goto out;
		}
		ret = find.index;
	} else {
		entry = NULL;
	}
	if (ret >= 0)
		ret = entry->uid;
out:
	UID_READ_UNLOCK(heap, state);
	return ret;
}

int ksceGUIDGetUIDVectorByClass(SceClass *cls, int vis_level, SceUID *vector, SceSize num,
				SceSize *ret_num)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDVectorByClass walk;
	int state, total;

	state = FixedHeapReadLock(&heap->fixedHeap, "GUIDGetUIDVectorByClass", 5889);
	walk.heap = heap;
	walk.cls = cls;
	walk.level = vis_level;
	walk.size = num;
	walk.uids = vector;
	walk.count = 0;
	walk.total = 0;
	FixedHeapWalk(&heap->fixedHeap, UIDEntryCheckClass, &walk);
	if (ret_num != NULL)
		*ret_num = walk.count;
	total = walk.total;
	UID_READ_UNLOCK(heap, state);
	return total;
}

/* sceGUIDGetUIDVectorWeakByClass: sixteen entries at a time */
int SceSysmemForKernel_E9957329(SceClass *cls, int level, SceUID *vector, SceSize num,
				SceSize *ret_num)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	UIDVectorByClass walk;
	SceUInt32 start = 0, end;
	int state, ret;

	walk.heap = heap;
	walk.cls = cls;
	walk.level = level;
	walk.size = num;
	walk.uids = vector;
	walk.count = 0;
	walk.total = 0;
	do {
		state = FixedHeapReadLock(&heap->fixedHeap, "GUIDGetUIDVectorWeakByClass", 5854);
		ret = FixedHeapApplyRangeWithIndexArgRaw(&walk.heap->fixedHeap, start, 0x10,
							 UIDEntryCheckClass, &walk, &end);
		UID_READ_UNLOCK(heap, state);
		if (ret < 0)
			break;
		start += 0x10;
	} while (start <= end);
	if (ret_num != NULL)
		*ret_num = walk.count;
	return walk.total;
}

/* sceGUIDGetUIDVectorByPID */
int SceSysmemForKernel_66489B4B(SceUID pid, SceUInt32 level, SceUID *vector, SceSize size,
				SceSize *ret_num)
{
	UIDFuncVector walk;
	int ret;

	if (GUID_ENTRY_HEAP() == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	walk.match = UIDFuncMatchPID;
	walk.add = UIDFuncAddUID;
	walk.count = 0;
	walk.total = 0;
	walk.heap = GUID_ENTRY_HEAP();
	walk.level = level;
	walk.size = size;
	walk.buffer = vector;
	walk.key = pid;
	ret = UIDFuncVectorGetUIDVectorWeak(&walk);
	if (ret_num != NULL)
		*ret_num = ret < 0 ? 0 : walk.count;
	return ret;
}

/* sceGUIDGetEntryInfoAll: state bit 0 the active entries, bit 1 the closed ones */
int SceSysmemForKernel_3BF0DE65(SceUInt32 level, SceUInt32 state, SceKernelUIDEntryInfo *infos,
				SceSize size, SceSize *ret_num)
{
	UIDFuncVector walk;
	UIDEntryHeap *heap;
	int ret;

	if (infos == NULL) {
		if (size != 0)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	heap = GUID_ENTRY_HEAP();
	if (heap == NULL)
		return SCE_KERNEL_ERROR_ERROR;
	memset(&walk, 0, sizeof(walk));
	walk.match = UIDFuncMatchState;
	walk.add = UIDFuncAddEntryInfo;
	walk.heap = heap;
	walk.level = level;
	walk.size = size;
	walk.buffer = infos;
	walk.key = state;
	ret = UIDFuncVectorGetUIDVectorWeak(&walk);
	if (ret_num != NULL)
		*ret_num = ret < 0 ? 0 : walk.count;
	return ret;
}

int sceGUIDGetEntryInfoClass(SceClass *cls, SceUInt32 level, SceUInt32 reserved,
			     SceKernelUIDEntryInfo *infos, int size, SceSize *ret_num)
{
	UIDEntryHeap *heap;
	UIDEntryInfoWalk walk;
	int state, total;

	SCE_DBG_PRINTF(0xb, 6796, "()\n");
	if (size < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (size != 0) {
		if (infos == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	heap = GUID_ENTRY_HEAP();
	state = FixedHeapReadLock(&heap->fixedHeap, "GUIDGetEntryInfoClass", 5912);
	walk.size = size;
	walk.count = 0;
	walk.total = 0;
	walk.cls = cls;
	walk.level = level;
	walk.reserved = reserved;
	walk.infos = infos;
	FixedHeapWalk(&heap->fixedHeap, UIDEntryFuncForGetInfo, &walk);
	if (ret_num != NULL)
		*ret_num = walk.count;
	total = walk.total;
	UID_READ_UNLOCK(heap, state);
	return total;
}

/* sceGUIDGetEntry */
int SceSysmemForKernel_FF464A49(SceUID guid, UIDEntry **out)
{
	UIDEntryHeap *heap = GUID_ENTRY_HEAP();
	int state, ret;

	state = FixedHeapReadLock(&heap->fixedHeap, "sceGUIDGetEntry", 6831);
	ret = UIDEntryCheckInvalid(heap, guid, 0, out);
	if (ret >= 0) {
		if (!((*out)->attr & UID_ENTRY_ATTR_ACTIVE))
			ret = UID_ERROR_INVALID_UID;
		else if (guid != (*out)->uid)
			ret = UID_ERROR_INVALID_UID;
	}
	UID_READ_UNLOCK(heap, state);
	return ret;
}

int PUIDGetEntryInfoAll(SceUID pid, SceUInt32 level, SceUInt32 reserved, SceKernelUIDEntryInfo *infos,
			int size, SceSize *ret_num)
{
	UIDEntryHeap *heap;
	UIDEntryInfoWalk walk;
	int state, ret;

	if (size < 0)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (size != 0) {
		if (infos == NULL)
			return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	}
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 6906, "GetPUIDEntryHeap failed 0x%08x\n", ret);
		return ret;
	}
	state = UID_READ_LOCK(heap, 6910);
	walk.size = size;
	walk.cls = NULL;
	walk.count = 0;
	walk.total = 0;
	walk.level = level;
	walk.reserved = reserved;
	walk.infos = infos;
	FixedHeapWalk(&heap->fixedHeap, UIDEntryFuncForGetInfo, &walk);
	if (ret_num != NULL)
		*ret_num = walk.count;
	ret = walk.total;
	UID_READ_UNLOCK(heap, state);
	return ret;
}

/* name unknown: the PUID of pid for guid */
SceUID PUIDFindByGUID(SceUID pid, SceUID guid)
{
	UIDEntryHeap *heap = NULL;
	UIDEntryFind find;
	int state, ret;

	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = UID_READ_LOCK(heap, 7239);
	find.key = guid;
	ret = FixedHeapWalk(&heap->fixedHeap, UIDEntryFindByBaseGUID, &find);
	if (ret == 0 && find.entry != NULL) {
		ret = find.index;
		if (ret >= 0)
			ret = find.entry->uid;
	} else {
		ret = UID_ERROR_INVALID_UID;
	}
	UID_READ_UNLOCK(heap, state);
	return ret;
}

/* sceKernelPUIDtoGUIDNoWarning */
SceUID SceSysmemForKernel_188C8742(SceUID pid, SceUID puid)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUID2GUID(pid, puid, NULL, 1);
}

SceUID kscePUIDtoGUID(SceUID pid, SceUID puid)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUID2GUID(pid, puid, NULL, 0);
}

SceUID kscePUIDtoGUIDWithClass(SceUID pid, SceUID puid, SceClass *cls)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUID2GUID(pid, puid, cls, 0);
}

/* name unknown: and one user more of the GUID */
SceUID PUID2GUIDWithRefer(SceUID pid, SceUID puid, SceClass *cls)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUID2GUID(pid, puid, cls, 0x10);
}

/* PUIDGetObject */
static inline SceObjectBase *UIDEntryGetObject(const UIDEntry *entry)
{
	SceObjectBase *object = entry->object;

	if (object == NULL)
		SCE_DBG_PRINTF(0xb, 1116, "Warning:pObject is SCE_NULL\n");
	return object;
}

int kscePUIDGetObject(SceUID pid, SceUID puid, SceObjectBase **out)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (out == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid)) {
		*out = NULL;
		return UID_ERROR_INVALID_UID;
	}
	heap = NULL;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = FixedHeapReadLock(&heap->fixedHeap, "PUIDGetObject", 7007);
	ret = UIDEntryCheckInvalid(heap, puid, 0, &entry);
	if (ret < 0) {
		UID_READ_UNLOCK(heap, state);
		*out = NULL;
		return ret;
	}
	*out = UIDEntryGetObject(entry);
	UID_READ_UNLOCK(heap, state);
	return ret;
}

/* PUIDGetClass */
int kscePUIDGetClass(SceUID pid, SceUID puid, SceClass **out)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid)) {
		*out = NULL;
		return UID_ERROR_INVALID_UID;
	}
	heap = NULL;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = FixedHeapReadLock(&heap->fixedHeap, "PUIDGetClass", 7102);
	ret = UIDEntryCheckInvalid(heap, puid, 0, &entry);
	if (ret >= 0)
		ret = UIDEntryGetClass(entry, out);
	UID_READ_UNLOCK(heap, state);
	return ret;
}

/* PUIDGetName */
int kscePUIDGetName(SceUID pid, SceUID puid, const char **name)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid))
		return UID_ERROR_INVALID_UID;
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	heap = NULL;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = FixedHeapReadLock(&heap->fixedHeap, "PUIDGetName", 7039);
	ret = UIDEntryCheckInvalid(heap, puid, 0, &entry);
	if (ret >= 0)
		*name = entry->name;
	UID_READ_UNLOCK(heap, state);
	return ret;
}

/* PUIDSetName: the old name is not deleted, as in the original */
int kscePUIDSetName(SceUID pid, SceUID puid, const char *name)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	NameEntry *nameEntry;
	int state, ret;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid))
		return UID_ERROR_INVALID_UID;
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	heap = NULL;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = FixedHeapWriteLock(&heap->fixedHeap, "PUIDSetName", 7068);
	ret = UIDEntryCheckInvalid(heap, puid, 0, &entry);
	if (ret >= 0) {
		ret = NameHeapInsert(name, 1, &nameEntry);
		if (ret >= 0)
			entry->name = nameEntry->name;
	}
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

int scePUIDGetAttr(SceUID pid, SceUID puid, SceUInt32 *attr)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	int state, ret;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (attr == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid)) {
		*attr = 0;
		return UID_ERROR_INVALID_UID;
	}
	heap = NULL;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret >= 0) {
		UIDEntryHeap *entryHeap;

		state = FixedHeapReadLock(&heap->fixedHeap, "PUIDGetAttr", 7130);
		ret = UIDEntryCheckInvalid(heap, puid, 0, &entry);
		entryHeap = heap;
		if (ret >= 0)
			*attr = entry->attr;
		UID_READ_UNLOCK(entryHeap, state);
		if (ret >= 0)
			return ret;
	}
	SCE_DBG_PRINTF(0xb, 7719, "PUIDGetAttr(pid=0x%08x, puid=0x%08x) failed 0x%08x\n", pid, puid,
		       ret);
	return ret;
}

SceUID kscePUIDOpenByGUID(SceUID pid, SceUID guid)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	return PUIDOpenByGUID(pid, guid, 0, NULL, 0, UID_ENTRY_ATTR_LEVEL_DEFAULT);
}

SceUID kscePUIDOpenByGUIDWithFlags(SceUID pid, SceUID guid, SceUInt32 flags)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	if (flags & ~3)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUIDOpenByGUID(pid, guid, flags, NULL, 0, UID_ENTRY_ATTR_LEVEL_DEFAULT);
}

/* the class (mode 1: a subclass is fine) and attributes of a PUID open */
static inline void UIDCreateOptCheckForPUID(const UIDCreateOpt *opt, SceClass **cls, int *mode,
					    SceUInt32 *attr)
{
	SCE_DBG_ASSERT_CHECK(1, 213);
	if (opt == NULL) {
		*attr = UID_ENTRY_ATTR_LEVEL_DEFAULT;
		*cls = NULL;
		*mode = 0;
		return;
	}
	*attr = (opt->flags & UID_CREATE_OPT_ATTR) ? opt->attr : UID_ENTRY_ATTR_LEVEL_DEFAULT;
	if (opt->flags & UID_CREATE_OPT_CLASS) {
		*cls = opt->cls;
		*mode = *cls != NULL;
	} else {
		*cls = NULL;
		*mode = 0;
	}
}

/* sceKernelOpenPUIDByGUIDWithOpt */
SceUID SceSysmemForKernel_1FF2749A(SceUID pid, SceUID guid, const UIDCreateOpt *opt)
{
	SceClass *cls;
	SceUInt32 attr;
	int mode;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsGUID(guid))
		return UID_ERROR_INVALID_UID;
	UIDCreateOptCheckForPUID(opt, &cls, &mode, &attr);
	return PUIDOpenByGUID(pid, guid, 0, cls, mode, attr);
}

SceUID kscePUIDOpenByName(SceUID pid, const char *name)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUIDOpenByName(pid, name, NULL, 0, UID_ENTRY_ATTR_LEVEL_DEFAULT);
}

SceUID kscePUIDOpenByNameWithClass(SceUID pid, const char *name, SceClass *cls)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUIDOpenByName(pid, name, cls, 2, UID_ENTRY_ATTR_LEVEL_DEFAULT);
}

/* sceKernelOpenPUIDByNameWithOpt */
SceUID SceSysmemForKernel_201E970B(SceUID pid, const char *name, const UIDCreateOpt *opt)
{
	SceClass *cls;
	SceUInt32 attr;
	int mode;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	UIDCreateOptCheckForPUID(opt, &cls, &mode, &attr);
	return PUIDOpenByName(pid, name, cls, mode, attr);
}

int kscePUIDClose(SceUID pid, SceUID puid)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid))
		return UID_ERROR_INVALID_UID;
	return PUIDClose(pid, puid, 0);
}

/* name unknown: flags bit 0, close without closing the GUID */
int PUIDCloseWithFlags(SceUID pid, SceUID puid, SceUInt32 flags)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid))
		return UID_ERROR_INVALID_UID;
	if (flags >> 1)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUIDClose(pid, puid, flags);
}

/* scePUIDReferObject */
int SceSysmemForKernel_2C15615F(SceUID pid, SceUID puid, SceObjectBase **out)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid)) {
		*out = NULL;
		return UID_ERROR_INVALID_UID;
	}
	return PUIDReferObject(pid, puid, NULL, 7, out);
}

/* scePUIDReferObjectWithClass */
int SceSysmemForKernel_9C23DFF7(SceUID pid, SceUID puid, SceClass *cls, SceObjectBase **out)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid)) {
		*out = NULL;
		return UID_ERROR_INVALID_UID;
	}
	if (out == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return PUIDReferObject(pid, puid, cls, 7, out);
}

/* name unknown: scePUIDReferObjectWithLevel */
int PUIDReferObjectWithLevel(SceUID pid, SceUID puid, SceUInt32 level, SceObjectBase **out)
{
	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid)) {
		*out = NULL;
		return UID_ERROR_INVALID_UID;
	}
	return PUIDReferObject(pid, puid, NULL, level, out);
}

/* PUIDReleaseObject: one user less, closing an unused PUID and its GUID link */
int UIDEntryReleaseObject(SceUID pid, SceUID puid)
{
	UIDEntryHeap *heap;
	UIDEntry *entry;
	SceUID guid;
	int state, ret;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (!UIDIsPUID(puid))
		return UID_ERROR_INVALID_UID;
	heap = NULL;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = FixedHeapWriteLock(&heap->fixedHeap, "PUIDReleaseObject", 7189);
	ret = UIDEntryCheckInvalid(heap, puid, 0, &entry);
	if (ret >= 0) {
		UIDEntry *releaseEntry = entry;

		if (releaseEntry->object == NULL)
			SCE_DBG_PRINTF(0xb, 2937, "Warning:pEntry[%p]->pObject is SCE_NULL\n", releaseEntry);
		if (releaseEntry->attr & UID_ENTRY_ATTR_LINK_ANY) {
			if (releaseEntry->useCount == 0) {
				SCE_DBG_PRINTF(0xb, 2944,
					       "pid=0x%08x, puid=0x%08x, Illegal ucount=0x%04x\n",
					       pid, puid, 0);
				SCE_DBG_ASSERT_FAIL(2945);
			} else {
				releaseEntry->useCount--;
				if (UIDEntryIsUnused(releaseEntry) && (releaseEntry->attr & UID_ENTRY_ATTR_PUID) &&
				    (guid = releaseEntry->guid) >= 0) {
					UIDEntryDestructorRaw(releaseEntry);
					if (guid != 0x10000) {
						UIDEntryHeap *entryHeap = heap;
						UIDEntry *freeEntry = entry;

						UIDEntryHeapCheckFreeUIDEntry(entryHeap, freeEntry);
						ret = FixedHeapFreeObjectRaw(&entryHeap->fixedHeap, freeEntry);
						if (ret >= 0)
							ret = GUIDCloseLink(guid);
					}
				}
			}
		}
	}
	UID_WRITE_UNLOCK(heap, state);
	return ret;
}

/* scePUIDGetInfo */
int SceSysmemForKernel_857701D4(SceUID pid, SceUID puid, SceKernelPUIDInfo *info)
{
	UIDEntryHeap *heap = NULL;
	UIDEntry *entry;
	int state, ret;

	if (!UIDIsPUID(puid))
		return UID_ERROR_INVALID_UID;
	if (info == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	if (info->size != sizeof(*info))
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT_SIZE;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = FixedHeapReadLock(&heap->fixedHeap, "scePUIDGetInfo", 7992);
	ret = UIDEntryCheckInvalid(heap, puid, 0, &entry);
	if (ret >= 0) {
		UIDEntry *infoEntry = entry;
		SceUID owner = infoEntry->pid;
		SceUInt32 attr = infoEntry->attr;
		SceUID uid = infoEntry->uid;

		info->attr = attr;
		info->uid = uid;
		info->pid = owner;
		info->classAttr = infoEntry->classAttr;
		info->linkCount = infoEntry->linkCount;
		info->useCount = infoEntry->useCount;
	}
	UID_READ_UNLOCK(heap, state);
	return ret;
}

/* PUIDGetUIDVectorByClass */
int SceSysmemForKernel_08C05493(SceUID pid, SceClass *cls, SceUInt32 level, SceUID *vector,
				SceSize num, SceSize *ret_num)
{
	UIDEntryHeap *heap;
	UIDVectorByClass walk;
	int state, ret;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0)
		return ret;
	state = FixedHeapReadLock(&heap->fixedHeap, "PUIDGetUIDVectorByClass", 7491);
	walk.size = num;
	walk.heap = heap;
	walk.count = 0;
	walk.total = 0;
	walk.cls = cls;
	walk.level = level;
	walk.uids = vector;
	FixedHeapWalk(&heap->fixedHeap, UIDEntryCheckClass, &walk);
	if (ret_num != NULL)
		*ret_num = walk.count;
	ret = walk.total;
	UID_READ_UNLOCK(heap, state);
	return ret;
}

/* the PUIDs of pid that are links to guid */
int PUIDGetPUIDVectorByGUID(SceUID pid, SceUID guid, SceUInt32 level, SceUID *vector, SceSize size,
			    SceSize *ret_num)
{
	UIDEntryHeap *heap;
	UIDFuncVector walk;
	int ret;

	if (pid == SCE_KERNEL_PROCESS_ID)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = GetPUIDEntryHeap(pid, &heap);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 7517, "GetPUIDEntryHeap failed 0x%08x\n", ret);
		return ret;
	}
	walk.count = 0;
	walk.total = 0;
	walk.size = size;
	walk.match = UIDFuncMatchGUID;
	walk.add = UIDFuncAddUID;
	walk.heap = heap;
	walk.level = level;
	walk.buffer = vector;
	walk.key = guid;
	ret = UIDFuncVectorGetUIDVectorWeak(&walk);
	if (ret_num != NULL)
		*ret_num = ret >= 0 ? walk.count : 0;
	return ret;
}

/* name unknown: SceUIDEntryHeapClass, and SceUIDSysrootClass */
int UIDEntryHeapClassInit(UIDManager *manager)
{
	FixedHeapClass *cls = manager->entryHeapClass;
	SceClass *sysrootClass;
	int ret;

	s_registerHeap = manager->registerHeap;
	s_entryHeapClass = cls;
	ret = FixedHeapInitClass(cls, "SceUIDEntryHeapClass");
	if (ret < 0)
		return ret;
	cls = s_entryHeapClass;
	cls->onCarve = UIDEntryHeapClassOnCarve;
	cls->setPage = UIDEntryHeapClassSetPage;
	sysrootClass = (SceClass *)SceSysrootForKernel_085C2BCB();
	sysrootClass->create_cb = NULL;
	sysrootClass->name = "SceUIDSysrootClass";
	sysrootClass->destroy_cb = NULL;
	return 0;
}

int sceUIDEntryHeapSetHook(SceUID guid, FixedHeapObjectHook *hook)
{
#if FW_SINCE(0x04000000)
	UIDEntryHeap *heap;
	int ret;

	ret = ksceGUIDGetObjectWithClass(guid, &s_entryHeapClass->heapClass.cls,
					 (SceObjectBase **)&heap);
	if (ret < 0) {
		SCE_DBG_PRINTF(0xb, 8460, "sceGUIDGetObjectWithClass(0x%08x) failed 0x%08x\n", guid,
			       ret);
		return UID_ERROR_INVALID_UID;
	}
	return SceSysmemForKernel_811C452B(&heap->fixedHeap, hook);
#else
	/* no hooks before 4.00 */
	return 0;
#endif
}
