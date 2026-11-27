/*
 * PlayStation 1 BIOS GPU hardware primitives (0x1F801810 GP0, 0x1F801814 GP1).
 */

#include "bios_internal.h"

u32 biosGetGpuStatus(void)
{
    return PSX_IO32(REG_GPU_GP1);
}

s32 biosGpuSync(void)
{
    u32 timeout = 0x100000U;

    while ((PSX_IO32(REG_GPU_GP1) & 0x04000000U) == 0U && timeout > 0U)
        timeout--;
    return (timeout == 0U) ? -1 : 0;
}

void biosSendGp1(u32 cmd)
{
    PSX_IO32(REG_GPU_GP1) = cmd;
}

void biosGpuCw(u32 cmd)
{
    biosGpuSync();
    PSX_IO32(REG_GPU_GP0) = cmd;
}

void biosGpuCwp(const u32 *cmds, s32 count)
{
    s32 i;

    if (cmds == NULL || count <= 0)
        return;
    for (i = 0; i < count; i++)
        biosGpuCw(cmds[i]);
}

void biosGpuDw(u32 x, u32 y, u32 w, u32 h, const u16 *pixels)
{
    u32 words = (w * h + 1U) >> 1;
    const u32 *src = (const u32 *)pixels;
    u32 i;

    biosGpuCw(0xA0000000U);
    biosGpuCw((y << 16) | (x & 0xFFFFU));
    biosGpuCw((h << 16) | (w & 0xFFFFU));
    for (i = 0; i < words; i++)
        biosGpuCw(src[i]);
}

void biosResetGpu(void)
{
    biosSendGp1(0x00000000U); /* Reset GPU */
    biosSendGp1(0x03000000U); /* Display enable */
    biosSendGp1(0x04000000U); /* DMA off */
    biosSendGp1(0x05000000U); /* Display start (0,0) */
    biosSendGp1(0x06C60260U); /* Horizontal display range */
    biosSendGp1(0x07040010U); /* Vertical display range */
    biosSendGp1(0x08000000U); /* NTSC 256x240 15bpp */
}
