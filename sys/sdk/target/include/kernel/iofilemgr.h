/*
 * kernel/iofilemgr.h: the I/O file manager's constants, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/kernel/iofilemgr.h).
 */
#ifndef _SCE_KERNEL_IOFILEMGR_H
#define _SCE_KERNEL_IOFILEMGR_H

#include <scetypes.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/iofilemgr_dirent.h>

/* device types */
#define SCE_D_TYPE_NULL		0x0	/* dummy device */
#define SCE_D_TYPE_CHAR		0x1	/* character device */
#define SCE_D_TYPE_BLOCK	0x4	/* block device */
#define SCE_D_TYPE_FS		0x10	/* file system */
#define SCE_D_TYPE_ALIAS	0x20	/* alias name (device alias) */
#define SCE_D_TYPE_MOUNTPT	0x40	/* alias name (mount point) */

/* flags */
#define SCE_FREAD		(0x0001)	/* readable */
#define SCE_FWRITE		(0x0002)	/* writable */
#define SCE_FNBLOCK		(0x0004)	/* reserved: non-blocking reads */
#define SCE_FDIRO		(0x0008)	/* internal use for dopen */
#define SCE_FRLOCK		(0x0010)	/* reserved: read locked (non-shared) */
#define SCE_FWLOCK		(0x0020)	/* reserved: write locked (non-shared) */
#define SCE_FAPPEND		(0x0100)	/* append on each write */
#define SCE_FCREAT		(0x0200)	/* create if nonexistent */
#define SCE_FTRUNC		(0x0400)	/* truncate to zero length */
#define SCE_EXCL		(0x0800)	/* exclusive create */
#define SCE_FSCAN		(0x1000)	/* reserved: scan type */
#define SCE_FRCOM		(0x2000)	/* reserved: remote command entry */
#define SCE_FNBUF		(0x4000)	/* no device buffer */
#define SCE_FASYNC		(0x8000)	/* reserved: asynchronous I/O */
#define SCE_FFDEXCL		(0x01000000)	/* exclusive access */
#define SCE_FGAMEDATA		(0x40000000)

/* sceIoOpen's flags */
#define SCE_O_RDONLY		(SCE_FREAD)
#define SCE_O_WRONLY		(SCE_FWRITE)
#define SCE_O_RDWR		(SCE_FREAD|SCE_FWRITE)
#define SCE_O_NBLOCK		(SCE_FNBLOCK)	/* reserved: non-blocking I/O */
#define SCE_O_APPEND		(SCE_FAPPEND)	/* append (writes guaranteed at the end) */
#define SCE_O_CREAT		(SCE_FCREAT)	/* open with file create */
#define SCE_O_TRUNC		(SCE_FTRUNC)	/* open with truncation */
#define SCE_O_EXCL		(SCE_EXCL)	/* exclusive create */
#define SCE_O_NOBUF		(SCE_FNBUF)	/* no device buffer */
#define SCE_O_NOWAIT		(SCE_FASYNC)	/* reserved: asynchronous I/O */
#define SCE_O_FDEXCL		(SCE_FFDEXCL)	/* exclusive access */
#define SCE_O_PWLOCK		(SCE_FPWLOCK)	/* power control lock (Sony defines no SCE_FPWLOCK) */
#define SCE_O_FGAMEDATA		(SCE_FGAMEDATA)

/* sceIoLseek's whence */
#define SCE_SEEK_SET		(0)
#define SCE_SEEK_CUR		(1)
#define SCE_SEEK_END		(2)

/* paths */
#define SCE_IO_MAX_PATH_BUFFER_SIZE	(1024)	/* maximum path buffer size */
#define SCE_IO_MAX_PATH_LENGTH		(200)	/* maximum path length */

#endif /* _SCE_KERNEL_IOFILEMGR_H */
