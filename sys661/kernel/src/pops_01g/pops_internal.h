#ifndef POPS_INTERNAL_H
#define POPS_INTERNAL_H

#include <common/types.h>

#define POPS_STATIC_ASSERT(cond, name) \
    typedef char pops_static_assert_##name[(cond) ? 1 : -1]

/* Fixed PS1 / PSP memory regions used by POPS */
#define POPS_SCRATCHPAD_ADDR      0x00010000U
#define POPS_VRAM_CACHED_BASE     0x04000000U
#define POPS_VRAM_UNCACHED_BASE   0x44000000U
#define POPS_JIT_IR_BASE          0x041B0000U
#define POPS_JIT_CODE_BASE        0x09540000U
#define POPS_JIT_CODE_LIMIT       0x097B0004U
#define POPS_PSX_RAM_BASE         0x09800000U
#define POPS_GE_DLIST_BASE        0x49A00000U
#define POPS_GE_DLIST_LIMIT       0x49B7B800U
#define POPS_JIT_LOOKUP_RAM       0x09C00000U
#define POPS_JIT_LOOKUP_BIOS      0x09E00000U
#define POPS_ME_SPU_SHARED_BASE   0x49F40000U
#define POPS_DISPLAY_SHARED_BASE  0x49FE0000U
#define POPS_FRAMEBUF_BASE        0x490C0000U

/* Generic scheduled event node (16 bytes, linked via scratchpad event head) */
typedef struct PopsEvent {
    struct PopsEvent *prev;
    struct PopsEvent *next;
    u32 dueCycle;
    void (*callback)(struct PopsEvent *ev);
} PopsEvent;

/* DMA channel event (28 bytes per channel, 7 PS1 DMA channels) */
typedef struct PopsDmaChannel {
    PopsEvent ev;
    s32 (*transferFn)(u32 addr, u32 bytes, u32 chcr);
    s32 pendingCycles;
    u16 syncMode;
    u16 channelId;
} PopsDmaChannel;

/* SIO0 / SIO1 serial channel state (44 bytes per channel) */
typedef struct PopsSioChannel {
    PopsEvent ev;
    u16 statusReg;
    u16 ctrlReg;
    u8 byteIndex;
    u8 rxByte;
    u8 txByte;
    u8 portBit;
    u16 byteCycles;
    u16 ackDelayCycles;
    void (*transferCallback)(struct PopsSioChannel *chan);
    s32 (*deviceHandler)(u32 byteIdx, u32 portSel, u8 txByte);
    u32 prevPollCycle;
    u32 lastPollCycle;
} PopsSioChannel;

/* PS1 hardware root counter (32 bytes x 3 timers) */
typedef struct PopsRootCounter {
    PopsEvent ev;
    s32 targetAndFreeze;
    u32 baseCycle;
    u32 mode;
    u8 irqBit;
    u8 alignPad;
    union {
        u16 pair;
        struct {
            u8 divShift;
            u8 subShift;
        } shifts;
    } clock;
} PopsRootCounter;

/* PS1 GPU texture page cache descriptor (8 bytes x 32 pages) */
typedef struct PopsTexPageSlot {
    u32 vramAddr;
    u8 loadedFormat;
    s8 groupOffset;
    s16 blitCoord;
} PopsTexPageSlot;

/* CD-ROM 16-sector cache entry (16 bytes x 17 entries) */
typedef struct PopsDiscCacheSlot {
    struct PopsDiscCacheSlot *next;
    struct PopsDiscCacheSlot *prev;
    u32 baseLba;
    u32 bufAddr;
} PopsDiscCacheSlot;

/* CD-ROM response event node (28 bytes x 2) */
typedef struct PopsCdEvent {
    PopsEvent ev;
    u8 respData[10];
    s8 irqCode;
    u8 respLen;
} PopsCdEvent;

/* Pad / DualShock controller slot (32 bytes x 8 slots) */
typedef struct PopsPadSlot {
    u8 activeCmd;
    u8 modeId;
    u8 respLen;
    u8 configMode;
    u8 statusByte;
    u8 dualshockMode;
    u8 subCmdArg;
    s8 pspPadIndex;
    u16 buttons;
    u16 rightStick;
    u16 leftStick;
    u8 extBytes[10];
    u8 motorSmall;
    u8 motorLarge;
    u8 rumbleSelectMask;
    u8 rumbleEnableMask;
    u8 analogEnabled;
    u8 rumbleBlockFlag;
    u8 modeLocked;
    u8 analogAllowed;
} PopsPadSlot;

/* Multitap state (36 bytes x 2 ports) */
typedef struct PopsMultitapState {
    u8 lastCmd;
    u8 tapFlags;
    u8 enabled;
    u8 connectTimer;
    u8 responseBuf[32];
} PopsMultitapState;

/* PS1 I/O register read/write dispatch table entry (8 bytes x 512) */
typedef struct PopsIoHandler {
    u32 (*readFn)(u32 addr, u32 width);
    void (*writeFn)(u32 addr, u32 value, u32 width);
} PopsIoHandler;

/* PS1 DMA channel MMIO mirror registers (16 bytes x 7 channels) */
typedef struct PopsDmaMmio {
    u32 madr;
    u32 bcr;
    u32 chcr;
    u32 gap;
} PopsDmaMmio;

/* Complete 16 KB PSP scratchpad state addressed via $gp (0x00010000) */
typedef struct PopsScratchpad {
    /* 0x0000..0x00FF: GTE data (0..31) and control (32..63) registers */
    union {
        u32 regs[64];
        struct {
            s16 vxZero, vyZero, vzZero, padZero;
            s16 vxOne, vyOne, vzOne, padOne;
            s16 vxTwo, vyTwo, vzTwo, padTwo;
            u32 rgbc;
            u16 otz;
            u16 otzPad;
            s32 irZero;
            s32 irOne;
            s32 irTwo;
            s32 irThree;
            s16 sxZero, syZero;
            s16 sxOne, syOne;
            s16 sxTwo, syTwo;
            s16 sxP, syP;
            u16 szZero, szZeroPad;
            u16 szOne, szOnePad;
            u16 szTwo, szTwoPad;
            u16 szThree, szThreePad;
            u32 rgbZero;
            u32 rgbOne;
            u32 rgbTwo;
            u32 resOne;
            s32 macZero;
            s32 macOne;
            s32 macTwo;
            s32 macThree;
            u32 irgb;
            u32 orgb;
            u32 lzcs;
            u32 lzcr;
            /* Control registers 32..63 (0x0080..0x00FF) */
            s16 rotMatrix[9];
            s16 rotPad;
            s32 trx, tryCoord, trz;
            s16 lightMatrix[9];
            s16 lightPad;
            s32 rbk, gbk, bbk;
            s16 colorMatrix[9];
            s16 colorPad;
            s32 rfc, gfc, bfc;
            s32 ofx, ofy;
            u16 hVal, hPad;
            s16 dqa, dqaPad;
            s32 dqb;
            s16 zsfThree, zsfThreePad;
            s16 zsfFour, zsfFourPad;
            u32 flagOrSavedRa;
        } named;
    } gte;

    /* 0x0100..0x019F: COP0 registers (0x120 BadVAddr, 0x130 SR, 0x134 CAUSE, 0x138 EPC, 0x13C PRID)
       plus spilled guest GPRs at 0x180..0x19F */
    u32 copZeroRegs[32];
    u32 spilledGprs[8];

    /* 0x01A0..0x01E7: Core CPU / execution / event queue / JIT pool state */
    u32 pc;                     /* 0x01A0 (416) */
    u32 regHi;                  /* 0x01A4 (420) */
    u32 regLo;                  /* 0x01A8 (424) */
    u32 cycleTarget;            /* 0x01AC (428) */
    s32 cycleRemaining;         /* 0x01B0 (432) */
    u32 jitReturnAddr;          /* 0x01B4 (436) */
    PopsEvent *eventHead;       /* 0x01B8 (440) */
    u32 eventTail;              /* 0x01BC (444) */
    s16 haltFlags;              /* 0x01C0 (448) */
    s8 blockCycleShift;         /* 0x01C2 (450) */
    u8 irqThrottleActive;       /* 0x01C3 (451) */
    u32 irqThrottleLastCycle;   /* 0x01C4 (452) */
    u32 irqThrottleCount;       /* 0x01C8 (456) */
    u32 jitRamCodePtr;          /* 0x01CC (460) */
    u32 jitBiosCodePtr;         /* 0x01D0 (464) */
    u32 jitExceptionHandler;    /* 0x01D4 (468) */
    u32 jitDirtyPageMask;       /* 0x01D8 (472) */
    u8 *subchannelTable;        /* 0x01DC (476) */
    u32 cacheControlReg;        /* 0x01E0 (480) */
    s32 deferredCycles;         /* 0x01E4 (484) */

    /* 0x01E8..0x02AF: DMA channels (7 x 28 bytes = 196 bytes) + pending masks */
    PopsDmaChannel dmaChannels[7];
    u16 dmaPendingMask;         /* 0x02AC (684) */
    u16 dmaRoundRobinIdx;       /* 0x02AE (686) */

    /* 0x02B0..0x0307: SIO0 & SIO1 serial channels (2 x 44 bytes = 88 bytes) */
    PopsSioChannel sio[2];

    /* 0x0308..0x038F: SPU / ME audio bridge state (136 bytes) */
    u16 spuTransferFifo[32];    /* 0x0308..0x0347 (776..839) */
    s16 spuCtrlReg;             /* 0x0348 (840) */
    u8 spuIrqStatusBit;         /* 0x034A (842) */
    u8 spuFifoCount;            /* 0x034B (843) */
    u32 spuTransferAddr;        /* 0x034C (844) */
    PopsEvent spuIrqEvent;      /* 0x0350..0x035F (848..863) */
    PopsEvent spuDmaEvent;      /* 0x0360..0x036F (864..879) */
    u32 spuDmaSrc;              /* 0x0370 (880) */
    u32 spuDmaLen;              /* 0x0374 (884) */
    u32 spuLastSyncCycle;       /* 0x0378 (888) */
    u32 spuSampleClock;         /* 0x037C (892) */
    u32 spuCtrlShadow;          /* 0x0380 (896) */
    u32 spuTailWords[3];        /* 0x0384..0x038F */

    /* 0x0390..0x065B: MDEC quant/IDCT tables and control/DMA state */
    u8 mdecLumaQuant[64];       /* 0x0390..0x03CF */
    u8 mdecChromaQuant[64];     /* 0x03D0..0x040F */
    u32 mdecVfpuConstBlock[9];  /* 0x0410..0x0433 */
    float mdecScaleTable[127];  /* 0x0434..0x062F */
    u32 mdecStatus;             /* 0x0630 (1584) */
    u32 mdecRemainingWords;     /* 0x0634 (1588) */
    u32 mdecInputLimit;         /* 0x0638 (1592) */
    u32 mdecOutputBytes;        /* 0x063C (1596) */
    u32 mdecInputPtr;           /* 0x0640 (1600) */
    u32 mdecOutputPtr;          /* 0x0644 (1604) */
    u32 mdecBlockPhase;         /* 0x0648 (1608) */
    PopsEvent mdecEvent;        /* 0x064C..0x065B */

    /* 0x065C..0x06BB: 3 PS1 root counters (96 bytes) */
    PopsRootCounter timers[3];

    /* 0x06BC..0x077F: Per-game compatibility quirk configuration (49 words) */
    u32 quirkFlags;             /* 0x06BC (1724) */
    s32 jitModeFlags;           /* 0x06C0 (1728) */
    s32 displayHeightClamp;     /* 0x06C4 (1732) */
    s32 displayTopAdjustNormal; /* 0x06C8 (1736) */
    s32 displayTopAdjustWide;   /* 0x06CC (1740) */
    u32 cdSpeedScale;           /* 0x06D0 (1744) */
    s32 branchCycleBias;        /* 0x06D4 (1748) */
    s32 displayTopOffsetLow;    /* 0x06D8 (1752) */
    s32 displayTopOffsetHigh;   /* 0x06DC (1756) */
    u32 gpuClockDivisor;        /* 0x06E0 (1760) */
    u32 gpuTexColorMask;        /* 0x06E4 (1764) */
    s32 jitChecksumLimit;       /* 0x06E8 (1768) */
    s32 gpuVblankSlices;        /* 0x06EC (1772) */
    u32 jitSplitAddress;        /* 0x06F0 (1776) */
    u32 jitWatchAddress;        /* 0x06F4 (1780) */
    u32 gpuDmaCycleShift;       /* 0x06F8 (1784) */
    u32 cdSeekDelayBias;        /* 0x06FC (1788) */
    u32 spuDelayQuirk;          /* 0x0700 (1792) */
    u32 jitBreakAddress;        /* 0x0704 (1796) */
    u32 cdSectorQuirk;          /* 0x0708 (1800) */
    u32 jitRamLimitQuirk;       /* 0x070C (1804) */
    u32 spuDmaLatency;          /* 0x0710 (1808) */
    u32 cdReadLatency;          /* 0x0714 (1812) */
    s32 vblankIrqDelay;         /* 0x0718 (1816) */
    s32 gpuTexPageCost;         /* 0x071C (1820) */
    s32 gpuReadCycleCost;       /* 0x0720 (1824) */
    u32 gpuUvBias;              /* 0x0724 (1828) */
    s32 gpuVramCopyShift;       /* 0x0728 (1832) */
    u32 padPollQuirk;           /* 0x072C (1836) */
    u32 sioIrqLatency;          /* 0x0730 (1840) */
    s32 displayWidthBias;       /* 0x0734 (1844) */
    s32 quirkFlagsExt;          /* 0x0738 (1848) */
    u32 quirkExtraWord;         /* 0x073C (1852) */
    u32 cdCacheQuirk;           /* 0x0740 (1856) */
    u32 quirkTail[15];          /* 0x0744..0x077F */

    /* 0x0780..0x0FFF: JIT register allocator, block compiler, and constant tracker */
    u32 jitBlockStartPtr;       /* 0x0780 (1920) */
    u32 jitCachedBasePtr;       /* 0x0784 (1924) */
    s32 jitCachedSpOffset;      /* 0x0788 (1928) */
    u32 jitCachedUpperAddr;     /* 0x078C (1932) */
    u8 jitBaseGuestReg;         /* 0x0790 (1936) */
    u8 jitHiLoLiveMask;         /* 0x0791 (1937) */
    u8 jitHiLoDirtyMask;        /* 0x0792 (1938) */
    u8 jitEvictCursor;          /* 0x0793 (1939) */
    u8 jitSlotDirty[12];        /* 0x0794..0x079F */
    u8 jitSlotGuestReg[12];     /* 0x07A0..0x07AB */
    u8 jitSlotHostReg[12];      /* 0x07AC..0x07B7 */
    s8 jitGuestRegMap[32];      /* 0x07B8..0x07D7 */
    u8 jitAllocScratch[936];    /* 0x07D8..0x0B7F (also MDEC decoded macroblock workspace) */
    u16 jitCompileFlags;        /* 0x0B80 (2944) */
    u16 jitBaseInstCycles;      /* 0x0B82 (2946) */
    s32 jitPendingCycles;       /* 0x0B84 (2948) */
    u32 jitGuestMemBias;        /* 0x0B88 (2952) */
    u32 jitIrEndPtr;            /* 0x0B8C (2956) */
    u32 jitBlockStartPc;        /* 0x0B90 (2960) */
    u32 jitBlockLimitPc;        /* 0x0B94 (2964) */
    u32 jitConstValidMask;      /* 0x0B98 (2968) */
    u32 jitConstValues[32];     /* 0x0B9C..0x0C1B */
    u8 jitWorkspaceTail[996];   /* 0x0C1C..0x0FFF */

    /* 0x1000..0x1FFF: PS1 I/O port dispatch table (512 entries x 8 bytes) */
    PopsIoHandler ioTable[512];

    /* 0x2000..0x33FF: PS1 hardware register mirrors and scratchpad mirror */
    u8 hwIoLow[0x70];           /* 0x2000..0x206F */
    u32 irqStatus;              /* 0x2070 (8304) I_STAT */
    u32 irqMask;                /* 0x2074 (8308) I_MASK */
    u32 hwIrqGap[2];            /* 0x2078..0x207F */
    PopsDmaMmio dmaRegs[7];     /* 0x2080..0x20EF */
    u32 dmaDpcr;                /* 0x20F0 (8432) */
    u32 dmaDicr;                /* 0x20F4 (8436) */
    u8 hwIoHigh[0x0F08];        /* 0x20F8..0x2FFF */
    u8 psxScratchpad[0x400];    /* 0x3000..0x33FF (PS1 1 KB dcache scratchpad) */

    /* 0x3400..0x37FF: PS1 GPU / GE display list translator state */
    PopsTexPageSlot texPages[32]; /* 0x3400..0x34FF (13312..13567) */
    u32 gpuCmdFifo[47];         /* 0x3500..0x35BB (13568..13755) */
    u32 gpuReadWord;            /* 0x35BC (13756) */
    s32 gpuLastVcount;          /* 0x35C0 (13760) */
    u32 gpuFrameStartCycle;     /* 0x35C4 (13764) */
    u32 gpuLastDisplayPos;      /* 0x35C8 (13768) */
    s32 gpuGeListId;            /* 0x35CC (13772) */
    PopsEvent gpuSliceEvent;    /* 0x35D0..0x35DF (13776..13791) */
    s32 gpuSliceRemaining;      /* 0x35E0 (13792) */
    PopsEvent gpuVblankEvent;   /* 0x35E4..0x35F3 (13796..13811) */
    s32 gpuVblankRemainder;     /* 0x35F4 (13812) */
    PopsEvent gpuBusyEvent;     /* 0x35F8..0x3607 (13816..13831) */
    u32 gpuSyncSpuTick;         /* 0x3608 (13832) */
    s32 gpuSyncCpuCycle;        /* 0x360C (13836) */
    u16 gpuDispX;               /* 0x3610 (13840) */
    u16 gpuDispY;               /* 0x3612 (13842) */
    u16 gpuHorizStart;          /* 0x3614 (13844) */
    u16 gpuHorizEnd;            /* 0x3616 (13846) */
    u16 gpuVertStart;           /* 0x3618 (13848) */
    u16 gpuVertEnd;             /* 0x361A (13850) */
    u16 gpuDispWidth;           /* 0x361C (13852) */
    u16 gpuDispHeight;          /* 0x361E (13854) */
    s16 gpuClipLeft;            /* 0x3620 (13856) */
    s16 gpuClipTop;             /* 0x3622 (13858) */
    s16 gpuClipRight;           /* 0x3624 (13860) */
    s16 gpuClipBottom;          /* 0x3626 (13862) */
    s16 gpuDrawOffX;            /* 0x3628 (13864) */
    s16 gpuDrawOffY;            /* 0x362A (13866) */
    u32 gpuDlistCursor;         /* 0x362C (13868) */
    u32 gpuStatusReg;           /* 0x3630 (13872) */
    u32 gpuPrevStatCycle;       /* 0x3634 (13876) */
    u32 gpuLastStatCycle;       /* 0x3638 (13880) */
    union {
        u32 word;
        struct { u16 x; u16 y; } xy;
    } gpuXferStart;             /* 0x363C (13884) */
    union {
        u32 word;
        struct { u16 w; u16 h; } wh;
    } gpuXferSize;              /* 0x3640 (13888) */
    union {
        u32 word;
        struct { u16 x; u16 y; } xy;
    } gpuXferCur;               /* 0x3644 (13892) */
    u32 gpuLastReadLatch;       /* 0x3648 (13896) */
    u32 gpuTexWindowRaw;        /* 0x364C (13900) */
    u8 gpuTexWinOffX;           /* 0x3650 (13904) */
    u8 gpuTexWinOffY;           /* 0x3651 (13905) */
    u8 gpuTexWinMaskX;          /* 0x3652 (13906) */
    u8 gpuTexWinMaskY;          /* 0x3653 (13907) */
    u16 gpuTexPageState;        /* 0x3654 (13908) */
    u8 gpuTexDepth;             /* 0x3656 (13910) */
    u8 gpuPacketState;          /* 0x3657 (13911) */
    s8 gpuListState;            /* 0x3658 (13912) */
    u8 gpuReadMode;             /* 0x3659 (13913) */
    u8 gpuDrawInsideTex;        /* 0x365A (13914) */
    u8 gpuDrawOverlapsDisp;     /* 0x365B (13915) */
    u8 gpuInterlaceHalf;        /* 0x365C (13916) */
    u8 gpuInterlaceActive;      /* 0x365D (13917) */
    u8 gpuPalMode;              /* 0x365E (13918) */
    u8 gpuModeDirty;            /* 0x365F (13919) */
    s8 gpuFrameDirty;           /* 0x3660 (13920) */
    u8 gpuDeferredFlip;         /* 0x3661 (13921) */
    u8 gpuFieldParity;          /* 0x3662 (13922) */
    u8 gpuFrontBufIndex;        /* 0x3663 (13923) */
    u8 gpuRingBufIndex;         /* 0x3664 (13924) */
    s8 gpuSkipMode;             /* 0x3665 (13925) */
    u8 gpuFrameThrottle;        /* 0x3666 (13926) */
    s8 gpuFirstFlipDone;        /* 0x3667 (13927) */
    union {
        u32 word;
        struct {
            u8 flippedThisVblank;
            s8 muteCountdown;
            u8 dispModeByte;
            u8 idleFrames;
        } bytes;
    } gpuDispFlags;             /* 0x3668 (13928..13931) */
    u8 gpuFifoRemaining;        /* 0x366C (13932) */
    u8 gpuFifoWritePos;         /* 0x366D (13933) */
    u16 gpuFifoPad;             /* 0x366E */
    s32 gpuPalFrameAccum;       /* 0x3670 (13936) */
    u8 gpuTailGap[0x18C];       /* 0x3674..0x37FF */

    /* 0x3800..0x39FF: CD-ROM (CXD2940Q) controller state */
    PopsCdEvent cdCmdEvent;     /* 0x3800..0x381B (14336..14363) */
    PopsCdEvent cdRespEvent;    /* 0x381C..0x3837 (14364..14391) */
    PopsEvent cdLidEvent;       /* 0x3838..0x3847 (14392..14407) */
    PopsEvent cdReadEvent;      /* 0x3848..0x3857 (14408..14423) */
    PopsEvent cdSpinupEvent;    /* 0x3858..0x3867 (14424..14439) */
    u8 cdRespFifo[12];          /* 0x3868..0x3873 (14440..14451) */
    u8 cdRespCount;             /* 0x3874 (14452) */
    u8 cdParamCount;            /* 0x3875 (14453) */
    u8 cdParamFifo[4];          /* 0x3876..0x3879 (14454..14457) */
    u16 cdSectorOffset;         /* 0x387A (14458) */
    u16 cdSectorLimit;          /* 0x387C (14460) */
    s8 cdQueuedCmd;             /* 0x387E (14462) */
    u8 cdCurrentTrack;          /* 0x387F (14463) */
    u8 cdMuted;                 /* 0x3880 (14464) */
    u8 cdSubqIndex;             /* 0x3881 (14465) */
    u8 cdBufWriteIdx;           /* 0x3882 (14466) */
    u8 cdBufReadIdx;            /* 0x3883 (14467) */
    u8 cdErrorStatusMask;       /* 0x3884 (14468) */
    u8 cdErrorCode;             /* 0x3885 (14469) */
    u8 cdCmdLock;               /* 0x3886 (14470) */
    u8 cdXaInterleave;          /* 0x3887 (14471) */
    u8 cdSetlocPending;         /* 0x3888 (14472) */
    u8 cdRespReadIdx;           /* 0x3889 (14473) */
    u8 cdDiscChangeReady;       /* 0x388A (14474) */
    u8 cdTrayState;             /* 0x388B (14475) */
    u8 cdIdleCountdown;         /* 0x388C (14476) */
    u8 cdSpeedQuirkByte;        /* 0x388D (14477) */
    u8 cdBusyRetries;           /* 0x388E (14478) */
    u8 cdInitialized;           /* 0x388F (14479) */
    u8 cdSpinupState;           /* 0x3890 (14480) */
    u8 cdSeekDataFlag;          /* 0x3891 (14481) */
    u16 cdAlignPad;             /* 0x3892..0x3893 */
    u32 cdSectorBufs[2];        /* 0x3894..0x389B (14484..14491) */
    u8 cdModeReg;               /* 0x389C (14492) */
    u8 cdStatByte;              /* 0x389D (14493) */
    u16 cdXaFilter;             /* 0x389E (14494) */
    u32 cdRngState;             /* 0x38A0 (14496) */
    u32 cdSeekEndCycle;         /* 0x38A4 (14500) */
    u32 cdCurrentLba;           /* 0x38A8 (14504) */
    u32 cdTargetLba;            /* 0x38AC (14508) */
    u32 cdXaNextLba;            /* 0x38B0 (14512) */
    u32 cdLastHeaderPtr;        /* 0x38B4 (14516) */
    union {
        u32 word;
        u8 bytes[4];
    } cdAudioVol;               /* 0x38B8..0x38BB (14520..14523) */
    u8 cdStatusPort;            /* 0x38BC (14524) */
    u8 cdIrqEnable;             /* 0x38BD (14525) */
    u8 cdIrqFlags;              /* 0x38BE (14526) */
    s8 cdRequestReg;            /* 0x38BF (14527) */
    u8 cdTailGap[0x140];        /* 0x38C0..0x39FF */

    /* 0x3A00..0x3CEF: Pad and multitap state */
    PopsPadSlot pads[8];        /* 0x3A00..0x3AFF */
    PopsMultitapState taps[2];  /* 0x3B00..0x3B47 */
    u8 padTailGap[0x1A8];       /* 0x3B48..0x3CEF */

    /* 0x3CF0..0x3CFF: JIT codegen trampolines */
    u32 jitChecksumFailTarget;  /* 0x3CF0 (15600) */
    u32 jitRamExitTrampoline;   /* 0x3CF4 (15604) */
    u32 jitBiosExitTrampoline;  /* 0x3CF8 (15608) */
    u32 jitCachePad;            /* 0x3CFC */

    /* 0x3D00..0x3EFF: Disc image / PBP / ATRAC3 CDDA cache and worker state */
    PopsDiscCacheSlot discCache[17]; /* 0x3D00..0x3E0F (15616..15887) */
    PopsDiscCacheSlot *discCacheHead;/* 0x3E10 (15888) */
    s32 discPendingLba;         /* 0x3E14 (15892) */
    u32 discLastVerifyTick;     /* 0x3E18 (15896) */
    s32 discLastReadOff;        /* 0x3E1C (15900) */
    u32 discReadBufEnd;         /* 0x3E20 (15904) */
    u32 discOffsets[5];         /* 0x3E24..0x3E37 (15908..15927) */
    u32 discPbpBaseOff;         /* 0x3E38 (15928) */
    SceUID discEventFlag;       /* 0x3E3C (15932) */
    SceUID discFileFd;          /* 0x3E40 (15936) */
    u8 discCount;               /* 0x3E44 (15940) */
    u8 discActiveIndex;         /* 0x3E45 (15941) */
    u8 discCddaActive;          /* 0x3E46 (15942) */
    u8 discPadByte;             /* 0x3E47 */
    s32 discCddaTrack;          /* 0x3E48 (15944) */
    s32 discCddaReadLba;        /* 0x3E4C (15948) */
    u32 discCddaWriteLba;       /* 0x3E50 (15952) */
    u32 discCddaUnused;         /* 0x3E54 (15956) */
    s32 discCddaPcmPos;         /* 0x3E58 (15960) */
    s32 discCddaStreamPos;      /* 0x3E5C (15964) */
    u32 discCddaFileOff;        /* 0x3E60 (15968) */
    u32 discCddaCryptKey;       /* 0x3E64 (15972) */
    s32 *discAtracOutLeft;      /* 0x3E68 (15976) */
    s32 *discAtracOutRight;     /* 0x3E6C (15980) */
    u8 discReadRetries;         /* 0x3E70 (15984) */
    u8 discTailGap[0x8F];       /* 0x3E71..0x3EFF */

    /* 0x3F00..0x3FFF: Version / boot metadata */
    u8 bootInitFlag;            /* 0x3F00 (16128) */
    u8 metaGap[0xBF];           /* 0x3F01..0x3FBF */
    char buildBranchString[64]; /* 0x3FC0 (16320) */
} PopsScratchpad;

POPS_STATIC_ASSERT(sizeof(PopsEvent) == 16, event_size);
POPS_STATIC_ASSERT(sizeof(PopsDmaChannel) == 28, dma_chan_size);
POPS_STATIC_ASSERT(sizeof(PopsSioChannel) == 44, sio_chan_size);
POPS_STATIC_ASSERT(sizeof(PopsRootCounter) == 32, root_counter_size);
POPS_STATIC_ASSERT(sizeof(PopsTexPageSlot) == 8, texpage_slot_size);
POPS_STATIC_ASSERT(sizeof(PopsDiscCacheSlot) == 16, disc_cache_size);
POPS_STATIC_ASSERT(sizeof(PopsScratchpad) == 0x4000, scratchpad_total);

#ifndef POPS_NO_GP_REG
register PopsScratchpad *g_pops __asm__("$28");
#endif

/* Display / VSH shared memory block at 0x49FE0000 */
typedef struct PopsDispSlot {
    s16 srcX;
    s16 srcY;
    s16 srcW;
    s16 srcH;
    u8 modeByte;
    u8 bufIndex;
} PopsDispSlot;

typedef struct PopsSharedMem {
    PopsDispSlot slots[2];      /* 0x000..0x013 */
    u8 frontIndex;              /* 0x014 */
    u8 padModes[8];             /* 0x015..0x01C */
    u8 padModeGap[3];           /* 0x01D..0x01F */
    u32 padInputs[8][4];        /* 0x020..0x09F */
    u8 displayEnable;           /* 0x0A0 */
    u8 readyFlag;               /* 0x0A1 */
    u8 pauseMenuOpen;           /* 0x0A2 */
    u8 cdActiveIndicator;       /* 0x0A3 */
    u8 screenMode;              /* 0x0A4 */
    u8 discCountShadow;         /* 0x0A5 */
    u8 resetRequest;            /* 0x0A6 */
    u8 padAssignMode;           /* 0x0A7 */
    u8 analogMode;              /* 0x0A8 */
    u8 fastForward;             /* 0x0A9 */
    u8 discLoadSpeed;           /* 0x0AA */
    u8 mcBusyIndicator;         /* 0x0AB */
    u8 customConfigBlock[240];  /* 0x0AC..0x19B */
    u8 customModeFlag;          /* 0x19C */
    u8 customDisplayState;      /* 0x19D */
    u8 mcChangeRequest[2];      /* 0x19E..0x19F */
    u32 discQuirkFlag;          /* 0x1A0 */
    u32 customScreenParams[16]; /* 0x1A4..0x1E3 */
} PopsSharedMem;

#define POPS_SHARED_MEM ((volatile PopsSharedMem *)POPS_DISPLAY_SHARED_BASE)

/* ME SPU shared control block at 0x49F40000 */
typedef struct PopsMeSpuShared {
    u16 spuRegs[256];           /* 0x000..0x1FF (0x1A4=reg[0xD2], 0x1AA=reg[0xD5], 0x1AE=reg[0xD7]) */
    u8 reservedLow[0x60];       /* 0x200..0x25F */
    u32 cddaQueue[8];           /* 0x260..0x27F */
    u32 keyOnMask;              /* 0x280 */
    u32 keyOffMask;             /* 0x284 */
    u32 voiceDirtyMask;         /* 0x288 */
    u32 cdAudioVol;             /* 0x28C */
    union {
        u32 word;
        u16 idxPair;
        struct {
            u8 writeIdx;
            u8 readIdx;
            u8 irqFired;
            s8 cddaMode;
        } parts;
    } cddaCtrl;                 /* 0x290..0x293 */
    u32 voiceRepeatMask;        /* 0x294 */
    u32 sampleCounter;          /* 0x298 */
    u8 quirkConfigBytes[4];     /* 0x29C..0x29F */
    u8 lockPad[5];              /* 0x2A0..0x2A4 */
    u8 cpuLockWant;             /* 0x2A5 */
    union {
        u16 pair;
        struct {
            u8 meLockWant;
            u8 lockTurn;
        } parts;
    } lockState;                /* 0x2A6..0x2A7 */
    u8 ramGap[0x18];            /* 0x2A8..0x2BF */
    u16 spuRam[0x40000];        /* 0x2C0..0x802BF (512 KB PS1 SPU RAM) */
} PopsMeSpuShared;

#define POPS_ME_SPU_SHARED ((volatile PopsMeSpuShared *)POPS_ME_SPU_SHARED_BASE)

/* Memory Card slot image & metadata (131,264 bytes per slot x 2 slots) */
typedef struct PopsMemCardSlot {
    u8 statusFlags;             /* +0: bit 0=inserted, bit 1=dirty */
    u8 checksum;                /* +1 */
    u16 sectorAddr;             /* +2 */
    u8 activeCmd;               /* +4 */
    u8 flagByte;                /* +5 */
    u8 pocketLen;               /* +6 */
    u8 pocketEnable;            /* +7 */
    u8 headerPad[56];           /* +8..+63 */
    u8 vmpHeader[128];          /* +64..+191 */
    u8 cardData[0x20000];       /* +192..+131263 (128 KB PS1 memory card) */
} PopsMemCardSlot;

POPS_STATIC_ASSERT(sizeof(PopsMemCardSlot) == 131264, mc_slot_size);

extern PopsMemCardSlot g_mcSlots[2];
extern SceUID g_mcWritebackSema;
extern SceUID g_mcWritebackAlarm;
extern u32 g_syscallStubBase;
extern s32 g_syscallTargetDelta;
extern u32 g_menuState;
extern u32 g_pauseReason;
extern u32 g_displayHoldState;
extern u32 g_vmcInitState;
extern u32 g_vmcLoadStatusA;
extern u32 g_vmcLoadStatusB;
extern u8 g_gpuInitPhase;
extern s32 g_gpuMcChangeTimer[2];
extern u32 g_gpuPadModeShadow;
extern u32 g_gpuPauseFlushDone;
extern u32 g_subchannelAllocPtr;
extern u32 g_discLicenseFlag;
extern u32 g_discKeyMask;
extern u32 g_discKeyInitDone;
/* PS1 Memory Card File System (MCFS) slot state (8,216 bytes = 2,054 words x 5 slots) */
typedef struct PopsMcfsDirEntry {
    u32 allocState;
    u32 fileSize;
    u16 nextBlock;
    char fileName[21];
    u8 unusedBytes[95];
    u8 extraFlag;
    u8 checksum;
} PopsMcfsDirEntry;

POPS_STATIC_ASSERT(sizeof(PopsMcfsDirEntry) == 128, mcfs_dir_entry_size);

typedef struct PopsMcfsCardState {
    u32 cardType;               /* +0 */
    u32 cardHeaderGap[3];       /* +4..+15 */
    u8 headerFrame[128];        /* +16..+143 (offset 4 words) */
    PopsMcfsDirEntry dir[15];   /* +144..+2063 (offset 36 words) */
    u8 brokenSectors[20][128];  /* +2064..+4623 (offset 516 words) */
    u8 mirrorGap[3584];         /* +4624..+8207 */
    void *rawImagePtr;          /* +8208 (word 2052) */
    u32 tailWord;               /* +8212 (word 2053) */
} PopsMcfsCardState;

POPS_STATIC_ASSERT(sizeof(PopsMcfsCardState) == 8216, mcfs_card_state_size);

extern PopsMcfsCardState g_mcfsCards[5];
extern s8 g_msInsertedState;
extern u8 g_powerSuspending;
extern s32 g_savedataStatus;
extern s32 g_savedataBusyFlag;
extern s32 g_savedataTickCount;
extern const char *g_customTitleIdPtr;
extern u8 g_dispAdjustBytes[16];
extern u8 g_savedataKey[16];
extern const char *const g_vmcFileNames[2];
extern const u8 g_padRumbleTable[4];
extern const u8 g_padCmdEData[8];
extern const u8 g_padCmdFData[8];
extern const u8 g_padCmdGData[4];
extern const u8 g_cdParamMinTable[32];
extern const u8 g_cdTestChipId[8];
extern const u8 g_cdTestDate[4];
extern const u8 g_cdTestVersion[4];
extern const u32 g_padDefaultState[2];
extern const u32 g_mdecInitWeights[7];

/* JIT intermediate representation node (16 bytes, array at 0x041B0000) */
typedef struct PopsJitIrNode {
    s16 flags;                  /* +0 */
    u8 dstReg;                  /* +2 */
    u8 opcode;                  /* +3 */
    u16 kind;                   /* +4 (also overwritten with emitted code ptr) */
    u16 cycles;                 /* +6 */
    u32 rawInsnOrTarget;        /* +8 */
    u8 srcRegA;                 /* +12 (also overwritten with branch patch ptr) */
    u8 srcRegB;                 /* +13 */
    u8 copReg;                  /* +14 */
    u8 instCycles;              /* +15 */
} PopsJitIrNode;

POPS_STATIC_ASSERT(sizeof(PopsJitIrNode) == 16, jit_ir_node_size);

/* Inline Allegrex helpers */
static __inline__ s32 popsRoundToInt(float x)
{
    float rounded;
    s32 res;
    __asm__("round.w.s %0, %1" : "=f"(rounded) : "f"(x));
    __asm__("mfc1 %0, %1" : "=r"(res) : "f"(rounded));
    return res;
}

static __inline__ u32 popsRotr32(u32 v, u32 n)
{
    return (v >> n) | (v << (32U - n));
}

static __inline__ u32 popsFloatBits(float x)
{
    union { float f; u32 u; } conv;
    conv.f = x;
    return conv.u;
}

static __inline__ float popsBitsToFloat(u32 u)
{
    union { float f; u32 u; } conv;
    conv.u = u;
    return conv.f;
}

static __inline__ void popsClearQuadVfpu(void *dst)
{
    __asm__ __volatile__("sv.q R403.q, 0(%0)" :: "r"(dst) : "memory");
}

/* =========================================================================
 * External PSP Imports (all 95 imports from module.yml)
 * ========================================================================= */

/* IoFileMgrForUser */
int sceIoDevctl(const char *dev, u32 cmd, void *indata, int inlen, void *outdata, int outlen);
int sceIoClose(SceUID fileDesc);
SceUID sceIoDopen(const char *dirname);
int sceIoDread(SceUID fileDesc, void *dir);
int sceIoDclose(SceUID fileDesc);
SceUID sceIoOpen(const char *file, int flags, int mode);

/* Kernel_Library */
void *sceKernelMemset(void *dst, int ch, SceSize size);
void *sceKernelMemcpy(void *dst, const void *src, SceSize size);

/* SysMemUserForUser */
int sceKernelSetCompilerVersion(u32 version);
int sceKernelSetCompiledSdkVersion660(u32 version);

/* ThreadManForUser */
int sceKernelDelayThread(u32 usec);
SceUID sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *opt);
int sceKernelSetEventFlag(SceUID evid, u32 bits);
SceUID sceKernelCreateCallback(const char *name, int (*func)(int, int, void *), void *arg);
int sceKernelStartThread(SceUID thid, SceSize arglen, void *argp);
int sceKernelWaitEventFlagCB(SceUID evid, u32 bits, u32 wait, u32 *outBits, u32 *timeout);
u32 sceKernelGetSystemTimeLow(void);
int sceKernelSignalSema(SceUID semaid, int signal);
SceUID sceKernelCreateThread(const char *name, int (*entry)(SceSize, void *),
                             int initPriority, int stackSize, u32 attr, void *opt);
int sceKernelWaitSema(SceUID semaid, int signal, u32 *timeout);
SceUID sceKernelCreateEventFlag(const char *name, u32 attr, int bits, void *opt);
SceUID sceKernelSetAlarm(u32 clock, u32 (*handler)(void *), void *common);
int sceKernelDelayThreadCB(u32 usec);
int sceKernelChangeThreadPriority(SceUID thid, int priority);
int sceKernelCancelAlarm(SceUID alarmid);

/* UtilsForUser */
void sceKernelDcacheWritebackAll(void);
void sceKernelDcacheInvalidateRangeForUser(const void *p, u32 size);
void sceKernelDcacheWritebackInvalidateRange(const void *p, u32 size);
void sceKernelDcacheWritebackRange(const void *p, u32 size);

/* pspvmc */
int pspvmc_0820B291(void *buf, u32 *outSize);
int pspvmc_38A87A12(void *buf);

/* sceCtrl */
int sceCtrlSetSamplingMode(int mode);
int sceCtrlSetSamplingCycle(int cycle);
int sceCtrlSetIdleCancelThreshold(int unlerp, int lerp);
int sceCtrlPeekBufferNegative(void *padData, int count);

/* sceDisplay */
int sceDisplayWaitVblankStartCB(void);
int sceDisplaySetHoldMode(int mode);
int sceDisplayWaitVblankStart(void);
int sceDisplayGetVcount(void);

/* sceDmac */
int sceDmacMemcpy(void *dst, const void *src, SceSize size);

/* sceGe_user */
int sceGeListSync(int qid, int syncType);
int sceGeListEnQueue(const void *list, void *stall, int cbid, void *arg);
int sceGeDrawSync(int syncType);
int sceGeEdramSetAddrTranslation(int width);
int sceGeListUpdateStallAddr(int qid, void *stall);

/* sceImpose */
int sceImposeGetLanguageMode(int *lang, int *button);

/* sceKermitPeripheral */
int sceKermitPeripheral_C0EBC631(void);
int sceKermitPeripheral_D27C5E03(u32 flag);

/* sceMeAudio */
int sceMeAudio_10DABACD(u32 arg);
int sceMeAudio_34F2548F(void *hdr);
int sceMeAudio_59759606(SceUID fileDesc, void *buf, u32 off);
int sceMeAudio_6FCDD82D(const void *src, const void *sha, u32 size);
int sceMeAudio_83609AC9(void *buf, u32 disc);
int sceMeAudio_899A3BC0(void *stallPtr);
int sceMeAudio_9FBE4AD3(u32 cmd, u32 byteVal);
int sceMeAudio_A651EA7B(SceUID fileDesc, void *buf, u32 off, u32 size);
int sceMeAudio_A8176E49(u32 *outBaseOff);
int sceMeAudio_C4169D0F(u32 port, u32 rumble);
int sceMeAudio_C47D3670(void);
int sceMeAudio_D96DC042(int listId, void *stallPtr);
int sceMeAudio_EB52DFE0(void);
int sceMeAudio_F1502A62(void *hdr, u32 disc);
int sceMeAudio_F7CD1362(u32 arg);

/* scePaf */
void *scePaf_0C2CD696(u32 align, u32 size);
void *scePaf_0FCDFA1E(u32 size);
char *scePaf_1F02DD65(char *dst, const char *src, u32 maxLen);
void scePaf_20BEF384(void *pngStruct);
u32 scePaf_3C4BC2CD(const char *str, char **endPtr, int base);
u32 scePaf_3E564415(void *pngStruct, void *pngInfo, u32 *width, u32 *height,
                    s32 *bitDepth, s32 *colorType, s32 *interlace,
                    void *compression, void *filter);
void *scePaf_43759F51(void *pngStruct);
void scePaf_4E43A742(void *pngStruct, void *pngInfo);
void scePaf_5A12583F(void *pngStruct, void *ioPtr, void *readFn);
void scePaf_66CCA794(void *pngStruct);
void scePaf_6D55F3F5(void *pngStruct);
void scePaf_71B92320(void *ptr);
int scePaf_7B7133D5(const void *bufA, const void *bufB, u32 size);
void *scePaf_8C5CC663(const char *ver, void *errPtr, void *errFn, void *warnFn);
int scePaf_9CD6C5F4(const void *bufA, const void *bufB, u32 size);
void scePaf_9D854500(void *pngStruct, void *rows);
int scePaf_A138A376(char *dst, const char *fmt, ...);
void scePaf_A631AC8B(void *pngStruct);
void *scePaf_B4652CFE(void *dst, const void *src, u32 size);
void *scePaf_D9E2D6E1(void *dst, int val, u32 size);
void scePaf_E0B32AE8(void *pngStruct, u32 filler, int flags);
void scePaf_E49835DC(void *pngStruct, u32 filler, int flags);
void scePaf_FF9C876B(void **pngStructPtr, void **pngInfoPtr, void **endInfoPtr);

/* scePopsMan */
int scePopsMan_0090B2C8(int errorCode);

/* scePower */
int scePowerRegisterCallback(int slot, SceUID cbid);
int scePowerSetClockFrequency(int pllfreq, int cpufreq, int busfreq);

/* sceSuspendForUser */
int sceKernelPowerUnlock(int type);
int sceKernelPowerLock(int type);

/* sceUtility */
int sceUtilitySavedataInitStart(void *params);
int sceUtilitySavedataGetStatus(void);
int sceUtilitySavedataShutdownStart(void);
int sceUtilitySavedataUpdate(int n);

/* =========================================================================
 * Global Data & Tables (pops_data.c)
 * ========================================================================= */
typedef enum PopsQuirkSlot {
    POPS_QUIRK_SLOT_FLAGS               = -1,
    POPS_QUIRK_SLOT_JIT_MODE_FLAGS      = 0,
    POPS_QUIRK_SLOT_DISPLAY_HEIGHT_CLAMP = 1,
    POPS_QUIRK_SLOT_DISPLAY_TOP_NORMAL  = 2,
    POPS_QUIRK_SLOT_DISPLAY_TOP_WIDE    = 3,
    POPS_QUIRK_SLOT_CD_SPEED_SCALE      = 4,
    POPS_QUIRK_SLOT_BRANCH_CYCLE_BIAS   = 5,
    POPS_QUIRK_SLOT_DISPLAY_TOP_LOW     = 6,
    POPS_QUIRK_SLOT_DISPLAY_TOP_HIGH    = 7,
    POPS_QUIRK_SLOT_GPU_CLOCK_DIVISOR   = 8,
    POPS_QUIRK_SLOT_GPU_TEX_COLOR_MASK  = 9,
    POPS_QUIRK_SLOT_JIT_CHECKSUM_LIMIT  = 10,
    POPS_QUIRK_SLOT_GPU_VBLANK_SLICES   = 11,
    POPS_QUIRK_SLOT_JIT_SPLIT_ADDRESS   = 12,
    POPS_QUIRK_SLOT_JIT_WATCH_ADDRESS   = 13,
    POPS_QUIRK_SLOT_GPU_DMA_CYCLE_SHIFT = 14,
    POPS_QUIRK_SLOT_CD_SEEK_DELAY_BIAS  = 15,
    POPS_QUIRK_SLOT_SPU_DELAY_QUIRK     = 16,
    POPS_QUIRK_SLOT_JIT_BREAK_ADDRESS   = 17,
    POPS_QUIRK_SLOT_CD_SECTOR_QUIRK     = 18,
    POPS_QUIRK_SLOT_JIT_RAM_LIMIT       = 19,
    POPS_QUIRK_SLOT_SPU_DMA_LATENCY     = 20,
    POPS_QUIRK_SLOT_CD_READ_LATENCY     = 21,
    POPS_QUIRK_SLOT_VBLANK_IRQ_DELAY    = 22,
    POPS_QUIRK_SLOT_GPU_TEX_PAGE_COST   = 23,
    POPS_QUIRK_SLOT_GPU_READ_CYCLE_COST = 24,
    POPS_QUIRK_SLOT_GPU_UV_BIAS         = 25,
    POPS_QUIRK_SLOT_GPU_VRAM_COPY_SHIFT = 26,
    POPS_QUIRK_SLOT_PAD_POLL_QUIRK      = 27,
    POPS_QUIRK_SLOT_SIO_IRQ_LATENCY     = 28,
    POPS_QUIRK_SLOT_DISPLAY_WIDTH_BIAS  = 29,
    POPS_QUIRK_SLOT_FLAGS_EXT           = 30
} PopsQuirkSlot;

typedef struct PopsQuirkPair {
    s32 slot;
    u32 value;
} PopsQuirkPair;

typedef struct PopsQuirkIndex {
    u32 titleHash;
    u32 entryCount;
    const PopsQuirkPair *quirkPairs;
} PopsQuirkIndex;

extern u8 g_psxBiosRom[0x80000];
extern const u8 g_psxBiosPatch[0x16120];
extern const s8 g_jitDefaultRegMap[32];
extern const u8 g_copWriteKindTable[96];
extern void (*const g_gteDispatchTable[64])(u32 cmd);
extern const u8 g_gteCycleTable[64];
extern const u32 g_gteMvmvaTable[12];
extern const u32 g_mdecIdctCoeffTable[39];
extern const u32 g_gpuGeInitList[];
extern const u32 g_gpuGePostList[];
extern const u32 g_gpuGeSubLists[];
extern const u32 *const g_gpuGeDrawTableA[16];
extern const u32 *const g_gpuGeDrawTableB[4];
extern const u32 *const g_gpuGeDrawTableC[3];
extern const u32 *const g_gpuGeDrawTableD[8];
extern const u8 g_gpuCmdWordLen[64];
extern u8 g_gpuDispModeTable[5][9];
extern const u8 g_atracSpecTableA[20];
extern const u8 g_atracSpecTableB[20];
extern const u8 g_atracBitLenTable[191];
extern const s8 g_atracHuffDecodeTable[737];
extern const float g_atracQuantTable[204];
extern const float g_atracGainScaleTable[31];
extern const float g_atracImdctSinCos[512];
extern const float g_atracButterflyTable[127];
extern const u16 g_atracBitRevPairs[128];
extern const PopsQuirkIndex g_popsQuirkIndex[1043];
extern const PopsQuirkPair g_popsQuirkPool[];
extern const char g_popsSpecialTitleIds[5][12];

/* =========================================================================
 * Assembly Runtime Helpers (pops_jit_asm.S, pops_vfpu_asm.S)
 * ========================================================================= */
void popsJitEnter(void);
void popsJitEventLoop(void);
u32 popsMemReadByte(u32 addr);
void popsMemWriteByte(u32 addr, u32 val);
u32 popsMemReadHalf(u32 addr);
void popsMemWriteHalf(u32 addr, u32 val);
u32 popsMemReadWord(u32 addr);
void popsMemWriteWord(u32 addr, u32 val);
u32 popsMemReadByteUnsigned(u32 addr);
void popsJitDispatch(void);
void popsJitDispatchAlt(void);
u32 popsMemReadHalfUnsigned(u32 addr);
void popsJitCompileEntry(void);
void popsJitInvalidateEntry(void);
void popsJitLinkPatchEntry(void);
u32 popsMemTranslateUnaligned(u32 addr);
void popsJitVerifyChecksum(void);

u32 popsIoReadDefault(u32 addr, u32 width);
u32 popsIoReadHalf(u32 addr);
u32 popsIoReadWord(u32 addr);
u32 popsIoReadZero(u32 addr);
u32 popsIoReadByteUnsigned(u32 addr);
u32 popsIoReadHalfUnsigned(u32 addr);
u32 popsIoReadZeroAlt(u32 addr);
void popsIoWriteDefault(u32 addr, u32 val, u32 width);
void popsIoWriteHalf(u32 addr, u32 val);
void popsIoWriteWord(u32 addr, u32 val);
void popsIoWriteNop(u32 addr, u32 val);

void popsMdecDecodeVfpu(void);
void popsGteDpclFlagged(void);
void popsGteVfpuKernel(void);
void popsGteCdp(void);
void popsGteCc(void);
void popsGteNcds(void);
void popsGteNcdt(void);
void popsGteNccs(void);
void popsGteNcct(void);
void popsGteNcs(void);
void popsGteNct(void);
void popsGteMvmvaFlagged(void);
void popsGteMvmvaFlaggedInner(void);
void popsGteMvmva(u32 matAndShift, s32 limMin, u32 vecPtr, u32 transPtr);
void popsGteRtpsFlagged(void);
void popsGteRtps(void);
void popsGteRtptFlagged(void);
void popsGteRtpt(void);
void popsGteNclip(void);
void popsGteSqr(u32 shift);
void popsGteAvsz3(void);
void popsGteAvsz4(void);
void popsGteGpf(u32 shift);
void popsGteGpl(u32 shift);
void popsGteDpcs(void);
void popsGteIntpl(void);
void popsGteDpct(void);
void popsGteOp(u32 shift);
u32 popsGteReadRgb(void);
void popsVfpuInitConstants(void);
void popsSyscallPatchTrampoline(void);
void popsSyscallPatchRet(void);

/* =========================================================================
 * C Internal Prototypes across pops_* files
 * ========================================================================= */
/* pops_jit.c */
s32 popsJitFindGuestReg(s32 guestReg);
s32 popsJitPrevNotBranch(u32 emitPtr);
u32 *popsJitLoadGuestReg(u32 *emitPtr, s32 hostReg, s32 guestReg);
u32 *popsJitEmitImm(u32 *emitPtr, s32 hostReg, u32 imm);
u32 *popsJitSpillSlot(s32 slotIdx, u32 *emitPtr);
s32 popsJitAllocTempReg(s32 fallbackReg, u8 dirtyFlag);
void popsJitFreeTempReg(s32 hostReg);
u32 *popsJitFlushHiLo(u32 *emitPtr);
u32 *popsJitFlushSlots(u32 *emitPtr, s32 maxSlot);
u32 *popsJitSaveAllDirty(u32 *emitPtr);
u32 popsJitAcquireReg(u32 *emitPtr, s32 guestReg, u32 liveMask, u8 modeFlags);
u32 *popsJitEmitCallWithDelay(u32 *emitPtr, u32 encodedTarget);
u32 *popsJitAcquirePair(u32 *emitPtr, s32 dstGuest, s32 srcGuest, s32 *outDst, s32 *outSrc);
u32 *popsJitEmitAluImm(u32 op, s32 dstGuest, s32 srcGuest, u32 imm, u32 *emitPtr);
u32 *popsJitMoveToHost(u32 *emitPtr, s32 hostReg, s32 guestOrHost);
u32 *popsJitStoreFromV0(u32 *emitPtr, s32 guestOrHost, s32 allowDirectSpill);
u32 *popsJitEmitConstMemAccess(u32 *emitPtr, s32 op, s32 guestReg, u8 region, u32 addr, s32 directSpill);
u32 *popsJitEmitMemAccess(s32 op, s32 valReg, s32 baseReg, s16 offset, u32 *emitPtr, s32 directSpill);
u32 *popsJitEmitExitBranch(u32 *emitPtr, u32 **patchSlot, s32 cycles, u32 target);
u32 *popsJitEmitCondBranch(u32 branchOp, s32 regA, s32 regB, s16 cycles, u32 **patchSlot, u32 *emitPtr);
u32 *popsJitEmitCycleSub(s32 cycles, u32 *emitPtr);
u32 *popsJitEmitCopRead(u32 *emitPtr, s32 dstReg, s32 copOffset);
u32 *popsJitEmitCopWrite(u8 kind, s32 srcReg, u32 copOffset, u32 *emitPtr);
u32 *popsJitEmitBlockChecksum(u32 guestPc, u32 *emitPtr, s32 insnCount);
u32 *popsJitInvalidateBlock(void);
u32 *popsJitTrackConst(s32 guestReg, u32 constVal, u32 *emitPtr, PopsJitIrNode *node);
u32 *popsJitEmitBranchComparePreload(PopsJitIrNode *node, u32 *emitPtr);
s32 popsJitDecodePass(PopsJitIrNode *startNode);
u32 *popsJitRecompile(u32 guestPc);
u32 *popsJitEmitBlockLink(u32 targetPc, u32 *emitPtr);
u32 *popsJitEmitIrNode(s32 kind, PopsJitIrNode *node, u32 *emitPtr, s32 delayCycles);
u32 popsJitFlushCodeCache(void);

/* pops_dma_io.c */
void popsSpuCheckIrq(void);
void popsSpuWriteReg(u32 addr, u32 value, u32 width);
s32 popsSpuQueueCddaSector(u32 sectorAddr, s32 mode);
s32 popsSpuGetCddaQueuedCount(void);
s32 popsSpuGetCddaPending(void);
void popsSpuIrqPollCallback(PopsEvent *ev);
u32 popsSpuReadReg(u32 addr, u32 width);
s32 popsSpuDmaTransfer(u32 addr, u32 bytes, u32 chcr);
void popsSpuDmaDeferredCallback(PopsEvent *ev);
void popsSpuSyncVoiceRead(void);
void popsSpuSyncStatusRead(void);
u32 popsIoReadSlow(u32 addr, u32 width);
void popsIoWriteSlow(u32 addr, u32 value, u32 width);
void popsDmaCompleteCallback(PopsEvent *ev);
u32 popsDmaDelayActiveChannels(u16 syncMask, u32 addCycles, u32 scaleLimit);
void popsDmaContinueCallback(PopsEvent *ev);
void popsDmaRunPendingQueue(void);
u32 popsDmaAdjustMdecCycles(u32 inDelta, u32 outDelta, u32 scaleLimit);
void popsDmaStartChannel(u32 chanIdx);
void popsDmaWriteCtrl(u32 addr, u32 value, u32 width);
void popsDmaWriteChannelReg(u32 addr, u32 value, u32 width);
s32 popsDmaExecuteOtc(u32 addr, u32 bytes, u32 chcr);
s32 popsEventSchedule(PopsEvent *ev, s32 deltaCycles);
u32 *popsCpuRaiseException(u32 exCode);
u32 popsEventDispatch(void);
void popsEventCancel(PopsEvent *ev);
u32 popsCpuCheckIrq(void);
u32 popsIrqRaise(u32 irqBit);
void popsGteMvmvaWrapper(u32 cmd);
void popsGteSqrWrapper(u32 cmd);
void popsGteGpfWrapper(u32 cmd);
void popsGteGplWrapper(u32 cmd);
void popsGteOpWrapper(u32 cmd);

/* pops_sio_pad.c */
u32 popsIrqReadReg(u32 addr, u32 width);
void popsIrqWriteReg(u32 addr, u32 value, u32 width);
void popsTimersNotifyVblank(s32 inVblank);
void popsTimerScheduleNext(PopsRootCounter *tmr);
void popsTimerEventCallback(PopsEvent *ev);
u32 popsTimerUpdateValue(PopsRootCounter *tmr);
u32 popsTimerReadReg(u32 addr, u32 width);
void popsTimerWriteReg(u32 addr, u32 value, u32 width);
void popsSioTransferCallback(PopsSioChannel *chan);
u32 popsSioReadReg(u32 addr, u32 width);
void popsSioWriteData(u32 addr, u32 value, u32 width);
void popsSioWriteCtrl(u32 addr, u32 value, u32 width);
void popsSioUpdateBaud(PopsSioChannel *chan, const u16 *hwRegs);
void popsSioIrqCallback(PopsEvent *ev);
s32 popsPadExchangeByte(u32 byteIdx, u32 portSel, u8 txByte);
s32 popsPadIsConnected(u32 slotIdx);
void popsPadMuteAllRumble(void);
void popsPadRestoreAllRumble(void);
s32 popsMcExchangeByte(u32 byteIdx, u32 slotIdx, u8 txByte);
s32 popsMcHasInsertedCard(u32 slotIdx);
s32 popsMsfToLba(const u8 *msf);

/* pops_cdrom.c */
void popsCdExecCommand(u32 cmd);
u32 popsCdFindTrackForLba(u32 lba);
void popsCdFillTrackRelMsf(u32 lba, u8 *outSubq);
void popsCdDeliverRespCallback(PopsEvent *ev);
void popsCdEmitImmediateResp(s8 irqCode);
u8 *popsCdQueueAckResp(u8 respLen, s32 delayCycles);
void popsCdQueueCompleteResp(u8 respLen, s32 delayCycles);
s32 popsCdComputeSeekCycles(u32 targetLba);
void popsCdDataReadCallback(PopsEvent *ev);
void popsCdAudioPlayCallback(PopsEvent *ev);
void popsCdSpinupCallback(PopsEvent *ev);
s32 popsCdDmaTransfer(u32 addr, u32 bytes, u32 chcr);
u32 popsCdReadReg(u32 addr, u32 width);
void popsCdWriteReg(u32 addr, u32 value, u32 width);
void popsDiscPrefetch(u32 lba);
u8 *popsDiscFetchSector(u32 lba, s32 blocking);
void popsDiscStartCddaTrack(s32 trackNum, u32 startLba);
void popsDiscStopCddaTrack(void);
int popsDiscWorkerThread(SceSize args, void *argp);
void popsDiscWaitWorkerIdle(void);
s32 popsDiscReadCddaStream(u32 *dstBuf, u32 byteLen);

/* pops_mdec.c / pops_range.c */
s32 popsRangeDecodeBlock(s32 dstSize, const u8 *src, u8 *dst);
s32 popsMdecDmaIn(u32 addr, u32 bytes, u32 chcr);
s32 popsMdecDmaOut(u32 addr, u32 bytes, u32 chcr);
u32 popsMdecStatusRead(u32 addr, u32 width);
void popsMdecControlWrite(u32 addr, u32 value, u32 width);
u32 popsGteWriteIrgb(void);

/* pops_gpu.c */
s32 popsGpuHandleMenuAndVmc(void);
void popsGpuVblankStartCallback(PopsEvent *ev);
void popsGpuThrottleSpeed(void);
void popsGpuFlipDisplay(void);
void popsGpuBusyDoneCallback(PopsEvent *ev);
void popsGpuSetDisplayEnable(u8 enable);
s32 popsGpuMarkFrameDirty(void);
void popsGpuSetAnalogMode(u32 mode);
u32 popsGpuGetScreenMode(void);
u32 popsGpuSetCustomParam(u32 idx, u32 val);
s32 popsGpuAdvanceFrameSlot(s16 height, s16 topOff);
void popsGpuVblankEndCallback(PopsEvent *ev);
void popsGpuSliceCallback(PopsEvent *ev);
void popsGpuWriteReg(u32 addr, u32 value, u32 width);
s32 popsGpuDmaTransfer(u32 addr, u32 bytes, u32 chcr);
u32 popsGpuReadReg(u32 addr, u32 width);
void popsGpuReadVramData(u32 dstAddr, u32 byteLen);
s32 popsGpuExecutePackets(u32 startAddr, u32 byteLen);
void popsGpuScheduleNextVblank(s32 cycles);
void popsGpuVblankPostCallback(PopsEvent *ev);

/* pops_main.c */
int module_start(SceSize args, void *argp);
int popsMainThread(SceSize args, void *argp);
void popsPublishSpuQuirksA(void);
void popsPublishSpuQuirksB(void);
u32 popsInitJitPools(void);
void popsInitSpu(void);
void popsRegisterIoRange(u32 startAddr, u32 byteLen,
                         u32 (*readFn)(u32, u32),
                         void (*writeFn)(u32, u32, u32));
void popsInitSlowIo(void);
void popsInitDma(void);
void popsRegisterDmaChannel(u8 chanIdx, s32 (*transferFn)(u32, u32, u32));
void popsInitEventQueue(void);
void popsInitCpuAndBios(void);
void popsInitIrqIo(void);
void popsInitGteUnrTable(void);
void popsInitTimers(void);
void popsInitSio(void);
void popsSioNop(PopsSioChannel *chan);
s32 popsSioNullDevice(u32 byteIdx, u32 portSel, u8 txByte);
s32 popsPadApplyConfig(s32 assignMode, s32 analogMode, s32 analogAllow);
s32 popsPadResetAll(void);
s32 popsMcIsWritebackIdle(void);
s32 popsMcPollInitialLoad(void);
void popsMcFlushBothSlots(void);
s32 popsMcSetSlotInserted(u32 slotIdx, s32 inserted);
s32 popsMcSyncBeforeRun(void);
SceUID popsInitMcWorker(void);
s32 popsMcInitPocketStation(void);
u32 popsMcGetPocketEnable(void);
u32 popsMcWritebackAlarmCb(void *common);
int popsMcWorkerThread(SceSize args, void *argp);
void popsCdStartDiscSwap(void);
void popsCdReset(void);

/* pops_init.c */
void popsCdMarkDiscChangeReady(void);
u32 popsCdGetDiscChangeReady(void);
void popsCdSetLoadSpeed(s32 fastMode);
void popsCdInitState(void);
void popsCdLidOpenCallback(PopsEvent *ev);
s32 popsValidateTitleId(const char *titleId);
s32 popsDiscSelectImage(void);
s32 popsDiscMountPbp(void);
SceUID popsDiscOpenImageFd(void);
u32 popsDiscGetCount(void);
s32 popsDiscSwapAndReload(void);
s32 popsDiscCanSwap(void);
char *popsStripTitleSeparators(char *str, s32 maxLen);
void popsInitMdec(void);
void popsGpuApplyClocks(void);
void popsGpuSetPalMode(s32 mode);
void popsGpuBuildDispAdjust(s32 xShift, s8 yShift, s32 zoomDelta, s32 heightMode);
void popsSetDiscCountShadow(u8 count);
s32 popsGpuFlushGeList(void);
s32 popsGpuRestartGeList(void);
void popsInitGpu(void);
s32 popsLoadGameQuirks(const char *titleId, u32 pkgVersion, u32 minVersion, const u8 *extraConfig);
s32 popsPatchSyscallStubs(void);
u32 *popsResetMachine(void);
u32 *popsInitAllSubsystems(void);
s32 popsInitCustomScreenConfig(const char *titleStr, const char *gameId, const u8 *srcCfg);
s32 popsMenuNoop(void);
u32 popsGetPauseState(void);
u32 popsSetPauseState(u32 newState);
void popsUpdateDisplayMute(void);
void popsNullStubA(void);
s32 popsGetZeroSetting(void);
s32 popsGetVmcCreatedFlag(void);
s32 popsCheckVmcCreated(void);
s32 popsLatchDiscLicense(s32 token);
void popsCopyTitleHeaderBlock(const u32 *srcWords);
void popsSetIconChecksumKey(s8 widthByte);
s32 popsInitSavedataConfig(void);
s32 popsCheckExitState(void);
void popsHandleDiscError(s32 throttleOnResume);
int popsPowerCallback(int notifyId, int powerInfo, void *common);
int popsMsCallback(int notifyId, int msEvent, void *common);
s32 popsCheckExitRequest(void);

/* pops_vmc_savedata.c */
s32 popsLoadPbpIconPng(SceUID fileDesc);
void popsSavedataInitContext(const char *titleStr, const char *gameId, u32 unusedParam);
s32 popsSavedataClassifyError(s32 resultCode);
s32 popsSavedataSaveConfig(void);
s32 popsSavedataGetResult(u32 ctxIdx);
s32 popsSavedataHasSlot(void);
s32 popsSavedataSetConfigBuf(void *bufPtr, u32 bufSize);
s32 popsSavedataBindSlotBuf(u32 ctxIdx, u32 slotIdx, void *vmpBuf);
s32 popsSavedataPollStatus(void);
s32 popsSavedataWaitIdle(void);
s32 popsSavedataSaveVmc(u32 slotIdx);
s32 popsVmcLoadByCustomTitle(u32 slotIdx);
s32 popsVmcLoadCurrentTitle(u32 slotIdx);
s32 popsMcfsBindRawBuffer(u32 slotIdx, void *rawCardBuf);
s32 popsMcfsRepairEmptyBlocks(u32 slotIdx);
s32 popsMcfsIsCardEmpty(u32 slotIdx);
void popsPngReadCallback(void *pngStruct, void *outBuf, u32 byteCount);
s32 popsProbePngWidth(void *pngData, u32 pngSize);

/* pops_mcfs.c */
s32 popsMcfsWriteRaw(u32 slotIdx, const void *src, u32 offset, u32 byteLen);
s32 popsMcfsReadRaw(u32 slotIdx, void *dst, u32 offset, u32 byteLen);
s32 popsMcfsCheckReady(void);
const u32 *popsMcfsReadU32(const u32 *src, u32 *outVal);
const u16 *popsMcfsReadU16(const u16 *src, u16 *outVal);
u32 *popsMcfsWriteU32(u32 *dst, u32 val);
u16 *popsMcfsWriteU16(u16 *dst, u16 val);
u32 popsMcfsComputeXor(const u8 *frameBytes);
s32 popsMcfsReadFrames(u32 slotIdx, void *dst, u32 offset, u32 byteLen);
s32 popsMcfsWriteFrames(u32 slotIdx, const void *src, u32 offset, u32 byteLen);
s32 popsMcfsCheckMagic(u32 slotIdx);
s32 popsMcfsFormatCardBody(u32 slotIdx);
s32 popsMcfsMountCard(u32 slotIdx);
s32 popsMcfsCountFreeBlocks(u32 slotIdx, u32 *outFreeBlocks);
s32 popsMcfsGetCardInfo(u32 slotIdx, u32 *outInfo);
s32 popsMcfsFormatCard(u32 slotIdx);

/* pops_atrac.c */
s32 popsAtracDecodeSpectrum(void *workCtx, const u8 *specTable, s32 selector, s32 scaleIdx, float *dstStart, float *dstEnd);
s32 popsAtracDecodeFrame(const u8 *inBuf, float **outChans, void *workCtx);
void *popsAtracInitDecoder(s32 channels, s32 jointStereo, s32 frameBytes, s32 modeParam);

#endif /* POPS_INTERNAL_H */
