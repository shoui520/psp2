/* Copyright (C) 2011, 2012, 2013 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef LOADEXEC_H
#define LOADEXEC_H

#include <loadexec_kernel.h>

typedef struct
{
    s32 size; // 0
    s32 reserved4;
    s32 npDrm2_1; // 8
    s32 npDrm2_2; // 12
    char npDrm1[16]; // 16
    SceKernelLoadExecVSHParam vshParam; // 32
} NpDrmArg;

typedef struct
{
    s32 apiType;
    s32 args;
    // Sometimes contains filename (args is then set to 0)
    void *argp;
    SceKernelLoadExecVSHParam *vshParam;
    void *opt4;
    char *npDrm1;
    s32 npDrm2_1;
    s32 npDrm2_2;
    NpDrmArg *npDrmArg;
} RunExecParams;

typedef struct
{
    void *argp; // 0
    u32 args; // 4
    SceKernelRebootArgType type; // 8
    s32 reserved12;
    s32 reserved16;
    s32 reserved20;
    s32 reserved24;
} SceKernelArgsStor;

typedef struct
{
    void *addr; // 0
    s32 memorySize; // 4
    s32 reserved8;
    s32 numLoadModules;
    void *startAddr; // 16
    void *endAddr; // 20
    s32 argTypeFlags;
    s32 curArgs; // 28
    SceKernelArgsStor *args; // 32
    s32 vshParamIdx;
    s32 curArgOffset;
    s32 model; // 44
    s32 reserved48;
    s32 reserved52;
    s32 reserved56;
    s32 reserved60;
    s32 dipswLo; // 64
    s32 dipswHi; // 68
    void *npDrmArgAddr; // 72
    void *execArgp; // 76
    s32 cpTime; // 80
    // ... ? size is max 0x1000
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
SceKernelRebootParam *makeRebootParam();

#endif  /* LOADEXEC_H */

