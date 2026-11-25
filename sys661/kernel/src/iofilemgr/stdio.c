/* Copyright (C) 2011, 2012 The uOFW team
   See the file COPYING for copying permission.
*/

#include <stdarg.h>

#include <common_imp.h>

#include "sysmem_kdebug.h"
#include "sysmem_kernel.h"
#include "sysmem_sysclib.h"
#include "iofilemgr_kernel.h"
#include "threadman_kernel.h"

#define STDIN  0
#define STDOUT 1
#define STDERR 2

typedef struct SceStdioPrintCtx {
    s16 fileDesc;
    s16 count;
    char buf[164];
} SceStdioPrintCtx;

SceSSize _sceTtyProxyDevRead(SceIoIob *iob, void *buf, SceSize size);
SceSSize _sceTtyProxyDevWrite(SceIoIob *iob, const void *buf, SceSize size);
int _sceTtyProxyDevInit(SceIoDeviceEntry *dev);
int _sceTtyProxyDevExit(SceIoDeviceEntry *dev);
int _sceTtyProxyDevOpen(SceIoIob *iob, char *file, int flags, SceMode mode);
int _sceTtyProxyDevClose(SceIoIob *iob);
SceOff _sceTtyProxyDevLseek();
int _sceTtyProxyDevIoctl(SceIoIob *iob, int cmd, void *indata, SceSize inlen, void *outdata, SceSize outlen);

SceIoDeviceFunction g_TtyOps = // 6948
{
    _sceTtyProxyDevInit, // 0BE4
    _sceTtyProxyDevExit, // 0BEC
    _sceTtyProxyDevOpen, // 0BF4
    _sceTtyProxyDevClose, // 0C0C
    _sceTtyProxyDevRead, // 0980
    _sceTtyProxyDevWrite, // 0A74
    _sceTtyProxyDevLseek, // 0C24
    _sceTtyProxyDevIoctl, // 0C34
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL
};

SceIoDeviceTable g_TtyDevTbl = { "ttyproxy", 0x00000003, 0x00000001, "TTY2MsgPipe PROXY", &g_TtyOps }; // 69A0

SceUID g_stdin = -1; // 6AD0
SceUID g_stdout = -1; // 6AD4
SceUID g_stderr = -1; // 6AD8

int g_debugRead; // 6AF0

struct SceIoPipeList {
    SceUID id[3];
    SceSize size[3];
};
struct SceIoPipeList g_pipeList; // 6AF4

int g_linePos; // 6C34

int sceTtyProxyInit();
int stdoutReset(int flags, SceMode mode);
int _sceKernelRegisterStdPipe(int fileDesc, SceUID id);

// 0000
int StdioReInit()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    SceUID *fds[3];
    fds[0] = &g_stdin;
    fds[1] = &g_stdout;
    fds[2] = &g_stderr;
    int i;
    for (i = 0; i < 3; i++)
        *fds[i] = sceIoOpen("dummy_drv_iofile:", 3, 0x1FF);
    g_debugRead = 1;
    sceTtyProxyInit();
    return 0;
}

// 0094
int StdioInit()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return StdioReInit();
}

int sceKernelStdin()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return g_stdin;
}

int sceKernelStdout()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return g_stdout;
}

int sceKernelStderr()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return g_stderr;
}

int sceKernelStdoutReopen(const char *file, int flags, SceMode mode)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (g_stdout < 0)
        return 0x80020384;
    return sceIoReopen(file, flags, mode, g_stdout);
}

int sceKernelStderrReopen(const char *file, int flags, SceMode mode)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (g_stderr < 0)
        return 0x80020384;
    return sceIoReopen(file, flags, mode, g_stderr);
}

int sceKernelStdoutReset()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int ret = stdoutReset(3, 0x1FF);
    if (ret < 0)
        return ret;
    g_stdout = ret;
    return 0;
}

int sceKernelStderrReset()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int ret = stdoutReset(3, 0x1FF);
    if (ret < 0)
        return ret;
    g_stderr = ret;
    return 0;
}

// 019C
int stdoutReset(int flags, SceMode mode)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    char useTty = 0;
    if (sceKernelDipsw(58) == 1 && !sceKernelDipsw(59))
        useTty = 1;
    else
        useTty = 0;
    if (useTty)
    {
        int ret = sceIoOpen("tty0:", flags, mode);
        if (ret >= 0)
            return ret;
    }
    return sceIoOpen("dummy_drv_iofile:", flags, mode);
}

// 0244
void printf_char(void *ctx, int ch)
{
    SceStdioPrintCtx *printCtx = (SceStdioPrintCtx *)ctx;
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (ch < 0x200)
        dbg_printf("print %c\n", ch);
    if (ch == 0x200)
    {
        printCtx->count = 0;
        return;
    }
    if (ch == 0x201)
    {
        s16 cnt = printCtx->count;
        if (cnt <= 0)
            return;

        if (sceKernelDipsw(59) == 1)
            sceKernelDebugWrite(printCtx->fileDesc, printCtx->buf, printCtx->count);
        else
        {
            s32 outDesc = printCtx->fileDesc;
            if (outDesc == STDOUT)
                outDesc = g_stdout;
            if (outDesc == STDERR)
                outDesc = g_stderr;
            sceIoWrite(outDesc, printCtx->buf, printCtx->count);
        }
        return;
    }
    if (ch == '\n')
        printf_char(ctx, '\r');

    printCtx->count++;
    printCtx->buf[printCtx->count - 1] = (char)ch;
    if (printCtx->count == 128)
    {
        if (sceKernelDipsw(59) == 1)
        {
            sceKernelDebugWrite(printCtx->fileDesc, printCtx->buf, printCtx->count);
        }
        else
        {
            s32 outDesc = printCtx->fileDesc;
            if (outDesc == STDOUT)
                outDesc = g_stdout;
            if (outDesc == STDERR)
                outDesc = g_stderr;
            sceIoWrite(outDesc, printCtx->buf, printCtx->count);
        }
        printCtx->count = 0;
    }
}

int fdprintf(int fileDesc, const char *fmt, ...)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    va_list ap;
    SceStdioPrintCtx printCtx;
    va_start(ap, fmt);
    printCtx.fileDesc = (s16)fileDesc;
    printCtx.count = 0;
    int n = prnt(printf_char, &printCtx, fmt, ap);
    va_end(ap);
    return n;
}

int printf(const char *fmt, ...)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    va_list ap;
    SceStdioPrintCtx printCtx;
    va_start(ap, fmt);
    printCtx.fileDesc = STDOUT;
    printCtx.count = 0;
    int n = prnt(printf_char, &printCtx, fmt, ap);
    va_end(ap);
    return n;
}

int fdputc(int c, int fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    char str[1] = { (char)c };
    if (c == '\t')
    {
        if (sceKernelDipsw(59) == 1)
            sceKernelDebugWrite(fileDesc, "        ", 8 - (g_linePos & 7));
        else
        {
            if (fileDesc == STDOUT)
                fileDesc = g_stdout;
            if (fileDesc == STDERR)
                fileDesc = g_stderr;
            sceIoWrite(fileDesc, "        ", 8 - (g_linePos & 7));
        }
        g_linePos = (g_linePos & 0xFFFFFFF8) + 8;
    }
    else if (c == '\n')
    {
        if (sceKernelDipsw(59) == 1)
            sceKernelDebugWrite(fileDesc, "\r\n", 2);
        else
        {
            if (fileDesc == STDOUT)
                fileDesc = g_stdout;
            if (fileDesc == STDERR)
                fileDesc = g_stderr;
            sceIoWrite(fileDesc, "\r\n", 2);
        }
        g_linePos = 0;
    }
    else
    {
        if ((look_ctype_table(c) & 0x97) != 0)
            g_linePos++;
        if (sceKernelDipsw(59) == 1)
            sceKernelDebugWrite(fileDesc, str, 1);
        else
        {
            if (fileDesc == STDOUT)
                fileDesc = g_stdout;
            if (fileDesc == STDERR)
                fileDesc = g_stderr;
            sceIoWrite(fileDesc, str, 1);
        }
    }
    return c;
}

int fdgetc(int fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    unsigned char buf[1];
    int ret;
    if (g_debugRead == 0)
        ret = sceKernelDebugRead(fileDesc, (char *)buf, 1);
    else
        ret = sceIoRead((fileDesc != STDIN ? fileDesc : g_stdin), buf, 1);
    return (ret == 1 ? (int)buf[0] : -1);
}

char *fdgets(char *s, int fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    char *end = s + 125;
    char *curS = s;
    for (;;)
    {
        char c = (char)fdgetc(fileDesc);
        switch (c)
        {
        case '\026': // sync
            c = (char)fdgetc(fileDesc);
            if (curS < end)
            {
                *(curS++) = c;
                if (sceKernelDebugEcho() != 0)
                    fdputc(c, (fileDesc != STDIN ? fileDesc : STDOUT));
            }
            else if (sceKernelDebugEcho() != 0)
                fdputc('\a', (fileDesc != STDIN ? fileDesc : STDOUT));
            break;
        case '\177': // DEL
        case '\b':
            if (s >= curS)
                break;
            curS--;
            if (sceKernelDebugEcho() != 0)
            {
                int realDesc = (fileDesc != STDIN ? fileDesc : STDOUT);
                fdputc('\b', realDesc);
                fdputc(' ', realDesc);
                fdputc('\b', realDesc);
            }
            break;
        case -1:
            if (s == curS)
            {
                *curS = '\0';
                return NULL;
            }
            /* FALLTHRU */
        case '\n':
            /* FALLTHRU */
        case '\r':
            if (sceKernelDebugEcho() != 0)
                fdputc('\n', (fileDesc != STDIN ? fileDesc : STDOUT));
            *curS = '\0';
            return s;
        case '\t':
            c = ' ';
            /* FALLTHRU */
        default:
            if ((look_ctype_table(c) & 0x97) != 0 && curS < end)
            {
                *(curS++) = c;
                if (sceKernelDebugEcho() != 0)
                    fdputc(c, (fileDesc != STDIN ? fileDesc : STDOUT));
            }
            else if (sceKernelDebugEcho() != 0)
                fdputc('\a', (fileDesc != STDIN ? fileDesc : STDOUT));
            break;
        }
    }
}

int putchar(int c)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return fdputc(c, STDOUT);
}

int fdputs(const char *s, int fileDesc)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    char c;
    if (s == NULL)
        s = "<NULL>";
    while ((c = *(s++)) != '\0')
        fdputc(c, fileDesc);
    return 0;
}

int puts(const char *s)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    char c;
    if (s == NULL)
        s = "<NULL>";
    while ((c = *(s++)) != '\0')
        fdputc(c, STDOUT);
    return 0;
}

int getchar(void)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return fdgetc(STDIN);
}

char *gets(char *s)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return fdgets(s, STDIN);
}

int sceKernelStdioRead()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return SCE_ERROR_KERNEL_ERROR;
}

int sceKernelStdioLseek()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return SCE_ERROR_KERNEL_ERROR;
}

void sceKernelStdioSendChar()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
}

int sceKernelStdioWrite()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return SCE_ERROR_KERNEL_ERROR;
}

int sceKernelStdioClose()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return SCE_ERROR_KERNEL_ERROR;
}

int sceKernelStdioOpen()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return SCE_ERROR_KERNEL_ERROR;
}

SceSSize _sceTtyProxyDevRead(SceIoIob *iob, void *buf, SceSize size)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int cnt;
    int oldK1 = pspShiftK1();
    int count = 0;
    int k1 = 0;
    u8 *dst = buf;
    if (sceIoGetIobUserLevel(iob) != 8)
        k1 = 24;
    pspSetK1(k1);
    SceSize pipeSize = g_pipeList.size[iob->i_unit];
    SceUID id = g_pipeList.id[iob->i_unit];
    SceSize min;
    do
    {
        min = pipeSize;
        if (pipeSize >= size)
            min = size;
        int ret = sceKernelReceiveMsgPipe(id, dst, min, 1, &cnt, 0);
        if (ret < 0)
        {
            pspSetK1(oldK1);
            return ret;
        }
        size -= cnt;
        dst += cnt;
        count += cnt;
    } while ((SceSize)cnt >= min && size != 0);
    pspSetK1(oldK1);
    return count;
}

SceSSize _sceTtyProxyDevWrite(SceIoIob *iob, const void *buf, SceSize size)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    int count = 0;
    int curCnt;
    int k1 = 0;
    const u8 *src = buf;
    if (sceIoGetIobUserLevel(iob) != 8)
        k1 = 24;
    pspSetK1(k1);
    SceSize pipeSize = g_pipeList.size[iob->i_unit];
    SceUID id = g_pipeList.id[iob->i_unit];
    do
    {
        SceSize minSize = pipeSize;
        if (pipeSize >= size)
            minSize = size;
        int ret = sceKernelSendMsgPipe(id, (void *)src, minSize, 0, &curCnt, 0);
        if (ret < 0)
        {
            pspSetK1(oldK1);
            return ret;
        }
        size -= curCnt;
        count += curCnt;
        src += curCnt;
    } while (size != 0);
    pspSetK1(oldK1);
    return count;
}

// 0B58
int sceTtyProxyInit()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    sceIoAddDrv(&g_TtyDevTbl);
    return 0;
}

int sceKernelRegisterStdoutPipe(SceUID id)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    int ret = _sceKernelRegisterStdPipe(STDOUT, id);
    pspSetK1(oldK1);
    return ret;
}

int sceKernelRegisterStderrPipe(SceUID id)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    int oldK1 = pspShiftK1();
    int ret = _sceKernelRegisterStdPipe(STDERR, id);
    pspSetK1(oldK1);
    return ret;
}

// 0BE4
int _sceTtyProxyDevInit(SceIoDeviceEntry *dev __attribute__((unused)))
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return 0;
}

// 0BEC
int _sceTtyProxyDevExit(SceIoDeviceEntry *dev __attribute__((unused)))
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return 0;
}

int _sceTtyProxyDevOpen(SceIoIob *iob, char *file __attribute__((unused)), int flags __attribute__((unused)), SceMode mode __attribute__((unused)))
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (iob->i_unit < 3)
        return 0;
    return 0x80010006;
}

int _sceTtyProxyDevClose(SceIoIob *iob)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (iob->i_unit < 3)
        return 0;
    return 0x80010006;
}

SceOff _sceTtyProxyDevLseek()
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    return SCE_ERROR_KERNEL_BAD_FILE_DESCRIPTOR;
}

int _sceTtyProxyDevIoctl(SceIoIob *iob, int cmd, void *indata __attribute__((unused)), SceSize inlen __attribute__((unused)), void *outdata __attribute__((unused)), SceSize outlen __attribute__((unused)))
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (cmd != 0x00134002)
        return SCE_ERROR_KERNEL_INVALID_ARGUMENT;
    int oldK1 = pspShiftK1();
    int k1 = 24;
    if (sceIoGetIobUserLevel(iob) == 8)
        k1 = 0;
    pspSetK1(k1);
    sceKernelCancelMsgPipe(g_pipeList.id[iob->i_unit], 0, 0);
    pspSetK1(oldK1);
    return 0;
}

// 0CBC
int _sceKernelRegisterStdPipe(int fileDesc, SceUID id)
{
    dbg_printf("Calling %s\n", __FUNCTION__);
    if (id < 0)
    {
        g_pipeList.id[fileDesc] = -1;
        g_pipeList.size[fileDesc] = 0;
        if (fileDesc == STDOUT)
            return sceKernelStdoutReset();
        else if (fileDesc == STDERR)
            return sceKernelStderrReset();
        return 0;
    }
    if (sceKernelGetThreadmanIdType(id) != 7)
        return SCE_ERROR_KERNEL_ILLEGAL_ARGUMENT;
    SceSysmemUidCB *blk;
    if (sceKernelGetUIDcontrolBlock(id, &blk) != 0)
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    int flag = 2;
    if (pspK1IsUserMode())
        flag = blk->attr & 2;
    if (flag == 0)
        return SCE_ERROR_KERNEL_ILLEGAL_PERMISSION;
    SceKernelMppInfo mpp;
    mpp.size = 56;
    int ret = sceKernelReferMsgPipeStatus(id, &mpp);
    if (ret < 0)
        return ret;
    if (fileDesc == STDOUT)
        ret = sceKernelStdoutReopen("ttyproxy1:", 2, 0x1FF);
    else if (fileDesc == STDERR)
        ret = sceKernelStderrReopen("ttyproxy2:", 2, 0x1FF);
    if (ret < 0)
        return ret;
    g_pipeList.size[fileDesc] = mpp.bufSize;
    g_pipeList.id[fileDesc] = id;
    return 0;
}
