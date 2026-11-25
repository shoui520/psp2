#include <common_imp.h>
#include <exceptionman.h>
#include <interruptman.h>
#include <sysmem_kernel.h>
#include <threadman_kernel.h>

SCE_MODULE_INFO("sceBlueScreenOfDeath",
                SCE_MODULE_KERNEL | SCE_MODULE_ATTR_CANT_STOP | SCE_MODULE_ATTR_EXCLUSIVE_LOAD |
                    SCE_MODULE_ATTR_EXCLUSIVE_START,
                1, 8);
SCE_SDK_VERSION(0x06060110);

extern void excVecAddrLoad(void);
extern void excVecAddrStore(void);
extern void excVecBusFetch(void);
extern void excVecBusData(void);
extern void excVecBreak(void);
extern void excVecReservedInsn(void);
extern void excVecCopUnusable(void);
extern void excVecOverflow(void);
extern void excVecFpu(void);
extern void excVecNmi(void);

const char * const g_excNames[32] = {
    "External interrupt",
    "0x1",
    "0x2",
    "0x3",
    "Address error (load/fetch)",
    "Address error (store)",
    "Bus error (fetch)",
    "Bus error (load/store)",
    "Syscall",
    "Breakpoint",
    "Reserved instruction",
    "Coprocessor unusable",
    "Arithmetic overflow",
    "0xD",
    "0xE",
    "FPU exception",
    "0x10",
    "0x11",
    "0x12",
    "0x13",
    "0x14",
    "0x15",
    "0x16",
    "0x17",
    "0x18",
    "0x19",
    "0x1A",
    "0x1B",
    "0x1C",
    "0x1D",
    "0x1E",
    "NMI exception",
};

void *g_ctxPtr;
u8 g_stack[0x1FFC];
u32 g_stack_end[3];

extern s32 sceKernelRegisterDeadThreadHandler(void (*handler)(void));

static void __attribute__((noinline)) bsod_dead_thread_handler(void);
static s32 __attribute__((noinline)) bsod_register_handlers(void);
static s32 __attribute__((noinline)) bsod_release_handlers(void);
void __attribute__((noinline)) bsod_crash_handler(void);

s32 module_start(SceSize args, void *argp) __attribute__((alias("module_bootstart")));

s32 module_bootstart(SceSize args, void *argp)
{
    bsod_register_handlers();
    return 0;
}

s32 module_reboot_before(void)
{
    bsod_release_handlers();
    return 0;
}

static void __attribute__((noinline)) bsod_dead_thread_handler(void)
{
    bsod_crash_handler();
}

static s32 __attribute__((noinline)) bsod_register_handlers(void)
{
    s32 state = sceKernelCpuSuspendIntr();
    sceKernelRegisterExceptionHandler(4, excVecAddrLoad);
    sceKernelRegisterExceptionHandler(5, excVecAddrStore);
    sceKernelRegisterExceptionHandler(6, excVecBusFetch);
    sceKernelRegisterExceptionHandler(7, excVecBusData);
    sceKernelRegisterExceptionHandler(9, excVecBreak);
    sceKernelRegisterExceptionHandler(10, excVecReservedInsn);
    sceKernelRegisterPriorityExceptionHandler(11, 3, excVecCopUnusable);
    sceKernelRegisterExceptionHandler(12, excVecOverflow);
    sceKernelRegisterExceptionHandler(15, excVecFpu);
    sceKernelRegisterExceptionHandler(31, excVecNmi);
    sceKernelRegisterDeadThreadHandler(bsod_dead_thread_handler);
    sceKernelCpuResumeIntr(state);
    return 0;
}

static s32 __attribute__((noinline)) bsod_release_handlers(void)
{
    s32 state = sceKernelCpuSuspendIntr();
    sceKernelReleaseExceptionHandler(4, excVecAddrLoad);
    sceKernelReleaseExceptionHandler(5, excVecAddrStore);
    sceKernelReleaseExceptionHandler(6, excVecBusFetch);
    sceKernelReleaseExceptionHandler(7, excVecBusData);
    sceKernelReleaseExceptionHandler(9, excVecBreak);
    sceKernelReleaseExceptionHandler(10, excVecReservedInsn);
    sceKernelReleaseExceptionHandler(11, excVecCopUnusable);
    sceKernelReleaseExceptionHandler(12, excVecOverflow);
    sceKernelReleaseExceptionHandler(15, excVecFpu);
    sceKernelReleaseExceptionHandler(31, excVecNmi);
    sceKernelRegisterDeadThreadHandler(NULL);
    sceKernelCpuResumeIntr(state);
    return 0;
}

void __attribute__((noinline)) bsod_crash_handler(void)
{
    volatile u32 regReadback;
    volatile u32 kermitStatus;

    sceKernelMemset((void *)0xABD48000, 0, 0x8000);
    *(volatile u32 *)0xABD48000 = 0x425;
    asm volatile("sync" ::: "memory");

    *(volatile u32 *)0xBD000004 = 0xF;
    kermitStatus = *(volatile u32 *)0xBD000004;
    while (kermitStatus != 0) {
        kermitStatus = *(volatile u32 *)0xBD000004;
    }

    kermitStatus = *(volatile u32 *)0xBD000000;
    while (kermitStatus != 0xF) {
        kermitStatus = *(volatile u32 *)0xBD000000;
    }

    u32 val = *(volatile u32 *)0xBC300050;
    *(volatile u32 *)0xBC300050 = val & ~0x200u;
    regReadback = *(volatile u32 *)0xBC300050;
    *(volatile u32 *)0xBC300050 = val | 0x200u;
    regReadback = *(volatile u32 *)0xBC300050;
    *(volatile u32 *)0xBC300038 |= 0x200u;

    for (;;) {
    }
}
