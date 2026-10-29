#ifndef SYS661_KERMIT_H
#define SYS661_KERMIT_H

#include <stdint.h>
#include <stddef.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t s8;
typedef int16_t s16;
typedef int32_t s32;
typedef int64_t s64;

typedef int SceUID;
typedef unsigned int SceSize;
typedef int SceSSize;
typedef unsigned int SceUInt;
typedef int64_t SceOff;

#define SCE_KERNEL_SA_THFIFO 0x0000
#define SCE_KERNEL_SA_THPRI  0x0100

/* Hardware MMIO helper */
#define HW(addr) (*(volatile u32 *)(addr))

/* Kermit hardware registers */
#define KERMIT_REG_INTR_PEND 0xBC300030u
#define KERMIT_REG_INTR_MASK 0xBC300038u
#define KERMIT_REG_CFG_40    0xBC300040u
#define KERMIT_REG_CFG_44    0xBC300044u
#define KERMIT_REG_CFG_48    0xBC300048u
#define KERMIT_REG_CFG_4C    0xBC30004Cu
#define KERMIT_REG_TRIG      0xBC300050u

#define KERMIT_DMA_STATUS    0xBD000000u
#define KERMIT_DMA_SYNC      0xBD000004u

/* Kermit shared mailbox structures */
typedef struct SceKermitRequest {
	u32 cmd;
	SceUID sema_id;
	struct SceKermitRequest *self;
	u32 pad;
	u64 args[14];
} SceKermitRequest;

typedef struct SceKermitCommand {
	u32 cmd;
	SceKermitRequest *req;
} SceKermitCommand;

typedef struct SceKermitResponse {
	u64 result;
	SceUID sema_id;
	u32 pad;
	u64 *resp_ptr;
	u32 pad2[3];
} SceKermitResponse;

typedef struct SceKermitInterrupt {
	u32 intr;
	u32 pad;
} SceKermitInterrupt;

#define KERMIT_CMD_BOX       ((volatile SceKermitCommand *)0xBFC00800u)
#define KERMIT_RESP_BOX      ((volatile SceKermitResponse *)0xBFC00840u)
#define KERMIT_INTR_BOX      ((volatile SceKermitInterrupt *)0xBFC008C0u)

#define KERMIT_UNCACHED(p)   ((SceKermitRequest *)((u32)(p) | (u32)((((s32)(p) >> 31) + 2) << 29)))

static inline u32 pspGetK1(void)
{
	u32 k1;
	__asm__ volatile("move %0, $k1" : "=r"(k1));
	return k1;
}

static inline void pspSetK1(u32 k1)
{
	__asm__ volatile("move $k1, %0" : : "r"(k1));
}

/* Kermit modes */
enum SceKermitMode {
	KERMIT_MODE_ARBITRARY  = 0x1,
	KERMIT_MODE_ODD        = 0x2,
	KERMIT_MODE_MSFS       = 0x3,
	KERMIT_MODE_FLASHFS    = 0x4,
	KERMIT_MODE_AUDIOOUT   = 0x5,
	KERMIT_MODE_ME         = 0x6,
	KERMIT_MODE_LOWIO      = 0x7,
	KERMIT_MODE_USBPSPCM   = 0x8,
	KERMIT_MODE_PERIPHERAL = 0x9,
	KERMIT_MODE_WLAN       = 0xA,
	KERMIT_MODE_AUDIOIN    = 0xB,
	KERMIT_MODE_USB        = 0xC,
	KERMIT_MODE_UTILITY    = 0xD,
};

/* System event handler */
typedef struct SceSysEventHandler {
	int size;
	char *name;
	int type_mask;
	int (*handler)(int ev_id, char *ev_name, void *param, int *result);
	int r28;
	int busy;
	struct SceSysEventHandler *next;
	int reserved[9];
} SceSysEventHandler;

/* Common kernel imports */
int Kprintf(const char *fmt, ...);

int sceKernelCpuSuspendIntr(void);
void sceKernelCpuResumeIntr(int state);
int sceKernelEnableIntr(int intr_code);
int sceKernelDisableIntr(int intr_code);
int sceKernelRegisterIntrHandler(int intr_code, int sub_intr, void *handler, void *arg1, void *arg2);
int sceKernelReleaseIntrHandler(int intr_code);
int sceKernelRegisterSubIntrHandler(int intr_code, int sub_intr_code, void *handler, void *arg);
int sceKernelReleaseSubIntrHandler(int intr_code, int sub_intr_code);
int sceKernelEnableSubIntr(int intr_code, int sub_intr_code);
int sceKernelDisableSubIntr(int intr_code, int sub_intr_code);
int sceKernelIsIntrContext(void);

SceUID sceKernelCreateSema(const char *name, SceUInt attr, int init_val, int max_val, void *option);
int sceKernelDeleteSema(SceUID semaid);
int sceKernelSignalSema(SceUID semaid, int signal);
int sceKernelWaitSema(SceUID semaid, int signal, SceUInt *timeout);
int sceKernelWaitSemaCB(SceUID semaid, int signal, SceUInt *timeout);
int sceKernelPollSema(SceUID semaid, int signal);

SceUID sceKernelCreateEventFlag(const char *name, int attr, int bits, void *opt);
int sceKernelDeleteEventFlag(SceUID evid);
int sceKernelSetEventFlag(SceUID evid, u32 bits);
int sceKernelClearEventFlag(SceUID evid, u32 bits);
int sceKernelWaitEventFlag(SceUID evid, u32 bits, u32 wait, u32 *out_bits, SceUInt *timeout);
int sceKernelWaitEventFlagCB(SceUID evid, u32 bits, u32 wait, u32 *out_bits, SceUInt *timeout);

SceUID sceKernelCreateMutex(const char *name, SceUInt attr, int init_count, void *opt);
int sceKernelDeleteMutex(SceUID mutexid);
int sceKernelLockMutex(SceUID mutexid, int lock_count, SceUInt *timeout);
int sceKernelLockMutexCB(SceUID mutexid, int lock_count, SceUInt *timeout);
int sceKernelUnlockMutex(SceUID mutexid, int unlock_count);

SceUID sceKernelCreateMsgPipe(const char *name, int part, int attr, void *unk1, void *opt);
int sceKernelDeleteMsgPipe(SceUID uid);
int sceKernelSendMsgPipe(SceUID uid, void *message, unsigned int size, int unk1, void *unk2, unsigned int *timeout);
int sceKernelReceiveMsgPipe(SceUID uid, void *message, unsigned int size, int unk1, void *unk2, unsigned int *timeout);

int sceKernelDelayThread(SceUInt delay);
int sceKernelDelayThreadCB(SceUInt delay);
u32 sceKernelGetSystemTimeLow(void);
int sceKernelSuspendDispatchThread(void);
int sceKernelResumeDispatchThread(int state);
int sceKernelNotifyCallback(SceUID cb, int arg);
int sceKernelGetThreadmanIdType(SceUID uid);

void *sceKernelMemcpy(void *dst, const void *src, SceSize size);
int sceKernelMemset(void *dst, int ch, SceSize size);
int sceKernelGetUIDcontrolBlock(SceUID uid, void **block);
__attribute__((access(none, 1))) void sceKernelDcacheWritebackRange(const void *p, unsigned int size);
__attribute__((access(none, 1))) void sceKernelDcacheInvalidateRange(const void *p, unsigned int size);
__attribute__((access(none, 1))) void sceKernelDcacheWritebackInvalidateRange(const void *p, unsigned int size);
void sceKernelDcacheWritebackInvalidateAll(void);

int sceKernelPowerLock(int lock_type);
int sceKernelPowerUnlock(int lock_type);
int sceKernelPowerLockForUser(int lock_type);
int sceKernelPowerUnlockForUser(int lock_type);

int sceKernelRegisterSysEventHandler(SceSysEventHandler *handler);
int sceKernelUnregisterSysEventHandler(SceSysEventHandler *handler);

int LoadExecForKernel_CAFC9CB3(void *func);

/* sceKermit_driver exports */
int sceKermit_driver_36666181(SceKermitRequest *req, u32 mode, u32 cmd, u32 num_args, u32 is_cb, u64 *resp);
int sceKermit_driver_4C3A362D(u32 channel, u32 is_cb);
int sceKermit_driver_5280B410(u32 intr_code, int (*handler)(void));
int sceKermit_driver_9DDD7DCA(void);
int sceKermit_driver_D69C50BB(void);

/* sceKermitMemory_driver exports */
int sceKermitMemory_driver_70D1F13C(void *data, u32 size);
int sceKermitMemory_driver_80E1240A(const void *in_data, u32 size);
void sceKermitMemory_driver_AAF047AC(SceKermitRequest *req, int arg_idx, void *data, u32 size, int flags);
int sceKermitMemory_driver_90B662D0(void *out_data, u32 size);

/* sceKermitPeripheral_driver exports */
int sceKermitPeripheral_driver_C6F8B4E1(u32 key, u32 offset, void *buf, u32 len);

#endif /* SYS661_KERMIT_H */
