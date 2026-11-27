#include "pops_internal.h"

/* PSP controller negative-logic sample packet (16 bytes) */
typedef struct PopsCtrlSample {
    u32 timeStamp;
    u32 buttons;
    u8 leftX;
    u8 leftY;
    u8 rightX;
    u8 rightY;
    u32 padReserved;
} PopsCtrlSample;

/* 0x008174: PS1 I_STAT / I_MASK register read with tight-poll cycle throttle */
u32 popsIrqReadReg(u32 addr, u32 width)
{
    s32 remaining = g_pops->cycleRemaining;
    u32 curCycle = g_pops->cycleTarget - (u32)remaining;
    u32 regOff = addr & 0xFFCU;
    s32 step = 4;

    (void)width;
    if ((remaining > 0) & ((s32)(curCycle - g_pops->irqThrottleLastCycle) < 40)) {
        step = __builtin_allegrex_min(
            __builtin_allegrex_min(2 * (s32)g_pops->irqThrottleActive, remaining), 80);
        g_pops->irqThrottleCount += (u32)step;
        curCycle += (u32)step;
        g_pops->cycleRemaining = remaining - step;
    }

    g_pops->irqThrottleActive = (u8)step;
    *(volatile u32 *)&g_pops->irqThrottleLastCycle = g_pops->irqThrottleCount;
    *(volatile u32 *)&g_pops->irqThrottleCount = curCycle;

    return ((const u32 *)(const void *)g_pops->hwIoLow)[regOff >> 2];
}

/* 0x0081e8: PS1 I_STAT / I_MASK register write */
void popsIrqWriteReg(u32 addr, u32 value, u32 width)
{
    u32 prevStat = g_pops->irqStatus;
    u32 prevMask = g_pops->irqMask;
    u32 regOff = addr & 0xCU;
    u32 *regPtr = (u32 *)(void *)&g_pops->hwIoLow[0x70U + regOff];
    u32 newActive;

    (void)width;
    if (regOff == 0U) {
        value &= prevStat;
    }
    *regPtr = value;

    newActive = (g_pops->irqStatus & g_pops->irqMask) != 0U;
    if (newActive != ((prevStat & prevMask) != 0U)) {
        u32 sr = g_pops->copZeroRegs[12];
        u32 newCause = (g_pops->copZeroRegs[13] & ~0x400U) | (newActive << 10);
        u32 gated = ((sr & 1U) != 0U) ? newCause : 0U;

        g_pops->copZeroRegs[13] = newCause;
        if ((gated & sr & 0xFF00U) != 0U) {
            g_pops->cycleTarget -= (u32)g_pops->cycleRemaining;
            g_pops->cycleRemaining = 0;
        }
    }
}

/* 0x008264: Notify PS1 root counter 1 (H-blank / V-blank sync gate) of V-blank transitions */
void popsTimersNotifyVblank(s32 inVblank)
{
    PopsRootCounter *tmr = &g_pops->timers[1];
    u32 mode = tmr->mode;
    u32 syncMode;
    s32 prevTarget;
    s32 freeze;

    if ((mode & 1U) == 0U) {
        return;
    }

    syncMode = (mode >> 1) & 3U;
    prevTarget = tmr->targetAndFreeze;
    freeze = !inVblank;

    if (syncMode == 1U) {
        if (!inVblank) {
            return;
        }
        tmr->baseCycle = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
    } else if (syncMode == 0U) {
        u32 curVal = tmr->baseCycle;
        if (prevTarget >= 0) {
            curVal = popsTimerUpdateValue(tmr);
        }
        if (!inVblank) {
            tmr->baseCycle = curVal;
        } else {
            tmr->baseCycle = (g_pops->cycleTarget - (u32)g_pops->cycleRemaining) -
                             (curVal << tmr->clock.shifts.divShift);
        }
    } else if (syncMode == 2U) {
        freeze = inVblank;
        if (inVblank) {
            tmr->baseCycle = 0U;
        } else {
            tmr->baseCycle = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
        }
    } else {
        if (((u32)!inVblank | ((u32)~prevTarget >> 31)) != 0U) {
            return;
        }
        tmr->baseCycle = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
    }

    tmr->targetAndFreeze = (prevTarget & 0x7FFFFFFF) | (freeze << 31);
    if (tmr->ev.next != NULL) {
        popsEventCancel(&tmr->ev);
    }
    if (!freeze) {
        popsTimerScheduleNext(tmr);
    }
}

/* 0x008378: Schedule the next target/overflow event for a PS1 root counter */
void popsTimerScheduleNext(PopsRootCounter *tmr)
{
    u32 curVal = popsTimerUpdateValue(tmr);
    u32 mode = tmr->mode;
    u32 limit = (u32)tmr->targetAndFreeze;
    u8 shift = tmr->clock.shifts.divShift;

    if ((mode & 0x18U) == 0U) {
        limit = 0x10000U;
    }

    if ((mode & 0x30U) != 0U) {
        popsEventSchedule(&tmr->ev, (s32)((limit - curVal) << shift));
    } else if ((limit & (limit - 1U)) != 0U) {
        u32 maxSpan = 0x80000000U >> shift;
        u32 aligned = maxSpan - (maxSpan % limit);
        popsEventSchedule(&tmr->ev, (s32)((aligned - curVal) << shift));
    }
}

/* 0x0083f4: PS1 root counter target/overflow event callback */
void popsTimerEventCallback(PopsEvent *ev)
{
    PopsRootCounter *tmr = (PopsRootCounter *)ev;
    u32 mode = tmr->mode;

    if ((mode & 0x30U) != 0U) {
        if ((mode & 0x80U) != 0U) {
            mode ^= 0x400U;
            *(volatile u32 *)&tmr->mode = mode;
            if ((mode & 0x400U) == 0U) {
                popsTimerScheduleNext(tmr);
                return;
            }
        }
        if ((mode & 0x40U) == 0U) {
            mode &= 0xFCFU;
        }
        tmr->mode = mode;
        popsIrqRaise(tmr->irqBit);
        if ((u32)tmr->targetAndFreeze >= 0x4001U && (g_pops->quirkFlags & 0x1000U) == 0U) {
            popsGpuThrottleSpeed();
        }
    }

    popsTimerScheduleNext(tmr);
}

/* 0x008490: Advance and wrap a PS1 root counter to current CPU cycle */
u32 popsTimerUpdateValue(PopsRootCounter *tmr)
{
    u8 shift = tmr->clock.shifts.divShift;
    u32 nowCycle = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
    u32 target = (u32)tmr->targetAndFreeze;
    u32 ticks = (nowCycle - tmr->baseCycle) >> shift;
    u32 val = ticks - ((ticks >> 1) >> tmr->clock.shifts.subShift);

    if (val >= target) {
        u32 mode = tmr->mode;
        u32 newMode = mode | 0x800U;

        if ((mode & 8U) != 0U) {
            val %= target;
        } else if ((val >> 16) != 0U) {
            newMode = mode | 0x1800U;
            val &= 0xFFFFU;
        }
        tmr->mode = newMode;
        tmr->baseCycle = nowCycle - (val << shift);
    }

    return val;
}

/* 0x008514: PS1 root counter MMIO read (0x1F801100..0x1F80112F) */
u32 popsTimerReadReg(u32 addr, u32 width)
{
    u32 idx = (addr >> 4) & 3U;
    PopsRootCounter *tmr = &g_pops->timers[idx];
    u32 result = tmr->baseCycle;

    if (tmr->targetAndFreeze >= 0) {
        result = popsTimerUpdateValue(tmr);
    }
    if (width == 1U) {
        result = (u32)(s32)(s16)result;
    }

    if ((addr & 0xFU) != 0U) {
        result = tmr->mode;
        tmr->mode = result & 0x3FFU;
    }

    return result;
}

/* 0x008594: PS1 root counter MMIO write */
void popsTimerWriteReg(u32 addr, u32 value, u32 width)
{
    u32 idx = (addr >> 4) & 3U;
    PopsRootCounter *tmr = &g_pops->timers[idx];
    u32 regOff = addr & 0xFU;
    u32 lowHalf = value & 0xFFFFU;

    (void)width;
    if (tmr->targetAndFreeze >= 0) {
        popsTimerUpdateValue(tmr);
    }

    if (regOff == 4U) {
        u32 prevMode = tmr->mode;
        u32 newMode = lowHalf & 0x3FFU;
        s32 curTarget;

        tmr->mode = (prevMode - (prevMode & 0x3FFU)) | newMode;
        if ((newMode & 1U) == 0U || (newMode & 6U) < 4U) {
            newMode |= 0x8000U;
            tmr->targetAndFreeze &= 0x1FFFF;
        }
        curTarget = tmr->targetAndFreeze;
        if (curTarget >= 0) {
            tmr->baseCycle = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
        }

        if ((prevMode & 0x3FFU) != newMode) {
            u16 shiftPair = 0x1F00U;
            if (idx == 0U && (newMode & 0x100U) != 0U) {
                shiftPair = 0x1F03U;
            }
            if (idx == 1U && (newMode & 0x100U) != 0U) {
                shiftPair = ((g_pops->quirkFlags & 0x02000000U) != 0U) ? 0x030BU : 0x1F0BU;
            }
            if (idx == 2U && (newMode & 0x200U) != 0U) {
                shiftPair = 0x1F03U;
            }
            tmr->clock.pair = shiftPair;

            if ((idx & tmr->mode & 1U) != 0U) {
                if ((tmr->mode & 6U) == 6U) {
                    tmr->targetAndFreeze = curTarget | (s32)0x80000000U;
                    tmr->baseCycle = 0U;
                } else {
                    tmr->targetAndFreeze = curTarget & 0x1FFFF;
                    tmr->baseCycle = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
                }
            }
            if (tmr->ev.next != NULL) {
                popsEventCancel(&tmr->ev);
            }
            if (tmr->targetAndFreeze >= 0) {
                popsTimerScheduleNext(tmr);
            }
        }
    } else if (regOff == 0U) {
        s32 curTarget = tmr->targetAndFreeze;
        tmr->baseCycle = (curTarget < 0) ? 0U : (g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
        if ((idx & tmr->mode & 1U) != 0U) {
            if ((tmr->mode & 6U) == 6U) {
                tmr->targetAndFreeze = curTarget | (s32)0x80000000U;
                tmr->baseCycle = 0U;
            } else {
                tmr->targetAndFreeze = curTarget & 0x1FFFF;
                tmr->baseCycle = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
            }
        }
        if (tmr->ev.next != NULL) {
            popsEventCancel(&tmr->ev);
        }
        if (tmr->targetAndFreeze >= 0) {
            popsTimerScheduleNext(tmr);
        }
    } else if (regOff == 8U) {
        u32 targetVal = (lowHalf == 0U) ? 0x10000U : lowHalf;
        if ((u32)tmr->targetAndFreeze != targetVal) {
            u16 *ioMirror = (u16 *)(void *)&g_pops->hwIoLow[0];
            ioMirror[(addr & 0xFFFU) >> 1] = (u16)targetVal;
            tmr->targetAndFreeze = (tmr->targetAndFreeze & (s32)0xC0000000U) |
                                   (s32)(targetVal & 0x3FFFFFFFU);
            if (tmr->ev.next != NULL) {
                popsEventCancel(&tmr->ev);
            }
            if (tmr->targetAndFreeze >= 0) {
                popsTimerScheduleNext(tmr);
            }
        }
    }
}

/* 0x0087ac: PS1 SIO0 byte exchange completion callback */
void popsSioTransferCallback(PopsSioChannel *chan)
{
    u32 byteIdx = chan->byteIndex;
    u32 txByte = chan->txByte;
    u32 portSel = (chan->ctrlReg & 0x2000U) != 0U;
    s32 resp;

    if (byteIdx == 0U) {
        u16 ackDelay = 67U;
        s32 (*handler)(u32, u32, u8) = popsPadExchangeByte;

        if (txByte != 0x01U) {
            if (txByte == 0x81U) {
                handler = popsMcExchangeByte;
                ackDelay = 270U;
            } else {
                handler = popsSioNullDevice;
            }
        }
        chan->deviceHandler = handler;
        chan->ackDelayCycles = ackDelay;
    }

    resp = chan->deviceHandler(byteIdx, portSel, (u8)txByte);
    chan->rxByte = (u8)resp;
    chan->statusReg |= 7U;

    if ((resp & 0x100) != 0) {
        chan->deviceHandler = popsSioNullDevice;
    } else {
        chan->ev.callback = popsSioIrqCallback;
        popsEventSchedule(&chan->ev, (s32)chan->ackDelayCycles);
        chan->byteIndex++;
    }
}

/* 0x008878: PS1 SIO0/SIO1 MMIO read (0x1F801040 / 0x1F801050) */
u32 popsSioReadReg(u32 addr, u32 width)
{
    PopsSioChannel *chan = &g_pops->sio[(addr >> 4) & 1U];
    u32 regOff = addr & 0xFU;

    if (regOff == 0U) {
        u8 rxVal;
        if (chan->ev.next != NULL) {
            u32 target = g_pops->cycleTarget;
            u32 due = chan->ev.dueCycle;
            s32 rem = (s32)(due - (target - (u32)g_pops->cycleRemaining));
            if (rem < 34) {
                if (rem > 0) {
                    g_pops->cycleRemaining = (s32)(target - due);
                    g_pops->deferredCycles = __builtin_allegrex_max(g_pops->deferredCycles - rem, 0);
                }
                popsEventCancel(&chan->ev);
                chan->ev.callback(&chan->ev);
            }
        }
        rxVal = chan->rxByte;
        chan->statusReg &= (u16)~2U;
        return (width != 0U) ? (u32)rxVal : (u32)(s32)(s8)rxVal;
    }

    if (regOff == 4U) {
        s32 rem = g_pops->cycleRemaining;
        u32 nowCycle = g_pops->cycleTarget - (u32)rem;

        if ((u32)(nowCycle - chan->prevPollCycle) < 80U && chan->ev.next != NULL) {
            if (rem > 0) {
                g_pops->cycleRemaining = 0;
                g_pops->deferredCycles = __builtin_allegrex_max(g_pops->deferredCycles - rem, 0);
            }
            popsEventCancel(&chan->ev);
            chan->ev.callback(&chan->ev);
        }
        chan->prevPollCycle = chan->lastPollCycle;
        chan->lastPollCycle = nowCycle;
        return chan->statusReg;
    }

    return 0U;
}

/* 0x0089b4: PS1 SIO0/SIO1 TX data write (0x1F801040 / 0x1F801050) */
void popsSioWriteData(u32 addr, u32 value, u32 width)
{
    PopsSioChannel *chan = &g_pops->sio[(addr >> 4) & 1U];
    (void)width;

    if ((addr & 0xFU) == 0U) {
        chan->txByte = (u8)value;
        chan->statusReg &= 0xFFFAU;
        if (chan->ev.next != NULL) {
            popsEventCancel(&chan->ev);
        }
        chan->ev.callback = (void (*)(PopsEvent *))chan->transferCallback;
        popsEventSchedule(&chan->ev, (s32)chan->byteCycles);
    }
}

/* 0x008a30: PS1 SIO0/SIO1 MODE/CTRL/BAUD register write */
void popsSioWriteCtrl(u32 addr, u32 value, u32 width)
{
    PopsSioChannel *chan = &g_pops->sio[(addr >> 4) & 1U];
    u16 *ioMirror = (u16 *)(void *)&g_pops->hwIoLow[0];
    u32 regOff = addr & 0xFU;
    u16 ctrlVal = (u16)value;

    (void)width;
    if (regOff == 10U) {
        if ((ctrlVal & 0x40U) != 0U) {
            chan->byteIndex = 0U;
            if ((addr & 0x10U) == 0U) {
                chan->statusReg = 5U;
            }
            ctrlVal = 0U;
            if (chan->ev.next != NULL) {
                popsEventCancel(&chan->ev);
            }
            chan->ctrlReg = 0U;
            ioMirror[(addr & 0xFFFU) >> 1] = 0U;
            return;
        }

        {
            u16 prevCtrl = chan->ctrlReg;
            u8 rising = (u8)((ctrlVal & ~prevCtrl) & 0xFFU);

            if ((addr & 0x10U) == 0U && (ctrlVal & 0x10U) != 0U) {
                ctrlVal = (u16)(ctrlVal - 16U);
                chan->statusReg &= 0xFDC7U;
            }
            if ((rising & 2U) != 0U) {
                s32 take = __builtin_allegrex_min(g_pops->deferredCycles, 100);
                g_pops->deferredCycles -= take;
                g_pops->cycleRemaining -= take;
                chan->byteIndex = 0U;
            }
            if (((ctrlVal ^ prevCtrl) & 0x2000U) != 0U && chan->byteIndex != 0U) {
                chan->byteIndex = 0U;
            }
            chan->ctrlReg = ctrlVal;
            ioMirror[(addr & 0xFFFU) >> 1] = ctrlVal;
            return;
        }
    }

    if (ioMirror[(addr & 0xFFFU) >> 1] != ctrlVal) {
        ioMirror[(addr & 0xFFFU) >> 1] = ctrlVal;
        popsSioUpdateBaud(chan, &ioMirror[(addr & 0xFF0U) >> 1]);
    }
}

/* 0x008b60: Recompute SIO byte transfer duration from MODE and BAUD registers */
void popsSioUpdateBaud(PopsSioChannel *chan, const u16 *hwRegs)
{
    u32 mode = hwRegs[4];
    u32 baud = hwRegs[7];
    s32 mulShift = (48 >> (mode & 3U)) & 7;
    u32 bitCount = ((mode >> 2) & 3U) + ((mode >> 4) & 1U) + 5U;
    s32 cycles = (s32)((baud * bitCount) << (u32)mulShift);

    if (chan->portBit == 1U) {
        s32 latShift = (s32)g_pops->sioIrqLatency;
        if (latShift >= 0) {
            cycles >>= (u32)latShift;
        }
    }
    chan->byteCycles = (u16)cycles;
}

/* 0x008bbc: Deliver SIO0 / SIO1 ACK interrupt */
void popsSioIrqCallback(PopsEvent *ev)
{
    PopsSioChannel *chan = (PopsSioChannel *)ev;
    if ((chan->ctrlReg & 0x1000U) != 0U) {
        popsIrqRaise((u32)chan->portBit << 7);
    }
}

/* 0x008bec: PlayStation DualShock / Digital Pad / Multitap SIO0 protocol handler */
s32 popsPadExchangeByte(u32 byteIdx, u32 portSel, u8 txByte)
{
    u32 slotIdx = portSel & 7U;
    PopsPadSlot *pad = &g_pops->pads[slotIdx];
    u8 cmdByte = txByte;

    if (portSel < 2U) {
        PopsMultitapState *tap = &g_pops->taps[portSel];
        if (tap->connectTimer != 0U) {
            return 0x1FF;
        }
        if (tap->enabled != 0U) {
            if (byteIdx == 0U) {
                if ((tap->tapFlags & 4U) != 0U) {
                    tap->tapFlags ^= 6U;
                }
            } else if ((tap->tapFlags & 2U) != 0U) {
                if (byteIdx == 1U) {
                    return (txByte == 0x42U) ? 0x80 : 0x180;
                }
                if (byteIdx == 2U) {
                    if (txByte == 0U) {
                        tap->tapFlags |= 4U;
                        return 0x15A;
                    }
                    return 0x5A;
                }
                if (byteIdx >= 0x23U) {
                    return 0x1FF;
                }
                {
                    u32 subOff = byteIdx - 3U;
                    u32 prevResp = tap->responseBuf[subOff];
                    u32 subPort = subOff >> 3;
                    if (portSel != 0U) {
                        subPort += (g_pops->taps[0].enabled != 0U) ? 2U : 1U;
                    }
                    tap->responseBuf[subOff] = (u8)popsPadExchangeByte((subOff & 7U) + 1U, subPort + 8U, txByte);
                    return (s32)prevResp;
                }
            } else {
                if (byteIdx == 1U) {
                    tap->lastCmd = txByte;
                }
                if (byteIdx == 2U && (txByte & 1U) != 0U && tap->lastCmd == 0x42U) {
                    tap->tapFlags |= 4U;
                }
            }
        }
    }

    if (portSel == 1U && g_pops->taps[0].enabled != 0U && g_pops->taps[1].enabled == 0U) {
        return 0x1FF;
    }

    if (byteIdx == 1U) {
        s32 pollButtons = 0;

        if (txByte == 0x42U) {
            if (pad->dualshockMode != 0U) {
                cmdByte = 0x41U;
            }
            pollButtons = 1;
        } else if (txByte == 0x43U) {
            if (pad->dualshockMode != 0U && pad->configMode != 0U) {
                pad->buttons = 0U;
                pad->rightStick = 0U;
            } else {
                pollButtons = 1;
            }
        } else {
            if (pad->configMode == 0U) {
                cmdByte = 0U;
            }
        }

        if (pollButtons) {
            u16 btnMask = 0xFFFFU;
            if (pad->pspPadIndex >= 0) {
                PopsCtrlSample samples[2];
                s32 wantCount = ((g_pops->quirkFlags & 0x00100000U) != 0U) ? 2 : 1;
                s32 got = sceCtrlPeekBufferNegative(samples, wantCount);
                if (got > 0) {
                    u32 btns = samples[0].buttons;
                    if (got == 2) {
                        btns &= samples[1].buttons;
                    }
                    if ((btns & 0x20000U) != 0U) {
                        btnMask = (u16)btns;
                        if (pad->analogEnabled != 0U) {
                            pad->rightStick = (u16)((u32)samples[0].rightX | ((u32)samples[0].rightY << 8));
                            pad->leftStick = (u16)((u32)samples[0].leftX | ((u32)samples[0].leftY << 8));
                        } else {
                            pad->rightStick = 0x8080U;
                            pad->leftStick = 0x8080U;
                        }
                    }
                }
            }

            if (pad->pspPadIndex >= 0) {
                volatile u32 *extPad = POPS_SHARED_MEM->padInputs[(u32)pad->pspPadIndex];
                u32 wordBtn = extPad[0];
                u32 wordSticks = extPad[1];
                btnMask &= (u16)wordBtn;
                pad->rightStick = (u16)(wordSticks >> 16);
                pad->leftStick = (u16)(wordSticks & 0xFFFFU);
            }
            pad->buttons = btnMask;
        }

        pad->activeCmd = cmdByte;
        if (pad->configMode != 0U) {
            pad->respLen = 6U;
            return 0xF3;
        }
        pad->respLen = (u8)(2U * (pad->modeId & 0xFU));
        return pad->modeId;
    }

    if (byteIdx == 0U) {
        return 0xFF;
    }
    if (byteIdx == 2U) {
        if (pad->dualshockMode != 0U && pad->statusByte != 0U) {
            return 0x00;
        }
        return 0x5A;
    }
    if (byteIdx == 3U) {
        pad->subCmdArg = txByte;
    }

    switch (pad->activeCmd) {
    case 'A': {
        u32 dataPos = byteIdx - 3U;
        if (dataPos < 32U) {
            if ((pad->rumbleEnableMask & (1U << dataPos)) != 0U) {
                if ((pad->rumbleSelectMask & (1U << dataPos)) != 0U) {
                    pad->motorLarge = txByte;
                } else {
                    pad->motorSmall = txByte;
                }
            }
            if (byteIdx == 2U * (u32)(pad->modeId & 0xFU) + 2U && pad->pspPadIndex >= 0) {
                sceMeAudio_C4169D0F((u32)pad->pspPadIndex,
                                    (u32)pad->motorSmall + ((u32)pad->motorLarge << 8));
            }
        }
        if (dataPos >= pad->respLen) {
            const u8 *padBytes = (const u8 *)&pad->buttons;
            return (s32)(padBytes[pad->respLen - 1U] | 0x100U);
        }
        if (dataPos < 6U) {
            const u8 *padBytes = (const u8 *)&pad->buttons;
            return (s32)padBytes[dataPos];
        }
        return 0xFF;
    }
    case 'B':
    case 'C': {
        u32 dataPos = byteIdx - 3U;
        if (pad->activeCmd == 'B' && byteIdx == 4U) {
            u32 rumbleIdx = (((pad->subCmdArg & 0xC0U) == 0x40U) && txByte != 0U) ? 2U : 0U;
            sceMeAudio_C4169D0F(slotIdx, (u32)g_padRumbleTable[rumbleIdx] +
                                         ((u32)g_padRumbleTable[rumbleIdx + 1U] << 8));
        } else if (pad->activeCmd == 'C' && byteIdx == 3U) {
            if (txByte != 0U) {
                pad->statusByte = 0U;
            }
            pad->configMode = txByte;
            pad->dualshockMode = 1U;
        }
        if (dataPos >= pad->respLen) {
            const u8 *padBytes = (const u8 *)&pad->buttons;
            return (s32)(padBytes[pad->respLen - 1U] | 0x100U);
        }
        if (dataPos < 6U) {
            const u8 *padBytes = (const u8 *)&pad->buttons;
            return (s32)padBytes[dataPos];
        }
        return 0xFF;
    }
    case 'D':
        if (byteIdx == 3U) {
            pad->rumbleEnableMask = 0U;
            pad->statusByte = 0U;
            pad->motorSmall = 0U;
            pad->motorLarge = 0U;
            if (txByte < 2U) {
                pad->modeId = (txByte != 0U) ? 0x73U : 0x41U;
                pad->analogEnabled = txByte;
                if ((g_pops->quirkFlagsExt & 0x200) != 0 && txByte != 0U && slotIdx == 0U) {
                    g_pops->pads[1].modeId = 0x73U;
                    g_pops->pads[1].analogEnabled = txByte;
                }
                popsGpuSetAnalogMode(txByte);
            }
            if (pad->pspPadIndex >= 0) {
                sceMeAudio_C4169D0F((u32)pad->pspPadIndex,
                                    (u32)pad->motorSmall + ((u32)pad->motorLarge << 8));
            }
            return 0;
        }
        if (byteIdx == 4U && (txByte & 2U) != 0U) {
            pad->modeLocked = txByte & 1U;
            POPS_SHARED_MEM->padModes[slotIdx] = (u8)((txByte & 1U) + 2U * pad->analogEnabled);
        }
        return 0;
    case 'E':
        if (byteIdx >= 3U && byteIdx < 8U) {
            return (byteIdx == 5U) ? (s32)pad->analogEnabled : (s32)g_padCmdEData[byteIdx - 3U];
        }
        return 0;
    case 'F':
        if (byteIdx >= 5U && byteIdx < 9U && pad->subCmdArg < 2U) {
            return (s32)g_padCmdFData[4U * pad->subCmdArg + (byteIdx - 5U)];
        }
        return 0;
    case 'G':
        if (byteIdx >= 5U && byteIdx < 8U && pad->subCmdArg == 0U) {
            return (s32)g_padCmdGData[byteIdx - 5U];
        }
        return 0;
    case 'H':
        if (pad->subCmdArg < 2U) {
            if (byteIdx == 7U) {
                return 1;
            }
            if (byteIdx == 8U) {
                return (pad->subCmdArg == 0U) ? (s32)pad->motorSmall : (s32)pad->motorLarge;
            }
        }
        return 0;
    case 'I':
        if (byteIdx == 4U) {
            if (txByte == 0U) {
                pad->subCmdArg = 2U;
            }
            return 0;
        }
        if (byteIdx == 5U && pad->subCmdArg < 2U) {
            if (pad->subCmdArg == 0U) {
                pad->motorSmall = txByte;
            } else {
                pad->motorLarge = txByte;
            }
            if (pad->rumbleBlockFlag == 0U && pad->pspPadIndex >= 0) {
                sceMeAudio_C4169D0F((u32)pad->pspPadIndex,
                                    (u32)pad->motorSmall + ((u32)pad->motorLarge << 8));
            }
        }
        return 0;
    case 'J':
        if (byteIdx == 3U) {
            if (pad->rumbleBlockFlag != 0U && pad->pspPadIndex >= 0) {
                sceMeAudio_C4169D0F((u32)pad->pspPadIndex,
                                    (u32)pad->motorSmall + ((u32)pad->motorLarge << 8));
            }
            pad->rumbleBlockFlag = txByte;
        }
        return 0;
    case 'K':
        return 0;
    case 'L':
        if (byteIdx == 6U && pad->subCmdArg < 2U) {
            return (pad->subCmdArg != 0U) ? 7 : 4;
        }
        return 0;
    case 'M': {
        u32 bitPos = byteIdx - 3U;
        u8 bitMask;
        s32 prevVal;
        if (bitPos >= 6U) {
            return 0;
        }
        bitMask = (u8)(1U << bitPos);
        prevVal = 0xFF;
        if ((pad->rumbleEnableMask & bitMask) != 0U) {
            prevVal = ((pad->rumbleSelectMask & bitMask) != 0U) ? 1 : 0;
        }
        pad->rumbleEnableMask |= bitMask;
        if (txByte >= 2U) {
            pad->rumbleEnableMask = (u8)(pad->rumbleEnableMask - bitMask);
            return prevVal;
        }
        pad->rumbleSelectMask |= bitMask;
        if (txByte == 0U) {
            pad->rumbleSelectMask = (u8)(pad->rumbleSelectMask - bitMask);
        }
        if (pad->analogEnabled == 0U) {
            pad->modeId = (u8)((bitPos >> 1) + 0x41U);
        }
        return prevVal;
    }
    default:
        return 0x1FF;
    }
}

/* 0x0093b0: Query whether a virtual controller slot is assigned to a PSP pad */
s32 popsPadIsConnected(u32 slotIdx)
{
    return (g_pops->pads[slotIdx].pspPadIndex < 0) ? 0 : -1;
}

/* 0x0093cc: Stop rumble motors on all assigned pads */
void popsPadMuteAllRumble(void)
{
    u32 idx;
    for (idx = 0U; idx < 8U; idx++) {
        s32 pspIdx = g_pops->pads[idx].pspPadIndex;
        if (pspIdx >= 0) {
            sceMeAudio_C4169D0F((u32)pspIdx, 0U);
        }
    }
}

/* 0x009438: Restore rumble motor outputs on all assigned pads */
void popsPadRestoreAllRumble(void)
{
    u32 idx;
    for (idx = 0U; idx < 8U; idx++) {
        PopsPadSlot *pad = &g_pops->pads[idx];
        if (pad->pspPadIndex >= 0) {
            sceMeAudio_C4169D0F((u32)pad->pspPadIndex,
                                (u32)pad->motorSmall + ((u32)pad->motorLarge << 8));
        }
    }
}

/* 0x009490: PS1 Memory Card & PocketStation SIO0 protocol handler */
s32 popsMcExchangeByte(u32 byteIdx, u32 slotIdx, u8 txByte)
{
    PopsMemCardSlot *card;

    if (slotIdx >= 2U) {
        return 0x1FF;
    }
    card = &g_mcSlots[slotIdx];
    if ((card->statusFlags & 1U) == 0U) {
        return 0x1FF;
    }

    if (byteIdx == 0U) {
        return 0xFF;
    }
    if (byteIdx == 1U) {
        u8 prevFlag = card->flagByte;
        card->activeCmd = txByte;
        card->checksum = 0U;
        card->sectorAddr = 0U;
        return (s32)prevFlag;
    }

    /* PocketStation extended command commands when pocketEnable is active */
    if (card->pocketEnable != 0U) {
        if (card->activeCmd == 'X') {
            if (byteIdx == 2U) {
                return 2;
            }
            return (byteIdx == 3U) ? 1 : 0x101;
        }
        if (card->activeCmd >= 'Y' && card->activeCmd <= '^') {
            if (byteIdx == 2U) {
                s32 len;
                sceMeAudio_9FBE4AD3(1U, card->activeCmd);
                len = sceMeAudio_9FBE4AD3(2U, txByte);
                card->pocketLen = (u8)(len + 2);
                return len;
            }
            if ((u8)(card->activeCmd - '[') >= 2U && byteIdx == card->pocketLen) {
                return sceMeAudio_9FBE4AD3(byteIdx | 0x100U, txByte) | 0x100;
            }
            return sceMeAudio_9FBE4AD3(byteIdx, txByte);
        }
        if (card->activeCmd != 'R' && card->activeCmd != 'W') {
            return 0x1FF;
        }
    }

    if (byteIdx == 2U) {
        return (card->activeCmd == 'R' || card->activeCmd == 'W') ? 0x5A : 0x1FF;
    }
    if (byteIdx == 3U) {
        return 0x5D;
    }
    if (byteIdx == 4U || byteIdx == 5U) {
        card->checksum ^= txByte;
        card->sectorAddr = (u16)((card->sectorAddr << 8) + txByte);
        return 0;
    }

    if (card->activeCmd == 'R') {
        u32 sector = card->sectorAddr;
        if (byteIdx == 6U || byteIdx == 7U) {
            return (s32)(byteIdx + 86U);
        }
        if (byteIdx == 8U) {
            return (sector < 0x400U) ? (s32)(sector >> 8) : 0xFF;
        }
        if (byteIdx == 9U) {
            return (sector < 0x400U) ? (s32)(sector & 0xFFU) : 0x1FF;
        }
        if (byteIdx == 138U) {
            return (s32)card->checksum;
        }
        if (byteIdx == 139U) {
            if ((card->statusFlags & 2U) != 0U) {
                if (g_mcWritebackAlarm > 0) {
                    sceKernelCancelAlarm(g_mcWritebackAlarm);
                }
                g_mcWritebackAlarm = sceKernelSetAlarm(1000000U, popsMcWritebackAlarmCb, NULL);
            }
            return 0x147;
        }
        {
            u8 dataByte = card->cardData[(sector << 7) + (byteIdx - 10U)];
            card->checksum ^= dataByte;
            return (s32)dataByte;
        }
    }

    /* Write ('W') command */
    if (byteIdx == 6U && card->pocketEnable != 0U) {
        u32 sector = card->sectorAddr;
        if (sector >= 0x10U && sector < 0x38U) {
            card->sectorAddr = 0x400U;
        } else if (sector >= 0x400U) {
            card->sectorAddr = 0x401U;
        }
    }

    if (byteIdx < 134U) {
        u32 sector = card->sectorAddr;
        if (sector != 0x3FU && sector < 0x400U) {
            card->cardData[(sector << 7) + (byteIdx - 6U)] = txByte;
        }
        card->checksum ^= txByte;
        return 0xFF;
    }
    if (byteIdx == 134U) {
        card->checksum ^= txByte;
        return 0xFF;
    }
    if (byteIdx == 135U || byteIdx == 136U) {
        return (s32)(byteIdx - 43U);
    }
    if (byteIdx == 137U) {
        u32 sector;
        if (card->checksum != 0U) {
            return 0x14E;
        }
        sector = card->sectorAddr;
        if (sector >= 0x400U) {
            if (card->pocketEnable != 0U && sector == 0x400U) {
                return 0x1FD;
            }
            return 0x1FF;
        }
        if (sector != 0x3FU) {
            if ((card->statusFlags & 2U) == 0U) {
                card->statusFlags |= 2U;
                sceKernelPowerLock(0);
                sceKermitPeripheral_D27C5E03(0U);
            }
            if (g_mcWritebackAlarm > 0) {
                sceKernelCancelAlarm(g_mcWritebackAlarm);
            }
            g_mcWritebackAlarm = sceKernelSetAlarm(1000000U, popsMcWritebackAlarmCb, NULL);
        }
        card->flagByte = 0U;
        if (card->pocketEnable != 0U) {
            u32 secBase = (u32)(uintptr_t)&card->cardData[sector << 7];
            u32 lastByte = secBase + 127U;
            u32 line = (secBase & ~0x3FU) | (lastByte & 0x3FU);
            for (;;) {
                __builtin_allegrex_cache(0x1A, (void *)(uintptr_t)line);
                if (line == lastByte) {
                    break;
                }
                line += 64U;
            }
            sceMeAudio_10DABACD(sector << 8);
        }
        return 0x147;
    }

    {
        u32 sector = card->sectorAddr;
        if (sector != 0x3FU && sector < 0x400U) {
            card->cardData[(sector << 7) + (byteIdx - 6U)] = txByte;
        }
        card->checksum ^= txByte;
        return 0xFF;
    }
}

/* 0x009aac: Return -1 if VMC load fails, 0 if slot loaded cleanly */
s32 popsMcHasInsertedCard(u32 slotIdx)
{
    return (popsVmcLoadCurrentTitle(slotIdx) == 0) ? 0 : -1;
}

/* 0x009ad4: Convert BCD Minute:Second:Frame triple to PS1 LBA (0-based, minus 2s pregap) */
s32 popsMsfToLba(const u8 *msf)
{
    s32 minutes = (s32)msf[0] - 6 * (s32)(msf[0] >> 4);
    s32 seconds = (s32)msf[1] - 6 * (s32)(msf[1] >> 4);
    s32 frames = (s32)msf[2] - 6 * (s32)(msf[2] >> 4);
    return 75 * (60 * minutes + seconds) + frames - 150;
}
