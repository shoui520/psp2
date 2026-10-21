/*
 * Directory handles. Opening one lists the directory once, with what the
 * overlays add to it: the entries of every merged source directory, and a
 * directory entry for each overlay destination that lies inside it. The
 * listing is kept in blocks of a buffer in the caller's process heap; a
 * first pass with no buffer only computes the size.
 */

#include <processmgr/processmgr.h>
#include <sysmem/sysclib.h>
#include "fios2_internal.h"

#define FIOS_DH_ENTRY_SIZE	(sizeof(SceIoDirent) + 8)

/* the storage a directory handle lives in, before its buffer is known */
static const SceFiosKernelStorage g_fiosDHStorageTemplate = {
	"dh", 0, -1, 0, 1, 0, 0, NULL, NULL, 0
};

/* the stat of path through the overlays from index on (name unknown) */
static __attribute__((noinline, noclone)) int fiosDHGetstat(SceUID pid, int resolveFlag,
	const char *path, SceIoStat *stat, char *scratch, int index, uint8_t minOrder,
	uint8_t maxOrder)
{
	SceFiosKernelResolveParam param = {};
	char *resolved;
	int ret;

	param.pid = pid;
	param.resolveFlag = resolveFlag;
	param.maxPath = FIOS_PATH_MAX;
	param.minOrder = minOrder;
	param.maxOrder = maxOrder;
	memset(stat, 0, sizeof(*stat));
	resolved = fiosPathAlloc();
	if (resolved == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(resolved, 0, FIOS_PATH_MAX);
	strlcpy(resolved, path, FIOS_PATH_MAX);
	ret = sceFiosKernelOverlayResolve(&param, index, scratch, resolved);
	if (ret >= 0) {
		ret = sceFiosKernelPlatformGetstat(resolved, stat);
		if (ret == SCE_ERROR_ERRNO_EICV) {
			memset(stat, 0, sizeof(*stat));
			ret = 0;
		}
	}
	fiosPathFree(resolved);
	return ret;
}

/*
 * The overlay applies to the directory path: out is its source with the
 * rest of path behind it (name unknown).
 */
static __attribute__((noinline)) int fiosDHOverlayApplies(SceUID pid, const char *path,
	const SceFiosOverlay *overlay, unsigned int minOrder, uint8_t maxOrder, char *out)
{
	SceUID owner = overlay->pid;
	unsigned int order = overlay->order;
	unsigned int dstLen;
	signed char c;
	char *joined;
	int ret;

	if (owner != -1 && owner != pid)
		return 0;
	if (order < minOrder || order > maxOrder)
		return 0;
	dstLen = overlay->dst_len;
	if (dstLen != 0 && strncmp(path, overlay->dst, dstLen) != 0)
		return 0;
	c = path[dstLen - 1];
	if (c != '\\' && c != '/') {
		c = path[dstLen];
		if (c != '/' && c != '\\' && c != '\0')
			return 0;
	}

	joined = fiosPathAlloc();
	if (joined == NULL)
		return 1;
	ret = 0;
	strlcpy(joined, overlay->src, FIOS_PATH_MAX);
	strlcat(joined, "/", FIOS_PATH_MAX);
	if (strlcat(joined, path + overlay->dst_len, FIOS_PATH_MAX) < FIOS_PATH_MAX)
		ret = sceFiosKernelPathNormalizeDefault(out, joined, FIOS_PATH_MAX) < FIOS_PATH_MAX;
	fiosPathFree(joined);
	return ret;
}

int sceFiosKernelDHReadInternal(SceFiosKernelDH *dh, SceIoDirent *dirent, SceIoStat *stat,
				unsigned int minOrder, unsigned int maxOrder, void *platform)
{
	SceFiosKernelDHEntry *entry;
	int ret;

	if (dh == (SceFiosKernelDH *)-1 || dh->magic != SCE_FIOS_KERNEL_DH_MAGIC ||
	    dh->pid != SceThreadmgrForDriver_FA54D49A())
		return SCE_FIOS_ERROR_BAD_DH;

	if (dh->next != NULL) {
		memcpy(dirent, dh->next, sizeof(*dirent));
		dh->next = dh->next->next;
		return 0;
	}
	/* the directory's own entries, but not those the overlays already gave */
	for (;;) {
		if (dh->fd == -1)
			return SCE_FIOS_ERROR_EOF;
		ret = sceFiosKernelPlatformDread(dh->fd, dirent, platform);
		if (ret == 0)
			return SCE_FIOS_ERROR_EOF;
		if (ret < 0)
			return ret;
		for (entry = dh->head; entry != NULL; entry = entry->next)
			if (strncmp(entry->dirent.d_name, dirent->d_name, sizeof(dirent->d_name)) == 0)
				break;
		if (entry == NULL)
			return 0;
	}
}

int sceFiosKernelDHRead(SceFiosKernelDH *dh, SceIoDirent *dirent, SceIoStat *stat, void *platform)
{
	return sceFiosKernelDHReadInternal(dh, dirent, stat, 0, 0xff, platform);
}

/* the entry goes at the end of the listing */
static inline void fiosDHAppend(SceFiosKernelDH *dh, SceFiosKernelDHEntry *entry)	/* name unknown */
{
	if (dh->tail != NULL) {
		dh->tail->next = entry;
		dh->tail = entry;
	} else {
		dh->tail = entry;
		dh->next = entry;
		dh->head = entry;
	}
}

/* a copy of the current entry in the storage, at the end of the listing */
static inline void fiosDHAddEntry(SceFiosKernelDHBuild *build, SceFiosKernelDH *dh)	/* name unknown */
{
	SceFiosKernelDHEntry *entry;
	SceIoDirent *dirent = build->dirent;

	entry = (SceFiosKernelDHEntry *)fiosStorageAlloc(build->storage, FIOS_DH_ENTRY_SIZE);
	if (entry == NULL)
		return;
	memcpy(entry, dirent, sizeof(*dirent));
	entry->next = NULL;
	entry->reserved = 0;
	fiosDHAppend(dh, entry);
}

/*
 * Lists path from the overlay at index on into a new directory handle in
 * build's storage (*outDH), or without storage returns the bytes it needs.
 */
static int fiosDHBuild(SceFiosKernelDHBuild *build, SceFiosKernelDH **outDH, const char *path,
		       int index, int depth)	/* name unknown */
{
	SceFiosKernelOverlayTable table;
	SceFiosKernelDH *dh = NULL;
	SceFiosKernelDH *sub;
	SceFiosKernelDHEntry *entry, *other, **link;
	SceFiosOverlay *overlay;
	SceUInt32 sdkVersion;
	SceUID fd;
	SceSize len, prefixLen, componentLen;
	char *normal, *resolved, *name;
	const char *component, *slash, *backslash, *separator;
	int entries = 0, size = sizeof(SceFiosKernelDH);
	int applies, n, i, ret;

	for (i = 0; path[i] != '\0'; )
		if (++i == FIOS_PATH_MAX)
			return SCE_FIOS_ERROR_PATH_TOO_LONG;

	normal = fiosPathAlloc();
	if (normal == NULL)
		return SCE_KERNEL_ERROR_NO_MEMORY;
	memset(normal, 0, FIOS_PATH_MAX);
	len = sceFiosKernelPathNormalizeDefault(normal, path, FIOS_PATH_MAX);
	if (len >= FIOS_PATH_MAX) {
		ret = SCE_FIOS_ERROR_BAD_PATH;
		goto free;
	}
	/* what an overlay destination inside the directory starts with */
	prefixLen = len > 1 ? len + 1 : len;

	if (build->storage != NULL) {
		dh = (SceFiosKernelDH *)fiosStorageAlloc(build->storage, sizeof(SceFiosKernelDH));
		if (dh == NULL) {
			ret = SCE_FIOS_ERROR_BAD_SIZE;
			goto free;
		}
		dh->magic = SCE_FIOS_KERNEL_DH_MAGIC;
		dh->pid = build->pid;
		dh->fd = -1;
		dh->head = NULL;
		dh->tail = NULL;
		dh->next = NULL;
		strlcpy(dh->path, normal, FIOS_PATH_MAX);
		dh->path[FIOS_PATH_MAX - 1] = '\0';
	}

	sceFiosKernelRwlockReadLock(&g_fiosOverlayLock);
	table = sceFiosKernelOverlayGetTable(build->pid);
	n = *table.count;
	for (; index < n; index++) {
		overlay = &table.overlays[index];
		resolved = fiosPathAlloc();
		if (resolved == NULL) {
			ret = SCE_KERNEL_ERROR_NO_MEMORY;
			goto unlock;
		}
		applies = fiosDHOverlayApplies(build->pid, normal, overlay, build->minOrder,
					       build->maxOrder, resolved);

		if (len < overlay->dst_len && strncmp(normal, overlay->dst, len) == 0 &&
		    prefixLen != 0 && (overlay->dst[prefixLen - 1] == '/' ||
				       overlay->dst[prefixLen - 1] == '\\')) {
			if (applies)
				goto merge;
			/* the destination lies inside: its first component is an entry */
			size += FIOS_DH_ENTRY_SIZE;
			if (dh == NULL)
				goto next;
			component = &overlay->dst[prefixLen];
			slash = strchr(component, '/');
			backslash = strchr(component, '\\');
			if (slash != NULL && backslash != NULL) {
				if ((SceSize)(backslash - overlay->dst) < (SceSize)(slash - overlay->dst))
					separator = backslash;
				else
					separator = slash;
			} else if (slash != NULL) {
				separator = slash;
			} else {
				separator = backslash;
			}
			componentLen = separator != NULL ? (SceSize)(separator - overlay->dst) :
							   overlay->dst_len;
			if (strlcpy(*build->path, overlay->dst, componentLen + 1) >= FIOS_PATH_MAX)
				goto next;
			memset(build->dirent, 0, sizeof(*build->dirent));
			name = *build->path + prefixLen;
			while (*name == '/' || *name == '\\')
				name++;
			strlcpy(build->dirent->d_name, name, sizeof(build->dirent->d_name));
			if (fiosDHGetstat(build->pid, 0, *build->path, build->stat, *build->resolved,
					  index, build->minOrder, build->maxOrder) != 0) {
				if (separator == NULL)
					goto next;
				build->stat->st_mode |= SCE_STM_FDIR | SCE_STM_RWU;
			}
			entries++;
			fiosDHAddEntry(build, dh);
			goto next;
		}
		if (!applies)
			goto next;
merge:
		if (overlay->type == SCE_FIOS_OVERLAY_TYPE_OPAQUE) {
			/* the source replaces the directory */
			strlcpy(normal, resolved, FIOS_PATH_MAX);
			if (SceProcessmgrForDriver_D141C076(build->pid, &sdkVersion) != 0 ||
			    sdkVersion < 0x1800000) {
				entries = 0;
				if (dh != NULL) {
					dh->next = NULL;
					dh->head = NULL;
					dh->tail = NULL;
				}
			}
			goto next;
		}

		/* the source directory is merged in */
		sub = NULL;
		ret = fiosDHBuild(build, &sub, resolved, index + 1, depth + 1);
		if (ret < 0)
			goto next;
		if (dh == NULL) {
			size += ret;
			entries++;
			goto next;
		}
		if (sub->head != NULL) {
			entries += 2;
			if (dh->tail == NULL)
				dh->head = sub->head;
			else
				dh->tail->next = sub->head;
			dh->tail = sub->tail;
			sub->tail = NULL;
			sub->next = NULL;
			sub->head = NULL;
		} else {
			entries++;
		}
		for (;;) {
			memset(build->dirent, 0, sizeof(*build->dirent));
			if (sceFiosKernelDHReadInternal(sub, build->dirent, build->stat,
							build->minOrder, build->maxOrder,
							build->platform) != 0)
				break;
			entries++;
			size += FIOS_DH_ENTRY_SIZE;
			fiosDHAddEntry(build, dh);
		}
		sceFiosKernelDHClose(sub);
next:
		fiosPathFree(resolved);
	}

	if (dh == NULL) {
		/* the directory's own entries */
		if (depth > 0 && sceFiosKernelPlatformDopen(&fd, normal, build->platform) == 0) {
			while (sceFiosKernelPlatformDread(fd, build->dirent, build->platform) > 0)
				size += FIOS_DH_ENTRY_SIZE;
			sceFiosKernelPlatformDclose(fd);
		}
		goto size;
	}
	ret = sceFiosKernelPlatformDopen(&fd, normal, build->platform);
	if (ret == 0) {
		dh->fd = fd;
	} else if (entries == 0) {
		/* a directory only an overlay destination names is there */
		for (i = 0; i < *table.count; i++)
			if (strncmp(table.overlays[i].dst, normal, SCE_FIOS_OVERLAY_POINT_MAX) == 0)
				goto size;
		if (fd == (SceUID)SCE_ERROR_ERRNO_ENOTDIR)
			ret = SCE_FIOS_ERROR_NOT_A_DIRECTORY;
		else if (fd == (SceUID)SCE_ERROR_ERRNO_ENOENT || fd == (SceUID)SCE_ERROR_ERRNO_ENODEV)
			ret = SCE_FIOS_ERROR_BAD_PATH;
		goto unlock;
	}
	if (build->storage == NULL || entries == 0)
		goto size;

	/* an entry the overlays give twice is listed once */
	for (entry = dh->head; entry != NULL; entry = entry->next) {
		link = &entry->next;
		other = entry->next;
		while (other != NULL) {
			if (strncmp(entry->dirent.d_name, other->dirent.d_name,
				    sizeof(entry->dirent.d_name)) == 0) {
				sceFiosKernelStorageFreeBlocks(build->storage,
					((char *)other - (char *)build->storage->blocks) /
						build->storage->blockSize,
					((FIOS_DH_ENTRY_SIZE + build->storage->blockSize - 1) &
					 -build->storage->blockSize) / build->storage->blockSize);
				*link = (*link)->next;
				other = *link;
			} else {
				link = &other->next;
				other = other->next;
			}
		}
	}
	entry = dh->head;
	if (entry != NULL) {
		while (entry->next != NULL)
			entry = entry->next;
		dh->tail = entry;
		entry = dh->head;
	}
	dh->next = entry;
size:
	/* the blocks and their bitmap */
	ret = (size + (size + 7) / 8 + 7) & ~7;
unlock:
	sceFiosKernelRwlockReadUnlock(&g_fiosOverlayLock);
	*outDH = dh;
free:
	fiosPathFree(normal);
	return ret;
}

int sceFiosKernelDHOpenInternal(SceUID pid, SceFiosKernelDH **outDH, const char *path,
				unsigned int minOrder, unsigned int maxOrder, void *buffer,
				SceSize bufferSize, void *platform)
{
	SceFiosKernelDHBuild build;
	SceFiosKernelStorage storage;
	SceIoDirent dirent;
	void *own = NULL;
	char *pathBuffer, *resolved;
	int ret;

	build.maxOrder = maxOrder;
	build.storage = NULL;
	build.dirent = NULL;
	build.stat = NULL;
	build.platform = platform;
	build.pid = pid;
	build.minOrder = minOrder;

	pathBuffer = fiosPathAlloc();
	resolved = fiosPathAlloc();
	if (resolved == NULL) {
		ret = SCE_FIOS_ERROR_CANT_ALLOCATE_DH;
		goto free;
	}
	if (pathBuffer == NULL) {
		ret = SCE_FIOS_ERROR_CANT_ALLOCATE_DH;
		goto free;
	}
	build.path = &pathBuffer;
	build.resolved = &resolved;
	if (platform == NULL)
		own = build.platform = sceFiosKernelPlatformOpen();
	build.dirent = &dirent;
	build.stat = &dirent.d_stat;

	if (buffer == NULL) {
		ret = fiosDHBuild(&build, outDH, path, 0, 0);
	} else if (bufferSize < sizeof(SceFiosKernelDH)) {
		ret = SCE_FIOS_ERROR_BAD_SIZE;
	} else if (((uintptr_t)buffer & 7) != 0) {
		ret = SCE_FIOS_ERROR_BAD_ALIGNMENT;
	} else {
		storage = g_fiosDHStorageTemplate;
		ret = sceFiosKernelStorageInit(&storage, buffer, bufferSize, 0x4000, 8, 8);
		if (ret == 0) {
			build.storage = &storage;
			ret = fiosDHBuild(&build, outDH, path, 0, 0);
		}
	}
	if (own != NULL)
		sceFiosKernelPlatformClose(own);
free:
	if (resolved != NULL) {
		resolved[0] = '\0';
		if (resolved != NULL)
			SceProcessmgrForDriver_FC2A424E(resolved);
	}
	resolved = NULL;
	if (pathBuffer != NULL) {
		pathBuffer[0] = '\0';
		if (pathBuffer != NULL)
			SceProcessmgrForDriver_FC2A424E(pathBuffer);
	}
	return ret;
}

int sceFiosKernelDHClose(SceFiosKernelDH *dh)
{
	int privileged = fiosCallerIsPrivileged(SceThreadmgrForDriver_FA54D49A());

	if (dh == (SceFiosKernelDH *)-1 || dh->magic != SCE_FIOS_KERNEL_DH_MAGIC ||
	    (!privileged && dh->pid != SceThreadmgrForDriver_FA54D49A()))
		return SCE_FIOS_ERROR_BAD_DH;
	if (dh->fd != -1) {
		sceFiosKernelPlatformDclose(dh->fd);
		dh->fd = -1;
	}
	dh->magic = -1;
	return 0;
}
