/*
 * PS1 SPU (ME Audio), CD-ROM, Controller (sceCtrl), and Virtual Memory Card
 * (pspvmc / sceUtilitySavedata) bridges.
 */
#include "pops_internal.h"

void popsSpuInit(void)
{
    sceKernelMemset(&g_pops.spu, 0, sizeof(g_pops.spu));
    g_pops.spu.mainVolLeft = 0x3FFF;
    g_pops.spu.mainVolRight = 0x3FFF;
    if (sceMeAudio_34F2548F() >= 0) {
        g_pops.spu.meAudioReady = 1;
    }
}

void popsSpuWriteReg(u32 offset, u16 val)
{
    if (offset < 0x180U) {
        u32 voiceIdx = offset >> 4;
        u32 regIdx = (offset & 0x0FU) >> 1;
        if (voiceIdx < PS1_NUM_SPU_VOICES) {
            PopsSpuVoice *voice = &g_pops.spu.voices[voiceIdx];
            switch (regIdx) {
            case 0: voice->volLeft = val; break;
            case 1: voice->volRight = val; break;
            case 2: voice->pitch = val; break;
            case 3: voice->startAddr = val; break;
            default: break;
            }
        }
        return;
    }

    switch (offset) {
    case 0x180U: g_pops.spu.mainVolLeft = val; break;
    case 0x182U: g_pops.spu.mainVolRight = val; break;
    case 0x184U: g_pops.spu.reverbDepthLeft = val; break;
    case 0x186U: g_pops.spu.reverbDepthRight = val; break;
    case 0x1AAU: g_pops.spu.controlReg = val; break;
    default: break;
    }
}

void popsCdromInit(void)
{
    sceKernelMemset(&g_pops.cdrom, 0, sizeof(g_pops.cdrom));
    g_pops.cdrom.discFd = -1;
}

int popsCdromOpenDisc(const char *path)
{
    if (path == NULL) {
        return -1;
    }
    if (g_pops.cdrom.discFd >= 0) {
        sceIoClose(g_pops.cdrom.discFd);
    }
    g_pops.cdrom.discFd = sceIoOpen(path, 0x0001, 0);
    return g_pops.cdrom.discFd;
}

void popsCdromCloseDisc(void)
{
    if (g_pops.cdrom.discFd >= 0) {
        sceIoClose(g_pops.cdrom.discFd);
        g_pops.cdrom.discFd = -1;
    }
}

void popsPadInit(void)
{
    sceKernelMemset(&g_pops.pad, 0, sizeof(g_pops.pad));
    g_pops.pad.analogLx = 0x80;
    g_pops.pad.analogLy = 0x80;
    g_pops.pad.analogRx = 0x80;
    g_pops.pad.analogRy = 0x80;
    sceCtrlSetSamplingCycle(0);
    sceCtrlSetSamplingMode(1);
    sceCtrlSetIdleCancelThreshold(25, 25);
}

void popsPadPoll(void)
{
    PopsCtrlData ctrl;

    if (sceCtrlPeekBufferNegative(&ctrl, 1) > 0) {
        g_pops.pad.buttons = ctrl.buttons;
        g_pops.pad.analogLx = ctrl.lx;
        g_pops.pad.analogLy = ctrl.ly;
    }
}

void popsVmcInit(void)
{
    int slot;

    sceKernelMemset(g_pops.vmc, 0, sizeof(g_pops.vmc));
    for (slot = 0; slot < PS1_NUM_VMC_SLOTS; slot++) {
        if (pspvmc_0820B291(slot) >= 0) {
            g_pops.vmc[slot].mounted = 1;
            g_pops.vmc[slot].blockCount = 16;
        }
    }
}
