#include <common_imp.h>

extern int sceKernelDeleteSema(int semaid);
extern int sceKernelSignalSema(int semaid, int signal);
extern int sceKernelWaitSema(int semaid, int signal, u32 *timeout);
extern int sceKernelTryAllocateFpl(int fplid, void **data);
extern int sceKernelCreateFpl(const char *name, int part, int attr, unsigned int size, unsigned int blocks, void *opt);
extern int sceKernelCreateSema(const char *name, u32 attr, int initVal, int maxVal, void *option);
extern int sceKernelDeleteFpl(int fplid);

extern void *sceKernelMemmove(void *dst, const void *src, u32 n);
extern char *sceKernelInitDiscImage(void);
extern int sceUtilsBufferCopyWithRange(void *outbuf, int outsize, void *inbuf, int insize, int cmd);

typedef struct {
    int type;       /* 0x00 */
    u8 key[16];     /* 0x04..0x13 */
    u8 pad[16];     /* 0x14..0x23 */
    int pad_len;    /* 0x24 */
} SceBBMacKey;

typedef struct {
    int type;       /* 0x00 */
    u32 seed;       /* 0x04 */
    u8 key[16];     /* 0x08..0x17 */
} SceBBCipherKey;

static const u32 g_key_1cd4[4] = { 0x1ded50e3, 0xd01f0a91, 0x3e1cbb29, 0xfb7740f3 };
static const u32 g_key_1ce4[4] = { 0x7ca45f13, 0xa45b39ab, 0xa9ccb876, 0x45043a8f };
static const u32 g_key_1cf4[4] = { 0xa37f8d67, 0xd1a09c2a, 0x38d88a50, 0x7e014b5e };

static u8 g_kirk_buf[0x814];      /* 0x1dc0..0x25d3 */
static void *g_fpl_ptr;           /* 0x1d80 */
static int g_fpl_id;              /* 0x1d84 */
static int g_sema_id;             /* 0x25d4 */

static u8 *g_ptr_1d40 = g_kirk_buf;
static u8 *g_ptr_1d44 = g_kirk_buf + 0x14;
static u8 *g_ptr_1d48 = g_kirk_buf;
static u8 *g_ptr_1d4c = g_kirk_buf;
static u8 *g_ptr_1d50 = g_kirk_buf + 0x14;

static int sub_0000(u32 *buf, int size, int key_idx)
{
    buf[0] = 4;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = key_idx;
    buf[4] = size;
    return (sceUtilsBufferCopyWithRange(buf, size + 0x14, buf, size + 0x14, 4) == 0) ? 0 : (int)0x80510311;
}

static int sub_0054(u32 *buf, int size, int key_idx)
{
    buf[0] = 5;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = key_idx;
    buf[4] = size;
    return (sceUtilsBufferCopyWithRange(buf, size + 0x14, buf, size + 0x14, 7) == 0) ? 0 : (int)0x80510311;
}

static int sub_00a8(u32 *buf, int size)
{
    buf[0] = 4;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0x100;
    buf[4] = size;
    return (sceUtilsBufferCopyWithRange(buf, size + 0x14, buf, size + 0x14, 5) == 0) ? 0 : (int)0x80510312;
}

static int sub_0100(u32 *buf, int size)
{
    buf[0] = 5;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0x100;
    buf[4] = size;
    return (sceUtilsBufferCopyWithRange(buf, size + 0x14, buf, size + 0x14, 8) == 0) ? 0 : (int)0x80510312;
}

static int sub_0158(u8 *buf, int size, u8 *key, int key_idx)
{
    for (u32 i = 0; i < 0x10; i++)
        buf[0x14 + i] ^= key[i];
    int ret = sub_0000((u32 *)buf, size, key_idx);
    if (ret != 0)
        return ret;
    u8 *tail = buf + size;
    for (u32 i = 0; i < 0x10; i++)
        key[i] = tail[4 + i];
    return 0;
}

static int sub_01f8(u8 *buf, int size, u8 *key, int key_idx)
{
    u8 tmp[16];
    u8 *tail = buf + size;
    for (u32 i = 0; i < 0x10; i++)
        tmp[i] = tail[4 + i];
    int ret = sub_0054((u32 *)buf, size, key_idx);
    if (ret != 0)
        return ret;
    for (u32 i = 0; i < 0x10; i++)
        buf[i] ^= key[i];
    for (u32 i = 0; i < 0x10; i++)
        key[i] = tmp[i];
    return 0;
}

int sub_02a8(u8 *buf, int size, u8 *key)
{
    for (u32 i = 0; i < 0x10; i++)
        buf[0x14 + i] ^= key[i];
    int ret = sub_00a8((u32 *)buf, size);
    if (ret != 0)
        return ret;
    u8 *tail = buf + size;
    for (u32 i = 0; i < 0x10; i++)
        key[i] = tail[4 + i];
    return 0;
}

int sub_0344(u8 *buf, int size, u8 *key)
{
    u8 tmp[16];
    u8 *tail = buf + size;
    for (u32 i = 0; i < 0x10; i++)
        tmp[i] = tail[4 + i];
    int ret = sub_0100((u32 *)buf, size);
    if (ret != 0)
        return ret;
    for (u32 i = 0; i < 0x10; i++)
        buf[i] ^= key[i];
    for (u32 i = 0; i < 0x10; i++)
        key[i] = tmp[i];
    return 0;
}

static int sub_03f0(void *buf)
{
    return (sceUtilsBufferCopyWithRange(buf, 0x14, NULL, 0, 0x0e) == 0) ? 0 : (int)0x80510315;
}

static int sub_0428(u8 *kbuf, u8 *data, u32 size, const u8 *key, u32 *seed, int type)
{
    u8 iv[16];
    u8 blk_hdr[16];

    for (u32 i = 0; i < 0x10; i++)
        kbuf[0x14 + i] = key[i];

    const u8 *k2 = (const u8 *)g_key_1cf4;
    for (u32 i = 0; i < 0x10; i++)
        kbuf[0x14 + i] ^= k2[i];

    int ret = (type == 2) ? sub_0100((u32 *)kbuf, 0x10) : sub_0054((u32 *)kbuf, 0x10, 0x39);

    const u8 *k1 = (const u8 *)g_key_1ce4;
    for (u32 i = 0; i < 0x10; i++)
        kbuf[i] ^= k1[i];

    if (ret != 0)
        return ret;

    for (u32 i = 0; i < 0x10; i++)
        blk_hdr[i] = kbuf[i];

    u32 cur_seed = *seed;
    if (cur_seed == 1) {
        for (u32 i = 0; i < 0x10; i++)
            iv[i] = 0;
    } else {
        for (u32 i = 0; i < 0x0c; i++)
            iv[i] = blk_hdr[i];
        u32 prev = cur_seed - 1;
        iv[0x0c] = (u8)((u8)cur_seed - 1);
        iv[0x0d] = (u8)(prev >> 8);
        iv[0x0e] = (u8)(prev >> 16);
        iv[0x0f] = (u8)(prev >> 24);
    }

    for (u32 off = 0x14; off < size + 0x14; off += 0x10) {
        u8 *dst = kbuf + off;
        for (u32 i = 0; i < 0x0c; i++)
            dst[i] = blk_hdr[i];
        dst[0x0c] = *(u8 *)seed;
        dst[0x0d] = (u8)(*seed >> 8);
        dst[0x0e] = (u8)(*(u16 *)((u8 *)seed + 2));
        dst[0x0f] = *((u8 *)seed + 3);
        (*seed)++;
    }

    ret = sub_01f8(kbuf, size, iv, 0x63);
    if (ret != 0)
        return ret;

    for (u32 i = 0; i < size; i++)
        data[i] ^= kbuf[i];
    return 0;
}

static int sub_06dc(u8 *kbuf, u32 *data, u32 size, const u8 *key, u32 *seed, int type)
{
    u32 iv[4];
    u32 blk_hdr[4];
    u8 *iv8 = (u8 *)iv;
    u8 *hdr8 = (u8 *)blk_hdr;

    for (u32 i = 0; i < 0x10; i++)
        kbuf[0x14 + i] = key[i];

    const u8 *k2 = (const u8 *)g_key_1cf4;
    for (u32 i = 0; i < 0x10; i++)
        kbuf[0x14 + i] ^= k2[i];

    int ret = (type == 2) ? sub_0100((u32 *)kbuf, 0x10) : sub_0054((u32 *)kbuf, 0x10, 0x39);

    const u8 *k1 = (const u8 *)g_key_1ce4;
    for (u32 i = 0; i < 0x10; i++)
        kbuf[i] ^= k1[i];

    if (ret != 0)
        return ret;

    for (u32 i = 0; i < 0x10; i++)
        hdr8[i] = kbuf[i];

    u32 cur_seed = *seed;
    if (cur_seed == 1) {
        for (u32 i = 0; i < 0x10; i++)
            iv8[i] = 0;
    } else {
        for (u32 i = 0; i < 0x0c; i++)
            iv8[i] = hdr8[i];
        iv[3] = cur_seed - 1;
    }

    for (u32 off = 0x14; off < size + 0x14; off += 0x10) {
        u32 *dst = (u32 *)(kbuf + off);
        dst[0] = blk_hdr[0];
        dst[1] = blk_hdr[1];
        dst[2] = blk_hdr[2];
        dst[3] = *seed;
        (*seed)++;
    }

    ret = sub_01f8(kbuf, size, iv8, 0x63);
    if (ret != 0)
        return ret;

    u32 words = size >> 2;
    u32 *k32 = (u32 *)kbuf;
    for (u32 i = 0; i < words; i += 4) {
        data[i + 0] ^= k32[i + 0];
        data[i + 1] ^= k32[i + 1];
        data[i + 2] ^= k32[i + 2];
        data[i + 3] ^= k32[i + 3];
    }
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    int semaid = sceKernelCreateSema("SceAmctrl", 0, 1, 1, NULL);
    if (semaid <= 0)
        return 1;
    g_sema_id = semaid;

    if (sceKernelInitDiscImage() == NULL)
        return 0;

    int fplid = sceKernelCreateFpl("SceAmctrl", 1, 0, 0x8014, 1, NULL);
    if (fplid > 0) {
        g_fpl_id = fplid;
        if (sceKernelTryAllocateFpl(fplid, &g_fpl_ptr) == 0)
            return 0;
        g_fpl_ptr = NULL;
        sceKernelDeleteFpl(g_fpl_id);
    }
    sceKernelDeleteSema(g_sema_id);
    return 1;
}

int sceDrmBBMacInit(SceBBMacKey *mkey, int type)
{
    mkey->type = type;
    for (u32 i = 0; i < 0x10; i++)
        mkey->key[i] = 0;
    for (u32 i = 0; i < 0x10; i++)
        mkey->pad[i] = 0;
    mkey->pad_len = 0;
    return 0;
}

int sceDrmBBMacUpdate(SceBBMacKey *mkey, const u8 *buf, u32 size)
{
    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return 0x80000041;

    int ret = 0;
    u32 pad_len = (u32)mkey->pad_len;
    if (pad_len > 0x10) {
        ret = 0x80510302;
        goto out;
    }

    if (pad_len + size <= 0x10) {
        for (u32 i = 0; i < size; i++)
            mkey->pad[mkey->pad_len + i] = buf[i];
        mkey->pad_len += size;
        goto out;
    }

    int key_idx = (mkey->type == 2) ? 0x3a : 0x38;
    for (u32 i = 0; i < (u32)mkey->pad_len; i++)
        g_ptr_1d44[i] = mkey->pad[i];

    u32 cur_len = (u32)mkey->pad_len;
    u32 rem = (cur_len + size) & 0x0f;
    if (rem == 0)
        rem = 0x10;
    mkey->pad_len = rem;
    u32 body = size - rem;

    for (u32 i = 0; i < rem; i++)
        mkey->pad[i] = buf[body + i];

    for (u32 i = 0; i < body; i++) {
        if (cur_len == 0x800) {
            ret = sub_0158(g_kirk_buf, 0x800, mkey->key, key_idx);
            if (ret != 0)
                goto out;
            cur_len = 0;
        }
        g_ptr_1d44[cur_len++] = buf[i];
    }

    if (cur_len != 0)
        ret = sub_0158(g_kirk_buf, cur_len, mkey->key, key_idx);

out:
    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = 0x80000041;
    return ret;
}

int sceDrmBBMacFinal(SceBBMacKey *mkey, u8 *out, const u8 *vkey)
{
    u8 subkey[16];
    u8 tmp_key[16];

    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return 0x80000041;

    int ret = 0;
    if ((u32)mkey->pad_len > 0x10) {
        ret = 0x80510302;
        goto out;
    }

    int key_idx = (mkey->type == 2) ? 0x3a : 0x38;
    for (u32 i = 0; i < 0x10; i++)
        g_ptr_1d44[i] = 0;

    ret = sub_0000((u32 *)g_kirk_buf, 0x10, key_idx);
    if (ret != 0)
        goto out;

    for (u32 i = 0; i < 0x10; i++)
        subkey[i] = g_ptr_1d44[i];

    u8 xor_b = ((s8)subkey[0] < 0) ? 0x87 : 0;
    for (u32 i = 0; i < 0x0f; i++)
        subkey[i] = (u8)((subkey[i] << 1) | (subkey[i + 1] >> 7));
    subkey[0x0f] = (u8)((subkey[0x0f] << 1) ^ xor_b);

    u32 plen = (u32)mkey->pad_len;
    if (plen != 0x10) {
        xor_b = ((s8)subkey[0] < 0) ? 0x87 : 0;
        for (u32 i = 0; i < 0x0f; i++)
            subkey[i] = (u8)((subkey[i] << 1) | (subkey[i + 1] >> 7));
        subkey[0x0f] = (u8)((subkey[0x0f] << 1) ^ xor_b);

        if (plen < 0x10) {
            mkey->pad[plen] = 0x80;
            for (u32 i = plen + 1; i < 0x10; i++)
                mkey->pad[i] = 0;
        }
    }

    for (u32 i = 0; i < 0x10; i++)
        mkey->pad[i] ^= subkey[i];
    for (u32 i = 0; i < 0x10; i++)
        g_ptr_1d44[i] = mkey->pad[i];
    for (u32 i = 0; i < 0x10; i++)
        tmp_key[i] = mkey->key[i];

    ret = sub_0158(g_kirk_buf, 0x10, tmp_key, key_idx);
    if (ret != 0)
        goto out;

    const u8 *k0 = (const u8 *)g_key_1cd4;
    for (u32 i = 0; i < 0x10; i++)
        tmp_key[i] ^= k0[i];

    if (mkey->type == 2) {
        for (u32 i = 0; i < 0x10; i++)
            g_ptr_1d44[i] = tmp_key[i];
        ret = sub_00a8((u32 *)g_kirk_buf, 0x10);
        if (ret != 0)
            goto out;
        ret = sub_0000((u32 *)g_kirk_buf, 0x10, key_idx);
        if (ret != 0)
            goto out;
        for (u32 i = 0; i < 0x10; i++)
            tmp_key[i] = g_ptr_1d44[i];
    }

    if (vkey) {
        for (u32 i = 0; i < 0x10; i++)
            tmp_key[i] ^= vkey[i];
        for (u32 i = 0; i < 0x10; i++)
            g_ptr_1d44[i] = tmp_key[i];
        ret = sub_0000((u32 *)g_kirk_buf, 0x10, key_idx);
        if (ret != 0)
            goto out;
        for (u32 i = 0; i < 0x10; i++)
            tmp_key[i] = g_ptr_1d44[i];
    }

    for (u32 i = 0; i < 0x10; i++)
        out[i] = tmp_key[i];
    for (u32 i = 0; i < 0x10; i++)
        mkey->key[i] = 0;
    for (u32 i = 0; i < 0x10; i++)
        mkey->pad[i] = 0;
    mkey->pad_len = 0;
    mkey->type = 0;
    ret = 0;

out:
    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = 0x80000041;
    return ret;
}

int sceDrmBBMacFinal2(SceBBMacKey *mkey, const u8 *mac, const u8 *vkey)
{
    u8 calc_mac[16];
    int type = mkey->type;
    int ret = sceDrmBBMacFinal(mkey, calc_mac, vkey);
    if (ret != 0)
        return ret;

    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return 0x80000041;

    if (type == 3) {
        for (u32 i = 0; i < 0x10; i++)
            g_ptr_1d44[i] = mac[i];
        ret = sub_0054((u32 *)g_kirk_buf, 0x10, 0x63);
        if (ret != 0)
            goto out;
    } else {
        for (u32 i = 0; i < 0x10; i++)
            g_ptr_1d40[i] = mac[i];
    }

    int diff = 0;
    for (int i = 0; i < 0x10; i++) {
        if ((s8)calc_mac[i] != (s8)g_ptr_1d40[i]) {
            diff = (s8)calc_mac[i] - (s8)g_ptr_1d40[i];
            break;
        }
    }
    ret = (diff == 0) ? 0 : (int)0x80510300;

out:
    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = 0x80000041;
    return ret;
}

int sceAmctrl_driver_9227EA79(SceBBMacKey *mkey, const u8 *buf, u32 size)
{
    u8 *kbuf = g_kirk_buf;
    u8 *kdata = g_ptr_1d44;
    u32 max_chunk = 0x800;

    if (g_fpl_ptr) {
        kbuf = (u8 *)g_fpl_ptr;
        kdata = kbuf + 0x14;
        max_chunk = 0x8000;
    }

    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return 0x80000041;

    int ret = 0;
    u32 pad_len = (u32)mkey->pad_len;
    if (pad_len > 0x10) {
        ret = 0x80510302;
        goto out;
    }

    if (pad_len + size <= 0x10) {
        for (u32 i = 0; i < size; i++)
            mkey->pad[mkey->pad_len + i] = buf[i];
        mkey->pad_len += size;
        goto out;
    }

    int key_idx = (mkey->type == 2) ? 0x3a : 0x38;
    for (u32 i = 0; i < (u32)mkey->pad_len; i++)
        kdata[i] = mkey->pad[i];

    u32 cur_len = (u32)mkey->pad_len;
    u32 rem = (cur_len + size) & 0x0f;
    if (rem == 0)
        rem = 0x10;
    mkey->pad_len = rem;
    u32 body = size - rem;

    for (u32 i = 0; i < rem; i++)
        mkey->pad[i] = buf[body + i];

    u32 pos = 0;
    if (body + cur_len > max_chunk) {
        pos = max_chunk - cur_len;
        sceKernelMemmove(kdata + cur_len, buf, pos);
        ret = sub_0158(kbuf, max_chunk, mkey->key, key_idx);
        if (ret != 0)
            goto out;
        cur_len = 0;
        while (pos + max_chunk < body) {
            sceKernelMemmove(kdata, buf + pos, max_chunk);
            ret = sub_0158(kbuf, max_chunk, mkey->key, key_idx);
            if (ret != 0)
                goto out;
            pos += max_chunk;
        }
    }

    sceKernelMemmove(kdata + cur_len, buf + pos, body - pos);
    ret = sub_0158(kbuf, (body + cur_len) - pos, mkey->key, key_idx);

out:
    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = 0x80000041;
    return ret;
}

int sceDrmBBCipherInit(SceBBCipherKey *ckey, int type, int mode, u8 *header_key, const u8 *version_key, u32 seed)
{
    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return 0x80000041;

    int ret = 0;
    ckey->type = type;
    if (mode == 2) {
        ckey->seed = seed + 1;
        for (u32 i = 0; i < 0x10; i++)
            ckey->key[i] = header_key[i];
        if (version_key) {
            for (int i = 0; i < 0x10; i++)
                ckey->key[i] ^= version_key[i];
        }
    } else if (mode == 1) {
        ckey->seed = 1;
        ret = sub_03f0(g_ptr_1d4c);
        if (ret != 0)
            goto out;
        for (int i = 0x0f; i >= 0; i--)
            g_ptr_1d4c[0x14 + i] = g_ptr_1d4c[i];
        for (u32 i = 0; i < 4; i++)
            g_ptr_1d4c[0x20 + i] = 0;

        const u8 *k1 = (const u8 *)g_key_1ce4;
        const u8 *k2 = (const u8 *)g_key_1cf4;
        for (int i = 0; i < 0x10; i++)
            g_ptr_1d50[i] ^= k1[i];
        if (ckey->type == 2)
            ret = sub_00a8((u32 *)g_ptr_1d48, 0x10);
        else
            ret = sub_0000((u32 *)g_ptr_1d48, 0x10, 0x39);
        for (int i = 0; i < 0x10; i++)
            g_ptr_1d50[i] ^= k2[i];
        if (ret != 0)
            goto out;

        for (u32 i = 0; i < 0x10; i++)
            ckey->key[i] = g_ptr_1d50[i];
        for (u32 i = 0; i < 0x10; i++)
            header_key[i] = g_ptr_1d50[i];
        if (version_key) {
            for (int i = 0; i < 0x10; i++)
                ckey->key[i] ^= version_key[i];
        }
    }

out:
    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = 0x80000041;
    return ret;
}

int sceDrmBBCipherUpdate(SceBBCipherKey *ckey, u8 *data, u32 size)
{
    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return 0x80000041;

    int ret = 0;
    if (size == 0)
        goto out;
    if (size & 0x0f) {
        ret = 0x80510301;
        goto out;
    }

    u32 off = 0;
    while (size >= 0x800) {
        ret = sub_0428(g_ptr_1d48, data + off, 0x800, ckey->key, &ckey->seed, ckey->type);
        size -= 0x800;
        off += 0x800;
        if (ret != 0)
            goto out;
    }
    if (size != 0)
        ret = sub_0428(g_ptr_1d48, data + off, size, ckey->key, &ckey->seed, ckey->type);

out:
    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = 0x80000041;
    return ret;
}

int sceDrmBBCipherFinal(SceBBCipherKey *ckey)
{
    for (u32 i = 0; i < 0x10; i++)
        ckey->key[i] = 0;
    ckey->seed = 0;
    ckey->type = 0;
    return 0;
}

int sceAmctrl_driver_E04ADD4C(SceBBCipherKey *ckey, u8 *data, u32 size)
{
    u8 *kbuf = g_ptr_1d48;
    u32 max_chunk = 0x800;
    if (g_fpl_ptr) {
        kbuf = (u8 *)g_fpl_ptr;
        max_chunk = 0x8000;
    }

    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return 0x80000041;

    int ret = 0;
    if (size == 0)
        goto out;
    if (size & 0x0f) {
        ret = 0x80510301;
        goto out;
    }

    u32 off = 0;
    while (size >= max_chunk) {
        ret = sub_06dc(kbuf, (u32 *)(data + off), max_chunk, ckey->key, &ckey->seed, ckey->type);
        size -= max_chunk;
        off += max_chunk;
        if (ret != 0)
            goto out;
    }
    if (size != 0)
        ret = sub_06dc(kbuf, (u32 *)(data + off), size, ckey->key, &ckey->seed, ckey->type);

out:
    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = 0x80000041;
    return ret;
}
