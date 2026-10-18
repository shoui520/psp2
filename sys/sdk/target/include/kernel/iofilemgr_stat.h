/*
 * kernel/iofilemgr_stat.h: the I/O file manager's file status, under Sony's names and with Sony's
 * definitions (SDK Release 03.570.011, target/include/kernel/iofilemgr_stat.h).
 */
#ifndef _SCE_KERNEL_IOFILEMGR_STAT_H
#define _SCE_KERNEL_IOFILEMGR_STAT_H

#include <scetypes.h>
#include <kernel/types.h>

#if !defined(_LANGUAGE_ASSEMBLY)

/* st_mode: the file's type */
#define SCE_STM_FMT		(0xf  <<  12)
#define SCE_STM_FREG		(0x2  <<  12)	/* regular file */
#define SCE_STM_FDIR		(0x1  <<  12)	/* directory */
#define SCE_STM_ISREG(m)	(((m) & SCE_STM_FMT) == SCE_STM_FREG)
#define SCE_STM_ISDIR(m)	(((m) & SCE_STM_FMT) == SCE_STM_FDIR)

/* st_mode: the access rights */
#define SCE_STM_RWU		(00600)
#define SCE_STM_RU		(00400)

/* sceIoChstat's cbit: the fields to change */
#define SCE_CST_MODE		0x0001
#define SCE_CST_ATTR		0x0000		/* not supported */
#define SCE_CST_SIZE		0x0004
#define SCE_CST_CT		0x0008
#define SCE_CST_AT		0x0010
#define SCE_CST_MT		0x0020

/* a file's or directory's status (sceIoGetstat, sceIoChstat, ...ByFd) */
typedef struct SceIoStat {
	SceIoMode	st_mode;	/* the file's type and access rights */
	unsigned int	st_attr;	/* the device's attributes */
	SceOff		st_size;
	SceDateTime	st_ctime;	/* created */
	SceDateTime	st_atime;	/* last accessed */
	SceDateTime	st_mtime;	/* last modified */
	unsigned int	st_private[6];
} SceIoStat;

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_KERNEL_IOFILEMGR_STAT_H */
