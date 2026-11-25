#include <common_imp.h>

#define SCE_AMCTRL_ERROR_MAC_INVALID        ((int)0x80510300)
#define SCE_AMCTRL_ERROR_INVALID_SIZE       ((int)0x80510301)
#define SCE_AMCTRL_ERROR_INVALID_PAD_LEN    ((int)0x80510302)
#define SCE_AMCTRL_ERROR_KIRK_AES_KEY       ((int)0x80510311)
#define SCE_AMCTRL_ERROR_KIRK_AES_FUSE      ((int)0x80510312)
#define SCE_AMCTRL_ERROR_KIRK_PRNG          ((int)0x80510315)
#define SCE_AMCTRL_ERROR_SEMA               ((int)0x80000041)

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

static const u32 g_bbmac_final_xor_key[4] = { 0x1ded50e3, 0xd01f0a91, 0x3e1cbb29, 0xfb7740f3 };
static const u32 g_bbcipher_hdr_xor_key[4] = { 0x7ca45f13, 0xa45b39ab, 0xa9ccb876, 0x45043a8f };
static const u32 g_bbcipher_key_xor_key[4] = { 0xa37f8d67, 0xd1a09c2a, 0x38d88a50, 0x7e014b5e };

static u8 g_kirk_buf[0x814];
static void *g_fpl_ptr;
static int g_fpl_id;
static int g_sema_id;

static u8 *g_mac_kirk_buf = g_kirk_buf;
static u8 *g_mac_kirk_data = g_kirk_buf + 0x14;
static u8 *g_cipher_kirk_buf = g_kirk_buf;
static u8 *g_prng_kirk_buf = g_kirk_buf;
static u8 *g_cipher_kirk_data = g_kirk_buf + 0x14;

static int kirkEncryptAesCbc(u32 *buf, int size, int key_idx)
{
    buf[0] = 4;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = key_idx;
    buf[4] = size;
    return (sceUtilsBufferCopyWithRange(buf, size + 0x14, buf, size + 0x14, 4) == 0) ? 0 : SCE_AMCTRL_ERROR_KIRK_AES_KEY;
}

static int kirkDecryptAesCbc(u32 *buf, int size, int key_idx)
{
    buf[0] = 5;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = key_idx;
    buf[4] = size;
    return (sceUtilsBufferCopyWithRange(buf, size + 0x14, buf, size + 0x14, 7) == 0) ? 0 : SCE_AMCTRL_ERROR_KIRK_AES_KEY;
}

static int kirkEncryptAesCbcFuse(u32 *buf, int size)
{
    buf[0] = 4;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0x100;
    buf[4] = size;
    return (sceUtilsBufferCopyWithRange(buf, size + 0x14, buf, size + 0x14, 5) == 0) ? 0 : SCE_AMCTRL_ERROR_KIRK_AES_FUSE;
}

static int kirkDecryptAesCbcFuse(u32 *buf, int size)
{
    buf[0] = 5;
    buf[1] = 0;
    buf[2] = 0;
    buf[3] = 0x100;
    buf[4] = size;
    return (sceUtilsBufferCopyWithRange(buf, size + 0x14, buf, size + 0x14, 8) == 0) ? 0 : SCE_AMCTRL_ERROR_KIRK_AES_FUSE;
}

static int encryptCbcChain(u8 *buf, int size, u8 *key, int key_idx)
{
    for (u32 i = 0; i < 0x10; i++)
        buf[0x14 + i] ^= key[i];
    int ret = kirkEncryptAesCbc((u32 *)buf, size, key_idx);
    if (ret != 0)
        return ret;
    u8 *tail = buf + size;
    for (u32 i = 0; i < 0x10; i++)
        key[i] = tail[4 + i];
    return 0;
}

static int decryptCbcChain(u8 *buf, int size, u8 *key, int key_idx)
{
    u8 saved_iv[16];
    u8 *tail = buf + size;
    for (u32 i = 0; i < 0x10; i++)
        saved_iv[i] = tail[4 + i];
    int ret = kirkDecryptAesCbc((u32 *)buf, size, key_idx);
    if (ret != 0)
        return ret;
    for (u32 i = 0; i < 0x10; i++)
        buf[i] ^= key[i];
    for (u32 i = 0; i < 0x10; i++)
        key[i] = saved_iv[i];
    return 0;
}

int encryptCbcChainFuse(u8 *buf, int size, u8 *key)
{
    for (u32 i = 0; i < 0x10; i++)
        buf[0x14 + i] ^= key[i];
    int ret = kirkEncryptAesCbcFuse((u32 *)buf, size);
    if (ret != 0)
        return ret;
    u8 *tail = buf + size;
    for (u32 i = 0; i < 0x10; i++)
        key[i] = tail[4 + i];
    return 0;
}

int decryptCbcChainFuse(u8 *buf, int size, u8 *key)
{
    u8 saved_iv[16];
    u8 *tail = buf + size;
    for (u32 i = 0; i < 0x10; i++)
        saved_iv[i] = tail[4 + i];
    int ret = kirkDecryptAesCbcFuse((u32 *)buf, size);
    if (ret != 0)
        return ret;
    for (u32 i = 0; i < 0x10; i++)
        buf[i] ^= key[i];
    for (u32 i = 0; i < 0x10; i++)
        key[i] = saved_iv[i];
    return 0;
}

static int kirkGenPrng(void *buf)
{
    return (sceUtilsBufferCopyWithRange(buf, 0x14, NULL, 0, 0x0e) == 0) ? 0 : SCE_AMCTRL_ERROR_KIRK_PRNG;
}

static int bbCipherCryptUnaligned(u8 *kbuf, u8 *data, u32 size, const u8 *key, u32 *seed, int type)
{
    u8 iv[16];
    u8 blk_hdr[16];

    for (u32 i = 0; i < 0x10; i++)
        kbuf[0x14 + i] = key[i];

    const u8 *key_xor = (const u8 *)g_bbcipher_key_xor_key;
    for (u32 i = 0; i < 0x10; i++)
        kbuf[0x14 + i] ^= key_xor[i];

    int ret = (type == 2) ? kirkDecryptAesCbcFuse((u32 *)kbuf, 0x10) : kirkDecryptAesCbc((u32 *)kbuf, 0x10, 0x39);

    const u8 *hdr_xor = (const u8 *)g_bbcipher_hdr_xor_key;
    for (u32 i = 0; i < 0x10; i++)
        kbuf[i] ^= hdr_xor[i];

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

    for (u32 pos = 0x14; pos < size + 0x14; pos += 0x10) {
        u8 *dst = kbuf + pos;
        for (u32 i = 0; i < 0x0c; i++)
            dst[i] = blk_hdr[i];
        u32 blk_seed = *seed;
        dst[0x0c] = (u8)blk_seed;
        dst[0x0d] = (u8)(blk_seed >> 8);
        dst[0x0e] = (u8)(blk_seed >> 16);
        dst[0x0f] = (u8)(blk_seed >> 24);
        *seed = blk_seed + 1;
    }

    ret = decryptCbcChain(kbuf, size, iv, 0x63);
    if (ret != 0)
        return ret;

    for (u32 i = 0; i < size; i++)
        data[i] ^= kbuf[i];
    return 0;
}

static int bbCipherCryptAligned(u8 *kbuf, u32 *data, u32 size, const u8 *key, u32 *seed, int type)
{
    u32 iv[4];
    u32 blk_hdr[4];
    u8 *iv_bytes = (u8 *)iv;
    u8 *hdr_bytes = (u8 *)blk_hdr;

    for (u32 i = 0; i < 0x10; i++)
        kbuf[0x14 + i] = key[i];

    const u8 *key_xor = (const u8 *)g_bbcipher_key_xor_key;
    for (u32 i = 0; i < 0x10; i++)
        kbuf[0x14 + i] ^= key_xor[i];

    int ret = (type == 2) ? kirkDecryptAesCbcFuse((u32 *)kbuf, 0x10) : kirkDecryptAesCbc((u32 *)kbuf, 0x10, 0x39);

    const u8 *hdr_xor = (const u8 *)g_bbcipher_hdr_xor_key;
    for (u32 i = 0; i < 0x10; i++)
        kbuf[i] ^= hdr_xor[i];

    if (ret != 0)
        return ret;

    for (u32 i = 0; i < 0x10; i++)
        hdr_bytes[i] = kbuf[i];

    u32 cur_seed = *seed;
    if (cur_seed == 1) {
        for (u32 i = 0; i < 0x10; i++)
            iv_bytes[i] = 0;
    } else {
        for (u32 i = 0; i < 0x0c; i++)
            iv_bytes[i] = hdr_bytes[i];
        iv[3] = cur_seed - 1;
    }

    for (u32 pos = 0x14; pos < size + 0x14; pos += 0x10) {
        u32 *dst = (u32 *)(kbuf + pos);
        dst[0] = blk_hdr[0];
        dst[1] = blk_hdr[1];
        dst[2] = blk_hdr[2];
        dst[3] = *seed;
        (*seed)++;
    }

    ret = decryptCbcChain(kbuf, size, iv_bytes, 0x63);
    if (ret != 0)
        return ret;

    u32 words = size >> 2;
    u32 *kirk_words = (u32 *)kbuf;
    for (u32 i = 0; i < words; i += 4) {
        data[i + 0] ^= kirk_words[i + 0];
        data[i + 1] ^= kirk_words[i + 1];
        data[i + 2] ^= kirk_words[i + 2];
        data[i + 3] ^= kirk_words[i + 3];
    }
    return 0;
}

int module_start(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    g_sema_id = sceKernelCreateSema("SceAmctrl", 0, 1, 1, NULL);
    if (g_sema_id <= 0)
        return 1;

    if (sceKernelInitDiscImage() == NULL)
        return 0;

    g_fpl_id = sceKernelCreateFpl("SceAmctrl", 1, 0, 0x8014, 1, NULL);
    if (g_fpl_id > 0) {
        if (sceKernelTryAllocateFpl(g_fpl_id, &g_fpl_ptr) == 0)
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
        return SCE_AMCTRL_ERROR_SEMA;

    int ret = 0;
    u32 pad_len = (u32)mkey->pad_len;
    if (pad_len > 0x10) {
        ret = SCE_AMCTRL_ERROR_INVALID_PAD_LEN;
    } else if (pad_len + size <= 0x10) {
        for (u32 i = 0; i < size; i++)
            mkey->pad[mkey->pad_len + i] = buf[i];
        mkey->pad_len += size;
    } else {
        int key_idx = (mkey->type == 2) ? 0x3a : 0x38;
        for (u32 i = 0; i < (u32)mkey->pad_len; i++)
            g_mac_kirk_data[i] = mkey->pad[i];

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
                ret = encryptCbcChain(g_kirk_buf, 0x800, mkey->key, key_idx);
                if (ret != 0)
                    break;
                cur_len = 0;
            }
            g_mac_kirk_data[cur_len++] = buf[i];
        }

        if (ret == 0 && cur_len != 0)
            ret = encryptCbcChain(g_kirk_buf, cur_len, mkey->key, key_idx);
    }

    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = SCE_AMCTRL_ERROR_SEMA;
    return ret;
}

int sceDrmBBMacFinal(SceBBMacKey *mkey, u8 *out, const u8 *vkey)
{
    u8 subkey[16];
    u8 mac_key[16];

    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return SCE_AMCTRL_ERROR_SEMA;

    int ret = 0;
    do {
        if ((u32)mkey->pad_len > 0x10) {
            ret = SCE_AMCTRL_ERROR_INVALID_PAD_LEN;
            break;
        }

        int key_idx = (mkey->type == 2) ? 0x3a : 0x38;
        for (u32 i = 0; i < 0x10; i++)
            g_mac_kirk_data[i] = 0;

        ret = kirkEncryptAesCbc((u32 *)g_kirk_buf, 0x10, key_idx);
        if (ret != 0)
            break;

        for (u32 i = 0; i < 0x10; i++)
            subkey[i] = g_mac_kirk_data[i];

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
            g_mac_kirk_data[i] = mkey->pad[i];
        for (u32 i = 0; i < 0x10; i++)
            mac_key[i] = mkey->key[i];

        ret = encryptCbcChain(g_kirk_buf, 0x10, mac_key, key_idx);
        if (ret != 0)
            break;

        const u8 *final_xor = (const u8 *)g_bbmac_final_xor_key;
        for (u32 i = 0; i < 0x10; i++)
            mac_key[i] ^= final_xor[i];

        if (mkey->type == 2) {
            for (u32 i = 0; i < 0x10; i++)
                g_mac_kirk_data[i] = mac_key[i];
            ret = kirkEncryptAesCbcFuse((u32 *)g_kirk_buf, 0x10);
            if (ret != 0)
                break;
            ret = kirkEncryptAesCbc((u32 *)g_kirk_buf, 0x10, key_idx);
            if (ret != 0)
                break;
            for (u32 i = 0; i < 0x10; i++)
                mac_key[i] = g_mac_kirk_data[i];
        }

        if (vkey) {
            for (u32 i = 0; i < 0x10; i++)
                mac_key[i] ^= vkey[i];
            for (u32 i = 0; i < 0x10; i++)
                g_mac_kirk_data[i] = mac_key[i];
            ret = kirkEncryptAesCbc((u32 *)g_kirk_buf, 0x10, key_idx);
            if (ret != 0)
                break;
            for (u32 i = 0; i < 0x10; i++)
                mac_key[i] = g_mac_kirk_data[i];
        }

        for (u32 i = 0; i < 0x10; i++)
            out[i] = mac_key[i];
        for (u32 i = 0; i < 0x10; i++)
            mkey->key[i] = 0;
        for (u32 i = 0; i < 0x10; i++)
            mkey->pad[i] = 0;
        mkey->pad_len = 0;
        mkey->type = 0;
        ret = 0;
    } while (0);

    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = SCE_AMCTRL_ERROR_SEMA;
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
        return SCE_AMCTRL_ERROR_SEMA;

    if (type == 3) {
        for (u32 i = 0; i < 0x10; i++)
            g_mac_kirk_data[i] = mac[i];
        ret = kirkDecryptAesCbc((u32 *)g_kirk_buf, 0x10, 0x63);
    } else {
        for (u32 i = 0; i < 0x10; i++)
            g_mac_kirk_buf[i] = mac[i];
        ret = 0;
    }

    if (ret == 0) {
        int diff = 0;
        for (int i = 0; i < 0x10; i++) {
            if ((s8)calc_mac[i] != (s8)g_mac_kirk_buf[i]) {
                diff = (s8)calc_mac[i] - (s8)g_mac_kirk_buf[i];
                break;
            }
        }
        ret = (diff == 0) ? 0 : SCE_AMCTRL_ERROR_MAC_INVALID;
    }

    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = SCE_AMCTRL_ERROR_SEMA;
    return ret;
}

int sceAmctrl_driver_9227EA79(SceBBMacKey *mkey, const u8 *buf, u32 size)
{
    u8 *kbuf = g_kirk_buf;
    u8 *kdata = g_mac_kirk_data;
    u32 max_chunk = 0x800;

    if (g_fpl_ptr) {
        kbuf = (u8 *)g_fpl_ptr;
        kdata = kbuf + 0x14;
        max_chunk = 0x8000;
    }

    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return SCE_AMCTRL_ERROR_SEMA;

    int ret = 0;
    u32 pad_len = (u32)mkey->pad_len;
    if (pad_len > 0x10) {
        ret = SCE_AMCTRL_ERROR_INVALID_PAD_LEN;
    } else if (pad_len + size <= 0x10) {
        for (u32 i = 0; i < size; i++)
            mkey->pad[mkey->pad_len + i] = buf[i];
        mkey->pad_len += size;
    } else {
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
            ret = encryptCbcChain(kbuf, max_chunk, mkey->key, key_idx);
            if (ret == 0) {
                cur_len = 0;
                while (pos + max_chunk < body) {
                    sceKernelMemmove(kdata, buf + pos, max_chunk);
                    ret = encryptCbcChain(kbuf, max_chunk, mkey->key, key_idx);
                    if (ret != 0)
                        break;
                    pos += max_chunk;
                }
            }
        }

        if (ret == 0) {
            sceKernelMemmove(kdata + cur_len, buf + pos, body - pos);
            ret = encryptCbcChain(kbuf, (body + cur_len) - pos, mkey->key, key_idx);
        }
    }

    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = SCE_AMCTRL_ERROR_SEMA;
    return ret;
}

int sceDrmBBCipherInit(SceBBCipherKey *ckey, int type, int mode, u8 *header_key, const u8 *version_key, u32 seed)
{
    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return SCE_AMCTRL_ERROR_SEMA;

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
        ret = kirkGenPrng(g_prng_kirk_buf);
        if (ret == 0) {
            for (int i = 0x0f; i >= 0; i--)
                g_prng_kirk_buf[0x14 + i] = g_prng_kirk_buf[i];
            for (u32 i = 0; i < 4; i++)
                g_prng_kirk_buf[0x20 + i] = 0;

            const u8 *hdr_xor = (const u8 *)g_bbcipher_hdr_xor_key;
            const u8 *key_xor = (const u8 *)g_bbcipher_key_xor_key;
            for (int i = 0; i < 0x10; i++)
                g_cipher_kirk_data[i] ^= hdr_xor[i];
            if (ckey->type == 2)
                ret = kirkEncryptAesCbcFuse((u32 *)g_cipher_kirk_buf, 0x10);
            else
                ret = kirkEncryptAesCbc((u32 *)g_cipher_kirk_buf, 0x10, 0x39);
            for (int i = 0; i < 0x10; i++)
                g_cipher_kirk_data[i] ^= key_xor[i];
            if (ret == 0) {
                for (u32 i = 0; i < 0x10; i++)
                    ckey->key[i] = g_cipher_kirk_data[i];
                for (u32 i = 0; i < 0x10; i++)
                    header_key[i] = g_cipher_kirk_data[i];
                if (version_key) {
                    for (int i = 0; i < 0x10; i++)
                        ckey->key[i] ^= version_key[i];
                }
            }
        }
    }

    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = SCE_AMCTRL_ERROR_SEMA;
    return ret;
}

int sceDrmBBCipherUpdate(SceBBCipherKey *ckey, u8 *data, u32 size)
{
    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return SCE_AMCTRL_ERROR_SEMA;

    int ret = 0;
    if (size != 0) {
        if (size & 0x0f) {
            ret = SCE_AMCTRL_ERROR_INVALID_SIZE;
        } else {
            u32 pos = 0;
            while (size >= 0x800) {
                ret = bbCipherCryptUnaligned(g_cipher_kirk_buf, data + pos, 0x800, ckey->key, &ckey->seed, ckey->type);
                size -= 0x800;
                pos += 0x800;
                if (ret != 0)
                    break;
            }
            if (ret == 0 && size != 0)
                ret = bbCipherCryptUnaligned(g_cipher_kirk_buf, data + pos, size, ckey->key, &ckey->seed, ckey->type);
        }
    }

    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = SCE_AMCTRL_ERROR_SEMA;
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
    u8 *kbuf = g_cipher_kirk_buf;
    u32 max_chunk = 0x800;
    if (g_fpl_ptr) {
        kbuf = (u8 *)g_fpl_ptr;
        max_chunk = 0x8000;
    }

    if (sceKernelWaitSema(g_sema_id, 1, NULL) != 0)
        return SCE_AMCTRL_ERROR_SEMA;

    int ret = 0;
    if (size != 0) {
        if (size & 0x0f) {
            ret = SCE_AMCTRL_ERROR_INVALID_SIZE;
        } else {
            u32 pos = 0;
            while (size >= max_chunk) {
                ret = bbCipherCryptAligned(kbuf, (u32 *)(data + pos), max_chunk, ckey->key, &ckey->seed, ckey->type);
                size -= max_chunk;
                pos += max_chunk;
                if (ret != 0)
                    break;
            }
            if (ret == 0 && size != 0)
                ret = bbCipherCryptAligned(kbuf, (u32 *)(data + pos), size, ckey->key, &ckey->seed, ckey->type);
        }
    }

    if (sceKernelSignalSema(g_sema_id, 1) != 0)
        ret = SCE_AMCTRL_ERROR_SEMA;
    return ret;
}
