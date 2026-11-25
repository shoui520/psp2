/* Copyright (C) 2011, 2012, 2013 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef LOADEXEC_H
#define LOADEXEC_H

#include <loadexec_kernel.h>

typedef struct
{
    s32 size;
    s32 pad;
    s32 npDrmKeyLo;
    s32 npDrmKeyHi;
    char npDrmKey[16];
    SceKernelLoadExecVSHParam vshParam;
} NpDrmArg;

typedef struct
{
    s32 apiType;
    s32 args;
    /* Sometimes contains filename (args is then set to 0) */
    void *argp;
    SceKernelLoadExecVSHParam *vshParam;
    void *vshArg;
    char *npDrmKey;
    s32 npDrmKeyLo;
    s32 npDrmKeyHi;
    NpDrmArg *npDrmArg;
} RunExecParams;

typedef struct
{
    void *argp;
    u32 args;
    SceKernelRebootArgType type;
    s32 reserved[4];
} SceKernelArgsStor;

typedef struct
{
    void *addr;
    s32 memorySize;
    s32 reservedMem;
    s32 numLoadModules;
    void *startAddr;
    void *endAddr;
    s32 argTypeFlags;
    s32 curArgs;
    SceKernelArgsStor *args;
    s32 vshParamIdx;
    s32 curArgOffset;
    s32 model;
    s32 reservedConfig[4];
    s32 dipswLo;
    s32 dipswHi;
    void *npDrmArgAddr;
    void *execArgp;
    s32 cpTime;
} SceKernelRebootParam;

s32 decodeKL4E(void *dest, s32 size, const void *src, const void **srcEnd);

void copyArgsToRebootParam(SceKernelRebootParam *hwOpt, SceKernelLoadExecVSHParam *opt);
void fixupArgsAddr(SceKernelRebootParam *hwOpt, SceKernelLoadExecVSHParam *opt __attribute__((unused)));
s32 runExec(RunExecParams *args);
s32 ioctlAndDevctl(char *name, s32 devcmd, s32 iocmd);
s32 checkVSHParam(SceKernelLoadExecVSHParam *opt);
s32 loadExecVSH(s32 apiType, char *file, SceKernelLoadExecVSHParam *opt, u32 flags);
s32 loadExecVSHWithArgs(s32 apiType, s32 args, void *argp, SceKernelLoadExecVSHParam *opt, u32 flags);
s32 loadExecKernel(s32 apiType, SceKernelLoadExecVSHParam *opt);
s32 rebootKernel(SceKernelLoadExecVSHParam *arg);
s32 runExecFromThread(u32 args, RunExecParams *opt);
void copyVSHParam(SceKernelLoadExecVSHParam *dstOpt, u32 flags, SceKernelLoadExecVSHParam *srcOpt);
s32 runReboot(RunExecParams *opt);
SceKernelRebootParam *makeRebootParam(void);

#endif  /* LOADEXEC_H */
