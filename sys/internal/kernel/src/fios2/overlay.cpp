/*
 * The overlay tables. The system-wide overlays (pid -1) are merged into a
 * process's own table, in order, the first time the process looks at it
 * after they changed. A caller that is not privileged sees and changes only
 * its own process's overlays of the user orders (0..127).
 */

#include <psp2kern/kernel/processmgr.h>
#include <psp2kern/kernel/sysclib.h>
#include "fios2_internal.h"

static SceFiosOverlayID g_fiosNextOverlayId = 1;
static int g_fiosOverlayGeneration = 1;
SceFiosKernelRwlock g_fiosOverlayLock = { -1, -1, -1, 0, 0, 0, 0 };
static SceFiosOverlay g_fiosOverlays[FIOS_OVERLAY_MAX];
static int g_fiosOverlayCount;

static inline size_t fiosStrnlen(const char *s, size_t max)	/* name unknown */
{
	size_t n;

	for (n = 0; n < max; n++)
		if (s[n] == '\0')
			break;
	return n;
}

static inline int fiosIsAlnum(int c)			/* name unknown */
{
	return (unsigned int)((c | 0x20) - 'a') < 26 || (unsigned int)(c - '0') < 10;
}

/* "app0:", "app0:/" and "app0:." name a whole drive */
static inline int fiosPathIsDrive(const char *path)	/* name unknown */
{
	int i;

	for (i = 0; path[i] != '\0'; i++) {
		if (!fiosIsAlnum(path[i]))
			return 0;
		if (path[i + 1] == ':')
			return path[i + 2] == '\0' ||
			       (path[i + 3] == '\0' && (path[i + 2] == '.' || path[i + 2] == '/'));
	}
	return 0;
}

/* the caller may see and change this overlay of pid's */
static inline int fiosOverlayIsAccessible(SceUID pid, const SceFiosOverlay *overlay)	/* name unknown */
{
	SceUID caller = SceThreadmgrForDriver_FA54D49A();

	return fiosCallerIsPrivileged(caller) ||
	       ((signed char)overlay->order >= 0 && pid == caller && pid == overlay->pid);
}

/* the system-wide overlays changed: every process merges them again */
static inline void fiosOverlayChanged(void)		/* name unknown */
{
	if (++g_fiosOverlayGeneration == 0)
		g_fiosOverlayGeneration = 1;
}

/* the overlay goes before the first one of a later order */
static inline SceFiosOverlay *fiosOverlayInsert(SceFiosKernelOverlayTable *table,
						const SceFiosOverlay *overlay)	/* name unknown */
{
	SceFiosOverlay *overlays = table->overlays;
	int n = *table->count;
	int i;

	for (i = 0; i < n; i++) {
		if (overlays[i].order >= overlay->order) {
			memmove(&overlays[i + 1], &overlays[i], (n - i) * sizeof(SceFiosOverlay));
			break;
		}
	}
	(*table->count)++;
	memcpy(&table->overlays[i], overlay, sizeof(SceFiosOverlay));
	return &table->overlays[i];
}

/* the overlay at index leaves the table */
static inline void fiosOverlayDelete(SceFiosKernelOverlayTable *table, int index)	/* name unknown */
{
	int n = --*table->count;

	if (index < n)
		memmove(&table->overlays[index], &table->overlays[index + 1],
			(n - index) * sizeof(SceFiosOverlay));
}

/* checks an overlay from the caller and copies it with its paths normalized (name unknown) */
static int fiosOverlayCopyIn(const SceFiosOverlay *in, SceFiosOverlay *out)
{
	size_t dstLen;

	if (in->type > SCE_FIOS_OVERLAY_TYPE_WRITABLE)
		return SCE_FIOS_ERROR_BAD_OVERLAY;
	dstLen = fiosStrnlen(in->dst, SCE_FIOS2_OVERLAY_PATH_SIZE);
	if (fiosStrnlen(in->src, SCE_FIOS2_OVERLAY_PATH_SIZE) >= SCE_FIOS2_OVERLAY_PATH_SIZE ||
	    dstLen >= SCE_FIOS2_OVERLAY_PATH_SIZE)
		return SCE_FIOS_ERROR_PATH_TOO_LONG;

	memset(out, 0, sizeof(*out));
	out->type = in->type;
	out->pid = in->pid;
	out->order = in->order;
	out->dst_len = sceFiosKernelPathNormalizeDefault(out->dst, in->dst,
							 SCE_FIOS2_OVERLAY_PATH_SIZE);
	out->src_len = sceFiosKernelPathNormalizeDefault(out->src, in->src,
							 SCE_FIOS2_OVERLAY_PATH_SIZE);
	if (out->dst_len >= SCE_FIOS2_OVERLAY_PATH_SIZE)
		return SCE_FIOS_ERROR_BAD_PATH;
	if (out->src_len >= SCE_FIOS2_OVERLAY_PATH_SIZE)
		return SCE_FIOS_ERROR_BAD_PATH;
	return 0;
}

/* the table of pid, -1 the system-wide one */
SceFiosKernelOverlayTable sceFiosKernelOverlayGetTable(SceUID pid)
{
	SceFiosKernelOverlayTable table;
	SceFiosKernelProcessStorage *storage = NULL;
	int i;

	if (pid != -1) {
		if (ksceKernelGetProcessLocalStorageAddrForPid(pid, g_fiosPlsKey,
							       (void **)&storage, 1) != 0) {
			table.max = 0;
			table.count = &table.max;
			table.overlays = NULL;
			return table;
		}
		if (storage != NULL) {
			table.max = FIOS_PROCESS_OVERLAY_MAX;
			table.count = &storage->count;
			table.overlays = storage->overlays;
			if (storage->generation != g_fiosOverlayGeneration) {
				/* drop the system-wide overlays merged before */
				for (i = 0; i < *table.count; i++)
					if (table.overlays[i].pid == -1)
						fiosOverlayDelete(&table, i);
				for (i = 0; i < g_fiosOverlayCount; i++)
					fiosOverlayInsert(&table, &g_fiosOverlays[i]);
				storage->generation = g_fiosOverlayGeneration;
			}
			return table;
		}
	}
	table.max = FIOS_OVERLAY_MAX;
	table.count = &g_fiosOverlayCount;
	table.overlays = g_fiosOverlays;
	return table;
}

int sceFiosKernelOverlayAddInternal(SceUID pid, const SceFiosOverlay *overlay, SceFiosOverlayID *outId)
{
	SceFiosKernelOverlayTable table;
	SceFiosOverlay copy;
	SceFiosOverlay *added;
	SceUID caller;
	int users, i, ret;

	if (outId == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	*outId = 0;
	caller = SceThreadmgrForDriver_FA54D49A();
	if (!fiosCallerIsPrivileged(caller)) {
		if (fiosPathIsDrive(overlay->dst))
			return SCE_FIOS_ERROR_BAD_PATH;
		if (pid != caller || pid == -1)
			return SCE_FIOS_ERROR_ACCESS;
	}

	ret = fiosOverlayCopyIn(overlay, &copy);
	if (ret != 0)
		return ret;
	copy.pid = pid;

	sceFiosKernelRwlockWriteLock(&g_fiosOverlayLock);
	table = sceFiosKernelOverlayGetTable(pid);
	if (!(copy.order & 0x80) && *table.count > 0) {
		users = 0;
		for (i = 0; i < *table.count; i++)
			if ((signed char)table.overlays[i].order >= 0)
				users++;
		if (users >= SCE_FIOS_OVERLAY_MAX_OVERLAYS) {
			ret = SCE_FIOS_ERROR_TOO_MANY_OVERLAYS;
			goto unlock;
		}
	}
	if (*table.count >= table.max) {
		ret = SCE_FIOS_ERROR_TOO_MANY_OVERLAYS;
		goto unlock;
	}
	if (pid == -1)
		fiosOverlayChanged();
	added = fiosOverlayInsert(&table, &copy);
	added->id = g_fiosNextOverlayId;
	if (++g_fiosNextOverlayId == 0)
		g_fiosNextOverlayId++;
	*outId = added->id;
unlock:
	sceFiosKernelRwlockWriteUnlock(&g_fiosOverlayLock);
	return ret;
}

int sceFiosKernelOverlayRemoveInternal(SceUID pid, SceFiosOverlayID id)
{
	SceFiosKernelOverlayTable table;
	SceUID caller;
	int i, ret;

	caller = SceThreadmgrForDriver_FA54D49A();
	if (!fiosCallerIsPrivileged(caller) && (pid != caller || pid == -1))
		return SCE_FIOS_ERROR_ACCESS;

	sceFiosKernelRwlockWriteLock(&g_fiosOverlayLock);
	table = sceFiosKernelOverlayGetTable(pid);
	for (i = 0; i < *table.count; i++) {
		if (table.overlays[i].id != id)
			continue;
		if (pid == -1) {
			fiosOverlayChanged();
			break;
		}
		if (fiosOverlayIsAccessible(pid, &table.overlays[i]))
			break;
	}
	if (i < *table.count) {
		fiosOverlayDelete(&table, i);
		ret = 0;
	} else {
		ret = SCE_FIOS_ERROR_BAD_OVERLAY;
	}
	sceFiosKernelRwlockWriteUnlock(&g_fiosOverlayLock);
	return ret;
}

/* every overlay of pid the caller may remove goes, one at a time */
void sceFiosKernelOverlayRemoveAll(SceUID pid)
{
	SceFiosKernelOverlayTable table;
	SceFiosOverlay *overlay;
	SceFiosOverlayID id;
	SceUID caller;
	unsigned int maxOrder = pid == -1 ? 0xff : SCE_FIOS_OVERLAY_ORDER_USER_LAST;
	int found, i;

	for (;;) {
		sceFiosKernelRwlockReadLock(&g_fiosOverlayLock);
		table = sceFiosKernelOverlayGetTable(pid);
		if (*table.count <= 0) {
			sceFiosKernelRwlockReadUnlock(&g_fiosOverlayLock);
			return;
		}
		id = 0;
		found = 0;
		for (i = 0; i < *table.count; i++) {
			overlay = &table.overlays[i];
			caller = SceThreadmgrForDriver_FA54D49A();
			if (fiosCallerIsPrivileged(caller)) {
				if (overlay->order > maxOrder)
					continue;
			} else if ((signed char)overlay->order < 0 || pid != caller ||
				   pid != overlay->pid) {
				continue;
			}
			if (found == 0)
				id = overlay->id;
			found++;
		}
		sceFiosKernelRwlockReadUnlock(&g_fiosOverlayLock);
		if (found == 0)
			return;
		sceFiosKernelOverlayRemoveInternal(pid, id);
	}
}

int sceFiosKernelOverlayGetInfoInternal(SceUID pid, SceFiosOverlayID id, SceFiosOverlay *outOverlay)
{
	SceFiosKernelOverlayTable table;
	SceUID caller;
	int i, ret;

	if (outOverlay == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;
	caller = SceThreadmgrForDriver_FA54D49A();
	if (!fiosCallerIsPrivileged(caller) && (pid != caller || pid == -1))
		return SCE_FIOS_ERROR_ACCESS;

	sceFiosKernelRwlockReadLock(&g_fiosOverlayLock);
	table = sceFiosKernelOverlayGetTable(pid);
	ret = SCE_FIOS_ERROR_BAD_OVERLAY;
	for (i = 0; i < *table.count; i++) {
		if (table.overlays[i].id != id)
			continue;
		if (pid == -1 || fiosOverlayIsAccessible(pid, &table.overlays[i])) {
			memcpy(outOverlay, &table.overlays[i], sizeof(SceFiosOverlay));
			ret = 0;
			break;
		}
	}
	sceFiosKernelRwlockReadUnlock(&g_fiosOverlayLock);
	return ret;
}

int sceFiosKernelOverlayModifyInternal(SceUID pid, SceFiosOverlayID id, const SceFiosOverlay *overlay)
{
	SceFiosKernelOverlayTable table;
	SceFiosOverlay copy;
	SceUID caller;
	int i, ret;

	caller = SceThreadmgrForDriver_FA54D49A();
	if (!fiosCallerIsPrivileged(caller) && (pid != caller || pid == -1))
		return SCE_FIOS_ERROR_ACCESS;
	ret = fiosOverlayCopyIn(overlay, &copy);
	if (ret != 0)
		return ret;
	copy.pid = pid;
	copy.id = id;

	sceFiosKernelRwlockWriteLock(&g_fiosOverlayLock);
	table = sceFiosKernelOverlayGetTable(pid);
	for (i = 0; i < *table.count; i++) {
		if (table.overlays[i].id != id)
			continue;
		if (pid == -1) {
			fiosOverlayChanged();
			break;
		}
		if (fiosOverlayIsAccessible(pid, &table.overlays[i]))
			break;
	}
	if (i >= *table.count) {
		ret = SCE_FIOS_ERROR_BAD_OVERLAY;
	} else if (overlay->order == table.overlays[i].order) {
		memcpy(&table.overlays[i], &copy, sizeof(SceFiosOverlay));
	} else {
		/* a new order puts it somewhere else */
		fiosOverlayDelete(&table, i);
		fiosOverlayInsert(&table, &copy);
	}
	sceFiosKernelRwlockWriteUnlock(&g_fiosOverlayLock);
	return ret;
}

int sceFiosKernelOverlayGetListInternal(SceUID pid, unsigned int minOrder, unsigned int maxOrder,
					SceFiosOverlayID *outIds, SceSize maxIds, SceSize *outCount)
{
	SceFiosKernelOverlayTable table;
	SceFiosOverlay *overlay;
	SceSize n;
	int i;

	if (maxIds != 0 && outIds == NULL)
		return SCE_FIOS_ERROR_BAD_PTR;

	sceFiosKernelRwlockReadLock(&g_fiosOverlayLock);
	if (outIds != NULL)
		memset(outIds, 0, maxIds * sizeof(SceFiosOverlayID));
	table = sceFiosKernelOverlayGetTable(pid);
	n = 0;
	for (i = 0; i < *table.count; i++) {
		overlay = &table.overlays[i];
		if (!fiosOverlayIsAccessible(pid, overlay))
			continue;
		if (overlay->order < minOrder || overlay->order > maxOrder)
			continue;
		if (outIds != NULL && n < maxIds)
			outIds[n] = overlay->id;
		n++;
	}
	if (outCount != NULL)
		*outCount = n;
	sceFiosKernelRwlockReadUnlock(&g_fiosOverlayLock);
	return 0;
}
