#include <common_imp.h>

SCE_MODULE_INFO("sceCert_Loader",
                SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START,
                1, 2);
SCE_SDK_VERSION(0x06060110);

extern int sceIoOpen(const char *file, int flags, int mode);
extern int sceIoClose(int fileDesc);
extern s64 sceIoLseek(int fileDesc, s64 offset, int whence);
extern int sceIoRead(int fileDesc, void *data, u32 size);
extern u32 sceKernelGetCompiledSdkVersion(void);

typedef struct SceCertNode {
    void *data;
    struct SceCertNode *next;
} SceCertNode;

static s32 loadCertCategoryInternal(u32 cat, u32 mask, SceCertNode **outList,
                                    u8 *buf, u32 bufSize, u32 *outSize, s32 writeMode);

static s32 __attribute__((noinline)) loadCertCategory(u32 cat, u32 mask, SceCertNode **outList,
                                                      u8 *buf, u32 bufSize, u32 *outSize, s32 writeMode)
{
    return loadCertCategoryInternal(cat, mask, outList, buf, bufSize, outSize, writeMode);
}

static u32 __attribute__((noinline)) getDefaultCertMask(u32 cat)
{
    u32 sdkVer = sceKernelGetCompiledSdkVersion();
    switch (cat) {
    case 1:
    case 8:
    case 12:
    case 13:
        return 0x3;
    case 2:
        if (sdkVer <= 0x0602FFFFu)
            return 0x1FFF;
        return (sdkVer > 0x060600FFu) ? 0x7FFF : 0x3FFF;
    case 3:
        return 0x1F;
    case 4:
        return (sdkVer > 0x05050510u) ? 0xF : 0x7;
    case 5:
        return (sdkVer > 0x060600FFu) ? 0x3 : 0x1;
    case 6:
    case 11:
    case 14:
        return 0x1;
    case 7:
        return (sdkVer > 0x060600FFu) ? 0x1F : 0xF;
    case 9:
    case 10:
        return 0x7;
    default:
        return 0;
    }
}

s32 sceCertLoader_DD629A24(u32 cat, u32 mask, SceCertNode **outList, void *buf, u32 bufSize, u32 *outSize)
{
    s32 oldk1 = pspShiftK1();
    u32 k1mask = (u32)(oldk1 << 11);
    u8 *curBuf = (u8 *)buf;
    s32 writeMode = 0;
    s32 totalCount = 0;
    u32 totalBytes = 0;
    u32 bytesUsed = 0;

    if (outList != NULL && buf != NULL) {
        u32 baddr = (u32)buf;
        if ((s32)(k1mask & ((baddr + bufSize) | baddr | bufSize)) < 0) {
            pspSetK1(oldk1);
            return 0x80000023;
        }
        if ((s32)(k1mask & (u32)outList) < 0) {
            pspSetK1(oldk1);
            return 0x80000023;
        }
        *outList = NULL;
        writeMode = 1;
    }

    if (outSize != NULL) {
        if ((s32)(k1mask & (u32)outSize) < 0) {
            pspSetK1(oldk1);
            return 0x80000023;
        }
        *outSize = 0;
    }

    if (cat >= 15) {
        pspSetK1(oldk1);
        return 0x80000025;
    }

    if (cat != 0 && mask == 0xFFFFFFFFu) {
        mask = getDefaultCertMask(cat);
    }

    u32 sdkVer = sceKernelGetCompiledSdkVersion();
    u32 invalidBits = 0;
    switch (cat) {
    case 1:
    case 8:
    case 12:
    case 13:
        invalidBits = mask & ~0x3u;
        break;
    case 2:
        if (sdkVer <= 0x060600FFu) {
            if ((mask & ~0x3FFFu) != 0) {
                pspSetK1(oldk1);
                return 0x80000025;
            }
        }
        invalidBits = mask & ~0x7FFFu;
        break;
    case 3:
        invalidBits = mask & ~0x1Fu;
        break;
    case 4:
        invalidBits = (sdkVer <= 0x05050510u) ? (mask & ~0x7u) : (mask & ~0xFu);
        break;
    case 5:
        invalidBits = (sdkVer <= 0x060600FFu) ? (mask & ~0x1u) : (mask & ~0x3u);
        break;
    case 6:
    case 11:
    case 14:
        invalidBits = mask & ~0x1u;
        break;
    case 7:
        invalidBits = (sdkVer <= 0x060600FFu) ? (mask & ~0xFu) : (mask & ~0x1Fu);
        break;
    case 9:
    case 10:
        invalidBits = mask & ~0x7u;
        break;
    default:
        break;
    }
    if (invalidBits != 0) {
        pspSetK1(oldk1);
        return 0x80000025;
    }

#define LOAD_CAT(c, cond) \
    do { \
        if (cat == (c) || (cat == 0 && (cond))) { \
            if (cat == 0) \
                mask = getDefaultCertMask(c); \
            s32 n = loadCertCategory((c), mask, outList, curBuf, bufSize - totalBytes, &bytesUsed, writeMode); \
            if (n < 0) { \
                pspSetK1(oldk1); \
                return n; \
            } \
            totalCount += n; \
            totalBytes += bytesUsed; \
            curBuf += bytesUsed; \
            bytesUsed = 0; \
        } \
    } while (0)

    LOAD_CAT(1, 1);
    LOAD_CAT(3, sdkVer > 0x03030010u);
    LOAD_CAT(2, 1);
    LOAD_CAT(4, sdkVer > 0x0200000Fu);
    LOAD_CAT(5, sdkVer > 0x02000010u);
    LOAD_CAT(6, sdkVer > 0x02000010u);
    LOAD_CAT(7, sdkVer > 0x02000010u);
    LOAD_CAT(8, sdkVer > 0x02060010u);
    LOAD_CAT(9, sdkVer > 0x04010010u);
    LOAD_CAT(10, sdkVer > 0x0605FFFFu);
    LOAD_CAT(11, sdkVer > 0x060600FFu);
    LOAD_CAT(12, sdkVer > 0x060600FFu);
    LOAD_CAT(13, sdkVer > 0x060600FFu);
    LOAD_CAT(14, sdkVer > 0x060600FFu);
#undef LOAD_CAT

    if (outSize != NULL) {
        *outSize = totalBytes;
    }
    pspSetK1(oldk1);
    return totalCount;
}

s32 sceCertLoader_driver_11391041(void)
{
    return 0;
}

s32 sceCertLoader_driver_E86A7DC1(void)
{
    return 0;
}

static s32 loadCertCategoryInternal(u32 cat, u32 mask, SceCertNode **outList,
                                    u8 *buf, u32 bufSize, u32 *outSize, s32 writeMode)
{
    u32 curK1 = pspGetK1();
    s32 fileDesc = sceIoOpen("flash0:/data/cert/CA_LIST.cer", 0x04000001, 0);
    if (fileDesc < 0) {
        return fileDesc;
    }

    s32 ret = 0;
    s32 done = 0;
    s32 failed = 0;
    s32 count = 0;
    u32 totalBytes = 0;
    SceCertNode *head = NULL;
    SceCertNode *tail = NULL;

    while (!done && !failed) {
        s64 offset = 0;
        u32 certSize = 0;

        if (cat >= 15 || cat == 0) {
            ret = 0x80000025;
            failed = 1;
            break;
        }

        switch (cat) {
        case 1:
            if (mask & 0x1) { offset = 28824; certSize = 1084; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 29924; certSize = 1254; mask ^= 0x2; }
            else { done = 1; }
            break;
        case 2:
            if (mask & 0x1) { offset = 15784; certSize = 854; mask ^= 0x1; }
            else if (mask & 0x8) { offset = 13092; certSize = 1122; mask ^= 0x8; }
            else if (mask & 0x80) { offset = 14244; certSize = 1508; mask ^= 0x80; }
            else if (mask & 0x2) { offset = 15784; certSize = 848; mask ^= 0x2; }
            else if (mask & 0x10) { offset = 16668; certSize = 1126; mask ^= 0x10; }
            else if (mask & 0x100) { offset = 17828; certSize = 1504; mask ^= 0x100; }
            else if (mask & 0x4) { offset = 22936; certSize = 848; mask ^= 0x4; }
            else if (mask & 0x20) { offset = 20244; certSize = 1122; mask ^= 0x20; }
            else if (mask & 0x200) { offset = 21396; certSize = 1508; mask ^= 0x200; }
            else if (mask & 0x40) { offset = 23816; certSize = 1122; mask ^= 0x40; }
            else if (mask & 0x400) { offset = 24972; certSize = 1508; mask ^= 0x400; }
            else if (mask & 0x1000) { offset = 26516; certSize = 840; mask ^= 0x1000; }
            else if (mask & 0x800) { offset = 27396; certSize = 1402; mask ^= 0x800; }
            else if (mask & 0x2000) { offset = 44192; certSize = 1760; mask ^= 0x2000; }
            else if (mask & 0x4000) { offset = 51792; certSize = 1728; mask ^= 0x4000; }
            else { done = 1; }
            break;
        case 3:
            if (mask & 0x1) { offset = 31200; certSize = 1410; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 32632; certSize = 1410; mask ^= 0x2; }
            else if (mask & 0x4) { offset = 34064; certSize = 1410; mask ^= 0x4; }
            else if (mask & 0x8) { offset = 35496; certSize = 1410; mask ^= 0x8; }
            else if (mask & 0x10) { offset = 36928; certSize = 1410; mask ^= 0x10; }
            else { done = 1; }
            break;
        case 4:
            if (mask & 0x1) { offset = 24; certSize = 1236; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 1284; certSize = 1162; mask ^= 0x2; }
            else if (mask & 0x4) { offset = 2480; certSize = 948; mask ^= 0x4; }
            else if (mask & 0x8) { offset = 43200; certSize = 964; mask ^= 0x8; }
            else { done = 1; }
            break;
        case 5:
            if (mask & 0x1) { offset = 3464; certSize = 1768; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 50204; certSize = 1558; mask ^= 0x2; }
            else { done = 1; }
            break;
        case 6:
            if (mask & 0x1) { offset = 5256; certSize = 1084; mask ^= 0x1; }
            else { done = 1; }
            break;
        case 7:
            if (mask & 0x1) { offset = 6376; certSize = 1282; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 7692; certSize = 890; mask ^= 0x2; }
            else if (mask & 0x4) { offset = 8612; certSize = 758; mask ^= 0x4; }
            else if (mask & 0x8) { offset = 9408; certSize = 1282; mask ^= 0x8; }
            else if (mask & 0x10) { offset = 63924; certSize = 1250; mask ^= 0x10; }
            else { done = 1; }
            break;
        case 8:
            if (mask & 0x1) { offset = 10720; certSize = 1174; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 11916; certSize = 1146; mask ^= 0x2; }
            else { done = 1; }
            break;
        case 9:
            if (mask & 0x1) { offset = 38372; certSize = 1546; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 39952; certSize = 1542; mask ^= 0x2; }
            else if (mask & 0x4) { offset = 41528; certSize = 1632; mask ^= 0x4; }
            else { done = 1; }
            break;
        case 10:
            if (mask & 0x1) { offset = 45992; certSize = 1390; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 47416; certSize = 1372; mask ^= 0x2; }
            else if (mask & 0x4) { offset = 48816; certSize = 1360; mask ^= 0x4; }
            else { done = 1; }
            break;
        case 11:
            if (mask & 0x1) { offset = 53552; certSize = 1390; mask ^= 0x1; }
            else { done = 1; }
            break;
        case 12:
            if (mask & 0x1) { offset = 54976; certSize = 1422; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 56428; certSize = 1448; mask ^= 0x2; }
            else { done = 1; }
            break;
        case 13:
            if (mask & 0x1) { offset = 57892; certSize = 2714; mask ^= 0x1; }
            else if (mask & 0x2) { offset = 60628; certSize = 1958; mask ^= 0x2; }
            else { done = 1; }
            break;
        case 14:
            if (mask & 0x1) { offset = 62620; certSize = 1282; mask ^= 0x1; }
            else { done = 1; }
            break;
        }

        if (done != 1) {
            u32 pad = (-(certSize + 1)) & 3u;
            s64 seekRes = sceIoLseek(fileDesc, offset, 0);
            if (seekRes < 0) {
                ret = (s32)seekRes;
                failed = 1;
                break;
            }
            u32 paddedSize = certSize + pad;
            u8 *certData = NULL;
            if (writeMode == 1) {
                if (bufSize - totalBytes < paddedSize + 9) {
                    ret = 0x80000022;
                    failed = 1;
                    break;
                }
                ret = sceIoRead(fileDesc, buf, certSize);
                if (ret < 0) {
                    failed = 1;
                    break;
                }
                buf[certSize] = '\0';
                certData = buf;
                buf += paddedSize + 1;
            }
            totalBytes += paddedSize + 1;
            count++;
            if (writeMode == 1) {
                SceCertNode *node = (SceCertNode *)buf;
                if (tail != NULL) {
                    tail->next = node;
                } else {
                    head = node;
                }
                tail = node;
                node->next = NULL;
                node->data = certData;
                buf += sizeof(SceCertNode);
            }
            totalBytes += sizeof(SceCertNode);
        }
    }

    if (!failed && writeMode == 1) {
        if (*outList != NULL) {
            SceCertNode *cur = *outList;
            if ((s32)(curK1 & (u32)cur) < 0) {
                ret = 0x80000023;
                failed = 1;
            } else {
                while (cur->next != NULL) {
                    if ((s32)(curK1 & (u32)cur->next) < 0) {
                        ret = 0x80000023;
                        failed = 1;
                        break;
                    }
                    cur = cur->next;
                }
                if (!failed) {
                    cur->next = head;
                }
            }
        } else {
            *outList = head;
        }
    }

    if (!failed) {
        if (outSize != NULL) {
            *outSize += totalBytes;
        }
        ret = count;
    }

    if (fileDesc != 0) {
        sceIoClose(fileDesc);
    }
    return ret;
}
