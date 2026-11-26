/* Copyright (C) 2011 - 2015 The uOFW team
   See the file COPYING for copying permission.
*/

#ifndef MODULEMGR_INT_H
#define MODULEMGR_INT_H

#include <common_header.h>
#include <loadcore.h>
#include <sysmem_kernel.h>

#define THREAD_SM_LEGAL_ATTR     (SCE_KERNEL_TH_NO_FILLSTACK | SCE_KERNEL_TH_CLEAR_STACK \
                                 | SCE_KERNEL_TH_LOW_STACK | SCE_KERNEL_TH_UNK_800000 \
                                 | SCE_KERNEL_TH_USE_VFPU | SCE_KERNEL_TH_NEVERUSE_FPU)

typedef struct {
    u8 modeStart; //0 The Operation to start on, Use one of the ModuleMgrExeModes modes
    u8 modeFinish; //1 The Operation to finish on, Use one of the ModuleMgrExeModes modes
    // SceSysMemBlockType position
    u8 position; //2
    u8 access; //3
    SceUID *pResult; //4
    SceUID *pNewBlockId; // 8
    SceModule *pMod; //12
    SceLoadCoreExecFileInfo *pExecInfo; //16
    u32 apiType; //20
    SceUID fileFd; // 24
    s32 threadPriority; //28
    u32 threadAttr; //32
    SceUID mpIdText; // 36
    SceUID mpIdData; // 40
    SceUID threadMpIdStack; //44
    SceSize stackSize; //48
    SceUID modId; //52
    SceUID callerModId; //56
    SceSize modSize; //60
    void *fileBase; // 64
    SceSize argSize; //68
    void *argp; //72
    u32 unusedBeforeStatus[2]; // 76, 80
    s32 *pStatus; // 84
    SceUID eventId; // 88
    u32 unusedAfterEvent; // 92
    u32 unusedBeforeFlags; // 96
    u32 loadFlags; // 100
    SceUID externMemBlockIdKernel; // 104
    SceUID externMemBlockPartitionId; // 108
    SceSize externMemBlockSize; // 112
    u32 sepMemBlockId; // 116
    void *blockGzip; // 120 
    u32 isBufferLoad; // 124
    char secureInstallId[SCE_SECURE_INSTALL_ID_LEN]; // 128
    SceUID externMemBlockIdUser; //144
    u32 offsetAlign; // 148
    SceOff memBlockOffset; // 152
} SceModuleMgrParam; //size = 160

typedef struct {
    SceUID threadId; // 0
    SceUID mutexId; // 4
    SceUID eventId; // 8
    SceUID userThreadId; // 12
    u32 initState;
    void *listHead;
    void *listTail;
    u32 listReserved;
    s32(*npDrmGetModuleKeyFunction)(SceUID fileFd, void *, void *); // 32
    /** Module whose text segment checksum can be checked with sceKernelCheckTextSegment (user). */
    SceModule *pModule;
} SceModuleManagerCB;

enum ModuleMgrExecModes {
    CMD_LOAD_MODULE, //0
    CMD_RELOCATE_MODULE, //1
    CMD_START_MODULE, //2
    CMD_STOP_MODULE, //3
    CMD_UNLOAD_MODULE, //4
};

extern SceModuleManagerCB g_ModuleManager;
extern s32 g_workerPriorityBoost;
extern s32 g_pathOverrideMode;
extern s32 g_discEmuPatchEnabled;

/*
 * Reset and free the specified memory block.
 */
static inline __attribute__((always_inline)) s32 ClearFreePartitionMemory(SceUID blockId)
{
    s32 status;

    if (blockId <= 0)
        return SCE_ERROR_OK;

    SceSysmemMemoryBlockInfo blkInfo;
    blkInfo.size = sizeof(SceSysmemMemoryBlockInfo);
    status = sceKernelQueryMemoryBlockInfo(blockId, &blkInfo);
    if (status < SCE_ERROR_OK)
        return status;

    sceKernelMemset((void *)blkInfo.addr, 0, blkInfo.memSize);
    status = sceKernelFreePartitionMemory(blockId);

    return status;
}

/*
 * Check if specified memory partition is a USER partition.
 */
static inline __attribute__((always_inline)) s32 _CheckUserModulePartition(SceUID memoryPartitionId)
{
    s32 status;
    SceSysmemPartitionInfo partitionInfo;

    partitionInfo.size = sizeof(SceSysmemPartitionInfo);
    status = sceKernelQueryMemoryPartitionInfo(memoryPartitionId, &partitionInfo);
    if (status < SCE_ERROR_OK || !(partitionInfo.attr & 3))
        return SCE_ERROR_KERNEL_PARTITION_MISMATCH;

    return SCE_ERROR_OK;
}

s32 _start_exe_thread(SceModuleMgrParam *pModParams);

void ChunkInit(void);

#endif	/* MODULEMGR_INT_H */

