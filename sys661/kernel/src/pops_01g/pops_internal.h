/*
 * PlayStation One emulator (pops / pops_01g, ePSP 6.61) internal definitions.
 *
 * Emulates the PS1 R3000A CPU, interrupt controller, GPU (via PSP GE),
 * SPU (via ME Audio), CD-ROM, controller, and virtual memory cards (pspvmc).
 */
#ifndef POPS_INTERNAL_H
#define POPS_INTERNAL_H

#include "common_header.h"

/* POPS constants */
#define POPS_SDK_VERSION            0x06060110U
#define POPS_COMPILER_VERSION       0x00030306U
#define POPS_MAIN_THREAD_PRIO       32
#define POPS_MAIN_THREAD_STACK      15980800
#define POPS_MAIN_THREAD_ATTR       0x80104000U
#define POPS_MS_CALLBACK_DEVCTL     0x02415821U
#define POPS_SVN_REVISION           13921

/* POPS fatal error codes */
#define POPS_ERR_IO_FAILURE         ((int)0x80010005)
#define POPS_ERR_FATAL_IO           ((int)0x80010005)
#define POPS_ERR_FATAL_GENERIC      ((int)0x80010089)

/* PS1 hardware IRQ bits (I_STAT / I_MASK) */
#define PS1_IRQ_VBLANK              (1U << 0)
#define PS1_IRQ_GPU                 (1U << 1)
#define PS1_IRQ_CDROM               (1U << 2)
#define PS1_IRQ_DMA                 (1U << 3)
#define PS1_IRQ_TMR0                (1U << 4)
#define PS1_IRQ_TMR1                (1U << 5)
#define PS1_IRQ_TMR2                (1U << 6)
#define PS1_IRQ_SIO0                (1U << 7)
#define PS1_IRQ_SIO1                (1U << 8)
#define PS1_IRQ_SPU                 (1U << 9)
#define PS1_IRQ_PIO                 (1U << 10)

#define PS1_NUM_DMA_CHANNELS        7
#define PS1_NUM_SPU_VOICES          24
#define PS1_NUM_VMC_SLOTS           2
#define PS1_CD_SECTOR_SIZE          2352
#define POPS_GE_LIST_WORDS          1024

/* PS1 R3000A CPU register file */
typedef struct PopsCpuRegs {
    u32 gpr[32];
    u32 lo;
    u32 hi;
    u32 pc;
    u32 nextPc;
    u32 cycleCount;
} PopsCpuRegs;

/* PS1 COP0 system control state */
typedef struct PopsCop0State {
    u32 status;
    u32 cause;
    u32 epc;
    u32 badVaddr;
    u32 prid;
} PopsCop0State;

/* PS1 interrupt controller state (intr.c) */
typedef struct PopsIntrState {
    u32 iStat;
    u32 iMask;
    u32 vblankCount;
    int pendingIrq;
    int alarmId;
    int eventFlagId;
} PopsIntrState;

/* PS1 DMA channel state */
typedef struct PopsDmaChannel {
    u32 baseAddr;
    u32 blockCtrl;
    u32 chanCtrl;
} PopsDmaChannel;

/* PS1 GPU / PSP GE translation state */
typedef struct PopsGpuState {
    u32 statusReg;
    u32 displayStart;
    u32 horizRange;
    u32 vertRange;
    u32 drawAreaTopLeft;
    u32 drawAreaBotRight;
    u32 drawOffset;
    int geListId;
    int holdMode;
    int frameCount;
    u32 geCmdBuf[POPS_GE_LIST_WORDS];
    u32 geCmdPos;
} PopsGpuState;

/* PS1 SPU voice & ME Audio bridge state */
typedef struct PopsSpuVoice {
    u16 volLeft;
    u16 volRight;
    u16 pitch;
    u16 startAddr;
    u32 adsr;
    int keyOn;
} PopsSpuVoice;

typedef struct PopsSpuState {
    u16 mainVolLeft;
    u16 mainVolRight;
    u16 reverbDepthLeft;
    u16 reverbDepthRight;
    u16 controlReg;
    u16 statusReg;
    u32 irqAddr;
    int meAudioReady;
    PopsSpuVoice voices[PS1_NUM_SPU_VOICES];
} PopsSpuState;

/* PS1 CD-ROM state */
typedef struct PopsCdromState {
    int discFd;
    u32 currentLba;
    u32 sectorCount;
    u8 status;
    u8 mode;
    int reading;
    u8 sectorBuf[PS1_CD_SECTOR_SIZE];
} PopsCdromState;

/* PS1 controller state */
typedef struct PopsPadState {
    u32 buttons;
    u8 analogLx;
    u8 analogLy;
    u8 analogRx;
    u8 analogRy;
    int analogMode;
} PopsPadState;

/* Virtual Memory Card state */
typedef struct PopsVmcState {
    int mounted;
    int dirty;
    u32 blockCount;
    u32 savedataStatus;
} PopsVmcState;

/* PSP controller sampling buffer */
typedef struct PopsCtrlData {
    u32 timeStamp;
    u32 buttons;
    u8 lx;
    u8 ly;
    u8 reserved[6];
} PopsCtrlData;

/* Master POPS emulator state */
typedef struct PopsState {
    int initialized;
    int running;
    int powerSuspended;
    int msInserted;
    int language;
    int mainThreadId;
    int powerCbId;
    int msCbId;
    int syncSemaId;
    char versionBanner[64];
    PopsCpuRegs cpu;
    PopsCop0State cop0;
    PopsIntrState intr;
    PopsDmaChannel dma[PS1_NUM_DMA_CHANNELS];
    PopsGpuState gpu;
    PopsSpuState spu;
    PopsCdromState cdrom;
    PopsPadState pad;
    PopsVmcState vmc[PS1_NUM_VMC_SLOTS];
} PopsState;

extern PopsState g_pops;

/* External host imports matching module.yml */
extern void *sceKernelMemcpy(void *dst, const void *src, u32 len);
extern void *sceKernelMemset(void *dst, int val, u32 len);
extern int sceKernelSetCompiledSdkVersion660(u32 ver);
extern int sceKernelSetCompilerVersion(u32 ver);
extern int sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *),
                                 int initPriority, int stackSize, u32 attr, void *opt);
extern int sceKernelStartThread(int thid, SceSize arglen, void *argp);
extern int sceKernelDelayThread(u32 usec);
extern int sceKernelDelayThreadCB(u32 usec);
extern int sceKernelCreateCallback(const char *name, int (*func)(int, int, void *), void *arg);
extern int sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *opt);
extern int sceKernelWaitSema(int semaId, int signal, u32 *timeout);
extern int sceKernelSignalSema(int semaId, int signal);
extern int sceKernelCreateEventFlag(const char *name, u32 attr, u32 bits, void *opt);
extern int sceKernelSetEventFlag(int evfId, u32 bits);
extern int sceKernelWaitEventFlagCB(int evfId, u32 bits, u32 wait, u32 *outBits, u32 *timeout);
extern u32 sceKernelGetSystemTimeLow(void);
extern int sceKernelSetAlarm(u32 clock, u32 (*handler)(void *), void *common);
extern int sceKernelCancelAlarm(int alarmId);
extern int sceKernelChangeThreadPriority(int thid, int priority);
extern int sceKernelPowerLock(int lockType);
extern int sceKernelPowerUnlock(int lockType);
extern void sceKernelDcacheWritebackAll(void);
extern void sceKernelDcacheWritebackRange(const void *addr, u32 size);
extern void sceKernelDcacheInvalidateRangeForUser(const void *addr, u32 size);
extern void sceKernelDcacheWritebackInvalidateRange(const void *addr, u32 size);

extern int sceDisplaySetHoldMode(int mode);
extern int sceDisplayWaitVblankStart(void);
extern int sceDisplayWaitVblankStartCB(void);
extern u32 sceDisplayGetVcount(void);

extern int sceGeListEnQueue(const void *list, void *stall, int cbid, void *arg);
extern int sceGeListUpdateStallAddr(int qid, void *stall);
extern int sceGeListSync(int qid, int syncType);
extern int sceGeDrawSync(int syncType);
extern int sceGeEdramSetAddrTranslation(int size);

extern int sceDmacMemcpy(void *dst, const void *src, u32 size);
extern int scePowerRegisterCallback(int slot, int cbid);
extern int scePowerSetClockFrequency(int pllfreq, int cpufreq, int busfreq);
extern int sceImposeGetLanguageMode(int *lang, int *btnAssign);

extern int sceCtrlSetSamplingCycle(int cycle);
extern int sceCtrlSetSamplingMode(int mode);
extern int sceCtrlSetIdleCancelThreshold(int idlereset, int idleback);
extern int sceCtrlPeekBufferNegative(PopsCtrlData *padData, int count);

extern int sceIoOpen(const char *file, int flags, int mode);
extern int sceIoClose(int fd);
extern int sceIoDevctl(const char *dev, u32 cmd, void *indata, int inlen, void *outdata, int outlen);

extern int sceUtilitySavedataInitStart(void *params);
extern int sceUtilitySavedataShutdownStart(void);
extern void sceUtilitySavedataUpdate(int n);
extern int sceUtilitySavedataGetStatus(void);

extern int scePopsMan_0090B2C8(int errCode);
extern int scePaf_A138A376(char *dst, const char *fmt, ...);
extern int sceMeAudio_34F2548F(void);
extern int pspvmc_0820B291(int slot);

/* Cross-file internal prototypes */
void popsIntrInit(void);
void popsIntrRaise(u32 irqBit);
void popsIntrAck(u32 irqMask);
void popsIntrSetMask(u32 mask);
void popsIntrDispatch(void);

void popsGpuInit(void);
void popsGpuWriteCommand(u32 cmdWord);
void popsGpuFlush(void);
void popsGpuVblank(void);

void popsSpuInit(void);
void popsSpuWriteReg(u32 offset, u16 val);
void popsCdromInit(void);
int popsCdromOpenDisc(const char *path);
void popsCdromCloseDisc(void);
void popsPadInit(void);
void popsPadPoll(void);
void popsVmcInit(void);

#endif /* POPS_INTERNAL_H */
