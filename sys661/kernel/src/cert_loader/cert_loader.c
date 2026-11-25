#include <common_imp.h>

SCE_MODULE_INFO("sceCert_Loader", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 2);

extern int sceIoOpen(const char *file, int flags, int mode);
extern int sceIoClose(int fd);
extern s64 sceIoLseek(int fd, s64 offset, int whence);
extern int sceIoRead(int fd, void *data, u32 size);
extern int sceKernelGetCompiledSdkVersion(void);

typedef struct SceCertNode {
    void *data;
    u32 size;
    u32 reserved[3];
    struct SceCertNode *prev;
    struct SceCertNode *next;
} SceCertNode;

int loadCertificates(u32 cat, u32 mask, u32 buf, u32 bufSize, u32 countPtr, u32 *outSize, int mode);

int loadCertificatesDirect(u32 cat, u32 mask, u32 buf, u32 bufSize, u32 countPtr, u32 *outSize)
{
    return loadCertificates(cat, mask, buf, bufSize, countPtr, outSize, 1);
}

u32 getCertCategoryMask(u32 cat)
{
    switch (cat) {
    case 0:
        return 0x000001BF;
    case 1:
        return (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x17000018 : 0x00000018;
    case 2:
        return (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x15000400 : 0x00000400;
    case 3:
        return (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x14002000 : 0x00002000;
    case 4:
        return (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x15810000 : 0x00810000;
    case 5:
        return (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x17020000 : 0x00020000;
    case 6:
        return 0x17000000;
    case 7:
        return (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x15400000 : 0x00400000;
    case 8:
        return 0x1C000000;
    case 9:
        return 0x14200000;
    case 10:
        return 0x15100000;
    case 11:
        return 0x15080000;
    case 12:
        return 0x15000800;
    case 13:
        return 0x34000000;
    case 14:
        return 0x90000000;
    default:
        return 0;
    }
}

#define HANDLE_CERT_CASE(cat_idx, mask_expr) \
    do { \
        if (a2 != 0 && a3 != NULL) { \
            if (a1 != 0) { \
                if (a0 != 0) { \
                    u32 m = (mask_expr); \
                    if (m != 0) \
                        ret = loadCertificates(a4, m, a0, a1, a2, a3, 1); \
                    else \
                        ret = 0x80510002; \
                } else { \
                    ret = 0x80510002; \
                } \
            } else if (a0 != 0xFFFFFFFF) { \
                u32 m = (mask_expr); \
                if (m != 0) \
                    ret = loadCertificates(a4, m, 0, 0, a2, a3, 0); \
                else \
                    ret = 0x80510002; \
            } else { \
                ret = 0x80510002; \
            } \
        } else { \
            ret = 0x80510002; \
        } \
    } while (0)

int sceCertLoader_DD629A24(u32 a0, u32 a1, u32 a2, u32 *a3, u32 a4)
{
    int k1 = pspShiftK1();
    int ret;

    if (a4 >= 16) {
        pspSetK1(k1);
        return 0x80510002;
    }

    switch (a4) {
    case 0:
        if (a2 != 0 && a3 != NULL) {
            if (a1 != 0) {
                if (a0 != 0)
                    ret = loadCertificates(0, 0x7FFFFFFF, a0, a1, a2, a3, 1);
                else
                    ret = 0x80510002;
            } else if (a0 != 0xFFFFFFFF) {
                ret = loadCertificates(a4, a0, 0, 0, a2, a3, 0);
            } else {
                ret = 0x80510002;
            }
        } else {
            ret = 0x80510002;
        }
        break;
    case 1:
        HANDLE_CERT_CASE(0, 0x000001BF);
        break;
    case 2:
        HANDLE_CERT_CASE(1, (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x17000018 : 0x00000018);
        break;
    case 3:
        HANDLE_CERT_CASE(2, (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x15000400 : 0x00000400);
        break;
    case 4:
        HANDLE_CERT_CASE(3, (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x14002000 : 0x00002000);
        break;
    case 5:
        HANDLE_CERT_CASE(4, (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x15810000 : 0x00810000);
        break;
    case 6:
        HANDLE_CERT_CASE(5, (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x17020000 : 0x00020000);
        break;
    case 7:
        HANDLE_CERT_CASE(6, 0x17000000);
        break;
    case 8:
        HANDLE_CERT_CASE(7, (sceKernelGetCompiledSdkVersion() >= 0x05000000) ? 0x15400000 : 0x00400000);
        break;
    case 9:
        HANDLE_CERT_CASE(8, 0x1C000000);
        break;
    case 10:
        HANDLE_CERT_CASE(9, 0x14200000);
        break;
    case 11:
        HANDLE_CERT_CASE(10, 0x15100000);
        break;
    case 12:
        HANDLE_CERT_CASE(11, 0x15080000);
        break;
    case 13:
        HANDLE_CERT_CASE(12, 0x15000800);
        break;
    case 14:
        HANDLE_CERT_CASE(13, 0x34000000);
        break;
    case 15:
        HANDLE_CERT_CASE(14, 0x90000000);
        break;
    default:
        ret = 0x80510002;
        break;
    }

    pspSetK1(k1);
    return ret;
}

int sceCertLoader_driver_11391041(void)
{
    return 0;
}

int sceCertLoader_driver_E86A7DC1(void)
{
    return 0;
}

int loadCertificates(u32 cat, u32 mask, u32 buf, u32 bufSize, u32 countPtr, u32 *outSize, int mode)
{
    if (cat >= 16 || outSize == NULL)
        return 0x80510002;

    if (mode != 0) {
        if (mode != 1)
            return 0x80510002;
        if (buf == 0 || bufSize == 0 || countPtr == 0)
            return 0x80510002;
    } else {
        if (buf != 0 || bufSize != 0)
            return 0x80510002;
    }

    int fd = -1;
    if (mode == 1) {
        fd = sceIoOpen("flash0:/data/cert/CA_LIST.cer", 0x04000001, 0);
        if (fd < 0)
            return 0x80510003;
    }

    u8 *curBuf = (u8 *)buf;
    u32 totalSize = 0;
    u32 count = 0;
    SceCertNode *prevNode = NULL;
    int err = 0;

    for (u32 idx = 0; idx < 32; idx++) {
        s64 offset = 0;
        u32 certSize = 0;
        int match = 0;

        switch (idx) {
        case 0:
            if (mask & 0x00000001) { offset = 0xF49C; certSize = 0x502; match = 1; }
            break;
        case 1:
            if (mask & 0x00000002) { offset = 0xECD4; certSize = 0x7A6; match = 1; }
            break;
        case 2:
            if (mask & 0x00000004) { offset = 0xE7EC; certSize = 0x4C6; match = 1; }
            break;
        case 3:
            if (mask & 0x00000008) { offset = 0xE310; certSize = 0x4BA; match = 1; }
            break;
        case 4:
            if (mask & 0x00000010) { offset = 0xDE30; certSize = 0x4BE; match = 1; }
            break;
        case 5:
            if (mask & 0x00000020) { offset = 0xD930; certSize = 0x4DF; match = 1; }
            break;
        case 6:
            if (mask & 0x00000040) { offset = 0xCFEC; certSize = 0x922; match = 1; }
            break;
        case 7:
            if (mask & 0x00000080) { offset = 0xC6C8; certSize = 0x902; match = 1; }
            break;
        case 8:
            if (mask & 0x00000100) { offset = 0xC198; certSize = 0x50D; match = 1; }
            break;
        case 9:
            if (mask & 0x00000200) { offset = 0xBAFC; certSize = 0x67A; match = 1; }
            break;
        case 10:
            if (mask & 0x00000400) { offset = 0xB5D4; certSize = 0x505; match = 1; }
            break;
        case 11:
            if (mask & 0x00000800) { offset = 0xAFEC; certSize = 0x5C6; match = 1; }
            break;
        case 12:
            if (mask & 0x00001000) { offset = 0xAA0C; certSize = 0x5BE; match = 1; }
            break;
        case 13:
            if (mask & 0x00002000) { offset = 0xA428; certSize = 0x5C1; match = 1; }
            break;
        case 14:
            if (mask & 0x00004000) { offset = 0x9D8C; certSize = 0x679; match = 1; }
            break;
        case 15:
            if (mask & 0x00008000) { offset = 0x9888; certSize = 0x4E1; match = 1; }
            break;
        case 16:
            if (mask & 0x00010000) { offset = 0x929C; certSize = 0x5CA; match = 1; }
            break;
        case 17:
            if (mask & 0x00020000) { offset = 0x8CA4; certSize = 0x5D6; match = 1; }
            break;
        case 18:
            if (mask & 0x00040000) { offset = 0x86A4; certSize = 0x5DD; match = 1; }
            break;
        case 19:
            if (mask & 0x00080000) { offset = 0x80EC; certSize = 0x595; match = 1; }
            break;
        case 20:
            if (mask & 0x00100000) { offset = 0x7BA4; certSize = 0x526; match = 1; }
            break;
        case 21:
            if (mask & 0x00200000) { offset = 0x765C; certSize = 0x526; match = 1; }
            break;
        case 22:
            if (mask & 0x00400000) { offset = 0x70E8; certSize = 0x552; match = 1; }
            break;
        case 23:
            if (mask & 0x00800000) { offset = 0x6AA4; certSize = 0x622; match = 1; }
            break;
        case 24:
            if (mask & 0x01000000) { offset = 0x64F8; certSize = 0x589; match = 1; }
            break;
        case 25:
            if (mask & 0x02000000) { offset = 0x5DC4; certSize = 0x711; match = 1; }
            break;
        case 26:
            if (mask & 0x04000000) { offset = 0x5574; certSize = 0x82D; match = 1; }
            break;
        case 27:
            if (mask & 0x08000000) { offset = 0x5098; certSize = 0x4BA; match = 1; }
            break;
        case 28:
            if (mask & 0x10000000) { offset = 0x4AE4; certSize = 0x591; match = 1; }
            break;
        case 29:
            if (mask & 0x20000000) { offset = 0x4108; certSize = 0x9BB; match = 1; }
            break;
        case 30:
            if (mask & 0x40000000) { offset = 0x3978; certSize = 0x76E; match = 1; }
            break;
        case 31:
            if (mask & 0x80000000) { offset = 0x31E8; certSize = 0x76D; match = 1; }
            break;
        default:
            return 0x80510002;
        }

        if (!match)
            continue;

        u32 entrySize = (cat >= 10) ? (certSize + sizeof(SceCertNode)) : (certSize + 1);
        totalSize += entrySize;

        if (mode == 1) {
            if (totalSize > bufSize) {
                err = 0x80510001;
                break;
            }
            s64 seekRes = sceIoLseek(fd, offset, 0);
            if (seekRes != offset) {
                err = 0x80510003;
                break;
            }
            if (cat < 10) {
                int r = sceIoRead(fd, curBuf, certSize);
                if ((u32)r != certSize) {
                    err = 0x80510003;
                    break;
                }
                curBuf[certSize] = 0;
                curBuf += entrySize;
            } else {
                SceCertNode *node = (SceCertNode *)curBuf;
                u8 *dataPtr = curBuf + sizeof(SceCertNode);
                int r = sceIoRead(fd, dataPtr, certSize);
                if ((u32)r != certSize) {
                    err = 0x80510003;
                    break;
                }
                node->data = dataPtr;
                node->size = certSize;
                node->reserved[0] = 0;
                node->reserved[1] = 0;
                node->reserved[2] = 0;
                node->prev = prevNode;
                node->next = NULL;
                if (prevNode != NULL)
                    prevNode->next = node;
                prevNode = node;
                curBuf = dataPtr + certSize;
            }
        }
        count++;
    }

    if (mode == 1) {
        int closeRes = sceIoClose(fd);
        if (closeRes < 0 && err == 0)
            return 0x80510003;
    }

    if (err != 0)
        return err;

    *outSize = totalSize;
    if (countPtr != 0) {
        if (cat < 10)
            *(u32 *)countPtr = count;
        else
            *(u32 *)countPtr = buf;
    }
    return 0;
}
