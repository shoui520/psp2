/*
 * PlayStation 1 BIOS SIO0 Controller Pad Driver Ver 3.0 and Memory Card (bu00:/bu10:) driver.
 */

#include "bios_internal.h"

static u8 sioTransferByte(u8 txByte, s32 waitAck)
{
    u32 timeout = 2000U;
    u8 rxByte;

    PSX_IO8(REG_JOY_DATA) = txByte;
    while ((PSX_IO16(REG_JOY_STAT) & 0x0002U) == 0U && timeout > 0U)
        timeout--;
    rxByte = PSX_IO8(REG_JOY_DATA);

    if (waitAck) {
        timeout = 2000U;
        while ((PSX_IO16(REG_JOY_STAT) & 0x0080U) == 0U && timeout > 0U)
            timeout--;
        PSX_IO16(REG_JOY_CTRL) |= 0x0010U;
    }
    return rxByte;
}

static void sioSelectPort(u32 port)
{
    u16 ctrl = 0x0003U; /* TXEN | DTR */

    if (port != 0U)
        ctrl |= 0x2000U;
    PSX_IO16(REG_JOY_MODE) = 0x000DU;
    PSX_IO16(REG_JOY_BAUD) = 0x0088U;
    PSX_IO16(REG_JOY_CTRL) = ctrl;
}

static void sioDeselect(void)
{
    PSX_IO16(REG_JOY_CTRL) = 0x0000U;
}

static void pollSinglePad(u32 port, u8 *buf, u32 maxLen)
{
    u8 idByte;
    u8 readyByte;
    u32 extraBytes;
    u32 i;

    if (buf == NULL || maxLen < 4U)
        return;

    sioSelectPort(port);
    sioTransferByte(0x01U, 1);
    idByte = sioTransferByte(0x42U, 1);
    readyByte = sioTransferByte(0x00U, 1);

    if (readyByte != 0x5AU) {
        buf[0] = 0xFFU;
        buf[1] = 0xFFU;
        sioDeselect();
        return;
    }

    buf[0] = 0x00U;
    buf[1] = idByte;
    extraBytes = (u32)(idByte & 0x0FU) * 2U;
    if (extraBytes + 2U > maxLen)
        extraBytes = maxLen - 2U;

    for (i = 0; i < extraBytes; i++)
        buf[2U + i] = sioTransferByte(0x00U, (i + 1U < extraBytes) ? 1 : 0);

    sioDeselect();
}

void biosPadPollVblank(void)
{
    if (PSX_BIOS->padBufPortA != NULL)
        pollSinglePad(0U, PSX_BIOS->padBufPortA, PSX_BIOS->padBufLenA);
    if (PSX_BIOS->padBufPortB != NULL)
        pollSinglePad(1U, PSX_BIOS->padBufPortB, PSX_BIOS->padBufLenB);
}

s32 biosInitPad(u8 *bufA, u32 lenA, u8 *bufB, u32 lenB)
{
    PSX_BIOS->padBufPortA = bufA;
    PSX_BIOS->padBufLenA = lenA;
    PSX_BIOS->padBufPortB = bufB;
    PSX_BIOS->padBufLenB = lenB;
    PSX_BIOS->padRunning = 0U;
    return 1;
}

s32 biosStartPad(void)
{
    PSX_BIOS->padRunning = 1U;
    PSX_IO32(REG_I_MASK) |= (1U << PSX_IRQ_VBLANK);
    return 1;
}

void biosStopPad(void)
{
    PSX_BIOS->padRunning = 0U;
}

void biosPadInitDirect(u8 *bufA, u8 *bufB)
{
    biosInitPad(bufA, 34U, bufB, 34U);
}

void biosPadStartCom(void)
{
    biosStartPad();
}

void biosPadStopCom(void)
{
    biosStopPad();
}

/* Memory card sector read/write over SIO0 (128-byte frames, 1024 blocks = 128 KB) */
static s32 cardFrameRead(u32 port, u32 block, u8 *dst128)
{
    u8 idA;
    u8 idB;
    u8 msb = (u8)((block >> 8) & 0xFFU);
    u8 lsb = (u8)(block & 0xFFU);
    u8 ackMsb;
    u8 ackLsb;
    u8 checksum;
    u8 endByte;
    u32 i;

    sioSelectPort(port);
    sioTransferByte(0x81U, 1);
    sioTransferByte(0x52U, 1); /* Read command */
    idA = sioTransferByte(0x00U, 1);
    idB = sioTransferByte(0x00U, 1);
    if (idA != 0x5AU || idB != 0x5DU) {
        sioDeselect();
        return -1;
    }

    sioTransferByte(msb, 1);
    sioTransferByte(lsb, 1);
    sioTransferByte(0x00U, 1); /* CMD ACK1 */
    sioTransferByte(0x00U, 1); /* CMD ACK2 */
    ackMsb = sioTransferByte(0x00U, 1);
    ackLsb = sioTransferByte(0x00U, 1);
    (void)ackMsb;
    (void)ackLsb;

    for (i = 0; i < 128U; i++)
        dst128[i] = sioTransferByte(0x00U, 1);
    checksum = sioTransferByte(0x00U, 1);
    endByte = sioTransferByte(0x00U, 0);
    sioDeselect();
    (void)checksum;
    return (endByte == 0x47U) ? 0 : -1;
}

static s32 cardFrameWrite(u32 port, u32 block, const u8 *src128)
{
    u8 idA;
    u8 idB;
    u8 msb = (u8)((block >> 8) & 0xFFU);
    u8 lsb = (u8)(block & 0xFFU);
    u8 xorSum = (u8)(msb ^ lsb);
    u8 endByte;
    u32 i;

    sioSelectPort(port);
    sioTransferByte(0x81U, 1);
    sioTransferByte(0x57U, 1); /* Write command */
    idA = sioTransferByte(0x00U, 1);
    idB = sioTransferByte(0x00U, 1);
    if (idA != 0x5AU || idB != 0x5DU) {
        sioDeselect();
        return -1;
    }

    sioTransferByte(msb, 1);
    sioTransferByte(lsb, 1);
    for (i = 0; i < 128U; i++) {
        xorSum ^= src128[i];
        sioTransferByte(src128[i], 1);
    }
    sioTransferByte(xorSum, 1);
    sioTransferByte(0x00U, 1);
    sioTransferByte(0x00U, 1);
    endByte = sioTransferByte(0x00U, 0);
    sioDeselect();
    return (endByte == 0x47U) ? 0 : -1;
}

void biosInitCard(s32 padEnable)
{
    PSX_BIOS->cardInited = 1U;
    PSX_BIOS->cardRunning = 0U;
    if (padEnable != 0)
        PSX_BIOS->padRunning = 1U;
}

s32 biosStartCard(void)
{
    PSX_BIOS->cardRunning = 1U;
    return 1;
}

s32 biosStopCard(void)
{
    PSX_BIOS->cardRunning = 0U;
    return 1;
}

void biosBuInit(void)
{
    biosCardRegisterDevice();
}

s32 biosCardInfo(s32 chan)
{
    u8 probe[128];
    u32 port = ((u32)chan >> 4) & 1U;

    if (cardFrameRead(port, 0U, probe) < 0) {
        PSX_BIOS->cardLastStatus = -1;
        biosDeliverEvent(EV_CLASS_CARD_HW, EV_SPEC_TIMEOUT);
        biosDeliverEvent(EV_CLASS_CARD_SW, EV_SPEC_TIMEOUT);
        return 0;
    }
    PSX_BIOS->cardLastStatus = 0;
    biosDeliverEvent(EV_CLASS_CARD_HW, EV_SPEC_IOE);
    biosDeliverEvent(EV_CLASS_CARD_SW, EV_SPEC_IOE);
    return 1;
}

s32 biosCardLoad(s32 chan)
{
    return biosCardInfo(chan);
}

s32 biosCardRead(s32 chan, u32 block, u8 *dst)
{
    u32 port = ((u32)chan >> 4) & 1U;

    if (dst == NULL || cardFrameRead(port, block, dst) < 0) {
        biosDeliverEvent(EV_CLASS_CARD_HW, EV_SPEC_ERROR);
        return 0;
    }
    biosDeliverEvent(EV_CLASS_CARD_HW, EV_SPEC_IOE);
    return 1;
}

s32 biosCardWrite(s32 chan, u32 block, const u8 *src)
{
    u32 port = ((u32)chan >> 4) & 1U;

    if (src == NULL || cardFrameWrite(port, block, src) < 0) {
        biosDeliverEvent(EV_CLASS_CARD_HW, EV_SPEC_ERROR);
        return 0;
    }
    biosDeliverEvent(EV_CLASS_CARD_HW, EV_SPEC_IOE);
    return 1;
}

s32 biosCardStatus(s32 chan)
{
    (void)chan;
    return PSX_BIOS->cardLastStatus;
}

s32 biosCardClear(s32 chan)
{
    (void)chan;
    return 1;
}

s32 biosCardAuto(s32 flag)
{
    s32 prev = (s32)PSX_BIOS->cardAutoFormat;
    PSX_BIOS->cardAutoFormat = (flag != 0) ? 1U : 0U;
    return prev;
}

void biosNewCard(void)
{
    PSX_BIOS->cardLastStatus = 0;
}

/* Memory card (bu00:/bu10:) DCB handlers over 15 x 8KB block directory */
static s32 cardFindSlot(u32 port, const char *name, u32 *slotOut, u32 *sizeOut)
{
    u8 dirFrame[128];
    u32 slot;

    for (slot = 1U; slot <= 15U; slot++) {
        if (cardFrameRead(port, slot, dirFrame) < 0)
            return -1;
        if (dirFrame[0] == 0x51U && biosStrncmp((const char *)&dirFrame[10], name, 20U) == 0) {
            if (slotOut != NULL)
                *slotOut = slot;
            if (sizeOut != NULL) {
                *sizeOut = (u32)dirFrame[4] | ((u32)dirFrame[5] << 8) |
                           ((u32)dirFrame[6] << 16) | ((u32)dirFrame[7] << 24);
            }
            return 0;
        }
    }
    return -1;
}

static s32 cardDevOpen(void *fcbPtr, const char *path, u32 mode)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;
    u32 port = (fileCb->diskId >> 4) & 1U;
    u32 slot = 0;
    u32 size = 0;

    (void)mode;
    if (cardFindSlot(port, path, &slot, &size) < 0) {
        fileCb->error = 2U;
        return -1;
    }
    fileCb->startLba = slot * 64U;
    fileCb->fileSize = size;
    fileCb->position = 0U;
    return 0;
}

static s32 cardDevClose(void *fcbPtr)
{
    (void)fcbPtr;
    return 0;
}

static s32 cardDevRead(void *fcbPtr, void *dst, u32 len)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;
    u32 port = (fileCb->diskId >> 4) & 1U;
    u8 *out = (u8 *)dst;
    u32 done = 0U;
    u8 frame[128];

    if (fileCb->position >= fileCb->fileSize)
        return 0;
    if (len > fileCb->fileSize - fileCb->position)
        len = fileCb->fileSize - fileCb->position;

    while (done < len) {
        u32 block = fileCb->startLba + (fileCb->position >> 7);
        u32 off = fileCb->position & 127U;
        u32 chunk = 128U - off;
        if (chunk > len - done)
            chunk = len - done;
        if (cardFrameRead(port, block, frame) < 0)
            return -1;
        biosMemcpy(out + done, &frame[off], chunk);
        fileCb->position += chunk;
        done += chunk;
    }
    return (s32)done;
}

static s32 cardDevWrite(void *fcbPtr, const void *src, u32 len)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;
    u32 port = (fileCb->diskId >> 4) & 1U;
    const u8 *in = (const u8 *)src;
    u32 done = 0U;
    u8 frame[128];

    while (done < len) {
        u32 block = fileCb->startLba + (fileCb->position >> 7);
        u32 off = fileCb->position & 127U;
        u32 chunk = 128U - off;
        if (chunk > len - done)
            chunk = len - done;
        if (off != 0U || chunk < 128U) {
            if (cardFrameRead(port, block, frame) < 0)
                biosMemset(frame, 0, sizeof(frame));
        }
        biosMemcpy(&frame[off], in + done, chunk);
        if (cardFrameWrite(port, block, frame) < 0)
            return -1;
        fileCb->position += chunk;
        done += chunk;
    }
    return (s32)done;
}

static s32 cardDevFormat(void *fcbPtr)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;
    u32 port = (fileCb->diskId >> 4) & 1U;
    u8 frame[128];
    u32 slot;

    biosMemset(frame, 0, sizeof(frame));
    frame[0] = 'M';
    frame[1] = 'C';
    frame[127] = 0x0EU;
    if (cardFrameWrite(port, 0U, frame) < 0)
        return -1;

    biosMemset(frame, 0, sizeof(frame));
    frame[0] = 0xA0U;
    frame[8] = 0xFFU;
    frame[9] = 0xFFU;
    frame[127] = 0xA0U;
    for (slot = 1U; slot <= 15U; slot++) {
        if (cardFrameWrite(port, slot, frame) < 0)
            return -1;
    }
    return 0;
}

static PsxDirEntry *cardDevNextFile(void *fcbPtr, PsxDirEntry *out)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;
    u32 port = (fileCb->diskId >> 4) & 1U;
    u8 frame[128];

    while (fileCb->dirIndex < 15U) {
        u32 slot = ++fileCb->dirIndex;
        if (cardFrameRead(port, slot, frame) < 0)
            return NULL;
        if (frame[0] == 0x51U) {
            biosMemcpy(out->name, &frame[10], 20U);
            out->attr = (s32)frame[0];
            out->size = (s32)((u32)frame[4] | ((u32)frame[5] << 8) |
                              ((u32)frame[6] << 16) | ((u32)frame[7] << 24));
            out->next = NULL;
            out->headLba = (s32)slot;
            out->systemInfo = 0U;
            return out;
        }
    }
    return NULL;
}

static PsxDirEntry *cardDevFirstFile(void *fcbPtr, const char *pattern, PsxDirEntry *out)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;

    (void)pattern;
    fileCb->dirIndex = 0U;
    return cardDevNextFile(fileCb, out);
}

static s32 cardDevTest(void *fcbPtr)
{
    PsxFcb *fileCb = (PsxFcb *)fcbPtr;
    u32 port = (fileCb->diskId >> 4) & 1U;
    u8 header[128];

    if (cardFrameRead(port, 0U, header) < 0)
        return -1;
    return (header[0] == 'M' && header[1] == 'C') ? 0 : -1;
}

static const PsxDcb g_cardDcb = {
    "bu",
    0x00000014U,
    128U,
    "MEMORY CARD",
    NULL,
    cardDevOpen,
    NULL,
    cardDevClose,
    NULL,
    cardDevRead,
    cardDevWrite,
    NULL,
    NULL,
    cardDevFirstFile,
    cardDevNextFile,
    cardDevFormat,
    NULL,
    NULL,
    NULL,
    cardDevTest
};

void biosCardRegisterDevice(void)
{
    biosAddDevice(&g_cardDcb);
}
