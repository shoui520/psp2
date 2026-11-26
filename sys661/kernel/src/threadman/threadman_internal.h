#ifndef THREADMAN_INTERNAL_H
#define THREADMAN_INTERNAL_H

#include <stddef.h>
#include <stdarg.h>
#include "common_header.h"

/* Standard PSP ThreadMan types & constants */
typedef s32 (*SceKernelThreadEntry)(SceSize args, void *argp);
typedef s32 (*SceKernelCallbackFunction)(s32 notify_count, s32 notify_arg, void *common);
typedef SceUInt (*SceKernelAlarmHandler)(void *common);

typedef struct SceSysmemUidCB {
    struct SceSysmemUidCB *PARENT0;
    struct SceSysmemUidCB *nextChild;
    struct SceSysmemUidCB *meta;
    SceUID uid;
    char *name;
    u8 childSize;
    u8 size;
    u16 attr;
    struct SceSysmemUidCB *next;
    struct SceSysmemUidCB *PARENT1;
    struct SceSysmemUidCB *childList;
} SceSysmemUidCB;

typedef struct SceSysmemUidLookupFunc {
    u32 id;
    void *func;
} SceSysmemUidLookupFunc;

typedef struct {
    SceUInt32 low;
    SceUInt32 hi;
} SceKernelSysClock;

typedef struct {
    SceSize size;
    SceUID stackMpid;
} SceKernelThreadOptParam;

typedef struct {
    SceSize size;
} SceKernelSemaOptParam, SceKernelEventFlagOptParam, SceKernelMutexOptParam,
  SceKernelVplOptParam, SceKernelFplOptParam;

typedef struct {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    SceUInt attr;
    int initCount;
    int currentCount;
    int maxCount;
    int numWaitThreads;
} SceKernelSemaInfo;

typedef struct {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    SceUInt attr;
    SceUInt initPattern;
    SceUInt currentPattern;
    int numWaitThreads;
} SceKernelEventFlagInfo;

typedef struct {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    SceUInt attr;
    s32 initCount;
    s32 currentCount;
    SceUID currentOwner;
    s32 numWaitThreads;
} SceKernelMutexInfo;

typedef struct {
    s32 lockCount;
    s32 thid;
    u32 flags;
    s32 numWaitThreads;
    s32 id;
} SceLwMutex;

typedef struct {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    SceUInt attr;
    int bufSize;
    int freeSize;
    int numSendWaitThreads;
    int numReceiveWaitThreads;
} SceKernelMppInfo;

typedef struct {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    SceUInt attr;
    int poolSize;
    int freeSize;
    int numWaitThreads;
} SceKernelVplInfo;

typedef struct {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    SceUInt attr;
    int blockSize;
    int numBlocks;
    int freeBlocks;
    int numWaitThreads;
} SceKernelFplInfo;

typedef struct {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    SceUID threadId;
    SceKernelCallbackFunction callback;
    void *common;
    s32 notifyCount;
    s32 notifyArg;
} SceKernelCallbackInfo;

typedef struct {
    SceSize size;
    SceKernelSysClock schedule;
    SceKernelAlarmHandler handler;
    void *common;
} SceKernelAlarmInfo;

typedef enum {
    SCE_KERNEL_TMID_Thread             = 1,
    SCE_KERNEL_TMID_Semaphore          = 2,
    SCE_KERNEL_TMID_EventFlag          = 3,
    SCE_KERNEL_TMID_Mbox               = 4,
    SCE_KERNEL_TMID_Vpl                = 5,
    SCE_KERNEL_TMID_Fpl                = 6,
    SCE_KERNEL_TMID_Mpipe              = 7,
    SCE_KERNEL_TMID_Callback           = 8,
    SCE_KERNEL_TMID_ThreadEventHandler = 9,
    SCE_KERNEL_TMID_Alarm              = 10,
    SCE_KERNEL_TMID_VTimer             = 11,
    SCE_KERNEL_TMID_SleepThread        = 64,
    SCE_KERNEL_TMID_DelayThread        = 65,
    SCE_KERNEL_TMID_SuspendThread      = 66,
    SCE_KERNEL_TMID_DormantThread      = 67,
} SceKernelIdListType;

#define SCE_KERNEL_TH_USER_MODE          0x80000000U
#define SCE_KERNEL_TH_NO_FILLSTACK       0x00100000U
#define SCE_KERNEL_TH_CLEAR_STACK        0x00200000U
#define SCE_KERNEL_TH_LOW_STACK          0x00400000U
#define SCE_KERNEL_TH_UNK_800000         0x00800000U
#define SCE_KERNEL_TH_USE_VFPU           0x00004000U
#define SCE_KERNEL_AT_THFIFO             0x00000000U
#define SCE_KERNEL_AT_THPRI              0x00000100U
#define SCE_KERNEL_MUTEX_ATTR_RECURSIVE  0x0200U
#define SCE_KERNEL_LWMUTEX_RECURSIVE     0x0200U
#define SCE_KERNEL_EA_MULTI              0x0200U
#define SCE_KERNEL_EW_OR                 0x01U
#define SCE_KERNEL_EW_CLEAR_ALL          0x10U
#define SCE_KERNEL_EW_CLEAR_PAT          0x20U

/* Wait types */
enum TmanWaitType {
    TMAN_WAIT_NONE          = 0,
    TMAN_WAIT_SLEEP         = 1,
    TMAN_WAIT_DELAY         = 2,
    TMAN_WAIT_SEMA          = 3,
    TMAN_WAIT_EVENTFLAG     = 4,
    TMAN_WAIT_MBX           = 5,
    TMAN_WAIT_VPL           = 6,
    TMAN_WAIT_FPL           = 7,
    TMAN_WAIT_MSGPIPE       = 8,
    TMAN_WAIT_THREAD_END    = 9,
    TMAN_WAIT_EVENT_HANDLER = 10,
    TMAN_WAIT_CALLBACK_DEL  = 11,
    TMAN_WAIT_MUTEX         = 12,
    TMAN_WAIT_LWMUTEX       = 13,
    TMAN_WAIT_TLSPL         = 14,
};

/* Thread statuses */
enum TmanThreadStatus {
    TMAN_TH_RUNNING     = 1,
    TMAN_TH_READY       = 2,
    TMAN_TH_WAIT        = 4,
    TMAN_TH_SUSPEND     = 8,
    TMAN_TH_WAITSUSPEND = 12,
    TMAN_TH_STOPPED     = 16,
    TMAN_TH_KILLED      = 32,
};

/* Thread event masks */
enum TmanThreadEventMask {
    TMAN_THREV_CREATE = 1,
    TMAN_THREV_START  = 2,
    TMAN_THREV_EXIT   = 4,
    TMAN_THREV_DELETE = 8,
};

/* UID function table IDs */
#define TMAN_UID_FUNC_CREATE        0xD310D2D9U
#define TMAN_UID_FUNC_DELETE        0x87089863U
#define TMAN_UID_FUNC_CANCEL_WAIT   0x86D94883U
#define TMAN_UID_FUNC_CHECK_WAIT    0x85FE2B4CU
#define TMAN_UID_FUNC_REMOVE_WAIT   0x5BD49380U

/* Additional kernel error codes used by ThreadMan */
#define SCE_ERROR_KERNEL_THREAD_ALREADY_WAIT        0x800201A6
#define SCE_ERROR_KERNEL_ILLEGAL_PERMISSIONS        SCE_ERROR_KERNEL_ILLEGAL_PERMISSION

/* Hardware timer and profiler MMIO registers */
#define TMAN_REG_SYS_TIME_LO        (*(vu32 *)0xBC600000U)
#define TMAN_REG_SYS_TIME_CMP       (*(vu32 *)0xBC600004U)
#define TMAN_REG_SYS_TIME_CTRL      (*(vu32 *)0xBC600008U)
#define TMAN_REG_SYS_TIME_PRESCALE  (*(vu32 *)0xBC60000CU)
#define TMAN_REG_SYS_TIME_MODE      (*(vu32 *)0xBC600010U)
#define TMAN_REG_PROF_BASE          ((vu32 *)0xBC400000U)
#define TMAN_REG_SYS_CTRL_30        (*(vu32 *)0xBC000030U)
#define TMAN_REG_VFPU_CTRL          (*(vu32 *)0xBC000044U)

/* Allegrex COP0 / K1 / cache helpers */
static inline u32 tmanGetK1(void)
{
    u32 val;
    __asm__ volatile("move %0, $27" : "=r"(val));
    return val;
}

static inline u32 tmanGetGP(void)
{
    u32 val;
    __asm__ volatile("move %0, $28" : "=r"(val));
    return val;
}

static inline int tmanIsUserCaller(void)
{
    return ((s32)(tmanGetK1() << 11)) < 0;
}

static inline int tmanCheckUserPtr(const void *ptr, u32 size)
{
    if (!tmanIsUserCaller())
        return 1;
    if (ptr == NULL)
        return 1;
    u32 addr = (u32)ptr;
    return ((s32)(addr | (addr + size))) >= 0;
}

static inline int tmanCheckUserAddr(u32 addr)
{
    if (!tmanIsUserCaller())
        return 1;
    return ((s32)addr) >= 0;
}

static inline u32 tmanReadIntrDepth(void)
{
    u32 val;
    __asm__ volatile("cfc0 %0, $13" : "=r"(val));
    return val;
}

#define tmanReadCfc0(reg) ({ \
    u32 _tman_v; \
    __asm__ volatile("cfc0 %0, $" #reg : "=r"(_tman_v)); \
    _tman_v; \
})

#define tmanWriteCtc0(reg, val) do { \
    u32 _tman_v = (u32)(val); \
    __asm__ volatile("ctc0 %0, $" #reg :: "r"(_tman_v)); \
} while (0)

#define tmanMfc0(reg) ({ \
    u32 _tman_v; \
    __asm__ volatile("mfc0 %0, $" #reg : "=r"(_tman_v)); \
    _tman_v; \
})

#define tmanMtc0(reg, val) do { \
    u32 _tman_v = (u32)(val); \
    __asm__ volatile("mtc0 %0, $" #reg :: "r"(_tman_v)); \
} while (0)

static inline u32 tmanGetIntrMask(void)
{
    u32 val;
    __asm__ volatile("mfic %0, $0" : "=r"(val));
    return val;
}

static inline void tmanSync(void)
{
    __asm__ volatile("sync");
}

static inline void tmanBreak(void)
{
    __asm__ volatile("break 7");
}

static inline u32 tmanClz(u32 val)
{
    u32 res;
    __asm__("clz %0, %1" : "=r"(res) : "r"(val));
    return res;
}

static inline u32 tmanRor32(u32 val, u32 shift)
{
    u32 res;
    __asm__("rotrv %0, %1, %2" : "=r"(res) : "r"(val), "r"(shift));
    return res;
}

#define tmanCacheOp(op, addr) do { \
    u32 _tman_p = (u32)(addr); \
    __asm__ volatile("cache %0, 0(%1)" :: "i"(op), "r"(_tman_p)); \
} while (0)

/* Intrusive circular doubly-linked list */
typedef struct TmanListNode {
    struct TmanListNode *next;
    struct TmanListNode *prev;
} TmanListNode;

static inline void tmanListInit(TmanListNode *head)
{
    head->next = head;
    head->prev = head;
}

static inline int tmanListEmpty(const TmanListNode *head)
{
    return head->next == head;
}

static inline void tmanListInsertBefore(TmanListNode *pos, TmanListNode *node)
{
    node->next = pos;
    node->prev = pos->prev;
    pos->prev->next = node;
    pos->prev = node;
}

static inline void tmanListInsertTail(TmanListNode *head, TmanListNode *node)
{
    tmanListInsertBefore(head, node);
}

static inline void tmanListRemove(TmanListNode *node)
{
    node->next->prev = node->prev;
    node->prev->next = node->next;
}

/* SysMem UID object access helpers */
static inline void *tmanUidData(SceSysmemUidCB *cb, const SceSysmemUidCB *type)
{
    return (void *)((u32 *)cb + type->size);
}

static inline SceSysmemUidCB *tmanDataToUid(void *data, const SceSysmemUidCB *type)
{
    return (SceSysmemUidCB *)((u32 *)data - type->size);
}

static inline int tmanCheckUidAttr(const SceSysmemUidCB *cb, u16 mask)
{
    if (!tmanIsUserCaller())
        return 1;
    return (cb->attr & mask) != 0;
}

typedef struct TmanUidFunc {
    u32 nid;
    void *func;
} TmanUidFunc;

/* Wait queue base object (20 bytes, UID type "WaitQ") */
typedef struct TmanWaitQueue {
    u32 attr;
    u32 skip_first_pri;
    s32 num_wait_threads;
    TmanListNode wait_list;
} TmanWaitQueue;

/* Saved MIPS Allegrex thread register context (320 bytes, 64-byte aligned) */
typedef struct TmanThreadContext {
    u32 type;
    u32 reserved[8];
    u32 status_flag;
    u32 fcr31;
    u32 hi;
    u32 lo;
    u32 sr;
    u32 epc;
    u32 cause;
    u32 gpr[32];
    float fpr[32];
} TmanThreadContext;

/* Syscall stack header frame */
typedef struct TmanSyscallStack {
    u32 prev_kstack;
    u32 saved_sp;
    u32 user_sp;
    u32 user_ra;
    u32 saved_gp;
    u32 saved_s8;
    u32 return_val;
} TmanSyscallStack;

/* Extended stack allocation record */
typedef struct TmanExtStackNode {
    struct TmanExtStackNode *next;
    void *stack_base;
    SceUID block_id;
    u32 stack_size;
    u32 saved_sp;
    u32 saved_user_ra;
    u32 saved_user_sp;
} TmanExtStackNode;

/* Thread control block (320 bytes, inherits "WaitQ") */
typedef struct TmanThreadCB {
    TmanListNode link;
    SceUID uid;
    u32 status;
    s32 current_priority;
    u32 attr;
    u32 vfpu_dirty;
    TmanThreadContext *ctx;
    void *vfpu_ctx;
    TmanSyscallStack *syscall_stack;
    void *kstack_top;
    void *stack;
    void *kstack;
    u32 run_clocks_lo;
    u32 run_clocks_hi;
    void *profiler_regs;
    u32 *stat_counter_ptr;
    u32 preempt_count;
    u32 release_count;
    u32 wait_release_count;
    s32 wakeup_count;
    s32 exit_status;
    u32 wait_type;
    SceSysmemUidCB *wait_obj;
    u32 wait_param[4];
    u32 wait_extra[2];
    SceUID timeout_delay_id;
    u32 *timeout_ptr;
    s32 wait_result;
    TmanThreadContext *saved_cb_ctx;
    u32 event_mask_active;
    SceSysmemUidCB *active_event_handler;
    TmanListNode cb_list;
    s32 cb_wait_enabled;
    s32 cb_notify_pending;
    s32 cb_nesting;
    SceSysmemUidCB *active_cb;
    TmanListNode owned_mutex_list;
    TmanListNode owned_lwmutex_list;
    s16 init_priority;
    s16 base_priority;
    s32 inherited_priority;
    void *entry;
    u32 stack_size;
    void *init_sp;
    u32 kstack_size;
    TmanExtStackNode *ext_stack_list;
    TmanExtStackNode *ext_kstack_list;
    u32 gp_reg;
    void *user_tls;
    void *ktls[16];
    u32 init_attr;
    u32 arg_len;
    void *arg_ptr;
    void *opt_param;
    SceUID stack_block_id;
    SceUID kstack_block_id;
    SceUID stack_partition_id;
    SceUID kstack_partition_id;
} TmanThreadCB;

/* Semaphore CB (inherits "WaitQ") */
typedef struct TmanSemaCB {
    void *opt;
    s32 cur_count;
    s32 max_count;
    s32 init_count;
} TmanSemaCB;

/* EventFlag CB (inherits "WaitQ") */
typedef struct TmanEvfBlock {
    void *opt;
    u32 init_pattern;
    u32 cur_pattern;
    TmanThreadCB *wait_owner;
    s32 wait_owner_refs;
    TmanListNode owner_link;
} TmanEvfBlock;

/* Mutex CB (inherits "WaitQ") */
typedef struct TmanMutexCB {
    TmanListNode owner_link;
    SceUID uid;
    s32 init_count;
    s32 lock_count;
    TmanThreadCB *owner;
    void *opt;
    s32 prio_ceiling;
} TmanMutexCB;

/* Lightweight Mutex CB (inherits "WaitQ") */
typedef struct TmanLwMutexCB {
    TmanListNode owner_link;
    SceLwMutex *work_area;
    s32 init_count;
    void *opt;
} TmanLwMutexCB;

/* MessageBox message header */
typedef struct TmanMsgPacket {
    struct TmanMsgPacket *next;
    u8 msg_priority;
    u8 reserved[3];
} TmanMsgPacket;

/* MessageBox CB (inherits "WaitQ") */
typedef struct TmanMbxCB {
    void *opt;
    s32 num_msgs;
    TmanMsgPacket *msg_head;
} TmanMbxCB;

/* MessagePipe CB (inherits "WaitQ") */
typedef struct TmanMsgPipeCB {
    void *opt;
    SceUID buf_block_id;
    u8 *buf;
    u32 buf_size;
    u32 free_size;
    u32 read_pos;
    u32 write_pos;
    TmanListNode send_wait_list;
    s32 num_send_wait;
    u32 attr;
} TmanMsgPipeCB;

/* VPL CB (inherits "WaitQ") */
typedef struct TmanVplCB {
    void *opt;
    SceUID mem_id;
    SceUID heap_id;
    void *pool;
    u32 pool_size;
    u32 attr;
} TmanVplCB;

/* FPL CB (inherits "WaitQ") */
typedef struct TmanFplCB {
    SceUID mem_id;
    void *pool;
    void **free_tail;
    void **link_table;
    u32 free_blocks;
    u32 num_blocks;
    u32 block_size;
    u32 attr;
    void *opt;
} TmanFplCB;

/* TLSPL CB (inherits "WaitQ") */
typedef struct TmanTlsplCB {
    SceUID mem_id;
    void *pool;
    void **free_tail;
    void **link_table;
    u32 free_blocks;
    s32 tls_index;
    u32 num_blocks;
    u32 block_size;
    u32 attr;
    void *opt;
} TmanTlsplCB;

/* ThreadEventHandler CB (inherits "WaitQ") */
typedef struct TmanThreadEventHandlerCB {
    TmanListNode link;
    SceUID uid;
    s32 active_count;
    s32 deleting;
    void *target_thread;
    u32 is_user;
    u32 mask;
    void *handler;
    void *common;
    u32 gp_reg;
} TmanThreadEventHandlerCB;

/* Callback CB (inherits "WaitQ") */
typedef struct TmanCallbackCB {
    TmanListNode link;
    SceUID uid;
    s32 active_count;
    s32 deleting;
    TmanThreadCB *owner;
    s32 notify_count;
    s32 notify_arg;
    u32 is_user;
    void *handler;
    void *common;
    u32 gp_reg;
} TmanCallbackCB;

/* Base Timer CB (20 bytes, UID type "Timer") */
typedef struct TmanTimerCB {
    TmanListNode link;
    u64 schedule;
    u32 flags;
} TmanTimerCB;

/* Delay Timer CB (inherits "Timer") */
typedef struct TmanDelayCB {
    void (*handler)(void *arg);
    void *arg;
} TmanDelayCB;

/* Alarm CB (inherits "Timer") */
typedef struct TmanAlarmCB {
    u32 is_user;
    SceKernelAlarmHandler handler;
    void *common;
    u32 gp_reg;
} TmanAlarmCB;

/* VTimer CB (inherits "Timer") */
typedef struct TmanVTimerCB {
    u32 active;
    u64 base_time;
    u64 schedule;
    u32 is_user;
    u32 (*handler)(SceUID uid, SceKernelSysClock *base, SceKernelSysClock *current, void *common);
    void *common;
    u32 gp_reg;
} TmanVTimerCB;

/* KTLS slot descriptor */
typedef struct TmanKtlsEntry {
    u32 used;
    s32 ref_count;
    u32 size;
    void (*callback)(void *tls, void *arg);
    void *common;
    u32 gp_reg;
} TmanKtlsEntry;

/* Deferred thread deletion node */
typedef struct TmanCleanerNode {
    struct TmanCleanerNode *next;
    SceUID uids[8];
    s32 count;
} TmanCleanerNode;

/* Thread status structures for Refer*Status */
typedef struct TmanThreadEventHandlerInfo {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    SceUID thread_id;
    u32 mask;
    void *handler;
    void *common;
} TmanThreadEventHandlerInfo;

typedef struct TmanMbxInfo {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    u32 attr;
    s32 num_wait_threads;
    s32 num_messages;
    void *first_message;
} TmanMbxInfo;

typedef struct TmanLwMutexInfo {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    u32 attr;
    SceUID uid;
    void *work_area;
    s32 init_count;
    s32 current_count;
    SceUID current_owner;
    s32 num_wait_threads;
} TmanLwMutexInfo;

typedef struct TmanTlsplInfo {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    u32 attr;
    s32 index;
    u32 block_size;
    u32 total_blocks;
    u32 free_blocks;
    s32 num_wait_threads;
} TmanTlsplInfo;

typedef struct TmanVTimerInfo {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    s32 active;
    SceKernelSysClock base;
    SceKernelSysClock current;
    SceKernelSysClock schedule;
    void *handler;
    void *common;
} TmanVTimerInfo;

typedef struct TmanThreadInfo {
    SceSize size;
    char name[SCE_UID_NAME_LEN + 1];
    u32 attr;
    s32 status;
    void *entry;
    void *stack;
    s32 stack_size;
    u32 gp_reg;
    s32 init_priority;
    s32 current_priority;
    s32 wait_type;
    SceUID wait_id;
    s32 wakeup_count;
    s32 exit_status;
    SceKernelSysClock run_clocks;
    u32 intr_preempt_count;
    u32 thread_preempt_count;
    u32 release_count;
} TmanThreadInfo;

typedef struct TmanThreadRunStatus {
    SceSize size;
    s32 status;
    s32 current_priority;
    s32 wait_type;
    SceUID wait_id;
    s32 wakeup_count;
    SceKernelSysClock run_clocks;
    u32 intr_preempt_count;
    u32 thread_preempt_count;
    u32 release_count;
} TmanThreadRunStatus;

typedef struct TmanSystemStatus {
    SceSize size;
    u32 status;
    SceKernelSysClock idle_clocks;
    u32 comes_out_of_idle_count;
    u32 thread_switch_count;
    u32 vfpu_switch_count;
} TmanSystemStatus;

typedef int (*TmanLogHandler)(u32 event, u32 obj, u32 argc, ...);

/* Global ThreadMan state */
typedef struct TmanGlobals {
    TmanThreadCB *current_thread;
    TmanThreadCB *next_thread;
    TmanLogHandler log_handler;
    s32 dispatch_suspended;
    u32 switch_count;
    u32 noswitch_count;
    u32 vfpu_switch_count;
    void *vfpu_owner_ctx;
    s32 vfpu_owner_mode;
    s32 delay_pool_count;
    TmanListNode delay_pool;
    s32 user_threads_suspended;
    s32 cleaner_needed;
    TmanCleanerNode cleaner_head;
    SceUID active_timer_uid;
    TmanTimerCB *timer_iter;
    TmanListNode timer_list;
    u64 last_switch_time;
    u64 sys_timer_high;
    u64 next_timer_alarm;
    u32 ready_bitmap[4];
    TmanListNode ready_queues[128];
    TmanListNode sleeping_list;
    TmanListNode delayed_list;
    TmanListNode stopped_list;
    TmanListNode suspended_list;
    TmanListNode dormant_list;
    TmanListNode event_handler_list;
    SceSysmemUidCB *uid_waitq;
    SceSysmemUidCB *uid_thread;
    SceSysmemUidCB *uid_sema;
    SceSysmemUidCB *uid_evf;
    SceSysmemUidCB *uid_mutex;
    SceSysmemUidCB *uid_lwmutex;
    SceSysmemUidCB *uid_mbx;
    SceSysmemUidCB *uid_msgpipe;
    SceSysmemUidCB *uid_vpl;
    SceSysmemUidCB *uid_fpl;
    SceSysmemUidCB *uid_tlspl;
    SceSysmemUidCB *uid_threv;
    SceSysmemUidCB *uid_cb;
    SceSysmemUidCB *uid_timer;
    SceSysmemUidCB *uid_delay;
    SceSysmemUidCB *uid_alarm;
    SceSysmemUidCB *uid_vtimer;
    TmanThreadCB *idle_thread;
    TmanThreadCB *cleaner_thread;
    TmanKtlsEntry ktls[16];
    SceUID tls_uid_table[16];
    u32 refer_scratch[61];
    u32 cpu_prid;
    s32 sys_timer_intr;
    SceUID sys_stat_evf;
    u32 syscall_exit_thread;
    u32 syscall_ret_change_stack;
    u32 syscall_ret_callback;
    SceUID heap_id;
    u32 profiler_mode;
    u32 timer_min_delta;
    u32 timer_max_delta;
    u32 timer_res_lo;
    u32 timer_res_hi;
    void (*dead_thread_handler)(SceUID uid);
    void *get_time_low_fn;
    void *get_time_wide_fn;
    u32 uid_xor_key;
    u32 tls_index_bitmap;
    u32 comes_out_of_idle_count;
} TmanGlobals;

extern TmanGlobals g_tman;
extern u32 g_saved_sys_timer;
extern u32 ThreadManForKernel_921B7ACD;

/* Assembly helpers (threadman_asm.S) */
extern void tman_idle_loop(void);
extern void tman_vfpu_save(void *ctx);
extern void tman_vfpu_restore(void *ctx);
extern void tman_vfpu_flush(void);
extern u32 tman_fpu_exc_handler[];
extern void tmanSaveFpuContext(TmanThreadContext *ctx);
extern void tman_systime_low_syscall(void);
extern void tman_idle_halt(void);

/* UID function tables */
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
extern int InterruptManagerForKernel_551097F2(void *ctx);
extern int InterruptManagerForKernel_E790EAED(void *eret_fn, void *sched_fn);
extern int SaveThreadContext(void *ctx);
extern int sceKernelCpuEnableIntr(void);
extern int sceKernelCpuResumeIntr(int intr);
extern int sceKernelCpuResumeIntrWithSync(int intr);
extern int sceKernelCpuSuspendIntr(void);
extern int sceKernelDisableIntr(int intr);
extern int sceKernelEnableIntr(int intr);
extern int sceKernelRegisterIntrHandler(int intr, int sub, void *handler, void *arg, void *opt);
extern int sceKernelReleaseIntrHandler(int intr);
extern int sceKernelSetIntrLevel(int intr, int level);
extern int sceKernelSetPrimarySyscallHandler(int syscall_num, void *handler);
extern int sceKernelQuerySystemCall(void *func);
extern int sceKernelCallUserIntrHandler(u32 reg_a, u32 reg_b, u32 reg_c, u32 reg_d, void *handler, u32 sp);
extern u32 sceKernelGetUserIntrStack(void);
extern int sceKernelRegisterExceptionHandler(int code, void *handler);
extern int sceKernelReleaseExceptionHandler(int code, void *handler);
extern int sceKernelRegisterSuspendHandler(int id, void *handler, void *arg);
extern int sceKernelRegisterResumeHandler(int id, void *handler, void *arg);
extern int sceKernelGetModuleGPByAddressForKernel(u32 addr);
extern int sceKernelSetBootCallbackLevel(void *cb, int level, int arg);
extern int Kprintf(const char *fmt, ...);
extern int sceKernelDipsw(int reg);
extern int sceKernelIsDevelopmentToolMode(void);
extern void *memcpy(void *dst, const void *src, u32 len);
extern void *memset(void *dst, int val, u32 len);
extern char *strncpy(char *dst, const char *src, u32 len);
extern int snprintf(char *buf, u32 size, const char *fmt, ...);
extern SceUID sceKernelCreateHeap(SceUID part_id, SceSize size, int attr, const char *name);
extern int sceKernelDeleteHeap(SceUID heap_id);
extern void *sceKernelAllocHeapMemory(SceUID heap_id, SceSize size);
extern void *sceKernelAllocHeapMemoryWithOption(SceUID heap_id, SceSize size, void *opt);
extern int sceKernelFreeHeapMemory(SceUID heap_id, void *ptr);
extern SceSize sceKernelHeapTotalFreeSize(SceUID heap_id);
extern SceUID sceKernelAllocPartitionMemory(SceUID part_id, const char *name, int type, SceSize size, void *addr);
extern int sceKernelFreePartitionMemory(SceUID block_id);
extern void *sceKernelGetBlockHeadAddr(SceUID block_id);
extern int sceKernelMemset32(void *addr, u32 val, SceSize size);
extern int sceKernelGetInitialRandomValue(void);
extern int sceKernelGetCompiledSdkVersion(void);
extern int sceKernelQueryMemoryInfo(void *addr, void *part_id, SceUID *block_id);
extern int sceKernelQueryMemoryBlockInfo(SceUID block_id, void *info);
extern int sceKernelQueryMemoryPartitionInfo(SceUID part_id, void *info);
extern int sceKernelCreateUIDtype(const char *name, int size, SceSysmemUidLookupFunc *func_table, SceSysmemUidLookupFunc *meta_table, SceSysmemUidCB **out_type);
extern int sceKernelCreateUIDtypeInherit(const char *parent_name, const char *name, int size, SceSysmemUidLookupFunc *func_table, SceSysmemUidLookupFunc *meta_table, SceSysmemUidCB **out_type);
extern int sceKernelCreateUID(SceSysmemUidCB *type, const char *name, char attr, SceSysmemUidCB **out_cb);
extern int sceKernelDeleteUID(SceUID uid);
extern int sceKernelGetUIDcontrolBlock(SceUID uid, SceSysmemUidCB **out_cb);
extern int sceKernelGetUIDcontrolBlockWithType(SceUID uid, SceSysmemUidCB *type, SceSysmemUidCB **out_cb);
extern int sceKernelIsKindOf(SceSysmemUidCB *cb, SceSysmemUidCB *type);
extern int sceKernelCallUIDObjFunction(SceSysmemUidCB *cb, s32 func_id, ...);
extern int sceKernelCallUIDObjCommonFunction(SceSysmemUidCB *cb, SceSysmemUidCB *type, s32 func_id, void *args);

/* Internal scheduler / wait queue functions (tman_sched.c) */
int tmanAddThreadToWaitQueue(SceSysmemUidCB *wait_obj, u32 wait_type, u32 attr, u64 *timeout_abs, u32 *timeout_out, u32 *wait_counter);
int tmanWaitOnObject(SceSysmemUidCB *wait_obj, u32 wait_type, u32 attr, u64 *timeout_abs, u32 *timeout_out);
void tmanParkCurrentThread(TmanListNode *list, u32 wait_type);
void tmanInsertThreadByPriority(TmanThreadCB *th, SceSysmemUidCB *wait_obj);
int tmanWakeupWaitingThread(TmanThreadCB *th);
int tmanWakeupAndReschedule(TmanThreadCB *th, int in_intr, int intr_state);
int tmanWakeupFirstWaiter(SceSysmemUidCB *wait_obj, s32 wait_result, int in_intr, int intr_state);
int tmanWakeupAllWaiters(SceSysmemUidCB *wait_obj, s32 wait_result);
void tmanClearThreadWait(TmanThreadCB *th);
int tmanNotifyWaitObject(SceSysmemUidCB *wait_obj);
void tmanInitWaitQueueType(void);
void tmanWaitTimeoutCallback(void *arg);
void tmanSchedule(int save_wait_stat, int can_switch);
int tmanNeedReschedule(void);
void tmanInitEventAndCallbackTypes(void);
int tmanDispatchThreadEvents(u32 mask, TmanThreadCB *target, int intr_state);
int tmanCheckPendingCallbacks(int intr_state);
void tmanCleanupActiveEventHandler(TmanThreadCB *th);
void tmanCleanupActiveCallback(TmanThreadCB *th);
void tmanDeleteThreadCallbacks(TmanThreadCB *th);
int tmanExecuteHandlerContext(int is_user, void *handler, u32 reg_a, u32 reg_b, u32 reg_c, u32 reg_d, u32 gp_reg);
int tmanReturnFromHandlerContext(void);
void tmanDelayTimeoutCallback(void *arg);

/* Internal sync functions (tman_sync.c) */
void tmanInitSemaType(void);
void tmanInitEventFlagType(void);
void tmanReleaseOwnedEventFlags(TmanThreadCB *th);
void tmanInitMutexType(void);
void tmanMutexAcquire(SceSysmemUidCB *cb, TmanThreadCB *owner, s32 count);
void tmanMutexRelease(SceSysmemUidCB *cb);
void tmanReleaseOwnedMutexes(TmanThreadCB *th);
void tmanInitLwMutexType(void);
void tmanInitMbxType(void);
int tmanWaitQueueHasWaiters(const TmanWaitQueue *wq);

/* Internal memory pool / timer functions (tman_mem.c) */
void tmanInitMsgPipeType(void);
void tmanMsgPipeWriteRing(TmanMsgPipeCB *mpp, const u8 *src, u32 len);
void tmanInitVplType(void);
void tmanInitFplType(void);
int tmanFplFreeBlock(TmanFplCB *fpl, s32 idx);
int tmanFplAllocBlock(TmanFplCB *fpl);
void tmanInitTlsplType(void);
int tmanTlsplFreeBlock(TmanTlsplCB *tls, s32 idx);
int tmanTlsplAllocBlock(TmanTlsplCB *tls);
int tmanTlsplValidateBlock(TmanTlsplCB *tls, void *ptr);
void tmanFreeThreadTlspl(TmanThreadCB *th);
void tmanInitTimerTypes(void);
void tmanInitSysTimer(void);
u64 tmanGetSystemTimeWide(void);
void tmanCalcAbsTimeoutUsec(u64 *abs_out, u32 usec);
void tmanCalcAbsTimeoutClock(u64 *abs_out, const SceKernelSysClock *clk);
SceUID tmanCreateDelayTimer(const u64 *abs_time, void (*handler)(void *), void *arg);
int tmanCancelDelayTimer(SceUID delay_id, u32 *remain_usec);
void tmanInsertTimerSorted(TmanTimerCB *timer);
void tmanProgramNextTimer(void);
SceUID tmanFindMsgPipeByMemBlock(SceUID block_id);
SceUID tmanFindVplByMemBlock(SceUID block_id);
SceUID tmanFindFplByMemBlock(SceUID block_id);
SceUID tmanFindTlsplByMemBlock(SceUID block_id);

/* Internal thread core functions (tman_thread.c) */
void tmanInitThreadType(void);
void tmanInitThreadContext(TmanThreadContext *ctx);
void tmanAddReadyQueue(TmanThreadCB *th);
void tmanRemoveReadyQueue(TmanThreadCB *th, u32 prio);
void tmanMakeReadyAndPreempt(TmanThreadCB *th, int intr_state);
TmanThreadContext *tmanSwitchContext(TmanThreadContext *cur_ctx, int reason);
void tmanSelectNextThread(void);
void tmanSwitchVfpuContext(TmanThreadCB *th);
void tmanCheckKernelStackGuard(void);
int tmanQueueThreadForCleaner(SceUID thid);
void tmanFreeThreadResources(TmanThreadCB *th);
void tmanRestoreExtStacks(TmanThreadCB *th);
int tmanFreeThreadKtls(TmanThreadCB *th);
int tmanChangeThreadPriorityInternal(TmanThreadCB *th, s32 new_prio, int from_user);
void tmanExitCurrentThread(int delete_self, int intr_state, s32 exit_status);
int tmanResumeDispatchInternal(int mode, int intr_state);
void tmanThreadIsDead(SceSysmemUidCB *cb);
void tmanHandleStackOverflow(TmanThreadCB *th, int is_kernel, u32 sp);
int tmanExtendThreadStackCommon(SceUID block_id, void *stack_base, u32 stack_size, void *entry, void *arg);
void tmanFillThreadDebugStatus(SceSysmemUidCB *cb, void *status);
void tmanFillSystemStatus(TmanSystemStatus *local, int intr_state);

/* Exported functions with custom signatures */
int _sceKernelReturnFromCallback(void);
int _sceKernelReturnFromChangeStack(void);
int sceKernelExtendThreadStack(u32 stack_size, void *entry, void *arg);
int sceKernelExitThread(s32 status);
s32 sceKernelExitDeleteThread(s32 exit_status);
unsigned int sceKernelGetSystemTimeLow(void);
SceUID sceKernelCreateEventFlag(const char *name, int attr, int bits, SceKernelEventFlagOptParam *opt);
int sceKernelReferThreadEventHandlerStatus(SceUID id, TmanThreadEventHandlerInfo *info);
int sceKernelReferCallbackStatus(SceUID id, SceKernelCallbackInfo *info);
int sceKernelReferMbxStatus(SceUID id, TmanMbxInfo *info);
int sceKernelReferLwMutexStatusByID(SceUID id, TmanLwMutexInfo *info);
int sceKernelReferTlsplStatus(SceUID id, TmanTlsplInfo *info);
int sceKernelReferVTimerStatus(SceUID id, TmanVTimerInfo *info);
int sceKernelReferThreadStatus(SceUID thid, TmanThreadInfo *info);
int sceKernelReferThreadRunStatus(SceUID thid, TmanThreadRunStatus *info);
int sceKernelReferSystemStatus(TmanSystemStatus *info);

#endif /* THREADMAN_INTERNAL_H */
