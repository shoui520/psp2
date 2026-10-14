/*
 * reg.h: what the registry manager and the reg library share. Names unknown.
 */

#ifndef REG_H
#define REG_H

/* how the registry files are opened (names unknown) */
typedef struct RegMgrRegistryParam {
	int mode;		/* 1 system, 2 backup, 3 temp; 0x80: files closed after the open */
	char reserved[0x104];	/* not read */
	int size;		/* the data file, in 128 KiB */
	int blockSize;		/* in 512 bytes */
} RegMgrRegistryParam;

#define REG_PARAM_CLOSE_FILES	0x80

#endif
