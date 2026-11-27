/*
 * PlayStation 1 BIOS event control blocks (EvCB), thread control blocks (TCB),
 * and hardware root counter (timer 0..2) services.
 */

#include "bios_internal.h"

u32 biosOpenEvent(u32 desc, s32 spec, s32 mode, void (*handler)(void))
{
    u32 i;

    for (i = 0; i < PSX_BIOS->evcbCount; i++) {
        PsxEvcb *ev = &PSX_BIOS->evcbs[i];
        if (ev->status == EV_STATUS_FREE) {
            ev->desc = desc;
            ev->spec = (u32)spec;
            ev->mode = (u32)mode;
            ev->handler = handler;
            ev->status = EV_STATUS_DISABLED;
            return 0xF1000000U | i;
        }
    }
    return 0xFFFFFFFFU;
}

s32 biosCloseEvent(u32 eventId)
{
    u32 idx = eventId & 0xFFFFU;

    if (idx >= PSX_BIOS->evcbCount)
        return 0;
    PSX_BIOS->evcbs[idx].status = EV_STATUS_FREE;
    PSX_BIOS->evcbs[idx].handler = NULL;
    return 1;
}

s32 biosEnableEvent(u32 eventId)
{
    u32 idx = eventId & 0xFFFFU;

    if (idx >= PSX_BIOS->evcbCount)
        return 0;
    if (PSX_BIOS->evcbs[idx].status != EV_STATUS_FREE)
        PSX_BIOS->evcbs[idx].status = EV_STATUS_ENABLED;
    return 1;
}

s32 biosDisableEvent(u32 eventId)
{
    u32 idx = eventId & 0xFFFFU;

    if (idx >= PSX_BIOS->evcbCount)
        return 0;
    if (PSX_BIOS->evcbs[idx].status != EV_STATUS_FREE)
        PSX_BIOS->evcbs[idx].status = EV_STATUS_DISABLED;
    return 1;
}

s32 biosTestEvent(u32 eventId)
{
    u32 idx = eventId & 0xFFFFU;

    if (idx >= PSX_BIOS->evcbCount)
        return 0;
    if (PSX_BIOS->evcbs[idx].status == EV_STATUS_ALREADY) {
        PSX_BIOS->evcbs[idx].status = EV_STATUS_ENABLED;
        return 1;
    }
    return 0;
}

s32 biosWaitEvent(u32 eventId)
{
    u32 idx = eventId & 0xFFFFU;

    if (idx >= PSX_BIOS->evcbCount)
        return 0;
    if (PSX_BIOS->evcbs[idx].status == EV_STATUS_DISABLED)
        return 0;
    while (PSX_BIOS->evcbs[idx].status != EV_STATUS_ALREADY)
        ;
    PSX_BIOS->evcbs[idx].status = EV_STATUS_ENABLED;
    return 1;
}

void biosDeliverEvent(u32 desc, s32 spec)
{
    u32 i;

    for (i = 0; i < PSX_BIOS->evcbCount; i++) {
        PsxEvcb *ev = &PSX_BIOS->evcbs[i];
        if (ev->status == EV_STATUS_ENABLED && ev->desc == desc && ev->spec == (u32)spec) {
            if (ev->mode == EV_MODE_CALLBACK && ev->handler != NULL) {
                ev->handler();
            } else {
                ev->status = EV_STATUS_ALREADY;
            }
        }
    }
}

void biosUnDeliverEvent(u32 desc, s32 spec)
{
    u32 i;

    for (i = 0; i < PSX_BIOS->evcbCount; i++) {
        PsxEvcb *ev = &PSX_BIOS->evcbs[i];
        if (ev->status == EV_STATUS_ALREADY && ev->desc == desc && ev->spec == (u32)spec)
            ev->status = EV_STATUS_ENABLED;
    }
}

u32 biosOpenThread(u32 pc, u32 sp, u32 gp)
{
    u32 i;

    for (i = 0; i < PSX_BIOS->tcbCount; i++) {
        PsxTcb *tcb = &PSX_BIOS->tcbs[i];
        if (tcb->status == TCB_STATUS_FREE) {
            biosMemset(tcb, 0, sizeof(*tcb));
            tcb->status = TCB_STATUS_ALLOC;
            tcb->epc = pc;
            tcb->regs[28] = gp;
            tcb->regs[29] = sp;
            tcb->regs[30] = sp;
            tcb->sr = 0x00000404U;
            return 0xFF000000U | i;
        }
    }
    return 0xFFFFFFFFU;
}

s32 biosCloseThread(u32 thId)
{
    u32 idx = thId & 0xFFFFU;

    if (idx >= PSX_BIOS->tcbCount)
        return 0;
    PSX_BIOS->tcbs[idx].status = TCB_STATUS_FREE;
    return 1;
}

s32 biosChangeThread(u32 thId)
{
    u32 idx = thId & 0xFFFFU;

    if (idx >= PSX_BIOS->tcbCount || PSX_BIOS->tcbs[idx].status != TCB_STATUS_ALLOC)
        return 0;
    PSX_BIOS->pcb.currentTcb = &PSX_BIOS->tcbs[idx];
    return 1;
}

s32 biosInitRCnt(u32 spec, u16 target, u32 mode)
{
    u32 ch = spec & 3U;
    u32 hwMode = 0x0008U;

    if (ch >= 3U)
        return 0;
    PSX_IO16(REG_TMR_MODE(ch)) = 0U;
    PSX_IO16(REG_TMR_TARGET(ch)) = target;
    if ((mode & 0x1000U) != 0U)
        hwMode |= 0x0010U;
    if ((mode & 0x0100U) != 0U)
        hwMode |= 0x0100U;
    if ((mode & 0x0001U) != 0U)
        hwMode |= 0x0001U;
    PSX_IO16(REG_TMR_MODE(ch)) = (u16)hwMode;
    PSX_IO16(REG_TMR_VAL(ch)) = 0U;
    return 1;
}

s32 biosGetRCnt(u32 spec)
{
    u32 ch = spec & 3U;

    if (ch >= 3U)
        return 0;
    return (s32)(u32)PSX_IO16(REG_TMR_VAL(ch));
}

s32 biosResetRCnt(u32 spec)
{
    u32 ch = spec & 3U;

    if (ch >= 3U)
        return 0;
    PSX_IO16(REG_TMR_VAL(ch)) = 0U;
    return 1;
}

s32 biosStartRCnt(u32 spec)
{
    u32 ch = spec & 3U;

    if (ch == 3U) {
        PSX_IO32(REG_I_MASK) |= (1U << PSX_IRQ_VBLANK);
        return 1;
    }
    PSX_IO32(REG_I_MASK) |= (1U << (PSX_IRQ_TMR0 + ch));
    return 1;
}

s32 biosStopRCnt(u32 spec)
{
    u32 ch = spec & 3U;

    if (ch == 3U) {
        PSX_IO32(REG_I_MASK) &= ~(1U << PSX_IRQ_VBLANK);
        return 1;
    }
    PSX_IO32(REG_I_MASK) &= ~(1U << (PSX_IRQ_TMR0 + ch));
    return 1;
}

s32 biosChangeClearRCnt(u32 spec, u32 flag)
{
    u32 ch = spec & 3U;
    u32 irq = (ch == 3U) ? PSX_IRQ_VBLANK : (PSX_IRQ_TMR0 + ch);

    return biosChangeClearInt(irq, (s32)flag);
}
