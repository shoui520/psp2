/*
 * PlayStation 1 Boot Shell interrupt & VSync library (libetc intr.c equivalent)
 * plus thin BIOS A0 syscall call bridges.
 */

#include "shell_internal.h"

static s32 g_videoMode;
static u32 g_vsyncCount;
static void (*g_vsyncCallback)(void);
static void (*g_irqCallbacks[11])(void);
static void (*g_dmaCallbacks[7])(void);

s32 shellResetCallback(void)
{
    u32 i;

    g_vsyncCount = 0U;
    g_vsyncCallback = NULL;
    for (i = 0; i < 11U; i++)
        g_irqCallbacks[i] = NULL;
    for (i = 0; i < 7U; i++)
        g_dmaCallbacks[i] = NULL;
    SHELL_IO32(SHELL_REG_I_STAT) = 0U;
    return 0;
}

s32 shellSetVideoMode(s32 mode)
{
    s32 prev = g_videoMode;
    g_videoMode = (mode == SHELL_VIDEO_PAL) ? SHELL_VIDEO_PAL : SHELL_VIDEO_NTSC;
    return prev;
}

s32 shellGetVideoMode(void)
{
    return g_videoMode;
}

void *shellVSyncCallback(void (*fn)(void))
{
    void (*prev)(void) = g_vsyncCallback;
    g_vsyncCallback = fn;
    return (void *)prev;
}

void *shellInterruptCallback(s32 irq, void (*fn)(void))
{
    void (*prev)(void);

    if (irq < 0 || irq >= 11)
        return NULL;
    prev = g_irqCallbacks[irq];
    g_irqCallbacks[irq] = fn;
    return (void *)prev;
}

void *shellDmaCallback(s32 ch, void (*fn)(void))
{
    void (*prev)(void);

    if (ch < 0 || ch >= 7)
        return NULL;
    prev = g_dmaCallbacks[ch];
    g_dmaCallbacks[ch] = fn;
    return (void *)prev;
}

s32 shellVSync(s32 mode)
{
    u32 timeout = 0x40000U;

    if (mode < 0)
        return (s32)g_vsyncCount;
    if (mode == 1)
        return (s32)g_vsyncCount;

    SHELL_IO32(SHELL_REG_I_STAT) = ~1U;
    while ((SHELL_IO32(SHELL_REG_I_STAT) & 1U) == 0U && timeout > 0U)
        timeout--;
    SHELL_IO32(SHELL_REG_I_STAT) = ~1U;
    g_vsyncCount++;
    if (g_vsyncCallback != NULL)
        g_vsyncCallback();
    return (s32)g_vsyncCount;
}

/* Call PS1 BIOS A0:A5 (CdReadSector) through the standard 0x800000A0 vector */
s32 shellBiosCdReadSector(s32 count, u32 lba, void *dst)
{
    register s32 regCount __asm__("$4") = count;
    register u32 regLba __asm__("$5") = lba;
    register void *regDst __asm__("$6") = dst;
    register u32 regSysIndex __asm__("$9") = 0xA5U;
    register s32 regResult __asm__("$2");

    __asm__ __volatile__(
        "li    $8, 0x800000A0\n\t"
        "jalr  $8\n\t"
        "nop"
        : "=r"(regResult), "+r"(regCount), "+r"(regLba), "+r"(regDst), "+r"(regSysIndex)
        :
        : "$3", "$7", "$8", "$10", "$11", "$12", "$13", "$14", "$15", "$24", "$25", "$31", "memory"
    );
    return regResult;
}
