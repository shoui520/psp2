/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include "common_header.h"
#include "loadcore.h"

enum SceInterrupts {
    SCE_GPIO_INT = 4,
    SCE_ATA_INT  = 5,
    SCE_UMD_INT  = 6,
    SCE_MSCM0_INT = 7,
    SCE_WLAN_INT  = 8,
    SCE_AUDIO_INT = 10,
    SCE_I2C_INT   = 12,
    SCE_SIRCS_INT = 14,
    SCE_SYSTIMER0_INT = 15,
    SCE_SYSTIMER1_INT = 16,
    SCE_SYSTIMER2_INT = 17,
    SCE_SYSTIMER3_INT = 18,
    SCE_THREAD0_INT   = 19,
    SCE_NAND_INT      = 20,
    SCE_DMACPLUS_INT  = 21,
    SCE_DMA0_INT      = 22,
    SCE_DMA1_INT      = 23,
    SCE_MEMLMD_INT    = 24,
    SCE_GE_INT        = 25,
    SCE_VBLANK_INT = 30,
    SCE_MECODEC_INT  = 31,
    SCE_HPREMOTE_INT = 36,
    SCE_MSCM1_INT    = 60,
    SCE_MSCM2_INT    = 61,
    SCE_THREAD1_INT  = 65,
    SCE_INTERRUPT_INT = 66
};

typedef struct {
    // Handler address
    s32 handler; // 0
    // GP of the module
    s32 gp; // 4
    // Argument given by sceKernelRegisterSubIntrHandler
    s32 arg; // 8
    u32 callCount; // 12
    u32 minClockLo; // 16
    u32 minClockHi; // 20
    u32 maxClockLo; // 24
    u32 maxClockHi; // 28
    u32 totalClockLo; // 32
    u32 totalClockHi; // 36
    s32 reservedSub; // 40
    s32 reservedCb; // 44
    union {
        u32 flags;
        struct {
            u8 subCount;
            u8 attr;
            u8 intrNum;
            u8 isSub;
        } bits;
    }; // 48
    s32 reserved[3]; // 52
} SubInterrupt; // Size: 64

typedef struct {
    s32 size; // 0
    s32 attr; // 4
    // Callback called before setting sub interrupt, when registering
    s32 (*cbRegBefore)(s32, s32, void*, void*); // 8
    // Callback called after
    s32 (*cbRegAfter)(s32, s32, void*, void*); // 12
    // Callback called before resetting handler to 0
    s32 (*cbRelBefore)(s32, s32); // 16
    // Callback called after
    s32 (*cbRelAfter)(s32, s32); // 20
    s32 (*cbEnable)(s32, s32); // 24
    s32 (*cbDisable)(s32, s32); // 28
    s32 (*cbSuspend)(s32, s32, s32*); // 32
    s32 (*cbResume)(s32, s32, s32); // 36
    s32 (*cbIsOccured)(s32, s32); // 40
} SceIntrCb; // Size: 44

typedef struct {
    // Handler address, bit 1 indicates full context save
    s32 handler; // 0
    // GP of the module
    s32 gp; // 4
    // Argument given by sceKernelRegisterIntrHandler
    void *arg; // 8
    u32 callCount; // 12
    u32 minClockLo; // 16
    u32 minClockHi; // 20
    u32 maxClockLo; // 24
    u32 maxClockHi; // 28
    u32 totalClockLo; // 32
    u32 totalClockHi; // 36
    // Pointer to sub interrupts
    SubInterrupt *subIntrs; // 40
    // Sub-interrupt callback table registered via sceKernelRegisterIntrHandler
    SceIntrCb *cb; // 44
    // Lower byte is the max number of sub interrupts; bits 8..9 level; bit 10/11 user-mode; bit 15 logging disable; bits 16..23 intrNum
    union {
        u32 flags;
        struct {
            u8 subCount;
            u8 attr;
            u8 intrNum;
            u8 isSub;
        } bits;
    }; // 48
    s32 reserved[3]; // 52
} Interrupt; // Size: 64

typedef struct {
    s32 size; // must be 12
    s32 numSubIntrs; // 4
    SceIntrCb *callbacks; // 8
} SubIntrInfo; // Size: 12

typedef struct {
    s32 size;
    s32 attr;
    void *cb;
} SceIntrHandler;

typedef struct {
    s32 size; // 0 (must be 56)
    s32 entry; // 4
    void *common; // 8
    s32 gp; // 12
    s16 intrCode; // 16
    s16 subCount; // 18
    s16 intrLevel; // 20
    s16 enabled; // 22
    u32 calls; // 24
    u32 reserved; // 28
    u32 totalClockLo; // 32
    u32 totalClockHi; // 36
    u32 minClockLo; // 40
    u32 minClockHi; // 44
    u32 maxClockLo; // 48
    u32 maxClockHi; // 52
} PspIntrHandlerOptionParam; // Size: 56

typedef s32 (*MonitorCb)(s32 intrNum, s32 subIntrNum, s32, s32, s32, s32, u8);

s32 sceKernelRegisterIntrHandler(s32 intrNum, s32 saveContext, void *func, void *common, SceIntrHandler *handler);
s32 sceKernelSetUserModeIntrHanlerAcceptable(s32 intrNum, s32 subIntrNum, s32 setBit);
s32 sceKernelReleaseIntrHandler(s32 intrNum);
s32 sceKernelSetIntrLevel(s32 intrNum, s32 level);
s32 sceKernelSetIntrLogging(s32 intrNum, s32 enable);
s32 sceKernelEnableIntr(s32 intNum);
s32 sceKernelSuspendIntr(s32 intrNum, s32 *prevState);
s32 sceKernelResumeIntr(s32 intrNum, s32 prevState);
void ReleaseContextHooks(void);
void InterruptManagerForKernel_E790EAED(s32 (*beforeHook)(), s32 (*afterHook)());
s32 sceKernelCallSubIntrHandler(s32 intrNum, s32 subIntrNum, s32 userArg, s32 extraArg);
s32 sceKernelGetUserIntrStack(void);
s32 sceKernelRegisterSubIntrHandler(s32 intrNum, s32 subIntrNum, void *handler, void *arg);
s32 sceKernelReleaseSubIntrHandler(s32 intrNum, s32 subIntrNum);
s32 sceKernelEnableSubIntr(s32 intrNum, s32 subIntrNum);
s32 sceKernelDisableSubIntr(s32 intrNum, s32 subIntrNum);
s32 sceKernelSuspendSubIntr(s32 intrNum, s32 subIntrNum, s32 *prevState);
s32 sceKernelResumeSubIntr(s32 intrNum, s32 subIntrNum, s32 prevState);
s32 sceKernelIsSubInterruptOccurred(s32 intrNum, s32 subIntrNum);
s32 sceKernelQueryIntrHandlerInfo(s32 intrNum, s32 subIntrNum, PspIntrHandlerOptionParam *out);
s32 sceKernelSetPrimarySyscallHandler(s32 syscallId, void (*syscall)());
void sceKernelCpuEnableIntr(void);
s32 InterruptManagerForKernel_6FCBA912(s32 set);
s32 sceKernelClearIntrLogging(s32 intrNum);
s32 sceKernelIsInterruptOccurred(s32 intrNum);
s32 sceKernelDisableIntr(s32 intrNum);
void RegisterSubIntrruptMonitor(MonitorCb before, MonitorCb after);
void ReleaseSubIntrruptMonitor(void);
s32 UnSupportIntr(s32 intrNum);
s32 InterruptManagerForKernel_8DFBD787(void);
s32 QueryIntrHandlerInfoForUser(void);
s32 sceKernelRegisterUserSpaceIntrStack(s32 addr, s32 size, s32 stackFlagAddr);
s32 sceKernelGetCpuClockCounter(void);
u64 sceKernelGetCpuClockCounterWide(void);
u32 _sceKernelGetCpuClockCounterLow(void);
s32 sceKernelRegisterSystemCallTable(SceSyscallTable *newMap);
s32 sceKernelQuerySystemCall(void (*sysc)());
void InterruptManagerForKernel_E526B767(s32 mode);
s32 sceKernelGetSyscallRA(void);
s32 sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(s32 intr);
void sceKernelCpuResumeIntrWithSync(s32 intr);
s32 sceKernelIsIntrContext(void);
int sceKernelCallUserIntrHandler(int, int, int, int, int, int);
