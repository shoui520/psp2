/*
 * SceTty2uart: the tty0: .. ttyN: devices ("uart_ttyp_dev_fs", mounted at
 * /tty) on the debug UART.
 *
 * Every process gets one of 16 ring buffers of 4 KiB when it is created;
 * what it writes to a tty is kept there (readable through the sysroot log
 * reader) and, with dipsw 0xc4 / 0xc5, echoed to the kernel console. Reads
 * come from UART 0. The two-character sequence "\n" (backslash, n) written by
 * a process is turned into a newline.
 */

#include <sysmem/cpu.h>
#include <sysmem/dipsw.h>
#include <modulemgr/modulemgr.h>
#include <sysmem/procevent.h>
#include <sysmem/sysclib.h>
#include <sysmem/sysmem.h>
#include <sysmem/uart.h>
#include <iofilemgr/vfs.h>
#include <sysmem/debug.h>
#include <kernel/error.h>
#include <sysmem/sysroot.h>
#include <threadmgr/threadmgr.h>

#define TTY2_FS_NAME		"uart_ttyp_dev_fs"
#define TTY2_BUFFERS		16
#define TTY2_BUFFER_SIZE	0x1000
#define TTY2_UART		0


#define TTY2_TRACE(_flags, _line, _fmt, ...)				\
	SCE_DBG_PRINTF_LEVEL(2, _flags, _line, _fmt, ##__VA_ARGS__)
#define TTY2_ERROR(_flags, _line, _fmt, ...)				\
	SCE_DBG_PRINTF_LEVEL(1, _flags, _line, _fmt, ##__VA_ARGS__)

typedef struct Tty2Buffer {
	SceKernelSpinlock lock;
	SceUID pid;				/* owner, -1: free */
	SceUID uid;				/* memblock */
	signed char *base;
	signed char *end;
	signed char *cur;			/* next character */
	SceSize size;
	SceUInt32 count;			/* times wrapped */
} Tty2Buffer;

typedef struct Tty2Control {
	SceKernelRWSpinlock lock;		/* buffer ownership */
	const SceKernelProcEventHandler *handler;	/* = &procHandler */
	SceUInt32 nbuf;
	Tty2Buffer *buf;
	SceUID handlerId;
	SceKernelProcEventHandler procHandler;
} Tty2Control;

static int procCreateHandler(SceUID pid, ProcEventCreateParam *param, void *common);
static int procExitHandler(SceUID pid, ProcEventParam *param, void *common);
static int procKillHandler(SceUID pid, ProcEventParam *param, void *common);

static Tty2Buffer s_buffer[TTY2_BUFFERS];
static Tty2Control s_control = {
	.handler = &s_control.procHandler,
	.nbuf = TTY2_BUFFERS,
	.buf = s_buffer,
	.handlerId = -1,
	.procHandler = {
		.size = sizeof(SceKernelProcEventHandler),
		.create = procCreateHandler,
		.exit = procExitHandler,
		.kill = procKillHandler,
	},
};
static SceVfsInfo s_vfsInfo;
static SceUID s_mutex;

/* -- per-process ring buffers ------------------------------------------- */

static Tty2Buffer *tty2bufferFindByPidRaw(SceUID pid)
{
	SceUInt32 i;

	for (i = 0; i < s_control.nbuf; i++) {
		if (s_control.buf[i].pid == pid)
			return &s_control.buf[i];
	}
	return NULL;
}

static int controlGetLogRaw(SceUID pid, void *buf, int size);

static void tty2bufferStart(Tty2Buffer *p)
{
	SceKernelIntrStatus intr;
	int ret;

	TTY2_TRACE(0xb, 150, "(%p)\n", p);
	p->lock = 0;
	intr = SceCpuForDriver_D32ACE9E(&p->lock);
	p->pid = -1;
	p->size = TTY2_BUFFER_SIZE;
	p->uid = sceKernelAllocMemBlockForKernel("SceTty2uartBuffer", SCE_KERNEL_MEMBLOCK_TYPE_KERNEL_ROOT_RW, TTY2_BUFFER_SIZE, NULL);
	if (p->uid < 0) {
		TTY2_ERROR(0xb, 159, "sceKernelAllocMemBlockForKernel failed (0x%08X)\n", p->uid);
	} else {
		ret = sceKernelGetMemBlockVBase(p->uid, (void **)&p->base);
		if (ret < 0) {
			TTY2_ERROR(0xb, 165, "sceKernelGetMemBlockVBase failed (0x%08X)\n", ret);
			SceSysmemForDriver_009E1C61(p->uid);
		} else {
			p->count = 0;
			p->cur = p->base;
			p->end = p->base + p->size;
			SceSysrootForDriver_AA770EF7(controlGetLogRaw);
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&p->lock, intr);
}

static void tty2bufferPutcharRaw(Tty2Buffer *p, signed char c)
{
	TTY2_TRACE(0xb, 219, "(%p, [%c])\n", p, c);
	if (p->cur >= p->end) {
		p->count++;
		p->cur = p->base;
		TTY2_TRACE(0xb, 223, "reset: p->count=%d\n", p->count);
	}
	*p->cur = c;
	p->cur++;
}

static void tty2bufferWrite(Tty2Buffer *p, const signed char *buf, SceSize n)
{
	SceKernelIntrStatus intr;
	SceSize i;

	TTY2_TRACE(0xb, 238, "(%p, [%-16s])\n", p, buf);
	intr = SceCpuForDriver_D32ACE9E(&p->lock);
	for (i = 0; i < n; ) {
		if (buf[i] == '\\' && buf[i + 1] == 'n') {
			tty2bufferPutcharRaw(p, '\n');
			i += 2;
		} else {
			tty2bufferPutcharRaw(p, buf[i]);
			i++;
		}
	}
	sceKernelSpinlockLowUnlockCpuResumeIntr(&p->lock, intr);
}

/* -- process events ----------------------------------------------------- */

static int controlOpen(SceUID pid)
{
	SceKernelIntrStatus wintr, intr;
	Tty2Buffer *p;

	TTY2_TRACE(0xb, 466, "(pid 0x%08x)\n", pid);
	wintr = SceCpuForDriver_4C38CE4D(&s_control.lock);
	p = tty2bufferFindByPidRaw(-1);
	if (p == NULL) {
		TTY2_ERROR(0xf, 474, "Cannot allocate free tty2buffer\n");
	} else {
		intr = SceCpuForDriver_D32ACE9E(&p->lock);
		p->count = 0;
		p->pid = pid;
		p->cur = p->base;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&p->lock, intr);
		TTY2_TRACE(0xb, 471, "pid=0x%08x\n", p->pid);
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&s_control.lock, wintr);
	return 0;
}

static int controlClose(SceUID pid)
{
	SceKernelIntrStatus wintr, intr;
	Tty2Buffer *p;

	TTY2_TRACE(0xb, 533, "(pid 0x%08x)\n", pid);
	wintr = SceCpuForDriver_4C38CE4D(&s_control.lock);
	p = tty2bufferFindByPidRaw(pid);
	if (p == NULL) {
		TTY2_ERROR(0xb, 542, "tty2bufferFindByPidRaw(0x%08x) failed\n", pid);
	} else {
		intr = SceCpuForDriver_D32ACE9E(&p->lock);
		p->pid = -1;
		p->count = 0;
		p->cur = p->base;
		sceKernelSpinlockLowUnlockCpuResumeIntr(&p->lock, intr);
	}
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&s_control.lock, wintr);
	return 0;
}

static int procCreateHandler(SceUID pid, ProcEventCreateParam *param, void *common)
{
	TTY2_TRACE(0xb, 300, "(pid 0x%08x)\n", pid);
	return controlOpen(pid);
}

static int procExitHandler(SceUID pid, ProcEventParam *param, void *common)
{
	TTY2_TRACE(0xb, 310, "(pid 0x%08x)\n", pid);
	return controlClose(pid);
}

static int procKillHandler(SceUID pid, ProcEventParam *param, void *common)
{
	TTY2_TRACE(0xb, 320, "(pid 0x%08x)\n", pid);
	return controlClose(pid);
}

/* the newest size bytes written by process pid (sysroot log reader) */
static int controlGetLogRaw(SceUID pid, void *buf, int size)
{
	Tty2Buffer *p;
	signed char *src, *dst = buf;
	int n = 0, m;

	p = tty2bufferFindByPidRaw(pid);
	if (p == NULL) {
		TTY2_ERROR(0xf, 588, "tty2bufferFindByPidRaw(0x%08x) failed\n", pid);
		return SCE_KERNEL_ERROR_ERROR;
	}
	if (p->count != 0) {
		/* wrapped: the older part, after cur */
		src = p->cur;
		n = p->end - p->cur;
		if (size - (p->cur - p->base) < n) {
			n = size - (p->cur - p->base);
			src = p->end - n;
		}
		dst = (signed char *)buf + n;
		memcpy(buf, src, n);
	}
	size -= n;
	src = p->base;
	m = p->cur - p->base;
	if (size < m) {
		src = p->cur - size;
		m = size;
	}
	memcpy(dst, src, m);
	n += m;
	if (n < 0) {
		TTY2_ERROR(0xf, 584, "tty2bufferGetRaw() failed 0x%08x\n", n);
		return n;
	}
	return n;
}

static int controlWrite(const signed char *buf, SceSize n)
{
	SceKernelIntrStatus intr;
	Tty2Buffer *p;
	SceUID pid;

	pid = sceKernelGetProcessId();
	intr = SceCpuForDriver_EC53D007(&s_control.lock);
	p = tty2bufferFindByPidRaw(pid);
	if (p == NULL)
		TTY2_ERROR(0xf, 564, "tty2bufferFindByPidRaw(0x%08x) failed\n", pid);
	else
		tty2bufferWrite(p, buf, n);
	sceKernelRWSpinlockLowReadUnlockCpuResumeIntr(&s_control.lock, intr);
	return 0;
}

static int startControl(void)
{
	SceKernelIntrStatus intr;
	SceUInt32 i;
	int ret;

	s_control.lock = 0;
	intr = SceCpuForDriver_4C38CE4D(&s_control.lock);
	for (i = 0; i < s_control.nbuf; i++)
		tty2bufferStart(&s_control.buf[i]);
	ret = sceKernelRegisterProcEventHandler("SceTty2UartProcEvent", &s_control.procHandler,
						 &s_control);
	if (ret < 0)
		return ret;		/* (with the control lock held) */
	s_control.handlerId = ret;
	sceKernelRWSpinlockLowWriteUnlockCpuResumeIntr(&s_control.lock, intr);
	return 0;
}

/* -- uart_ttyp_dev_fs --------------------------------------------------- */

static int dev_tty_vfs_mount(SceVfsOpMountArgs *argp)
{
	SceVfsMount *mnt;

	if (argp == NULL) {
		TTY2_ERROR(0xf, 744, "invalid arg\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	mnt = argp->mnt;
	TTY2_TRACE(0xf, 749, "args = %p, mp = %p\n", argp, mnt);
	if (mnt == NULL) {
		TTY2_ERROR(0xf, 752, "invalid arg\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	mnt->availableEntryNum = 0x40;
	return 0;
}

static int dev_tty_vfs_umount(SceVfsOpUmountArgs *argp)
{
	TTY2_TRACE(0xf, 764, "args = %p\n", argp);
	return 0;
}

static int dev_tty_vfs_set_root(SceVfsOpSetRootArgs *argp)
{
	SceVfsMount *mnt;
	SceVfsVnode *vp;

	if (argp == NULL) {
		TTY2_ERROR(0xf, 774, "invalid arg\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	mnt = argp->mnt;
	vp = argp->vp;
	TTY2_TRACE(0xf, 781, "args = %p, mp = %p, vp = %p\n", argp, mnt, vp);
	if (mnt == NULL) {
		TTY2_ERROR(0xf, 784, "invalid arg\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	vp->core.nodeData = mnt->pData;
	vp->core.mnt = mnt;
	vp->core.type = SCE_VNODE_TYPE_ROOTDIR_DEVFS;
	vp->core.dd = NULL;
	vp->core.fid[0] = 0;
	vp->core.fid[1] = 0;
	vp->core.size = 0;
	vp->core.aclData[0] = 0;
	vp->core.aclData[1] = 0;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	return 0;
}

static int dev_tty_vfs_init(SceVfsOpInitArgs *argp)
{
	TTY2_TRACE(0xf, 728, "args = %p\n", argp);
	return 0;
}

static int dev_tty_vfs_fini(SceVfsOpFiniArgs *argp)
{
	TTY2_TRACE(0xf, 735, "args = %p\n", argp);
	return 0;
}

/* "ttyN": a character device vnode for unit N */
static int dev_tty_vop_lookup(SceVopLookupArgs *argp)
{
	SceVfsVnode *dvp, **vpp, *vp;
	SceVfsMount *mnt;
	const signed char *name;
	SceSize len;
	int unit, ret;

	if (argp == NULL || argp->path == NULL) {
		TTY2_ERROR(0xf, 812, "invalid arg\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	len = argp->path->nameLength;
	dvp = argp->dvp;
	vpp = argp->vpp;
	name = (const signed char *)argp->path->name;
	TTY2_TRACE(0xf, 822, "args = %p, %p/%p, %s, %d\n", argp, dvp, vpp, name, len);
	if (len != 4 || name[0] != 't' || name[1] != 't' || name[2] != 'y')
		return SCE_ERROR_ERRNO_ENOENT;
	unit = name[3] - '0';
	TTY2_TRACE(0xf, 835, "unit = %d\n", unit);
	if (dvp == NULL || (mnt = dvp->core.mnt) == NULL || mnt->mntVfsInf == NULL) {
		TTY2_ERROR(0xf, 838, "invalid arg\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	ret = vfsGetNewVnode(mnt, (SceVopTable *)mnt->mntVfsInf->defaultVops, 0, &vp);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 846, "vfsGetNewNode failed (0x%08X)\n", ret);
		return ret;
	}
	if (vp == NULL) {
		TTY2_ERROR(0xf, 851, "invalid pointer\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	vfsLockVnode(vp);
	vp->core.nodeData = (void *)unit;
	vp->core.mnt = dvp->core.mnt;
	vp->core.dd = dvp;
	vp->core.state = SCE_VNODE_STATE_ACTIVE;
	vp->core.type = SCE_VNODE_TYPE_CHRDEV;
	*vpp = vp;
	return 0;
}

static int dev_tty_vop_open(SceVopOpenArgs *argp)
{
	TTY2_TRACE(0xf, 870, "args = %p\n", argp);
	return 0;
}

static int dev_tty_vop_close(SceVopCloseArgs *argp)
{
	TTY2_TRACE(0xf, 877, "args = %p\n", argp);
	return 0;
}

static SceSSize dev_tty_vop_read(SceVopReadArgs *argp)
{
	signed char *buf;
	int n, i, c, ret;

	if (argp == NULL) {
		TTY2_ERROR(0xf, 890, "invalid arg\n");
		return SCE_ERROR_ERRNO_EINVAL;
	}
	n = argp->nbyte;
	buf = argp->buf;
	TTY2_TRACE(0xf, 897, "args = %p, buf = %p, nbyte = %d\n", argp, buf, n);
	if (n < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 900, "invalid size (%d)\n", n);
		return SCE_ERROR_ERRNO_EINVAL;
	}
	if (!SceDipswForDriver_A98FC2FD(0xc4))
		return 0;
	ret = sceKernelLockMutex(s_mutex, 1, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 913, "LockMutex failed (0x%08X)\n", ret);
		return ret;
	}
	for (i = 0; i != n; i++) {
		c = SceUartForKernel_4E97D3AD(TTY2_UART);
		if (c < 0) {
			sceKernelUnlockMutex(s_mutex, 1);
			return c;
		}
		buf[i] = c;
	}
	sceKernelUnlockMutex(s_mutex, 1);
	return n;
}

/* name unknown */
static SceSSize dev_tty_vop_write(SceVopWriteArgs *argp)
{
	const signed char *buf;
	int n, i;

	if (argp == NULL || (int)argp->nbyte < 0)
		return SCE_ERROR_ERRNO_EINVAL;
	buf = argp->buf;
	n = argp->nbyte;
	controlWrite(buf, n);

	if ((SceDipswForDriver_A98FC2FD(0xc4) || SceDipswForDriver_A98FC2FD(0xc5)) &&
	    sceKernelLockMutex(s_mutex, 1, NULL) >= 0) {
		for (i = 0; i < n; i++) {
			if (buf[i] == '\\' && buf[i + 1] == 'n') {
				i++;
				SceDebugForDriver_391B74B7("\n");
			} else {
				SceDebugForDriver_391B74B7("%c", buf[i]);
			}
		}
		sceKernelUnlockMutex(s_mutex, 1);
	}
	return n;
}

static int dev_tty_vop_ioctl(SceVopIoctlArgs *argp)
{
	TTY2_TRACE(0xf, 1012, "args = %p\n", argp);
	return 0;
}

static int dev_tty_vop_inactive(SceVopInactiveArgs *argp)
{
	TTY2_TRACE(0xf, 1019, "args = %p\n", argp);
	return 0;
}

static const SceVfsOpTable s_vfsOps = {
	.vfs_mount = dev_tty_vfs_mount,
	.vfs_umount = dev_tty_vfs_umount,
	.vfs_set_root = dev_tty_vfs_set_root,
	.vfs_init = dev_tty_vfs_init,
	.vfs_fini = dev_tty_vfs_fini,
};

static const SceVopTable s_vops = {
	.vop_open = dev_tty_vop_open,
	.vop_close = dev_tty_vop_close,
	.vop_lookup = dev_tty_vop_lookup,
	.vop_read = dev_tty_vop_read,
	.vop_write = dev_tty_vop_write,
	.vop_ioctl = dev_tty_vop_ioctl,
	.vop_inactive = dev_tty_vop_inactive,
};

static const SceVfsMountData s_mountData = {
	.assignName = "tty",
	.fsName = TTY2_FS_NAME,
	.blockdevName = TTY2_FS_NAME,
};

static const SceVfsMountParam s_mountParam = {
	.rootPath = "/tty",
	.fsType = SCE_VFS_FS_TYPE_CHRDEV,
	.opt = 0x400,
	.mntFlags = SCE_VFS_MOUNT_FLAG_NUMBERED | SCE_VFS_MOUNT_FLAG_NOBUF | SCE_VFS_MOUNT_TYPE_DEVFS,
	.vfsName = TTY2_FS_NAME,
	.misc = (SceVfsMountData *)&s_mountData,
};

/* (writable: kept away from the operation tables) */
static SceVfsUmountParam s_umountParam = {
	.assignName = "/tty",
};

static int tty2uartFsInit(void)
{
	int ret;

	ret = vfsUmount(&s_umountParam);
	if (ret < 0)
		TTY2_TRACE(0xf, 611, "vfsUmount failed (0x%08X)\n", ret);

	s_vfsInfo.vfsOps = &s_vfsOps;
	s_vfsInfo.vfsName = TTY2_FS_NAME;
	s_vfsInfo.type = SCE_VFS_TYPE_DEVFS;
	s_vfsInfo.defaultVops = &s_vops;
	s_vfsInfo.vfsNameLen = sizeof(TTY2_FS_NAME);
	s_vfsInfo.vfsData = NULL;
	ret = sceVfsAddVfs(&s_vfsInfo);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 634, "sceVfsAddVfs failed (0x%08X)\n", ret);
		return ret;
	}
	ret = vfsMount((SceVfsMountParam *)&s_mountParam);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 653, "vfsMount failed (0x%08X)\n", ret);
		return ret;
	}
	return ret;
}

int tty2uartModuleStart(SceSize args, void *argp)
{
	int ret;

	TTY2_ERROR(0xb, 666, "()\n");
	if (!SceDipswForDriver_A98FC2FD(0xd3))
		return SCE_KERNEL_START_NO_RESIDENT;

	ret = sceKernelCreateMutex("SceDeci4pTtyp", SCE_KERNEL_ATTR_TH_PRIO, 0, NULL);
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 677, "CreateMutex failed (0x%08X)\n", ret);
		return SCE_KERNEL_START_FAILED;
	}
	s_mutex = ret;

	ret = tty2uartFsInit();
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 685, "tty2uartFsInit failed (0x%08X)\n", ret);
		return SCE_KERNEL_START_FAILED;
	}
	ret = startControl();
	if (ret < 0) {
		SCE_PRINTF_WITH_INFO(0xf, 692, "startControl failed (0x%08X)\n", ret);
		return SCE_KERNEL_START_FAILED;
	}
	return SCE_KERNEL_START_SUCCESS;
}
