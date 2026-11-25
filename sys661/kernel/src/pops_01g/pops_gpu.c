/*
 * PS1 GPU to PSP GE display list translation and VBlank synchronization.
 */
#include "pops_internal.h"

void popsGpuInit(void)
{
    sceKernelMemset(&g_pops.gpu, 0, sizeof(g_pops.gpu));
    g_pops.gpu.statusReg = 0x14802000U;
    g_pops.gpu.geListId = -1;
    sceGeEdramSetAddrTranslation(0x400);
}

void popsGpuWriteCommand(u32 cmdWord)
{
    if (g_pops.gpu.geCmdPos >= POPS_GE_LIST_WORDS - 2) {
        popsGpuFlush();
    }
    g_pops.gpu.geCmdBuf[g_pops.gpu.geCmdPos++] = cmdWord;
}

void popsGpuFlush(void)
{
    if (g_pops.gpu.geCmdPos == 0) {
        return;
    }
    /* Append GE FINISH + END commands */
    g_pops.gpu.geCmdBuf[g_pops.gpu.geCmdPos++] = 0x0F000000U;
    g_pops.gpu.geCmdBuf[g_pops.gpu.geCmdPos++] = 0x0C000000U;
    sceKernelDcacheWritebackRange(g_pops.gpu.geCmdBuf, g_pops.gpu.geCmdPos * sizeof(u32));

    g_pops.gpu.geListId = sceGeListEnQueue(g_pops.gpu.geCmdBuf, NULL, -1, NULL);
    if (g_pops.gpu.geListId >= 0) {
        sceGeListSync(g_pops.gpu.geListId, 0);
    }
    sceGeDrawSync(0);
    g_pops.gpu.geCmdPos = 0;
}

void popsGpuVblank(void)
{
    popsGpuFlush();
    g_pops.gpu.frameCount++;
    g_pops.intr.vblankCount = sceDisplayGetVcount();
}
