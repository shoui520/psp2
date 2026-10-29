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

static const u8 g_pcactKey1[16] = {
    0x38, 0x0A, 0x18, 0x0E, 0x36, 0x58, 0xBC, 0xC1,
    0x70, 0x64, 0x69, 0xE3, 0x29, 0x60, 0x81, 0xF9
};

static const u8 g_pcactKey2[16] = {
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

static const u8 g_mlnpsnlKey1[16] = {
    0x90, 0xAB, 0x41, 0xEE, 0xEA, 0x3E, 0xD1, 0xEC,
    0xD8, 0xC4, 0x92, 0xFD, 0x66, 0x15, 0x5E, 0x6C
};

static const u8 g_mlnpsnlKey2[16] = {
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
static s32 sub_0000(u8 *buf, u32 size, u32 keyId)
{
    KirkAESHeader *hdr = (KirkAESHeader *)buf;
    u32 totalSize = size + sizeof(KirkAESHeader);

    hdr->mode = 5;
    hdr->unk4 = 0;
    hdr->unk8 = 0;
    hdr->keyIndex = keyId;
    hdr->dataSize = size;
    g_pKirkBuf = buf;
    return sceUtilsBufferCopyWithRange(buf, totalSize, buf, totalSize, KIRK_CMD_DECRYPT_AES_CBC_IV_NONE);
}

/* 0x004C */
static s32 sub_004C(u8 *buf, u32 size, u32 keyId)
{
    KirkAESHeader *hdr = (KirkAESHeader *)buf;
    u32 totalSize = size + sizeof(KirkAESHeader);

    hdr->mode = 4;
    hdr->unk4 = 0;
    hdr->unk8 = 0;
    hdr->keyIndex = keyId;
    hdr->dataSize = size;
    g_pKirkBuf = buf;
    return sceUtilsBufferCopyWithRange(buf, totalSize, buf, totalSize, KIRK_CMD_ENCRYPT_AES_CBC_IV_NONE);
}

/* 0x0098 */
static s32 sub_0098(u8 *buf, u32 size)
{
    KirkAESHeader *hdr = (KirkAESHeader *)buf;
    u32 totalSize = size + sizeof(KirkAESHeader);

    hdr->mode = 4;
    hdr->unk4 = 0;
    hdr->unk8 = 0;
    hdr->keyIndex = 0x100;
    hdr->dataSize = size;
    g_pKirkBuf = buf;
    return sceUtilsBufferCopyWithRange(buf, totalSize, buf, totalSize, KIRK_CMD_ENCRYPT_AES_CBC_IV_FUSE);
}

/* 0x00E8 */
static s32 sub_00E8(u8 *cert)
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
static s32 sub_01C4(u8 *cert)
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

    status = sub_00E8(g_certBuf);
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
s32 sceOpenPSIDGetPSID(SceOpenPSID *pPSID, s32 unk)
{
    s32 status;
    u32 i;

    if (unk != 1)
        return SCE_ERROR_INVALID_POINTER;
    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return SCE_ERROR_SEMAPHORE;

    status = sub_01C4(g_certBuf);
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
s32 sceOpenPSID_driver_E8316C16(u8 *pOut, s32 unk)
{
    s32 status;
    u32 i;

    if (unk != 0)
        return SCE_ERROR_INVALID_SIZE;
    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return SCE_ERROR_SEMAPHORE;

    status = sub_01C4(g_certBuf);
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

    status = sub_01C4(g_certBuf);
    if (status == 0) {
        ((u8 *)pProductCode)[0] = g_certBuf[5];
        ((u8 *)pProductCode)[1] = g_certBuf[4];
    }
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;

    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = SCE_ERROR_SEMAPHORE;

    pspSetK1(oldK1);
    return status;
}

/* 0x0668 */
s32 scePcactAuth1BB(u8 flag, u8 *arg1, u8 *arg2, u8 *arg3, u8 *out128, u8 *out64)
{
    s32 status = 0x80520336;
    u32 i;
    u32 fuse90, fuse94;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return status;

    if (arg1 == NULL || arg2 == NULL || arg3 == NULL || out128 == NULL || out64 == NULL) {
        status = 0x80520330;
        goto cleanup;
    }
    if (arg2[0] != 0x10 || arg2[1] != 0) {
        status = 0x80520331;
        goto cleanup;
    }
    for (i = 0; i < 14; i++) {
        if (arg2[2 + i] != 0) {
            status = 0x80520331;
            goto cleanup;
        }
    }

    for (i = 0; i < 0x80; i++)
        out128[i] = 0;
    for (i = 0; i < 0x40; i++)
        out64[i] = 0;

    if (sceUtilsBufferCopyWithRange(g_certBuf, 0x14, NULL, 0, KIRK_CMD_PRN_GEN) != 0) {
        status = 0x80520332;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        out64[i] = g_certBuf[i];
    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = out64[i];

    if (sub_0000(g_certBuf, 0x10, 0x68) != 0) {
        status = 0x80520333;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        out64[0x10 + i] = g_certBuf[i];
    out64[0x20] = flag;

    for (i = 0; i < 0x10; i++)
        g_ticketBuf[i] = g_certBuf[i];
    for (i = 0; i < 0x10; i++)
        g_ticketBuf[0x10 + i] = g_pcactSeed[i];
    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = arg1[i];

    if (sub_0000(g_certBuf, 0x10, 0x68) != 0) {
        status = 0x80520333;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        g_ticketBuf[0x20 + i] = g_certBuf[i];
    for (i = 0; i < 8; i++)
        g_ticketBuf[0x30 + i] = arg3[i];
    for (i = 0; i < 0x20; i++)
        g_ticketBuf[0x38 + i] = arg2[0x10 + i];

    fuse90 = *(volatile u32 *)0xBC100090;
    fuse94 = *(volatile u32 *)0xBC100094;
    g_ticketBuf[0x58] = (u8)(fuse94 >> 24);
    g_ticketBuf[0x59] = (u8)(fuse94 >> 16);
    g_ticketBuf[0x5A] = (u8)(fuse94 >> 8);
    g_ticketBuf[0x5B] = (u8)fuse94;
    g_ticketBuf[0x5C] = (u8)(fuse90 >> 24);
    g_ticketBuf[0x5D] = (u8)(fuse90 >> 16);
    g_ticketBuf[0x5E] = (u8)(fuse90 >> 8);
    g_ticketBuf[0x5F] = (u8)fuse90;

    for (i = 0; i < 0x60; i++)
        g_certBuf[0x14 + i] = g_ticketBuf[i] ^ g_pcactKey1[i & 0xF];

    if (sub_004C(g_certBuf, 0x60, 0x13) != 0) {
        status = 0x80520334;
        goto cleanup;
    }
    for (i = 0; i < 0x60; i++)
        g_certBuf[0x14 + i] ^= g_pcactKey1[i & 0xF];

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
        g_certBuf[4 + i] = g_pcactKey1[i];
    for (i = 0; i < 0x70; i++)
        g_certBuf[0x14 + i] = g_ticketBuf[i];

    if (sceUtilsBufferCopyWithRange(g_certBuf, 0x84, g_certBuf, 0x84, KIRK_CMD_HASH_GEN_SHA1) != 0) {
        status = 0x80520335;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = g_certBuf[i];
    if (sub_004C(g_certBuf, 0x10, 0x13) != 0) {
        status = 0x80520334;
        goto cleanup;
    }
    for (i = 0; i < 0x70; i++)
        out128[i] = g_ticketBuf[i];
    for (i = 0; i < 0x10; i++)
        out128[0x70 + i] = g_certBuf[0x14 + i];
    status = 0;

cleanup:
    for (i = 0; i < 0x70; i++)
        g_ticketBuf[i] = 0;
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520336;
    return status;
}

/* 0x0C18 */
s32 scePcactAuth2BB(u8 *arg0, u8 *arg1, u8 *out16)
{
    s32 status = 0x80520330;
    u32 i;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return 0x80520336;

    if (arg0 == NULL || arg1 == NULL || out16 == NULL)
        goto cleanup;

    if (arg0[0] != 0x11 || arg0[1] != 2 || arg0[2] != arg1[0x20]) {
        status = 0x80520331;
        goto cleanup;
    }
    for (i = 0; i < 13; i++) {
        if (arg0[3 + i] != 0) {
            status = 0x80520331;
            goto cleanup;
        }
    }

    for (i = 0; i < 0x10; i++)
        out16[i] = 0;

    g_pKirkBuf = g_certBuf;
    *(u32 *)g_certBuf = 0x50;
    for (i = 0; i < 0x10; i++)
        g_certBuf[4 + i] = g_pcactKey2[i];
    for (i = 0; i < 0x40; i++)
        g_certBuf[0x14 + i] = arg0[i];

    if (sceUtilsBufferCopyWithRange(g_certBuf, 0x54, g_certBuf, 0x54, KIRK_CMD_HASH_GEN_SHA1) != 0) {
        status = 0x80520335;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = g_certBuf[i];
    if (sub_004C(g_certBuf, 0x10, 0x13) != 0) {
        status = 0x80520334;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++) {
        if (g_certBuf[0x14 + i] != arg0[0x40 + i]) {
            status = 0x80520337;
            goto cleanup;
        }
    }

    for (i = 0; i < 0x30; i++)
        g_certBuf[0x14 + i] = arg0[0x10 + i] ^ g_pcactKey2[i & 0xF];
    if (sub_0000(g_certBuf, 0x30, 0x13) != 0) {
        status = 0x80520333;
        goto cleanup;
    }
    for (i = 0; i < 0x30; i++)
        g_certBuf[i] ^= g_pcactKey2[i & 0xF];
    for (i = 0; i < 8; i++)
        g_certBuf[0x10 + i] = g_certBuf[8 + i];
    for (i = 0; i < 8; i++)
        g_certBuf[0x18 + i] = g_certBuf[i];

    for (i = 0; i < 0x10; i++) {
        if (g_certBuf[0x10 + i] != arg1[0x10 + i]) {
            status = 0x80520338;
            goto cleanup;
        }
    }

    for (i = 0; i < 0x10; i++)
        out16[i] = arg1[i];
    status = 0;

cleanup:
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520336;
    return status;
}

/* 0x0FB8 */
s32 sceMlnpsnlAuth1BB(u8 *arg0, u8 *arg1, u8 *out128, u8 *out64)
{
    s32 status = 0x80520330;
    u8 psid[16];
    u32 i;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return 0x80520336;

    if (arg0 == NULL || arg1 == NULL || out128 == NULL || out64 == NULL)
        goto cleanup;

    for (i = 0; i < 0x80; i++)
        out128[i] = 0;
    for (i = 0; i < 0x40; i++)
        out64[i] = 0;

    if (sub_01C4(g_certBuf) != 0) {
        status = 0x80520340;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        psid[i] = g_certBuf[i];

    if (sceUtilsBufferCopyWithRange(g_certBuf, 0x14, NULL, 0, KIRK_CMD_PRN_GEN) != 0) {
        status = 0x80520332;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        out64[i] = g_certBuf[i];
    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = out64[i];

    if (sub_0000(g_certBuf, 0x10, 0x68) != 0) {
        status = 0x80520333;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        out64[0x10 + i] = g_certBuf[i];
    out64[0x20] = 0x80;

    for (i = 0; i < 0x10; i++)
        g_ticketBuf[i] = g_certBuf[i];
    for (i = 0; i < 0x10; i++)
        g_ticketBuf[0x10 + i] = g_mlnpsnlSeed[i];
    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = psid[i];

    if (sub_0000(g_certBuf, 0x10, 0x68) != 0) {
        status = 0x80520333;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        g_ticketBuf[0x20 + i] = g_certBuf[i];
    for (i = 0; i < 8; i++)
        g_ticketBuf[0x30 + i] = arg1[i];
    for (i = 0; i < 0x20; i++)
        g_ticketBuf[0x38 + i] = arg0[i];
    for (i = 0; i < 8; i++)
        g_ticketBuf[0x58 + i] = 0;

    for (i = 0; i < 0x60; i++)
        g_certBuf[0x14 + i] = g_ticketBuf[i] ^ g_mlnpsnlKey1[i & 0xF];
    if (sub_004C(g_certBuf, 0x60, 0x13) != 0) {
        status = 0x80520334;
        goto cleanup;
    }
    for (i = 0; i < 0x60; i++)
        g_certBuf[0x14 + i] ^= g_mlnpsnlKey1[i & 0xF];

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
        g_certBuf[4 + i] = g_mlnpsnlKey1[i];
    for (i = 0; i < 0x70; i++)
        g_certBuf[0x14 + i] = g_ticketBuf[i];

    if (sceUtilsBufferCopyWithRange(g_certBuf, 0x84, g_certBuf, 0x84, KIRK_CMD_HASH_GEN_SHA1) != 0) {
        status = 0x80520335;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = g_certBuf[i];
    if (sub_004C(g_certBuf, 0x10, 0x13) != 0) {
        status = 0x80520334;
        goto cleanup;
    }
    for (i = 0; i < 0x70; i++)
        out128[i] = g_ticketBuf[i];
    for (i = 0; i < 0x10; i++)
        out128[0x70 + i] = g_certBuf[0x14 + i];
    status = 0;

cleanup:
    for (i = 0; i < 0x70; i++)
        g_ticketBuf[i] = 0;
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520336;
    return status;
}

/* 0x1514 */
s32 sceMlnpsnlAuth2BB(u8 *arg0, u8 *arg1, u8 *out16)
{
    s32 status = 0x80520330;
    u32 i;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return 0x80520336;

    if (arg0 == NULL || arg1 == NULL || out16 == NULL)
        goto cleanup;

    if (arg0[0] != 0x20 || arg0[1] != 2 || arg0[2] != arg1[0x20]) {
        status = 0x80520331;
        goto cleanup;
    }
    for (i = 0; i < 13; i++) {
        if (arg0[3 + i] != 0) {
            status = 0x80520331;
            goto cleanup;
        }
    }

    for (i = 0; i < 0x10; i++)
        out16[i] = 0;

    g_pKirkBuf = g_certBuf;
    *(u32 *)g_certBuf = 0x50;
    for (i = 0; i < 0x10; i++)
        g_certBuf[4 + i] = g_mlnpsnlKey2[i];
    for (i = 0; i < 0x40; i++)
        g_certBuf[0x14 + i] = arg0[i];

    if (sceUtilsBufferCopyWithRange(g_certBuf, 0x54, g_certBuf, 0x54, KIRK_CMD_HASH_GEN_SHA1) != 0) {
        status = 0x80520335;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = g_certBuf[i];
    if (sub_004C(g_certBuf, 0x10, 0x13) != 0) {
        status = 0x80520334;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++) {
        if (g_certBuf[0x14 + i] != arg0[0x40 + i]) {
            status = 0x80520337;
            goto cleanup;
        }
    }

    for (i = 0; i < 0x30; i++)
        g_certBuf[0x14 + i] = arg0[0x10 + i] ^ g_mlnpsnlKey2[i & 0xF];
    if (sub_0000(g_certBuf, 0x30, 0x13) != 0) {
        status = 0x80520333;
        goto cleanup;
    }
    for (i = 0; i < 0x30; i++)
        g_certBuf[i] ^= g_mlnpsnlKey2[i & 0xF];
    for (i = 0; i < 8; i++)
        g_certBuf[0x10 + i] = g_certBuf[8 + i];
    for (i = 0; i < 8; i++)
        g_certBuf[0x18 + i] = g_certBuf[i];

    for (i = 0; i < 0x10; i++) {
        if (g_certBuf[0x10 + i] != arg1[0x10 + i]) {
            status = 0x80520338;
            goto cleanup;
        }
    }

    for (i = 0; i < 0x10; i++)
        out16[i] = arg1[i];
    status = 0;

cleanup:
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520336;
    return status;
}

static void udiv64_1000(u32 lo, u32 hi, u32 *outLo, u32 *outHi)
{
    u32 q3 = (hi >> 16) / 1000;
    u32 r3 = (((hi >> 16) % 1000) << 16) | (hi & 0xFFFF);
    u32 q2 = r3 / 1000;
    u32 r2 = ((r3 % 1000) << 16) | (lo >> 16);
    u32 q1 = r2 / 1000;
    u32 r1 = ((r2 % 1000) << 16) | (lo & 0xFFFF);
    u32 q0 = r1 / 1000;

    *outHi = (q3 << 16) | q2;
    *outLo = (q1 << 16) | q0;
}

/* 0x1A7C */
static s32 sub_1A7C(u8 *outBuf, s32 mode)
{
    u8 privKey[0x20];
    u8 qtgp2[0x10];
    u8 qtgp3[0x10];
    u8 gameInfo10[0x10];
    u32 qtgp3Val = 0;
    u64 rtcTick;
    u32 msTime[2];
    s32 status = 0x80520400;
    s32 hasGameInfo = 0;
    s32 intr;
    SceGameInfo *gameInfo;
    u8 *p;
    u32 i;
    u32 fuse90, fuse94;
    u32 tickLo, tickHi, borrow;

    if (outBuf == NULL)
        goto cleanup;

    for (i = 0; i < 0x100; i++)
        outBuf[i] = 0;

    status = sub_01C4(g_certBuf);
    if (status != 0)
        goto cleanup;

    p = g_ticketBuf + 0x14;
    for (i = 0; i < 0x60; i++)
        p[i] = g_certBuf[i];
    for (i = 0; i < 0x20; i++)
        privKey[i] = g_certBuf[0x88 + i];
    p += 0x60;

    intr = sceKernelCpuSuspendIntr();
    gameInfo = sceKernelGetGameInfo();

    if (mode == 1) {
        if (g_hasCustomTitleId == 1) {
            if ((gameInfo->flags & 1) == 0) {
                status = 0x8052040B;
                goto cleanup;
            }
            hasGameInfo = 1;
            for (i = 0; i < 9; i++)
                p[i] = (u8)g_customTitleId[i];
            for (i = 0; i < 0x10; i++)
                gameInfo10[i] = ((u8 *)gameInfo)[8 + i];
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
                p[i] = ((u8 *)gameInfo)[0x44 + i];
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

    fuse90 = *(volatile u32 *)0xBC100090;
    fuse94 = *(volatile u32 *)0xBC100094;
    p[0] = (u8)(fuse94 >> 24);
    p[1] = (u8)(fuse94 >> 16);
    p[2] = (u8)(fuse94 >> 8);
    p[3] = (u8)fuse94;
    p[4] = (u8)(fuse90 >> 24);
    p[5] = (u8)(fuse90 >> 16);
    p[6] = (u8)(fuse90 >> 8);
    p[7] = (u8)fuse90;
    p += 8;

    for (i = 0; i < 8; i++)
        p[i] = 0;
    p += 8;

    if (hasGameInfo != 0) {
        if (sceKernelGetQTGP3((char *)qtgp3) != 0) {
            status = 0x80520407;
            goto cleanup;
        }
        qtgp3Val = ((u32)(u8)((qtgp3[10] << 2) | (qtgp3[9] >> 6)) << 24)
                 | ((u32)(u8)((qtgp3[9] << 2) | (qtgp3[8] >> 6)) << 16)
                 | ((u32)(u8)((qtgp3[8] << 2) | (qtgp3[7] >> 6)) << 8)
                 | (u32)(u8)((qtgp3[7] << 2) | (qtgp3[6] >> 6));
        for (i = 0; i < 4; i++)
            p[i] = ((u8 *)&qtgp3Val)[i];
    } else {
        for (i = 0; i < 4; i++)
            p[i] = 0;
    }
    p += 4;

    if (hasGameInfo != 0) {
        if (sceKernelGetQTGP2((char *)qtgp2) != 0) {
            status = 0x80520408;
            goto cleanup;
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
        goto cleanup;
    }

    tickLo = (u32)rtcTick;
    tickHi = (u32)(rtcTick >> 32);
    borrow = (tickLo < 0xFF2BC000U) ? 1U : 0U;
    tickLo -= 0xFF2BC000U;
    tickHi = tickHi - 0x00DCBFFE - borrow;
    udiv64_1000(tickLo, tickHi, &msTime[0], &msTime[1]);
    for (i = 0; i < 8; i++)
        p[i] = ((u8 *)msTime)[i];

    if (mode == 1) {
        for (i = 0; i < 5; i++)
            g_ticketBuf[0x7D + i] = gameInfo10[i];
        for (i = 0; i < 5; i++)
            g_ticketBuf[0x8C + i] = gameInfo10[5 + i];
    }

    if (sub_0000(g_ticketBuf, 0x90, 0x68) != 0) {
        status = 0x80520402;
        goto cleanup;
    }
    if (sceUtilsBufferCopyWithRange(g_certBuf + 0x14, 0x14, NULL, 0, KIRK_CMD_PRN_GEN) != 0) {
        status = 0x80520401;
        goto cleanup;
    }
    for (i = 0; i < 0x90; i++)
        g_certBuf[0x24 + i] = g_ticketBuf[i];
    for (i = 0; i < 0xA0; i++)
        g_certBuf[0x14 + i] ^= g_psidTicketKey[i & 0xF];
    if (sub_004C(g_certBuf, 0xA0, 0x13) != 0) {
        status = 0x80520403;
        goto cleanup;
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
        goto cleanup;
    }

    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = g_ticketBuf[i];
    if (sub_004C(g_certBuf, 0x10, 0x13) != 0) {
        status = 0x80520403;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        outBuf[0xA8 + i] = g_certBuf[0x14 + i];

    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = g_ticketBuf[i];
    if (sub_004C(g_certBuf, 0x10, 0x14) != 0) {
        status = 0x80520403;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        outBuf[0xB8 + i] = g_certBuf[0x14 + i];

    for (i = 0; i < 0x10; i++)
        g_certBuf[0x14 + i] = g_ticketBuf[i];
    if (sub_0098(g_certBuf, 0x10) != 0) {
        status = 0x80520404;
        goto cleanup;
    }
    for (i = 0; i < 0x10; i++)
        outBuf[0xC8 + i] = g_certBuf[0x14 + i];

    for (i = 0; i < 0x20; i++)
        g_certBuf[i] = privKey[i];
    for (i = 0; i < 0x14; i++)
        g_certBuf[0x20 + i] = g_ticketBuf[i];
    if (sceUtilsBufferCopyWithRange(g_certBuf, 0x34, g_certBuf, 0x34, KIRK_CMD_SIG_GEN_ECDSA) != 0) {
        status = 0x8052040A;
        goto cleanup;
    }
    for (i = 0; i < 0x28; i++)
        outBuf[0xD8 + i] = g_certBuf[i];

    status = (hasGameInfo != 0) ? 1 : 2;

cleanup:
    for (i = 0; i < 0x100; i++)
        g_ticketBuf[i] = 0;
    for (i = 0; i < KIRK_CERT_LEN; i++)
        g_certBuf[i] = 0;
    return status;
}

/* 0x18B4 */
s32 sceOpenPSID_AE6DD68C(const u8 *titleId9, u8 *out256)
{
    s32 oldK1 = pspShiftK1();
    s32 status;
    u32 i;

    if (!pspK1PtrOk(titleId9 + 9) || !pspK1StaBufOk(out256, 0x100)) {
        pspSetK1(oldK1);
        return SCE_ERROR_PRIV_REQUIRED;
    }
    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0) {
        pspSetK1(oldK1);
        return 0x80520406;
    }

    if (titleId9 != NULL) {
        for (i = 0; i < 9; i++)
            g_customTitleId[i] = titleId9[i];
        g_hasCustomTitleId = 1;
    } else {
        for (i = 0; i < 9; i++)
            g_customTitleId[i] = 0;
        g_hasCustomTitleId = 0;
    }

    status = sub_1A7C(out256, 1);

    for (i = 0; i < 9; i++)
        g_customTitleId[i] = 0;
    g_hasCustomTitleId = 0;

    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520406;

    pspSetK1(oldK1);
    return status;
}

/* 0x1A04 */
s32 sceOpenPSID_driver_8546569E(u8 *out256)
{
    s32 status;

    if (sceKernelWaitSema(g_semaId, 1, NULL) != 0)
        return 0x80520406;

    status = sub_1A7C(out256, 0);
    if (sceKernelSignalSema(g_semaId, 1) != 0)
        status = 0x80520406;

    return status;
}
