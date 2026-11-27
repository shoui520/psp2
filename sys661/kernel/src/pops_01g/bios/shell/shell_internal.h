/*
 * PlayStation 1 Boot Shell / PAL Overlay (linked at 0x80030000, 0x16120 bytes).
 * Runs inside PS1 RAM after being copied from 0xBFC18000 by the BIOS bootstrap.
 */

#ifndef PSX_SHELL_INTERNAL_H
#define PSX_SHELL_INTERNAL_H

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;
typedef signed char    s8;
typedef short          s16;
typedef int            s32;

#define NULL ((void *)0)

#define SHELL_IO32(off) (*(volatile u32 *)(0xBF800000U + (off)))
#define SHELL_IO16(off) (*(volatile u16 *)(0xBF800000U + (off)))

#define SHELL_REG_I_STAT   0x1070U
#define SHELL_REG_I_MASK   0x1074U
#define SHELL_REG_GPU_GP0  0x1810U
#define SHELL_REG_GPU_GP1  0x1814U
#define SHELL_REG_GPU_MADR 0x10A0U
#define SHELL_REG_GPU_BCR  0x10A4U
#define SHELL_REG_GPU_CHCR 0x10A8U
#define SHELL_REG_OTC_MADR 0x10E0U
#define SHELL_REG_OTC_BCR  0x10E4U
#define SHELL_REG_OTC_CHCR 0x10E8U
#define SHELL_REG_DPCR     0x10F0U

#define SHELL_VIDEO_NTSC   0
#define SHELL_VIDEO_PAL    1

typedef struct ShellRect {
    s16 x;
    s16 y;
    s16 w;
    s16 h;
} ShellRect;

typedef struct ShellDrawEnv {
    ShellRect clip;
    s16 ofs[2];
    ShellRect tw;
    u16 tpage;
    u8 dtd;
    u8 dfe;
    u8 isbg;
    u8 bgRed;
    u8 bgGreen;
    u8 bgBlue;
} ShellDrawEnv;

typedef struct ShellDispEnv {
    ShellRect disp;
    ShellRect screen;
    u8 isinter;
    u8 isrgb24;
    u8 pad[2];
} ShellDispEnv;

/* libetc / intr.c primitives */
s32 shellResetCallback(void);
s32 shellVSync(s32 mode);
void *shellVSyncCallback(void (*fn)(void));
s32 shellSetVideoMode(s32 mode);
s32 shellGetVideoMode(void);
void *shellInterruptCallback(s32 irq, void (*fn)(void));
void *shellDmaCallback(s32 ch, void (*fn)(void));

/* libgpu / sys.c primitives */
s32 shellResetGraph(s32 mode);
s32 shellSetGraphDebug(s32 level);
void shellSetDispMask(s32 mask);
s32 shellDrawSync(s32 mode);
s32 shellClearImage(const ShellRect *rect, u8 r, u8 g, u8 b);
s32 shellLoadImage(const ShellRect *rect, const u32 *pixels);
s32 shellStoreImage(const ShellRect *rect, u32 *pixels);
s32 shellMoveImage(const ShellRect *srcRect, s32 dstX, s32 dstY);
u32 *shellClearOTag(u32 *ot, s32 count);
u32 *shellClearOTagR(u32 *ot, s32 count);
void shellDrawOTag(const u32 *ot);
ShellDrawEnv *shellSetDefDrawEnv(ShellDrawEnv *env, s32 x, s32 y, s32 w, s32 h);
ShellDispEnv *shellSetDefDispEnv(ShellDispEnv *env, s32 x, s32 y, s32 w, s32 h);
ShellDrawEnv *shellPutDrawEnv(const ShellDrawEnv *env);
ShellDispEnv *shellPutDispEnv(const ShellDispEnv *env);

/* BIOS syscall helpers invoked via A0/B0 vectors */
s32 shellBiosPrintf(const char *fmt, ...);
s32 shellBiosCdReadSector(s32 count, u32 lba, void *dst);

#endif /* PSX_SHELL_INTERNAL_H */
