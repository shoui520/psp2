/* Copyright (C) 2011, 2012, 2013 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef INIT_INT_H
#define	INIT_INT_H

#define INIT_CLEAR_MEM_SIZE     (0x28)

typedef struct {
    u32 memSize;
    u32 headerSize;
    s32 bootType;
    s32 flags;
    s32 exitMode;
    s32 rebootError;
    s32 exitError;
    s32 reserved;
} SceInitVshMainArg;

typedef struct {
    SceUID initThreadId;
    SceLoadCoreBootInfo *bootInfo;
} SceInitThreadArgs;

void initClearMem(u32 *baseAddr, u32 size);

#endif	/* INIT_INT_H */

