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

extern void exc_vec_0(void);
extern void exc_vec_1(void);
extern void exc_vec_2(void);
extern void exc_vec_3(void);
extern void exc_vec_4(void);
extern void exc_vec_5(void);
extern void exc_vec_6(void);
extern void exc_vec_7(void);
extern void exc_vec_8(void);
extern void exc_vec_9(void);

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

static void __attribute__((noipa)) bsod_dead_thread_handler(void);
static s32 __attribute__((noipa)) bsod_register_handlers(void);
static s32 __attribute__((noipa)) bsod_release_handlers(void);
void __attribute__((noipa)) bsod_crash_handler(void);

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

static void __attribute__((noipa)) bsod_dead_thread_handler(void)
{
    bsod_crash_handler();
}

static s32 __attribute__((noipa)) bsod_register_handlers(void)
{
    s32 state = sceKernelCpuSuspendIntr();
    sceKernelRegisterExceptionHandler(4, exc_vec_0);
    sceKernelRegisterExceptionHandler(5, exc_vec_1);
    sceKernelRegisterExceptionHandler(6, exc_vec_2);
    sceKernelRegisterExceptionHandler(7, exc_vec_3);
    sceKernelRegisterExceptionHandler(9, exc_vec_4);
    sceKernelRegisterExceptionHandler(10, exc_vec_5);
    sceKernelRegisterPriorityExceptionHandler(11, 3, exc_vec_6);
    sceKernelRegisterExceptionHandler(12, exc_vec_7);
    sceKernelRegisterExceptionHandler(15, exc_vec_8);
    sceKernelRegisterExceptionHandler(31, exc_vec_9);
    sceKernelRegisterDeadThreadHandler(bsod_dead_thread_handler);
    sceKernelCpuResumeIntr(state);
    return 0;
}

static s32 __attribute__((noipa)) bsod_release_handlers(void)
{
    s32 state = sceKernelCpuSuspendIntr();
    sceKernelReleaseExceptionHandler(4, exc_vec_0);
    sceKernelReleaseExceptionHandler(5, exc_vec_1);
    sceKernelReleaseExceptionHandler(6, exc_vec_2);
    sceKernelReleaseExceptionHandler(7, exc_vec_3);
    sceKernelReleaseExceptionHandler(9, exc_vec_4);
    sceKernelReleaseExceptionHandler(10, exc_vec_5);
    sceKernelReleaseExceptionHandler(11, exc_vec_6);
    sceKernelReleaseExceptionHandler(12, exc_vec_7);
    sceKernelReleaseExceptionHandler(15, exc_vec_8);
    sceKernelReleaseExceptionHandler(31, exc_vec_9);
    sceKernelRegisterDeadThreadHandler(NULL);
    sceKernelCpuResumeIntr(state);
    return 0;
}

void __attribute__((noipa)) bsod_crash_handler(void)
{
    volatile u32 dummy1, dummy2;

    sceKernelMemset((void *)0xABD48000, 0, 0x8000);
    *(volatile u32 *)0xABD48000 = 0x425;
    asm volatile("sync" ::: "memory");

    *(volatile u32 *)0xBD000004 = 0xF;
    do {
        dummy2 = *(volatile u32 *)0xBD000004;
    } while (dummy2 != 0);

    do {
        dummy2 = *(volatile u32 *)0xBD000000;
    } while (dummy2 != 0xF);

    u32 val = *(volatile u32 *)0xBC300050;
    *(volatile u32 *)0xBC300050 = val & ~0x200u;
    dummy1 = *(volatile u32 *)0xBC300050;
    *(volatile u32 *)0xBC300050 = val | 0x200u;
    dummy1 = *(volatile u32 *)0xBC300050;
    *(volatile u32 *)0xBC300038 |= 0x200u;

    for (;;) {
    }
}
