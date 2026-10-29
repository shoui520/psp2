#include <common_imp.h>

SCE_MODULE_INFO("sceCert_Loader", SCE_MODULE_KERNEL | SCE_MODULE_ATTR_EXCLUSIVE_LOAD | SCE_MODULE_ATTR_EXCLUSIVE_START, 1, 2);

extern int sceIoOpen(const char *file, int flags, int mode);
extern int sceIoClose(int fd);
extern s64 sceIoLseek(int fd, s64 offset, int whence);
extern int sceIoRead(int fd, void *data, u32 size);
extern int sceKernelGetCompiledSdkVersion(void);

int sub_0a30(u32 cat, u32 mask, u32 buf, u32 buf_size, u32 count_ptr, u32 *out_size, int mode);

int sub_0918(u32 cat, u32 mask, u32 buf, u32 buf_size, u32 count_ptr, u32 *out_size)
{
    return sub_0a30(cat, mask, buf, buf_size, count_ptr, out_size, 1);
}

u32 sub_0934(u32 cat)
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
                        ret = sub_0a30(a4, m, a0, a1, a2, a3, 1); \
                    else \
                        ret = 0x80510002; \
                } else { \
                    ret = 0x80510002; \
                } \
            } else if (a0 != 0xFFFFFFFF) { \
                u32 m = (mask_expr); \
                if (m != 0) \
                    ret = sub_0a30(a4, m, 0, 0, a2, a3, 0); \
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
                    ret = sub_0a30(0, 0x7FFFFFFF, a0, a1, a2, a3, 1);
                else
                    ret = 0x80510002;
            } else if (a0 != 0xFFFFFFFF) {
                ret = sub_0a30(a4, a0, 0, 0, a2, a3, 0);
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

int sub_0a30(u32 cat, u32 mask, u32 buf, u32 buf_size, u32 count_ptr, u32 *out_size, int mode)
{
    if (cat >= 16 || out_size == NULL)
        return 0x80510002;

    if (mode != 0) {
        if (mode != 1)
            return 0x80510002;
        if (buf == 0 || buf_size == 0 || count_ptr == 0)
            return 0x80510002;
    } else {
        if (buf != 0 || buf_size != 0)
            return 0x80510002;
    }

    int fd = -1;
    if (mode == 1) {
        fd = sceIoOpen("flash0:/data/cert/CA_LIST.cer", 0x04000001, 0);
        if (fd < 0)
            return 0x80510003;
    }

    u32 cur_buf = buf;
    u32 total_size = 0;
    u32 count = 0;
    u32 prev_node = 0;
    int err = 0;

    for (u32 idx = 0; idx < 32; idx++) {
        s64 offset = 0;
        u32 cert_size = 0;
        int match = 0;

        switch (idx) {
        case 0:
            if (mask & 0x00000001) { offset = 0xF49C; cert_size = 0x502; match = 1; }
            break;
        case 1:
            if (mask & 0x00000002) { offset = 0xECD4; cert_size = 0x7A6; match = 1; }
            break;
        case 2:
            if (mask & 0x00000004) { offset = 0xE7EC; cert_size = 0x4C6; match = 1; }
            break;
        case 3:
            if (mask & 0x00000008) { offset = 0xE310; cert_size = 0x4BA; match = 1; }
            break;
        case 4:
            if (mask & 0x00000010) { offset = 0xDE30; cert_size = 0x4BE; match = 1; }
            break;
        case 5:
            if (mask & 0x00000020) { offset = 0xD930; cert_size = 0x4DF; match = 1; }
            break;
        case 6:
            if (mask & 0x00000040) { offset = 0xCFEC; cert_size = 0x922; match = 1; }
            break;
        case 7:
            if (mask & 0x00000080) { offset = 0xC6C8; cert_size = 0x902; match = 1; }
            break;
        case 8:
            if (mask & 0x00000100) { offset = 0xC198; cert_size = 0x50D; match = 1; }
            break;
        case 9:
            if (mask & 0x00000200) { offset = 0xBAFC; cert_size = 0x67A; match = 1; }
            break;
        case 10:
            if (mask & 0x00000400) { offset = 0xB5D4; cert_size = 0x505; match = 1; }
            break;
        case 11:
            if (mask & 0x00000800) { offset = 0xAFEC; cert_size = 0x5C6; match = 1; }
            break;
        case 12:
            if (mask & 0x00001000) { offset = 0xAA0C; cert_size = 0x5BE; match = 1; }
            break;
        case 13:
            if (mask & 0x00002000) { offset = 0xA428; cert_size = 0x5C1; match = 1; }
            break;
        case 14:
            if (mask & 0x00004000) { offset = 0x9D8C; cert_size = 0x679; match = 1; }
            break;
        case 15:
            if (mask & 0x00008000) { offset = 0x9888; cert_size = 0x4E1; match = 1; }
            break;
        case 16:
            if (mask & 0x00010000) { offset = 0x929C; cert_size = 0x5CA; match = 1; }
            break;
        case 17:
            if (mask & 0x00020000) { offset = 0x8CA4; cert_size = 0x5D6; match = 1; }
            break;
        case 18:
            if (mask & 0x00040000) { offset = 0x86A4; cert_size = 0x5DD; match = 1; }
            break;
        case 19:
            if (mask & 0x00080000) { offset = 0x80EC; cert_size = 0x595; match = 1; }
            break;
        case 20:
            if (mask & 0x00100000) { offset = 0x7BA4; cert_size = 0x526; match = 1; }
            break;
        case 21:
            if (mask & 0x00200000) { offset = 0x765C; cert_size = 0x526; match = 1; }
            break;
        case 22:
            if (mask & 0x00400000) { offset = 0x70E8; cert_size = 0x552; match = 1; }
            break;
        case 23:
            if (mask & 0x00800000) { offset = 0x6AA4; cert_size = 0x622; match = 1; }
            break;
        case 24:
            if (mask & 0x01000000) { offset = 0x64F8; cert_size = 0x589; match = 1; }
            break;
        case 25:
            if (mask & 0x02000000) { offset = 0x5DC4; cert_size = 0x711; match = 1; }
            break;
        case 26:
            if (mask & 0x04000000) { offset = 0x5574; cert_size = 0x82D; match = 1; }
            break;
        case 27:
            if (mask & 0x08000000) { offset = 0x5098; cert_size = 0x4BA; match = 1; }
            break;
        case 28:
            if (mask & 0x10000000) { offset = 0x4AE4; cert_size = 0x591; match = 1; }
            break;
        case 29:
            if (mask & 0x20000000) { offset = 0x4108; cert_size = 0x9BB; match = 1; }
            break;
        case 30:
            if (mask & 0x40000000) { offset = 0x3978; cert_size = 0x76E; match = 1; }
            break;
        case 31:
            if (mask & 0x80000000) { offset = 0x31E8; cert_size = 0x76D; match = 1; }
            break;
        default:
            return 0x80510002;
        }

        if (!match)
            continue;

        u32 entry_size = (cat >= 10) ? (cert_size + 28) : (cert_size + 1);
        total_size += entry_size;

        if (mode == 1) {
            if (total_size > buf_size) {
                err = 0x80510001;
                break;
            }
            s64 seek_res = sceIoLseek(fd, offset, 0);
            if (seek_res != offset) {
                err = 0x80510003;
                break;
            }
            if (cat < 10) {
                int r = sceIoRead(fd, (void *)cur_buf, cert_size);
                if ((u32)r != cert_size) {
                    err = 0x80510003;
                    break;
                }
                *(u8 *)(cur_buf + cert_size) = 0;
                cur_buf += entry_size;
            } else {
                u32 data_ptr = cur_buf + 28;
                int r = sceIoRead(fd, (void *)data_ptr, cert_size);
                if ((u32)r != cert_size) {
                    err = 0x80510003;
                    break;
                }
                u32 *node = (u32 *)cur_buf;
                node[0] = data_ptr;
                node[1] = cert_size;
                node[2] = 0;
                node[3] = 0;
                node[4] = 0;
                node[5] = prev_node;
                node[6] = 0;
                if (prev_node != 0)
                    *(u32 *)(prev_node + 24) = cur_buf;
                prev_node = cur_buf;
                cur_buf = data_ptr + cert_size;
            }
        }
        count++;
    }

    if (mode == 1) {
        int close_res = sceIoClose(fd);
        if (close_res < 0 && err == 0)
            return 0x80510003;
    }

    if (err != 0)
        return err;

    *out_size = total_size;
    if (count_ptr != 0) {
        if (cat < 10)
            *(u32 *)count_ptr = count;
        else
            *(u32 *)count_ptr = buf;
    }
    return 0;
}
