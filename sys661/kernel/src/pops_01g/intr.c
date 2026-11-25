/*
 * PS1 interrupt controller and alarm/event dispatch.
 *
 * Corresponds to Sony's intr.c ($Id: intr.c,v 1.75 ...) in pops_01g.prx.
 */
#include "pops_internal.h"

void popsIntrInit(void)
{
    sceKernelMemset(&g_pops.intr, 0, sizeof(g_pops.intr));
    g_pops.intr.alarmId = -1;
    g_pops.intr.eventFlagId = sceKernelCreateEventFlag("popsIntrEvf", 0, 0, NULL);
}

void popsIntrRaise(u32 irqBit)
{
    g_pops.intr.iStat |= irqBit;
    if ((g_pops.intr.iStat & g_pops.intr.iMask) != 0) {
        g_pops.intr.pendingIrq = 1;
        g_pops.cop0.cause |= 0x00000400U;
        if (g_pops.intr.eventFlagId >= 0) {
            sceKernelSetEventFlag(g_pops.intr.eventFlagId, irqBit);
        }
    }
}

void popsIntrAck(u32 irqMask)
{
    g_pops.intr.iStat &= irqMask;
    if ((g_pops.intr.iStat & g_pops.intr.iMask) == 0) {
        g_pops.intr.pendingIrq = 0;
        g_pops.cop0.cause &= ~0x00000400U;
    }
}

void popsIntrSetMask(u32 mask)
{
    g_pops.intr.iMask = mask & 0x07FFU;
    if ((g_pops.intr.iStat & g_pops.intr.iMask) != 0) {
        g_pops.intr.pendingIrq = 1;
        g_pops.cop0.cause |= 0x00000400U;
    } else {
        g_pops.intr.pendingIrq = 0;
        g_pops.cop0.cause &= ~0x00000400U;
    }
}

void popsIntrDispatch(void)
{
    if (!g_pops.intr.pendingIrq) {
        return;
    }
    /* Check COP0 IEc bit and IM2 mask bit before taking PS1 IRQ exception */
    if ((g_pops.cop0.status & 0x00000401U) == 0x00000401U) {
        g_pops.cop0.epc = g_pops.cpu.pc;
        g_pops.cop0.status = (g_pops.cop0.status & ~0x3FU) | ((g_pops.cop0.status & 0x0FU) << 2);
        g_pops.cop0.cause &= ~0x0000007CU;
        g_pops.cpu.pc = ((g_pops.cop0.status & 0x00400000U) != 0) ? 0xBFC00180U : 0x80000080U;
        g_pops.cpu.nextPc = g_pops.cpu.pc + 4U;
    }
}
