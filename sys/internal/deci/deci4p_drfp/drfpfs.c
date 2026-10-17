/*
 * SceDeci4pDrfp: "deci4p_drfp_dev_fs", the host0: file system. Every vnode
 * operation is a DRFP request to the host (Sony's drfpfs.c).
 */

#include <psp2kern/kernel/sysclib.h>
#include <psp2kern/kernel/sysmem/heap.h>
#include <psp2kern/vfs.h>
#include <kernel/error.h>
#include <deci4p_drfp/deci4p_fs.h>
#include "drfp_internal.h"

#define DRFP_FS_NAME		"deci4p_drfp_dev_fs"
#define DRFP_DEVCTL_TRANSLATE_PATH 0x5400

#define DRFP_CST_TIMES		(SCE_CST_CT | SCE_CST_AT | SCE_CST_MT)

/* the lookup's path (Iofilemgr's VfsLookupPath); the host gets the element */
typedef struct DrfpLookupPath {
	SceVfsPath path;
	const char *elem;
} DrfpLookupPath;

/* SCE_DEVCTL 0x5400 (translate path) arguments */
typedef struct DrfpTranslateArg {
	const char *base;
	SceSize baseLength;
	const char *path;
	SceSize pathLength;
} DrfpTranslateArg;

typedef struct DrfpTranslateBuf {
	char *path;
	SceSize len;
} DrfpTranslateBuf;

/* name unknown: host time to SceDateTime (out of line here) */
static void drfpUsecToDateTime(SceUInt64 usec, SceDateTime *dt)
{
	deci4pFsUsecToDateTime(usec, dt);
}

static inline __attribute__((always_inline)) int drfpDateTimeValid(const SceDateTime *dt)
{
	return dt->year != 0 && (SceUInt32)dt->month - 1 < 12 &&
	       (SceUInt32)dt->day - 1 < 31 && dt->hour < 24 &&
	       dt->minute < 60 && dt->second < 60 && dt->microsecond < DECI4P_FS_USEC_PER_SEC;
}

/* host mode to SceIoStat.st_mode */
static inline __attribute__((always_inline)) SceMode drfpStatMode(SceUInt32 mode)
{
	SceMode m;

	m = (mode & DECI4P_FS_MODE_IFREG) ? SCE_S_IFREG : 0;
	if (mode & DECI4P_FS_MODE_IFDIR)
		m |= SCE_S_IFDIR;
	if (mode & DECI4P_FS_MODE_IRUSR)
		m |= SCE_S_IRUSR | SCE_S_IRSYS;
	if (mode & DECI4P_FS_MODE_IWUSR)
		m |= SCE_S_IRUSR | SCE_S_IWUSR | SCE_S_IRSYS | SCE_S_IWSYS;
	return m;
}

/* SceIoStat.st_mode of a new file to the host mode */
static inline __attribute__((always_inline)) int drfpHostMode(int mode)
{
	if (!(mode & (SCE_S_IRUSR | SCE_S_IWUSR | SCE_S_IRSYS | SCE_S_IWSYS)))
		return 0;
	if (mode & (SCE_S_IRUSR | SCE_S_IRSYS))
		return DECI4P_FS_MODE_IRUSR | DECI4P_FS_MODE_IWUSR;
	return DECI4P_FS_MODE_IWUSR;
}

static inline void drfpStatToIoStat(const Deci4pFsStat *st, SceIoStat *stat)
{
	stat->st_mode = drfpStatMode(st->mode);
	stat->st_size = st->size;
	drfpUsecToDateTime(st->atime, &stat->st_atime);
	drfpUsecToDateTime(st->mtime, &stat->st_mtime);
	drfpUsecToDateTime(st->ctime, &stat->st_ctime);
}

static inline void drfpSetVnodeFile(SceVfsVnode *vp, const Deci4pFsStat *st)
{
	if (st->fid == 0) {
		vp->core.fid[0] = (SceUInt32)vp;
		vp->core.fid[1] = 0;
	} else {
		vp->core.fid[0] = st->fid;
		vp->core.fid[1] = st->fid >> 32;
	}
}

static inline int drfpIsRoot(const SceVfsVnode *vp)
{
	return (vp->core.type & SCE_VNODE_TYPE_ROOTDIR) == SCE_VNODE_TYPE_ROOTDIR;
}

static inline void drfpStatZero(Deci4pFsStat *st)
{
	st->mode = 0;
	st->dev = 0;
	st->fid = 0;
	st->size = 0;
	st->atime = 0;
	st->mtime = 0;
	st->ctime = 0;
}

/* name unknown */
static int dev_drfp_vfs_init(SceVfsOpInitArgs *argp)
{
	return 0;
}

/* name unknown */
static int dev_drfp_vfs_umount(SceVfsOpUmountArgs *argp)
{
	return 0;
}

/* name unknown */
static int dev_drfp_vfs_set_root(SceVfsOpSetRootArgs *argp)
{
	SceVfsMount *mnt = argp->mnt;
	SceVfsVnode *vp = argp->vp;

	vp->core.type = SCE_VNODE_TYPE_ROOTDIR;
	vp->core.node_data = mnt->data;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.mnt = mnt;
	vp->core.dd = NULL;
	vp->core.fid[0] = 0;
	vp->core.fid[1] = 0;
	vp->core.size = 0;
	vp->core.acl_data[0] = 0;
	vp->core.acl_data[1] = 0;
	return 0;
}

/* name unknown */
static int dev_drfp_vfs_sync(SceVfsOpSyncArgs *argp)
{
	return 0;
}

/* name unknown */
static int dev_drfp_vop_inactive(SceVopInactiveArgs *argp)
{
	return 0;
}

/* name unknown */
static int dev_drfp_vop_sync(SceVopSyncArgs *argp)
{
	return 0;
}

/* name unknown */
static SceSSize dev_drfp_vop_pwrite(SceVopPwriteArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	SceOff offset = argp->offset;
	SceSize nbyte = argp->nbyte;
	SceOff end;
	int ret;

	ret = sceDeci4pDrfpWrite(argp->file->fd, argp->buf, offset, nbyte);
	if (ret >= 0) {
		end = offset + nbyte;
		if (end < (SceOff)vp->core.size)
			end = vp->core.size;
		vp->core.size = end;
	}
	return ret;
}

/* name unknown */
static SceSSize dev_drfp_vop_write(SceVopWriteArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	SceVfsFile *file = argp->file;
	SceOff position, size;
	int ret;

	ret = sceDeci4pDrfpWrite(file->fd, argp->buf, file->position, argp->nbyte);
	if (ret >= 0) {
		position = file->position;
		size = vp->core.size;
		position += ret;
		file->position = position;
		if (size < position)
			vp->core.size = position;
	}
	return ret;
}

/* name unknown */
static SceSSize dev_drfp_vop_pread(SceVopPreadArgs *argp)
{
	return sceDeci4pDrfpRead(argp->file->fd, argp->buf, argp->offset, argp->nbyte);
}

/* name unknown */
static SceSSize dev_drfp_vop_read(SceVopReadArgs *argp)
{
	SceVfsFile *file = argp->file;
	int ret;

	ret = sceDeci4pDrfpRead(file->fd, argp->buf, file->position, argp->nbyte);
	if (ret >= 0)
		file->position += ret;
	return ret;
}

/* name unknown */
static int dev_drfp_vfs_mount(SceVfsOpMountArgs *argp)
{
	SceVfsMount *mnt = argp->mnt;

	mnt->allocator = drfpGetHeap();
	mnt->available_entry_num = 0x80;
	mnt->default_io_cache_size = 0x10000;
	return 0;
}

/* name unknown */
static int dev_drfp_vop_dclose(SceVopDcloseArgs *argp)
{
	int ret;

	ret = sceDeci4pDrfpDclose(argp->file->fd);
	if (ret == SCE_ERROR_ERRNO_ENODEV)
		ret = 0;
	return ret;
}

/* name unknown */
static int dev_drfp_vop_dopen(SceVopDopenAgrs *argp)
{
	SceVfsFile *file = argp->file;
	const char *path = argp->path->name;
	int ret;

	if (drfpIsRoot(argp->vp))
		path = "";
	ret = sceDeci4pDrfpDopen(path);
	if (ret >= 0) {
		file->fd = ret;
		ret = 0;
	}
	return ret;
}

/* name unknown */
static int dev_drfp_vop_rmdir(SceVopRmdirArgs *argp)
{
	return sceDeci4pDrfpRmdir(argp->path->name);
}

/* name unknown */
static int dev_drfp_vop_remove(SceVopRemoveArgs *argp)
{
	return sceDeci4pDrfpRemove(argp->path->name);
}

static SceOff dev_drfp_vop_lseek(SceVopLseekArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	SceVfsFile *file = argp->file;
	SceOff offset = argp->offset;

	switch (argp->whence) {
	case SCE_SEEK_SET:
		break;
	case SCE_SEEK_CUR:
		offset += file->position;
		break;
	case SCE_SEEK_END:
		offset += vp->core.size;
		break;
	default:
		SCE_DBG_ASSERT(0, 846);
		return file->position;
	}
	file->position = offset;
	return offset;
}

/* name unknown */
static int dev_drfp_vop_close(SceVopCloseArgs *argp)
{
	int ret;

	ret = sceDeci4pDrfpClose(argp->file->fd);
	if (ret == SCE_ERROR_ERRNO_ENODEV)
		ret = 0;
	return ret;
}

/* name unknown: 0x5400 asks the host how it resolves a path */
static int dev_drfp_vfs_devctl(SceVfsOpDevctlArg *argp)
{
	const DrfpTranslateArg *in = argp->arg;
	DrfpTranslateBuf *out = argp->buf;
	SceSize len = 0;
	int ret;

	if (argp->cmd != DRFP_DEVCTL_TRANSLATE_PATH)
		return SCE_ERROR_ERRNO_EUNSUP;
	if (argp->arg_len != sizeof(*in) || argp->buf_len != sizeof(*out))
		return SCE_ERROR_ERRNO_EINVAL;
	ret = sceDeci4pDrfpTranslatePath(in->base, in->path, DRFP_PATH_MAX, out->path, &len);
	if (ret == 0) {
		out->len = len - 1;
	} else if (ret == SCE_ERROR_ERRNO_ENOTSUP) {
		/* an old host: the path stays as it is */
		strncpy(out->path, in->path, DRFP_PATH_MAX);
		out->len = strnlen(out->path, DRFP_PATH_MAX);
		ret = 0;
	}
	return ret;
}

static int dev_drfp_vop_rename(SceVopRenameArgs *argp)
{
	SceVfsVnode **nvpp = argp->nvpp;
	SceVfsVnode *dvp, *vp;
	SceVfsMount *mnt;
	Deci4pFsStat st = {0};
	int ret;

	ret = sceDeci4pDrfpRename(argp->old_path->name, argp->new_path->name, &st);
	if (ret != 0)
		return ret;
	dvp = argp->ndvp;
	if (dvp == NULL)
		dvp = argp->odvp;
	mnt = dvp->core.mnt;
	ret = vfsGetNewVnode(mnt, (SceVopTable *)mnt->mnt_vfs_inf->default_vops, 0, &vp);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 1200, "vfsGetNewVnode failed (0x%08x)\n", ret);
		return ret;
	}
	vfsLockVnode(vp);
	vp->core.mnt = mnt;
	vp->core.dd = dvp;
	if (st.mode & DECI4P_FS_MODE_IFREG) {
		vp->core.type = SCE_VNODE_TYPE_REG;
	} else if (st.mode & DECI4P_FS_MODE_IFDIR) {
		vp->core.type = SCE_VNODE_TYPE_DIR;
	} else {
		DRFP_PRINTF(0xf, 1212, "unknown file type 0x%x\n", st.mode);
		vp->core.type = SCE_VNODE_TYPE_REG;
	}
	drfpSetVnodeFile(vp, &st);
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.size = st.size;
	vp->core.node_data = NULL;
	*nvpp = vp;
	return ret;
}

static int dev_drfp_vop_lookup(SceVopLookupArgs *argp)
{
	SceVfsVnode *dvp = argp->dvp;
	SceVfsVnode **vpp = argp->vpp;
	SceVfsVnode *vp;
	Deci4pFsStat st;
	int ret;

	drfpStatZero(&st);
	ret = sceDeci4pDrfpStat(((const DrfpLookupPath *)argp->path)->elem, &st);
	if (ret != 0)
		return ret;
	ret = vfsGetNewVnode(dvp->core.mnt, (SceVopTable *)dvp->core.mnt->mnt_vfs_inf->default_vops,
			     0, &vp);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 603, "vfsGetNewVnode failed (0x%08x)\n", ret);
		return ret;
	}
	vfsLockVnode(vp);
	vp->core.mnt = dvp->core.mnt;
	vp->core.dd = dvp;
	if (st.mode & DECI4P_FS_MODE_IFREG) {
		vp->core.type = SCE_VNODE_TYPE_REG;
	} else if (st.mode & DECI4P_FS_MODE_IFDIR) {
		vp->core.type = SCE_VNODE_TYPE_DIR;
	} else {
		DRFP_PRINTF(0xf, 615, "unknown file type 0x%x\n", st.mode);
		vp->core.type = SCE_VNODE_TYPE_REG;
	}
	drfpSetVnodeFile(vp, &st);
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.node_data = NULL;
	vp->core.size = st.size;
	*vpp = vp;
	return 0;
}

static int dev_drfp_vop_mkdir(SceVopMkdirArgs *argp)
{
	SceVfsVnode *dvp = argp->dvp;
	SceVfsVnode **vpp = argp->vpp;
	SceVfsVnode *vp;
	Deci4pFsStat st;
	int ret;

	drfpStatZero(&st);
	ret = sceDeci4pDrfpMkdir(argp->path->name, argp->mode, &st);
	if (ret < 0)
		return ret;
	ret = vfsGetNewVnode(dvp->core.mnt, (SceVopTable *)dvp->core.mnt->mnt_vfs_inf->default_vops,
			     0, &vp);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 885, "vfsGetNewVnode failed (0x%08x)\n", ret);
		return ret;
	}
	vfsLockVnode(vp);
	vp->core.mnt = dvp->core.mnt;
	vp->core.dd = dvp;
	vp->core.type = SCE_VNODE_TYPE_DIR;
	drfpSetVnodeFile(vp, &st);
	vp->core.size = st.size;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.node_data = NULL;
	*vpp = vp;
	return ret;
}

static int dev_drfp_vop_create(SceVopCreateArgs *argp)
{
	SceVfsVnode *dvp = argp->dvp;
	SceVfsVnode **vpp = argp->vpp;
	const char *name = argp->path->name;
	int flags = argp->flags;
	int mode = argp->mode;
	SceVfsVnode *vp;
	Deci4pFsStat st;
	int ret;

	if (!(flags & SCE_O_CREAT)) {
		flags |= SCE_O_CREAT;
		DRFP_PRINTF(0xf, 652, "flag for create doesn't contain SCE_O_CREAT?\n");
	}
	drfpStatZero(&st);
	ret = sceDeci4pDrfpCreate(name, drfpHostMode(mode), &st);
	if (ret < 0 && ret != SCE_ERROR_ERRNO_ENOTSUP) {
		DRFP_PRINTF(0xf, 662, "Create('%s', 0%o) failed (0x%08x)\n", name, mode, ret);
		return ret;
	}
	if (ret == SCE_ERROR_ERRNO_ENOTSUP) {
		/* an old host: create by opening */
		ret = sceDeci4pDrfpOpen(name, flags & (SCE_O_RDWR | SCE_O_APPEND | SCE_O_CREAT |
						       SCE_O_TRUNC | SCE_O_EXCL),
					drfpHostMode(mode), &st);
		if (ret < 0) {
			DRFP_PRINTF(0xf, 671, "Open('%s',0%o) for create failed (0x%08x)\n", name,
				    mode, SCE_ERROR_ERRNO_ENOTSUP);
			return ret;
		}
		sceDeci4pDrfpClose(ret);
	}
	ret = vfsGetNewVnode(dvp->core.mnt, (SceVopTable *)dvp->core.mnt->mnt_vfs_inf->default_vops,
			     0, &vp);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 682, "vfsGetNewVnode failed (0x%08x)\n", ret);
		return ret;
	}
	vfsLockVnode(vp);
	vp->core.type = SCE_VNODE_TYPE_REG;
	vp->core.mnt = dvp->core.mnt;
	vp->core.dd = dvp;
	drfpSetVnodeFile(vp, &st);
	vp->core.size = st.size;
	vp->core.node_data = NULL;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	*vpp = vp;
	return 0;
}

/* name unknown: the host names the path element itself, by file id when it has one */
static int dev_drfp_vfs_decode_path_elem(SceVfsOpDecodePathElemArgs *argp)
{
	const char *path = argp->path;
	Deci4pFsStat st;
	SceSize len;
	int ret;

	drfpStatZero(&st);
	ret = sceDeci4pDrfpStat(path, &st);
	if (ret != 0)
		return ret;
	len = strnlen(path, DRFP_PATH_MAX);
	*argp->decode_len = len;
	*argp->path2 = path;
	*argp->path3 = path + len;
	if (st.fid == 0) {
		strncpy(argp->buf, path, argp->buf_len);
	} else {
		snprintf(argp->buf, argp->buf_len, "dev0x%08x/node0x%016llx", st.dev, st.fid);
		*argp->decode_len = strnlen(argp->buf, DRFP_PATH_MAX);
	}
	return ret;
}

/* name unknown */
static int dev_drfp_vop_getstat(SceVopGetstatArgs *argp)
{
	const char *path = argp->path->name;
	Deci4pFsStat st;
	int ret;

	if (drfpIsRoot(argp->vp))
		path = "";
	drfpStatZero(&st);
	ret = sceDeci4pDrfpStat(path, &st);
	if (ret == 0)
		drfpStatToIoStat(&st, argp->stat);
	return ret;
}

/* name unknown */
static int dev_drfp_vop_open(SceVopOpenArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	SceVfsFile *file = argp->file;
	int flags = argp->flags;
	int hostFlags;
	Deci4pFsStat st;
	int ret;

	/* the host always opens for reading */
	hostFlags = (flags & SCE_O_WRONLY) ? SCE_O_RDWR : SCE_O_RDONLY;
	if (flags & SCE_O_APPEND)
		hostFlags |= SCE_O_APPEND;
	if (flags & SCE_O_TRUNC)
		hostFlags |= SCE_O_TRUNC;
	drfpStatZero(&st);
	ret = sceDeci4pDrfpOpen(argp->path->name, hostFlags, 0, &st);
	if (ret >= 0) {
		file->fd = ret;
		ret = 0;
		drfpSetVnodeFile(vp, &st);
		vp->core.size = st.size;
	}
	return ret;
}

/* name unknown */
static int dev_drfp_vop_fgetstat(SceVopFgetstatArgs *argp)
{
	Deci4pFsStat st;
	int ret;

	drfpStatZero(&st);
	ret = sceDeci4pDrfpFstat(argp->file->fd, &st);
	if (ret == 0)
		drfpStatToIoStat(&st, argp->stat);
	return ret;
}

/* name unknown */
static int dev_drfp_vop_dread(SceVopDreadArgs *argp)
{
	SceIoDirent *dir = argp->dir;
	DrfpDirent *ent = NULL;
	SceSize len;
	int ret;

	ret = sceDeci4pDrfpDread(argp->file->fd, &ent);
	if (ret > 0) {
		drfpStatToIoStat(&ent->stat, &dir->d_stat);
		len = ent->nameLen;
		if (len > sizeof(dir->d_name))
			len = sizeof(dir->d_name);
		memcpy(dir->d_name, ent->name, len);
		dir->d_name[sizeof(dir->d_name) - 1] = '\0';
	}
	if (ent != NULL)
		ksceKernelFreeHeapMemory(drfpGetHeap(), ent);
	return ret;
}

/* the times to set, checked before anything changes */
static inline __attribute__((always_inline))
int drfpChstatTimesValid(const SceIoStat *stat, SceUInt32 bit)
{
	if (!(bit & DRFP_CST_TIMES))
		return 1;
	return drfpDateTimeValid(&stat->st_ctime) && drfpDateTimeValid(&stat->st_mtime) &&
	       drfpDateTimeValid(&stat->st_atime);
}

static inline __attribute__((always_inline))
void drfpChstatTimes(Deci4pFsStat *st, const SceIoStat *stat, SceUInt32 bit)
{
	drfpStatZero(st);
	if (bit & SCE_CST_CT)
		st->ctime = deci4pFsDateTimeToUsec(&stat->st_ctime);
	if (bit & SCE_CST_MT)
		st->mtime = deci4pFsDateTimeToUsec(&stat->st_mtime);
	if (bit & SCE_CST_AT)
		st->atime = deci4pFsDateTimeToUsec(&stat->st_atime);
}

static int dev_drfp_vop_chstat(SceVopChstatArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	const SceIoStat *stat = argp->stat;
	SceUInt32 bit = argp->bit;
	const char *path = argp->path->name;
	Deci4pFsStat st;
	int ret;

	if (!drfpChstatTimesValid(stat, bit))
		return SCE_ERROR_ERRNO_EINVAL;
	if (drfpIsRoot(vp))
		path = "";
	ret = 0;
	if (bit & SCE_CST_SIZE) {
		ret = sceDeci4pDrfpTruncate(path, stat->st_size);
		if (ret < 0)
			return ret;
		vp->core.size = stat->st_size;
	}
	if (bit & DRFP_CST_TIMES) {
		drfpChstatTimes(&st, stat, bit);
		ret = sceDeci4pDrfpChstat(path, bit, &st);
		if (ret == SCE_ERROR_ERRNO_ENOTSUP)
			SCE_PRINTF_WITH_INFO(0, 1153, "please update host tools\n");
	}
	return ret;
}

static int dev_drfp_vop_fchstat(SceVopFchstatArgs *argp)
{
	SceVfsVnode *vp = argp->vp;
	const SceIoStat *stat = argp->stat;
	SceUInt32 bit = argp->bit;
	int fd = argp->file->fd;
	Deci4pFsStat st;
	int ret;

	if (!drfpChstatTimesValid(stat, bit))
		return SCE_ERROR_ERRNO_EINVAL;
	ret = 0;
	if (bit & SCE_CST_SIZE) {
		ret = sceDeci4pDrfpFtruncate(fd, stat->st_size);
		if (ret < 0)
			return ret;
		vp->core.size = stat->st_size;
	}
	if (bit & DRFP_CST_TIMES) {
		drfpChstatTimes(&st, stat, bit);
		ret = sceDeci4pDrfpFchstat(fd, bit, &st);
		if (ret == SCE_ERROR_ERRNO_ENOTSUP)
			SCE_PRINTF_WITH_INFO(0, 1095, "please update host tools\n");
	}
	return ret;
}

static const SceVfsOpTable s_drfpVfsOps = {
	.vfs_mount = dev_drfp_vfs_mount,
	.vfs_umount = dev_drfp_vfs_umount,
	.vfs_set_root = dev_drfp_vfs_set_root,
	.vfs_sync = dev_drfp_vfs_sync,
	.vfs_init = dev_drfp_vfs_init,
	.vfs_devctl = dev_drfp_vfs_devctl,
	.vfs_decode_path_elem = dev_drfp_vfs_decode_path_elem,
};

static const SceVopTable s_drfpVops = {
	.vop_open = dev_drfp_vop_open,
	.vop_create = dev_drfp_vop_create,
	.vop_close = dev_drfp_vop_close,
	.vop_lookup = dev_drfp_vop_lookup,
	.vop_read = dev_drfp_vop_read,
	.vop_write = dev_drfp_vop_write,
	.vop_lseek = dev_drfp_vop_lseek,
	.vop_remove = dev_drfp_vop_remove,
	.vop_mkdir = dev_drfp_vop_mkdir,
	.vop_rmdir = dev_drfp_vop_rmdir,
	.vop_dopen = dev_drfp_vop_dopen,
	.vop_dclose = dev_drfp_vop_dclose,
	.vop_dread = dev_drfp_vop_dread,
	.vop_getstat = dev_drfp_vop_getstat,
	.vop_chstat = dev_drfp_vop_chstat,
	.vop_rename = dev_drfp_vop_rename,
	.vop_pread = dev_drfp_vop_pread,
	.vop_pwrite = dev_drfp_vop_pwrite,
	.vop_inactive = dev_drfp_vop_inactive,
	.vop_sync = dev_drfp_vop_sync,
	.vop_fgetstat = dev_drfp_vop_fgetstat,
	.vop_fchstat = dev_drfp_vop_fchstat,
};

static SceVfsInfo s_drfpVfsInfo = {
	.vfs_ops = &s_drfpVfsOps,
	.vfs_name = DRFP_FS_NAME,
	.vfs_name_len = sizeof(DRFP_FS_NAME),
	.type = SCE_VFS_TYPE_DEVFS,
	.default_vops = &s_drfpVops,
};

static const SceVfsMountData s_drfpMountData = {
	.assign_name = "host0:",
	.fs_name = DRFP_FS_NAME,
	.blockdev_name = DRFP_FS_NAME,
};

static const SceVfsMountParam s_drfpMountParam = {
	.root_path = "/host",
	.fs_type = SCE_VFS_FS_TYPE_HOSTFS,
	.opt = 0x300,
	.mnt_flags = SCE_VFS_MOUNT_FLAG_REMOTE | SCE_VFS_MOUNT_TYPE_HOSTFS,
	.vfs_name = DRFP_FS_NAME,
	.misc = (SceVfsMountData *)&s_drfpMountData,
};

static const SceVfsUmountParam s_drfpUmountParam = {
	.assign_name = "/host",
};

/* a host0: left by an earlier instance goes first */
int sceDeci4pDrfpFsInit(void)
{
	int ret;

	ret = vfsUmount((SceVfsUmountParam *)&s_drfpUmountParam);
	if (ret < 0)
		DRFP_PRINTF(0xf, 144, "vfsUmount failed (0x%08X)\n", ret);
	ret = ksceVfsDeleteVfs(DRFP_FS_NAME, NULL);
	if (ret < 0)
		DRFP_PRINTF(0xf, 150, "vfsDeleteVfs failed (0x%08X)\n", ret);
	return 0;
}

int sceDeci4pDrfpFsStart(void)
{
	int ret;

	ret = ksceVfsAddVfs(&s_drfpVfsInfo);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 163, "sceVfsAddVfs failed (0x%08X)\n", ret);
		return ret;
	}
	ret = vfsMount((SceVfsMountParam *)&s_drfpMountParam);
	if (ret < 0) {
		DRFP_PRINTF(0xf, 181, "vfsMount failed (0x%08X)\n", ret);
		return ret;
	}
	return ret;
}
