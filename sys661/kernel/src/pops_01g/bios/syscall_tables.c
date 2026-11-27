/*
 * Complete PlayStation 1 BIOS A0 (0x00..0xB4), B0 (0x00..0x5D), and C0 (0x00..0x1C)
 * syscall jump tables installed into kernel RAM at 0x80000200, 0x80000874, 0x80000674.
 */

#include "bios_internal.h"

static s32 biosUnimplemented(void)
{
    return 0;
}

static void biosNoop(void)
{
}

static s32 biosReturnOne(void)
{
    return 1;
}

static void biosSystemError(void)
{
    for (;;)
        ;
}

static void biosSetMemSize(u32 megabytes)
{
    *(volatile u32 *)0x80000060U = megabytes;
    PSX_IO32(REG_RAM_SIZE) = (megabytes >= 8U) ? 0x00000B98U : 0x00000B88U;
}

static void biosSetConf(u32 evcbCount, u32 tcbCount, u32 stackTop)
{
    PSX_BIOS->cfgEvcbCount = evcbCount;
    PSX_BIOS->cfgTcbCount = tcbCount;
    PSX_BIOS->cfgStackTop = stackTop;
    biosInitKernelTables(tcbCount, evcbCount);
}

static void biosGetConf(u32 *evcbOut, u32 *tcbOut, u32 *stackOut)
{
    if (evcbOut != NULL)
        *evcbOut = PSX_BIOS->cfgEvcbCount;
    if (tcbOut != NULL)
        *tcbOut = PSX_BIOS->cfgTcbCount;
    if (stackOut != NULL)
        *stackOut = PSX_BIOS->cfgStackTop;
}

static void biosInstallTable(u32 *dst, const void *const *src, u32 count)
{
    u32 i;

    for (i = 0; i < count; i++)
        dst[i] = (u32)(src[i] != NULL ? src[i] : (const void *)biosUnimplemented);
}

static const void *const g_a0Table[0xB5] = {
    [0x00] = (const void *)biosFileOpen,
    [0x01] = (const void *)biosFileSeek,
    [0x02] = (const void *)biosFileRead,
    [0x03] = (const void *)biosFileWrite,
    [0x04] = (const void *)biosFileClose,
    [0x05] = (const void *)biosUnimplemented, /* FileIoctl */
    [0x06] = (const void *)biosWarmBoot,      /* exit */
    [0x07] = (const void *)biosUnimplemented, /* FileGetDeviceFlag */
    [0x08] = (const void *)biosFileGetc,
    [0x09] = (const void *)biosFilePutc,
    [0x0A] = (const void *)biosTolower,       /* todigit */
    [0x0B] = (const void *)biosAtoi,
    [0x0C] = (const void *)biosStrtoul,
    [0x0D] = (const void *)biosStrtol,
    [0x0E] = (const void *)biosAbs,
    [0x0F] = (const void *)biosAbs,           /* labs */
    [0x10] = (const void *)biosAtoi,
    [0x11] = (const void *)biosAtoi,          /* atol */
    [0x12] = (const void *)biosStrtol,        /* atob */
    [0x13] = (const void *)biosSetjmp,
    [0x14] = (const void *)biosLongjmp,
    [0x15] = (const void *)biosStrcat,
    [0x16] = (const void *)biosStrncat,
    [0x17] = (const void *)biosStrcmp,
    [0x18] = (const void *)biosStrncmp,
    [0x19] = (const void *)biosStrcpy,
    [0x1A] = (const void *)biosStrncpy,
    [0x1B] = (const void *)biosStrlen,
    [0x1C] = (const void *)biosStrchr,        /* index */
    [0x1D] = (const void *)biosStrrchr,       /* rindex */
    [0x1E] = (const void *)biosStrchr,
    [0x1F] = (const void *)biosStrrchr,
    [0x20] = (const void *)biosStrpbrk,
    [0x21] = (const void *)biosStrspn,
    [0x22] = (const void *)biosStrcspn,
    [0x23] = (const void *)biosStrtok,
    [0x24] = (const void *)biosStrstr,
    [0x25] = (const void *)biosToupper,
    [0x26] = (const void *)biosTolower,
    [0x27] = (const void *)biosBcopy,
    [0x28] = (const void *)biosBzero,
    [0x29] = (const void *)biosBcmp,
    [0x2A] = (const void *)biosMemcpy,
    [0x2B] = (const void *)biosMemset,
    [0x2C] = (const void *)biosMemmove,
    [0x2D] = (const void *)biosMemcmp,
    [0x2E] = (const void *)biosMemchr,
    [0x2F] = (const void *)biosRand,
    [0x30] = (const void *)biosSrand,
    [0x31] = (const void *)biosQsort,
    [0x32] = (const void *)biosStrtol,        /* strtod stub */
    [0x33] = (const void *)biosMalloc,
    [0x34] = (const void *)biosFree,
    [0x35] = (const void *)biosBsearch,       /* lsearch */
    [0x36] = (const void *)biosBsearch,
    [0x37] = (const void *)biosCalloc,
    [0x38] = (const void *)biosRealloc,
    [0x39] = (const void *)biosInitHeap,
    [0x3A] = (const void *)biosWarmBoot,      /* SystemErrorExit */
    [0x3B] = (const void *)biosGetchar,
    [0x3C] = (const void *)biosPutchar,
    [0x3D] = (const void *)biosGets,
    [0x3E] = (const void *)biosPuts,
    [0x3F] = (const void *)biosPrintf,
    [0x40] = (const void *)biosSystemError,   /* SystemErrorUnresolvedException */
    [0x41] = (const void *)biosLoadExe,       /* LoadTest */
    [0x42] = (const void *)biosLoadExe,       /* Load */
    [0x43] = (const void *)biosExecExe,       /* Exec */
    [0x44] = (const void *)biosFlushCache,
    [0x45] = (const void *)biosInstallVectors,/* init_a0_b0_c0_vectors */
    [0x46] = (const void *)biosGpuDw,
    [0x47] = (const void *)biosGpuCw,         /* GPU_send_dma */
    [0x48] = (const void *)biosSendGp1,
    [0x49] = (const void *)biosGpuCw,
    [0x4A] = (const void *)biosGpuCwp,
    [0x4B] = (const void *)biosGpuCwp,
    [0x4C] = (const void *)biosGpuSync,
    [0x4D] = (const void *)biosGpuSync,
    [0x4E] = (const void *)biosGpuSync,
    [0x4F] = (const void *)biosNoop,          /* SystemErrorBootOrDiskFailure */
    [0x50] = (const void *)biosBootstrap,
    [0x51] = (const void *)biosLoadExec,
    [0x54] = (const void *)biosCdInit,
    [0x55] = (const void *)biosCdInit,        /* _bu_init */
    [0x56] = (const void *)biosCdRemove,
    [0x70] = (const void *)biosBuInit,
    [0x71] = (const void *)biosCdInit,
    [0x72] = (const void *)biosCdRemove,
    [0x78] = (const void *)biosCdAsyncSeekL,
    [0x7C] = (const void *)biosCdAsyncGetStatus,
    [0x7E] = (const void *)biosCdAsyncReadSector,
    [0x81] = (const void *)biosCdAsyncSetMode,
    [0x95] = (const void *)biosCdInit,        /* CdInitSubFunc */
    [0x96] = (const void *)biosCdRegisterDevice,
    [0x97] = (const void *)biosCardRegisterDevice,
    [0x99] = (const void *)biosAddNullTty,
    [0x9E] = (const void *)biosSetMemSize,
    [0x9F] = (const void *)biosSetMemSize,
    [0xA0] = (const void *)biosWarmBoot,
    [0xA1] = (const void *)biosSystemError,
    [0xA2] = (const void *)biosReturnOne,     /* EnqueueCdIntr */
    [0xA3] = (const void *)biosReturnOne,     /* DequeueCdIntr */
    [0xA4] = (const void *)biosCdGetLbn,
    [0xA5] = (const void *)biosCdReadSector,
    [0xA6] = (const void *)biosCdGetStatus,
    [0xA7] = (const void *)biosBuInit,
    [0xA8] = (const void *)biosCdStop,
    [0xA9] = (const void *)biosCdReset,
    [0xAA] = (const void *)biosCdSetSession,
    [0xAB] = (const void *)biosCardInfo,
    [0xAC] = (const void *)biosCardLoad,
    [0xAD] = (const void *)biosCardAuto,
    [0xB4] = (const void *)biosGetSystemInfo,
};

static const void *const g_b0Table[0x5E] = {
    [0x00] = (const void *)biosMalloc,        /* alloc_kernel_memory */
    [0x01] = (const void *)biosFree,          /* free_kernel_memory */
    [0x02] = (const void *)biosInitRCnt,
    [0x03] = (const void *)biosGetRCnt,
    [0x04] = (const void *)biosStartRCnt,
    [0x05] = (const void *)biosStopRCnt,
    [0x06] = (const void *)biosResetRCnt,
    [0x07] = (const void *)biosDeliverEvent,
    [0x08] = (const void *)biosOpenEvent,
    [0x09] = (const void *)biosCloseEvent,
    [0x0A] = (const void *)biosWaitEvent,
    [0x0B] = (const void *)biosTestEvent,
    [0x0C] = (const void *)biosEnableEvent,
    [0x0D] = (const void *)biosDisableEvent,
    [0x0E] = (const void *)biosOpenThread,
    [0x0F] = (const void *)biosCloseThread,
    [0x10] = (const void *)biosChangeThread,
    [0x12] = (const void *)biosInitPad,
    [0x13] = (const void *)biosStartPad,
    [0x14] = (const void *)biosStopPad,
    [0x15] = (const void *)biosPadInitDirect,
    [0x16] = (const void *)biosPadStartCom,
    [0x17] = (const void *)biosReturnFromException,
    [0x18] = (const void *)biosSetDefaultExitFromException,
    [0x19] = (const void *)biosSetCustomExitFromException,
    [0x20] = (const void *)biosUnDeliverEvent,
    [0x32] = (const void *)biosFileOpen,
    [0x33] = (const void *)biosFileSeek,
    [0x34] = (const void *)biosFileRead,
    [0x35] = (const void *)biosFileWrite,
    [0x36] = (const void *)biosFileClose,
    [0x37] = (const void *)biosUnimplemented, /* FileIoctl */
    [0x38] = (const void *)biosWarmBoot,      /* exit */
    [0x39] = (const void *)biosUnimplemented, /* FileGetDeviceFlag */
    [0x3A] = (const void *)biosFileGetc,
    [0x3B] = (const void *)biosFilePutc,
    [0x3C] = (const void *)biosGetchar,
    [0x3D] = (const void *)biosPutchar,
    [0x3E] = (const void *)biosGets,
    [0x3F] = (const void *)biosPuts,
    [0x40] = (const void *)biosCdGetLbn,
    [0x41] = (const void *)biosFileFormat,
    [0x42] = (const void *)biosFirstFile,
    [0x43] = (const void *)biosNextFile,
    [0x44] = (const void *)biosFileRename,
    [0x45] = (const void *)biosFileDelete,
    [0x46] = (const void *)biosUnimplemented, /* FileUndelete */
    [0x47] = (const void *)biosAddDevice,
    [0x48] = (const void *)biosDelDevice,
    [0x49] = (const void *)biosPrintDevices,
    [0x4A] = (const void *)biosInitCard,
    [0x4B] = (const void *)biosStartCard,
    [0x4C] = (const void *)biosStopCard,
    [0x4D] = (const void *)biosBuInit,
    [0x4E] = (const void *)biosCardWrite,
    [0x4F] = (const void *)biosCardRead,
    [0x50] = (const void *)biosNewCard,
    [0x51] = (const void *)biosKrom2RawAdd,
    [0x52] = (const void *)biosNoop,
    [0x53] = (const void *)biosKrom2Offset,
    [0x54] = (const void *)biosGetLastError,
    [0x55] = (const void *)biosGetLastFileError,
    [0x56] = (const void *)biosGetC0Table,
    [0x57] = (const void *)biosGetB0Table,
    [0x58] = (const void *)biosCardStatus,
    [0x5B] = (const void *)biosChangeClearPad,
    [0x5C] = (const void *)biosCardStatus,
    [0x5D] = (const void *)biosCardClear,
};

static const void *const g_c0Table[0x1D] = {
    [0x00] = (const void *)biosReturnOne,     /* EnqueueTimerAndVblankIrqs */
    [0x01] = (const void *)biosReturnOne,     /* EnqueueSyscallHandler */
    [0x02] = (const void *)biosSysEnqIntRP,
    [0x03] = (const void *)biosSysDeqIntRP,
    [0x04] = (const void *)biosUnimplemented, /* get_free_EvCB_slot */
    [0x05] = (const void *)biosUnimplemented, /* get_free_TCB_slot */
    [0x06] = (const void *)biosNoop,          /* ExceptionHandler */
    [0x07] = (const void *)biosInstallVectors,
    [0x08] = (const void *)biosSysInitMemory,
    [0x09] = (const void *)biosNoop,          /* SysInitKMem */
    [0x0A] = (const void *)biosChangeClearRCnt,
    [0x0C] = (const void *)biosInitDevices,
    [0x0D] = (const void *)biosNoop,          /* FlushStdInOutPut */
    [0x12] = (const void *)biosInstallVectors,
    [0x13] = (const void *)biosHookEntryInt,
    [0x14] = (const void *)biosResetEntryInt,
    [0x15] = (const void *)biosNoop,          /* _cdevinput */
    [0x16] = (const void *)biosNoop,          /* _cdevscan */
    [0x17] = (const void *)biosUnimplemented, /* _circgetc */
    [0x18] = (const void *)biosUnimplemented, /* _circputc */
    [0x19] = (const void *)biosNoop,          /* _ioabort */
    [0x1A] = (const void *)biosSetConf,
    [0x1B] = (const void *)biosGetConf,
    [0x1C] = (const void *)biosGetA0Table,
};

void biosPopulateSyscallTables(void)
{
    biosInstallTable(PSX_RAM_A0_TABLE, g_a0Table, (u32)(sizeof(g_a0Table) / sizeof(g_a0Table[0])));
    biosInstallTable(PSX_RAM_B0_TABLE, g_b0Table, (u32)(sizeof(g_b0Table) / sizeof(g_b0Table[0])));
    biosInstallTable(PSX_RAM_C0_TABLE, g_c0Table, (u32)(sizeof(g_c0Table) / sizeof(g_c0Table[0])));
}
