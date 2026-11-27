/*
 * PlayStation 1 Boot Shell GPU library (libgpu sys.c equivalent):
 * ResetGraph, ClearImage, LoadImage, StoreImage, MoveImage, OTag, DrawEnv/DispEnv.
 */

#include "shell_internal.h"

static s32 g_graphDebugLevel;
static u32 g_displayModeBits;

static void gpuWaitReady(void)
{
    u32 timeout = 0x100000U;

    while ((SHELL_IO32(SHELL_REG_GPU_GP1) & 0x04000000U) == 0U && timeout > 0U)
        timeout--;
}

static void gpuWriteGp0(u32 cmd)
{
    gpuWaitReady();
    SHELL_IO32(SHELL_REG_GPU_GP0) = cmd;
}

static void gpuWriteGp1(u32 cmd)
{
    SHELL_IO32(SHELL_REG_GPU_GP1) = cmd;
}

s32 shellResetGraph(s32 mode)
{
    if (mode == 0) {
        gpuWriteGp1(0x00000000U); /* Reset GPU */
        gpuWriteGp1(0x04000000U); /* DMA off */
        gpuWriteGp1(0x03000001U); /* Display off */
        SHELL_IO32(SHELL_REG_DPCR) |= 0x08000800U; /* Enable DMA ch2 (GPU) + ch6 (OTC) */
    } else if (mode == 1) {
        gpuWriteGp1(0x01000000U); /* Reset command FIFO */
        gpuWriteGp1(0x04000000U);
    } else if (mode == 3) {
        gpuWriteGp1(0x01000000U);
        gpuWriteGp1(0x03000000U); /* Display on */
    }
    return 0;
}

s32 shellSetGraphDebug(s32 level)
{
    s32 prev = g_graphDebugLevel;
    g_graphDebugLevel = level;
    return prev;
}

void shellSetDispMask(s32 mask)
{
    gpuWriteGp1(mask != 0 ? 0x03000000U : 0x03000001U);
}

s32 shellDrawSync(s32 mode)
{
    if (mode != 0)
        return ((SHELL_IO32(SHELL_REG_GPU_GP1) & 0x1C000000U) == 0x1C000000U) ? 0 : 1;
    gpuWaitReady();
    while ((SHELL_IO32(SHELL_REG_GPU_GP1) & 0x10000000U) == 0U)
        ;
    return 0;
}

s32 shellClearImage(const ShellRect *rect, u8 r, u8 g, u8 b)
{
    if (rect == NULL || rect->w <= 0 || rect->h <= 0)
        return -1;
    gpuWriteGp0(0x02000000U | ((u32)b << 16) | ((u32)g << 8) | (u32)r);
    gpuWriteGp0(((u32)(u16)rect->y << 16) | (u32)(u16)rect->x);
    gpuWriteGp0(((u32)(u16)rect->h << 16) | (u32)(u16)rect->w);
    return 0;
}

s32 shellLoadImage(const ShellRect *rect, const u32 *pixels)
{
    u32 words;
    u32 i;

    if (rect == NULL || pixels == NULL || rect->w <= 0 || rect->h <= 0)
        return -1;
    words = ((u32)rect->w * (u32)rect->h + 1U) >> 1;
    gpuWriteGp0(0xA0000000U);
    gpuWriteGp0(((u32)(u16)rect->y << 16) | (u32)(u16)rect->x);
    gpuWriteGp0(((u32)(u16)rect->h << 16) | (u32)(u16)rect->w);
    for (i = 0; i < words; i++)
        gpuWriteGp0(pixels[i]);
    return 0;
}

s32 shellStoreImage(const ShellRect *rect, u32 *pixels)
{
    u32 words;
    u32 i;

    if (rect == NULL || pixels == NULL || rect->w <= 0 || rect->h <= 0)
        return -1;
    words = ((u32)rect->w * (u32)rect->h + 1U) >> 1;
    gpuWriteGp0(0xC0000000U);
    gpuWriteGp0(((u32)(u16)rect->y << 16) | (u32)(u16)rect->x);
    gpuWriteGp0(((u32)(u16)rect->h << 16) | (u32)(u16)rect->w);
    for (i = 0; i < words; i++) {
        while ((SHELL_IO32(SHELL_REG_GPU_GP1) & 0x08000000U) == 0U)
            ;
        pixels[i] = SHELL_IO32(SHELL_REG_GPU_GP0);
    }
    return 0;
}

s32 shellMoveImage(const ShellRect *srcRect, s32 dstX, s32 dstY)
{
    if (srcRect == NULL || srcRect->w <= 0 || srcRect->h <= 0)
        return -1;
    gpuWriteGp0(0x80000000U);
    gpuWriteGp0(((u32)(u16)srcRect->y << 16) | (u32)(u16)srcRect->x);
    gpuWriteGp0(((u32)(u16)dstY << 16) | (u32)(u16)dstX);
    gpuWriteGp0(((u32)(u16)srcRect->h << 16) | (u32)(u16)srcRect->w);
    return 0;
}

u32 *shellClearOTag(u32 *ot, s32 count)
{
    s32 i;

    if (ot == NULL || count <= 0)
        return ot;
    for (i = 0; i < count - 1; i++)
        ot[i] = ((u32)&ot[i + 1]) & 0x00FFFFFFU;
    ot[count - 1] = 0x00FFFFFFU;
    return ot;
}

u32 *shellClearOTagR(u32 *ot, s32 count)
{
    if (ot == NULL || count <= 0)
        return ot;
    SHELL_IO32(SHELL_REG_OTC_MADR) = ((u32)&ot[count - 1]) & 0x00FFFFFFU;
    SHELL_IO32(SHELL_REG_OTC_BCR) = (u32)count;
    SHELL_IO32(SHELL_REG_OTC_CHCR) = 0x11000002U;
    while ((SHELL_IO32(SHELL_REG_OTC_CHCR) & 0x01000000U) != 0U)
        ;
    return ot;
}

void shellDrawOTag(const u32 *ot)
{
    if (ot == NULL)
        return;
    gpuWaitReady();
    gpuWriteGp1(0x04000002U); /* DMA CPU->GP0 */
    SHELL_IO32(SHELL_REG_GPU_MADR) = ((u32)ot) & 0x00FFFFFFU;
    SHELL_IO32(SHELL_REG_GPU_BCR) = 0U;
    SHELL_IO32(SHELL_REG_GPU_CHCR) = 0x01000401U;
    while ((SHELL_IO32(SHELL_REG_GPU_CHCR) & 0x01000000U) != 0U)
        ;
    gpuWriteGp1(0x04000000U);
}

ShellDrawEnv *shellSetDefDrawEnv(ShellDrawEnv *env, s32 x, s32 y, s32 w, s32 h)
{
    env->clip.x = (s16)x;
    env->clip.y = (s16)y;
    env->clip.w = (s16)w;
    env->clip.h = (s16)h;
    env->ofs[0] = (s16)x;
    env->ofs[1] = (s16)y;
    env->tw.x = 0;
    env->tw.y = 0;
    env->tw.w = 0;
    env->tw.h = 0;
    env->tpage = 0x000AU;
    env->dtd = 1U;
    env->dfe = 0U;
    env->isbg = 0U;
    env->bgRed = 0U;
    env->bgGreen = 0U;
    env->bgBlue = 0U;
    return env;
}

ShellDispEnv *shellSetDefDispEnv(ShellDispEnv *env, s32 x, s32 y, s32 w, s32 h)
{
    env->disp.x = (s16)x;
    env->disp.y = (s16)y;
    env->disp.w = (s16)w;
    env->disp.h = (s16)h;
    env->screen.x = 0;
    env->screen.y = 0;
    env->screen.w = 256;
    env->screen.h = (s16)((shellGetVideoMode() == SHELL_VIDEO_PAL) ? 256 : 240);
    env->isinter = 0U;
    env->isrgb24 = 0U;
    env->pad[0] = 0U;
    env->pad[1] = 0U;
    return env;
}

ShellDrawEnv *shellPutDrawEnv(const ShellDrawEnv *env)
{
    u32 clipLeft;
    u32 clipTop;
    u32 clipRight;
    u32 clipBottom;

    if (env == NULL)
        return NULL;
    clipLeft = (u32)env->clip.x & 0x3FFU;
    clipTop = (u32)env->clip.y & 0x3FFU;
    clipRight = (u32)(env->clip.x + env->clip.w - 1) & 0x3FFU;
    clipBottom = (u32)(env->clip.y + env->clip.h - 1) & 0x3FFU;

    gpuWriteGp0(0xE1000000U | (u32)env->tpage | ((u32)env->dtd << 9) | ((u32)env->dfe << 10));
    gpuWriteGp0(0xE3000000U | (clipTop << 10) | clipLeft);
    gpuWriteGp0(0xE4000000U | (clipBottom << 10) | clipRight);
    gpuWriteGp0(0xE5000000U | (((u32)env->ofs[1] & 0x7FFU) << 11) | ((u32)env->ofs[0] & 0x7FFU));

    if (env->isbg != 0U)
        shellClearImage(&env->clip, env->bgRed, env->bgGreen, env->bgBlue);
    return (ShellDrawEnv *)env;
}

ShellDispEnv *shellPutDispEnv(const ShellDispEnv *env)
{
    u32 mode = 0U;

    if (env == NULL)
        return NULL;
    if (env->disp.w > 512)
        mode |= 0x03U; /* 640 */
    else if (env->disp.w > 384)
        mode |= 0x02U; /* 512 */
    else if (env->disp.w > 256)
        mode |= 0x01U; /* 320 */

    if (env->disp.h > 256)
        mode |= 0x04U; /* 480 / 512 vertical */
    if (shellGetVideoMode() == SHELL_VIDEO_PAL)
        mode |= 0x08U;
    if (env->isrgb24 != 0U)
        mode |= 0x10U;
    if (env->isinter != 0U)
        mode |= 0x20U;

    g_displayModeBits = mode;
    gpuWriteGp1(0x05000000U | (((u32)env->disp.y & 0x1FFU) << 10) | ((u32)env->disp.x & 0x3FFU));
    gpuWriteGp1(0x06C60260U);
    gpuWriteGp1((shellGetVideoMode() == SHELL_VIDEO_PAL) ? 0x0704C01CU : 0x07040010U);
    gpuWriteGp1(0x08000000U | mode);
    return (ShellDispEnv *)env;
}
