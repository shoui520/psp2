/*
 * PlayStation 1 BIOS device table (DCB), file descriptor table (FCB),
 * and standard File* / tty00: I/O services.
 */

#include "bios_internal.h"

static s32 ttyOpen(void *fileCb, const char *path, u32 mode)
{
    (void)fileCb;
    (void)path;
    (void)mode;
    return 0;
}

static s32 ttyClose(void *fileCb)
{
    (void)fileCb;
    return 0;
}

static s32 ttyRead(void *fileCb, void *dst, u32 len)
{
    (void)fileCb;
    (void)dst;
    (void)len;
    return 0;
}

static s32 ttyWrite(void *fileCb, const void *src, u32 len)
{
    const u8 *bytes = (const u8 *)src;
    u32 i;

    (void)fileCb;
    for (i = 0; i < len; i++)
        PSX_IO8(0x2041U) = bytes[i];
    return (s32)len;
}

static s32 ttyAction(void *fileCb, u32 mode)
{
    PsxFcb *f = (PsxFcb *)fileCb;

    if (mode == 1U)
        return ttyRead(f, f->transferBuf, f->transferLen);
    return ttyWrite(f, f->transferBuf, f->transferLen);
}

static const PsxDcb g_ttyDcb = {
    "tty",
    0x00000003U,
    1U,
    "CONSOLE",
    NULL,
    ttyOpen,
    ttyAction,
    ttyClose,
    NULL,
    ttyRead,
    ttyWrite,
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

void biosAddNullTty(void)
{
    biosAddDevice(&g_ttyDcb);
}

void biosInitDevices(void)
{
    u32 i;

    biosMemset(PSX_BIOS->dcbs, 0, sizeof(PSX_BIOS->dcbs));
    biosMemset(PSX_BIOS->fcbs, 0, sizeof(PSX_BIOS->fcbs));
    for (i = 0; i < 16U; i++)
        PSX_BIOS->fcbs[i].fcbNumber = i;

    biosAddNullTty();

    /* Pre-open stdin (fileDesc 0) and stdout (fileDesc 1) on tty00: */
    PSX_BIOS->fcbs[0].status = 1U;
    PSX_BIOS->fcbs[0].dcb = &PSX_BIOS->dcbs[0];
    PSX_BIOS->fcbs[1].status = 2U;
    PSX_BIOS->fcbs[1].dcb = &PSX_BIOS->dcbs[0];
}

s32 biosAddDevice(const PsxDcb *dcb)
{
    u32 i;

    if (dcb == NULL || dcb->name == NULL)
        return 0;

    for (i = 0; i < 10U; i++) {
        if (PSX_BIOS->dcbs[i].name != NULL &&
            biosStrcmp(PSX_BIOS->dcbs[i].name, dcb->name) == 0) {
            PSX_BIOS->dcbs[i] = *dcb;
            if (dcb->initFn != NULL)
                dcb->initFn();
            return 1;
        }
    }

    for (i = 0; i < 10U; i++) {
        if (PSX_BIOS->dcbs[i].name == NULL) {
            PSX_BIOS->dcbs[i] = *dcb;
            if (dcb->initFn != NULL)
                dcb->initFn();
            return 1;
        }
    }
    return 0;
}

s32 biosDelDevice(const char *name)
{
    u32 i;

    if (name == NULL)
        return 0;
    for (i = 0; i < 10U; i++) {
        if (PSX_BIOS->dcbs[i].name != NULL &&
            biosStrcmp(PSX_BIOS->dcbs[i].name, name) == 0) {
            if (PSX_BIOS->dcbs[i].removeFn != NULL)
                PSX_BIOS->dcbs[i].removeFn();
            biosMemset(&PSX_BIOS->dcbs[i], 0, sizeof(PsxDcb));
            return 1;
        }
    }
    return 0;
}

void biosPrintDevices(void)
{
    u32 i;

    for (i = 0; i < 10U; i++) {
        if (PSX_BIOS->dcbs[i].name != NULL)
            biosPrintf("  %s: %s\n", PSX_BIOS->dcbs[i].name,
                       PSX_BIOS->dcbs[i].description ? PSX_BIOS->dcbs[i].description : "");
    }
}

static PsxDcb *resolveDevice(const char *path, const char **relPathOut, u32 *diskIdOut)
{
    char devBuf[16];
    const char *colon;
    u32 nameLen;
    u32 i;
    u32 diskId = 0;

    if (path == NULL)
        return NULL;

    colon = biosStrchr(path, ':');
    if (colon == NULL) {
        if (relPathOut != NULL)
            *relPathOut = path;
        if (diskIdOut != NULL)
            *diskIdOut = 0U;
        for (i = 0; i < 10U; i++) {
            if (PSX_BIOS->dcbs[i].name != NULL &&
                biosStrcmp(PSX_BIOS->dcbs[i].name, "cdrom") == 0)
                return &PSX_BIOS->dcbs[i];
        }
        return NULL;
    }

    nameLen = (u32)(colon - path);
    if (nameLen >= sizeof(devBuf))
        nameLen = sizeof(devBuf) - 1U;
    biosMemcpy(devBuf, path, nameLen);
    devBuf[nameLen] = '\0';

    /* Strip optional trailing two-digit unit number (e.g. bu00:, bu10:, tty00:) */
    if (nameLen >= 2U && devBuf[nameLen - 1U] >= '0' && devBuf[nameLen - 1U] <= '9' &&
        devBuf[nameLen - 2U] >= '0' && devBuf[nameLen - 2U] <= '9') {
        diskId = (u32)(devBuf[nameLen - 2U] - '0') * 16U + (u32)(devBuf[nameLen - 1U] - '0');
        devBuf[nameLen - 2U] = '\0';
    }

    if (relPathOut != NULL)
        *relPathOut = colon + 1;
    if (diskIdOut != NULL)
        *diskIdOut = diskId;

    for (i = 0; i < 10U; i++) {
        if (PSX_BIOS->dcbs[i].name != NULL &&
            biosStrcmp(PSX_BIOS->dcbs[i].name, devBuf) == 0)
            return &PSX_BIOS->dcbs[i];
    }
    return NULL;
}

s32 biosFileOpen(const char *path, u32 mode)
{
    const char *relPath = NULL;
    u32 diskId = 0;
    PsxDcb *dcb = resolveDevice(path, &relPath, &diskId);
    u32 i;

    if (dcb == NULL || dcb->openFn == NULL) {
        PSX_BIOS->lastError = 19U;
        return -1;
    }

    for (i = 2U; i < 16U; i++) {
        PsxFcb *fileCb = &PSX_BIOS->fcbs[i];
        if (fileCb->status == 0U) {
            biosMemset(fileCb, 0, sizeof(*fileCb));
            fileCb->fcbNumber = i;
            fileCb->status = (mode != 0U) ? mode : 1U;
            fileCb->diskId = diskId;
            fileCb->dcb = dcb;
            if (dcb->openFn(fileCb, relPath, mode) < 0) {
                fileCb->status = 0U;
                PSX_BIOS->lastError = fileCb->error ? fileCb->error : 2U;
                return -1;
            }
            return (s32)i;
        }
    }

    PSX_BIOS->lastError = 24U;
    return -1;
}

s32 biosFileClose(s32 fileDesc)
{
    PsxFcb *fileCb;

    if (fileDesc < 0 || fileDesc >= 16)
        return -1;
    fileCb = &PSX_BIOS->fcbs[fileDesc];
    if (fileCb->status == 0U)
        return -1;
    if (fileCb->dcb != NULL && fileCb->dcb->closeFn != NULL)
        fileCb->dcb->closeFn(fileCb);
    fileCb->status = 0U;
    return fileDesc;
}

s32 biosFileRead(s32 fileDesc, void *dst, u32 len)
{
    PsxFcb *fileCb;

    if (fileDesc < 0 || fileDesc >= 16 || dst == NULL)
        return -1;
    fileCb = &PSX_BIOS->fcbs[fileDesc];
    if (fileCb->status == 0U || fileCb->dcb == NULL || fileCb->dcb->readFn == NULL)
        return -1;
    return fileCb->dcb->readFn(fileCb, dst, len);
}

s32 biosFileWrite(s32 fileDesc, const void *src, u32 len)
{
    PsxFcb *fileCb;

    if (fileDesc < 0 || fileDesc >= 16 || src == NULL)
        return -1;
    fileCb = &PSX_BIOS->fcbs[fileDesc];
    if (fileCb->status == 0U || fileCb->dcb == NULL || fileCb->dcb->writeFn == NULL)
        return -1;
    return fileCb->dcb->writeFn(fileCb, src, len);
}

s32 biosFileSeek(s32 fileDesc, s32 offset, s32 whence)
{
    PsxFcb *fileCb;
    s32 target;

    if (fileDesc < 0 || fileDesc >= 16)
        return -1;
    fileCb = &PSX_BIOS->fcbs[fileDesc];
    if (fileCb->status == 0U)
        return -1;

    if (whence == 0)
        target = offset;
    else if (whence == 1)
        target = (s32)fileCb->position + offset;
    else if (whence == 2)
        target = (s32)fileCb->fileSize + offset;
    else
        return -1;

    if (target < 0)
        return -1;
    fileCb->position = (u32)target;
    return target;
}

s32 biosFileGetc(s32 fileDesc)
{
    u8 byteVal = 0;

    if (biosFileRead(fileDesc, &byteVal, 1U) != 1)
        return -1;
    return (s32)byteVal;
}

s32 biosFilePutc(s32 ch, s32 fileDesc)
{
    u8 byteVal = (u8)ch;

    if (biosFileWrite(fileDesc, &byteVal, 1U) != 1)
        return -1;
    return (s32)byteVal;
}

s32 biosFileDelete(const char *path)
{
    const char *relPath = NULL;
    u32 diskId = 0;
    PsxDcb *dcb = resolveDevice(path, &relPath, &diskId);
    PsxFcb tempFcb;

    if (dcb == NULL || dcb->eraseFn == NULL)
        return 0;
    biosMemset(&tempFcb, 0, sizeof(tempFcb));
    tempFcb.diskId = diskId;
    tempFcb.dcb = dcb;
    return (dcb->eraseFn(&tempFcb, relPath) >= 0) ? 1 : 0;
}

s32 biosFileRename(const char *oldPath, const char *newPath)
{
    const char *oldRel = NULL;
    const char *newRel = NULL;
    u32 diskId = 0;
    PsxDcb *dcb = resolveDevice(oldPath, &oldRel, &diskId);
    PsxFcb tempFcb;

    resolveDevice(newPath, &newRel, NULL);
    if (dcb == NULL || dcb->renameFn == NULL)
        return 0;
    biosMemset(&tempFcb, 0, sizeof(tempFcb));
    tempFcb.diskId = diskId;
    tempFcb.dcb = dcb;
    return (dcb->renameFn(&tempFcb, oldRel, newRel) >= 0) ? 1 : 0;
}

s32 biosFileFormat(const char *devName)
{
    const char *relPath = NULL;
    u32 diskId = 0;
    PsxDcb *dcb = resolveDevice(devName, &relPath, &diskId);
    PsxFcb tempFcb;

    if (dcb == NULL || dcb->formatFn == NULL)
        return 0;
    biosMemset(&tempFcb, 0, sizeof(tempFcb));
    tempFcb.diskId = diskId;
    tempFcb.dcb = dcb;
    return (dcb->formatFn(&tempFcb) >= 0) ? 1 : 0;
}

static PsxFcb g_dirSearchFcb;

PsxDirEntry *biosFirstFile(const char *pattern, PsxDirEntry *out)
{
    const char *relPath = NULL;
    u32 diskId = 0;
    PsxDcb *dcb = resolveDevice(pattern, &relPath, &diskId);

    if (dcb == NULL || dcb->firstFileFn == NULL || out == NULL)
        return NULL;
    biosMemset(&g_dirSearchFcb, 0, sizeof(g_dirSearchFcb));
    g_dirSearchFcb.diskId = diskId;
    g_dirSearchFcb.dcb = dcb;
    return dcb->firstFileFn(&g_dirSearchFcb, relPath, out);
}

PsxDirEntry *biosNextFile(PsxDirEntry *out)
{
    if (g_dirSearchFcb.dcb == NULL || g_dirSearchFcb.dcb->nextFileFn == NULL || out == NULL)
        return NULL;
    return g_dirSearchFcb.dcb->nextFileFn(&g_dirSearchFcb, out);
}

s32 biosTestDevice(const char *devName)
{
    const char *relPath = NULL;
    u32 diskId = 0;
    PsxDcb *dcb = resolveDevice(devName, &relPath, &diskId);
    PsxFcb tempFcb;

    if (dcb == NULL || dcb->testDeviceFn == NULL)
        return -1;
    biosMemset(&tempFcb, 0, sizeof(tempFcb));
    tempFcb.diskId = diskId;
    tempFcb.dcb = dcb;
    return dcb->testDeviceFn(&tempFcb);
}

s32 biosGetLastError(void)
{
    return (s32)PSX_BIOS->lastError;
}

s32 biosGetLastFileError(s32 fileDesc)
{
    if (fileDesc < 0 || fileDesc >= 16)
        return (s32)PSX_BIOS->lastError;
    return (s32)PSX_BIOS->fcbs[fileDesc].error;
}
