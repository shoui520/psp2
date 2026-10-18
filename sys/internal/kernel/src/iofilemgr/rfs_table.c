/*
 * SceIofilemgr: the tree of device roots under "/" (Sony's rfs_table.c).
 */

#include "iofilemgr_internal.h"

RfsEntry g_rfsTable[RFS_TABLE_ENTRY_NUM];
SceKernelFastMutex g_rfsLock;

/* the element name equals the entry's name */
static inline __attribute__((always_inline)) int rfs_name_equal(const RfsEntry *ent,
								const VfsPathElem *elem)
{
	const signed char *s = (const signed char *)elem->name;
	const signed char *n = (const signed char *)ent->name;
	signed char c = *n;

	while (*s++ == c) {
		c = *++n;
		if (c == '\0' && elem->len == (SceSize)(s - (const signed char *)elem->name))
			return 1;
	}
	return 0;
}

int rfs_table_lookup_entry(const VfsPathElem *elem, int num, SceUInt32 flags, RfsEntry **entp,
			   int *depth)
{
	RfsEntry *dir, *ent, *last;
	SceUInt32 entFlags;
	SceSize len;
	const char *source, *end;
	char *dest;
	int i, n;

	if (num < 1) {
		*entp = NULL;
		return 0;
	}
	dir = &g_rfsTable[0];
	for (i = 0; i < num; i++, dir = ent) {
		len = elem[i].len;
		if (len == 0) {
			ent = dir;
			entFlags = dir->flags;
		} else {
			for (ent = dir->child; ent != NULL; ent = ent->next) {
				if (rfs_name_equal(ent, &elem[i]))
					break;
			}
			if (ent != NULL) {
				entFlags = ent->flags;
				if ((entFlags & RFS_ENTRY_PLACEHOLDER) == 0)
					goto check_root;
				if ((flags & RFS_LOOKUP_CREATE) == 0)
					goto placeholder;
				entFlags = RFS_ENTRY_CREATED | RFS_ENTRY_VALID;
				ent->flags = entFlags;
				goto check_root;
			}
			if ((flags & (RFS_LOOKUP_ENTER | RFS_LOOKUP_CREATE)) == 0) {
				*entp = NULL;
				return SCE_ERROR_ERRNO_ENOMEM;
			}
			for (n = 1; g_rfsTable[n].flags != 0;) {
				if (++n == RFS_TABLE_ENTRY_NUM) {
					*entp = NULL;
					return SCE_ERROR_ERRNO_ENOMEM;
				}
			}
			ent = &g_rfsTable[n];
			if ((int)len > 0) {
				source = elem[i].name;
				end = source + len;
				dest = ent->name;
				do {
					*dest++ = *source++;
				} while (source != end);
			}
			ent->name[(int)len > 0 ? len : 0] = '\0';
			ent->child = NULL;
			last = dir->child;
			ent->vp = NULL;
			ent->attr = 0;
			ent->next = NULL;
			ent->parent = dir;
			ent->flags = (flags & RFS_LOOKUP_CREATE) != 0 ?
					     RFS_ENTRY_CREATED | RFS_ENTRY_VALID :
					     RFS_ENTRY_PLACEHOLDER;
			if (last == NULL) {
				dir->child = ent;
			} else {
				for (; last->next != NULL; last = last->next)
					;
				last->next = ent;
			}
			entFlags = ent->flags;
		}
		if ((entFlags & RFS_ENTRY_PLACEHOLDER) != 0) {
placeholder:
			SCE_DBG_PRINTF_LEVEL(2, 0xf, 347, "ent->flags = %d 0x%x %s\n", entFlags,
					     entFlags, ent->name);
			*entp = ent;
			return SCE_ERROR_ERRNO_ENOENT;
		}
check_root:
		if ((flags & RFS_LOOKUP_STOP_AT_ROOT) != 0 && (entFlags & RFS_ENTRY_ROOT) != 0) {
			if (depth != NULL) {
				*depth = i;
				*entp = ent;
				return 0;
			}
			break;
		}
	}
	*entp = ent;
	return 0;
}

/* name unknown: frees an entry and everything under it */
static inline void rfs_clear_entry(RfsEntry *ent)
{
	RfsEntry *child, *next;

	for (child = ent->child; child != NULL; child = next) {
		next = child->next;
		rfs_clear_entry(child);
	}
	memset(ent->name, 0, sizeof(ent->name));
	ent->vp = NULL;
	ent->attr = 0;
	ent->parent = NULL;
	ent->child = NULL;
	ent->next = NULL;
	ent->flags = 0;
}

/* name unknown: the first device root in a list of entries and under them */
static inline RfsEntry *rfs_find_root(RfsEntry *ent)
{
	RfsEntry *found;

	for (; ent != NULL; ent = ent->next) {
		if ((ent->flags & RFS_ENTRY_ROOT) != 0)
			return ent;
		found = rfs_find_root(ent->child);
		if (found != NULL)
			return found;
	}
	return NULL;
}

void rfs_table_initialize(void)
{
	RfsEntry *ent;

	for (ent = g_rfsTable; ent != &g_rfsTable[RFS_TABLE_ENTRY_NUM]; ent++)
		memset(ent, 0, sizeof(*ent));
	g_rfsTable[0].name[1] = '\0';
	g_rfsTable[0].vp = NULL;
	g_rfsTable[0].attr = 0;
	g_rfsTable[0].next = NULL;
	g_rfsTable[0].parent = NULL;
	g_rfsTable[0].child = NULL;
	g_rfsTable[0].name[0] = '/';
	g_rfsTable[0].flags = RFS_ENTRY_TOP | RFS_ENTRY_VALID;
	if (sceKernelInitializeFastMutex(&g_rfsLock, "SceVfsRfsLock", 2, NULL) != 0)
		SCE_DBG_ASSERT(0, 193);
}

int rfs_add_entry(const VfsPathElem *elem, int num, SceVfsVnode *vp, int attr)
{
	RfsEntry *ent = NULL;
	int ret;

	ret = rfs_table_lookup_entry(elem, num, RFS_LOOKUP_CREATE, &ent, NULL);
	if (ret != 0)
		return ret;
	if (ent->vp != NULL)
		return SCE_ERROR_ERRNO_EEXIST;
	ent->vp = vp;
	ent->attr = attr;
	ent->flags = RFS_ENTRY_ROOT | RFS_ENTRY_VALID;
	return 0;
}

int rfs_get_root(const VfsPathElem *elem, int num, RfsRootInfo *root, int *depth)
{
	RfsEntry *ent = NULL;

	if (rfs_table_lookup_entry(elem, num, RFS_LOOKUP_STOP_AT_ROOT | RFS_LOOKUP_ENTER, &ent,
				   depth) != 0)
		return -1;
	if ((ent->flags & RFS_ENTRY_ROOT) == 0)
		return -2;
	root->vp = ent->vp;
	root->attr = ent->attr;
	return 0;
}

SceVfsVnode *rfs_remove_entry(const VfsPathElem *elem, int num)
{
	RfsEntry *ent, *prev, *child, *next;
	SceVfsVnode *vp = NULL;

	if (rfs_table_lookup_entry(elem, num, RFS_LOOKUP_STOP_AT_ROOT | RFS_LOOKUP_ENTER, &ent,
				   NULL) != 0)
		return NULL;
	if ((ent->flags & RFS_ENTRY_TOP) != 0)
		return NULL;
	vp = ent->vp;
	if (ent->child != NULL && rfs_find_root(ent->child) != NULL) {
		ent->flags = RFS_ENTRY_CREATED | RFS_ENTRY_VALID;
		return vp;
	}
	prev = ent->parent->child;
	if (ent == prev) {
		ent->parent->child = ent->next;
	} else {
		while (prev->next != ent)
			prev = prev->next;
		prev->next = ent->next;
	}
	for (child = ent->child; child != NULL; child = next) {
		next = child->next;
		rfs_clear_entry(child);
	}
	memset(ent->name, 0, sizeof(ent->name));
	ent->parent = NULL;
	ent->vp = NULL;
	ent->attr = 0;
	ent->child = NULL;
	ent->next = NULL;
	ent->flags = 0;
	return vp;
}

void rfs_lock(void)
{
	if (lockFastMutex(&g_rfsLock) != 0)
		SCE_DBG_ASSERT(0, 548);
}

void rfs_unlock(void)
{
	if (sceKernelUnlockFastMutex(&g_rfsLock) != 0)
		SCE_DBG_ASSERT(0, 552);
}

int rfs_trylock(void)
{
	return sceKernelTryLockFastMutex(&g_rfsLock);
}
