#include <stdarg.h>
#include <sysmem_sysclib.h>

#include <common_imp.h>

#include "intr.h"
#include "start.h"

int kprnt(short *work, const char *fmt, va_list ap, int userMode);

// 14434
char kprnt_outbuf[260];
// 13BC4
int (*kprinthandler)(short*, const char*, va_list, int) = kprnt;
// 13BC8
void *kprintwork = (void*)kprnt_outbuf;
// 13BCC
int kprintf_putchar_initial = 1;
// 13BD0
int debug_echo_mode = 1;

// 14410
u32 gDipLo;
// 14414
u32 gDipHi;
// 14418
u32 g_uiCpTime;
// 1441C
int gUmdDvdInitialized;
// 14420
int gIsDvdMode;
// 14424
void *g_pSm1Ops;
// 14428
void (*kprintf_putchar_handler)(short*, int);
// 1442C
int (*debug_write_handler)();
// 14430
int (*debug_read_handler)();

int DipswSet(int reg, int val);
void SetIsDvdMode(void);

int DipswInit(u32 lo, u32 hi, u32 cpTime)
{
    gDipLo = lo;
    gDipHi = hi;
    g_uiCpTime = cpTime;
    return 0;
}

int sceKernelDipsw(u32 reg)
{
    if (reg >= 64)
        return SCE_ERROR_KERNEL_ERROR;
    if (reg >= 32) {
        // FC00
        return (gDipHi >> (reg - 32)) & 1;
    }
    return (gDipLo >> reg) & 1;
}

u32 sceKernelDipswAll()
{
    return gDipLo;
}

u32 sceKernelDipswLow32()
{
    return gDipLo;
}

u32 sceKernelDipswHigh32()
{
    return gDipHi;
}

int sceKernelDipswSet(u32 reg)
{
    return DipswSet(reg, 1);
}

int sceKernelDipswClear(u32 reg)
{
    return DipswSet(reg, 0);
}

int sceKernelDipswCpTime(void)
{
    return g_uiCpTime;
}

int sceKernelIsToolMode(void)
{
    if (((gDipLo >> 30) & 1) == 1 || ((gDipLo >> 28) & 1) == 1)
        return 1;
    return 0;
}

int sceKernelIsDevelopmentToolMode()
{
    return (gDipLo >> 30) & 1;
}

int sceKernelIsUMDMode()
{
    SetIsDvdMode();
    return (gIsDvdMode == 0);
}

int sceKernelIsDVDMode()
{
    SetIsDvdMode();
    return gIsDvdMode;
}

typedef struct {
    int reservedBeforeGpi[7];
    int getGpi;
    int setGpo;
    int reservedBeforePtrig[4];
    int getPtrig;
    int setPtrig;
} SceSm1Ops;

int sceKernelSm1RegisterOperations(void *arg)
{
    SceSm1Ops *ops = (SceSm1Ops *)arg;
    g_pSm1Ops = arg;
    if (ops != NULL)
    {
        g_GetGPI = ops->getGpi;
        g_SetGPO = ops->setGpo;
        g_GetPTRIG = ops->getPtrig;
        g_SetPTRIG = ops->setPtrig;
    }
    return 0;
}

void *sceKernelSm1ReferOperations()
{
    return g_pSm1Ops;
}

int DipswSet(int reg, int val)
{
    int oldK1 = pspShiftK1();
    if (reg < 0 || reg >= 64)
    {
        // FDA0 dup
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ERROR;
    }

    if (pspK1IsUserMode())
    {
        switch (reg) // jump table at 0x13858
        {
        case 10:
        case 11:
        case 12:
        case 17:
        case 18:
        case 19:
        case 20:
        case 24:
        case 49:
        case 51:
            break;

        default:
            // FDA0 dup
            pspSetK1(oldK1);
            return SCE_ERROR_KERNEL_ERROR;
        }
    }
    // (FDB0)
    // FDB4
    u32 *regPtr;
    if (reg >= 32) {
        regPtr = &gDipHi;
        reg -= 32;
    }
    else
        regPtr = &gDipLo;

    // FDCC
    int oldVar = (*regPtr >> reg) & 1;
    if (val == 0)
        *regPtr &= ~(1 << reg);
    else
        *regPtr |= (1 << reg);
    // FDF8
    pspSetK1(oldK1);
    return oldVar;
}

void SetIsDvdMode(void)
{
    if (gUmdDvdInitialized != 0)
        return;
    gUmdDvdInitialized = 1;
    if (sceKernelIsToolMode() && ((gDipLo >> 16) & 1) == 1) {
        // FE5C
        gIsDvdMode = 1;
    }
    else {
        // FE48
        gIsDvdMode = 0;
    }
}

int kprnt(short *work, const char *fmt, va_list ap, int userMode)
{
    char str[21];
    void (*func)(short*, int) = kprintf_putchar_handler;
    work[0] = 1;
    work[1] = 0;
    if (func == NULL || fmt == NULL)
        return 0;
    func(work, 512);
    char *hexNumChars = "0123456789abcdef";
    int numChars = 0;
    for (;;)
    {
        if (*fmt == '\0')
        {
            func(work, 513);
            return numChars;
        }
        if (*fmt == '%')
        {
            int oldStrSize = 0;
            int attr = 0;
            int strSize = -1;
            int argWidth = 0;
            char sign = '\0';
            int base = 10;
            u32 curArg = 0;
            u64 longVar = 0;
            char *curStr = NULL;
            int stringLen = 0;
            int formatKind = 0; /* 0: done, 1: 32-bit int, 2: 64-bit int, 3: string */

            for (;;)
            {
                fmt++;
                switch (*fmt)
                {
                case ' ':
                    if (sign == '\0')
                        sign = ' ';
                    continue;

                case '\0':
                    func(work, 513);
                    return numChars;

                case '#':
                    attr |= 8;
                    continue;

                case '*':
                    argWidth = va_arg(ap, int);
                    if (argWidth < 0) {
                        argWidth = -argWidth;
                        attr |= 0x10;
                    }
                    continue;

                case '-':
                    attr |= 0x10;
                    continue;

                case '+':
                    sign = '+';
                    continue;

                case '.':
                {
                    fmt++;
                    int val;
                    if (*fmt == '*')
                    {
                        val = va_arg(ap, int);
                    }
                    else
                    {
                        val = 0;
                        while (*fmt >= '0' && *fmt <= '9')
                            val = val * 10 + *(fmt++) - '0';
                        fmt--;
                    }
                    strSize = pspMax(val, -1);
                    continue;
                }

                case '0':
                    attr |= 0x20;
                    continue;

                case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9':
                {
                    int val = 0;
                    do
                    {
                        val = val * 10 + (*(fmt++) - '0');
                        if (*fmt < 0)
                            break;
                    } while (*fmt >= '0' && *fmt <= '9');
                    fmt--;
                    argWidth = val;
                    continue;
                }

                case 'D':
                    attr |= 1;
                    /* FALLTHRU */

                case 'd':
                case 'i':
                    base = 10;
                    if ((attr & 2) == 0)
                    {
                        int signedArg = va_arg(ap, int);
                        if (signedArg < 0)
                        {
                            signedArg = -signedArg;
                            sign = '-';
                        }
                        curArg = (u32)signedArg;
                        formatKind = 1;
                    }
                    else
                    {
                        s64 signedLong = va_arg(ap, s64);
                        if (signedLong < 0)
                        {
                            signedLong = -signedLong;
                            sign = '-';
                        }
                        longVar = (u64)signedLong;
                        formatKind = 2;
                    }
                    break;

                case 'L':
                    attr |= 2;
                    continue;

                case 'O':
                    attr |= 1;
                    /* FALLTHRU */

                case 'o':
                    base = 8;
                    sign = '\0';
                    if ((attr & 2) == 0)
                    {
                        curArg = va_arg(ap, u32);
                        formatKind = 1;
                    }
                    else
                    {
                        longVar = va_arg(ap, u64);
                        formatKind = 2;
                    }
                    break;

                case 'U':
                    attr |= 1;
                    /* FALLTHRU */

                case 'u':
                    base = 10;
                    sign = '\0';
                    if ((attr & 2) == 0)
                    {
                        curArg = va_arg(ap, u32);
                        formatKind = 1;
                    }
                    else
                    {
                        longVar = va_arg(ap, u64);
                        formatKind = 2;
                    }
                    break;

                case 'X':
                    hexNumChars = "0123456789ABCDEF";
                    /* FALLTHRU */

                case 'x':
                    base = 16;
                    sign = '\0';
                    if ((attr & 2) == 0)
                    {
                        curArg = va_arg(ap, u32);
                        if (((attr >> 3) & 1) != 0 && curArg != 0)
                            attr |= 0x40;
                        formatKind = 1;
                    }
                    else
                    {
                        longVar = va_arg(ap, u64);
                        if (longVar != 0 && ((attr >> 3) & 1) != 0)
                            attr |= 0x40;
                        formatKind = 2;
                    }
                    break;

                case 'c':
                    curStr = str;
                    stringLen = 1;
                    sign = '\0';
                    str[0] = (char)va_arg(ap, int);
                    formatKind = 3;
                    break;

                case 'h':
                    attr |= 0x4;
                    continue;

                case 'l':
                    if ((attr & 1) == 0)
                        attr |= 1;
                    else
                        attr = (attr & 0xFFFFFFFE) | 2;
                    continue;

                case 'n':
                    if ((attr & 1) == 0 && (attr & 4) != 0)
                    {
                        short *ptr = va_arg(ap, short*);
                        if (userMode != 0 && !pspK1PtrOk(ptr))
                            return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
                        *ptr = numChars;
                    }
                    else
                    {
                        int *ptr = va_arg(ap, int*);
                        if (userMode != 0 && !pspK1PtrOk(ptr))
                            return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
                        *ptr = numChars;
                    }
                    break;

                case 'p':
                    curArg = va_arg(ap, u32);
                    base = 16;
                    sign = '\0';
                    formatKind = 1;
                    break;

                case 's':
                {
                    curStr = va_arg(ap, char*);
                    if (curStr == NULL)
                        curStr = "(null)";
                    if (userMode != 0 && !pspK1PtrOk(curStr))
                        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
                    if (strSize < 0)
                    {
                        stringLen = strlen(curStr);
                    }
                    else
                    {
                        char *nulPos = memchr(curStr, '\0', strSize);
                        stringLen = strSize;
                        if (nulPos != NULL)
                        {
                            stringLen = nulPos - curStr;
                            if (strSize < nulPos - curStr)
                                stringLen = strSize;
                        }
                    }
                    sign = '\0';
                    formatKind = 3;
                    break;
                }

                default:
                    func(work, *fmt);
                    numChars++;
                    break;
                }
                break;
            }

            if (formatKind == 2)
            {
                oldStrSize = strSize;
                if (strSize >= 0)
                    attr &= 0xFFFFFFDF;
                curStr = str + 21;
                if (longVar != 0 || strSize != 0)
                {
                    char lastChar;
                    do
                    {
                        u32 mod = __umoddi3(longVar, base);
                        lastChar = hexNumChars[mod];
                        *(--curStr) = lastChar;
                        longVar = __udivdi3(longVar, base);
                    } while (longVar != 0);
                    hexNumChars = "0123456789abcdef";
                    if (((attr >> 3) & 1) != 0 && base == 8 && lastChar != '0')
                        *(--curStr) = '0';
                }
                stringLen = &str[21] - curStr;
            }
            else if (formatKind == 1)
            {
                oldStrSize = strSize;
                if (strSize >= 0)
                    attr &= 0xFFFFFFDF;
                curStr = str + 21;
                if (curArg != 0 || strSize != 0)
                {
                    char lastC;
                    do
                    {
                        lastC = hexNumChars[curArg % (u32)base];
                        curArg /= (u32)base;
                        *(--curStr) = lastC;
                    } while (curArg != 0);
                    hexNumChars = "0123456789abcdef";
                    if (((attr >> 3) & 1) != 0 && base == 8 && lastC != '0')
                        *(--curStr) = '0';
                }
                stringLen = &str[21] - curStr;
            }

            if (formatKind != 0)
            {
                strSize = stringLen;
                if (sign != 0)
                    strSize = stringLen + 1;
                if ((attr & 0x40) != 0)
                    strSize += 2;
                int maxSize = pspMax(strSize, oldStrSize);
                int i;
                if ((attr & 0x30) == 0 && argWidth != 0)
                    for (i = 0; i < argWidth - maxSize; i++)
                        func(work, ' ');
                if (sign != '\0')
                    func(work, sign);
                if ((attr & 0x40) != 0)
                {
                    func(work, '0');
                    func(work, *fmt);
                }
                if ((attr & 0x30) == 0x20)
                {
                    for (i = 0; i < argWidth - maxSize; i++)
                        func(work, '0');
                }
                for (i = 0; i < oldStrSize - strSize; i++)
                    func(work, '0');
                for (i = stringLen - 1; i >= 0; i--)
                    func(work, *(curStr++));
                if ((attr & 0x10) != 0)
                    for (i = 0; i < argWidth - maxSize; i++)
                        func(work, ' ');
                numChars += pspMax(maxSize, argWidth);
            }
        }
        else
        {
            func(work, *fmt);
            numChars++;
        }
        fmt++;
    }
}

int sceKernelPrintf(const char *fmt, ...) __attribute__((alias("KprintfForUser")));

int KprintfForUser(const char *fmt, ...)
{
    va_list ap;
    int oldK1 = pspShiftK1();
    int ret = 0;
    va_start(ap, fmt);
    if (!pspK1PtrOk(fmt)) {
        pspSetK1(oldK1);
        return SCE_ERROR_KERNEL_ILLEGAL_ADDR;
    }
    int oldIntr = suspendIntr();
    if (kprinthandler != NULL && sceKernelDipsw(56) != 0) // 1082C
        ret = kprinthandler(kprintwork, fmt, ap, 1);
    // 107F4
    resumeIntr(oldIntr);
    va_end(ap);
    pspSetK1(oldK1);
    return ret;
}

int Kprintf(const char *fmt, ...)
{
    va_list ap;
    int oldK1 = pspShiftK1();
    va_start(ap, fmt);
    int ret = 0;
    int oldIntr = suspendIntr();
    if (kprinthandler != NULL && sceKernelDipsw(56) != 0) // 10900
        ret = kprinthandler(kprintwork, fmt, ap, 0);
    // 108CC
    resumeIntr(oldIntr);
    va_end(ap);
    pspSetK1(oldK1);
    return ret;
}

void (*sceKernelGetDebugPutchar(void))(short*, int)
{
    if (kprintf_putchar_initial != 0)
        return NULL;
    return kprintf_putchar_handler;
}

void sceKernelRegisterDebugPutchar(void (*func)(short*, int))
{
    kprintf_putchar_handler = func;
    kprintf_putchar_initial = 0;
}

void sceKernelRegisterDebugPutcharByBootloader(void (*func)(short*, int))
{
    if (kprintf_putchar_initial != 0)
        kprintf_putchar_handler = func;
}

void sceKernelRegisterKprintfHandler(int (*func)(short*, const char*, va_list, int), void *param)
{
    kprinthandler = func;
    kprintwork = param;
}

int _CheckDebugHandler(void *ptr, u32 size)
{
    int ret = 0;
    if ((void*)kprintf_putchar_handler < ptr || (void*)kprintf_putchar_handler >= ptr + size)
    {
        // 109C4
        kprintf_putchar_handler = NULL;
        ret = SCE_ERROR_KERNEL_ERROR;
    }
    // 109D0
    if ((void*)kprinthandler < ptr || (void*)kprinthandler >= ptr + size)
    {
        kprinthandler = NULL;
        // 109E4
        return SCE_ERROR_KERNEL_ERROR;
    }
    return ret;
}

int sceKernelDebugWrite()
{
    int (*func)() = debug_write_handler;
    if (func == NULL)
        return SCE_ERROR_KERNEL_ERROR;
    return func();
}

int sceKernelRegisterDebugWrite(int (*func)())
{
    debug_write_handler = func;
    return 0;
}

int sceKernelDebugRead()
{
    int (*func)() = debug_read_handler;
    if (func == NULL)
        return SCE_ERROR_KERNEL_ERROR;
    return func();
}

int sceKernelRegisterDebugRead(int (*func)())
{
    debug_read_handler = func;
    return 0;
}

int sceKernelDebugEcho(void)
{
    return debug_echo_mode;
}

int sceKernelDebugEchoSet(int echo)
{
    debug_echo_mode = echo;
    return echo;
}

