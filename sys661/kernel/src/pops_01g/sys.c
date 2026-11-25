/*
 * POPS system initialization, main thread, and power/MS callbacks.
 *
 * Corresponds to Sony's sys.c ($Id: sys.c,v 1.140 ...) in pops_01g.prx.
 */
#include "pops_internal.h"

u32 module_start_thread_parameter[4] = { 3, 0x20, 0x400, 0 };

PopsState g_pops;

static int popsPowerCallback(int unknown, int powerInfo, void *common)
{
    (void)unknown;
    (void)common;
    if ((powerInfo & 0x00010000) != 0) {
        g_pops.powerSuspended = 1;
    } else if ((powerInfo & 0x00040000) != 0) {
        g_pops.powerSuspended = 0;
    }
    return 0;
}

static int popsMsCallback(int unknown, int msEvent, void *common)
{
    (void)unknown;
    (void)common;
    g_pops.msInserted = (msEvent != 0) ? 1 : 0;
    return 0;
}

static int popsInitSubsystems(void)
{
    int btnAssign = 0;

    scePowerSetClockFrequency(222, 222, 111);
    sceImposeGetLanguageMode(&g_pops.language, &btnAssign);

    g_pops.syncSemaId = sceKernelCreateSema("popsSync", 0, 1, 1, NULL);
    if (g_pops.syncSemaId < 0) {
        return g_pops.syncSemaId;
    }

    /* Reset PS1 R3000A CPU and COP0 state */
    sceKernelMemset(&g_pops.cpu, 0, sizeof(g_pops.cpu));
    sceKernelMemset(&g_pops.cop0, 0, sizeof(g_pops.cop0));
    g_pops.cpu.pc = 0xBFC00000U;
    g_pops.cpu.nextPc = 0xBFC00004U;
    g_pops.cop0.prid = 0x00000002U;
    g_pops.cop0.status = 0x10900000U;

    popsIntrInit();
    popsGpuInit();
    popsSpuInit();
    popsCdromInit();
    popsPadInit();
    popsVmcInit();

    g_pops.initialized = 1;
    return 0;
}

static int popsMainThread(SceSize args, void *argp)
{
    int msCb;
    int ret;

    (void)args;
    (void)argp;

    scePaf_A138A376(g_pops.versionBanner, "branches/pops-for-vita-350/pops/src(r%d)", POPS_SVN_REVISION);

    g_pops.powerCbId = sceKernelCreateCallback("powerCB", popsPowerCallback, NULL);
    if (g_pops.powerCbId >= 0) {
        scePowerRegisterCallback(-1, g_pops.powerCbId);
    }

    msCb = sceKernelCreateCallback("msCB", popsMsCallback, NULL);
    g_pops.msCbId = msCb;
    if (msCb >= 0) {
        sceIoDevctl("fatms0:", POPS_MS_CALLBACK_DEVCTL, &msCb, sizeof(msCb), NULL, 0);
    }

    ret = popsInitSubsystems();
    if (ret < 0) {
        int fatalErr = (ret == POPS_ERR_IO_FAILURE) ? POPS_ERR_FATAL_IO : POPS_ERR_FATAL_GENERIC;
        scePopsMan_0090B2C8(fatalErr);
        return ret;
    }

    g_pops.running = 1;
    while (g_pops.running) {
        if (!g_pops.powerSuspended) {
            popsPadPoll();
            popsGpuVblank();
            popsIntrRaise(PS1_IRQ_VBLANK);
            popsIntrDispatch();
        }
        sceDisplayWaitVblankStartCB();
    }

    popsCdromCloseDisc();
    return 0;
}

int module_start(SceSize args, void *argp)
{
    int thid;
    int ret;

    (void)args;
    (void)argp;

    sceKernelSetCompiledSdkVersion660(POPS_SDK_VERSION);
    sceKernelSetCompilerVersion(POPS_COMPILER_VERSION);
    sceDisplaySetHoldMode(1);

    thid = sceKernelCreateThread("popsmain", popsMainThread, POPS_MAIN_THREAD_PRIO,
                                 POPS_MAIN_THREAD_STACK, POPS_MAIN_THREAD_ATTR, NULL);
    g_pops.mainThreadId = thid;
    ret = sceKernelStartThread(thid, 0, NULL);
    if (ret < 0) {
        int fatalErr = (ret == POPS_ERR_IO_FAILURE) ? POPS_ERR_FATAL_IO : POPS_ERR_FATAL_GENERIC;
        scePopsMan_0090B2C8(fatalErr);
        popsMainThread(0, NULL);
    }
    return 0;
}
