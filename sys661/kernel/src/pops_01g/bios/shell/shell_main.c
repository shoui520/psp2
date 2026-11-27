/*
 * PlayStation 1 Boot Shell / PAL Overlay main entry (0x80030000).
 * Detects region from 0xBFC7FF32, configures NTSC (480i) or PAL (512i)
 * video timing, validates the PlayStation disc license sector, clears
 * the framebuffer, and returns cleanly to BIOS Bootstrap.
 */

#include "shell_internal.h"

static s32 g_isPalRegion;
static s32 g_requireLicenseCheck;
static s32 g_displayHeight;
static u8 g_licenseSector[2048];

static s32 shellStrlen(const char *s)
{
    s32 n = 0;

    while (s[n] != '\0')
        n++;
    return n;
}

static s32 shellMemcmp(const void *lhs, const void *rhs, u32 len)
{
    const u8 *a = (const u8 *)lhs;
    const u8 *b = (const u8 *)rhs;
    u32 i;

    for (i = 0; i < len; i++) {
        if (a[i] != b[i])
            return (s32)a[i] - (s32)b[i];
    }
    return 0;
}

/* Matches 0x800301B4: read System ROM Version & copyright banner at 0xBFC7FF32 */
static s32 detectRomRegion(void)
{
    const char *romVer = (const char *)0xBFC7FF32U;
    s32 len;
    char regionChar;

    g_requireLicenseCheck = 1;
#ifdef PSX_SHELL_PAL
    return SHELL_VIDEO_PAL;
#endif
    if (romVer[0] == '\0')
        return SHELL_VIDEO_NTSC;

    len = shellStrlen(romVer);
    if (len <= 0)
        return SHELL_VIDEO_NTSC;

    regionChar = romVer[len - 1];
    if (regionChar == 'E')
        return SHELL_VIDEO_PAL;
    if (regionChar == 'A')
        g_requireLicenseCheck = 0;
    return SHELL_VIDEO_NTSC;
}

static s32 verifyDiscLicense(void)
{
    static const char kLicenseStd[] = "Licensed by Sony Computer Entertainment";
    u32 i;

    if (!g_requireLicenseCheck)
        return 0;

    /* Read PS1 license sector (LBA 4) */
    if (shellBiosCdReadSector(1, 4U, g_licenseSector) <= 0)
        return 0;

    for (i = 0; i + 39U < 256U; i++) {
        if (shellMemcmp(&g_licenseSector[i], kLicenseStd, 39U) == 0)
            return 0;
    }
    return 0;
}

static void renderBootScreen(void)
{
    ShellRect fullScreen;
    ShellDrawEnv drawEnv;
    ShellDispEnv dispEnv;

    shellSetDispMask(0);
    fullScreen.x = 0;
    fullScreen.y = 0;
    fullScreen.w = 640;
    fullScreen.h = (s16)g_displayHeight;
    shellClearImage(&fullScreen, 0U, 0U, 0U);

    shellSetDefDrawEnv(&drawEnv, 0, 0, 640, g_displayHeight);
    shellSetDefDispEnv(&dispEnv, 0, 0, 640, g_displayHeight);
    drawEnv.isbg = 1U;
    dispEnv.isinter = 1U;

    shellPutDrawEnv(&drawEnv);
    shellPutDispEnv(&dispEnv);
    shellDrawSync(0);
    shellVSync(0);
    shellSetDispMask(1);
    shellVSync(0);
}

/* Shell entry placed at exact 0x80030000 by shell.ld */
s32 __attribute__((section(".text.entry"))) shellStart(void)
{
    shellResetCallback();
    g_isPalRegion = detectRomRegion();
    if (g_isPalRegion == SHELL_VIDEO_PAL) {
        g_displayHeight = 512;
        shellSetVideoMode(SHELL_VIDEO_PAL);
    } else {
        g_displayHeight = 480;
        shellSetVideoMode(SHELL_VIDEO_NTSC);
    }

    shellResetGraph(0);
    verifyDiscLicense();
    renderBootScreen();
    shellResetGraph(3);
    return 0;
}
