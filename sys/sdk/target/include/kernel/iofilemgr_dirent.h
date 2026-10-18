/*
 * kernel/iofilemgr_dirent.h: the I/O file manager's directory entry, under Sony's names and with
 * Sony's definitions (SDK Release 03.570.011, target/include/kernel/iofilemgr_dirent.h).
 */
#ifndef _SCE_KERNEL_IOFILEMGR_DIRENT_H
#define _SCE_KERNEL_IOFILEMGR_DIRENT_H

#include <kernel/iofilemgr_stat.h>

#if !defined(_LANGUAGE_ASSEMBLY)

/* a directory entry (sceIoDread) */
typedef struct SceIoDirent {
	SceIoStat	d_stat;
	char		d_name[256];	/* the file name, not the full path */
	void		*d_private;	/* reserved (0) */
	int		dummy;		/* padding */
} SceIoDirent;

#endif /* !_LANGUAGE_ASSEMBLY */

#endif /* _SCE_KERNEL_IOFILEMGR_DIRENT_H */
