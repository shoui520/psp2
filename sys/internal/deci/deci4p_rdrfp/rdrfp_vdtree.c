/*
 * SceDeci4pRDrfp virtual directory tree (Sony's rdrfp_vdtree.c). Besides the
 * real devices the host sees two trees, "Memory Card" and "Vita Card", each
 * with "game titles/<title>/{app0,savedata0}". Opening app0 or savedata0
 * mounts the title's data through SceAppMgr; paths below them go to the
 * mount point. Every file system call runs under a process ID and permission
 * set for the host, elevated for a title's sce_sys files.
 */
#include <iofilemgr/iofilemgr.h>
#include <sysmem/cpu.h>
#include <threadmgr/threadmgr.h>
#include <stdio.h>
#include <string.h>
#include <sysmem/sysclib.h>
#include <appmgr/appmgr_rdrfp.h>
#include "rdrfp.h"

/* the host's file system calls run as (names unknown) */
#define RDRFP_PID			0x10013
#define RDRFP_PERMISSION		0x20
#define RDRFP_PID_SCE_SYS		0x10015
#define RDRFP_PERMISSION_SCE_SYS	0x40

/* SceAppMgr mount types (names unknown) */
#define RDRFP_MOUNT_APP			0x384
#define RDRFP_MOUNT_SAVEDATA		0x385

#define RDRFP_APP_DATA_ALL_SAVEDATA	0x80

#define RDRFP_PATH_BUF			0x400

typedef SceAppMgrRdrfpMountPoint RdrfpMountPoint;
typedef SceAppMgrRdrfpPasscode RdrfpPasscode;

typedef struct RdrfpRoot {
	const char *name;
	RdrfpVDEntry *root;
	RdrfpVDEntry *titles;			/* "game titles" */
} RdrfpRoot;

typedef struct RdrfpIoContext {
	SceUID pid;
	int permission;
} RdrfpIoContext;

static RdrfpRoot rdrfpRoots[2] = {
	{ "Memory Card", NULL, NULL },
	{ "Vita Card", NULL, NULL },
};

/* ux0:data/savedata, mounted for the host whole */
static RdrfpMountPoint rdrfpSavedataMountPoint;

/* a title's sce_sys files the host reaches */
static const char *const rdrfpSceSysFiles[] = { "sdslot.dat", "safemem.dat", NULL };

static inline __attribute__((always_inline)) void beginIo(RdrfpIoContext *ctx, int elevated)
{
	if (elevated) {
		ctx->pid = SceThreadmgrForDriver_0486F239(RDRFP_PID_SCE_SYS);
		ctx->permission = sceKernelSetThreadAccessLevel(RDRFP_PERMISSION_SCE_SYS);
	} else {
		ctx->pid = SceThreadmgrForDriver_0486F239(RDRFP_PID);
		ctx->permission = sceKernelSetThreadAccessLevel(RDRFP_PERMISSION);
	}
}

static inline __attribute__((always_inline)) void endIo(const RdrfpIoContext *ctx)
{
	SceThreadmgrForDriver_0486F239(ctx->pid);
	sceKernelSetThreadAccessLevel(ctx->permission);
}

/* elevated, the others' permissions count as the owner's */
static inline void fixStatMode(SceIoStat *stat)
{
	if (stat->st_mode & IO_STM_RS)
		stat->st_mode |= SCE_STM_RU;
	if (stat->st_mode & IO_STM_WS)
		stat->st_mode |= IO_STM_WU;
}

static inline void fixChstatMode(SceIoStat *stat)
{
	SceMode mode = stat->st_mode;

	if (mode & SCE_STM_RU) {
		mode = (mode & ~SCE_STM_RU) | IO_STM_RS;
		stat->st_mode = mode;
	}
	if (mode & IO_STM_WU) {
		mode = (mode & ~IO_STM_WU) | IO_STM_WS;
		stat->st_mode = mode;
	}
}

/* ux0:data/savedata or a title's directory in it, while not mounted whole */
static inline int isSavedataDir(const char *path)
{
	const char *p;

	if (rdrfpSavedataMountPoint.path[0] != '\0' ||
	    strncmp(path, "ux0:data/savedata", 0x11) != 0)
		return 0;
	p = path[4] == '/' ? path + 5 : path + 4;
	return p[0xd] == '/' || p[0xd] == '\0';
}

/*
 * A path the host sent, checked to be UTF-8 and made canonical: "dev:" then
 * the components, without empty ones, "." and "..".
 */
static int normalizePath(const char *path, char *buf)
{
	const SceUInt8 *s = (const SceUInt8 *)path;
	const char *p, *comp;
	char *d, *root, *t;
	SceUInt32 c, code;
	SceSize len;
	int n, i;

	while ((c = *s) != 0) {
		if (!(c & 0x80)) {
			s++;
			continue;
		}
		if (c < 0xc2)
			return SCE_ERROR_ERRNO_EINVAL;
		n = __builtin_clz(~(c << 24));
		if (n > 6)
			return SCE_ERROR_ERRNO_EINVAL;
		code = c & (0xffu >> (n + 1));
		if (n <= 1) {
			s++;
		} else {
			for (i = 1; i < 6; i++) {
				if ((s[i] & 0xc0) != 0x80)
					return SCE_ERROR_ERRNO_EINVAL;
				code = (code << 6) | (s[i] & 0x3f);
				if (n == i + 1) {
					s += i + 1;
					break;
				}
			}
		}
		if (code < 0x80)
			return SCE_ERROR_ERRNO_EINVAL;
	}

	for (p = path; *p != ':' && *p != '\0'; p++)
		;
	if (*p == '\0')
		return SCE_ERROR_ERRNO_EINVAL;
	p++;
	len = p - path;
	d = buf + len;
	memcpy(buf, path, len);
	root = d - 1;
	while (*p == '/')
		p++;
	while (*p != '\0') {
		comp = p;
		while (*p != '/' && *p != '\0')
			p++;
		len = p - comp;
		while (*p == '/')
			p++;
		if (len == 1 && comp[0] == '.')
			continue;
		if (len == 2 && comp[0] == '.' && comp[1] == '.') {
			if (d - 1 == root) {
				d = root + 1;
			} else if (d[-1] == '/') {
				d--;
			} else {
				for (t = d - 2; t != root && *t != '/'; t--)
					;
				d = t == root ? root + 1 : t;
			}
			continue;
		}
		if (d - 1 != root)
			*d++ = '/';
		memcpy(d, comp, len);
		d += len;
	}
	*d = '\0';
	return 0;
}

/* whether a path is a title's sce_sys file the host may reach */
int checkSavedataSceSysAccess(const char *path)
{
	const char *const *name;
	const char *p;
	int ret;

	ret = strncmp(path, "ux0:data/savedata", 0x11);
	if (ret == 0) {
		if (rdrfpSavedataMountPoint.path[0] == '\0')
			return ret;
		p = path[4] == '/' ? path + 5 : path + 4;
		if (p[0xd] != '/')
			return ret;
		p += 0xe;
		if (strncmp(p, "sce_sys", 7) != 0)
			return ret;
	} else {
		ret = strncmp(path, "ux0:", 4);
		if (ret == 0)
			return ret;
		for (p = path; *p != '\0' && *p != ':'; p++)
			;
		if (*p == '\0') {
			SCE_DBG_ASSERT_LEVEL(0, 0, 279);
			return 0;
		}
		p++;
		if (strncmp(p, "sce_sys", 7) != 0)
			return 0;
	}
	if (p[7] == '\0')
		return 1;
	if (p[7] != '/')
		return 0;
	for (name = rdrfpSceSysFiles; *name; name++) {
		if (strncmp(p + 8, *name, 0xc) == 0)
			return 1;
	}
	return 0;
}

static int rdrfpUpdateVDTreeOfApp(void)
{
	RdrfpVDEntry *title;
	SceIoDirent dirent;
	SceUID fd;
	int ret;

	fd = sceIoDopen("ux0:app");
	if (fd < 0)
		return fd;
	memset(&dirent, 0, sizeof(dirent));
	while ((ret = sceIoDread(fd, &dirent)) > 0) {
		title = rdrfpFindVDEntry(rdrfpRoots[0].titles, dirent.d_name);
		if (title == NULL) {
			title = rdrfpAddVDEntry(rdrfpRoots[0].titles, dirent.d_name);
			if (title == NULL) {
				ret = SCE_ERROR_ERRNO_ENOMEM;
				goto failed;
			}
		}
		rdrfpUpdateVDEntryOfApp(title);
		title->visited = 1;
	}
	if (ret != 0) {
failed:
		SCE_DBG_PRINTF_WITH_INFO(0xf, 495,
				     "sceIoDread failed (0x%x), or creating title entry failed\n", ret);
	}
	sceIoDclose(fd);
	return ret;
}

static int rdrfpUpdateVDTreeOfSavedata(int which)
{
	RdrfpVDEntry *title;
	SceIoDirent dirent;
	char mount[0x20];
	char dir[0x29];
	char *path;
	SceUID fd;
	int ret;

	if (which == 0) {
		SceAppMgrForDriver_0813AFCA(2, mount, sizeof(mount));
		snprintf(dir, sizeof(dir), "%s/%s", mount, "savedata");
	} else {
		__builtin_strcpy(dir, "grw0:savedata");
	}
	fd = sceIoDopen(dir);
	if (fd < 0)
		return fd;
	path = rdrfpAlloc(RDRFP_PATH_BUF);
	if (path == NULL) {
		sceIoDclose(fd);
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	memset(&dirent, 0, sizeof(dirent));
	while ((ret = sceIoDread(fd, &dirent)) > 0) {
		title = rdrfpFindVDEntry(rdrfpRoots[which].titles, dirent.d_name);
		if (title == NULL) {
			title = rdrfpAddVDEntry(rdrfpRoots[which].titles, dirent.d_name);
			if (title == NULL) {
				ret = SCE_ERROR_ERRNO_ENOMEM;
				goto failed;
			}
		}
		snprintf(path, RDRFP_PATH_BUF, "%s/%s", dir, dirent.d_name);
		rdrfpUpdateVDEntryOfSavedata(title, path);
		title->visited = 1;
	}
	if (ret != 0) {
failed:
		SCE_DBG_PRINTF_WITH_INFO(0xf, 557,
				     "sceIoDread failed (0x%x), or creating title entry failed\n", ret);
	}
	rdrfpFree(path);
	sceIoDclose(fd);
	return ret;
}

int rdrfpVDTreeInit(void)
{
	RdrfpRoot *r = rdrfpRoots;
	RdrfpVDEntry *titles;

	__builtin_memset(&rdrfpSavedataMountPoint, 0, sizeof(rdrfpSavedataMountPoint));
	do {
		r->root = rdrfpCreateVDEntry(r->name);
		if (r->root == NULL) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 318, "couldn't create root entry\n");
			return -1;
		}
		titles = rdrfpAddVDEntry(r->root, "game titles");
		r->titles = titles;
		r++;
		if (titles == NULL) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 328, "couldn't create \"%s\" entry\n", "game titles");
			return -1;
		}
	} while (r != &rdrfpRoots[2]);
	return 0;
}

/* a title's app0 or savedata0 goes, unmounted */
static inline void rdrfpDeleteVDTreeLeaf(RdrfpList *list, RdrfpListElem *elem)
{
	RdrfpVDEntry *leaf = elem->data;
	RdrfpMountPoint mountPoint;
	int ret;

	rdrfpRemoveFromList(list, elem);
	if (rdrfpVDEntryIsMounted(leaf)) {
		mountPoint = *(RdrfpMountPoint *)leaf->mountPoint;
		ret = sceAppMgrRdrfpUmount(&mountPoint);
		if (ret < 0)
			SCE_DBG_PRINTF_WITH_INFO(0xf, 389, "sceAppMgrRdrfpUmount ret = 0x%x\n", ret);
	}
	rdrfpDeleteVDEntry(leaf);
}

/* the trees follow the titles there are now; those gone are deleted */
int rdrfpUpdateVDTree(void)
{
	RdrfpListElem *elem, *child;
	RdrfpVDEntry *title;
	RdrfpList *titles;
	int which, i, j, ret;

	for (which = 0; which < 2; which++) {
		titles = &rdrfpRoots[which].titles->children;
		for (i = 0; (elem = rdrfpGetListElem(titles, i)) != NULL; i++) {
			title = elem->data;
			title->visited = 0;
			for (j = 0; (child = rdrfpGetListElem(&title->children, j)) != NULL; j++)
				((RdrfpVDEntry *)child->data)->visited = 0;
		}
	}

	ret = rdrfpUpdateVDTreeOfApp();
	if (ret < 0 && ret != (int)SCE_ERROR_ERRNO_ENOENT && ret != (int)SCE_ERROR_ERRNO_ENODEV)
		return ret;
	for (which = 0; which < 2; which++) {
		ret = rdrfpUpdateVDTreeOfSavedata(which);
		if (ret < 0 && ret != (int)SCE_ERROR_ERRNO_ENOENT && ret != (int)SCE_ERROR_ERRNO_ENODEV)
			return ret;
		titles = &rdrfpRoots[which].titles->children;
		i = 0;
		while ((elem = rdrfpGetListElem(titles, i)) != NULL) {
			title = elem->data;
			if (!title->visited) {
				rdrfpRemoveFromList(titles, elem);
				while ((child = rdrfpGetListElem(&title->children, 0)) != NULL)
					rdrfpDeleteVDTreeLeaf(&title->children, child);
				rdrfpDeleteVDEntry(title);
				continue;
			}
			j = 0;
			while ((child = rdrfpGetListElem(&title->children, j)) != NULL) {
				if (((RdrfpVDEntry *)child->data)->visited)
					j++;
				else
					rdrfpDeleteVDTreeLeaf(&title->children, child);
			}
			i++;
		}
	}
	return 0;
}

/* the game card went: its titles go */
void rdrfpDeleteVDTreeOfGameCard(void)
{
	RdrfpList *titles = &rdrfpRoots[1].titles->children;
	RdrfpListElem *elem, *child;
	RdrfpVDEntry *title;

	while ((elem = rdrfpGetListElem(titles, 0)) != NULL) {
		title = elem->data;
		rdrfpRemoveFromList(titles, elem);
		while ((child = rdrfpGetListElem(&title->children, 0)) != NULL)
			rdrfpDeleteVDTreeLeaf(&title->children, child);
		rdrfpDeleteVDEntry(title);
	}
}

/*
 * Where a canonical path leads: a virtual directory (*entry), a path below a
 * mounted entry (*realPath, the caller frees it), or neither when the path
 * is a real device's.
 */
static int rdrfpVDTreeFindEntry(const char *path, SceSize size, RdrfpVDEntry **entry, char **realPath)
{
	RdrfpVDEntry *e, *found;
	char *buf, *p, *rest, *end, *next;
	char *real = NULL;
	int which, ret;

	buf = rdrfpAlloc(size);
	if (buf == NULL) {
		*entry = NULL;
		*realPath = NULL;
		return SCE_ERROR_ERRNO_ENOMEM;
	}
	memcpy(buf, path, size);
	buf[size - 1] = '\0';
	for (p = buf; *p != '\0' && *p != ':'; p++)
		;
	if (*p == ':') {
		rest = p + 1;
		*p = '\0';
	} else {
		rest = NULL;
	}
	if (strncmp(buf, rdrfpRoots[0].name, 0x20) == 0) {
		which = 0;
	} else if (strncmp(buf, rdrfpRoots[1].name, 0x20) == 0) {
		which = 1;
	} else {
		rdrfpFree(buf);
		*entry = NULL;
		*realPath = NULL;
		return 0;
	}
	if (*rest == '/')
		rest++;
	ret = rdrfpUpdateVDTree();
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 652, "couldn't update vdtree 0x%x\n", ret);
		*entry = NULL;
		*realPath = NULL;
		return ret;
	}

	e = rdrfpRoots[which].root;
	for (;;) {
		for (end = rest; *end != '/' && *end != '\0'; end++)
			;
		next = end;
		if (*end == '/') {
			next = end + 1;
			*end = '\0';
		}
		if (*rest == '\0') {
			found = e;
			break;
		}
		found = rdrfpFindVDEntry(e, rest);
		if (found == NULL)
			break;
		if (*next == '\0') {
			e = found;
			break;
		}
		if (rdrfpVDEntryIsMounted(found)) {
			e = found;
			goto mounted;
		}
		e = found;
		rest = next;
	}
	if (rdrfpVDEntryIsMounted(e)) {
mounted:
		found = NULL;
		ret = rdrfpVDEntryMakePath(e, next, &real);
	}
	*entry = found;
	*realPath = real;
	rdrfpFree(buf);
	return ret;
}

int rdrfpGetstat(const char *path, SceSize len, SceIoStat *stat)
{
	RdrfpVDEntry *entry = NULL;
	char *realPath = NULL;
	RdrfpIoContext ctx;
	const char *p;
	char *buf;
	int ret, elevated;

	buf = rdrfpAlloc(len);
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = normalizePath(path, buf);
	if (ret < 0)
		goto out;
	ret = rdrfpVDTreeFindEntry(buf, len, &entry, &realPath);
	if (ret < 0)
		goto out;
	if (entry) {
		beginIo(&ctx, 0);
		rdrfpVDEntryGetstat(entry, stat);
		endIo(&ctx);
		goto out;
	}
	if (realPath) {
		p = realPath;
		elevated = checkSavedataSceSysAccess(realPath) != 0;
	} else {
		p = buf;
		elevated = isSavedataDir(buf) || checkSavedataSceSysAccess(buf);
	}
	if (elevated) {
		beginIo(&ctx, 1);
		ret = sceIoGetstat(p, stat);
		endIo(&ctx);
		if (ret == 0)
			fixStatMode(stat);
	} else {
		beginIo(&ctx, 0);
		ret = sceIoGetstat(p, stat);
		endIo(&ctx);
	}
out:
	if (realPath)
		rdrfpFree(realPath);
	rdrfpFree(buf);
	return ret;
}

int rdrfpGetstatByFd(SceUInt32 fd, SceIoStat *stat)
{
	RdrfpIoContext ctx;
	RdrfpFd *f;
	int ret;

	f = rdrfpFindFd(fd);
	if (f == NULL)
		return SCE_ERROR_ERRNO_EBADF;
	if (f->uid == -1) {
		beginIo(&ctx, 0);
		rdrfpVDEntryGetstat(f->entry, stat);
		endIo(&ctx);
		return 0;
	}
	if (!rdrfpFdIsElevated(f)) {
		beginIo(&ctx, 0);
		ret = sceIoGetstatByFd(f->uid, stat);
		endIo(&ctx);
		return ret;
	}
	beginIo(&ctx, 1);
	ret = sceIoGetstatByFd(f->uid, stat);
	endIo(&ctx);
	if (ret != 0)
		return ret;
	fixStatMode(stat);
	return 0;
}

int rdrfpChstat(const char *path, SceSize len, SceIoStat *stat, int bits)
{
	RdrfpVDEntry *entry = NULL;
	char *realPath = NULL;
	RdrfpIoContext ctx;
	const char *p;
	char *buf;
	int ret, elevated;

	buf = rdrfpAlloc(len);
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = normalizePath(path, buf);
	if (ret >= 0) {
		ret = rdrfpVDTreeFindEntry(buf, len, &entry, &realPath);
		if (ret >= 0) {
			if (entry == NULL) {
				p = realPath ? realPath : buf;
				elevated = checkSavedataSceSysAccess(p);
				beginIo(&ctx, elevated);
				if (elevated)
					fixChstatMode(stat);
				ret = sceIoChstat(p, stat, bits);
				endIo(&ctx);
			} else if (bits & SCE_CST_SIZE) {
				ret = SCE_ERROR_ERRNO_EISDIR;
			}
		}
	}
	if (realPath)
		rdrfpFree(realPath);
	rdrfpFree(buf);
	return ret;
}

int rdrfpChstatByFd(SceUInt32 fd, SceIoStat *stat, int bits)
{
	RdrfpIoContext ctx;
	RdrfpFd *f;
	int ret, elevated;

	f = rdrfpFindFd(fd);
	if (f == NULL)
		return SCE_ERROR_ERRNO_EBADF;
	if (f->uid == -1)
		return (bits & SCE_CST_SIZE) ? SCE_ERROR_ERRNO_EISDIR : 0;
	elevated = rdrfpFdIsElevated(f);
	beginIo(&ctx, elevated);
	if (elevated)
		fixChstatMode(stat);
	ret = sceIoChstatByFd(f->uid, stat, bits);
	endIo(&ctx);
	return ret;
}

int rdrfpOpen(const char *path, SceSize len, int flags, SceIoMode mode, RdrfpFd **fd)
{
	RdrfpVDEntry *entry = NULL;
	char *realPath = NULL;
	RdrfpIoContext ctx;
	const char *p;
	char *buf;
	SceUID uid;
	int ret, found, elevated;
	RdrfpFd *f;

	buf = rdrfpAlloc(len);
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = normalizePath(path, buf);
	if (ret >= 0) {
		found = rdrfpVDTreeFindEntry(buf, len, &entry, &realPath);
		ret = found;
		if (found >= 0) {
			if (entry == NULL) {
				p = realPath ? realPath : buf;
				elevated = checkSavedataSceSysAccess(p) != 0;
				beginIo(&ctx, elevated);
				uid = sceIoOpen(p, flags, mode);
				endIo(&ctx);
				ret = uid;
				if (uid >= 0) {
					f = rdrfpCreateFd(entry, uid);
					*fd = f;
					ret = found;
					if (elevated)
						rdrfpSetFdFlags(f, RDRFP_FD_SCE_SYS);
				}
			} else {
				SCE_DBG_ASSERT_LEVEL(0, realPath == NULL, 959);
				ret = SCE_ERROR_ERRNO_EISDIR;
			}
		}
	}
	if (realPath)
		rdrfpFree(realPath);
	rdrfpFree(buf);
	return ret;
}

int rdrfpClose(SceUInt32 fd)
{
	RdrfpIoContext ctx;
	RdrfpFd *f;
	int ret;

	f = rdrfpFindFd(fd);
	if (f == NULL)
		return SCE_ERROR_ERRNO_EBADF;
	beginIo(&ctx, 0);
	if (f->entry == NULL)
		ret = sceIoClose(f->uid);
	else
		ret = rdrfpVDEntryDclose(f->entry);
	endIo(&ctx);
	if (ret != 0)
		return ret;
	rdrfpDeleteFd(fd);
	return 0;
}

int rdrfpRead(SceUInt32 fd, void *buf, SceSize size, SceOff offset)
{
	RdrfpIoContext ctx;
	RdrfpFd *f;
	int ret;

	f = rdrfpFindFd(fd);
	if (f == NULL)
		return SCE_ERROR_ERRNO_EBADF;
	if (f->entry)
		return SCE_ERROR_ERRNO_EISDIR;
	beginIo(&ctx, rdrfpFdIsElevated(f));
	ret = sceIoPread(f->uid, buf, size, offset);
	endIo(&ctx);
	return ret;
}

int rdrfpWrite(SceUInt32 fd, const void *buf, SceSize size, SceOff offset)
{
	RdrfpIoContext ctx;
	RdrfpFd *f;
	int ret;

	f = rdrfpFindFd(fd);
	if (f == NULL)
		return SCE_ERROR_ERRNO_EBADF;
	if (f->entry)
		return SCE_ERROR_ERRNO_EISDIR;
	beginIo(&ctx, rdrfpFdIsElevated(f));
	ret = sceIoPwrite(f->uid, buf, size, offset);
	endIo(&ctx);
	return ret;
}

int rdrfpDopen(const char *path, SceSize len, RdrfpFd **fd)
{
	RdrfpVDEntry *entry = NULL;
	char *realPath = NULL;
	RdrfpIoContext ctx;
	const char *p;
	char *buf;
	SceUID uid;
	int ret, savedataDir = 0, sceSysDir = 0;
	RdrfpFd *f;

	buf = rdrfpAlloc(len);
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = normalizePath(path, buf);
	if (ret < 0)
		goto out;
	ret = rdrfpVDTreeFindEntry(buf, len, &entry, &realPath);
	if (ret < 0)
		goto out;
	if (entry == NULL) {
		if (realPath == NULL) {
			p = buf;
			if (isSavedataDir(buf))
				savedataDir = 1;
			else
				sceSysDir = checkSavedataSceSysAccess(buf);
		} else {
			p = realPath;
			sceSysDir = checkSavedataSceSysAccess(realPath);
		}
		beginIo(&ctx, savedataDir || sceSysDir);
		uid = sceIoDopen(p);
		endIo(&ctx);
		if (uid < 0) {
			ret = uid;
			goto out;
		}
	} else {
		beginIo(&ctx, 0);
		ret = rdrfpVDEntryDopen(entry);
		endIo(&ctx);
		if (ret < 0)
			goto out;
		uid = -1;
		p = buf;
	}
	f = rdrfpCreateFd(entry, uid);
	*fd = f;
	if (strncmp(p, "ux0:/", strnlen(p, 8)) == 0)
		rdrfpSetFdFlags(f, RDRFP_FD_ROOT_DIR);
	if (savedataDir)
		rdrfpSetFdFlags(*fd, RDRFP_FD_SAVEDATA_DIR);
	if (sceSysDir)
		rdrfpSetFdFlags(*fd, RDRFP_FD_SCE_SYS_DIR);
out:
	if (realPath)
		rdrfpFree(realPath);
	rdrfpFree(buf);
	return ret;
}

int rdrfpDclose(SceUInt32 fd)
{
	RdrfpIoContext ctx;
	RdrfpFd *f;
	int ret;

	f = rdrfpFindFd(fd);
	if (f == NULL)
		return SCE_ERROR_ERRNO_EBADF;
	beginIo(&ctx, 0);
	if (f->entry == NULL)
		ret = sceIoDclose(f->uid);
	else
		ret = rdrfpVDEntryDclose(f->entry);
	endIo(&ctx);
	if (ret != 0)
		return ret;
	rdrfpDeleteFd(fd);
	return 0;
}

/* elevated listings show only the sce_sys files the host may reach */
int rdrfpDread(SceUInt32 fd, SceIoDirent *dirent)
{
	const char *const *name;
	RdrfpIoContext ctx;
	RdrfpFd *f;
	int ret;

	f = rdrfpFindFd(fd);
	if (f == NULL)
		return SCE_ERROR_ERRNO_EBADF;
	beginIo(&ctx, rdrfpFdIsElevated(f));
	if (f->entry) {
		ret = rdrfpVDEntryDread(f->entry, f->readIndex, dirent);
		if (ret > 0)
			SceCpuForDriver_5F6A8743(&f->readIndex, 1);
		goto out;
	}
	for (;;) {
		memset(dirent, 0, sizeof(*dirent));
		ret = sceIoDread(f->uid, dirent);
		if (ret <= 0)
			break;
		if (rdrfpFdIsRootDir(f) &&
		    (dirent->d_stat.st_mode & (SCE_STM_FMT | SCE_STM_RU)) == SCE_STM_FDIR)
			continue;
		if (!rdrfpFdIsElevated(f))
			break;
		if (!rdrfpFdIsSceSysDir(f)) {
			fixStatMode(&dirent->d_stat);
			break;
		}
		for (name = rdrfpSceSysFiles; *name; name++) {
			if (strncmp(dirent->d_name, *name, 0xc) == 0)
				break;
		}
		if (*name) {
			fixStatMode(&dirent->d_stat);
			break;
		}
	}
out:
	endIo(&ctx);
	return ret;
}

int rdrfpRmdir(const char *path, SceSize len)
{
	RdrfpVDEntry *entry = NULL;
	char *realPath = NULL;
	RdrfpIoContext ctx;
	const char *p;
	char *buf;
	int ret, elevated;

	buf = rdrfpAlloc(len);
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = normalizePath(path, buf);
	if (ret >= 0) {
		ret = rdrfpVDTreeFindEntry(buf, len, &entry, &realPath);
		if (ret >= 0) {
			ret = SCE_ERROR_ERRNO_EACCES;
			if (entry == NULL) {
				if (realPath == NULL) {
					p = buf;
					elevated = isSavedataDir(buf) || checkSavedataSceSysAccess(buf);
				} else {
					p = realPath;
					elevated = checkSavedataSceSysAccess(realPath);
				}
				beginIo(&ctx, elevated);
				ret = sceIoRmdir(p);
				endIo(&ctx);
			}
		}
	}
	if (realPath)
		rdrfpFree(realPath);
	rdrfpFree(buf);
	return ret;
}

int rdrfpRemove(const char *path, SceSize len)
{
	RdrfpVDEntry *entry = NULL;
	char *realPath = NULL;
	RdrfpIoContext ctx;
	const char *p;
	char *buf;
	int ret, elevated;

	buf = rdrfpAlloc(len);
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = normalizePath(path, buf);
	if (ret >= 0) {
		ret = rdrfpVDTreeFindEntry(buf, len, &entry, &realPath);
		if (ret >= 0) {
			ret = SCE_ERROR_ERRNO_EISDIR;
			if (entry == NULL) {
				if (realPath == NULL) {
					p = buf;
					elevated = isSavedataDir(buf) || checkSavedataSceSysAccess(buf);
				} else {
					p = realPath;
					elevated = checkSavedataSceSysAccess(realPath);
				}
				beginIo(&ctx, elevated);
				ret = sceIoRemove(p);
				endIo(&ctx);
			}
		}
	}
	if (realPath)
		rdrfpFree(realPath);
	rdrfpFree(buf);
	return ret;
}

/* the parent directory of a canonical path, and its last component */
static inline char *splitParent(char *parent, const char *path, SceSize len)
{
	char *last, *q;
	SceSize n;

	memcpy(parent, path, len);
	parent[len - 1] = '\0';
	n = strnlen(parent, len);
	last = parent + n;
	if (parent[n - 1] == '/')
		parent[n - 1] = '\0';
	for (q = parent; *q != '\0'; q++) {
		if (*q == '/')
			last = q;
	}
	*last = '\0';
	return last;
}

int rdrfpMkdir(const char *path, SceSize len, SceIoMode mode)
{
	RdrfpVDEntry *entry = NULL;
	char *realPath = NULL;
	char *buf, *parent = NULL, *tmp = NULL, *last;
	RdrfpIoContext ctx;
	const char *p;
	int ret, elevated;

	buf = rdrfpAlloc(len);
	if (buf == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	ret = normalizePath(path, buf);
	if (ret >= 0) {
		parent = rdrfpAlloc(len);
		tmp = rdrfpAlloc(RDRFP_PATH_BUF);
		if (parent == NULL || tmp == NULL) {
			ret = SCE_ERROR_ERRNO_ENOMEM;
		} else {
			last = splitParent(parent, buf, len);
			ret = rdrfpVDTreeFindEntry(parent, strnlen(parent, len) + 1, &entry, &realPath);
			if (ret >= 0) {
				ret = SCE_ERROR_ERRNO_EEXIST;
				if (entry == NULL) {
					p = buf;
					if (realPath) {
						snprintf(tmp, RDRFP_PATH_BUF, "%s/%s", realPath, last + 1);
						p = tmp;
					}
					elevated = checkSavedataSceSysAccess(p);
					beginIo(&ctx, elevated);
					ret = sceIoMkdir(p, mode);
					endIo(&ctx);
				}
			}
		}
	}
	if (realPath)
		rdrfpFree(realPath);
	if (tmp)
		rdrfpFree(tmp);
	if (parent)
		rdrfpFree(parent);
	rdrfpFree(buf);
	return ret;
}

int rdrfpRename(const char *oldPath, SceSize oldLen, const char *newPath, SceSize newLen)
{
	RdrfpVDEntry *oldEntry = NULL, *newEntry = NULL;
	char *oldReal = NULL, *newReal = NULL;
	char *oldBuf, *newBuf, *parent = NULL, *tmp = NULL, *last;
	RdrfpIoContext ctx;
	const char *from, *to;
	int ret;

	oldBuf = rdrfpAlloc(oldLen);
	newBuf = rdrfpAlloc(newLen);
	if (oldBuf == NULL || newBuf == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	ret = normalizePath(oldPath, oldBuf);
	if (ret < 0)
		goto out;
	ret = normalizePath(newPath, newBuf);
	if (ret < 0)
		goto out;
	ret = rdrfpVDTreeFindEntry(oldBuf, oldLen, &oldEntry, &oldReal);
	if (ret < 0)
		goto out;
	if (oldEntry) {
		ret = SCE_ERROR_ERRNO_EACCES;
		goto out;
	}
	parent = rdrfpAlloc(newLen);
	tmp = rdrfpAlloc(RDRFP_PATH_BUF);
	if (parent == NULL || tmp == NULL) {
		ret = SCE_ERROR_ERRNO_ENOMEM;
		goto out;
	}
	last = splitParent(parent, newBuf, newLen);
	ret = rdrfpVDTreeFindEntry(parent, strnlen(parent, newLen) + 1, &newEntry, &newReal);
	if (ret < 0)
		goto out;
	if (newEntry == NULL) {
		from = oldReal ? oldReal : oldBuf;
		if (!checkSavedataSceSysAccess(from)) {
			beginIo(&ctx, 0);
			to = newBuf;
			if (newReal) {
				snprintf(tmp, RDRFP_PATH_BUF, "%s/%s", newReal, last + 1);
				to = tmp;
			}
			ret = sceIoRename(from, to);
			endIo(&ctx);
			goto out;
		}
	}
	ret = SCE_ERROR_ERRNO_EACCES;
out:
	if (oldReal)
		rdrfpFree(oldReal);
	if (newReal)
		rdrfpFree(newReal);
	if (tmp)
		rdrfpFree(tmp);
	if (parent)
		rdrfpFree(parent);
	if (newBuf)
		rdrfpFree(newBuf);
	if (oldBuf)
		rdrfpFree(oldBuf);
	return ret;
}

/* a title's app0 and savedata0 mounted for the host, or all of ux0:data/savedata */
int rdrfpMountAppData(int type, const char *titleId, void *arg)
{
	const RdrfpPasscode *passcodeArg = arg;
	RdrfpVDEntry *title, *app, *savedata;
	RdrfpMountPoint mountPoint;
	RdrfpPasscode passcode;
	char mount[0x20];
	int ret, gameCard;
	char *path;

	if (type == RDRFP_APP_DATA_ALL_SAVEDATA) {
		ret = sceAppMgrRdrfpMount(RDRFP_MOUNT_SAVEDATA, "ux0:data/savedata", NULL,
						  &mountPoint);
		if (ret < 0)
			SCE_DBG_PRINTF_WITH_INFO(0xf, 1687, "sceAppMgrRdrfpMount ret = 0x%x\n", ret);
		else
			rdrfpSavedataMountPoint = mountPoint;
		return ret;
	}
	ret = rdrfpUpdateVDTree();
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1700, "couldn't update vdtree 0x%x\n", ret);
		return ret;
	}
	title = rdrfpFindVDEntry(rdrfpRoots[0].titles, titleId);
	if (title == NULL) {
		title = rdrfpFindVDEntry(rdrfpRoots[1].titles, titleId);
		if (title == NULL)
			return SCE_ERROR_ERRNO_ENOENT;
		path = rdrfpAlloc(RDRFP_PATH_BUF);
		if (path == NULL)
			return SCE_ERROR_ERRNO_ENOMEM;
		gameCard = 1;
		app = NULL;
		goto savedata;
	}
	path = rdrfpAlloc(RDRFP_PATH_BUF);
	if (path == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	app = rdrfpFindVDEntry(title, "app0");
	gameCard = 0;
	if (app) {
		if (rdrfpVDEntryIsMounted(app)) {
			ret = SCE_ERROR_ERRNO_EBUSY;
			goto out;
		}
		snprintf(path, RDRFP_PATH_BUF, "ux0:app/%s", titleId);
		passcode = *passcodeArg;
		ret = sceAppMgrRdrfpMount(RDRFP_MOUNT_APP, path, &passcode, &mountPoint);
		if (ret < 0) {
			if (ret != (int)SCE_ERROR_ERRNO_EICV) {
				SCE_DBG_PRINTF_WITH_INFO(0xf, 1741, "sceAppMgrRdrfpMount ret = 0x%x\n", ret);
				goto out;
			}
			ret = SCE_ERROR_ERRNO_ENOENT;
		} else {
			rdrfpVDEntrySetMountPoint(app, mountPoint.path);
		}
	}

savedata:
	savedata = rdrfpFindVDEntry(title, "savedata0");
	if (savedata == NULL) {
		if (app == NULL)
			ret = SCE_ERROR_ERRNO_ENOENT;
		goto out;
	}
	if (rdrfpVDEntryIsMounted(savedata)) {
		ret = SCE_ERROR_ERRNO_EBUSY;
		goto out;
	}
	if (gameCard) {
		snprintf(path, RDRFP_PATH_BUF, "%s/%s", "grw0:savedata", titleId);
	} else {
		SceAppMgrForDriver_0813AFCA(2, mount, sizeof(mount));
		snprintf(path, RDRFP_PATH_BUF, "%s/savedata/%s", mount, titleId);
	}
	passcode = *passcodeArg;
	ret = sceAppMgrRdrfpMount(RDRFP_MOUNT_SAVEDATA, path, &passcode, &mountPoint);
	if (ret < 0)
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1775, "sceAppMgrRdrfpMount ret = 0x%x\n", ret);
	else
		rdrfpVDEntrySetMountPoint(savedata, mountPoint.path);
out:
	rdrfpFree(path);
	return ret;
}

int rdrfpUmountAppData(int type, const char *titleId)
{
	RdrfpVDEntry *title, *app, *savedata;
	RdrfpMountPoint mountPoint;
	int ret;

	if (type == RDRFP_APP_DATA_ALL_SAVEDATA) {
		ret = 0;
		if (rdrfpSavedataMountPoint.path[0] != '\0') {
			mountPoint = rdrfpSavedataMountPoint;
			ret = sceAppMgrRdrfpUmount(&mountPoint);
			if (ret < 0)
				SCE_DBG_PRINTF_WITH_INFO(0xf, 1812, "sceAppMgrRdrfpMount ret = 0x%x\n", ret);
			else
				__builtin_memset(&rdrfpSavedataMountPoint, 0, sizeof(rdrfpSavedataMountPoint));
		}
		return ret;
	}
	title = rdrfpFindVDEntry(rdrfpRoots[0].titles, titleId);
	if (title == NULL) {
		title = rdrfpFindVDEntry(rdrfpRoots[1].titles, titleId);
		if (title == NULL)
			return SCE_ERROR_ERRNO_ENOENT;
		ret = 0;
		app = NULL;
		goto savedata;
	}
	app = rdrfpFindVDEntry(title, "app0");
	ret = 0;
	if (app && rdrfpVDEntryIsMounted(app)) {
		mountPoint = *(RdrfpMountPoint *)app->mountPoint;
		ret = sceAppMgrRdrfpUmount(&mountPoint);
		if (ret < 0) {
			SCE_DBG_PRINTF_WITH_INFO(0xf, 1843, "sceAppMgrRdrfpUmount ret = 0x%x\n", ret);
			return ret;
		}
		rdrfpVDEntryClearMountPoint(app);
	}

savedata:
	savedata = rdrfpFindVDEntry(title, "savedata0");
	if (savedata == NULL)
		return app ? ret : (int)SCE_ERROR_ERRNO_ENOENT;
	if (!rdrfpVDEntryIsMounted(savedata))
		return ret;
	mountPoint = *(RdrfpMountPoint *)savedata->mountPoint;
	ret = sceAppMgrRdrfpUmount(&mountPoint);
	if (ret < 0) {
		SCE_DBG_PRINTF_WITH_INFO(0xf, 1861, "sceAppMgrRdrfpUmount ret = 0x%x\n", ret);
		return ret;
	}
	rdrfpVDEntryClearMountPoint(savedata);
	return ret;
}
