#include "pops_internal.h"

/* PSISOIMG0000 header and index tables mapped at fixed address 0x09E80000 */
#define POPS_ISO_HDR_BASE         0x09E80000U
#define POPS_ISO_TOC_FIRST_TRACK  (*(const volatile u8 *)0x09E80807U)
#define POPS_ISO_TOC_LAST_TRACK   (*(const volatile u8 *)0x09E80811U)
#define POPS_ISO_TOC_LEADOUT_MSF  ((const u8 *)0x09E8081BU)
#define POPS_ISO_TOC_TRACK1_MSF   ((const u8 *)0x09E80825U)
#define POPS_ISO_TOC_TRACK2_MSF   ((const u8 *)0x09E8082FU)
#define POPS_ISO_DATA_START_OFF   (*(const volatile u32 *)0x09E80BFCU)
#define POPS_ISO_CDDA_TABLE_BASE  0x09E80BE0U
#define POPS_ISO_INDEX_BASE       0x09E84000U
#define POPS_CD_RAW_STAGE_BUF     0x09480000U
#define POPS_CDDA_PCM_RING_BASE   0x49492600U

typedef struct PopsIsoIndexEntry {
    u32 fileOffset;
    u16 compressedSize;
    u16 flags;
    u8 shaDigest[16];
    s32 readBytes;
    u32 padWord;
} PopsIsoIndexEntry;

typedef struct PopsCddaTrackEntry {
    u32 startOffset;
    u32 endOffset;
    u32 trackFlags;
    u32 cryptSeed;
} PopsCddaTrackEntry;

static __inline__ u8 popsToBcd(u32 val)
{
    return (u8)(val + 6U * ((103U * val) >> 10));
}

static __inline__ u32 popsFromBcd(u8 bcd)
{
    return (u32)bcd - 6U * ((u32)bcd >> 4);
}

static __inline__ u32 popsGetTrackCount(void)
{
    return popsFromBcd(POPS_ISO_TOC_LAST_TRACK) - popsFromBcd(POPS_ISO_TOC_FIRST_TRACK) + 1U;
}

static __inline__ const u8 *popsGetTrackMsf(u32 trackNum)
{
    return (const u8 *)(uintptr_t)(0x09E8081BU + 10U * trackNum);
}

static __inline__ const PopsIsoIndexEntry *popsGetIsoIndex(u32 lba)
{
    return &((const PopsIsoIndexEntry *)(uintptr_t)POPS_ISO_INDEX_BASE)[lba >> 4];
}

static __inline__ const PopsCddaTrackEntry *popsGetCddaTrack(s32 trackNum)
{
    return &((const PopsCddaTrackEntry *)(uintptr_t)POPS_ISO_CDDA_TABLE_BASE)[trackNum];
}

static __inline__ u32 popsIrqDisable(void)
{
    u32 prev;
    __asm__ __volatile__("mfic %0, $0\n\tmtic $zero, $0" : "=r"(prev));
    return prev;
}

static __inline__ void popsIrqRestore(u32 prev)
{
    __asm__ __volatile__("mtic %0, $0" :: "r"(prev));
}

/* 0x009b48: Execute PS1 CXD2940Q CD-ROM controller command (0x00..0x1E) */
void popsCdExecCommand(u32 cmd)
{
    u32 quirks = g_pops->quirkFlags;
    u8 *resp;

    g_pops->cdQueuedCmd = -1;
    if (g_pops->cdParamCount < g_cdParamMinTable[cmd]) {
        g_pops->cdErrorCode = 0x20U;
        g_pops->cdErrorStatusMask = 1U;
    }
    if (g_pops->cdRespEvent.irqCode == 2) {
        g_pops->cdRespEvent.irqCode = -1;
    }
    g_pops->cdStatusPort |= 0x80U;

    if ((quirks & 0x10000U) == 0U) {
        s32 isHoldCmd = (cmd == 9U) ||
                        ((cmd == 6U || cmd == 0x1BU) && (quirks & 0x200000U) != 0U);
        if (isHoldCmd) {
            if (g_pops->cdDiscChangeReady == 0U && (quirks & 0x4000U) == 0U) {
                g_pops->cdIdleCountdown = 20U;
                if ((quirks & 0x800000U) != 0U) {
                    g_pops->cdDiscChangeReady = 1U;
                }
            }
        } else if (cmd != 0x13U && cmd != 0x0EU && cmd != 1U && cmd != 2U) {
            g_pops->cdIdleCountdown = 0U;
            g_pops->cdDiscChangeReady = 0U;
        }
    }

    if (cmd >= 0x1FU) {
        g_pops->cdErrorCode = 0x40U;
        g_pops->cdErrorStatusMask = 1U;
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdParamCount = 0U;
        return;
    }

    switch (cmd) {
    case 0x00U:
    case 0x17U:
    case 0x18U:
    case 0x1DU:
        g_pops->cdErrorCode = 0x40U;
        g_pops->cdErrorStatusMask = 1U;
        popsCdQueueAckResp(1U, 0x4000);
        break;

    case 0x01U: /* Getstat */
        if ((g_pops->cdStatByte & 0x10U) != 0U) {
            if (g_pops->cdTrayState == 2U) {
                u8 savedMode = g_pops->cdModeReg;
                popsCdReset();
                g_pops->cdModeReg = savedMode;
                g_pops->cdStatByte = 0U;
                g_pops->cdSectorOffset = ((savedMode & 0x20U) != 0U) ? 12U : 24U;
                g_pops->cdSectorLimit = ((savedMode & 0x30U) != 0U) ? 2352U : 2072U;
                popsEventSchedule(&g_pops->cdSpinupEvent, 50803200);
                g_pops->cdTrayState = 3U;
            }
        } else if (g_pops->cdIdleCountdown != 0U) {
            g_pops->cdIdleCountdown--;
            if (g_pops->cdIdleCountdown == 0U) {
                g_pops->cdDiscChangeReady = 1U;
                g_pops->cdIdleCountdown = 20U;
            }
        }
        popsCdQueueAckResp(1U, 0x4000);
        break;

    case 0x02U: /* Setloc */
        if (g_pops->cdTrayState != 0U) {
            g_pops->cdErrorCode = 0x80U;
            g_pops->cdErrorStatusMask = 1U;
        } else if (g_pops->cdParamFifo[1] >= 0x60U ||
                   g_pops->cdParamFifo[2] >= 0x75U ||
                   (g_pops->cdParamFifo[0] & 0xFU) >= 0xAU ||
                   (g_pops->cdParamFifo[1] & 0xFU) >= 0xAU ||
                   (g_pops->cdParamFifo[2] & 0xFU) >= 0xAU) {
            g_pops->cdErrorCode = 0x10U;
            g_pops->cdErrorStatusMask = 1U;
        }
        if (popsCdQueueAckResp(1U, 0x4000) != NULL) {
            s32 target;
            if ((g_pops->cdStatByte & 0xE0U) == 0U) {
                popsDiscStopCddaTrack();
            }
            target = __builtin_allegrex_max(popsMsfToLba(g_pops->cdParamFifo), 0);
            g_pops->cdTargetLba = (u32)target;
            g_pops->cdSetlocPending = 1U;
            popsDiscPrefetch((u32)target);
            if ((g_pops->cdStatByte & 0x40U) != 0U) {
                s32 seek = popsCdComputeSeekCycles((u32)target);
                g_pops->cdSeekEndCycle = (g_pops->cycleTarget - (u32)g_pops->cycleRemaining) +
                                         (u32)seek + 0x4000U;
            }
        }
        break;

    case 0x03U: { /* Play */
        u32 playLba = (g_pops->cdSetlocPending != 0U) ? g_pops->cdTargetLba : g_pops->cdCurrentLba;
        popsCdQueueAckResp(1U, 0x4000);
        if (g_pops->cdParamCount != 0U && g_pops->cdParamFifo[0] != 0U) {
            u8 bcdTrk = g_pops->cdParamFifo[0];
            u32 maxTracks = popsGetTrackCount();
            u32 trk = popsFromBcd(bcdTrk) - 1U;
            if ((bcdTrk & 0xFU) >= 0xAU || maxTracks < trk) {
                trk = maxTracks;
            }
            if (trk == g_pops->cdCurrentTrack) {
                break;
            }
            playLba = (u32)popsMsfToLba(popsGetTrackMsf(trk + 1U));
        }
        {
            u32 trackIdx = popsCdFindTrackForLba(playLba);
            s32 seek;
            g_pops->cdSubqIndex = (u8)((playLba % 75U) / 10U);
            g_pops->cdCurrentTrack = (u8)trackIdx;
            popsDiscStartCddaTrack((s32)trackIdx, playLba);
            g_pops->cdMuted = 0U;
            g_pops->cdModeReg |= 1U;
            g_pops->cdXaInterleave = 0U;
            g_pops->cdXaNextLba = 0U;
            if (g_pops->cdReadEvent.next != NULL) {
                popsEventCancel(&g_pops->cdReadEvent);
            }
            g_pops->cdStatByte = (u8)((g_pops->cdStatByte & 0x15U) | 0x42U);
            seek = popsCdComputeSeekCycles(playLba);
            g_pops->cdReadEvent.callback = popsCdAudioPlayCallback;
            popsEventSchedule(&g_pops->cdReadEvent, seek + 467968);
            POPS_ME_SPU_SHARED->cdAudioVol = g_pops->cdAudioVol.word;
        }
        break;
    }

    case 0x04U: /* Forward */
    case 0x05U: /* Backward */
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdXaInterleave = 0U;
        g_pops->cdXaNextLba = 0U;
        if (g_pops->cdReadEvent.next != NULL) {
            popsEventCancel(&g_pops->cdReadEvent);
        }
        g_pops->cdStatByte &= 0x17U;
        break;

    case 0x06U: /* ReadN */
    case 0x1BU: /* ReadS */
        if (g_pops->cdTrayState != 0U ||
            ((g_pops->cdStatByte & 2U) == 0U && g_pops->cdSpinupEvent.next == NULL &&
             g_pops->cdSetlocPending == 0U)) {
            g_pops->cdErrorCode = 0x80U;
            g_pops->cdErrorStatusMask = 1U;
        }
        if (popsCdQueueAckResp(1U, 0x4000) == NULL) {
            break;
        }
        if (g_pops->cdReadEvent.next != NULL) {
            if (g_pops->cdSetlocPending == 0U && g_pops->cdReadEvent.callback == popsCdDataReadCallback) {
                break;
            }
            g_pops->cdXaInterleave = 0U;
            g_pops->cdXaNextLba = 0U;
            popsEventCancel(&g_pops->cdReadEvent);
            g_pops->cdStatByte &= 0x17U;
            popsDiscStopCddaTrack();
        }
        {
            u32 delay = ((s8)g_pops->cdModeReg < 0) ? 242176U : 467968U;
            if (g_pops->cdSetlocPending != 0U) {
                delay += (u32)popsCdComputeSeekCycles(g_pops->cdTargetLba);
            } else if ((g_pops->cdSpeedScale & 0x40U) == 0U) {
                delay += 5419008U;
            }
            if ((g_pops->cdStatByte & 0x40U) != 0U) {
                u32 now = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
                u32 seekRem = g_pops->cdSeekEndCycle - now;
                if (delay < seekRem) {
                    delay = seekRem;
                    if (g_pops->cdRespEvent.ev.next != NULL) {
                        popsEventCancel(&g_pops->cdRespEvent.ev);
                    }
                }
            } else if (g_pops->cdSetlocPending == 0U) {
                g_pops->cdStatByte |= 0x40U;
            }
            g_pops->cdReadEvent.callback = popsCdDataReadCallback;
            popsEventSchedule(&g_pops->cdReadEvent, (s32)delay);
            popsDiscPrefetch(g_pops->cdCurrentLba);
            g_pops->cdMuted = 0U;
            if ((g_pops->cdModeReg & 0x40U) != 0U) {
                POPS_ME_SPU_SHARED->cdAudioVol = g_pops->cdAudioVol.word;
            }
        }
        break;

    case 0x07U: { /* MotorOn */
        s32 completeDelay = 0x8000;
        if ((g_pops->cdStatByte & 2U) != 0U) {
            g_pops->cdErrorCode = 0x20U;
            g_pops->cdErrorStatusMask = 1U;
        } else {
            s32 rem;
            if (g_pops->cdSpinupEvent.next == NULL) {
                popsEventSchedule(&g_pops->cdSpinupEvent, 50803200);
            }
            rem = (s32)(g_pops->cdSpinupEvent.dueCycle -
                        (g_pops->cycleTarget - (u32)g_pops->cycleRemaining));
            completeDelay = __builtin_allegrex_max(rem, 0x8000);
        }
        if (popsCdQueueAckResp(1U, 0x4000) != NULL) {
            popsCdQueueCompleteResp(1U, completeDelay);
        }
        break;
    }

    case 0x08U: { /* Stop */
        s32 stopDelay = 29184;
        g_pops->cdDiscChangeReady = ((quirks & 0x10000U) == 0U) ? 1U : 0U;
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdXaInterleave = 0U;
        g_pops->cdXaNextLba = 0U;
        if (g_pops->cdReadEvent.next != NULL) {
            popsEventCancel(&g_pops->cdReadEvent);
        }
        g_pops->cdStatByte &= 0x17U;
        if ((g_pops->cdStatByte & 2U) != 0U) {
            g_pops->cdStatByte -= 2U;
            stopDelay = 13631488;
        }
        popsCdQueueCompleteResp(1U, stopDelay);
        g_pops->cdMuted = 1U;
        g_pops->cdTargetLba = 0U;
        g_pops->cdCurrentLba = 0U;
        g_pops->cdCurrentTrack = 0U;
        if (g_pops->cdSpinupEvent.next != NULL) {
            popsEventCancel(&g_pops->cdSpinupEvent);
        }
        break;
    }

    case 0x09U: { /* Pause */
        s32 pauseDelay = 22656;
        g_pops->cdInitialized = 1U;
        if (g_pops->cdTrayState != 0U || (g_pops->cdStatByte & 0x40U) != 0U) {
            g_pops->cdErrorCode = 0x80U;
            g_pops->cdErrorStatusMask = 1U;
        }
        if (popsCdQueueAckResp(1U, 0x4000) == NULL) {
            break;
        }
        if (g_pops->cdReadEvent.next != NULL) {
            s32 minDelay = ((s8)g_pops->cdStatByte < 0) ? 388940 : 761497;
            s32 rem = (s32)(g_pops->cdReadEvent.dueCycle -
                            (g_pops->cycleTarget - (u32)g_pops->cycleRemaining));
            pauseDelay = __builtin_allegrex_max(rem, 22656);
            if ((g_pops->cdSpeedScale & 4U) == 0U) {
                minDelay *= 6;
            }
            if (pauseDelay < minDelay) {
                pauseDelay = minDelay;
            }
        } else if (g_pops->cdRespEvent.ev.next != NULL) {
            s32 rem = (s32)(g_pops->cdRespEvent.ev.dueCycle -
                            (g_pops->cycleTarget - (u32)g_pops->cycleRemaining));
            pauseDelay = __builtin_allegrex_max(rem, 22656);
        }
        g_pops->cdMuted = 1U;
        if ((g_pops->cdModeReg & 0x41U) != 0U) {
            popsDiscStopCddaTrack();
        }
        g_pops->cdCurrentTrack = 0U;
        g_pops->cdXaInterleave = 0U;
        g_pops->cdXaNextLba = 0U;
        if (g_pops->cdReadEvent.next != NULL) {
            popsEventCancel(&g_pops->cdReadEvent);
        }
        g_pops->cdStatByte &= 0x17U;
        popsCdQueueCompleteResp(1U, pauseDelay);
        break;
    }

    case 0x0AU: /* Init */
    case 0x1EU: { /* ReadTOC */
        u32 rng = (1103515245U * g_pops->cdRngState + 12345U) & 0x7FFFFFFFU;
        u32 delay = 33U * (rng & 0xFFFFU) + 0x800000U;
        u8 lockMode = (cmd == 0x0AU) ? 1U : 2U;

        g_pops->cdRngState = rng;
        if (g_pops->cdReadEvent.next != NULL) {
            delay += g_pops->cdReadEvent.dueCycle -
                     (g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
        }
        if (g_pops->cdSpinupEvent.next != NULL) {
            u32 spinRem = g_pops->cdSpinupEvent.dueCycle -
                          (g_pops->cycleTarget - (u32)g_pops->cycleRemaining);
            if (delay < spinRem) {
                delay = spinRem;
            }
        } else if ((g_pops->cdStatByte & 2U) == 0U) {
            delay = 33868800U;
        }
        popsCdReset();
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdCmdLock = lockMode;
        popsCdQueueCompleteResp(1U, (s32)delay);
        break;
    }

    case 0x0BU: /* Mute */
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdMuted = 1U;
        POPS_ME_SPU_SHARED->cdAudioVol = 0U;
        break;

    case 0x0CU: /* Demute */
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdMuted = 0U;
        POPS_ME_SPU_SHARED->cdAudioVol = g_pops->cdAudioVol.word;
        break;

    case 0x0DU: /* Setfilter */
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdXaFilter = (u16)((u32)g_pops->cdParamFifo[0] + ((u32)g_pops->cdParamFifo[1] << 8));
        if ((g_pops->cdSpeedScale & 0x1000U) == 0U) {
            g_pops->cdXaNextLba = 1U;
        }
        break;

    case 0x0EU: { /* Setmode */
        u8 newMode = g_pops->cdParamFifo[0];
        s8 diffMode = (s8)(g_pops->cdModeReg ^ newMode);

        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdModeReg = newMode;
        if (g_pops->cdSpinupEvent.next == NULL) {
            if ((g_pops->cdStatByte & 2U) == 0U || diffMode < 0) {
                s32 spinDelay = 50803200;
                if ((g_pops->cdStatByte & 2U) != 0U) {
                    g_pops->cdSpinupState = 1U;
                    spinDelay = ((g_pops->cdSpeedScale & 0x80U) != 0U) ? 33868800 : 10000;
                }
                popsEventSchedule(&g_pops->cdSpinupEvent, spinDelay);
                if (g_pops->cdReadEvent.next != NULL) {
                    s32 newReadDelay = (s32)(g_pops->cdReadEvent.dueCycle -
                                             (g_pops->cycleTarget - (u32)g_pops->cycleRemaining)) + spinDelay;
                    g_pops->cdReadEvent.prev->next = g_pops->cdReadEvent.next;
                    g_pops->cdReadEvent.next->prev = g_pops->cdReadEvent.prev;
                    popsEventSchedule(&g_pops->cdReadEvent, newReadDelay);
                }
            }
        } else if (g_pops->cdSpinupState == 1U && diffMode < 0) {
            u32 now = g_pops->cycleTarget - (u32)g_pops->cycleRemaining;
            s32 rem = (s32)(g_pops->cdSpinupEvent.dueCycle - now);
            s32 extra = __builtin_allegrex_min(33868800, 67737600 - 4 * rem);
            g_pops->cdSpinupState = 2U;
            g_pops->cdSpinupEvent.prev->next = g_pops->cdSpinupEvent.next;
            g_pops->cdSpinupEvent.next->prev = g_pops->cdSpinupEvent.prev;
            popsEventSchedule(&g_pops->cdSpinupEvent, rem + extra);
        }

        if ((diffMode & 0x41) != 0) {
            if ((newMode & 0x41U) != 0U) {
                POPS_ME_SPU_SHARED->cdAudioVol = g_pops->cdAudioVol.word;
            } else {
                popsGpuThrottleSpeed();
                POPS_ME_SPU_SHARED->cdAudioVol = 0U;
                g_pops->cdXaInterleave = 0U;
            }
        }
        break;
    }

    case 0x0FU: /* Getparam */
        resp = popsCdQueueAckResp(5U, 0x4000);
        if (resp != NULL) {
            resp[1] = g_pops->cdModeReg;
            resp[2] = g_pops->cdErrorCode;
            resp[3] = (u8)(g_pops->cdXaFilter & 0xFFU);
            resp[4] = (u8)(g_pops->cdXaFilter >> 8);
        }
        break;

    case 0x10U: { /* GetlocL */
        s32 delay = 0x8000;
        if ((g_pops->cdStatByte & 0xC0U) != 0U) {
            g_pops->cdErrorCode = 0x80U;
            g_pops->cdErrorStatusMask = 1U;
            delay = 0x4000;
        }
        resp = popsCdQueueAckResp(8U, delay);
        if (resp != NULL) {
            if (g_pops->cdLastHeaderPtr != 0U) {
                const u8 *hdr = (const u8 *)(uintptr_t)g_pops->cdLastHeaderPtr;
                u32 idx;
                for (idx = 0U; idx < 8U; idx++) {
                    resp[idx] = hdr[idx];
                }
                if (g_pops->spuDelayQuirk != 0U && g_pops->cdXaNextLba != 0U &&
                    popsSpuGetCddaQueuedCount() >= 2) {
                    u32 xaAbs = g_pops->cdXaNextLba - g_pops->cdXaInterleave + 149U;
                    resp[0] = popsToBcd((xaAbs / 75U) / 60U);
                    resp[1] = popsToBcd((xaAbs / 75U) % 60U);
                    resp[2] = popsToBcd(xaAbs % 75U);
                }
            } else if (g_pops->cdSeekDataFlag != 0U) {
                u8 *sec = popsDiscFetchSector(g_pops->cdCurrentLba, 1);
                if (sec != NULL) {
                    u32 idx;
                    g_pops->cdLastHeaderPtr = (u32)(uintptr_t)(sec + 12);
                    for (idx = 0U; idx < 8U; idx++) {
                        resp[idx] = sec[12U + idx];
                    }
                } else {
                    u32 idx;
                    for (idx = 0U; idx < 8U; idx++) {
                        resp[idx] = 0U;
                    }
                }
            } else {
                u32 idx;
                for (idx = 0U; idx < 8U; idx++) {
                    resp[idx] = 0U;
                }
            }
        }
        break;
    }

    case 0x11U: { /* GetlocP */
        u32 lba = g_pops->cdCurrentLba;
        u32 subOffset;
        if ((g_pops->cdStatByte & 0xE0U) != 0U) {
            lba--;
            if ((g_pops->cdModeReg & 0x40U) != 0U && g_pops->cdXaNextLba != 0U) {
                u32 xaLba = g_pops->cdXaNextLba - g_pops->cdXaInterleave;
                if (popsSpuGetCddaQueuedCount() >= 2) {
                    if (g_pops->spuDelayQuirk != 0U) {
                        lba = xaLba - 1U;
                    }
                } else if (lba < xaLba) {
                    lba = xaLba;
                }
            }
        }
        subOffset = ((g_pops->cdSpeedScale & 0x100U) == 0U) ? 1U : 0U;
        resp = popsCdQueueAckResp(8U, 0x8000);
        if (resp != NULL) {
            s32 pos = (s32)(lba - subOffset);
            s32 *subTbl = (s32 *)(void *)g_pops->subchannelTable;
            s32 foundExact = 0;

            if (subTbl != NULL) {
                s32 step = (*subTbl >= pos) ? -1 : 1;
                for (;;) {
                    s32 diff = *subTbl - pos;
                    if (diff == 0) {
                        sceKernelMemcpy(resp, subTbl + 1, 8U);
                        g_pops->subchannelTable = (u8 *)(void *)subTbl;
                        foundExact = 1;
                        break;
                    }
                    if ((diff ^ step) >= 0 || subTbl[3 * step] == -1) {
                        g_pops->subchannelTable = (u8 *)(void *)subTbl;
                        pos += ((const s32 *)(uintptr_t)g_subchannelAllocPtr)[1];
                        break;
                    }
                    subTbl += 3 * step;
                }
            }

            if (!foundExact) {
                u32 absSec = (u32)(pos + 150) / 75U;
                u32 absFrm = (u32)(pos + 150) % 75U;
                popsCdFillTrackRelMsf((u32)pos, resp);
                resp[5] = popsToBcd(absSec / 60U);
                resp[6] = popsToBcd(absSec % 60U);
                resp[7] = popsToBcd(absFrm);
            }
        }
        break;
    }

    case 0x12U: /* SetSession */
        if (g_pops->cdReadEvent.next != NULL) {
            g_pops->cdErrorCode = 0x80U;
            g_pops->cdErrorStatusMask = 1U;
        } else if (g_pops->cdParamFifo[0] == 0U) {
            g_pops->cdErrorCode = 0x10U;
            g_pops->cdErrorStatusMask = 1U;
        }
        if (popsCdQueueAckResp(1U, 0x4000) != NULL) {
            popsCdQueueCompleteResp(1U, 0x8000);
        }
        break;

    case 0x13U: /* GetTN */
        if (g_pops->cdTrayState != 0U) {
            g_pops->cdErrorCode = 0x80U;
            g_pops->cdErrorStatusMask = 1U;
        }
        resp = popsCdQueueAckResp(3U, 0x4000);
        if (resp != NULL) {
            resp[1] = popsToBcd(POPS_ISO_TOC_FIRST_TRACK);
            resp[2] = popsToBcd(popsGetTrackCount());
        }
        break;

    case 0x14U: { /* GetTD */
        u8 bcdTrk = g_pops->cdParamFifo[0];
        u32 trk = popsFromBcd(bcdTrk);
        if ((bcdTrk & 0xFU) >= 0xAU || popsGetTrackCount() < trk) {
            g_pops->cdErrorCode = 0x10U;
            g_pops->cdErrorStatusMask = 1U;
        }
        resp = popsCdQueueAckResp(3U, 0x4000);
        if (resp != NULL) {
            const u8 *msf = popsGetTrackMsf(trk);
            resp[1] = msf[0];
            resp[2] = msf[1];
        }
        break;
    }

    case 0x15U: /* SeekL */
    case 0x16U: { /* SeekP */
        u32 seekDur;
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdSeekDataFlag = (u8)(cmd - 0x16U);
        if ((g_pops->cdStatByte & 0x40U) != 0U && g_pops->cdReadEvent.next != NULL) {
            seekDur = (g_pops->cdReadEvent.dueCycle -
                       (g_pops->cycleTarget - (u32)g_pops->cycleRemaining)) + 745113U;
        } else {
            seekDur = (u32)popsCdComputeSeekCycles(g_pops->cdTargetLba);
        }
        g_pops->cdSeekEndCycle = (g_pops->cycleTarget - (u32)g_pops->cycleRemaining) + seekDur;
        if (seekDur < 0x4000U) {
            seekDur = 0x4000U;
        }
        popsCdQueueCompleteResp(1U, (s32)(seekDur + 0x4000U));
        g_pops->cdXaInterleave = 0U;
        g_pops->cdXaNextLba = 0U;
        if (g_pops->cdReadEvent.next != NULL) {
            popsEventCancel(&g_pops->cdReadEvent);
        }
        g_pops->cdStatByte &= 0x17U;
        popsDiscStopCddaTrack();
        g_pops->cdStatByte |= 0x42U;
        break;
    }

    case 0x19U: { /* Test */
        const void *srcData = NULL;
        u32 respLen = 0U;
        switch (g_pops->cdParamFifo[0]) {
        case 0x00U: case 0x01U: case 0x02U: case 0x03U: case 0x04U:
        case 0x10U: case 0x11U: case 0x12U: case 0x13U: case 0x14U:
        case 0x15U: case 0x16U: case 0x17U: case 0x18U: case 0x19U: case 0x1AU:
            respLen = 1U;
            break;
        case 0x05U:
            srcData = g_cdTestVersion;
            respLen = 2U;
            break;
        case 0x20U:
            srcData = g_cdTestDate;
            respLen = 4U;
            break;
        case 0x22U:
            srcData = "for Japan";
            respLen = 9U;
            break;
        case 0x23U: case 0x24U: case 0x25U:
            srcData = g_cdTestChipId;
            respLen = 8U;
            break;
        default:
            break;
        }
        if (respLen != 0U) {
            resp = popsCdQueueAckResp((u8)respLen, 0x4000);
            if (srcData != NULL && resp != NULL) {
                sceKernelMemcpy(resp, srcData, respLen);
            }
        }
        break;
    }

    case 0x1AU: /* GetID */
        popsCdQueueAckResp(1U, 0x4000);
        g_pops->cdXaInterleave = 0U;
        g_pops->cdXaNextLba = 0U;
        if (g_pops->cdReadEvent.next != NULL) {
            popsEventCancel(&g_pops->cdReadEvent);
        }
        g_pops->cdStatByte &= 0x17U;
        popsCdQueueCompleteResp(8U, 0x8000);
        g_pops->cdCmdLock = 1U;
        g_pops->cdRespEvent.respData[1] = 0U;
        g_pops->cdRespEvent.respData[2] = 0U;
        g_pops->cdRespEvent.respData[3] = 2U;
        g_pops->cdRespEvent.respData[4] = 'S';
        g_pops->cdRespEvent.respData[5] = 'C';
        g_pops->cdRespEvent.respData[6] = 'E';
        g_pops->cdRespEvent.respData[7] = 'I';
        if (g_pops->cdInitialized == 0U) {
            g_pops->cdInitialized = 1U;
        }
        break;

    case 0x1CU: /* Reset */
        g_pops->cdRngState = (1103515245U * g_pops->cdRngState + 12345U) & 0x7FFFFFFFU;
        popsCdReset();
        popsCdQueueAckResp(1U, 0x4000);
        break;

    default:
        break;
    }

    g_pops->cdParamCount = 0U;
}

/* 0x00adec: Find 1-based CD track number containing the given LBA */
u32 popsCdFindTrackForLba(u32 lba)
{
    u32 maxTracks = popsGetTrackCount();
    u32 absSector = lba + 150U;
    u32 track = 1U;

    while (track < maxTracks) {
        u32 nextStart = (u32)popsMsfToLba(popsGetTrackMsf(track + 1U));
        if (absSector < nextStart) {
            break;
        }
        track++;
    }
    return track;
}

/* 0x00ae9c: Populate track, index, and track-relative BCD MSF in subchannel Q response */
void popsCdFillTrackRelMsf(u32 lba, u8 *outSubq)
{
    u32 track = popsCdFindTrackForLba(lba);
    s32 trackStart = popsMsfToLba(popsGetTrackMsf(track));
    s32 relSigned = (s32)lba - trackStart;
    u32 relAbs = (u32)__builtin_allegrex_max(relSigned, -relSigned);
    u32 relSec = relAbs / 75U;
    u32 relFrm = relAbs % 75U;

    outSubq[0] = popsToBcd(track);
    outSubq[1] = (relSigned >= 0) ? 1U : 0U;
    outSubq[2] = popsToBcd(relSec / 60U);
    outSubq[3] = popsToBcd(relSec % 60U);
    outSubq[4] = popsToBcd(relFrm);
}

/* 0x00afdc: Deliver queued CD-ROM INT response and raise PS1 IRQ2 */
void popsCdDeliverRespCallback(PopsEvent *ev)
{
    PopsCdEvent *cdEv = (PopsCdEvent *)ev;
    u8 irqFlags = g_pops->cdIrqFlags;
    u8 irqEnable = g_pops->cdIrqEnable;

    if (irqFlags == 0U) {
        s8 irqCode = cdEv->irqCode;
        u8 status = g_pops->cdStatusPort;
        cdEv->irqCode = 0;

        if (irqCode < 0) {
            g_pops->cdStatByte &= (u8)~0x40U;
            return;
        } else {
            u32 *dstWords = (u32 *)(void *)g_pops->cdRespFifo;
            const u32 *srcWords = (const u32 *)(const void *)cdEv->respData;
            g_pops->cdRespReadIdx = 0U;
            g_pops->cdRespCount = cdEv->respLen;
            dstWords[0] = srcWords[0];
            dstWords[1] = srcWords[1];
            dstWords[2] = srcWords[2];

            if (cdEv == &g_pops->cdCmdEvent) {
                status &= (u8)~0x80U;
            } else {
                u8 stat = g_pops->cdStatByte;
                if ((stat & 0x40U) != 0U) {
                    stat -= 0x40U;
                    g_pops->cdStatByte = stat;
                    if (g_pops->cdSeekDataFlag != 0U) {
                        g_pops->cdLastHeaderPtr = 0U;
                    }
                }
                g_pops->cdCmdLock = 0U;
                if (g_pops->cdErrorStatusMask != 0U) {
                    g_pops->cdRespCount = 2U;
                    stat |= g_pops->cdErrorStatusMask;
                    g_pops->cdRespFifo[1] = g_pops->cdErrorCode;
                    g_pops->cdErrorStatusMask = 0U;
                }
                g_pops->cdRespFifo[0] = stat;
            }
            g_pops->cdStatusPort = (u8)(status | 0x20U);
            irqFlags = (u8)irqCode;
            g_pops->cdIrqFlags = irqFlags;
        }
    }

    if ((irqFlags & irqEnable) != 0U) {
        popsIrqRaise(4U);
    }
}

/* 0x00b0cc: Emit an immediate INT response on the secondary response slot */
void popsCdEmitImmediateResp(s8 irqCode)
{
    g_pops->cdRespEvent.irqCode = irqCode;
    g_pops->cdRespEvent.respLen = 1U;
    if (g_pops->cdRespEvent.ev.next != NULL) {
        popsEventCancel(&g_pops->cdRespEvent.ev);
    }
    popsCdDeliverRespCallback(&g_pops->cdRespEvent.ev);
}

/* 0x00b118: Schedule command ACK (INT3) or immediate error (INT5) */
u8 *popsCdQueueAckResp(u8 respLen, s32 delayCycles)
{
    g_pops->cdCmdEvent.respLen = respLen;
    popsEventSchedule(&g_pops->cdCmdEvent.ev, delayCycles);
    g_pops->cdCmdEvent.respData[0] = g_pops->cdStatByte | g_pops->cdErrorStatusMask;

    if (g_pops->cdErrorStatusMask != 0U) {
        g_pops->cdCmdEvent.respLen = 2U;
        g_pops->cdCmdEvent.respData[1] = g_pops->cdErrorCode;
        g_pops->cdErrorStatusMask = 0U;
        g_pops->cdCmdEvent.irqCode = 5;
        return NULL;
    }

    g_pops->cdCmdEvent.irqCode = 3;
    return g_pops->cdCmdEvent.respData;
}

/* 0x00b18c: Schedule deferred command completion response (INT2) */
void popsCdQueueCompleteResp(u8 respLen, s32 delayCycles)
{
    s8 prevCode = g_pops->cdRespEvent.irqCode;
    g_pops->cdRespEvent.respLen = respLen;
    g_pops->cdRespEvent.irqCode = 2;

    if (prevCode < 0 && g_pops->cdRespEvent.ev.next != NULL) {
        popsEventCancel(&g_pops->cdRespEvent.ev);
    }
    popsEventSchedule(&g_pops->cdRespEvent.ev, delayCycles);
}

/* 0x00b1f4: Compute mechanical CD-ROM head seek latency in PS1 cycles */
s32 popsCdComputeSeekCycles(u32 targetLba)
{
    s32 dist = __builtin_allegrex_max((s32)(g_pops->cdCurrentLba - targetLba),
                                      (s32)(targetLba - g_pops->cdCurrentLba));
    u32 speedFlags = g_pops->cdSpeedScale;
    s32 cycles;
    u32 rng;

    g_pops->cdCurrentLba = targetLba;
    if (dist >= 7000) {
        float root;
        float fDist = (float)(dist - 6000);
        __asm__("sqrt.s %0, %1" : "=f"(root) : "f"(fDist));
        cycles = 6000 * (s32)root + 1000000;
        if ((speedFlags & 2U) == 0U) {
            cycles *= 8;
        }
    } else {
        cycles = (s32)((500U * (u32)dist + 10000U) / 3U);
        if ((speedFlags & 2U) == 0U) {
            cycles *= 24;
        }
    }

    if ((speedFlags & 1U) != 0U) {
        if (dist == 0) {
            cycles = 0;
        }
    } else {
        s32 floorCycles = ((speedFlags & 0x20U) != 0U) ? 304819 : 1219276;
        if (cycles < floorCycles) {
            cycles = floorCycles;
        }
    }

    if ((g_pops->cdSpeedQuirkByte & 0x10U) != 0U) {
        cycles = __builtin_allegrex_min(1185408, cycles);
    }
    if (g_pops->cdSpinupEvent.next != NULL) {
        s32 spinRem = (s32)(g_pops->cdSpinupEvent.dueCycle -
                            (g_pops->cycleTarget - (u32)g_pops->cycleRemaining));
        cycles = __builtin_allegrex_max(cycles, spinRem);
    }

    rng = (1103515245U * g_pops->cdRngState + 12345U) & 0x7FFFFFFFU;
    g_pops->cdRngState = rng;
    cycles += (s32)(32U * (128U - (rng & 0x7FU)));
    if (targetLba == g_pops->cdSeekDelayBias) {
        cycles *= 24;
    }

    g_pops->cdSetlocPending = 0U;
    return (g_pops->cdInitialized == 0U) ? 1 : cycles;
}

/* 0x00b360: CD-ROM data / XA sector read streaming event callback */
void popsCdDataReadCallback(PopsEvent *ev)
{
    u8 mode = g_pops->cdModeReg;
    u32 lba = g_pops->cdCurrentLba;
    u8 *secBuf;
    u32 xaPrevLba;
    u32 nextDelay;
    u32 nextLba;

    (void)ev;
    g_pops->cdStatByte = (u8)((g_pops->cdStatByte & 0x9FU) | 0x20U);

    if ((mode & 0x40U) == 0U && (g_pops->cdSpeedQuirkByte & 8U) != 0U &&
        g_pops->cdRequestReg >= 0 && g_pops->cdBusyRetries < 3U) {
        g_pops->cdBusyRetries++;
        popsEventSchedule(&g_pops->cdReadEvent, ((s8)mode < 0) ? 112896 : 225792);
        return;
    }

    g_pops->cdBusyRetries = 0U;
    if (lba >= g_pops->cdCacheQuirk || (secBuf = popsDiscFetchSector(lba, 1)) == NULL) {
        g_pops->cdStatByte -= 0x20U;
        g_pops->cdErrorCode = 4U;
        g_pops->cdErrorStatusMask = 8U;
        popsCdEmitImmediateResp(5);
        g_pops->cdXaNextLba = 0U;
        return;
    }
    {
        const u32 *syncWords = (const u32 *)(const void *)secBuf;
        if (((syncWords[0] + 256U) | (syncWords[1] + 1U) | (syncWords[2] - 0xFFFFFFU)) != 0U) {
            g_pops->cdStatByte -= 0x20U;
            g_pops->cdErrorCode = 4U;
            g_pops->cdErrorStatusMask = 8U;
            popsCdEmitImmediateResp(5);
            g_pops->cdXaNextLba = 0U;
            return;
        }
    }

    g_pops->cdLastHeaderPtr = (u32)(uintptr_t)(secBuf + 12);
    xaPrevLba = g_pops->cdXaNextLba;

    if (secBuf[15] == 2U) {
        s32 isXaAudio = ((secBuf[18] & 0x44U) == 0x44U) && ((g_pops->cdModeReg & 0x40U) != 0U);
        if (!isXaAudio) {
            u32 slot = g_pops->cdBufWriteIdx;
            g_pops->cdSectorBufs[slot] = (u32)(uintptr_t)secBuf;
            g_pops->cdBufWriteIdx = (u8)(slot ^ 1U);

            if ((g_pops->cdStatusPort & 0x40U) == 0U) {
                g_pops->cdSectorOffset = ((g_pops->cdModeReg & 0x20U) != 0U) ? 12U : 24U;
                g_pops->cdSectorLimit = ((g_pops->cdModeReg & 0x30U) != 0U) ? 2352U : 2072U;
                g_pops->cdStatusPort |= 0x40U;
            }
            {
                s8 prevIrq = g_pops->cdRespEvent.irqCode;
                g_pops->cdRespEvent.respLen = 1U;
                g_pops->cdRespEvent.irqCode = 1;
                if (prevIrq < 0 && g_pops->cdRespEvent.ev.next != NULL) {
                    popsEventCancel(&g_pops->cdRespEvent.ev);
                }
                popsCdDeliverRespCallback(&g_pops->cdRespEvent.ev);
            }
        } else {
            u16 subHdr = (u16)((u32)secBuf[16] | ((u32)secBuf[17] << 8));
            if ((subHdr == g_pops->cdXaFilter || (g_pops->cdModeReg & 8U) == 0U) && lba >= xaPrevLba) {
                u32 interleave = 4U << (((s8)g_pops->cdModeReg < 0 ? 1U : 0U) +
                                        (((secBuf[19] ^ 1U) & 1U)) +
                                        (((secBuf[19] & 4U) != 0U) ? 1U : 0U));
                s32 sendXa = 1;
                if (g_pops->cdXaInterleave != 0U && g_pops->cdXaInterleave != interleave) {
                    g_pops->cdSubqIndex++;
                    if (g_pops->cdSubqIndex < 4U) {
                        popsDiscStopCddaTrack();
                        sendXa = 0;
                    }
                }
                if (sendXa) {
                    g_pops->cdXaInterleave = (u8)interleave;
                    if (g_pops->cdMuted == 0U) {
                        popsSpuQueueCddaSector((u32)(uintptr_t)secBuf, 2);
                    }
                    g_pops->cdSubqIndex = 0U;
                }
                g_pops->cdXaNextLba = lba + g_pops->cdXaInterleave;
            }
        }
    }

    mode = g_pops->cdModeReg;
    if ((mode & 0x40U) == 0U) {
        nextDelay = ((g_pops->cdSpeedQuirkByte & 8U) != 0U) ? 225792U : 451584U;
    } else if (g_pops->cdMuted != 0U) {
        nextDelay = 451584U;
    } else {
        s32 spuDelay = (s32)g_pops->spuDelayQuirk;
        s32 queued = popsSpuGetCddaQueuedCount();
        u32 xaNext = g_pops->cdXaNextLba;

        nextDelay = 451584U;
        if (xaNext != 0U && (queued < spuDelay - 2 || xaPrevLba == 0U)) {
            nextDelay = (g_pops->cdXaInterleave == 4U) ? 338688U : 225792U;
        }
        if (xaNext != 0U && queued < 2) {
            u8 *ahead = popsDiscFetchSector(xaNext, 0);
            s32 queuedAhead = 0;
            if (ahead != NULL) {
                const u32 *aheadWords = (const u32 *)(const void *)ahead;
                if (((aheadWords[0] + 256U) | (aheadWords[1] + 1U) | (aheadWords[2] - 0xFFFFFFU)) == 0U &&
                    (ahead[18] & 0x44U) == 0x44U) {
                    u16 aheadHdr = (u16)((u32)ahead[16] | ((u32)ahead[17] << 8));
                    if (aheadHdr == g_pops->cdXaFilter || (g_pops->cdModeReg & 8U) == 0U) {
                        u32 ilv = 4U << (((s8)g_pops->cdModeReg < 0 ? 1U : 0U) +
                                         (((ahead[19] ^ 1U) & 1U)) +
                                         (((ahead[19] & 4U) != 0U) ? 1U : 0U));
                        if (g_pops->cdXaInterleave == 0U || g_pops->cdXaInterleave == ilv) {
                            g_pops->cdXaInterleave = (u8)ilv;
                            popsSpuQueueCddaSector((u32)(uintptr_t)ahead, 2);
                            g_pops->cdXaNextLba += g_pops->cdXaInterleave;
                            queuedAhead = 1;
                        }
                    }
                }
            }
            if (!queuedAhead && spuDelay != 0) {
                nextDelay = 423360U;
            }
        }
    }

    mode = g_pops->cdModeReg;
    nextDelay >>= (((s8)mode < 0) ? 1U : 0U);
    if ((g_pops->cdSpeedScale & 0x200U) != 0U && g_pops->cdSetlocPending != 0U &&
        lba != g_pops->cdTargetLba - 1U) {
        lba = g_pops->cdTargetLba - 1U;
        nextDelay = (u32)popsCdComputeSeekCycles(lba);
        g_pops->cdXaInterleave = 0U;
        g_pops->cdXaNextLba = 0U;
    }

    nextLba = lba + 1U;
    g_pops->cdCurrentLba = nextLba;
    popsEventSchedule(&g_pops->cdReadEvent, (s32)nextDelay);

    if (g_pops->cdIrqFlags == 0U && g_pops->cdQueuedCmd >= 0 && g_pops->cdCmdEvent.ev.next == NULL) {
        popsCdExecCommand((u32)g_pops->cdQueuedCmd);
    }
}

/* 0x00b81c: CD-DA audio playback streaming event callback */
void popsCdAudioPlayCallback(PopsEvent *ev)
{
    u32 lba = g_pops->cdCurrentLba;
    s32 stepCycles = 451584;
    u32 track;
    u8 *secBuf;
    u8 mode;
    u32 subqStep;

    (void)ev;
    if (lba >= g_pops->cdCacheQuirk) {
        if ((g_pops->cdModeReg & 2U) == 0U) {
            g_pops->cdMuted = 1U;
            g_pops->cdStatByte &= (u8)~2U;
            g_pops->cdTargetLba = 0U;
            g_pops->cdCurrentLba = 0U;
        }
        g_pops->cdCurrentTrack = 0U;
        g_pops->cdStatByte &= 0x17U;
        popsCdEmitImmediateResp(4);
        return;
    }

    track = popsCdFindTrackForLba(lba);
    if (track != g_pops->cdCurrentTrack) {
        if ((g_pops->cdModeReg & 2U) != 0U) {
            g_pops->cdCurrentTrack = 0U;
            g_pops->cdStatByte &= 0x17U;
            popsCdEmitImmediateResp(4);
            return;
        }
        if (lba >= (u32)(popsMsfToLba(popsGetTrackMsf(track)) + 75)) {
            g_pops->cdCurrentTrack = (u8)track;
            popsDiscStartCddaTrack((s32)track, lba);
        }
    }

    secBuf = popsDiscFetchSector(lba, 1);
    mode = g_pops->cdModeReg;
    if ((mode & 1U) != 0U && track != 1U && g_pops->cdMuted == 0U && secBuf != NULL) {
        s32 queued;
        popsSpuQueueCddaSector((u32)(uintptr_t)secBuf, 1);
        queued = popsSpuGetCddaQueuedCount();
        if (queued < 4) {
            stepCycles = 225792;
        } else if (queued >= 7) {
            stepCycles = 564479;
        }
        mode = g_pops->cdModeReg;
    }

    g_pops->cdStatByte = (u8)((g_pops->cdStatByte & 0x3FU) | 0x80U);
    subqStep = (lba % 75U) / 10U;

    if ((mode & 4U) != 0U && g_pops->cdSubqIndex != subqStep) {
        s32 skipReport = ((g_pops->quirkFlags & 0x400U) != 0U &&
                          lba + 150U < (u32)popsMsfToLba(popsGetTrackMsf(g_pops->cdCurrentTrack)));
        if (!skipReport) {
            s8 prevIrq = g_pops->cdRespEvent.irqCode;
            u16 peakWord;

            g_pops->cdRespEvent.irqCode = 1;
            g_pops->cdRespEvent.respLen = 8U;
            if (prevIrq < 0 && g_pops->cdRespEvent.ev.next != NULL) {
                popsEventCancel(&g_pops->cdRespEvent.ev);
            }
            g_pops->cdSubqIndex = (u8)subqStep;
            popsCdFillTrackRelMsf(lba, &g_pops->cdRespEvent.respData[1]);
            g_pops->cdRespEvent.respData[4] |= 0x80U;
            if ((subqStep & 1U) == 0U) {
                u32 absSec = (lba + 150U) / 75U;
                g_pops->cdRespEvent.respData[3] = popsToBcd(absSec / 60U);
                g_pops->cdRespEvent.respData[4] = popsToBcd(absSec % 60U);
                g_pops->cdRespEvent.respData[5] = popsToBcd((lba + 150U) % 75U);
            }
            peakWord = (u16)(lba & 1U);
            if (secBuf != NULL) {
                const u16 *samples = (const u16 *)(const void *)secBuf;
                peakWord |= (u16)(samples[peakWord] & 0xFFFEU);
            }
            g_pops->cdRespEvent.respData[6] = (u8)(peakWord & 0xFFU);
            g_pops->cdRespEvent.respData[7] = (u8)(peakWord >> 8);
            popsCdDeliverRespCallback(&g_pops->cdRespEvent.ev);
        }
    }

    g_pops->cdCurrentLba = lba + 1U;
    popsEventSchedule(&g_pops->cdReadEvent, stepCycles);
    if (g_pops->cdIrqFlags == 0U && g_pops->cdQueuedCmd >= 0 && g_pops->cdCmdEvent.ev.next == NULL) {
        popsCdExecCommand((u32)g_pops->cdQueuedCmd);
    }
}

/* 0x00bbcc: CD-ROM motor spin-up completion callback */
void popsCdSpinupCallback(PopsEvent *ev)
{
    (void)ev;
    g_pops->cdTrayState = 0U;
    g_pops->cdStatByte |= 2U;
    g_pops->cdSpinupState = 0U;
}

/* 0x00bbe4: PS1 DMA3 (CD-ROM sector buffer -> Main RAM) transfer */
s32 popsCdDmaTransfer(u32 addr, u32 bytes, u32 chcr)
{
    u32 secOff = g_pops->cdSectorOffset;
    u32 endOff = secOff + bytes;
    u32 ramDst = (addr & 0x1FFFFFU) | POPS_PSX_RAM_BASE;
    const u8 *srcBase = (const u8 *)(uintptr_t)g_pops->cdSectorBufs[g_pops->cdBufReadIdx];
    u32 limit;
    u32 copyLen;
    u32 *dstWords;
    const u32 *srcWords;
    u32 wordsRem;

    if ((chcr & 0x10000000U) == 0U) {
        return (s32)bytes;
    }

    /* Invalidate JIT translation lookup pointers for affected 64 KB PS1 RAM pages */
    if (endOff >= 25U) {
        u32 pageMask = g_pops->jitDirtyPageMask >> ((ramDst >> 16) & 31U);
        u32 nextPage = (ramDst | 0xFFFFU) + 1U;
        u32 invStart = ((pageMask & 1U) != 0U) ? ramDst : nextPage;
        u32 invEnd = ramDst + bytes;
        s32 invLen;

        if (nextPage < invEnd && (pageMask & 2U) == 0U) {
            invEnd = nextPage;
        }
        invLen = (s32)(invEnd - invStart);
        if (invLen > 0) {
            u32 *jitCur = (u32 *)(uintptr_t)(invStart + 0x400000U);
            u32 *jitEnd = (u32 *)(uintptr_t)(invEnd + 0x400000U);
            while (jitCur != jitEnd) {
                *jitCur++ = 0U;
            }
        }
    }

    limit = g_pops->cdSectorLimit;
    if (endOff >= limit) {
        endOff = limit;
        g_pops->cdStatusPort &= (u8)~0x40U;
        g_pops->cdRequestReg &= 0x7F;
    }

    copyLen = endOff - secOff;
    g_pops->cdSectorOffset = (u16)endOff;

    dstWords = (u32 *)(uintptr_t)ramDst;
    srcWords = (const u32 *)(const void *)(srcBase + secOff);
    wordsRem = copyLen >> 2;
    while (wordsRem != 0U) {
        *dstWords++ = *srcWords++;
        wordsRem--;
    }

    if (copyLen != bytes) {
        u8 fillByte = ((const u8 *)(uintptr_t)ramDst)[copyLen - 1U];
        sceKernelMemset((void *)(uintptr_t)(ramDst + copyLen), fillByte, bytes - copyLen);
    }

    return (s32)bytes;
}

/* 0x00be54: PS1 CD-ROM MMIO read (0x1F801800..0x1F801803) */
u32 popsCdReadReg(u32 addr, u32 width)
{
    u32 port = addr & 0xFU;
    u32 val = 0U;
    s32 signedVal = 0;

    switch (port) {
    case 0U:
        val = g_pops->cdStatusPort;
        if ((g_pops->cdSpeedScale & 0x400U) != 0U) {
            if (g_pops->cdRequestReg >= 0) {
                val &= ~0x40U;
            }
        } else if (g_pops->cdRequestReg < 0) {
            val |= 0x40U;
        }
        if (g_pops->cdXaNextLba != 0U) {
            val |= 4U;
        }
        if (g_pops->cdParamCount < 4U) {
            if (g_pops->cdParamCount == 0U) {
                val |= 8U;
            }
            val |= 0x10U;
        }
        signedVal = (s32)(s8)val;
        break;

    case 1U:
        if ((g_pops->cdStatusPort & 0x20U) != 0U) {
            u8 readIdx = g_pops->cdRespReadIdx;
            val = g_pops->cdRespFifo[readIdx];
            g_pops->cdRespReadIdx = (u8)(readIdx + 1U);
            if (g_pops->cdRespReadIdx == g_pops->cdRespCount) {
                g_pops->cdStatusPort -= 0x20U;
            }
            signedVal = (s32)(s8)val;
        }
        break;

    case 2U: {
        u32 nextOff = (u32)g_pops->cdSectorOffset + 1U;
        const u8 *secBuf = (const u8 *)(uintptr_t)g_pops->cdSectorBufs[g_pops->cdBufReadIdx];
        if (nextOff >= g_pops->cdSectorLimit) {
            nextOff = g_pops->cdSectorLimit;
            g_pops->cdStatusPort &= (u8)~0x40U;
            g_pops->cdRequestReg &= 0x7F;
        }
        g_pops->cdSectorOffset = (u16)nextOff;
        val = secBuf[nextOff - 1U];
        signedVal = (s32)(s8)val;
        break;
    }

    case 3U:
        if ((g_pops->cdStatusPort & 3U) == 1U) {
            val = g_pops->cdIrqFlags | 0xE0U;
            signedVal = (s32)(s8)val;
        } else if ((g_pops->cdStatusPort & 3U) == 0U) {
            val = g_pops->cdIrqEnable;
            signedVal = (s32)(s8)val;
        }
        break;

    default:
        break;
    }

    return (width == 0U) ? (u32)signedVal : val;
}

/* 0x00bf94: PS1 CD-ROM MMIO write (0x1F801800..0x1F801803) */
void popsCdWriteReg(u32 addr, u32 value, u32 width)
{
    u32 port = addr & 0xFU;
    u8 byteVal = (u8)value;
    u32 indexPort;

    (void)width;
    if (port >= 4U) {
        return;
    }

    indexPort = port + 4U * (g_pops->cdStatusPort & 3U);
    switch (indexPort) {
    case 0U:
    case 4U:
    case 8U:
    case 12U:
        g_pops->cdStatusPort = (u8)((g_pops->cdStatusPort & 0xFCU) | (byteVal & 3U));
        break;

    case 1U: {
        u32 cmd = (byteVal < 0x1FU) ? byteVal : 0U;
        if (g_pops->cdCmdLock == 0U) {
            if ((g_pops->cdIrqFlags | (u8)g_pops->cdCmdEvent.irqCode) != 0U) {
                g_pops->cdQueuedCmd = (s8)cmd;
            } else if (g_pops->cdReadEvent.next != NULL &&
                       (s32)(g_pops->cdReadEvent.dueCycle -
                             (g_pops->cycleTarget - (u32)g_pops->cycleRemaining)) <= 50802) {
                g_pops->cdQueuedCmd = (s8)cmd;
            } else {
                popsCdExecCommand(cmd);
            }
        }
        break;
    }

    case 2U:
        if (g_pops->cdCmdLock == 0U && g_pops->cdParamCount < 4U) {
            g_pops->cdParamFifo[g_pops->cdParamCount++] = byteVal;
        }
        break;

    case 3U:
        if ((byteVal & 0x80U) != 0U) {
            g_pops->cdBufReadIdx = (u8)(g_pops->cdBufWriteIdx ^ 1U);
            if (g_pops->cdRequestReg >= 0) {
                g_pops->cdSectorOffset = ((g_pops->cdModeReg & 0x20U) != 0U) ? 12U : 24U;
                g_pops->cdSectorLimit = ((g_pops->cdModeReg & 0x30U) != 0U) ? 2352U : 2072U;
            }
        }
        g_pops->cdRequestReg = (s8)byteVal;
        break;

    case 6U:
        g_pops->cdIrqEnable = byteVal;
        if ((g_pops->cdIrqFlags & byteVal) != 0U) {
            popsIrqRaise(4U);
        }
        break;

    case 7U:
        if ((byteVal & 0x40U) != 0U) {
            g_pops->cdParamCount = 0U;
        }
        if ((byteVal & 0x20U) != 0U) {
            popsDiscStopCddaTrack();
            g_pops->cdXaNextLba = 0U;
            g_pops->cdXaInterleave = 0U;
        }
        g_pops->cdIrqFlags &= (u8)~byteVal;
        if (g_pops->cdIrqFlags != 0U) {
            break;
        }
        if (g_pops->cdRespEvent.ev.next == NULL && g_pops->cdRespEvent.irqCode != 0) {
            g_pops->cdRespEvent.ev.callback(&g_pops->cdRespEvent.ev);
            break;
        }
        if (g_pops->cdCmdEvent.ev.next == NULL) {
            if (g_pops->cdCmdEvent.irqCode != 0) {
                g_pops->cdCmdEvent.ev.callback(&g_pops->cdCmdEvent.ev);
            } else if (g_pops->cdQueuedCmd >= 0) {
                popsCdExecCommand((u32)g_pops->cdQueuedCmd);
            }
        }
        break;

    case 10U:
    case 11U:
        g_pops->cdAudioVol.bytes[indexPort - 10U] = byteVal;
        break;

    case 13U:
    case 14U:
        g_pops->cdAudioVol.bytes[indexPort - 11U] = byteVal;
        break;

    case 15U:
        if ((byteVal & 0x20U) != 0U && g_pops->cdMuted == 0U) {
            popsGpuThrottleSpeed();
            POPS_ME_SPU_SHARED->cdAudioVol = g_pops->cdAudioVol.word;
        }
        break;

    default:
        break;
    }
}

/* 0x00c204: Queue asynchronous disc chunk prefetch in the 17-slot LRU cache */
void popsDiscPrefetch(u32 lba)
{
    u32 reqLba = lba;
    PopsDiscCacheSlot *head;
    PopsDiscCacheSlot *slot;

    if (lba >= g_pops->cdCacheQuirk || g_pops->discPendingLba != -1) {
        return;
    }

    if (g_pops->discCddaActive != 0U) {
        u32 ringSlot = (u32)(g_pops->discCddaPcmPos + 4096) / 37632U;
        if ((s32)g_pops->discCache[ringSlot].baseLba >= g_pops->discCddaReadLba - 32) {
            return;
        }
        g_pops->discPendingLba = (s32)reqLba;
        sceKernelSetEventFlag(g_pops->discEventFlag, 1U);
        return;
    }

    if (reqLba >= (u32)(popsMsfToLba(POPS_ISO_TOC_TRACK2_MSF) - 150) &&
        popsGetCddaTrack(2)->endOffset != 0U) {
        return;
    }
    reqLba &= ~0xFU;

    if (g_pops->discPendingLba == (s32)reqLba) {
        return;
    }

    head = g_pops->discCacheHead;
    slot = head;
    for (;;) {
        if ((reqLba - slot->baseLba) < 16U) {
            break;
        }
        slot = slot->next;
        if (slot == head) {
            slot = NULL;
            break;
        }
    }

    if (slot != NULL) {
        /* Move hit slot to MRU head */
        PopsDiscCacheSlot *oldTail = head->prev;
        slot->next->prev = slot->prev;
        slot->prev->next = slot->next;
        slot->prev = oldTail;
        slot->next = oldTail->next;
        slot->next->prev = slot;
        oldTail->next = slot;
        g_pops->discCacheHead = slot;

        /* Check up to 2 chunks ahead for read-ahead prefetch */
        reqLba += 16U;
        if (reqLba < g_pops->cdCacheQuirk) {
            PopsDiscCacheSlot *probe = slot;
            s32 found = 0;
            for (;;) {
                if ((reqLba - probe->baseLba) < 16U) {
                    found = 1;
                    break;
                }
                probe = probe->next;
                if (probe == slot) {
                    break;
                }
            }
            if (!found) {
                g_pops->discPendingLba = (s32)reqLba;
                sceKernelSetEventFlag(g_pops->discEventFlag, 1U);
                return;
            }
        }
        reqLba += 16U;
        if (reqLba < g_pops->cdCacheQuirk) {
            PopsDiscCacheSlot *headNow = g_pops->discCacheHead;
            PopsDiscCacheSlot *probe = headNow;
            s32 found = 0;
            for (;;) {
                if ((reqLba - probe->baseLba) < 16U) {
                    found = 1;
                    break;
                }
                probe = probe->next;
                if (probe == headNow) {
                    break;
                }
            }
            if (!found) {
                g_pops->discPendingLba = (s32)reqLba;
                sceKernelSetEventFlag(g_pops->discEventFlag, 1U);
            }
        }
        return;
    }

    g_pops->discPendingLba = (s32)reqLba;
    sceKernelSetEventFlag(g_pops->discEventFlag, 1U);
}

/* 0x00c3b4: Fetch a 2352-byte CD sector from cache (or wait for cdworker to load it) */
u8 *popsDiscFetchSector(u32 lba, s32 blocking)
{
    PopsDiscCacheSlot *head = g_pops->discCacheHead;
    PopsDiscCacheSlot *slot = head;

    for (;;) {
        if ((lba - slot->baseLba) < 16U) {
            break;
        }
        slot = slot->next;
        if (slot == head) {
            slot = NULL;
            break;
        }
    }

    if (slot == NULL) {
        if (!blocking) {
            return NULL;
        }
        if (g_pops->discCddaActive != 0U) {
            u32 trkStart = (u32)popsMsfToLba(popsGetTrackMsf(g_pops->discCddaActive));
            if (lba >= trkStart - 150U && lba < trkStart) {
                popsDiscPrefetch(trkStart);
            }
            return NULL;
        }

        for (;;) {
            popsDiscWaitWorkerIdle();
            popsDiscPrefetch(lba & ~0xFU);
            if (g_pops->discPendingLba == (s32)(lba & ~0xFU)) {
                popsDiscWaitWorkerIdle();
            }

            head = g_pops->discCacheHead;
            slot = head;
            for (;;) {
                if ((lba - slot->baseLba) < 16U) {
                    break;
                }
                slot = slot->next;
                if (slot == head) {
                    slot = NULL;
                    break;
                }
            }
            if (slot != NULL) {
                break;
            }
            if (lba >= (u32)popsMsfToLba(POPS_ISO_TOC_TRACK2_MSF)) {
                return NULL;
            }
            g_pops->discReadRetries++;
            if (g_pops->discReadRetries >= 3U) {
                scePopsMan_0090B2C8((s32)0x80010005U);
                return NULL;
            }
            popsHandleDiscError(1);
        }
    }

    if (g_pops->discCddaActive != 0U) {
        g_pops->discCddaReadLba = (s32)lba;
    }

    {
        PopsDiscCacheSlot *oldTail = head->prev;
        u32 relIdx = lba - slot->baseLba;
        u16 isoFlags = popsGetIsoIndex(lba)->flags;
        u8 *sectorPtr;

        slot->next->prev = slot->prev;
        slot->prev->next = slot->next;
        slot->prev = oldTail;
        slot->next = oldTail->next;
        slot->next->prev = slot;
        oldTail->next = slot;
        g_pops->discCacheHead = slot;

        if (isoFlags != 0U) {
            u32 absSec = (lba + 150U) / 75U;
            if (isoFlags == 2U) {
                u32 rev = 15U - relIdx;
                __asm__("bitrev %0, %1" : "=r"(rev) : "r"(rev));
                relIdx = rev >> 28;
            }
            sectorPtr = (u8 *)(uintptr_t)(slot->bufAddr + 2352U * relIdx);
            {
                u32 *syncOut = (u32 *)(void *)sectorPtr;
                syncOut[0] = 0xFFFFFF00U;
                syncOut[1] = 0xFFFFFFFFU;
                syncOut[2] = 0x00FFFFFFU;
            }
            sectorPtr[12] = popsToBcd(absSec / 60U);
            sectorPtr[13] = popsToBcd(absSec % 60U);
            sectorPtr[14] = popsToBcd((lba + 150U) % 75U);
        } else {
            sectorPtr = (u8 *)(uintptr_t)(slot->bufAddr + 2352U * relIdx);
        }

        popsDiscPrefetch(lba + 16U);
        g_pops->discReadRetries = 0U;
        return sectorPtr;
    }
}

/* 0x00c6a4: Begin ATRAC3 CD-DA track streaming into the disc cache ring */
void popsDiscStartCddaTrack(s32 trackNum, u32 startLba)
{
    const PopsCddaTrackEntry *entry;
    u32 trackStart;
    u32 alignedLba;
    u32 fileOff;
    s32 frameDelta;
    u32 cryptSeed;
    u32 idx;

    if (g_pops->discCddaActive != 0U) {
        g_pops->discCddaTrack = 0;
        popsDiscWaitWorkerIdle();
        g_pops->discCddaActive = 0U;
    }
    popsGpuThrottleSpeed();
    POPS_ME_SPU_SHARED->cddaCtrl.parts.cddaMode = -1;

    if (trackNum < 2) {
        popsDiscPrefetch(startLba);
        return;
    }

    entry = popsGetCddaTrack(trackNum);
    if (entry->endOffset == 0U) {
        popsDiscPrefetch(startLba);
        return;
    }

    for (idx = 0U; idx < 17U; idx++) {
        g_pops->discCache[idx].baseLba = 0x80000000U;
    }

    g_pops->discCddaActive = 1U;
    g_pops->discLastReadOff = -1;
    g_pops->discCddaTrack = trackNum;

    trackStart = (u32)popsMsfToLba(popsGetTrackMsf((u32)trackNum));
    alignedLba = (u32)__builtin_allegrex_max((s32)startLba, (s32)trackStart);
    fileOff = entry->startOffset;
    frameDelta = 588 * (s32)(alignedLba - trackStart) - 1162;
    cryptSeed = entry->cryptSeed;

    if (frameDelta > 0) {
        u32 frames = (u32)frameDelta >> 10;
        fileOff += 384U * frames;
        cryptSeed = popsRotr32(cryptSeed, frames & 31U);
    }

    g_pops->discCddaReadLba = (s32)(alignedLba - 1U);
    g_pops->discCddaFileOff = fileOff;
    g_pops->discCddaCryptKey = cryptSeed;
    g_pops->discCddaWriteLba = alignedLba;
    g_pops->discCddaUnused = 0U;
    g_pops->discCddaPcmPos = 0;
    g_pops->discCddaStreamPos = -1;

    if (g_pops->discPendingLba == -1) {
        g_pops->discPendingLba = (s32)alignedLba;
        sceKernelSetEventFlag(g_pops->discEventFlag, 1U);
    } else if (alignedLba >= trackStart) {
        popsDiscWaitWorkerIdle();
        g_pops->discPendingLba = (s32)alignedLba;
        sceKernelSetEventFlag(g_pops->discEventFlag, 1U);
    }
}

/* 0x00c818: Stop active ATRAC3 CD-DA background decode */
void popsDiscStopCddaTrack(void)
{
    if (g_pops->discCddaActive != 0U) {
        g_pops->discCddaTrack = 0;
        popsDiscWaitWorkerIdle();
        g_pops->discCddaActive = 0U;
    }
    popsGpuThrottleSpeed();
    POPS_ME_SPU_SHARED->cddaCtrl.parts.cddaMode = -1;
}

/* 0x00c860: Background CD-ROM / PBP / ATRAC3 worker thread ("cdworker") */
int popsDiscWorkerThread(SceSize args, void *argp)
{
    (void)args;
    (void)argp;

    for (;;) {
        u32 reqLba;
        PopsDiscCacheSlot *head;
        PopsDiscCacheSlot *probe;
        s32 alreadyCached;
        PopsDiscCacheSlot *victim;
        const PopsIsoIndexEntry *isoIdx;
        s32 readBytes;
        s32 status = -1;
        u32 irqState;

        sceKernelWaitEventFlagCB(g_pops->discEventFlag, 1U, 33U, NULL, NULL);
        reqLba = (u32)g_pops->discPendingLba;

        if (g_pops->discCddaActive != 0U) {
            s32 streamPos = g_pops->discCddaStreamPos;
            s32 activeTrack = g_pops->discCddaTrack;

            if ((u32)streamPos > 0xDC7FU) {
                if (activeTrack >= 2) {
                    s32 remBytes = (s32)(popsGetCddaTrack(activeTrack)->endOffset -
                                         g_pops->discCddaFileOff);
                    s32 ready = 1;
                    if (streamPos < 0) {
                        ready = 0;
                        if (popsDiscReadCddaStream((u32 *)(uintptr_t)POPS_CD_RAW_STAGE_BUF, 384U) >= 0) {
                            popsAtracInitDecoder(2, 0, 192, 0);
                            popsAtracDecodeFrame((const u8 *)(uintptr_t)POPS_CD_RAW_STAGE_BUF,
                                                 (float **)(void *)&g_pops->discAtracOutLeft, NULL);
                            ready = 1;
                        }
                    }
                    if (ready && remBytes > 0) {
                        u32 chunkBytes = (u32)__builtin_allegrex_min(56448, remBytes);
                        if (g_pops->discCddaTrack >= 2 &&
                            popsDiscReadCddaStream((u32 *)(uintptr_t)POPS_CD_RAW_STAGE_BUF, chunkBytes) >= 0) {
                            g_pops->discCddaStreamPos = 0;
                            streamPos = 0;
                        }
                    }
                }
            }

            if ((u32)streamPos <= 0xDC7FU) {
                while (g_pops->discCddaTrack > 0) {
                    s32 pcmPos = g_pops->discCddaPcmPos;
                    s32 pcmNext = pcmPos + 4096;
                    u32 ringSlot = (u32)pcmNext / 37632U;

                    if ((s32)g_pops->discCache[ringSlot].baseLba >= g_pops->discCddaReadLba - 32) {
                        break;
                    }
                    if (popsAtracDecodeFrame((const u8 *)(uintptr_t)(POPS_CD_RAW_STAGE_BUF + (u32)g_pops->discCddaStreamPos),
                                             (float **)(void *)&g_pops->discAtracOutLeft, NULL) != 384) {
                        break;
                    }

                    {
                        u32 dstAddr = POPS_CDDA_PCM_RING_BASE + (u32)pcmPos;
                        u32 *dstWords = (u32 *)(uintptr_t)dstAddr;
                        const s32 *leftSamples = g_pops->discAtracOutLeft;
                        const s32 *rightSamples = g_pops->discAtracOutRight;
                        u32 sampleIdx;

                        {
                            u32 curAddr = dstAddr;
                            u32 endAddr = dstAddr + 4096U;
                            u32 alignedEnd;
                            if ((curAddr & 0x3FU) != 0U) {
                                __builtin_allegrex_cache(0x1B, (void *)(uintptr_t)curAddr);
                                curAddr = (curAddr & ~0x3FU) + 64U;
                            }
                            if ((endAddr & 0x3FU) != 0U) {
                                __builtin_allegrex_cache(0x1B, (void *)(uintptr_t)endAddr);
                            }
                            alignedEnd = endAddr & ~0x3FU;
                            if (((s32)(endAddr - curAddr) >> 17) != 0) {
                                sceKernelDcacheInvalidateRangeForUser((const void *)(uintptr_t)curAddr, alignedEnd - curAddr);
                            } else {
                                while (curAddr != alignedEnd) {
                                    __builtin_allegrex_cache(0x19, (void *)(uintptr_t)curAddr);
                                    curAddr += 64U;
                                }
                            }
                        }
                        for (sampleIdx = 0U; sampleIdx < 1024U; sampleIdx++) {
                            s32 sampleL = __builtin_allegrex_min(
                                __builtin_allegrex_max(leftSamples[sampleIdx], -32768), 32767);
                            s32 sampleR = __builtin_allegrex_min(
                                __builtin_allegrex_max(rightSamples[sampleIdx], -32768), 32767);
                            dstWords[sampleIdx] = ((u32)(u16)sampleL) | ((u32)(u16)sampleR << 16);
                        }
                    }

                    g_pops->discCddaPcmPos = pcmNext;
                    g_pops->discCddaStreamPos += 384;

                    {
                        s32 slotBoundary = (s32)(37632U * ringSlot);
                        if (pcmPos < slotBoundary && pcmNext >= slotBoundary) {
                            g_pops->discCache[ringSlot - 1U].baseLba = g_pops->discCddaWriteLba;
                            g_pops->discCddaWriteLba += 16U;
                            if ((u32)pcmNext > 0x92FFFU) {
                                g_pops->discCddaPcmPos = 0;
                                break;
                            }
                        }
                    }
                }
            }

            irqState = popsIrqDisable();
            g_pops->discPendingLba = -1;
            sceKernelSetEventFlag(g_pops->discEventFlag, 2U);
            popsIrqRestore(irqState);
            continue;
        }

        head = g_pops->discCacheHead;
        probe = head;
        alreadyCached = 0;
        for (;;) {
            if ((reqLba - probe->baseLba) < 16U) {
                alreadyCached = 1;
                break;
            }
            probe = probe->next;
            if (probe == head) {
                break;
            }
        }
        if (alreadyCached) {
            continue;
        }

        irqState = popsIrqDisable();
        victim = g_pops->discCacheHead->prev;
        victim->baseLba = 0x80000000U;
        popsIrqRestore(irqState);

        isoIdx = popsGetIsoIndex(reqLba);
        readBytes = isoIdx->readBytes;

        if (readBytes == 0) {
            u32 *dstWords = (u32 *)(uintptr_t)victim->bufAddr;
            u32 wordCount = 37632U / 4U;
            while (wordCount-- != 0U) {
                *dstWords++ = 0U;
            }
            status = 0;
        } else if (readBytes < 0) {
            u32 rawLen = (u32)sceMeAudio_59759606(g_pops->discFileFd,
                                                  (void *)(uintptr_t)victim->bufAddr,
                                                  isoIdx->fileOffset);
            status = ((rawLen ^ 0x9300U) == 0U) ? 0 : -1;
        } else {
            u32 alignOff = isoIdx->fileOffset & 0x1FFU;
            u32 alignedPos = isoIdx->fileOffset - alignOff;
            u32 stageAddr = POPS_CD_RAW_STAGE_BUF;
            s32 needRead = 1;

            if ((s32)alignedPos == g_pops->discLastReadOff) {
                u32 prevEnd = g_pops->discReadBufEnd;
                readBytes -= 512;
                alignOff -= 512U;
                stageAddr = POPS_CD_RAW_STAGE_BUF + prevEnd;
                if (readBytes == 0) {
                    needRead = 0;
                } else {
                    alignedPos += 512U;
                    if (prevEnd + (u32)readBytes > 0x12600U) {
                        sceKernelMemcpy((void *)(uintptr_t)POPS_CD_RAW_STAGE_BUF,
                                        (const void *)(uintptr_t)(POPS_CD_RAW_STAGE_BUF + prevEnd - 512U),
                                        512U);
                        stageAddr = POPS_CD_RAW_STAGE_BUF + 512U;
                    }
                }
            }

            if (needRead) {
                s32 got;
                g_pops->discReadBufEnd = (stageAddr + (u32)readBytes) - POPS_CD_RAW_STAGE_BUF;
                g_pops->discLastReadOff = (s32)(alignedPos + (u32)readBytes - 512U);
                got = sceMeAudio_A651EA7B(g_pops->discFileFd, (void *)(uintptr_t)stageAddr,
                                          alignedPos, (u32)readBytes);
                if (got != readBytes) {
                    g_pops->discLastReadOff = -1;
                    if (got < 0) {
                        status = -1;
                        needRead = -1;
                    }
                }
            }

            if (needRead >= 0) {
                u32 srcAddr = stageAddr + alignOff;
                s32 verifyOk = 1;
                if ((s32)(stageAddr - POPS_CD_RAW_STAGE_BUF) >= 512 &&
                    popsMcIsWritebackIdle() == 0 && popsSpuGetCddaPending() > 0) {
                    u32 curTick = POPS_ME_SPU_SHARED->sampleCounter;
                    if ((curTick - g_pops->discLastVerifyTick) >= 0x33AFU) {
                        if (sceMeAudio_6FCDD82D((const void *)(uintptr_t)srcAddr,
                                                isoIdx->shaDigest,
                                                isoIdx->compressedSize) == 0) {
                            verifyOk = 0;
                        }
                    }
                    g_pops->discLastVerifyTick = curTick;
                }
                if (verifyOk) {
                    s32 decoded = popsRangeDecodeBlock(37632,
                                                       (const u8 *)(uintptr_t)srcAddr,
                                                       (u8 *)(uintptr_t)victim->bufAddr);
                    status = (((u32)decoded ^ 0x92FFU) == 0U) ? 0 : -1;
                }
            }
        }

        irqState = popsIrqDisable();
        if (status >= 0) {
            victim->baseLba = reqLba;
            g_pops->discCacheHead = victim;
        }
        g_pops->discPendingLba = -1;
        sceKernelSetEventFlag(g_pops->discEventFlag, 2U);
        popsIrqRestore(irqState);
    }
}

/* 0x00cd1c: Wait until the background cdworker thread finishes its active request */
void popsDiscWaitWorkerIdle(void)
{
    if (g_pops->discPendingLba != -1) {
        do {
            sceKernelWaitEventFlagCB(g_pops->discEventFlag, 2U, 33U, NULL, NULL);
        } while (g_pops->discPendingLba != -1);
    }
}

/* 0x00cd6c: Read and descramble ATRAC3 CD-DA stream bytes from PBP */
s32 popsDiscReadCddaStream(u32 *dstBuf, u32 byteLen)
{
    u32 fileOff = g_pops->discCddaFileOff;
    u32 got = (u32)sceMeAudio_A651EA7B(g_pops->discFileFd, dstBuf, fileOff, byteLen);
    u32 cryptKey;
    u32 *curWord = dstBuf;
    u32 *endWord;

    if (byteLen < got) {
        return -1;
    }

    cryptKey = g_pops->discCddaCryptKey;
    g_pops->discCddaFileOff = fileOff + byteLen;
    endWord = dstBuf + (got >> 2);

    if (cryptKey != 0U) {
        while (curWord < endWord) {
            u32 key = cryptKey;
            u32 wordsRem = 96U;
            while (wordsRem-- != 0U) {
                u32 cipher = *curWord;
                *curWord = cipher ^ key;
                key = cryptKey + 123456789U * cipher;
                curWord++;
            }
            cryptKey = popsRotr32(cryptKey, 1U);
        }
        g_pops->discCddaCryptKey = cryptKey;
    }

    if (got != byteLen) {
        u32 *zeroEnd = dstBuf + (byteLen >> 2);
        while (endWord != zeroEnd) {
            *endWord++ = 0U;
        }
    }

    return 0;
}
