#include "pops_internal.h"

/* MDEC DMA0 (macroblock input) and quantization table loader (0x00e378) */
s32 popsMdecDmaIn(u32 addr, u32 bytes, u32 chcr)
{
    u32 cmd = g_pops->mdecStatus;
    u32 ramAddr = (addr & 0x1FFFFFU) | POPS_PSX_RAM_BASE;
    const u8 *ramBytes = (const u8 *)(uintptr_t)ramAddr;
    u32 cmdType = cmd >> 29;

    (void)chcr;
    if (cmdType == 1U) {
        g_pops->mdecRemainingWords = 4U * (u16)cmd;
        g_pops->mdecInputPtr = ramAddr;
        g_pops->mdecInputLimit = bytes;
        g_pops->gpuDispFlags.bytes.muteCountdown = 1;
        if (g_pops->mdecRemainingWords != 0U) {
            if (g_pops->mdecOutputBytes != 0U) {
                popsMdecDecodeVfpu();
            }
            return 0;
        }
        return 0;
    }

    if (cmdType == 2U) {
        const u16 *idctWeights = (const u16 *)&g_mdecIdctCoeffTable[7];
        u32 index;
        for (index = 0U; index < 128U; index++) {
            u32 coeff = index & 63U;
            u32 outSlot = (coeff << 1) + (index >> 6);
            s32 scaled = (s32)ramBytes[index] * (s32)idctWeights[coeff];
            float fVal;

            if (coeff == 0U) {
                scaled <<= 3;
            }
            fVal = (float)scaled * 1.1368684e-13f;
            if ((index & 64U) == 0U) {
                fVal *= 1.772f;
            }
            g_pops->mdecScaleTable[outSlot] = fVal;
        }
    }

    return (s32)bytes;
}

/* MDEC DMA1 (decoded RGB output) transfer handler (0x00e480) */
s32 popsMdecDmaOut(u32 addr, u32 bytes, u32 chcr)
{
    (void)chcr;
    if (bytes == 0U) {
        return 1;
    }

    g_pops->mdecOutputPtr = (addr & 0x1FFFFFU) | 0x49800000U;
    g_pops->mdecOutputBytes = bytes;

    if (__builtin_allegrex_min((s32)g_pops->mdecRemainingWords,
                               (s32)g_pops->mdecInputLimit) > 0) {
        if ((g_pops->quirkFlags & 0x20000U) == 0U) {
            popsMdecDecodeVfpu();
            return 0;
        } else {
            s32 delay = ((g_pops->mdecStatus & 0x08000000U) != 0U) ? (s32)(6U * bytes) : (s32)(4U * bytes);
            u32 shift = (g_pops->quirkFlags >> 9) & 1U;
            popsEventSchedule(&g_pops->mdecEvent, delay >> shift);
        }
    }

    return 0;
}

/* MDEC status register read (0x1F801824 -> 0x00e510) */
u32 popsMdecStatusRead(u32 addr, u32 width)
{
    u32 baseBits = ((((g_pops->mdecStatus >> 25) & 0xFU) + 256U) << 23) & 0xDFFFFFFFU;
    u32 busyBit = (u32)((s32)g_pops->mdecRemainingWords >= 5) << 29;
    (void)addr;
    (void)width;
    return (baseBits | busyBit) + (g_pops->mdecRemainingWords >> 2);
}

/* MDEC command / control register write (0x1F801820 / 0x1F801824 -> 0x00e540) */
void popsMdecControlWrite(u32 addr, u32 value, u32 width)
{
    (void)width;
    if ((addr & 0xFU) == 0U) {
        g_pops->mdecStatus = value;
        return;
    }

    if ((s32)value < 0) {
        if (g_pops->mdecEvent.next != NULL) {
            popsEventCancel(&g_pops->mdecEvent);
        }
        g_pops->mdecStatus = 0U;
        g_pops->mdecRemainingWords = 0U;
        g_pops->mdecInputLimit = 0U;
        g_pops->mdecOutputBytes = 0U;
        g_pops->mdecInputPtr = 0U;
        g_pops->mdecOutputPtr = 0U;
        g_pops->mdecBlockPhase = 0U;
        g_pops->mdecEvent.prev = NULL;
        g_pops->mdecEvent.next = NULL;
        g_pops->mdecEvent.dueCycle = 0U;
        g_pops->mdecEvent.callback = NULL;
        popsInitMdec();
    }
}

/* GTE IRGB register write unpack helper (0x010218) */
u32 popsGteWriteIrgb(void)
{
    u32 irgb = g_pops->gte.named.irgb;
    u32 red = (irgb & 0x1FU) << 7;
    g_pops->gte.named.irOne = (s32)red;
    g_pops->gte.named.irTwo = (s32)(((irgb >> 5) & 0x1FU) << 7);
    g_pops->gte.named.irThree = (s32)(((irgb >> 10) & 0x1FU) << 7);
    return red;
}
