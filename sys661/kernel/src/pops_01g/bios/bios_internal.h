/*
 * PlayStation 1 System ROM (PS-X Realtime Kernel Ver.2.5) internal header.
 * Compiled for MIPS R3000A (-march=r3000 -mabi=32 -msoft-float) and linked at 0xBFC00000.
 */

#ifndef PSX_BIOS_INTERNAL_H
#define PSX_BIOS_INTERNAL_H

typedef unsigned char      u8;
typedef unsigned short     u16;
typedef unsigned int       u32;
typedef signed char        s8;
typedef short              s16;
typedef int                s32;
typedef unsigned int       size_t;

#define NULL ((void *)0)

/* Memory-mapped PlayStation 1 I/O registers (KSEG1 0xBF80xxxx / 0x1F80xxxx) */
#define PSX_IO8(off)   (*(volatile u8  *)(0xBF800000U + (off)))
#define PSX_IO16(off)  (*(volatile u16 *)(0xBF800000U + (off)))
#define PSX_IO32(off)  (*(volatile u32 *)(0xBF800000U + (off)))
#define PSX_CACHE_CTRL (*(volatile u32 *)0xFFFE0130U)

/* Memory control & bus registers */
#define REG_EXP1_BASE      0x1000U
#define REG_EXP2_BASE      0x1004U
#define REG_EXP1_DELAY     0x1008U
#define REG_EXP3_DELAY     0x100CU
#define REG_BIOS_DELAY     0x1010U
#define REG_SPU_DELAY      0x1014U
#define REG_CDROM_DELAY    0x1018U
#define REG_EXP2_DELAY     0x101CU
#define REG_COM_DELAY      0x1020U
#define REG_RAM_SIZE       0x1060U

/* Interrupt controller registers */
#define REG_I_STAT         0x1070U
#define REG_I_MASK         0x1074U

/* DMA controller registers */
#define REG_DMA_GPU_MADR   0x10A0U
#define REG_DMA_GPU_BCR    0x10A4U
#define REG_DMA_GPU_CHCR   0x10A8U
#define REG_DMA_CD_MADR    0x10B0U
#define REG_DMA_CD_BCR     0x10B4U
#define REG_DMA_CD_CHCR    0x10B8U
#define REG_DMA_OTC_MADR   0x10E0U
#define REG_DMA_OTC_BCR    0x10E4U
#define REG_DMA_OTC_CHCR   0x10E8U
#define REG_DPCR           0x10F0U
#define REG_DICR           0x10F4U

/* SIO0 (Pad / Memory Card) registers */
#define REG_JOY_DATA       0x1040U
#define REG_JOY_STAT       0x1044U
#define REG_JOY_MODE       0x1048U
#define REG_JOY_CTRL       0x104AU
#define REG_JOY_BAUD       0x104EU

/* Root counters (timers 0..2, stride 0x10) */
#define REG_TMR_BASE       0x1100U
#define REG_TMR_VAL(n)     (REG_TMR_BASE + ((u32)(n) << 4) + 0x0U)
#define REG_TMR_MODE(n)    (REG_TMR_BASE + ((u32)(n) << 4) + 0x4U)
#define REG_TMR_TARGET(n)  (REG_TMR_BASE + ((u32)(n) << 4) + 0x8U)

/* CD-ROM controller ports */
#define REG_CD_INDEX       0x1800U
#define REG_CD_PORT1       0x1801U
#define REG_CD_PORT2       0x1802U
#define REG_CD_PORT3       0x1803U

/* GPU ports */
#define REG_GPU_GP0        0x1810U
#define REG_GPU_GP1        0x1814U

/* SPU master/reverb volume registers */
#define REG_SPU_MVOL_L     0x1D80U
#define REG_SPU_MVOL_R     0x1D82U
#define REG_SPU_RVOL_L     0x1D84U
#define REG_SPU_RVOL_R     0x1D86U

/* Hardware IRQ bit positions in I_STAT / I_MASK */
#define PSX_IRQ_VBLANK     0U
#define PSX_IRQ_GPU        1U
#define PSX_IRQ_CDROM      2U
#define PSX_IRQ_DMA        3U
#define PSX_IRQ_TMR0       4U
#define PSX_IRQ_TMR1       5U
#define PSX_IRQ_TMR2       6U
#define PSX_IRQ_SIO0       7U
#define PSX_IRQ_SIO1       8U
#define PSX_IRQ_SPU        9U
#define PSX_IRQ_PIO        10U

/* PS1 kernel RAM layout (0x80000000..0x8000FFFF) */
#define PSX_RAM_A0_TABLE   ((u32 *)0x80000200U)
#define PSX_RAM_C0_TABLE   ((u32 *)0x80000674U)
#define PSX_RAM_B0_TABLE   ((u32 *)0x80000874U)
#define PSX_RAM_SHELL_ADDR 0x80030000U
#define PSX_ROM_SHELL_ADDR 0xBFC18000U
#define PSX_ROM_SHELL_SIZE 0x00016120U
#define PSX_ROM_FONT_ADDR  0xBFC64000U
#define PSX_ROM_VER_ADDR   0xBFC7FF32U

/* Event classes (EvCB desc) */
#define EV_CLASS_VBLANK    0xF0000001U
#define EV_CLASS_GPU       0xF0000002U
#define EV_CLASS_CDROM     0xF0000003U
#define EV_CLASS_DMA       0xF0000004U
#define EV_CLASS_RTC0      0xF0000005U
#define EV_CLASS_RTC1      0xF0000006U
#define EV_CLASS_RTC2      0xF0000007U
#define EV_CLASS_PAD       0xF0000008U
#define EV_CLASS_SPU       0xF0000009U
#define EV_CLASS_PIO       0xF000000AU
#define EV_CLASS_SIO       0xF000000BU
#define EV_CLASS_CARD_HW   0xF0000011U
#define EV_CLASS_CARD_SW   0xF4000001U
#define EV_CLASS_RCNT_BASE 0xF2000000U

/* Event specs */
#define EV_SPEC_COUNTER    0x0002U
#define EV_SPEC_INTR       0x0004U
#define EV_SPEC_IOE        0x0004U
#define EV_SPEC_COMMAND    0x0010U
#define EV_SPEC_ERROR      0x8000U
#define EV_SPEC_TIMEOUT    0x0100U
#define EV_SPEC_NEWDEV     0x2000U

/* Event modes & statuses */
#define EV_MODE_CALLBACK   0x1000U
#define EV_MODE_NO_CB      0x2000U

#define EV_STATUS_FREE     0x0000U
#define EV_STATUS_DISABLED 0x1000U
#define EV_STATUS_ENABLED  0x2000U
#define EV_STATUS_ALREADY  0x4000U

/* Thread Control Block (TCB) layout matching PS-X Realtime Kernel */
#define TCB_STATUS_FREE    0x1000U
#define TCB_STATUS_ALLOC   0x4000U

typedef struct PsxTcb {
    u32 status;
    u32 reserved;
    u32 regs[32];
    u32 epc;
    u32 hi;
    u32 lo;
    u32 sr;
    u32 cause;
    u32 tail[9];
} PsxTcb;

typedef struct PsxPcb {
    PsxTcb *currentTcb;
} PsxPcb;

/* Event Control Block (EvCB, 28 bytes) */
typedef struct PsxEvcb {
    u32 desc;
    u32 status;
    u32 spec;
    u32 mode;
    void (*handler)(void);
    u32 reserved[2];
} PsxEvcb;

/* Exception priority chain node (SysEnqIntRP) */
typedef struct PsxIntRpNode {
    struct PsxIntRpNode *next;
    s32 (*handler)(void);
    s32 (*verifier)(void);
    u32 pad;
} PsxIntRpNode;

/* Kernel table directory at 0x80000100 */
typedef struct PsxKernelTableEntry {
    void *base;
    u32 byteSize;
} PsxKernelTableEntry;

#define PSX_KTABLE_EXCB    0U
#define PSX_KTABLE_PCB     1U
#define PSX_KTABLE_TCB     2U
#define PSX_KTABLE_EVCB    4U
#define PSX_KTABLE_FCB     8U
#define PSX_KTABLE_DCB     10U

#define PSX_KTABLES ((PsxKernelTableEntry *)0x80000100U)

/* PS-X EXE header (2048 bytes on disc) */
typedef struct PsxExeHeader {
    char magic[8];      /* "PS-X EXE" */
    u32 textOff;
    u32 dataOff;
    u32 initialPc;
    u32 initialGp;
    u32 loadAddr;
    u32 loadSize;
    u32 dataAddr;
    u32 dataSize;
    u32 bssAddr;
    u32 bssSize;
    u32 stackBase;
    u32 stackSize;
    u32 savedSp;
    u32 savedFp;
    u32 savedGp;
    u32 savedRa;
    u32 savedS0;
} PsxExeHeader;

/* Directory search entry returned by FirstFile / NextFile */
typedef struct PsxDirEntry {
    char name[20];
    s32 attr;
    s32 size;
    struct PsxDirEntry *next;
    s32 headLba;
    u32 systemInfo;
} PsxDirEntry;

/* Device Control Block (DCB) */
typedef struct PsxDcb {
    const char *name;
    u32 flags;
    u32 blockSize;
    const char *description;
    void (*initFn)(void);
    s32 (*openFn)(void *fileCb, const char *path, u32 mode);
    s32 (*actionFn)(void *fileCb, u32 mode);
    s32 (*closeFn)(void *fileCb);
    s32 (*ioctlFn)(void *fileCb, u32 cmd, u32 arg);
    s32 (*readFn)(void *fileCb, void *dst, u32 len);
    s32 (*writeFn)(void *fileCb, const void *src, u32 len);
    s32 (*eraseFn)(void *fileCb, const char *path);
    s32 (*undeleteFn)(void *fileCb, const char *path);
    PsxDirEntry *(*firstFileFn)(void *fileCb, const char *pattern, PsxDirEntry *out);
    PsxDirEntry *(*nextFileFn)(void *fileCb, PsxDirEntry *out);
    s32 (*formatFn)(void *fileCb);
    s32 (*chdirFn)(void *fileCb, const char *path);
    s32 (*renameFn)(void *fileCb, const char *oldName, const char *newName);
    void (*removeFn)(void);
    s32 (*testDeviceFn)(void *fileCb);
} PsxDcb;

/* File Control Block (FCB) */
typedef struct PsxFcb {
    u32 status;
    u32 diskId;
    void *transferBuf;
    u32 transferLen;
    u32 position;
    u32 devFlags;
    u32 error;
    PsxDcb *dcb;
    u32 fileSize;
    u32 startLba;
    u32 fcbNumber;
    u32 dirIndex;
} PsxFcb;

/* Global kernel state placed in PS1 kernel RAM (0x80009000..0x8000F800) */
typedef struct PsxBiosState {
    PsxPcb pcb;
    PsxTcb tcbs[16];
    PsxEvcb evcbs[64];
    PsxFcb fcbs[16];
    PsxDcb dcbs[10];
    PsxIntRpNode *intRpHeads[4];
    u32 tcbCount;
    u32 evcbCount;
    u32 heapBase;
    u32 heapSize;
    u32 randSeed;
    u32 lastError;
    u32 clearIntFlags[16];
    u32 padClearFlag;
    void (*entryIntHook)(void);
    void (*customExitHook)(void);
    /* Pad driver state */
    u8 *padBufPortA;
    u8 *padBufPortB;
    u32 padBufLenA;
    u32 padBufLenB;
    u32 padRunning;
    u32 padType;
    /* Memory card state */
    u32 cardInited;
    u32 cardRunning;
    u32 cardAutoFormat;
    s32 cardLastStatus;
    /* CD-ROM driver state */
    u8 cdStatusResult[8];
    u32 cdInited;
    u32 cdRootDirLba;
    u32 cdRootDirSize;
    u32 cdAsyncActive;
    u32 cdAsyncRemSectors;
    u8 *cdAsyncDst;
    u32 cdAsyncLba;
    u32 cdSectorMode;
    /* Boot config parsed from SYSTEM.CNF */
    u32 cfgTcbCount;
    u32 cfgEvcbCount;
    u32 cfgStackTop;
    char cfgBootPath[128];
    /* Temporary sector buffer */
    u8 sectorBuf[2352];
} PsxBiosState;

#define PSX_BIOS ((PsxBiosState *)0x80009000U)

/* Assembly helpers (start.S) */
void biosFlushCache(void);
void biosReturnFromException(void);
s32 biosEnterCriticalSection(void);
void biosExitCriticalSection(void);
s32 biosSetjmp(u32 *env);
void biosLongjmp(u32 *env, s32 val);
void biosJumpToExe(u32 entryPc, u32 gp, u32 sp, u32 fp);
void biosExceptionVector(void);
void biosA0Vector(void);
void biosB0Vector(void);
void biosC0Vector(void);
void biosBrkVector(void);

/* Kernel initialization & bootstrap (kernel_init.c) */
void biosMainBoot(void);
void biosInitKernelTables(u32 tcbCount, u32 evcbCount);
void biosInstallVectors(void);
void biosSysInitMemory(u32 base, u32 size);
void biosInitHeap(void *addr, u32 size);
s32 biosLoadExec(const char *path, u32 stackBase, u32 stackOffset);
s32 biosLoadExe(const char *path, PsxExeHeader *hdrOut);
s32 biosExecExe(PsxExeHeader *hdr, s32 argc, char **argv);
void biosWarmBoot(void);
void biosBootstrap(void);

/* Exceptions & interrupts (exception.c) */
void biosExceptionDispatch(PsxTcb *tcb);
s32 biosSysEnqIntRP(s32 priority, PsxIntRpNode *node);
s32 biosSysDeqIntRP(s32 priority, PsxIntRpNode *node);
s32 biosChangeClearInt(u32 irq, s32 flag);
s32 biosChangeClearPad(s32 flag);
void biosHookEntryInt(void *jmpBuf);
void biosResetEntryInt(void);
void biosSetCustomExitFromException(void *handler);
void biosSetDefaultExitFromException(void);

/* Events, threads, root counters (events_threads.c) */
u32 biosOpenEvent(u32 desc, s32 spec, s32 mode, void (*handler)(void));
s32 biosCloseEvent(u32 eventId);
s32 biosEnableEvent(u32 eventId);
s32 biosDisableEvent(u32 eventId);
s32 biosTestEvent(u32 eventId);
s32 biosWaitEvent(u32 eventId);
void biosDeliverEvent(u32 desc, s32 spec);
void biosUnDeliverEvent(u32 desc, s32 spec);
u32 biosOpenThread(u32 pc, u32 sp, u32 gp);
s32 biosCloseThread(u32 thId);
s32 biosChangeThread(u32 thId);
s32 biosInitRCnt(u32 spec, u16 target, u32 mode);
s32 biosGetRCnt(u32 spec);
s32 biosResetRCnt(u32 spec);
s32 biosStartRCnt(u32 spec);
s32 biosStopRCnt(u32 spec);
s32 biosChangeClearRCnt(u32 spec, u32 flag);

/* Devices & filesystems (fs_devices.c) */
void biosInitDevices(void);
s32 biosAddDevice(const PsxDcb *dcb);
s32 biosDelDevice(const char *name);
void biosPrintDevices(void);
s32 biosFileOpen(const char *path, u32 mode);
s32 biosFileClose(s32 fileDesc);
s32 biosFileRead(s32 fileDesc, void *dst, u32 len);
s32 biosFileWrite(s32 fileDesc, const void *src, u32 len);
s32 biosFileSeek(s32 fileDesc, s32 offset, s32 whence);
s32 biosFileGetc(s32 fileDesc);
s32 biosFilePutc(s32 ch, s32 fileDesc);
s32 biosFileDelete(const char *path);
s32 biosFileRename(const char *oldPath, const char *newPath);
s32 biosFileFormat(const char *devName);
PsxDirEntry *biosFirstFile(const char *pattern, PsxDirEntry *out);
PsxDirEntry *biosNextFile(PsxDirEntry *out);
s32 biosTestDevice(const char *devName);
s32 biosGetLastError(void);
s32 biosGetLastFileError(s32 fileDesc);
void biosAddNullTty(void);

/* CD-ROM driver (cdrom_bios.c) */
s32 biosCdInit(void);
s32 biosCdRemove(void);
s32 biosCdAsyncSeekL(const u8 *msf);
s32 biosCdAsyncGetStatus(u8 *dst);
s32 biosCdAsyncReadSector(s32 count, void *dst, u32 mode);
s32 biosCdAsyncSetMode(u32 mode);
s32 biosCdReadSector(s32 count, u32 lba, void *dst);
s32 biosCdGetLbn(const char *path);
s32 biosCdGetStatus(void);
s32 biosCdStop(void);
s32 biosCdReset(void);
s32 biosCdSetSession(u32 session);
void biosCdHandleIrq(void);
void biosCdRegisterDevice(void);

/* Pad & Memory Card driver (pad_card.c) */
s32 biosInitPad(u8 *bufA, u32 lenA, u8 *bufB, u32 lenB);
s32 biosStartPad(void);
void biosStopPad(void);
void biosPadInitDirect(u8 *bufA, u8 *bufB);
void biosPadStartCom(void);
void biosPadStopCom(void);
void biosPadPollVblank(void);
void biosInitCard(s32 padEnable);
s32 biosStartCard(void);
s32 biosStopCard(void);
void biosBuInit(void);
s32 biosCardInfo(s32 chan);
s32 biosCardLoad(s32 chan);
s32 biosCardRead(s32 chan, u32 block, u8 *dst);
s32 biosCardWrite(s32 chan, u32 block, const u8 *src);
s32 biosCardStatus(s32 chan);
s32 biosCardClear(s32 chan);
s32 biosCardAuto(s32 flag);
void biosNewCard(void);
void biosCardRegisterDevice(void);

/* GPU driver (gpu_bios.c) */
void biosGpuCw(u32 cmd);
void biosGpuCwp(const u32 *cmds, s32 count);
void biosGpuDw(u32 x, u32 y, u32 w, u32 h, const u16 *pixels);
s32 biosGpuSync(void);
void biosSendGp1(u32 cmd);
u32 biosGetGpuStatus(void);
void biosResetGpu(void);

/* C standard library & font lookup (libc_bios.c) */
void *biosMemcpy(void *dst, const void *src, size_t n);
void *biosMemmove(void *dst, const void *src, size_t n);
void *biosMemset(void *dst, s32 c, size_t n);
s32 biosMemcmp(const void *lhs, const void *rhs, size_t n);
void *biosMemchr(const void *buf, s32 c, size_t n);
void biosBcopy(const void *src, void *dst, size_t n);
void biosBzero(void *dst, size_t n);
s32 biosBcmp(const void *lhs, const void *rhs, size_t n);
char *biosStrcpy(char *dst, const char *src);
char *biosStrncpy(char *dst, const char *src, size_t n);
char *biosStrcat(char *dst, const char *src);
char *biosStrncat(char *dst, const char *src, size_t n);
s32 biosStrcmp(const char *lhs, const char *rhs);
s32 biosStrncmp(const char *lhs, const char *rhs, size_t n);
size_t biosStrlen(const char *str);
char *biosStrchr(const char *str, s32 ch);
char *biosStrrchr(const char *str, s32 ch);
char *biosStrpbrk(const char *str, const char *accept);
size_t biosStrspn(const char *str, const char *accept);
size_t biosStrcspn(const char *str, const char *reject);
char *biosStrtok(char *str, const char *delim);
char *biosStrstr(const char *haystack, const char *needle);
s32 biosToupper(s32 ch);
s32 biosTolower(s32 ch);
s32 biosAtoi(const char *str);
s32 biosStrtol(const char *str, char **endPtr, s32 base);
u32 biosStrtoul(const char *str, char **endPtr, s32 base);
s32 biosAbs(s32 val);
s32 biosRand(void);
void biosSrand(u32 seed);
void biosQsort(void *base, size_t count, size_t elemSize, s32 (*cmp)(const void *, const void *));
void *biosBsearch(const void *key, const void *base, size_t count, size_t elemSize,
                  s32 (*cmp)(const void *, const void *));
void *biosMalloc(size_t size);
void biosFree(void *ptr);
void *biosCalloc(size_t count, size_t size);
void *biosRealloc(void *ptr, size_t size);
s32 biosPrintf(const char *fmt, ...);
s32 biosSprintf(char *out, const char *fmt, ...);
void biosPutchar(s32 ch);
void biosPuts(const char *str);
s32 biosGetchar(void);
char *biosGets(char *dst);
u32 biosKrom2RawAdd(u32 sjis);
s32 biosKrom2Offset(u32 sjis);
u32 biosGetSystemInfo(u32 index);
u32 *biosGetA0Table(void);
u32 *biosGetB0Table(void);
u32 *biosGetC0Table(void);

/* Syscall jump tables (syscall_tables.c) */
void biosPopulateSyscallTables(void);

#endif /* PSX_BIOS_INTERNAL_H */
