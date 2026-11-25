#ifndef THREADMAN_INTERNAL_H
#define THREADMAN_INTERNAL_H

#include <stddef.h>

typedef unsigned char u8;
typedef signed char s8;
typedef unsigned short u16;
typedef signed short s16;
typedef unsigned int u32;
typedef signed int s32;
typedef unsigned long long u64;
typedef signed long long s64;

static inline u32 byteswap32(u32 v)
{
    return ((v & 0x000000FFU) << 24) |
           ((v & 0x0000FF00U) << 8)  |
           ((v & 0x00FF0000U) >> 8)  |
           ((v & 0xFF000000U) >> 24);
}
static inline u16 byteswap16(u16 v) { return (u16)((v << 8) | (v >> 8)); }
static inline u16 _wsbh(u16 v) { return byteswap16(v); }
static inline s32 tman_min_s32(s32 a, s32 b) { return a < b ? a : b; }
static inline s32 tman_max_s32(s32 a, s32 b) { return a > b ? a : b; }

#define tman_cfc0(reg) ({ u32 _v; __asm__ volatile("cfc0 %0, $" #reg : "=r"(_v)); _v; })
#define tman_ctc0(reg, val) do { u32 _v = (u32)(val); __asm__ volatile("ctc0 %0, $" #reg :: "r"(_v)); } while (0)
static inline u32 _mfc0(u32 reg, u32 sel)
{
    u32 v = 0;
    (void)sel;
    if (reg == 12) __asm__ volatile("mfc0 %0, $12" : "=r"(v));
    else if (reg == 13) __asm__ volatile("mfc0 %0, $13" : "=r"(v));
    else if (reg == 22) __asm__ volatile("mfc0 %0, $22" : "=r"(v));
    else __asm__ volatile("mfc0 %0, $25" : "=r"(v));
    return v;
}
static inline void _mtc0(u32 reg, u32 sel, u32 v)
{
    (void)sel;
    if (reg == 12) __asm__ volatile("mtc0 %0, $12" :: "r"(v));
    else if (reg == 13) __asm__ volatile("mtc0 %0, $13" :: "r"(v));
    else if (reg == 14) __asm__ volatile("mtc0 %0, $14" :: "r"(v));
    else __asm__ volatile("mtc0 %0, $25" :: "r"(v));
}
static inline void tman_mtic_0(u32 v)
{
    __asm__ volatile("mtic %0, $0" :: "r"(v));
}

#define LODWORD(x) ((u32)((u64)(x)))
#define HIDWORD(x) ((u32)((u64)(x) >> 32))
#define SHIDWORD(x) ((s32)((u64)(x) >> 32))
#define LOWORD(x)  ((u16)((u32)(x)))
#define HIWORD(x)  ((u16)((u32)(x) >> 16))
#define LOBYTE(x)  ((u8)((u32)(x)))
#define HIBYTE(x)  ((u8)((u32)(x) >> 8))
#define BYTE1(x)   ((u8)((u32)(x) >> 8))
#define BYTE2(x)   ((u8)((u32)(x) >> 16))
#define BYTE3(x)   ((u8)((u32)(x) >> 24))
#define __PAIR64__(hi, lo) (((u64)(u32)(hi) << 32) | (u32)(lo))
#define __CFADD__(x, y)    ((u32)((u32)(x) + (u32)(y)) < (u32)(x))
#define true 1
#define false 0
#define _break(...) __asm__ volatile("break 7")
#define _sync() __asm__ volatile("sync")
#define JUMPOUT(addr) do { return 0; } while (0)

static inline u32 _clz(u32 v)
{
    u32 r;
    __asm__("clz %0, %1" : "=r"(r) : "r"(v));
    return r;
}

static inline u32 __builtin_rotateright32(u32 v, u32 n)
{
    n &= 31;
    return n ? ((v >> n) | (v << (32 - n))) : v;
}

static inline u32 getInterruptMask(void)
{
    u32 v;
    __asm__ volatile("mfic %0, $0" : "=r"(v));
    return v;
}

static inline void _cache(u32 op, u32 addr)
{
    switch (op) {
        case 0x19: __asm__ volatile("cache 0x19, 0(%0)" :: "r"(addr)); break;
        case 0x1a: __asm__ volatile("cache 0x1a, 0(%0)" :: "r"(addr)); break;
        case 0x1b: __asm__ volatile("cache 0x1b, 0(%0)" :: "r"(addr)); break;
        default:   __asm__ volatile("cache 0x1b, 0(%0)" :: "r"(addr)); break;
    }
}

typedef struct {
    u32 nid;
    void *func;
} TmanUidFunc;

extern u8 g_tman_bss[0x860];
#define TMAN_BSS_U64(off) (*(u64 *)(g_tman_bss + (off)))
#define TMAN_BSS_U32(off) (*(u32 *)(g_tman_bss + (off)))
#define TMAN_BSS_U8(off)  (*(u8 *)(g_tman_bss + (off)))

extern void tman_idle_loop(void);
extern void tman_vfpu_save(void *ctx);
extern void tman_vfpu_restore(void *ctx);
extern void tman_vfpu_flush(void);
extern void tman_vfpu_exc_handler(void);
extern void tman_eret(void);
extern void tman_idle_halt(void);

extern const TmanUidFunc g_uid_waitq_funcs[6];
extern const TmanUidFunc g_uid_threv_funcs[3];
extern const TmanUidFunc g_uid_cb_funcs[3];
extern const TmanUidFunc g_uid_sema_funcs[4];
extern const TmanUidFunc g_uid_evf_funcs[4];
extern const TmanUidFunc g_uid_mutex_funcs[4];
extern const TmanUidFunc g_uid_lwmutex_funcs[5];
extern const TmanUidFunc g_uid_mbx_funcs[3];
extern const TmanUidFunc g_uid_msgpipe_funcs[5];
extern const TmanUidFunc g_uid_vpl_funcs[4];
extern const TmanUidFunc g_uid_fpl_funcs[3];
extern const TmanUidFunc g_uid_tlspl_funcs[3];
extern const TmanUidFunc g_uid_delay_funcs[3];
extern const TmanUidFunc g_uid_alarm_funcs[3];
extern const TmanUidFunc g_uid_vtimer_funcs[3];
extern const TmanUidFunc g_uid_timer_funcs[3];
extern const TmanUidFunc g_uid_thread_funcs[3];

/* Imported kernel functions */
extern int ExcepManCB();
extern int ExceptionManagerForKernel_96D47F7E();
extern int InterruptManagerForKernel_551097F2();
extern int InterruptManagerForKernel_E790EAED();
extern int Kprintf();
extern int SaveThreadContext();
extern int intInfo();
extern int memcpy();
extern int memset();
extern int name();
extern int sceKernelAllocHeapMemory();
extern int sceKernelAllocHeapMemoryWithOption();
extern int sceKernelAllocPartitionMemory();
extern int sceKernelCallUIDObjCommonFunction();
extern int sceKernelCallUIDObjFunction();
extern int sceKernelCallUserIntrHandler();
extern int sceKernelCpuEnableIntr();
extern int sceKernelCpuResumeIntr();
extern int sceKernelCpuResumeIntrWithSync();
extern int sceKernelCpuSuspendIntr();
extern int sceKernelCreateHeap();
extern int sceKernelCreateUID();
extern int sceKernelCreateUIDtype();
extern int sceKernelCreateUIDtypeInherit();
extern int sceKernelDeleteHeap();
extern int sceKernelDeleteUID();
extern int sceKernelDipsw();
extern int sceKernelDisableIntr();
extern int sceKernelEnableIntr();
extern int sceKernelFreeHeapMemory();
extern int sceKernelFreePartitionMemory();
extern int sceKernelGetBlockHeadAddr();
extern int sceKernelGetCompiledSdkVersion();
extern int sceKernelGetInitialRandomValue();
extern int sceKernelGetModuleGPByAddressForKernel();
extern int sceKernelGetUIDcontrolBlock();
extern int sceKernelGetUIDcontrolBlockWithType();
extern int sceKernelGetUserIntrStack();
extern int sceKernelHeapTotalFreeSize();
extern int sceKernelIsDevelopmentToolMode();
extern int sceKernelIsKindOf();
extern int sceKernelMemset32();
extern int sceKernelQueryMemoryBlockInfo();
extern int sceKernelQueryMemoryInfo();
extern int sceKernelQueryMemoryPartitionInfo();
extern int sceKernelQuerySystemCall();
extern int sceKernelRegisterExceptionHandler();
extern int sceKernelRegisterIntrHandler();
extern int sceKernelRegisterResumeHandler();
extern int sceKernelRegisterSuspendHandler();
extern int sceKernelReleaseExceptionHandler();
extern int sceKernelReleaseIntrHandler();
extern int sceKernelSetBootCallbackLevel();
extern int sceKernelSetIntrLevel();
extern int sceKernelSetPrimarySyscallHandler();
extern int snprintf();
extern int strncpy();

/* Module functions */
u32 tman_sched_fn_1( int a1, float a2, float a3, float a4, float a5, float a6, float a7, float a8, float a9);
int tman_sched_fn_2();
u32 * tman_sched_fn_3();
u32 * tman_sched_fn_4();
int tman_sched_fn_5();
int tman_sched_fn_6();
int tman_sched_fn_7();
int tman_sched_fn_8();
int tman_sched_fn_9();
int tman_sched_fn_10();
int tman_sched_fn_11();
int tman_sched_fn_12();
int tman_sched_fn_13();
int tman_sched_fn_14();
int tman_sched_fn_15();
int tman_sched_fn_16();
int tman_sched_fn_17();
int tman_sched_fn_18();
void tman_sched_fn_19();
int sceKernelRegisterThreadEventHandler(int a1, int a2, int a3, int a4, int a5);
int sceKernelReleaseThreadEventHandler(int a1);
int tman_sched_fn_20();
int sceKernelReferThreadEventHandlerStatus(int a1, unsigned int *a2);
int sceKernelCreateCallback(int a1, int a2, int a3);
int sceKernelDeleteCallback(int a1);
int sceKernelNotifyCallback(int a1, int a2);
int sceKernelCancelCallback(int a1);
int sceKernelGetCallbackCount(int a1);
int sceKernelCheckCallback();
int tman_sched_fn_21();
int sceKernelReferCallbackStatus(int a1, unsigned int *a2);
int sceKernelReturnFromCallback(int a1, int a2);
void tman_sched_fn_22();
int tman_sched_fn_23();
int tman_sched_fn_24();
int ThreadManForUser_8672E3D0(int a1, int a2);
void tman_sched_fn_25();
int tman_sched_fn_26();
int tman_sched_fn_27();
int tman_sched_fn_28();
int tman_sched_fn_29();
int tman_sched_fn_30();
int tman_sched_fn_31();
int tman_sched_fn_32();
int tman_sched_fn_33();
int sceKernelSleepThread(int a1, int a2);
int sceKernelSleepThreadCB(int a1, int a2);
int sceKernelWakeupThread(int a1, int a2);
int sceKernelCancelWakeupThread(int a1, int a2);
int sceKernelDonateWakeupThread(int a1, int a2);
int sceKernelSuspendThread(int a1, int a2);
int sceKernelResumeThread(int a1, int a2);
int sceKernelSuspendAllUserThreads(int a1, int a2);
int sceKernelWaitThreadEnd(int a1, int a2, int a3, int a4);
void sceKernelDelayThread();
int sceKernelDelayThreadCB(int a1, int a2);
void sceKernelDelaySysClockThread();
int sceKernelDelaySysClockThreadCB(unsigned int a1);
int tman_sched_fn_34();
int sceKernelCreateSema(int a1, int a2, int a3, int a4, int a5);
int sceKernelDeleteSema(int a1, int a2);
int sceKernelSignalSema(int a1, int a2);
int sceKernelWaitSema(int a1, int a2, int a3);
int sceKernelPollSema(int a1, int a2);
int sceKernelCancelSema(int a1, int a2, int *a3);
int tman_sync_fn_1();
int sceKernelReferSemaStatus(int a1, unsigned int *a2);
int tman_sync_fn_2();
int tman_sync_fn_3();
int tman_sync_fn_4();
int tman_sync_fn_5();
int sceKernelCreateEventFlag(int a1, int a2, int a3, int a4);
int sceKernelDeleteEventFlag(int a1, int a2);
int sceKernelSetEventFlag(int a1, int a2);
int sceKernelClearEventFlag(int a1, int a2);
void sceKernelWaitEventFlag();
int sceKernelWaitEventFlagCB(int a1, int a2, int a3, unsigned int a4, int a5, int a6);
int sceKernelPollEventFlag(int a1, int a2, int a3, unsigned int a4);
int sceKernelCancelEventFlag(int a1, int a2, int *a3);
int sceKernelReferEventFlagStatus(int a1, unsigned int *a2);
int tman_sync_fn_6();
int tman_sync_fn_7();
int tman_sync_fn_8();
int tman_sync_fn_9();
int tman_sync_fn_10();
int sceKernelCreateMutex(int a1, int a2, int a3, int *a4);
int sceKernelDeleteMutex(int a1, int a2);
u32 * tman_sync_fn_11();
void tman_sync_fn_12();
int sceKernelUnlockMutex(int a1, int a2);
void sceKernelLockMutex();
int sceKernelLockMutexCB(int a1, int a2, int a3);
int sceKernelTryLockMutex(int a1, int a2);
int sceKernelCancelMutex(int a1, int a2, int *a3);
int sceKernelReferMutexStatus(int a1, unsigned int *a2);
int tman_sync_fn_13();
void tman_sync_fn_14();
int tman_sync_fn_15();
int tman_sync_fn_16();
int tman_sync_fn_17();
int sceKernelCreateLwMutex(int *a1, int a2, int a3, int a4, int *a5, int a6);
int sceKernelDeleteLwMutex(int a1);
int sceKernelUnlockLwMutex(unsigned int a1, int a2);
void sceKernelLockLwMutex();
int sceKernelLockLwMutexCB(int *a1, int a2, int a3, int a4, int a5, int a6);
int sceKernelTryLockLwMutex(int *a1, int a2);
int sceKernelReferLwMutexStatusByID(int a1, unsigned int *a2);
int tman_sync_fn_18();
int tman_sync_fn_19();
int tman_sync_fn_20();
int tman_sync_fn_21();
int tman_sync_fn_22();
int sceKernelCreateMbx(int a1, int a2, int a3, int a4);
int sceKernelDeleteMbx(int a1, int a2);
int sceKernelSendMbx(int a1, int a2);
int sceKernelReceiveMbx(int a1, unsigned int a2, int a3, int a4, int a5, int a6);
int sceKernelPollMbx(int a1, unsigned int a2);
int sceKernelCancelReceiveMbx(int a1, int *a2);
int sceKernelReferMbxStatus(int a1, int *a2);
int tman_sync_fn_23();
int tman_sync_fn_24();
int tman_sync_fn_25();
int tman_sync_fn_26();
int sceKernelCreateMsgPipe(int a1, int a2, int a3, int a4, int a5);
int sceKernelDeleteMsgPipe(int a1, int a2);
void sceKernelSendMsgPipe();
int sceKernelSendMsgPipeCB(int a1, int a2, unsigned int a3, int a4, unsigned int a5, int a6);
int sceKernelTrySendMsgPipe(int a1, int a2, unsigned int a3, int a4, unsigned int a5);
int sceKernelReceiveMsgPipe(int a1, int a2, unsigned int a3, int a4, u32 *a5, int a6);
int sceKernelTryReceiveMsgPipe(int a1, int a2, unsigned int a3, int a4, unsigned int a5);
int sceKernelCancelMsgPipe(int a1, unsigned int a2, unsigned int a3);
int sceKernelReferMsgPipeStatus(int a1, unsigned int *a2);
int tman_mem_fn_1();
int tman_mem_fn_2();
int tman_mem_fn_3();
int tman_mem_fn_4();
int tman_mem_fn_5();
int tman_mem_fn_6();
int tman_mem_fn_7();
int tman_mem_fn_8();
int sceKernelCreateVpl(int a1, int a2, int a3, int a4, int a5);
int sceKernelDeleteVpl(int a1, int a2);
int sceKernelAllocateVpl(int a1, unsigned int a2, int *a3, int a4, int a5, int a6);
int sceKernelTryAllocateVpl(int a1, unsigned int a2, unsigned int a3);
int sceKernelFreeVpl(int a1, int a2);
int sceKernelCancelVpl(int a1, int *a2);
int sceKernelReferVplStatus(int a1, unsigned int *a2);
int tman_mem_fn_9();
int tman_mem_fn_10();
int tman_mem_fn_11();
int tman_mem_fn_12();
int tman_mem_fn_13();
int sceKernelCreateFpl(int a1, int a2, int a3, int a4, signed int a5, int *a6);
int sceKernelDeleteFpl(int a1, int a2);
int sceKernelAllocateFpl(int a1, unsigned int a2, int a3, int a4, int a5, int a6);
int sceKernelTryAllocateFpl(int a1, unsigned int a2);
int sceKernelFreeFpl(int a1, unsigned int a2);
int sceKernelCancelFpl(int a1, int *a2);
int sceKernelReferFplStatus(int a1, unsigned int *a2);
int tman_mem_fn_14();
int tman_mem_fn_15();
int tman_mem_fn_16();
int tman_mem_fn_17();
int tman_mem_fn_18();
int tman_mem_fn_19();
int tman_mem_fn_20();
int tman_mem_fn_21();
int sceKernelCreateTlspl(int a1, int a2, int a3, int *a4, signed int a5, int *a6);
int sceKernelDeleteTlspl(unsigned int a1, int a2);
int sceKernelAllocateTlspl(unsigned int a1, int *a2, int a3, int a4, int a5);
int sceKernelFreeTlspl(unsigned int a1, int a2);
int tman_mem_fn_22();
int sceKernelReferTlsplStatus(unsigned int a1, unsigned int *a2);
unsigned int tman_mem_fn_23();
int tman_mem_fn_24();
int tman_mem_fn_25();
int tman_mem_fn_26();
int tman_mem_fn_27();
int tman_mem_fn_28();
int tman_mem_fn_29();
unsigned int tman_mem_fn_30();
int tman_mem_fn_31();
int sceKernelUSec2SysClock(int a1, unsigned int a2);
int tman_mem_fn_32();
int sceKernelCancelAlarm(int a1, int a2);
int tman_mem_fn_33();
int sceKernelReferAlarmStatus(int a1, unsigned int *a2);
int sceKernelCreateVTimer(int a1, int a2);
int sceKernelDeleteVTimer(int a1, int a2);
int sceKernelGetVTimerBase(int a1, unsigned int a2);
s64 sceKernelGetVTimerBaseWide(int a1, int a2);
int sceKernelGetVTimerTime(int a1, unsigned int a2);
s64 sceKernelGetVTimerTimeWide(int a1, int a2);
int sceKernelSetVTimerTime(int a1, unsigned int a2);
s64 sceKernelSetVTimerTimeWide(int a1, int a2, s64 a3);
int sceKernelStartVTimer(int a1, int a2);
int sceKernelStopVTimer(int a1, int a2);
int sceKernelSetVTimerHandler(int a1, unsigned int a2, int a3, int a4);
int sceKernelSetVTimerHandlerWide(int a1, int a2, int a3, int a4, int a5, int a6);
int sceKernelCancelVTimerHandler(int a1, int a2);
int sceKernelReferVTimerStatus(int a1, unsigned int *a2);
int sceKernelReturnFromTimerHandler();
s64 sceKernelUSec2SysClockWide(unsigned int a1);
int sceKernelSysClock2USec(unsigned int a1, unsigned int a2, unsigned int a3);
int sceKernelSysClock2USecWide(int a1, int a2, unsigned int a3, unsigned int a4);
int sceKernelGetSystemTime(int *a1);
s64 sceKernelGetSystemTimeWide(int a1, int a2);
int sceKernelGetSystemTimeLow();
int tman_mem_fn_34();
int tman_mem_fn_35();
int sceKernelSetAlarm(int a1, int a2, int a3, int a4, int a5, int a6);
int sceKernelSetSysClockAlarm(unsigned int a1, int a2, int a3);
int tman_mem_fn_36();
int tman_mem_fn_37();
int tman_mem_fn_38();
void tman_mem_fn_39();
int tman_mem_fn_40();
int tman_mem_fn_41();
int tman_mem_fn_42();
int tman_mem_fn_43();
int tman_mem_fn_44();
int tman_mem_fn_45();
int tman_mem_fn_46();
int tman_mem_fn_47();
int tman_mem_fn_48();
int tman_mem_fn_49();
int tman_mem_fn_50();
int tman_mem_fn_51();
int tman_mem_fn_52();
int tman_mem_fn_53();
int sceKernelAllocateKTLS(int a1, int a2, int a3);
int sceKernelFreeKTLS(unsigned int a1, int a2);
int sceKernelGetThreadKTLS(unsigned int a1, int a2, int a3);
int tman_thread_fn_1();
int sceKernelGetKTLS(unsigned int a1);
int module_bootstart(int a1, int a2);
int tman_thread_fn_2();
void tman_thread_fn_3();
int tman_thread_fn_4();
unsigned int tman_thread_fn_5();
int tman_thread_fn_6();
void tman_thread_fn_7(u32 *a1);
int sceKernelCreateThread(const char *a1, int a2, int a3, unsigned int a4, int a5, int *a6);
int sceKernelDeleteThread(int a1, int a2);
u32 * tman_thread_fn_8();
int sceKernelStartThread(int a1, int a2, int a3, int a4, int a5);
u32 sceKernelExitThread(int a1);
u32 sceKernelExitDeleteThread(int a1);
int sceKernelTerminateThread(int a1, int a2);
int sceKernelTerminateDeleteThread(int a1, int a2);
int tman_thread_fn_9();
int sceKernelSuspendDispatchThread(int a1, int a2);
int tman_thread_fn_10();
int sceKernelResumeDispatchThread(int a1, int a2);
int sceKernelChangeCurrentThreadAttr(int a1, int a2);
int tman_thread_fn_11();
int sceKernelChangeThreadPriority(int a1, s32 a2);
int sceKernelRotateThreadReadyQueue(u32 a1);
int sceKernelReleaseWaitThread(int a1, int a2);
int sceKernelGetThreadExitStatus(int a1, int a2);
int sceKernelReturnFromChangeStack(int a1, int a2);
int ThreadManForUser_28BFD974(int a1, int a2, int a3);
int sceKernelExtendThreadStack(unsigned int a1, int a2, int a3);
int sceKernelExtendKernelStack(unsigned int a1, int (*a2)(int), int a3);
void tman_thread_fn_12();
int tman_thread_fn_13();
int sceKernelCheckThreadStack(int a1, char *a2);
int sceKernelGetThreadStackFreeSize(int a1, int a2);
int sceKernelGetThreadKernelStackFreeSize(int a1, int a2);
int sceKernelReferThreadStatus(int a1, unsigned int *a2);
int sceKernelReferThreadRunStatus(int a1, unsigned int *a2);
int sceKernelGetThreadmanIdList(int a1, int *a2, int a3, unsigned int a4);
int sceKernelGetThreadmanIdType(int a1, int a2);
int sceKernelGetThreadmanIdOwner(int a1, int *a2);
// write access to const memory has been detected, the output may be wrong! int ThreadManForKernel_D366D35A(int a1);
int tman_thread_fn_14();
int module_reboot_before(int a1, int a2);
int sceKernelSetThreadmanLogHandler(int (*a1)(u32, u32, u32, u32), u32 *a2);
void tman_thread_fn_15();
void tman_thread_fn_16();
u32 _sceKernelExitThread();
int sceKernelGetThreadId();
int sceKernelGetThreadCurrentPriority();
int sceKernelGetUserLevel();
int sceKernelIsUserModeThread();
int tman_thread_fn_17();
int sceKernelCheckThreadKernelStack(int a1, char *a2, int a3, int a4);
int sceKernelGetSystemStatusFlag();
int sceKernelRegisterDeadThreadHandler(int a1);
int sceKernelReferSystemStatus(int *a1);
int sceKernelReferThreadProfiler(int a1, int a2, int a3);
int sceKernelReferGlobalProfiler();
int tman_thread_fn_18();
u32 tman_thread_fn_19();
u32 ** tman_thread_fn_20();
int tman_thread_fn_21();
int tman_thread_fn_22();
int tman_thread_fn_23();
void tman_thread_fn_24();
int tman_thread_fn_25();
int tman_thread_fn_26();
int tman_thread_fn_27( int a1, int a2, float a3, float a4, float a5, float a6, float a7, float a8, float a9, float a10);
int tman_thread_fn_28();
int tman_thread_fn_29();
int tman_thread_fn_30();

#endif /* THREADMAN_INTERNAL_H */
