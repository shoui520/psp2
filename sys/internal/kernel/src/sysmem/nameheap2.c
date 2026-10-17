/*
 * SceSysmem: the name heap. Kernel object names are kept once each, counted,
 * in a hash table: 512 buckets under 16 RW spinlocks. Names of up to 31
 * characters are 0x30-byte objects of a fixed heap ("FixedHeap48"), longer
 * ones (flag 0x10000) come from the kernel heap. There are two tables: one
 * for flag 2, one for the rest.
 */

#include <psp2kern/kernel/sysclib.h>
#include "sysmem_internal.h"

#define NAME_HEAP_ERROR_NO_MEMORY	SCE_KERNEL_ERROR_SYSMEM_ERROR
#define NAME_HEAP_ERROR_EXIST		SCE_KERNEL_ERROR_DUPLICATE_NAME
/* name unknown */

/* both tables, set up by the UID manager; [0] for flag 2, [1] for the rest */
static NameHeapControl *s_nameHeapControls;

/* sceKernelSetNameHeapHook: no hooks in this build */
int SceSysmemForKernel_6DBDA03B(void *hook)
{
	return 0;
}

/* name unknown: a name of table 0 that belongs to a UID entry */
int NameEntrySetUIDEntry(NameEntry *entry, void *uidEntry)
{
	entry->owner = uidEntry;
	entry->attr = (entry->attr & ~NAME_ENTRY_ATTR_KIND_MASK) | NAME_ENTRY_KIND_UID_ENTRY;
	return 0;
}

static inline NameHeapControl *NameHeapGetControl(SceUInt32 flags)
{
	NameHeapControl *control = s_nameHeapControls;

	if (control != NULL && !(flags & NAME_HEAP_FLAG_SECOND))
		control = &s_nameHeapControls[1];
	return control;
}

/* the hash table slot of a name */
static inline SceUInt32 NameHeapHash(const NameHeapControl *control, const char *name)
{
	return SceSysmemForKernel_EB48682E(name, strnlen(name, NAME_ENTRY_SHORT_NAME),
					  control->seed);
}

static inline void NameEntryIncCount(NameEntry *entry)
{
	SCE_DBG_ASSERT_CHECK(entry->refCount < 0xf0000000, 151);
	entry->refCount++;
}

static inline NameEntry *NameHeapControlNameEntryNew(NameHeapControl *control, const char *name,
						     SceUInt32 flags, SceUInt32 bucket)
{
	NameEntry *entry;
	SceSize length;

	if (!(flags & NAME_HEAP_FLAG_LONG)) {
		entry = SceSysmemForKernel_ABA5313C(control->entryHeap);
		if (entry == NULL) {
			SCE_DBG_PRINTF(0xb, 297, "NameHeapControlNameEntryAlloc failed\n");
			return NULL;
		}
		SceCpuForKernel_9DA58A9E((SceUInt32 *)&control->count);
		strncpy(entry->name, name, NAME_ENTRY_SHORT_NAME - 1);
		entry->name[NAME_ENTRY_SHORT_NAME - 1] = '\0';
		entry->next = NULL;
		entry->refCount = 1;
		entry->attr = bucket << NAME_ENTRY_ATTR_BUCKET_SHIFT | NAME_ENTRY_ATTR_VALID;
	} else {
		length = strnlen(name, 0x100);
		entry = SceSysmemForKernel_85571907(length + 1 + offsetof(NameEntry, name));
		if (entry == NULL) {
			SCE_DBG_PRINTF(0xb, 288, "SceKernelAlloc(%d) failed\n", length + 1);
			return NULL;
		}
		memcpy(entry->name, name, length + 1);
		entry->refCount = 1;
		entry->next = NULL;
		entry->attr = bucket << NAME_ENTRY_ATTR_BUCKET_SHIFT | NAME_ENTRY_ATTR_VALID |
			      NAME_ENTRY_ATTR_KERNEL_HEAP;
	}
	return entry;
}

/* the entry a name handed out by the name heap is in, when it says so */
static inline NameEntry *NameEntryFromName(const char *name)
{
	return (NameEntry *)(name - offsetof(NameEntry, name));
}

static inline int NameEntryGetBucket(const NameEntry *entry, SceUInt32 *bucket)
{
	if (!(entry->attr & NAME_ENTRY_ATTR_VALID))
		return 0;
	*bucket = (entry->attr & NAME_ENTRY_ATTR_BUCKET_MASK) >> NAME_ENTRY_ATTR_BUCKET_SHIFT;
	return *bucket < NAME_HEAP_BUCKETS;
}

static inline int NameHeapControlLink(NameHeapControl *control, const char *name, SceUInt32 flags,
				      NameEntry **out)
{
	NameEntry **link, *entry, *self = NULL;
	SceUInt32 hash, bucket;
	SceKernelRWSpinlock *lock;
	int state;

	if (flags & NAME_HEAP_FLAG_BY_LINK) {
		/* name is an entry's own: try its bucket first */
		self = NameEntryFromName(name);
		if (NameEntryGetBucket(self, &bucket)) {
			lock = &control->locks[bucket % NAME_HEAP_LOCKS];
			state = SceCpuForDriver_4C38CE4D(lock);
			for (link = &control->buckets[bucket]; (entry = *link) != NULL;
			     link = &entry->next) {
				if (strncmp(entry->name, name, NAME_ENTRY_SHORT_NAME) == 0) {
					NameEntryIncCount(entry);
					sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
					*out = *link;
					return 0;
				}
			}
			sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
		}
	}

	hash = NameHeapHash(control, name);
	bucket = hash % NAME_HEAP_BUCKETS;
	lock = &control->locks[hash % NAME_HEAP_LOCKS];
	state = SceCpuForDriver_4C38CE4D(lock);
	for (link = &control->buckets[bucket]; (entry = *link) != NULL; link = &entry->next) {
		if (strncmp(entry->name, name, NAME_ENTRY_SHORT_NAME) == 0)
			break;
	}
	if (entry == NULL) {
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
		SCE_DBG_PRINTF(0xb, 504, "NOT FOUND:pControl=%p, Error for %p[%s], hash=0x%08x\n",
			       control, name, name, bucket);
		*out = NULL;
		return NAME_HEAP_ERROR_NO_MEMORY;
	}
	NameEntryIncCount(entry);
	if (flags & NAME_HEAP_FLAG_BY_LINK)
		SCE_DBG_PRINTF(0xb, 492, "search by link=NG, str=OK [%s][%p-%p]\n", name, name,
			       self);
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
	*out = *link;
	return 0;
}

static inline int NameHeapControlInsert(NameHeapControl *control, const char *name,
					SceUInt32 flags, NameEntry **out)
{
	NameEntry **head, **link, *entry;
	SceUInt32 hash, bucket;
	SceKernelRWSpinlock *lock;
	int state, ret;

	hash = NameHeapHash(control, name);
	bucket = hash % NAME_HEAP_BUCKETS;
	lock = &control->locks[hash % NAME_HEAP_LOCKS];
	state = SceCpuForDriver_4C38CE4D(lock);
	head = &control->buckets[bucket];
	for (link = head; (entry = *link) != NULL; link = &entry->next) {
		if (strncmp(entry->name, name, NAME_ENTRY_SHORT_NAME) != 0)
			continue;
		if (control->attr & NAME_HEAP_CONTROL_ATTR_UNIQUE) {
			ret = NAME_HEAP_ERROR_EXIST;
			goto failed;
		}
		NameEntryIncCount(entry);
		goto found;
	}
	entry = NameHeapControlNameEntryNew(control, name, flags, bucket);
	if (entry == NULL) {
		ret = NAME_HEAP_ERROR_NO_MEMORY;
		SCE_DBG_PRINTF(0xb, 572, "no memory (FixedHeap48) for [%s]\n", name);
		goto failed;
	}
	entry->next = *head;
	*head = entry;
	link = head;
found:
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
	*out = *link;
	return 0;

failed:
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
	*out = NULL;
	return ret;
}

/* one reference less; the last one frees the entry */
static inline void NameHeapControlRelease(NameHeapControl *control, NameEntry **link)
{
	NameEntry *entry = *link;

	if (entry->refCount != 0 && --entry->refCount != 0)
		return;
	*link = entry->next;
	if (entry->attr & NAME_ENTRY_ATTR_KERNEL_HEAP)
		SceSysmemForKernel_4233C16D(entry);
	else
		SceSysmemForKernel_93344755(control->entryHeap, entry);
	SceCpuForKernel_0E04BD2E((SceInt32 *)&control->count);
}

static inline int NameHeapControlDelete(NameHeapControl *control, const char *name,
					SceUInt32 flags)
{
	NameEntry **link, *entry, *self = NULL;
	SceUInt32 hash, bucket;
	SceKernelRWSpinlock *lock;
	int state;

	if (flags & NAME_HEAP_FLAG_BY_LINK) {
		/* name is an entry's own: look for that entry in its bucket */
		self = NameEntryFromName(name);
		if (NameEntryGetBucket(self, &bucket)) {
			lock = &control->locks[bucket % NAME_HEAP_LOCKS];
			state = SceCpuForDriver_4C38CE4D(lock);
			for (link = &control->buckets[bucket]; (entry = *link) != NULL;
			     link = &entry->next) {
				if (entry == self) {
					SCE_DBG_ASSERT_CHECK(self->refCount != 0, 632);
					NameHeapControlRelease(control, link);
					sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
					return 0;
				}
			}
			sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
		}
	}

	hash = NameHeapHash(control, name);
	bucket = hash % NAME_HEAP_BUCKETS;
	lock = &control->locks[hash % NAME_HEAP_LOCKS];
	state = SceCpuForDriver_4C38CE4D(lock);
	for (link = &control->buckets[bucket]; (entry = *link) != NULL; link = &entry->next) {
		if (strncmp(entry->name, name, NAME_ENTRY_SHORT_NAME) == 0)
			break;
	}
	if (entry == NULL) {
		SCE_DBG_PRINTF(0xb, 680, "search by str %p failed [%s] hash 0x%08x\n", self, name,
			       bucket);
		sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
		return SCE_KERNEL_ERROR_ERROR;
	}
	SCE_DBG_ASSERT_CHECK(entry->refCount != 0, 659);
	NameHeapControlRelease(control, link);
	if (flags & NAME_HEAP_FLAG_BY_LINK)
		SCE_DBG_PRINTF(0xb, 670, "search by link=NG, str=OK [%s][%p-%p], hash=0x%08x\n",
			       name, name, self, bucket);
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(lock, state);
	return 0;
}

/* a counted copy of name, or another reference to the one already there */
int NameHeapInsert(const char *name, SceUInt32 flags, NameEntry **out)
{
	NameHeapControl *control;

	if (name == NULL) {
		SCE_DBG_PRINTF(0xb, 729, "name is NULL\n");
		SCE_DBG_ASSERT_FAIL(730);
	}
	if (s_nameHeapControls == NULL)
		return SCE_KERNEL_ERROR_NONAMEHEAP;
	control = (flags & NAME_HEAP_FLAG_SECOND) ? &s_nameHeapControls[0] : &s_nameHeapControls[1];
	return NameHeapControlInsert(control, name, flags, out);
}

/* another reference to a name already there */
int NameHeapLink(const char *name, SceUInt32 flags, NameEntry **out)
{
	int ret;

	if (name == NULL) {
		SCE_DBG_PRINTF(0xb, 754, "name is NULL\n");
		SCE_DBG_ASSERT_FAIL(755);
	}
	ret = NameHeapControlLink(NameHeapGetControl(flags), name, flags, out);
	if (ret < 0)
		SCE_DBG_PRINTF(0xb, 762, "[%s], 0x%08x failed 0x%08x\n", name, flags, ret);
	return ret;
}

int NameHeapDelete(const char *name, SceUInt32 flags)
{
	if (name == NULL) {
		SCE_DBG_PRINTF(0xb, 776, "name is NULL\n");
		SCE_DBG_ASSERT_FAIL(777);
	}
	return NameHeapControlDelete(NameHeapGetControl(flags), name, flags);
}

/* the entry of a name, without a reference */
int NameHeapFind(const char *name, SceUInt32 flags, NameEntry **out)
{
	NameHeapControl *control;
	NameEntry **link, *entry;
	SceUInt32 hash;
	SceKernelRWSpinlock *lock;
	int state, ret;

	if (name == NULL) {
		SCE_DBG_PRINTF(0xb, 795, "name is NULL\n");
		SCE_DBG_ASSERT_FAIL(796);
	}
	control = NameHeapGetControl(flags);
	hash = NameHeapHash(control, name);
	lock = &control->locks[hash % NAME_HEAP_LOCKS];
	state = SceCpuForDriver_EC53D007(lock);
	ret = SCE_KERNEL_ERROR_ERROR;
	for (link = &control->buckets[hash % NAME_HEAP_BUCKETS]; (entry = *link) != NULL;
	     link = &entry->next) {
		if (strncmp(entry->name, name, NAME_ENTRY_SHORT_NAME) == 0) {
			ret = 0;
			break;
		}
	}
	if (entry == NULL)
		*link = NULL;		/* already NULL; as in the original */
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(lock, state);
	*out = *link;
	return ret;
}

/* sceKernelNameHeapInsert: *out is the kept copy of name */
int SceSysmemForKernel_B628B46B(const char *name, unsigned int flags, char **out)
{
	NameEntry *entry;
	int ret;

	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = NameHeapInsert(name, flags, &entry);
	*out = ret < 0 ? NULL : entry->name;
	return ret;
}

/* sceKernelNameHeapLink */
int SceSysmemForKernel_40965FD1(const char *name, SceUInt32 flags, char **out)
{
	NameEntry *entry;
	int ret;

	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = NameHeapLink(name, flags, &entry);
	*out = ret < 0 ? NULL : entry->name;
	return ret;
}

/* sceKernelNameHeapDelete */
int SceSysmemForKernel_634D0D0C(char *name, unsigned int flags)
{
	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	return NameHeapDelete(name, flags);
}

/* sceKernelNameHeapFind */
int SceSysmemForKernel_9F1A9AA6(const char *name, SceUInt32 flags, char **out)
{
	NameEntry *entry;
	int ret;

	if (name == NULL)
		return SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	ret = NameHeapFind(name, flags, &entry);
	*out = ret < 0 ? NULL : entry->name;
	return ret;
}

/* name unknown: the entries in use and their high-water mark */
static inline void NameHeapControlGetCount(const NameHeapControl *control, SceUInt32 *count,
					   SceUInt32 *peak)
{
	*count = control->count & 0xffff;
	*peak = control->count >> 16;
}

int SceSysmemForKernel_01194C2E(void *pInfo)
{
	SceKernelNameHeapInfo *info = pInfo;
	int ret;

	if (info == NULL || info->size != sizeof(*info)) {
		ret = SCE_KERNEL_ERROR_INVALID_ARGUMENT;
	} else {
		NameHeapControlGetCount(&s_nameHeapControls[0], &info->heaps[0].count,
					&info->heaps[0].peak);
		NameHeapControlGetCount(&s_nameHeapControls[1], &info->heaps[1].count,
					&info->heaps[1].peak);
		ret = 0;
	}
	return ret;
}

/* name unknown */
int NameHeapInit(struct UIDManager *manager)
{
	s_nameHeapControls = manager->nameHeapControls;
	return 0;
}
