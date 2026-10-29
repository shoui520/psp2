#include <common_imp.h>

extern int sceDrmBBCipherUpdate(void *ckey, void *data, int size);
extern int sceDrmBBCipherInit(void *ckey, int type, int mode, void *header_key, void *version_key, u32 seed);
extern int sceDrmBBMacInit(void *mkey, int type);
extern int sceAmctrl_driver_9227EA79(void *mkey, const void *buf, int size);
extern int sceDrmBBCipherFinal(void *ckey);
extern int sceAmctrl_driver_E04ADD4C(void *ckey, void *data, int size);
extern int sceDrmBBMacFinal(void *mkey, void *buf, void *vkey);
extern int sceDrmBBMacFinal2(void *mkey, void *out, void *vkey);

extern void *sceKernelMemcpy(void *dst, const void *src, u32 n);
extern void *sceKernelMemset(void *s, int c, u32 n);
extern int memcmp(const void *s1, const void *s2, u32 n);

extern int sceKernelDeleteSema(int semaid);
extern int sceKernelSignalSema(int semaid, int signal);
extern int sceKernelWaitSema(int semaid, int signal, u32 *timeout);
extern int sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *option);

extern int sceIoAddHook(void *hook);

extern int sceKernelApplicationType(void);
extern char *sceKernelInitDiscImage(void);

typedef struct {
    int (*open)(void *arg, char *file, int flags, int mode);
    int (*close)(void *arg);
    int (*read)(void *arg, void *data, int len);
    int (*write)(void *arg, const void *data, int len);
    s64 (*lseek)(void *arg, s64 ofs, int whence);
    int (*ioctl)(void *arg, u32 cmd, void *indata, int inlen, void *outdata, int outlen);
} SceIoHookFuncs;

typedef struct {
    int unk0;
    void *arg;
    SceIoHookFuncs *funcs;
    void *priv;
} SceIoHookArg;

typedef struct {
    u32 pgd_info[12];  /* 0x00..0x2f */
    u32 type;          /* 0x30 */
    u8 vkey[16];       /* 0x34..0x43 */
    u32 in_use;        /* 0x44 */
    u32 flags;         /* 0x48 */
    u32 base_offset;   /* 0x4c */
    u32 cur_pos;       /* 0x50 */
    u32 data_offset;   /* 0x54 */
    u32 table_offset;  /* 0x58 */
    u32 check_table;   /* 0x5c */
} SceDnasFileSlot;     /* size 0x60 */

typedef struct {
    SceDnasFileSlot slots[8]; /* 0x1980..0x1c7f (0x300 bytes) */
    int semaid;               /* 0x1c80 */
    int open_count;           /* 0x1c84 */
    int pad[2];               /* 0x1c88..0x1c8f */
} SceDnasState;

static SceDnasState g_state __attribute__((section(".data"))) = { .semaid = 0 };
static u32 g_dnas_key1[4] = { 0x2d5de2ed, 0xe512f8bb, 0x32595c3c, 0x43e2e3fa };
static u32 g_dnas_key2[4] = { 0xebfb7427, 0xd701a0a4, 0x339e5602, 0x8357198c };

static int g_reboot_flag;          /* 0x1cb0 */
static u8 g_work_buf[0x640];       /* 0x1cc0..0x22ff */

static const u32 g_pgd_mac_key[8] = {
    0x4054052a, 0xe31fd962, 0xc62bd0f2, 0x0e20ff21,
    0xdf2844b1, 0x5b14cd0a, 0x903619c8, 0x2f9942d1
};

static int sub_008c(void);
static int sub_01a0(void);
static int sub_01a8(void);
static int sub_01b0(int a0, int a1, int flags);
static int sub_01b8(void);
static int sub_01f4(SceIoHookArg *harg, char *file, int flags, int mode);
static int sub_0468(SceIoHookArg *harg);
static s64 sub_04ec(SceIoHookArg *harg, s64 ofs, int whence);
static int sub_0618(SceIoHookArg *harg, char *data, int len);
static int sub_0aa8(SceIoHookArg *harg, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen);
static int sub_0c34(SceIoHookArg *harg, void *key);
static int sub_1108(SceIoHookArg *harg, u32 offset);
static int sub_1184(SceIoHookArg *harg, u32 flag);
static int sub_11f4(SceIoHookArg *harg, void *outdata, u32 offset, int size);
static int sub_1314(SceIoHookArg *harg);
static int sub_1398(SceIoHookArg *harg);
static int sub_1428(void *buf, int size, u32 block, void *vkey, void *header_key, int mode);
static int sub_14fc(const void *buf, int size, const void *vkey, const void *mac, int type, u32 xor_val);
static int sub_1614(const void *buf, int size, const void *vkey, const void *mac, int type);

static const void * const g_hook_funcs[9] = {
    sub_01a0,
    sub_01a8,
    sub_01b0,
    sub_01f4,
    sub_0468,
    sub_0618,
    NULL,
    sub_04ec,
    sub_0aa8
};

static const void * const g_hook_desc[5] = {
    "gamedata",
    NULL,
    NULL,
    "filemgr_dnas",
    g_hook_funcs
};

int module_reboot_phase(int phase)
{
    if (g_state.semaid <= 0)
        return 0;
    if (phase == 2)
        return sceKernelWaitSema(g_state.semaid, 1, NULL);
    if (phase < 3) {
        if (phase == 1)
            return sub_008c();
        return 0;
    }
    if (phase == 3)
        g_reboot_flag = 1;
    return 0;
}

static int sub_008c(void)
{
    return sceKernelSignalSema(g_state.semaid, 1);
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    g_reboot_flag = 0;
    int semaid = sceKernelCreateSema("SceIoFilemgrDNAS1", 0, 1, 1, NULL);
    if (semaid < 0) {
        sceKernelMemset(&g_state, 0, 0x330);
        sceKernelMemset(g_work_buf, 0, 0x640);
        return 1;
    }
    g_state.semaid = semaid;
    sceIoAddHook((void *)g_hook_desc);
    return 0;
}

int module_reboot_before(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    if (g_state.semaid > 0)
        sceKernelDeleteSema(g_state.semaid);
    sceKernelMemset(&g_state, 0, 0x330);
    sceKernelMemset(g_work_buf, 0, 0x640);
    return 0;
}

int module_stop(SceSize args, void *argp) __attribute__((alias("module_reboot_before")));

static int sub_01a0(void)
{
    return 0;
}

static int sub_01a8(void)
{
    return 0;
}

static int sub_01b0(int a0, int a1, int flags)
{
    (void)a0;
    (void)a1;
    return ((u32)flags >> 30) & 1;
}

static int sub_01b8(void)
{
    if (g_reboot_flag != 0)
        return 0x80000041;
    return sceKernelWaitSema(g_state.semaid, 1, NULL);
}

static int sub_01f4(SceIoHookArg *harg, char *file, int flags, int mode)
{
    if (flags & 2)
        return 0x80510201;

    int in_use_val = (((u32)flags >> 26) & 1) ? 1 : 2;
    int ret = sub_01b8();
    if (ret < 0)
        return ret;

    if ((u32)g_state.open_count >= 8) {
        sub_008c();
        return 0x80510202;
    }

    int fd = harg->funcs->open(harg->arg, file, flags, mode);
    if (fd < 0) {
        sub_008c();
        return fd;
    }

    g_state.open_count++;
    int ioctl_res = harg->funcs->ioctl(harg->arg, 0x00208002, NULL, 0, NULL, 0);
    if (ioctl_res < 0) {
        if (harg->funcs->ioctl(harg->arg, 0x00208011, NULL, 0, NULL, 0) < 0 &&
            harg->funcs->ioctl(harg->arg, 0x0020800c, NULL, 0, NULL, 0) < 0) {
            ret = 0x80020146;
            goto fail;
        }
    }

    int slot_idx = -1;
    for (int i = 0; i < 8; i++) {
        if (g_state.slots[i].in_use == 0) {
            sceKernelMemset(&g_state.slots[i], 0, sizeof(SceDnasFileSlot));
            g_state.slots[i].in_use = in_use_val;
            slot_idx = i;
            break;
        }
    }

    if (slot_idx < 0) {
        ret = 0x80510202;
        goto fail;
    }

    SceDnasFileSlot *slot = &g_state.slots[slot_idx];
    harg->priv = slot;
    slot->flags = 0;
    ret = sub_008c();
    return (ret < 0) ? ret : fd;

fail:
    if (fd >= 0) {
        harg->funcs->close(harg->arg);
        g_state.open_count--;
        if (slot_idx >= 0)
            sceKernelMemset(&g_state.slots[slot_idx], 0, sizeof(SceDnasFileSlot));
    }
    sub_008c();
    return (ret < 0) ? ret : (int)0x80510204;
}

static int sub_0468(SceIoHookArg *harg)
{
    SceDnasFileSlot *slot = (SceDnasFileSlot *)harg->priv;
    harg->funcs->close(harg->arg);
    int ret = sub_01b8();
    if (ret < 0)
        return ret;
    sceKernelMemset(slot, 0, sizeof(SceDnasFileSlot));
    if (g_state.open_count != 0)
        g_state.open_count--;
    sub_008c();
    return ret;
}

static s64 sub_04ec(SceIoHookArg *harg, s64 ofs, int whence)
{
    SceDnasFileSlot *slot = (SceDnasFileSlot *)harg->priv;
    if ((slot->flags & 0x10) == 0)
        return (s64)(s32)0x80510206;

    int ret = sub_01b8();
    if (ret < 0)
        return (s64)ret;

    s64 new_pos;
    if (whence == 0) {
        new_pos = ofs;
    } else if (whence == 1) {
        new_pos = ofs + (u64)slot->cur_pos;
    } else if (whence == 2) {
        new_pos = ofs + (u64)slot->pgd_info[5];
    } else {
        sub_008c();
        return (s64)(s32)0x80510201;
    }

    if (new_pos < 0)
        new_pos = 0;
    if ((u64)new_pos > (u64)slot->pgd_info[5])
        new_pos = (u64)slot->pgd_info[5];
    slot->cur_pos = (u32)new_pos;

    ret = sub_008c();
    if (ret < 0)
        return (s64)ret;
    return new_pos;
}

static int sub_0618(SceIoHookArg *harg, char *data, int len)
{
    SceDnasFileSlot *slot = (SceDnasFileSlot *)harg->priv;
    int ret = sub_01b8();
    if (ret < 0)
        return ret;

    if ((slot->flags & 0x10) == 0) {
        ret = 0x80510206;
        goto err_out;
    }

    if (len == 0) {
        ret = sub_008c();
        return ret;
    }

    u32 file_size = slot->pgd_info[5];
    u32 cur_pos = slot->cur_pos;
    if (cur_pos >= file_size) {
        ret = sub_008c();
        return (ret < 0) ? ret : 0;
    }

    int mac_type = 2;
    int cipher_mode = 2;
    if (slot->flags & 4) {
        mac_type = (slot->flags & 8) ? 3 : 1;
        cipher_mode = 1;
    }

    u32 end_pos = cur_pos + (u32)len;
    if (end_pos > file_size)
        end_pos = file_size;
    int skip_mac = (slot->check_table == 0) ? 1 : 0;
    u32 block_size = slot->pgd_info[6];

    ret = sub_008c();
    if (ret < 0)
        return ret;

    int total_read = 0;
    while (cur_pos < end_pos) {
        ret = sub_01b8();
        if (ret < 0)
            return ret;
        if ((slot->flags & 0x10) == 0) {
            ret = 0x80510203;
            goto err_out;
        }

        u32 block_off = cur_pos & (block_size - 1);
        u32 aligned_pos = cur_pos - block_off;
        u32 rem = end_pos - aligned_pos;
        u32 chunk_len;
        u32 read_len;
        u8 *work_ptr;

        if (block_off != 0 || rem < block_size) {
            chunk_len = block_size;
            read_len = block_size;
            work_ptr = g_work_buf;
            if (aligned_pos + block_size > file_size)
                read_len = ((file_size - aligned_pos) + 0x0f) & ~0x0fu;
            if (aligned_pos + block_size > end_pos)
                chunk_len = rem;
        } else {
            chunk_len = rem & ~(block_size - 1);
            read_len = chunk_len;
            work_ptr = (u8 *)data;
        }

        if (harg->funcs->lseek(harg->arg, (s64)(slot->data_offset + aligned_pos), 0) < 0) {
            ret = -1;
            goto err_out;
        }
        int r = harg->funcs->read(harg->arg, work_ptr, read_len);
        if (r < (int)read_len) {
            ret = r;
            goto err_out;
        }

        if (!skip_mac) {
            u32 blk_idx = aligned_pos / block_size;
            if (harg->funcs->lseek(harg->arg, (s64)(slot->table_offset + (blk_idx << 4)), 0) < 0) {
                ret = -1;
                goto err_out;
            }
            u32 verified = 0;
            u32 sub_idx = 0;
            while (verified < read_len) {
                u32 vchunk = read_len - verified;
                if (vchunk > block_size)
                    vchunk = block_size;
                if ((sub_idx & 0x1f) == 0) {
                    u32 nblocks = (read_len / block_size) - sub_idx;
                    if (nblocks == 0)
                        nblocks = 1;
                    else if (nblocks > 0x20)
                        nblocks = 0x20;
                    sceKernelMemset(g_work_buf + 0x400, 0, 0x200);
                    int mr = harg->funcs->read(harg->arg, g_work_buf + 0x400, nblocks << 4);
                    if (mr < (int)(nblocks << 4)) {
                        ret = mr;
                        goto err_out;
                    }
                }
                u8 *mac_ptr = g_work_buf + 0x400 + ((sub_idx & 0x1f) << 4);
                if (slot->type < 3)
                    ret = sub_1614(work_ptr + verified, vchunk, slot->vkey, mac_ptr, mac_type);
                else
                    ret = sub_14fc(work_ptr + verified, vchunk, slot->vkey, mac_ptr, mac_type, aligned_pos + verified);
                if (ret < 0)
                    goto err_out;
                verified += block_size;
                sub_idx++;
            }
        }

        u32 align16 = block_off & ~0x0fu;
        ret = sub_1428(work_ptr + align16, read_len - align16, (aligned_pos + block_off) >> 4, slot->vkey, slot, cipher_mode);
        if (ret < 0)
            goto err_out;

        u32 copy_len = chunk_len - block_off;
        if (work_ptr != (u8 *)data)
            sceKernelMemcpy(data, work_ptr + block_off, copy_len);

        cur_pos += copy_len;
        slot->cur_pos = cur_pos;
        data += copy_len;
        total_read += copy_len;
        aligned_pos += chunk_len;
        sub_008c();
        cur_pos = aligned_pos;
    }

    return total_read;

err_out:
    sceKernelMemset(g_work_buf, 0, 0x400);
    sub_008c();
    return (ret < 0) ? ret : (int)0x80510207;
}

static int sub_0aa8(SceIoHookArg *harg, unsigned int cmd, void *indata, int inlen, void *outdata, int outlen)
{
    u32 k1 = pspGetK1();

    switch (cmd - 0x04100001) {
    case 0:
        if (!indata || (u32)inlen < 0x10)
            return 0x80510201;
        return sub_0c34(harg, indata);
    case 1:
        if (!indata || (u32)inlen < 4)
            return 0x80510201;
        return sub_1108(harg, *(u32 *)indata);
    case 2:
        if (sceKernelApplicationType() != 0x300)
            return 0x80510201;
        return sub_1184(harg, 1);
    case 3:
        return sub_1184(harg, 0);
    case 4: {
        if (!indata || (u32)inlen < 8 || !outdata)
            return 0x80510201;
        u32 *in32 = (u32 *)indata;
        u32 rsize = in32[1];
        if ((u32)outlen < rsize)
            return 0x80510201;
        if ((s32)(k1 & (((u32)outdata + rsize) | (u32)outdata | rsize)) < 0)
            return 0x80510201;
        return sub_11f4(harg, outdata, in32[0], rsize);
    }
    case 5:
        return sub_1314(harg);
    case 15:
        return sub_1398(harg);
    default:
        return harg->funcs->ioctl(harg->arg, cmd, indata, inlen, outdata, outlen);
    }
}

static int sub_0c34(SceIoHookArg *harg, void *key)
{
    SceDnasFileSlot *slot = (SceDnasFileSlot *)harg->priv;
    if (!key)
        return 0x80510201;

    u32 slot_flags = slot->in_use;
    int ret = sceKernelWaitSema(g_state.semaid, 1, NULL);
    if (ret < 0)
        return ret;

    slot->flags = 0;
    s64 pos = harg->funcs->lseek(harg->arg, (s64)slot->base_offset, 0);
    if (pos != (s64)slot->base_offset) {
        ret = (int)pos;
        goto fail;
    }

    int r = harg->funcs->read(harg->arg, g_work_buf, 0x90);
    if (r < 0x90) {
        ret = r;
        goto fail;
    }

    if (memcmp(g_work_buf, "\0PGD", 4) != 0) {
        ret = 0x80510204;
        goto fail;
    }

    u32 version = *(u32 *)(g_work_buf + 4);
    if (version >= 4) {
        ret = 0x80510205;
        goto fail;
    }
    slot->type = version;

    if (version == 1) {
        sceKernelMemcpy(slot->vkey, key, 0x10);
    } else {
        void *mkey = g_work_buf + 0x618;
        ret = sceDrmBBMacInit(mkey, 1);
        if (ret < 0)
            goto fail;
        ret = sceAmctrl_driver_9227EA79(mkey, (const u8 *)g_pgd_mac_key + (version << 4) - 0x10, 0x10);
        if (ret < 0)
            goto fail;
        ret = sceDrmBBMacFinal(mkey, slot->vkey, key);
        if (ret < 0)
            goto fail;
    }

    u32 drm_type = *(u32 *)(g_work_buf + 8);
    int mac_type = 2;
    int cipher_mode = 2;
    if (drm_type == 1) {
        slot_flags |= 4;
        mac_type = 1;
        cipher_mode = 1;
        if (*(u32 *)(g_work_buf + 4) >= 2) {
            slot_flags |= 8;
            mac_type = 3;
        }
    } else if (drm_type != 0 || (slot_flags & 4) != 0) {
        ret = 0x80510204;
        goto fail;
    }

    const void *dnas_key = NULL;
    if (slot_flags & 2)
        dnas_key = g_dnas_key1;
    if (slot_flags & 1)
        dnas_key = g_dnas_key2;
    if (!dnas_key) {
        ret = 0x80510201;
        goto fail;
    }

    if (sub_1614(g_work_buf, 0x80, dnas_key, g_work_buf + 0x80, mac_type) < 0) {
        ret = 0x80510204;
        goto fail;
    }
    if (sub_1614(g_work_buf, 0x70, slot->vkey, g_work_buf + 0x70, mac_type) < 0) {
        ret = 0x80510206;
        goto fail;
    }

    ret = sub_1428(g_work_buf + 0x30, 0x30, 0, slot->vkey, g_work_buf + 0x10, cipher_mode);
    if (ret < 0)
        goto fail;
    if (*(u32 *)(g_work_buf + 0x40) != 0) {
        ret = 0x80510205;
        goto fail;
    }
    if (*(u32 *)(g_work_buf + 0x48) != 0x400) {
        ret = 0x80510204;
        goto fail;
    }

    slot->flags = slot_flags | 0x10;
    sceKernelMemcpy(slot->pgd_info, g_work_buf + 0x30, 0x30);

    u32 align_size = (slot->pgd_info[5] + 0x0f) & ~0x0fu;
    u32 blk_size = slot->pgd_info[6];
    u32 aligned_total = (align_size + blk_size - 1) & ~(blk_size - 1);
    u32 table_size = aligned_total / (blk_size >> 4);

    slot->data_offset = slot->pgd_info[7] + slot->base_offset;
    slot->table_offset = align_size + 0x90 + slot->base_offset;

    if (table_size <= 0x7ffff || *(u32 *)(g_work_buf + 4) >= 3) {
        u8 mac_tmp[16];
        sceKernelMemcpy(mac_tmp, g_work_buf + 0x60, 0x10);
        void *mkey = g_work_buf + 0x618;
        ret = sceDrmBBMacInit(mkey, mac_type);
        if (ret < 0)
            goto fail;
        if (harg->funcs->lseek(harg->arg, (s64)slot->table_offset, 0) < 0) {
            ret = -1;
            goto fail;
        }
        u32 done = 0;
        while (done < table_size) {
            u32 chunk = table_size - done;
            if (chunk > 0x400)
                chunk = 0x400;
            int tr = harg->funcs->read(harg->arg, g_work_buf, chunk);
            if (tr < (int)chunk) {
                ret = tr;
                goto fail;
            }
            done += 0x400;
            ret = sceAmctrl_driver_9227EA79(g_work_buf + 0x618, g_work_buf, chunk);
            if (ret < 0)
                goto fail;
        }
        ret = sceDrmBBMacFinal2(g_work_buf + 0x618, mac_tmp, slot->vkey);
        if (ret < 0)
            goto fail;
    }

    if (harg->funcs->ioctl(harg->arg, 0x00208011, NULL, 0, NULL, 0) == 0 &&
        sceKernelInitDiscImage() == NULL) {
        slot->check_table = 1;
    }

    return sceKernelSignalSema(g_state.semaid, 1);

fail:
    sceKernelSignalSema(g_state.semaid, 1);
    return (ret < 0) ? ret : (int)0x80510204;
}

static int sub_1108(SceIoHookArg *harg, u32 offset)
{
    SceDnasFileSlot *slot = (SceDnasFileSlot *)harg->priv;
    if (slot->base_offset == offset)
        return 0;
    int ret = sceKernelWaitSema(g_state.semaid, 1, NULL);
    if (ret < 0)
        return ret;
    slot->base_offset = offset;
    slot->flags = 0;
    return sceKernelSignalSema(g_state.semaid, 1);
}

static int sub_1184(SceIoHookArg *harg, u32 flag)
{
    SceDnasFileSlot *slot = (SceDnasFileSlot *)harg->priv;
    int ret = sceKernelWaitSema(g_state.semaid, 1, NULL);
    if (ret < 0)
        return ret;
    slot->check_table = flag;
    return sceKernelSignalSema(g_state.semaid, 1);
}

static int sub_11f4(SceIoHookArg *harg, void *outdata, u32 offset, int size)
{
    int ret = sceKernelWaitSema(g_state.semaid, 1, NULL);
    if (ret < 0)
        return ret;

    s64 pos = harg->funcs->lseek(harg->arg, (s64)offset, 0);
    if (pos == (s64)offset) {
        int r = harg->funcs->read(harg->arg, outdata, size);
        ret = r;
        if (r < size) {
            if (r != (int)0x80020328)
                ret = 0x80010005;
        }
    } else {
        ret = (int)pos;
        if (ret != (int)0x80020328)
            ret = 0x8001001d;
    }

    int sret = sceKernelSignalSema(g_state.semaid, 1);
    return (ret < 0) ? ret : sret;
}

static int sub_1314(SceIoHookArg *harg)
{
    SceDnasFileSlot *slot = (SceDnasFileSlot *)harg->priv;
    int ret = sceKernelWaitSema(g_state.semaid, 1, NULL);
    if (ret < 0)
        return ret;
    if ((slot->flags & 0x10) == 0) {
        sceKernelSignalSema(g_state.semaid, 1);
        return 0x80510206;
    }
    ret = sceKernelSignalSema(g_state.semaid, 1);
    return (ret < 0) ? ret : 0;
}

static int sub_1398(SceIoHookArg *harg)
{
    SceDnasFileSlot *slot = (SceDnasFileSlot *)harg->priv;
    int ret = sceKernelWaitSema(g_state.semaid, 1, NULL);
    if (ret < 0)
        return ret;
    if ((slot->flags & 0x10) == 0) {
        sceKernelSignalSema(g_state.semaid, 1);
        return 0x80510206;
    }
    int size = slot->pgd_info[5];
    ret = sceKernelSignalSema(g_state.semaid, 1);
    return (ret < 0) ? ret : size;
}

static int sub_1428(void *buf, int size, u32 block, void *vkey, void *header_key, int mode)
{
    void *ckey = g_work_buf + 0x600;
    int ret = sceDrmBBCipherInit(ckey, mode, 2, header_key, vkey, block);
    if (ret >= 0) {
        if ((u32)buf & 3)
            ret = sceDrmBBCipherUpdate(ckey, buf, size);
        else
            ret = sceAmctrl_driver_E04ADD4C(ckey, buf, size);
        if (ret >= 0) {
            ret = sceDrmBBCipherFinal(ckey);
            if (ret >= 0)
                return 0;
        }
    }
    sceKernelMemset(ckey, 0, 0x18);
    return ret;
}

static int sub_14fc(const void *buf, int size, const void *vkey, const void *mac, int type, u32 xor_val)
{
    u32 tmp_key[4];
    void *mkey = g_work_buf + 0x618;

    if (!mac)
        return 0x80510201;

    int ret = sceDrmBBMacInit(mkey, type);
    if (ret >= 0) {
        ret = sceAmctrl_driver_9227EA79(mkey, buf, size);
        if (ret >= 0) {
            sceKernelMemcpy(tmp_key, vkey, 0x10);
            if (xor_val != 0)
                tmp_key[0] ^= xor_val;
            ret = sceDrmBBMacFinal2(mkey, (void *)mac, tmp_key);
            ret = (ret < 0) ? (int)0x80510207 : 0;
        }
    }
    sceKernelMemset(mkey, 0, 0x28);
    sceKernelMemset(tmp_key, 0, 0x10);
    return ret;
}

static int sub_1614(const void *buf, int size, const void *vkey, const void *mac, int type)
{
    return sub_14fc(buf, size, vkey, mac, type, 0);
}
