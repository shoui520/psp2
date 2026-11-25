#include <common_imp.h>
#include <crypto/kirk.h>
#include <idstorage.h>
#include <interruptman.h>
#include <openpsid_ddrdb.h>
#include <sysmem_kernel.h>
#include <sysmem_sysclib.h>
#include <sysmem_sysevent.h>
#include <threadman_kernel.h>

typedef struct SceOpenPSID {
    u8 data[16];
} SceOpenPSID;

typedef struct SceProductCode {
    u8 data[2];
} SceProductCode;

extern SceUID g_semaId;
extern s32 sceRtc_driver_CEEF238F(u64 *pTick);

static const u8 g_pcactReqKey[16] = {
    0x38, 0x0A, 0x18, 0x0E, 0x36, 0x58, 0xBC, 0xC1,
    0x70, 0x64, 0x69, 0xE3, 0x29, 0x60, 0x81, 0xF9
};

static const u8 g_pcactRespKey[16] = {
    0x80, 0x5E, 0x67, 0x47, 0x7E, 0x76, 0x48, 0xDC,
    0xC3, 0x72, 0x76, 0xD1, 0x6C, 0xC8, 0xD5, 0x2D
};

static const u8 g_pcactSeed[16] = {
    0xCA, 0x0E, 0x20, 0x6E, 0xCE, 0xE6, 0xFB, 0x66,
    0x68, 0x28, 0x65, 0x42, 0xEF, 0x4F, 0xF8, 0x8F
};

static const u8 g_psidTicketKey[16] = {
    0x67, 0x1E, 0xA0, 0xAF, 0xD7, 0x23, 0x04, 0x88,
    0x2A, 0xFC, 0x06, 0x6A, 0xDE, 0x86, 0x8D, 0x2E
};

static const u8 g_psidTicketHdr[4] = {
    0x4D, 0x31, 0x6D, 0x01
};

static const u8 g_mlnpsnlReqKey[16] = {
    0x90, 0xAB, 0x41, 0xEE, 0xEA, 0x3E, 0xD1, 0xEC,
    0xD8, 0xC4, 0x92, 0xFD, 0x66, 0x15, 0x5E, 0x6C
};

static const u8 g_mlnpsnlRespKey[16] = {
    0x53, 0xCB, 0xCC, 0xAD, 0xB7, 0x6C, 0xBA, 0x3A,
    0x60, 0x9C, 0xF2, 0x9F, 0x31, 0x8D, 0x92, 0xE4
};

static const u8 g_mlnpsnlSeed[16] = {
    0xFE, 0x09, 0x4A, 0x99, 0x06, 0x96, 0x1A, 0x57,
    0xEB, 0xE0, 0x2B, 0x1D, 0xB2, 0x98, 0x2D, 0x84
};

static u8 *g_pKirkBuf;                           /* 0x3AC0 */
static s32 g_hasCustomTitleId;                   /* 0x3AC4 */
static u8 g_certBuf[0xC0] __attribute__((aligned(64)));   /* 0x3B00 */
static u8 g_ticketBuf[0x100] __attribute__((aligned(64))); /* 0x3BC0 */
static u32 g_customTitleId[9];                   /* 0x3CC0 */

/* 0x0000 */
static s32 kirkDecryptAesCbc(u8 *buf, u32 size, u32 keyId)
{
    KirkAESHeader *hdr = (KirkAESHeader *)buf;
    u32 totalSize = size + sizeof(KirkAESHeader);

    hdr->mode = 5;
    hdr->keyIndex = keyId;
    hdr->pad[0] = 0;
    hdr->pad[1] = 0;
    hdr->dataSize = size;
    g_pKirkBuf = buf;
    return sceUtilsBufferCopyWithRange(buf, totalSize, buf, totalSize, KIRK_CMD_DECRYPT_AES_CBC_IV_NONE);
}

/* 0x004C */
static s32 kirkEncryptAesCbc(u8 *buf, u32 size, u32 keyId)
{
    KirkAESHeader *hdr = (KirkAESHeader *)buf;
    u32 totalSize = size + sizeof(KirkAESHeader);

    hdr->mode = 4;
    hdr->keyIndex = keyId;
    hdr->pad[0] = 0;
    hdr->pad[1] = 0;
    hdr->dataSize = size;
    g_pKirkBuf = buf;
    return sceUtilsBufferCopyWithRange(buf, totalSize, buf, totalSize, KIRK_CMD_ENCRYPT_AES_CBC_IV_NONE);
}

/* 0x0098 */
static s32 kirkEncryptAesCbcFuse(u8 *buf, u32 size)
{
    KirkAESHeader *hdr = (KirkAESHeader *)buf;
    u32 totalSize = size + sizeof(KirkAESHeader);

    hdr->mode = 4;
    hdr->keyIndex = 0x100;
    hdr->pad[0] = 0;
    hdr->pad[1] = 0;
    hdr->dataSize = size;
    g_pKirkBuf = buf;
    return sceUtilsBufferCopyWithRange(buf, totalSize, buf, totalSize, KIRK_CMD_ENCRYPT_AES_CBC_IV_FUSE);
}

/* 0x00E8 */
static s32 loadAndVerifyOpenPsidCert(u8 *cert)
{
    if (sceIdStorageLookup(0x101, 0x1D0, cert, 0x30) < 0) {
        if (sceIdStorageLookup(0x121, 0x1D0, cert, 0x30) < 0)
            return 0xC0520002;
    }
    if (sceIdStorageLookup(0x102, 0, cert + 0x30, 0x88) < 0) {
        if (sceIdStorageLookup(0x122, 0, cert + 0x30, 0x88) < 0)
            return 0xC0520002;
    }
    if (sceUtilsBufferCopyWithRange(NULL, 0, cert, KIRK_CERT_LEN, KIRK_CMD_CERT_VER) != 0)
        return 0xC0520001;
    return 0;
}

/* 0x01C4 */
static s32 loadAndVerifyConsoleIdCert(u8 *cert)
{
    if (sceIdStorageLookup(0x100, 0x38, cert, KIRK_CERT_LEN) < 0) {
        if (sceIdStorageLookup(0x120, 0x38, cert, KIRK_CERT_LEN) < 0)
            return 0xC0520002;
    }
    if (sceUtilsBufferCopyWithRange(NULL, 0, cert, KIRK_CERT_LEN, KIRK_CMD_CERT_VER) != 0)
        return 0xC0520001;
    return 0;
}

/* 0x0250 */
s32 module_start(SceSize args __attribute__((unused)), void *argp __attribute__((unused)))
{
    u32 i;

    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;

    g_semaId = sceKernelCreateSema("SceOpenPSID", 0, 1, 1, NULL);
    return (g_semaId < 1) ? 1 : 0;
}

/* 0x02B0 */
s32 sceOpenPSIDGetOpenPSID(SceOpenPSID *pOpenPSID)
{
    s32 oldK1 = pspShiftK1();
    s32 status = SCE_ERROR_PRIV_REQUIRED;
    u32 i;

    if (!pspK1StaBufOk(pOpenPSID, sizeof(SceOpenPSID))) {
        pspSetK1(oldK1);
        return status;
    }
    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0) {
        pspSetK1(oldK1);
        return SCE_ERROR_SEMAPHORE;
    }

    status = loadAndVerifyOpenPsidCert(g_certBuf);
    if (status == 0) {
        for (i = 0; i < sizeof(SceOpenPSID); i++)
            pOpenPSID->data[i] = g_certBuf[i];
    }
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;

    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = SCE_ERROR_SEMAPHORE;

    pspSetK1(oldK1);
    return status;
}

/* 0x03B4 */
s32 sceOpenPSIDGetPSID(SceOpenPSID *pPSID, s32 type)
{
    s32 status;
    u32 i;

    if (type != 1)
        return SCE_ERROR_INVALID_POINTER;
    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return SCE_ERROR_SEMAPHORE;

    status = loadAndVerifyConsoleIdCert(g_certBuf);
    if (status == 0) {
        for (i = 0; i < sizeof(SceOpenPSID); i++)
            pPSID->data[i] = g_certBuf[i];
    }
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;

    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = SCE_ERROR_SEMAPHORE;

    return status;
}

/* 0x0498 */
s32 sceOpenPSID_driver_E8316C16(u8 *pOut, s32 index)
{
    s32 status;
    u32 i;

    if (index != 0)
        return SCE_ERROR_INVALID_INDEX;
    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return SCE_ERROR_SEMAPHORE;

    status = loadAndVerifyConsoleIdCert(g_certBuf);
    if (status == 0) {
        if ((g_certBuf[8] >> 2) == 0x23) {
            *pOut = (u8)((g_certBuf[8] << 6) | (g_certBuf[9] >> 2));
        } else {
            status = 0x800001FE;
        }
    }
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;

    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = SCE_ERROR_SEMAPHORE;

    return status;
}

/* 0x0580 */
s32 sceOpenPSIDGetProductCode(SceProductCode *pProductCode)
{
    s32 oldK1 = pspShiftK1();
    s32 status = SCE_ERROR_PRIV_REQUIRED;
    u32 i;

    if (!pspK1StaBufOk(pProductCode, sizeof(SceProductCode))) {
        pspSetK1(oldK1);
        return status;
    }
    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0) {
        pspSetK1(oldK1);
        return SCE_ERROR_SEMAPHORE;
    }

    status = loadAndVerifyConsoleIdCert(g_certBuf);
    if (status == 0) {
        pProductCode->data[0] = g_certBuf[5];
        pProductCode->data[1] = g_certBuf[4];
    }
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;

    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = SCE_ERROR_SEMAPHORE;

    pspSetK1(oldK1);
    return status;
}

/* 0x0668 */
s32 scePcactAuth1BB(u8 flag, u8 *psidKey, u8 *authInput, u8 *challenge, u8 *outTicket, u8 *outSession)
{
    s32 status = 0x80520336;
    u32 i;
    u32 fuseIdLow, fuseIdHigh;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return status;

    do {
        if (psidKey == NULL || authInput == NULL || challenge == NULL || outTicket == NULL || outSession == NULL) {
            status = 0x80520330;
            break;
        }
        if (authInput[0] != 0x10 || authInput[1] != 0) {
            status = 0x80520331;
            break;
        }
        s32 badHeader = 0;
        for (i = 0; i < 14; i++) {
            if (authInput[2 + i] != 0) {
                badHeader = 1;
                break;
            }
        }
        if (badHeader) {
            status = 0x80520331;
            break;
        }

        for (i = 0; i < 0x80; i++)
            outTicket[i] = 0;
        for (i = 0; i < 0x40; i++)
            outSession[i] = 0;

        if (sceUtilsBufferCopyWithRange(g_certBuf, 0x14, NULL, 0, KIRK_CMD_PRN_GEN) != 0) {
            status = 0x80520332;
            break;
        }
        for (i = 0; i < 0x10; i++)
            outSession[i] = g_certBuf[i];
        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = outSession[i];

        if (kirkDecryptAesCbc(g_certBuf, 0x10, 0x68) != 0) {
            status = 0x80520333;
            break;
        }
        for (i = 0; i < 0x10; i++)
            outSession[0x10 + i] = g_certBuf[i];
        outSession[0x20] = flag;

        for (i = 0; i < 0x10; i++)
            g_ticketBuf[i] = g_certBuf[i];
        for (i = 0; i < 0x10; i++)
            g_ticketBuf[0x10 + i] = g_pcactSeed[i];
        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = psidKey[i];

        if (kirkDecryptAesCbc(g_certBuf, 0x10, 0x68) != 0) {
            status = 0x80520333;
            break;
        }
        for (i = 0; i < 0x10; i++)
            g_ticketBuf[0x20 + i] = g_certBuf[i];
        for (i = 0; i < 8; i++)
            g_ticketBuf[0x30 + i] = challenge[i];
        for (i = 0; i < 0x20; i++)
            g_ticketBuf[0x38 + i] = authInput[0x10 + i];

        fuseIdLow = *(volatile u32 *)0xBC100090;
        fuseIdHigh = *(volatile u32 *)0xBC100094;
        g_ticketBuf[0x58] = (u8)(fuseIdHigh >> 24);
        g_ticketBuf[0x59] = (u8)(fuseIdHigh >> 16);
        g_ticketBuf[0x5A] = (u8)(fuseIdHigh >> 8);
        g_ticketBuf[0x5B] = (u8)fuseIdHigh;
        g_ticketBuf[0x5C] = (u8)(fuseIdLow >> 24);
        g_ticketBuf[0x5D] = (u8)(fuseIdLow >> 16);
        g_ticketBuf[0x5E] = (u8)(fuseIdLow >> 8);
        g_ticketBuf[0x5F] = (u8)fuseIdLow;

        for (i = 0; i < 0x60; i++)
            g_certBuf[0x14 + i] = g_ticketBuf[i] ^ g_pcactReqKey[i & 0xF];

        if (kirkEncryptAesCbc(g_certBuf, 0x60, 0x13) != 0) {
            status = 0x80520334;
            break;
        }
        for (i = 0; i < 0x60; i++)
            g_certBuf[0x14 + i] ^= g_pcactReqKey[i & 0xF];

        g_ticketBuf[0] = 0x11;
        g_ticketBuf[1] = 1;
        g_ticketBuf[2] = flag;
        for (i = 0; i < 9; i++)
            g_ticketBuf[3 + i] = 0;
        g_ticketBuf[0xC] = 0;
        g_ticketBuf[0xD] = 0;
        g_ticketBuf[0xE] = 0;
        g_ticketBuf[0xF] = 0;
        for (i = 0; i < 0x60; i++)
            g_ticketBuf[0x10 + i] = g_certBuf[0x14 + i];

        g_pKirkBuf = g_certBuf;
        *(u32 *)g_certBuf = 0x80;
        for (i = 0; i < 0x10; i++)
            g_certBuf[4 + i] = g_pcactReqKey[i];
        for (i = 0; i < 0x70; i++)
            g_certBuf[0x14 + i] = g_ticketBuf[i];

        if (sceUtilsBufferCopyWithRange(g_certBuf, 0x84, g_certBuf, 0x84, KIRK_CMD_HASH_GEN_SHA1) != 0) {
            status = 0x80520335;
            break;
        }
        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = g_certBuf[i];
        if (kirkEncryptAesCbc(g_certBuf, 0x10, 0x13) != 0) {
            status = 0x80520334;
            break;
        }
        for (i = 0; i < 0x70; i++)
            outTicket[i] = g_ticketBuf[i];
        for (i = 0; i < 0x10; i++)
            outTicket[0x70 + i] = g_certBuf[0x14 + i];
        status = 0;
    } while (0);

    for (i = 0; i < 0x70; i++)
        g_ticketBuf[i] = 0;
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520336;
    return status;
}

/* 0x0C18 */
s32 scePcactAuth2BB(u8 *authResp, u8 *sessionBuf, u8 *outKey)
{
    s32 status = 0x80520330;
    u32 i;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return 0x80520336;

    do {
        if (authResp == NULL || sessionBuf == NULL || outKey == NULL)
            break;

        if (authResp[0] != 0x11 || authResp[1] != 2 || authResp[2] != sessionBuf[0x20]) {
            status = 0x80520331;
            break;
        }
        s32 badHeader = 0;
        for (i = 0; i < 13; i++) {
            if (authResp[3 + i] != 0) {
                badHeader = 1;
                break;
            }
        }
        if (badHeader) {
            status = 0x80520331;
            break;
        }

        for (i = 0; i < 0x10; i++)
            outKey[i] = 0;

        g_pKirkBuf = g_certBuf;
        *(u32 *)g_certBuf = 0x50;
        for (i = 0; i < 0x10; i++)
            g_certBuf[4 + i] = g_pcactRespKey[i];
        for (i = 0; i < 0x40; i++)
            g_certBuf[0x14 + i] = authResp[i];

        if (sceUtilsBufferCopyWithRange(g_certBuf, 0x54, g_certBuf, 0x54, KIRK_CMD_HASH_GEN_SHA1) != 0) {
            status = 0x80520335;
            break;
        }
        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = g_certBuf[i];
        if (kirkEncryptAesCbc(g_certBuf, 0x10, 0x13) != 0) {
            status = 0x80520334;
            break;
        }
        s32 macMismatch = 0;
        for (i = 0; i < 0x10; i++) {
            if (g_certBuf[0x14 + i] != authResp[0x40 + i]) {
                macMismatch = 1;
                break;
            }
        }
        if (macMismatch) {
            status = 0x80520337;
            break;
        }

        for (i = 0; i < 0x30; i++)
            g_certBuf[0x14 + i] = authResp[0x10 + i] ^ g_pcactRespKey[i & 0xF];
        if (kirkDecryptAesCbc(g_certBuf, 0x30, 0x13) != 0) {
            status = 0x80520333;
            break;
        }
        for (i = 0; i < 0x30; i++)
            g_certBuf[i] ^= g_pcactRespKey[i & 0xF];
        for (i = 0; i < 8; i++)
            g_certBuf[0x10 + i] = g_certBuf[8 + i];
        for (i = 0; i < 8; i++)
            g_certBuf[0x18 + i] = g_certBuf[i];

        s32 nonceMismatch = 0;
        for (i = 0; i < 0x10; i++) {
            if (g_certBuf[0x10 + i] != sessionBuf[0x10 + i]) {
                nonceMismatch = 1;
                break;
            }
        }
        if (nonceMismatch) {
            status = 0x80520338;
            break;
        }

        for (i = 0; i < 0x10; i++)
            outKey[i] = sessionBuf[i];
        status = 0;
    } while (0);

    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520336;
    return status;
}

/* 0x0FB8 */
s32 sceMlnpsnlAuth1BB(u8 *authInput, u8 *challenge, u8 *outTicket, u8 *outSession)
{
    s32 status = 0x80520330;
    u8 psid[16];
    u32 i;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return 0x80520336;

    do {
        if (authInput == NULL || challenge == NULL || outTicket == NULL || outSession == NULL)
            break;

        for (i = 0; i < 0x80; i++)
            outTicket[i] = 0;
        for (i = 0; i < 0x40; i++)
            outSession[i] = 0;

        if (loadAndVerifyConsoleIdCert(g_certBuf) != 0) {
            status = 0x80520340;
            break;
        }
        for (i = 0; i < 0x10; i++)
            psid[i] = g_certBuf[i];

        if (sceUtilsBufferCopyWithRange(g_certBuf, 0x14, NULL, 0, KIRK_CMD_PRN_GEN) != 0) {
            status = 0x80520332;
            break;
        }
        for (i = 0; i < 0x10; i++)
            outSession[i] = g_certBuf[i];
        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = outSession[i];

        if (kirkDecryptAesCbc(g_certBuf, 0x10, 0x68) != 0) {
            status = 0x80520333;
            break;
        }
        for (i = 0; i < 0x10; i++)
            outSession[0x10 + i] = g_certBuf[i];
        outSession[0x20] = 0x80;

        for (i = 0; i < 0x10; i++)
            g_ticketBuf[i] = g_certBuf[i];
        for (i = 0; i < 0x10; i++)
            g_ticketBuf[0x10 + i] = g_mlnpsnlSeed[i];
        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = psid[i];

        if (kirkDecryptAesCbc(g_certBuf, 0x10, 0x68) != 0) {
            status = 0x80520333;
            break;
        }
        for (i = 0; i < 0x10; i++)
            g_ticketBuf[0x20 + i] = g_certBuf[i];
        for (i = 0; i < 8; i++)
            g_ticketBuf[0x30 + i] = challenge[i];
        for (i = 0; i < 0x20; i++)
            g_ticketBuf[0x38 + i] = authInput[i];
        for (i = 0; i < 8; i++)
            g_ticketBuf[0x58 + i] = 0;

        for (i = 0; i < 0x60; i++)
            g_certBuf[0x14 + i] = g_ticketBuf[i] ^ g_mlnpsnlReqKey[i & 0xF];
        if (kirkEncryptAesCbc(g_certBuf, 0x60, 0x13) != 0) {
            status = 0x80520334;
            break;
        }
        for (i = 0; i < 0x60; i++)
            g_certBuf[0x14 + i] ^= g_mlnpsnlReqKey[i & 0xF];

        g_ticketBuf[0] = 0x20;
        g_ticketBuf[1] = 1;
        g_ticketBuf[2] = 0x80;
        for (i = 0; i < 9; i++)
            g_ticketBuf[3 + i] = 0;
        g_ticketBuf[0xC] = 0;
        g_ticketBuf[0xD] = 0;
        g_ticketBuf[0xE] = 0;
        g_ticketBuf[0xF] = 0;
        for (i = 0; i < 0x60; i++)
            g_ticketBuf[0x10 + i] = g_certBuf[0x14 + i];

        g_pKirkBuf = g_certBuf;
        *(u32 *)g_certBuf = 0x80;
        for (i = 0; i < 0x10; i++)
            g_certBuf[4 + i] = g_mlnpsnlReqKey[i];
        for (i = 0; i < 0x70; i++)
            g_certBuf[0x14 + i] = g_ticketBuf[i];

        if (sceUtilsBufferCopyWithRange(g_certBuf, 0x84, g_certBuf, 0x84, KIRK_CMD_HASH_GEN_SHA1) != 0) {
            status = 0x80520335;
            break;
        }
        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = g_certBuf[i];
        if (kirkEncryptAesCbc(g_certBuf, 0x10, 0x13) != 0) {
            status = 0x80520334;
            break;
        }
        for (i = 0; i < 0x70; i++)
            outTicket[i] = g_ticketBuf[i];
        for (i = 0; i < 0x10; i++)
            outTicket[0x70 + i] = g_certBuf[0x14 + i];
        status = 0;
    } while (0);

    for (i = 0; i < 0x70; i++)
        g_ticketBuf[i] = 0;
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520336;
    return status;
}

/* 0x1514 */
s32 sceMlnpsnlAuth2BB(u8 *authResp, u8 *sessionBuf, u8 *outKey)
{
    s32 status = 0x80520330;
    u32 i;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return 0x80520336;

    do {
        if (authResp == NULL || sessionBuf == NULL || outKey == NULL)
            break;

        if (authResp[0] != 0x20 || authResp[1] != 2 || authResp[2] != sessionBuf[0x20]) {
            status = 0x80520331;
            break;
        }
        s32 badHeader = 0;
        for (i = 0; i < 13; i++) {
            if (authResp[3 + i] != 0) {
                badHeader = 1;
                break;
            }
        }
        if (badHeader) {
            status = 0x80520331;
            break;
        }

        for (i = 0; i < 0x10; i++)
            outKey[i] = 0;

        g_pKirkBuf = g_certBuf;
        *(u32 *)g_certBuf = 0x50;
        for (i = 0; i < 0x10; i++)
            g_certBuf[4 + i] = g_mlnpsnlRespKey[i];
        for (i = 0; i < 0x40; i++)
            g_certBuf[0x14 + i] = authResp[i];

        if (sceUtilsBufferCopyWithRange(g_certBuf, 0x54, g_certBuf, 0x54, KIRK_CMD_HASH_GEN_SHA1) != 0) {
            status = 0x80520335;
            break;
        }
        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = g_certBuf[i];
        if (kirkEncryptAesCbc(g_certBuf, 0x10, 0x13) != 0) {
            status = 0x80520334;
            break;
        }
        s32 macMismatch = 0;
        for (i = 0; i < 0x10; i++) {
            if (g_certBuf[0x14 + i] != authResp[0x40 + i]) {
                macMismatch = 1;
                break;
            }
        }
        if (macMismatch) {
            status = 0x80520337;
            break;
        }

        for (i = 0; i < 0x30; i++)
            g_certBuf[0x14 + i] = authResp[0x10 + i] ^ g_mlnpsnlRespKey[i & 0xF];
        if (kirkDecryptAesCbc(g_certBuf, 0x30, 0x13) != 0) {
            status = 0x80520333;
            break;
        }
        for (i = 0; i < 0x30; i++)
            g_certBuf[i] ^= g_mlnpsnlRespKey[i & 0xF];
        for (i = 0; i < 8; i++)
            g_certBuf[0x10 + i] = g_certBuf[8 + i];
        for (i = 0; i < 8; i++)
            g_certBuf[0x18 + i] = g_certBuf[i];

        s32 nonceMismatch = 0;
        for (i = 0; i < 0x10; i++) {
            if (g_certBuf[0x10 + i] != sessionBuf[0x10 + i]) {
                nonceMismatch = 1;
                break;
            }
        }
        if (nonceMismatch) {
            status = 0x80520338;
            break;
        }

        for (i = 0; i < 0x10; i++)
            outKey[i] = sessionBuf[i];
        status = 0;
    } while (0);

    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520336;
    return status;
}

/* 0x1A7C */
static s32 generatePsidTicket(u8 *outBuf, s32 mode)
{
    u8 privKey[0x20];
    u8 qtgp2[0x10];
    u8 qtgp3[0x10];
    u8 gameMeta[0x10];
    u32 qtgp3Val = 0;
    u64 rtcTick;
    u64 msVal;
    s32 status = 0x80520400;
    s32 hasGameInfo = 0;
    s32 intr;
    SceGameInfo *gameInfo;
    u8 *p;
    u32 i;
    u32 fuseIdLow, fuseIdHigh;

    do {
        if (outBuf == NULL)
            break;

        for (i = 0; i < 0x100; i++)
            outBuf[i] = 0;

        status = loadAndVerifyConsoleIdCert(g_certBuf);
        if (status != 0)
            break;

        p = g_ticketBuf + 0x14;
        for (i = 0; i < 0x60; i++)
            p[i] = g_certBuf[i];
        for (i = 0; i < 0x20; i++)
            privKey[i] = g_certBuf[0x88 + i];
        p += 0x60;

        intr = sceKernelCpuSuspendIntr();
        gameInfo = sceKernelGetGameInfo();
        const u8 *gameInfoBytes = (const u8 *)gameInfo;

        if (mode == 1) {
            if (g_hasCustomTitleId == 1) {
                if ((gameInfo->flags & 1) == 0) {
                    status = 0x8052040B;
                    break;
                }
                hasGameInfo = 1;
                for (i = 0; i < 9; i++)
                    p[i] = (u8)g_customTitleId[i];
                for (i = 0; i < 0x10; i++)
                    gameMeta[i] = gameInfoBytes[8 + i];
                p += 9;
            } else {
                for (i = 0; i < 9; i++)
                    p[i] = 0;
                p += 9;
            }
        } else {
            if ((gameInfo->flags & 0x100) != 0) {
                hasGameInfo = 1;
                for (i = 0; i < 9; i++)
                    p[i] = gameInfoBytes[0x44 + i];
                p += 9;
            } else {
                for (i = 0; i < 9; i++)
                    p[i] = 0;
                p += 9;
            }
        }

        for (i = 0; i < 7; i++)
            p[i] = 0;
        p += 7;

        fuseIdLow = *(volatile u32 *)0xBC100090;
        fuseIdHigh = *(volatile u32 *)0xBC100094;
        p[0] = (u8)(fuseIdHigh >> 24);
        p[1] = (u8)(fuseIdHigh >> 16);
        p[2] = (u8)(fuseIdHigh >> 8);
        p[3] = (u8)fuseIdHigh;
        p[4] = (u8)(fuseIdLow >> 24);
        p[5] = (u8)(fuseIdLow >> 16);
        p[6] = (u8)(fuseIdLow >> 8);
        p[7] = (u8)fuseIdLow;
        p += 8;

        for (i = 0; i < 8; i++)
            p[i] = 0;
        p += 8;

        if (hasGameInfo != 0) {
            if (sceKernelGetQTGP3((char *)qtgp3) != 0) {
                status = 0x80520407;
                break;
            }
            qtgp3Val = ((u32)(u8)((qtgp3[10] << 2) | (qtgp3[9] >> 6)) << 24)
                     | ((u32)(u8)((qtgp3[9] << 2) | (qtgp3[8] >> 6)) << 16)
                     | ((u32)(u8)((qtgp3[8] << 2) | (qtgp3[7] >> 6)) << 8)
                     | (u32)(u8)((qtgp3[7] << 2) | (qtgp3[6] >> 6));
            const u8 *qtgp3Bytes = (const u8 *)&qtgp3Val;
            for (i = 0; i < 4; i++)
                p[i] = qtgp3Bytes[i];
        } else {
            for (i = 0; i < 4; i++)
                p[i] = 0;
        }
        p += 4;

        if (hasGameInfo != 0) {
            if (sceKernelGetQTGP2((char *)qtgp2) != 0) {
                status = 0x80520408;
                break;
            }
            for (i = 0; i < 4; i++)
                p[i] = qtgp2[i];
        } else {
            for (i = 0; i < 4; i++)
                p[i] = 0;
        }
        p += 4;

        sceKernelCpuResumeIntr(intr);

        if (sceRtc_driver_CEEF238F(&rtcTick) != 0) {
            status = 0x80520409;
            break;
        }

        msVal = (rtcTick - 0x00DCBFFEFF2BC000ULL) / 1000ULL;
        const u8 *msBytes = (const u8 *)&msVal;
        for (i = 0; i < 8; i++)
            p[i] = msBytes[i];

        if (mode == 1) {
            for (i = 0; i < 5; i++)
                g_ticketBuf[0x7D + i] = gameMeta[i];
            for (i = 0; i < 5; i++)
                g_ticketBuf[0x8C + i] = gameMeta[5 + i];
        }

        if (kirkDecryptAesCbc(g_ticketBuf, 0x90, 0x68) != 0) {
            status = 0x80520402;
            break;
        }
        if (sceUtilsBufferCopyWithRange(g_certBuf + 0x14, 0x14, NULL, 0, KIRK_CMD_PRN_GEN) != 0) {
            status = 0x80520401;
            break;
        }
        for (i = 0; i < 0x90; i++)
            g_certBuf[0x24 + i] = g_ticketBuf[i];
        for (i = 0; i < 0xA0; i++)
            g_certBuf[0x14 + i] ^= g_psidTicketKey[i & 0xF];
        if (kirkEncryptAesCbc(g_certBuf, 0xA0, 0x13) != 0) {
            status = 0x80520403;
            break;
        }
        for (i = 0; i < 0xA0; i++)
            g_certBuf[0x14 + i] ^= g_psidTicketKey[i & 0xF];

        g_pKirkBuf = g_ticketBuf;
        *(u32 *)g_ticketBuf = 0xB8;
        for (i = 0; i < 0x10; i++)
            g_ticketBuf[4 + i] = g_psidTicketKey[i];
        for (i = 0; i < 4; i++)
            g_ticketBuf[0x14 + i] = g_psidTicketHdr[i];
        for (i = 0; i < 4; i++)
            g_ticketBuf[0x18 + i] = 0;
        for (i = 0; i < 0xA0; i++)
            g_ticketBuf[0x1C + i] = g_certBuf[0x14 + i];
        for (i = 0; i < 0xA8; i++)
            outBuf[i] = g_ticketBuf[0x14 + i];

        if (sceUtilsBufferCopyWithRange(g_ticketBuf, 0xBC, g_ticketBuf, 0xBC, KIRK_CMD_HASH_GEN_SHA1) != 0) {
            status = 0x80520405;
            break;
        }

        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = g_ticketBuf[i];
        if (kirkEncryptAesCbc(g_certBuf, 0x10, 0x13) != 0) {
            status = 0x80520403;
            break;
        }
        for (i = 0; i < 0x10; i++)
            outBuf[0xA8 + i] = g_certBuf[0x14 + i];

        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = g_ticketBuf[i];
        if (kirkEncryptAesCbc(g_certBuf, 0x10, 0x14) != 0) {
            status = 0x80520403;
            break;
        }
        for (i = 0; i < 0x10; i++)
            outBuf[0xB8 + i] = g_certBuf[0x14 + i];

        for (i = 0; i < 0x10; i++)
            g_certBuf[0x14 + i] = g_ticketBuf[i];
        if (kirkEncryptAesCbcFuse(g_certBuf, 0x10) != 0) {
            status = 0x80520404;
            break;
        }
        for (i = 0; i < 0x10; i++)
            outBuf[0xC8 + i] = g_certBuf[0x14 + i];

        for (i = 0; i < 0x20; i++)
            g_certBuf[i] = privKey[i];
        for (i = 0; i < 0x14; i++)
            g_certBuf[0x20 + i] = g_ticketBuf[i];
        if (sceUtilsBufferCopyWithRange(g_certBuf, 0x34, g_certBuf, 0x34, KIRK_CMD_SIG_GEN_ECDSA) != 0) {
            status = 0x8052040A;
            break;
        }
        for (i = 0; i < 0x28; i++)
            outBuf[0xD8 + i] = g_certBuf[i];

        status = (hasGameInfo != 0) ? 1 : 2;
    } while (0);

    for (i = 0; i < 0x100; i++)
        g_ticketBuf[i] = 0;
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    return status;
}

/* 0x18B4 */
s32 sceOpenPSID_AE6DD68C(const u8 *titleId, u8 *outTicket)
{
    s32 oldK1 = pspShiftK1();
    s32 status;
    u32 i;

    if (!pspK1StaBufOk(titleId, 9) || !pspK1StaBufOk(outTicket, 0x100)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0) {
        pspSetK1(oldK1);
        return 0x80520406;
    }

    if (titleId != NULL) {
        for (i = 0; i < 9; i++)
            g_customTitleId[i] = titleId[i];
        g_hasCustomTitleId = 1;
    } else {
        for (i = 0; i < 9; i++)
            g_customTitleId[i] = 0;
        g_hasCustomTitleId = 0;
    }

    status = generatePsidTicket(outTicket, 1);

    for (i = 0; i < 9; i++)
        g_customTitleId[i] = 0;
    g_hasCustomTitleId = 0;

    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520406;

    pspSetK1(oldK1);
    return status;
}

/* 0x1A04 */
s32 sceOpenPSID_driver_8546569E(u8 *outTicket)
{
    s32 status;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return 0x80520406;

    status = generatePsidTicket(outTicket, 0);
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520406;

    return status;
}

/* 0x2348: unexported module stop stub in Sony's openpsid.c */
static s32 __attribute__((used, noinline)) openpsidStop(void)
{
    return 0;
}

/* 0x2350: unexported module reboot-before stub in Sony's openpsid.c */
static s32 __attribute__((used, noinline)) openpsidRebootBefore(void)
{
    return 0;
}
