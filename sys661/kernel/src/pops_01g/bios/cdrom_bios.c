/*
 * PlayStation 1 BIOS CD-ROM controller driver (0x1F801800..0x1F801803, DMA ch3)
 * and ISO9660 cdrom: filesystem driver.
 */

#include "bios_internal.h"

#define CD_CMD_GETSTAT   0x01U
#define CD_CMD_SETLOC    0x02U
#define CD_CMD_READN     0x06U
#define CD_CMD_STOP      0x08U
#define CD_CMD_PAUSE     0x09U
#define CD_CMD_INIT      0x0AU
#define CD_CMD_SETMODE   0x0EU
#define CD_CMD_SEEKL     0x15U
#define CD_CMD_SETSESS   0x1FU

static u8 toBcd(u32 val)
{
    return (u8)(((val / 10U) << 4) | (val % 10U));
}

static void lbaToMsf(u32 lba, u8 msf[3])
{
    u32 total = lba + 150U;
    u32 secs = total / 75U;
    u32 frames = total % 75U;
    u32 mins = secs / 60U;

    secs %= 60U;
    msf[0] = toBcd(mins);
    msf[1] = toBcd(secs);
    msf[2] = toBcd(frames);
}

static void cdWaitBusy(void)
{
    while ((PSX_IO8(REG_CD_INDEX) & 0x80U) != 0U)
        ;
}

static u8 cdWaitIrqAndAck(u8 *respOut, u32 maxResp)
{
    u8 irqFlags;
    u32 count = 0;

    for (;;) {
        PSX_IO8(REG_CD_INDEX) = 1U;
        irqFlags = PSX_IO8(REG_CD_PORT3) & 0x07U;
        if (irqFlags != 0U)
            break;
    }

    while ((PSX_IO8(REG_CD_INDEX) & 0x20U) != 0U) {
        u8 b = PSX_IO8(REG_CD_PORT1);
        if (respOut != NULL && count < maxResp)
            respOut[count] = b;
        count++;
    }

    PSX_IO8(REG_CD_INDEX) = 1U;
    PSX_IO8(REG_CD_PORT3) = 0x07U;
    return irqFlags;
}

static void cdSendCommand(u8 cmd, const u8 *params, u32 paramLen)
{
    u32 i;

    cdWaitBusy();
    PSX_IO8(REG_CD_INDEX) = 0U;
    for (i = 0; i < paramLen; i++)
        PSX_IO8(REG_CD_PORT2) = params[i];
    PSX_IO8(REG_CD_PORT1) = cmd;
}

static s32 cdReadSingleSector(u32 lba, u8 *dst)
{
    u8 msf[3];
    u8 resp[4];
    u8 irq;
    u32 i;

    lbaToMsf(lba, msf);
    cdSendCommand(CD_CMD_SETLOC, msf, 3U);
    cdWaitIrqAndAck(resp, 4U);

    cdSendCommand(CD_CMD_READN, NULL, 0U);
    cdWaitIrqAndAck(resp, 4U);

    irq = cdWaitIrqAndAck(resp, 4U);
    if (irq == 5U) {
        cdSendCommand(CD_CMD_PAUSE, NULL, 0U);
        cdWaitIrqAndAck(resp, 4U);
        return -1;
    }

    /* Request sector data and transfer 2048 bytes via DMA ch3 */
    PSX_IO8(REG_CD_INDEX) = 0U;
    PSX_IO8(REG_CD_PORT3) = 0x80U;
    while ((PSX_IO8(REG_CD_INDEX) & 0x40U) == 0U)
        ;

    if (((u32)dst & 3U) == 0U) {
        PSX_IO32(REG_DMA_CD_MADR) = (u32)dst & 0x00FFFFFFU;
        PSX_IO32(REG_DMA_CD_BCR) = 0x00010200U; /* 512 words = 2048 bytes */
        PSX_IO32(REG_DMA_CD_CHCR) = 0x11000000U;
        while ((PSX_IO32(REG_DMA_CD_CHCR) & 0x01000000U) != 0U)
            ;
    } else {
        for (i = 0; i < 2048U; i++)
            dst[i] = PSX_IO8(REG_CD_PORT2);
    }

    cdSendCommand(CD_CMD_PAUSE, NULL, 0U);
    cdWaitIrqAndAck(resp, 4U);
    cdWaitIrqAndAck(resp, 4U);
    return 0;
}

static u32 readLe32(const u8 *p)
{
    return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24);
}

static s32 cdLoadVolumeDescriptor(void)
{
    u8 *sec = PSX_BIOS->sectorBuf;

    if (cdReadSingleSector(16U, sec) < 0)
        return -1;
    if (sec[0] != 1U || biosMemcmp(&sec[1], "CD001", 5U) != 0)
        return -1;

    /* Root directory record sits at offset 156 of the ISO9660 PVD */
    PSX_BIOS->cdRootDirLba = readLe32(&sec[156 + 2]);
    PSX_BIOS->cdRootDirSize = readLe32(&sec[156 + 10]);
    return 0;
}

static s32 cdNameMatches(const char *isoName, u32 isoLen, const char *target, u32 targetLen)
{
    u32 i;

    if (isoLen != targetLen) {
        /* Allow omitting ";1" version suffix in caller path */
        if (!(isoLen == targetLen + 2U && isoName[targetLen] == ';' && isoName[targetLen + 1U] == '1'))
            return 0;
    }
    for (i = 0; i < targetLen; i++) {
        if (biosToupper((u8)isoName[i]) != biosToupper((u8)target[i]))
            return 0;
    }
    return 1;
}

static s32 cdLookupPath(const char *path, u32 *lbaOut, u32 *sizeOut)
{
    u32 dirLba;
    u32 dirSize;
    const char *cursor = path;

    if (PSX_BIOS->cdRootDirLba == 0U && cdLoadVolumeDescriptor() < 0)
        return -1;

    dirLba = PSX_BIOS->cdRootDirLba;
    dirSize = PSX_BIOS->cdRootDirSize;

    while (*cursor == '\\' || *cursor == '/')
        cursor++;

    while (*cursor != '\0') {
        const char *sep = cursor;
        u32 segLen;
        u32 remaining = dirSize;
        u32 curLba = dirLba;
        s32 found = 0;

        while (*sep != '\0' && *sep != '\\' && *sep != '/')
            sep++;
        segLen = (u32)(sep - cursor);

        while (remaining > 0U && !found) {
            u32 off = 0U;
            if (cdReadSingleSector(curLba, PSX_BIOS->sectorBuf) < 0)
                return -1;
            while (off < 2048U) {
                u8 recLen = PSX_BIOS->sectorBuf[off];
                u8 nameLen;
                if (recLen == 0U)
                    break;
                nameLen = PSX_BIOS->sectorBuf[off + 32U];
                if (cdNameMatches((const char *)&PSX_BIOS->sectorBuf[off + 33U], nameLen, cursor, segLen)) {
                    dirLba = readLe32(&PSX_BIOS->sectorBuf[off + 2U]);
                    dirSize = readLe32(&PSX_BIOS->sectorBuf[off + 10U]);
                    found = 1;
                    break;
                }
                off += recLen;
            }
            curLba++;
            remaining = (remaining > 2048U) ? (remaining - 2048U) : 0U;
        }

        if (!found)
            return -1;

        cursor = sep;
        while (*cursor == '\\' || *cursor == '/')
            cursor++;
    }

    if (lbaOut != NULL)
        *lbaOut = dirLba;
    if (sizeOut != NULL)
        *sizeOut = dirSize;
    return 0;
}

s32 biosCdInit(void)
{
    u8 resp[4];
    u8 mode = 0x80U; /* Double speed */

    PSX_IO8(REG_CD_INDEX) = 1U;
    PSX_IO8(REG_CD_PORT2) = 0x1FU;
    PSX_IO8(REG_CD_PORT3) = 0x1FU;
    PSX_IO32(REG_DPCR) |= 0x00008000U; /* Enable DMA ch3 (CD-ROM) */

    cdSendCommand(CD_CMD_INIT, NULL, 0U);
    cdWaitIrqAndAck(resp, 4U);
    cdWaitIrqAndAck(resp, 4U);

    cdSendCommand(CD_CMD_SETMODE, &mode, 1U);
    cdWaitIrqAndAck(resp, 4U);

    PSX_BIOS->cdInited = 1U;
    PSX_BIOS->cdRootDirLba = 0U;
    PSX_BIOS->cdRootDirSize = 0U;
    PSX_BIOS->cdSectorMode = mode;
    return 1;
}

s32 biosCdRemove(void)
{
    PSX_BIOS->cdInited = 0U;
    return 1;
}

s32 biosCdAsyncSeekL(const u8 *msf)
{
    u8 resp[4];

    if (msf == NULL)
        return 0;
    cdSendCommand(CD_CMD_SETLOC, msf, 3U);
    cdWaitIrqAndAck(resp, 4U);
    cdSendCommand(CD_CMD_SEEKL, NULL, 0U);
    cdWaitIrqAndAck(resp, 4U);
    cdWaitIrqAndAck(resp, 4U);
    biosDeliverEvent(EV_CLASS_CDROM, EV_SPEC_COMMAND);
    return 1;
}

s32 biosCdAsyncGetStatus(u8 *dst)
{
    u8 resp[4] = { 0 };

    cdSendCommand(CD_CMD_GETSTAT, NULL, 0U);
    cdWaitIrqAndAck(resp, 4U);
    if (dst != NULL)
        dst[0] = resp[0];
    PSX_BIOS->cdStatusResult[0] = resp[0];
    biosDeliverEvent(EV_CLASS_CDROM, EV_SPEC_COMMAND);
    return 1;
}

s32 biosCdAsyncSetMode(u32 mode)
{
    u8 m = (u8)mode;
    u8 resp[4];

    PSX_BIOS->cdSectorMode = m;
    cdSendCommand(CD_CMD_SETMODE, &m, 1U);
    cdWaitIrqAndAck(resp, 4U);
    biosDeliverEvent(EV_CLASS_CDROM, EV_SPEC_COMMAND);
    return 1;
}

s32 biosCdReadSector(s32 count, u32 lba, void *dst)
{
    u8 *out = (u8 *)dst;
    s32 i;

    if (count <= 0 || dst == NULL)
        return -1;
    for (i = 0; i < count; i++) {
        if (cdReadSingleSector(lba + (u32)i, out + ((u32)i * 2048U)) < 0)
            return i;
    }
    return count;
}

s32 biosCdAsyncReadSector(s32 count, void *dst, u32 mode)
{
    biosCdAsyncSetMode(mode);
    if (biosCdReadSector(count, PSX_BIOS->cdAsyncLba, dst) < 0) {
        biosDeliverEvent(EV_CLASS_CDROM, EV_SPEC_ERROR);
        return 0;
    }
    biosDeliverEvent(EV_CLASS_CDROM, EV_SPEC_COMMAND);
    return 1;
}

s32 biosCdGetLbn(const char *path)
{
    u32 lba = 0;

    if (path == NULL)
        return -1;
    if (cdLookupPath(path, &lba, NULL) < 0)
        return -1;
    return (s32)lba;
}

s32 biosCdGetStatus(void)
{
    u8 st = 0;

    biosCdAsyncGetStatus(&st);
    return (s32)st;
}

s32 biosCdStop(void)
{
    u8 resp[4];

    cdSendCommand(CD_CMD_STOP, NULL, 0U);
    cdWaitIrqAndAck(resp, 4U);
    cdWaitIrqAndAck(resp, 4U);
    return 1;
}

s32 biosCdReset(void)
{
    return biosCdInit();
}

s32 biosCdSetSession(u32 session)
{
    u8 s = (u8)session;
    u8 resp[4];

    cdSendCommand(CD_CMD_SETSESS, &s, 1U);
    cdWaitIrqAndAck(resp, 4U);
    cdWaitIrqAndAck(resp, 4U);
    return 1;
}

void biosCdHandleIrq(void)
{
    u8 irqFlags;

    PSX_IO8(REG_CD_INDEX) = 1U;
    irqFlags = PSX_IO8(REG_CD_PORT3) & 0x07U;
    if (irqFlags == 0U)
        return;

    while ((PSX_IO8(REG_CD_INDEX) & 0x20U) != 0U)
        PSX_BIOS->cdStatusResult[0] = PSX_IO8(REG_CD_PORT1);
    PSX_IO8(REG_CD_INDEX) = 1U;
    PSX_IO8(REG_CD_PORT3) = 0x07U;

    if (irqFlags == 5U)
        biosDeliverEvent(EV_CLASS_CDROM, EV_SPEC_ERROR);
    else
        biosDeliverEvent(EV_CLASS_CDROM, EV_SPEC_COMMAND);
}

/* cdrom: device callbacks */
static s32 cdDevOpen(void *fcbPtr, const char *path, u32 mode)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;
    u32 lba = 0;
    u32 size = 0;

    (void)mode;
    if (cdLookupPath(path, &lba, &size) < 0) {
        fileCb->error = 2U;
        return -1;
    }
    fileCb->startLba = lba;
    fileCb->fileSize = size;
    fileCb->position = 0U;
    return 0;
}

static s32 cdDevClose(void *fcbPtr)
{
    (void)fcbPtr;
    return 0;
}

static s32 cdDevRead(void *fcbPtr, void *dst, u32 len)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;
    u8 *out = (u8 *)dst;
    u32 done = 0U;

    if (fileCb->position >= fileCb->fileSize)
        return 0;
    if (len > fileCb->fileSize - fileCb->position)
        len = fileCb->fileSize - fileCb->position;

    while (done < len) {
        u32 lba = fileCb->startLba + (fileCb->position >> 11);
        u32 secOff = fileCb->position & 2047U;
        u32 chunk = 2048U - secOff;
        if (chunk > len - done)
            chunk = len - done;

        if (secOff == 0U && chunk == 2048U) {
            if (cdReadSingleSector(lba, out + done) < 0)
                return -1;
        } else {
            if (cdReadSingleSector(lba, PSX_BIOS->sectorBuf) < 0)
                return -1;
            biosMemcpy(out + done, &PSX_BIOS->sectorBuf[secOff], chunk);
        }
        fileCb->position += chunk;
        done += chunk;
    }
    return (s32)done;
}

static const PsxDcb g_cdromDcb = {
    "cdrom",
    0x00000014U,
    2048U,
    "CD-ROM",
    NULL,
    cdDevOpen,
    NULL,
    cdDevClose,
    NULL,
    cdDevRead,
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

void biosCdRegisterDevice(void)
{
    biosAddDevice(&g_cdromDcb);
}
