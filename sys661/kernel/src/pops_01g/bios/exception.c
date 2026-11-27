/*
 * PlayStation 1 BIOS exception dispatcher, SYSCALL 1/2/3 critical sections,
 * and hardware IRQ0..IRQ10 priority chain / event delivery.
 */

#include "bios_internal.h"

static void handleHardwareIrqs(void)
{
    u32 pending = PSX_IO32(REG_I_STAT) & PSX_IO32(REG_I_MASK);
    u32 prio;

    /* Walk registered SysEnqIntRP handlers in priority order 0..3 */
    for (prio = 0; prio < 4U; prio++) {
        PsxIntRpNode *node = PSX_BIOS->intRpHeads[prio];
        while (node != NULL) {
            if (node->verifier != NULL && node->verifier() != 0) {
                if (node->handler != NULL)
                    node->handler();
            }
            node = node->next;
        }
    }

    if (pending == 0U)
        return;

    /* VBlank (IRQ0) */
    if ((pending & (1U << PSX_IRQ_VBLANK)) != 0U) {
        if (PSX_BIOS->padRunning != 0U)
            biosPadPollVblank();
        if (PSX_BIOS->clearIntFlags[PSX_IRQ_VBLANK] == 0U)
            PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_VBLANK);
        biosDeliverEvent(EV_CLASS_VBLANK, EV_SPEC_INTR);
        biosDeliverEvent(EV_CLASS_RCNT_BASE + 3U, EV_SPEC_COUNTER);
    }

    /* GPU (IRQ1) */
    if ((pending & (1U << PSX_IRQ_GPU)) != 0U) {
        if (PSX_BIOS->clearIntFlags[PSX_IRQ_GPU] == 0U)
            PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_GPU);
        biosDeliverEvent(EV_CLASS_GPU, EV_SPEC_INTR);
    }

    /* CD-ROM (IRQ2) */
    if ((pending & (1U << PSX_IRQ_CDROM)) != 0U) {
        biosCdHandleIrq();
        if (PSX_BIOS->clearIntFlags[PSX_IRQ_CDROM] == 0U)
            PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_CDROM);
    }

    /* DMA (IRQ3) */
    if ((pending & (1U << PSX_IRQ_DMA)) != 0U) {
        if (PSX_BIOS->clearIntFlags[PSX_IRQ_DMA] == 0U)
            PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_DMA);
        biosDeliverEvent(EV_CLASS_DMA, EV_SPEC_INTR);
    }

    /* Root Counters 0..2 (IRQ4..IRQ6) */
    if ((pending & (1U << PSX_IRQ_TMR0)) != 0U) {
        if (PSX_BIOS->clearIntFlags[PSX_IRQ_TMR0] == 0U)
            PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_TMR0);
        biosDeliverEvent(EV_CLASS_RCNT_BASE + 0U, EV_SPEC_COUNTER);
    }
    if ((pending & (1U << PSX_IRQ_TMR1)) != 0U) {
        if (PSX_BIOS->clearIntFlags[PSX_IRQ_TMR1] == 0U)
            PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_TMR1);
        biosDeliverEvent(EV_CLASS_RCNT_BASE + 1U, EV_SPEC_COUNTER);
    }
    if ((pending & (1U << PSX_IRQ_TMR2)) != 0U) {
        if (PSX_BIOS->clearIntFlags[PSX_IRQ_TMR2] == 0U)
            PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_TMR2);
        biosDeliverEvent(EV_CLASS_RCNT_BASE + 2U, EV_SPEC_COUNTER);
    }

    /* SIO0 Pad / Memory Card (IRQ7) */
    if ((pending & (1U << PSX_IRQ_SIO0)) != 0U) {
        if (PSX_BIOS->padClearFlag == 0U)
            PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_SIO0);
        biosDeliverEvent(EV_CLASS_PAD, EV_SPEC_INTR);
        biosDeliverEvent(EV_CLASS_CARD_HW, EV_SPEC_IOE);
    }

    /* SIO1 / SPU / PIO (IRQ8..IRQ10) */
    if ((pending & (1U << PSX_IRQ_SIO1)) != 0U) {
        PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_SIO1);
        biosDeliverEvent(EV_CLASS_SIO, EV_SPEC_INTR);
    }
    if ((pending & (1U << PSX_IRQ_SPU)) != 0U) {
        PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_SPU);
        biosDeliverEvent(EV_CLASS_SPU, EV_SPEC_INTR);
    }
    if ((pending & (1U << PSX_IRQ_PIO)) != 0U) {
        PSX_IO32(REG_I_STAT) = ~(1U << PSX_IRQ_PIO);
        biosDeliverEvent(EV_CLASS_PIO, EV_SPEC_INTR);
    }

    if (PSX_BIOS->entryIntHook != NULL)
        PSX_BIOS->entryIntHook();
}

void biosExceptionDispatch(PsxTcb *tcb)
{
    u32 excCode = (tcb->cause >> 2) & 0x1FU;

    if (excCode == 0U) {
        /* External hardware interrupt */
        handleHardwareIrqs();
        return;
    }

    if (excCode == 8U) {
        /* SYSCALL: advance EPC past syscall instruction */
        u32 sysNum = tcb->regs[4]; /* syscall number in $4 */
        tcb->epc += 4U;

        if (sysNum == 1U) {
            /* EnterCriticalSection: return 1 if interrupts were previously enabled */
            u32 prevEnabled = ((tcb->sr & 0x0404U) == 0x0404U) ? 1U : 0U;
            tcb->sr &= ~0x0404U;
            tcb->regs[2] = prevEnabled;
        } else if (sysNum == 2U) {
            /* ExitCriticalSection: re-enable interrupts */
            tcb->sr |= 0x0404U;
        } else if (sysNum == 3U) {
            /* ChangeThreadSubFunction: swap current TCB pointer */
            if (tcb->regs[5] != 0U)
                PSX_BIOS->pcb.currentTcb = (PsxTcb *)tcb->regs[5];
        }
        return;
    }

    /* Other exceptions (break, address error, etc.): skip faulting instruction */
    tcb->epc += 4U;
}

s32 biosSysEnqIntRP(s32 priority, PsxIntRpNode *node)
{
    if (priority < 0 || priority >= 4 || node == NULL)
        return 0;
    node->next = PSX_BIOS->intRpHeads[priority];
    PSX_BIOS->intRpHeads[priority] = node;
    return 1;
}

s32 biosSysDeqIntRP(s32 priority, PsxIntRpNode *node)
{
    PsxIntRpNode **link;

    if (priority < 0 || priority >= 4 || node == NULL)
        return 0;
    link = &PSX_BIOS->intRpHeads[priority];
    while (*link != NULL) {
        if (*link == node) {
            *link = node->next;
            node->next = NULL;
            return 1;
        }
        link = &(*link)->next;
    }
    return 0;
}

s32 biosChangeClearInt(u32 irq, s32 flag)
{
    u32 oldVal;

    if (irq >= 16U)
        return 0;
    oldVal = PSX_BIOS->clearIntFlags[irq];
    PSX_BIOS->clearIntFlags[irq] = (flag != 0) ? 1U : 0U;
    return (s32)oldVal;
}

s32 biosChangeClearPad(s32 flag)
{
    u32 oldVal = PSX_BIOS->padClearFlag;
    PSX_BIOS->padClearFlag = (flag != 0) ? 1U : 0U;
    return (s32)oldVal;
}

void biosHookEntryInt(void *jmpBuf)
{
    PSX_BIOS->entryIntHook = (void (*)(void))jmpBuf;
}

void biosResetEntryInt(void)
{
    PSX_BIOS->entryIntHook = NULL;
}

void biosSetCustomExitFromException(void *handler)
{
    PSX_BIOS->customExitHook = (void (*)(void))handler;
}

void biosSetDefaultExitFromException(void)
{
    PSX_BIOS->customExitHook = NULL;
}
