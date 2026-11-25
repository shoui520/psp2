#include "apctl_internal.h"

/* =========================================================================
 * RC4 (OpenSSL-compatible)
 * ========================================================================= */

void rc4_crypt(Rc4Key *key, u32 len, const u8 *in, u8 *out)
{
    u32 x, y;
    u32 *d;
    u32 tx, ty;
    u32 blocks;
    u32 rem;

    if (len == 0)
        return;

    d = key->s;
    x = key->x;
    y = key->y;
    blocks = len >> 3;

    while (blocks-- != 0) {
        int k;
        for (k = 0; k < 8; k++) {
            x = (x + 1u) & 0xFFu;
            tx = d[x];
            y = (y + tx) & 0xFFu;
            ty = d[y];
            d[x] = ty;
            d[y] = tx;
            *out++ = (u8)(*in++ ^ d[(tx + ty) & 0xFFu]);
        }
    }

    rem = len & 7u;
    while (rem-- != 0) {
        x = (x + 1u) & 0xFFu;
        tx = d[x];
        y = (y + tx) & 0xFFu;
        ty = d[y];
        d[x] = ty;
        d[y] = tx;
        *out++ = (u8)(*in++ ^ d[(tx + ty) & 0xFFu]);
    }

    key->x = x;
    key->y = y;
}

void rc4_set_key(Rc4Key *key, int len, const u8 *data)
{
    u32 i;
    u32 id1 = 0;
    u32 id2 = 0;
    u32 *d = key->s;

    key->x = 0;
    key->y = 0;
    for (i = 0; i < 256u; i++)
        d[i] = i;

    for (i = 0; i < 256u; i++) {
        u32 tmp = d[i];
        id2 = (data[id1] + tmp + id2) & 0xFFu;
        if (++id1 == (u32)len)
            id1 = 0;
        d[i] = d[id2];
        d[id2] = tmp;
    }
}

const char *rc4_options(void)
{
    return "rc4( f)";
}

/* =========================================================================
 * WPA / WPA2 Supplicant
 * ========================================================================= */

static const u32 g_wpa_oui_tag     = 0x01F25000u; /* 00:50:F2:01 */
static const u32 g_wpa_oui_tkip    = 0x02F25000u; /* 00:50:F2:02 */
static const u32 g_wpa_oui_ccmp    = 0x04F25000u; /* 00:50:F2:04 */
static const u32 g_wpa_akm_8021x   = 0x01F25000u; /* 00:50:F2:01 */
static const u32 g_wpa_akm_psk     = 0x02F25000u; /* 00:50:F2:02 */

typedef struct WpaEvent {
    int type;
    void *arg1;
    u8 *pkt;
    int pkt_len;
} WpaEvent;

/*
 * Flat layout matching 0x17D70 .. 0x181BC (1100 bytes):
 *   0x000 (w0)  : state (0..5)
 *   0x004 (w1)  : initialized
 *   0x008 (w2)  : flags (bit 0 = connected notified)
 *   0x00C (w3)  : thid
 *   0x010 (w4)  : evf
 *   0x014 (w5)  : bpf_fd
 *   0x018 (w6)  : alarm_id
 *   0x020 (w8)  : timeout_us
 *   0x024 (w9..10) : last_mic_err_time (64-bit)
 *   0x02C (w11..12): last_gtk_time (64-bit)
 *   0x044 (w17) : auth_mode (0=PSK, 1=802.1X)
 *   0x048 (w18) : cipher (0=TKIP, 1=CCMP)
 *   0x04C (w19) : callback(void *info, int err)
 *   0x050 (w20) : local_mac[6]
 *   0x056       : bssid[6]
 *   0x05C (w23) : rx_buf[256]
 *   0x15C (w87) : rx_buf_size
 *   0x162       : tx_frame[256]
 *   0x268       : bss_info[92] (0x17FD8; ssid_len at +7 = 0x26F, ssid at +8 = 0x270, caps at +0x51 = 0x2B9)
 *   0x2C4 (w177): replay_counter[8]
 *   0x2CC (w179): prng_counter[32]
 *   0x2EC (w187): snonce[32]
 *   0x30C (w195): psk_passphrase_type
 *   0x310       : passphrase[64]
 *   0x350 (w212): pmk[32]
 *   0x370 (w220): ptk[64] (kck=+0, kek=+16, tk=+32)
 *   0x3B0 (w236): gtk[32] (0x18120)
 *   0x3D0 (w244): kck_ptr
 *   0x3D4 (w245): kek_ptr
 *   0x3D8 (w246): tk_ptr (0x18148)
 *   0x3DC       : config[112] (0x1814C; ssid_len at +8 = 0x3E4, ssid at +12 = 0x3E8)
 */
static u8 g_wpa_ctx_buf[1100];
static WpaEvent g_wpa_event;

#define WPA_W(c, i)   (((int *)(c))[(i)])
#define WPA_U32(c, o) (*(u32 *)(((u8 *)(c)) + (o)))
#define WPA_S32(c, o) (*(int *)(((u8 *)(c)) + (o)))
#define WPA_U16(c, o) (*(u16 *)(((u8 *)(c)) + (o)))

static int wpa_notify(int code, int err)
{
    void (*cb)(void *, int) = *(void (**)(void *, int))(g_wpa_ctx_buf + 0x4C);
    if (cb != NULL) {
        u8 msg[36];
        sceKernelMemset(msg, 0, sizeof(msg));
        *(int *)msg = code;
        if (code == 2)
            sceKernelMemcpy(msg + 4, *(void **)(g_wpa_ctx_buf + 0x3D8), 32);
        else if (code == 3)
            sceKernelMemcpy(msg + 4, g_wpa_ctx_buf + 0x3B0, 32);
        cb(msg, err);
    }
    return 0;
}

static int wpa_eapol_send(int fd, u8 *frame, const u8 *src_mac, const u8 *dst_mac,
                          u8 type, u16 len)
{
    return eapol_bpf_send(fd, frame, src_mac, dst_mac, 0, 1, type, len);
}

static void wpa_pbkdf2_f(const char *pass, const u8 *ssid, int ssid_len,
                         int iterations, u32 block_idx, u8 *out)
{
    int i;
    int pass_len = sceNet_lib_D8722983(pass);
    u8 buf[48];
    u8 u_prev[32];

    for (i = 0; i < pass_len; i++) {
        if (pass[i] < 32)
            return;
    }

    sceKernelMemcpy(buf, ssid, (SceSize)ssid_len);
    buf[ssid_len + 0] = (u8)(block_idx >> 24);
    buf[ssid_len + 1] = (u8)(block_idx >> 16);
    buf[ssid_len + 2] = (u8)(block_idx >> 8);
    buf[ssid_len + 3] = (u8)(block_idx);

    apctl_hmac_sha1(buf, ssid_len + 4, (u8 *)pass, (u32)pass_len, u_prev);
    sceKernelMemcpy(out, u_prev, 20);

    for (i = 1; i < iterations; i++) {
        int j;
        apctl_hmac_sha1(u_prev, 20, (u8 *)pass, (u32)pass_len, buf);
        sceKernelMemcpy(u_prev, buf, 20);
        for (j = 0; j < 20; j++)
            out[j] ^= buf[j];
        if ((i & 0x1FF) == 0)
            sceKernelDelayThread(1000);
    }
}

static int wpa_pbkdf2_sha1(const char *pass, const u8 *ssid, int ssid_len, u8 *out)
{
    u32 pass_len = (u32)sceNet_lib_D8722983(pass);
    if (pass_len < 64u && ssid_len < 33) {
        wpa_pbkdf2_f(pass, ssid, ssid_len, 4096, 1, out);
        wpa_pbkdf2_f(pass, ssid, ssid_len, 4096, 2, out + 20);
        return 1;
    }
    return 0;
}

static void wpa_aes_unwrap(const u8 *kek, int kek_len, const u8 *cipher,
                           int cipher_len, u8 *plain)
{
    u8 aes_ctx[976];
    u8 a[8];
    u8 b_in[16];
    u8 b_out[16];
    u8 r[248];
    int n;
    int j, i;

    if (kek == NULL || cipher == NULL || plain == NULL)
        return;
    if (kek_len != 16 || cipher_len < 24)
        return;

    sceKernelMemset(aes_ctx, 0, 972);
    sceNetInet_lib_0238B6DF(aes_ctx, 128, 128, kek);

    n = (cipher_len / 8) - 1;
    sceKernelMemcpy(a, cipher, 8);
    for (i = 0; i < n; i++)
        sceKernelMemcpy(r + 8 * i, cipher + 8 + 8 * i, 8);

    for (j = 5; j >= 0; j--) {
        for (i = n; i >= 1; i--) {
            a[7] ^= (u8)(n * j + i);
            sceKernelMemcpy(b_in, a, 8);
            sceKernelMemcpy(b_in + 8, r + 8 * (i - 1), 8);
            sceNetInet_lib_D609AD36(aes_ctx, b_in, b_out);
            sceKernelMemcpy(a, b_out, 8);
            sceKernelMemcpy(r + 8 * (i - 1), b_out + 8, 8);
        }
    }

    sceKernelMemset(b_out, 0xA6, 8);
    (void)sceNet_lib_E0A81C7C(a, b_out, 8);

    for (i = 0; i < n; i++)
        sceKernelMemcpy(plain + 8 * i, r + 8 * i, 8);
}

static void wpa_prf(const u8 *key, u32 key_len, const char *label, int label_len,
                    const u8 *data, int data_len, u8 *out, int out_len)
{
    u8 buf[1024];
    int total_len = label_len + data_len + 2;
    int blocks = (out_len + 19) / 20;
    int i;

    sceKernelMemcpy(buf, label, (SceSize)label_len);
    buf[label_len] = 0;
    sceKernelMemcpy(buf + label_len + 1, data, (SceSize)data_len);
    buf[label_len + 1 + data_len] = 0;

    for (i = 0; i < blocks; i++) {
        apctl_hmac_sha1(buf, total_len, (u8 *)key, key_len, out + 20 * i);
        buf[total_len - 1]++;
    }
}

static void wpa_hmac_md5(const u8 *key, u32 key_len, const u8 *data, int data_len, u8 *out)
{
    apctl_hmac_md5(data, data_len, (u8 *)key, key_len, out);
}

static void wpa_hmac_sha1(const u8 *key, u32 key_len, const u8 *data, int data_len, u8 *out)
{
    apctl_hmac_sha1(data, data_len, (u8 *)key, key_len, out);
}

static void wpa_rc4_decrypt(const u8 *key, int key_len, const u8 *in, u32 len, u8 *out)
{
    Rc4Key rc4;
    u8 skip[256];

    sceKernelMemset(&rc4, 0, sizeof(rc4));
    rc4_set_key(&rc4, key_len, key);
    rc4_crypt(&rc4, 256u, skip, skip);
    rc4_crypt(&rc4, len, in, out);
}

static int wpa_memcmp(const u8 *a, const u8 *b, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (a[i] != b[i])
            return -1;
    }
    return 0;
}

static const u8 *wpa_max_bytes(const u8 *a, const u8 *b, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (a[i] < b[i])
            return b;
        if (a[i] > b[i])
            return a;
    }
    return a;
}

static const u8 *wpa_min_bytes(const u8 *a, const u8 *b, int len)
{
    int i;
    for (i = 0; i < len; i++) {
        if (a[i] < b[i])
            return a;
        if (a[i] > b[i])
            return b;
    }
    return a;
}

static void wpa_inc_counter(u8 *ctr, int len)
{
    int i = len - 1;
    while (i >= 0) {
        if (ctr[i] != 0xFF) {
            ctr[i]++;
            return;
        }
        i--;
    }
    sceKernelMemset(ctr, 0, (SceSize)len);
}

static void wpa_derive_ptk_common(u8 *ctx, const u8 *pkt, int ptk_len)
{
    u8 snonce_tmp[112];
    u8 seed_nonce[16];
    u8 ptk_data[76];
    u8 ptk_out[144];
    int lbl_len;

    sceKernelMemcpy(seed_nonce, ctx + 0x50, 6);
    sceKernelGetSystemTime(seed_nonce + 8);
    lbl_len = sceNet_lib_D8722983("Init Counter");
    wpa_prf(ctx + 0x2CC, 32u, "Init Counter", lbl_len, seed_nonce, 16, snonce_tmp, 32);
    sceKernelMemcpy(ctx + 0x2EC, snonce_tmp, 32);

    sceKernelMemcpy(ptk_data + 0,  wpa_min_bytes(ctx + 0x50, ctx + 0x56, 6), 6);
    sceKernelMemcpy(ptk_data + 6,  wpa_max_bytes(ctx + 0x50, ctx + 0x56, 6), 6);
    sceKernelMemcpy(ptk_data + 12, wpa_min_bytes(ctx + 0x2EC, pkt + 31, 32), 32);
    sceKernelMemcpy(ptk_data + 44, wpa_max_bytes(ctx + 0x2EC, pkt + 31, 32), 32);

    lbl_len = sceNet_lib_D8722983("Pairwise key expansion");
    wpa_prf(ctx + 0x350, 32u, "Pairwise key expansion", lbl_len, ptk_data, 76, ptk_out, ptk_len);
    sceKernelMemcpy(ctx + 0x370, ptk_out, (SceSize)ptk_len);

    *(u8 **)(ctx + 0x3D0) = ctx + 0x370; /* KCK */
    *(u8 **)(ctx + 0x3D4) = ctx + 0x380; /* KEK */
    *(u8 **)(ctx + 0x3D8) = ctx + 0x390; /* TK  */
}

static void wpa_derive_ptk_tkip(u8 *ctx, const u8 *pkt)
{
    wpa_derive_ptk_common(ctx, pkt, 64);
}

static void wpa_derive_ptk_ccmp(u8 *ctx, const u8 *pkt)
{
    wpa_derive_ptk_common(ctx, pkt, 48);
}

static void wpa_decrypt_gtk_tkip(u8 *ctx, const u8 *pkt)
{
    u8 rc4_key[32];
    sceKernelMemcpy(rc4_key, pkt + 63, 16);
    sceKernelMemcpy(rc4_key + 16, *(u8 **)(ctx + 0x3D4), 16);
    wpa_rc4_decrypt(rc4_key, 32, pkt + 113, 32u, ctx + 0x3B0);
}

static void wpa_decrypt_gtk_ccmp(u8 *ctx, const u8 *pkt)
{
    u8 gtk_tmp[32];
    u16 key_data_len = bswap16((u16)((pkt[112] << 8) | pkt[111]));
    wpa_aes_unwrap(*(u8 **)(ctx + 0x3D4), 16, pkt + 113, key_data_len, gtk_tmp);
    sceKernelMemcpy(ctx + 0x3B0, gtk_tmp, 32);
}

static int wpa_notify_ptk(void) { return wpa_notify(2, 0); }
static int wpa_notify_gtk(void) { return wpa_notify(3, 0); }

static int wpa_send_start(u8 *ctx)
{
    u8 *tx = ctx + 0x162;
    tx[14] = 1;
    tx[15] = 1;
    tx[16] = 0;
    tx[17] = 0;
    return wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 1, 0);
}

static int wpa_send_msg2_tkip(u8 *ctx, const u8 *pkt)
{
    u8 *tx = ctx + 0x162;
    u16 be_val;
    u8 mic[48];

    sceKernelMemset(tx, 0, 137);
    tx[14] = 1;
    tx[15] = 3;
    be_val = bswap16(0x77u);
    tx[16] = (u8)be_val;
    tx[17] = (u8)(be_val >> 8);
    tx[18] = 0xFE;
    be_val = bswap16(0x0109u);
    sceKernelMemcpy(tx + 19, &be_val, 2);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemcpy(tx + 31, ctx + 0x2EC, 32);
    be_val = bswap16(0x18u);
    sceKernelMemcpy(tx + 111, &be_val, 2);
    tx[113] = 0xDD;
    tx[114] = 22;
    sceKernelMemcpy(tx + 115, &g_wpa_oui_tag, 4);
    tx[119] = 1;
    tx[120] = 0;
    sceKernelMemcpy(tx + 121, &g_wpa_oui_tkip, 4);
    tx[125] = 1;
    tx[126] = 0;
    sceKernelMemcpy(tx + 127, &g_wpa_oui_tkip, 4);
    tx[131] = 1;
    tx[132] = 0;
    if (WPA_S32(ctx, 0x44) == 0)
        sceKernelMemcpy(tx + 133, &g_wpa_akm_psk, 4);
    else if (WPA_S32(ctx, 0x44) == 1)
        sceKernelMemcpy(tx + 133, &g_wpa_akm_8021x, 4);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_md5(*(u8 **)(ctx + 0x3D0), 16u, tx + 14, 123, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 3, 0x77u);
    WPA_S32(ctx, 0) = 3;
    return 0;
}

static int wpa_send_msg4_tkip(u8 *ctx, const u8 *pkt)
{
    u8 *tx = ctx + 0x162;
    u16 be_val;
    u8 mic[48];

    sceKernelMemset(tx, 0, 113);
    tx[14] = 1;
    tx[15] = 3;
    be_val = bswap16(0x5Fu);
    tx[16] = (u8)be_val;
    tx[17] = (u8)(be_val >> 8);
    tx[18] = 0xFE;
    be_val = bswap16(0x0109u);
    sceKernelMemcpy(tx + 19, &be_val, 2);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemset(tx + 111, 0, 2);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_md5(*(u8 **)(ctx + 0x3D0), 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 3, 0x5Fu);
    WPA_S32(ctx, 0) = 4;
    return 4;
}

static int wpa_send_group2_tkip(u8 *ctx, const u8 *pkt)
{
    u8 *tx = ctx + 0x162;
    u16 be_val;
    u8 mic[48];

    sceKernelMemset(tx, 0, 113);
    tx[14] = 1;
    tx[15] = 3;
    be_val = bswap16(0x5Fu);
    tx[16] = (u8)be_val;
    tx[17] = (u8)(be_val >> 8);
    tx[18] = 0xFE;
    be_val = bswap16(0x0301u);
    sceKernelMemcpy(tx + 19, &be_val, 2);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemset(tx + 111, 0, 2);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_md5(*(u8 **)(ctx + 0x3D0), 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 3, 0x5Fu);
    if (WPA_S32(ctx, 0x18) != 0) {
        sceKernelCancelAlarm(WPA_S32(ctx, 0x18));
        WPA_S32(ctx, 0x18) = 0;
    }
    if ((WPA_S32(ctx, 0x08) & 1) == 0) {
        WPA_S32(ctx, 0x08) |= 1;
        wpa_notify(0, 0);
    }
    WPA_S32(ctx, 0) = 5;
    return 0;
}

static int wpa_send_msg2_ccmp(u8 *ctx, const u8 *pkt)
{
    u8 *tx = ctx + 0x162;
    u16 be_val;
    u8 mic[48];

    sceKernelMemset(tx, 0, 137);
    tx[14] = 1;
    tx[15] = 3;
    be_val = bswap16(0x77u);
    sceKernelMemcpy(tx + 16, &be_val, 2);
    tx[18] = 0xFE;
    be_val = bswap16(0x010Au);
    sceKernelMemcpy(tx + 19, &be_val, 2);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemcpy(tx + 31, ctx + 0x2EC, 32);
    be_val = bswap16(0x18u);
    sceKernelMemcpy(tx + 111, &be_val, 2);
    tx[113] = 0xDD;
    tx[114] = 22;
    sceKernelMemcpy(tx + 115, &g_wpa_oui_tag, 4);
    tx[119] = 1;
    tx[120] = 0;
    if ((ctx[0x2B9] & 4) != 0)
        sceKernelMemcpy(tx + 121, &g_wpa_oui_tkip, 4);
    else
        sceKernelMemcpy(tx + 121, &g_wpa_oui_ccmp, 4);
    tx[125] = 1;
    tx[126] = 0;
    sceKernelMemcpy(tx + 127, &g_wpa_oui_ccmp, 4);
    tx[131] = 1;
    tx[132] = 0;
    if (WPA_S32(ctx, 0x44) == 0)
        sceKernelMemcpy(tx + 133, &g_wpa_akm_psk, 4);
    else if (WPA_S32(ctx, 0x44) == 1)
        sceKernelMemcpy(tx + 133, &g_wpa_akm_8021x, 4);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_sha1(*(u8 **)(ctx + 0x3D0), 16u, tx + 14, 123, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 3, 0x77u);
    WPA_S32(ctx, 0) = 3;
    return 0;
}

static int wpa_send_msg4_ccmp(u8 *ctx, const u8 *pkt)
{
    u8 *tx = ctx + 0x162;
    u16 be_val;
    u8 mic[48];

    sceKernelMemset(tx, 0, 113);
    tx[14] = 1;
    tx[15] = 3;
    be_val = bswap16(0x5Fu);
    tx[16] = (u8)be_val;
    tx[17] = (u8)(be_val >> 8);
    tx[18] = 0xFE;
    be_val = bswap16(0x010Au);
    sceKernelMemcpy(tx + 19, &be_val, 2);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemset(tx + 111, 0, 2);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_sha1(*(u8 **)(ctx + 0x3D0), 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 3, 0x5Fu);
    WPA_S32(ctx, 0) = 4;
    return 4;
}

static int wpa_send_group2_ccmp(u8 *ctx, const u8 *pkt)
{
    u8 *tx = ctx + 0x162;
    u16 be_val;
    u8 mic[48];

    sceKernelMemset(tx, 0, 113);
    tx[14] = 1;
    tx[15] = 3;
    be_val = bswap16(0x5Fu);
    tx[16] = (u8)be_val;
    tx[17] = (u8)(be_val >> 8);
    tx[18] = 0xFE;
    be_val = bswap16(0x0302u);
    sceKernelMemcpy(tx + 19, &be_val, 2);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemset(tx + 111, 0, 2);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_sha1(*(u8 **)(ctx + 0x3D0), 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 3, 0x5Fu);
    if (WPA_S32(ctx, 0x18) != 0) {
        sceKernelCancelAlarm(WPA_S32(ctx, 0x18));
        WPA_S32(ctx, 0x18) = 0;
    }
    if ((WPA_S32(ctx, 0x08) & 1) == 0) {
        WPA_S32(ctx, 0x08) |= 1;
        wpa_notify(0, 0);
    }
    WPA_S32(ctx, 0) = 5;
    return 0;
}

static int wpa_send_mic_err_tkip(u8 *ctx, int is_group)
{
    u8 *tx = ctx + 0x162;
    u16 key_info = 0x0F01u;
    u16 be_val;
    u8 mic[48];

    sceKernelMemset(tx, 0, 113);
    tx[14] = 1;
    tx[15] = 3;
    be_val = bswap16(0x5Fu);
    tx[16] = (u8)be_val;
    tx[17] = (u8)(be_val >> 8);
    tx[18] = 0xFE;
    if (is_group == 0)
        key_info |= 8u;
    be_val = bswap16(key_info);
    sceKernelMemcpy(tx + 19, &be_val, 2);
    wpa_inc_counter(ctx + 0x2C4, 8);
    sceKernelMemcpy(tx + 23, ctx + 0x2C4, 8);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_md5(*(u8 **)(ctx + 0x3D0), 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);
    return wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 3, 0x5Fu);
}

static int wpa_send_mic_err_ccmp(u8 *ctx, int is_group)
{
    u8 *tx = ctx + 0x162;
    u16 key_info = 0x0F02u;
    u16 be_val;
    u8 mic[48];

    sceKernelMemset(tx, 0, 113);
    tx[14] = 1;
    tx[15] = 3;
    be_val = bswap16(0x5Fu);
    tx[16] = (u8)be_val;
    tx[17] = (u8)(be_val >> 8);
    tx[18] = 0xFE;
    if (is_group == 0)
        key_info |= 8u;
    be_val = bswap16(key_info);
    sceKernelMemcpy(tx + 19, &be_val, 2);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_sha1(*(u8 **)(ctx + 0x3D0), 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);
    return wpa_eapol_send(WPA_S32(ctx, 0x14), tx, ctx + 0x50, ctx + 0x56, 3, 0x5Fu);
}

static int wpa_handle_msg1(u8 *ctx, WpaEvent *ev)
{
    u8 *pkt = ev->pkt + *(u16 *)(ev->pkt + 16);
    u16 eapol_len = (u16)((pkt[17] << 8) | pkt[16]);
    u16 key_info;
    u16 key_len;

    if (eapol_len < 0x5Fu)
        return 0;

    sceKernelMemcpy(ctx + 0x2C4, pkt + 23, 8);
    if (pkt[18] != 0xFE)
        return 0;

    key_info = bswap16((u16)((pkt[20] << 8) | pkt[19]));
    if (((key_info & 7u) - 1u) < 2u && (key_info & 0x0DC8u) == 0x0088u) {
        key_len = bswap16((u16)((pkt[22] << 8) | pkt[21]));
        if ((key_info & 7u) == 1u) {
            if (WPA_S32(ctx, 0x48) == 0 && key_len == 32u) {
                wpa_derive_ptk_tkip(ctx, pkt);
                wpa_send_msg2_tkip(ctx, pkt);
            }
        } else if ((key_info & 7u) == 2u) {
            if (WPA_S32(ctx, 0x48) == 1 && key_len == 16u) {
                wpa_derive_ptk_ccmp(ctx, pkt);
                wpa_send_msg2_ccmp(ctx, pkt);
            }
        }
    }
    return 0;
}

static int wpa_handle_msg3(u8 *ctx, WpaEvent *ev)
{
    u8 *pkt = ev->pkt + *(u16 *)(ev->pkt + 16);
    u16 eapol_len = (u16)((pkt[17] << 8) | pkt[16]);
    u16 key_info;
    u8 rx_mic[16];
    u8 calc_mic[48];

    if (eapol_len < 0x5Fu)
        return 0;

    key_info = bswap16((u16)((pkt[20] << 8) | pkt[19]));
    if (((key_info & 7u) - 1u) >= 2u) {
        if (WPA_S32(ctx, 0x44) == 0)
            wpa_send_start(ctx);
        WPA_S32(ctx, 0) = 2;
        return 0;
    }

    if ((key_info & 0x0D88u) != 0x0188u) {
        if ((key_info & 0x0DC8u) == 0x0088u)
            return wpa_handle_msg1(ctx, ev);
        if (WPA_S32(ctx, 0x44) == 0)
            wpa_send_start(ctx);
        WPA_S32(ctx, 0) = 2;
        return 0;
    }

    if ((key_info & 7u) == 1u) {
        sceKernelMemcpy(rx_mic, pkt + 95, 16);
        sceKernelMemset(pkt + 95, 0, 16);
        wpa_hmac_md5(*(u8 **)(ctx + 0x3D0), 16u, pkt + 14,
                     bswap16(eapol_len) + 4, calc_mic);
        if (wpa_memcmp(rx_mic, calc_mic, 16) < 0)
            return 0;
        if (wpa_max_bytes(pkt + 23, ctx + 0x2C4, 8) == pkt + 23 &&
            WPA_S32(ctx, 0x48) == 0 &&
            bswap16((u16)((pkt[22] << 8) | pkt[21])) == 32u) {
            wpa_send_msg4_tkip(ctx, pkt);
            wpa_notify_ptk();
        }
    } else if ((key_info & 7u) == 2u) {
        sceKernelMemcpy(rx_mic, pkt + 95, 16);
        sceKernelMemset(pkt + 95, 0, 16);
        wpa_hmac_sha1(*(u8 **)(ctx + 0x3D0), 16u, pkt + 14,
                      bswap16(eapol_len) + 4, calc_mic);
        if (wpa_memcmp(rx_mic, calc_mic, 16) < 0)
            return 0;
        if (wpa_max_bytes(pkt + 23, ctx + 0x2C4, 8) == pkt + 23 &&
            WPA_S32(ctx, 0x48) == 1 &&
            bswap16((u16)((pkt[22] << 8) | pkt[21])) == 16u) {
            wpa_send_msg4_ccmp(ctx, pkt);
            wpa_notify_ptk();
        }
    }

    sceKernelMemcpy(ctx + 0x2C4, pkt + 23, 8);
    return 0;
}

static int wpa_handle_group(u8 *ctx, WpaEvent *ev)
{
    u8 *pkt = ev->pkt + *(u16 *)(ev->pkt + 16);
    u16 eapol_len = (u16)((pkt[17] << 8) | pkt[16]);
    u16 key_info;
    u8 rx_mic[16];
    u8 calc_mic[48];
    u8 prev_gtk[32];
    u8 now[8];

    if (eapol_len < 0x5Fu)
        return 0;

    key_info = bswap16((u16)((pkt[20] << 8) | pkt[19]));
    if (((key_info & 7u) - 1u) >= 2u) {
        if (WPA_S32(ctx, 0x44) == 0)
            wpa_send_start(ctx);
        WPA_S32(ctx, 0) = 2;
        return 0;
    }

    if ((key_info & 0x0F88u) != 0x0380u) {
        if ((key_info & 0x0D88u) == 0x0188u)
            return wpa_handle_msg3(ctx, ev);
        if ((key_info & 0x0DC8u) == 0x0088u)
            return wpa_handle_msg1(ctx, ev);
        if (WPA_S32(ctx, 0x44) == 0)
            wpa_send_start(ctx);
        WPA_S32(ctx, 0) = 2;
        return 0;
    }

    if (sceKernelGetSystemTime(now) >= 0)
        sceKernelMemcpy(ctx + 0x2C, now, 8);

    if ((key_info & 7u) == 1u) {
        sceKernelMemcpy(rx_mic, pkt + 95, 16);
        sceKernelMemset(pkt + 95, 0, 16);
        wpa_hmac_md5(*(u8 **)(ctx + 0x3D0), 16u, pkt + 14, 131, calc_mic);
        if (wpa_memcmp(rx_mic, calc_mic, 16) < 0)
            return 0;
        if (wpa_max_bytes(pkt + 23, ctx + 0x2C4, 8) == pkt + 23) {
            sceKernelMemcpy(prev_gtk, ctx + 0x3B0, 32);
            wpa_decrypt_gtk_tkip(ctx, pkt);
            if (sceNet_lib_E0A81C7C(prev_gtk, ctx + 0x3B0, 32) != 0)
                wpa_notify_gtk();
            if (WPA_S32(ctx, 0x48) == 0 &&
                bswap16((u16)((pkt[22] << 8) | pkt[21])) == 32u)
                wpa_send_group2_tkip(ctx, pkt);
        }
    } else if ((key_info & 7u) == 2u) {
        sceKernelMemcpy(rx_mic, pkt + 95, 16);
        sceKernelMemset(pkt + 95, 0, 16);
        wpa_hmac_sha1(*(u8 **)(ctx + 0x3D0), 16u, pkt + 14,
                      bswap16(eapol_len) + 4, calc_mic);
        if (wpa_memcmp(rx_mic, calc_mic, 16) < 0)
            return 0;
        if (wpa_max_bytes(pkt + 23, ctx + 0x2C4, 8) == pkt + 23) {
            u16 key_len;
            sceKernelMemcpy(prev_gtk, ctx + 0x3B0, 32);
            wpa_decrypt_gtk_ccmp(ctx, pkt);
            if (sceNet_lib_E0A81C7C(prev_gtk, ctx + 0x3B0, 32) != 0)
                wpa_notify_gtk();
            key_len = bswap16((u16)((pkt[22] << 8) | pkt[21]));
            if (WPA_S32(ctx, 0x48) == 1 && (key_len == 16u || key_len == 32u))
                wpa_send_group2_ccmp(ctx, pkt);
        }
    }

    sceKernelMemcpy(ctx + 0x2C4, pkt + 23, 8);
    return 0;
}

static int wpa_handle_mic_err(u8 *ctx, WpaEvent *ev)
{
    u32 now[2];
    int is_group = (int)ev->arg1;

    if (WPA_S32(ctx, 0x48) != 0 || sceKernelGetSystemTime(now) < 0)
        return 0;

    if (WPA_U32(ctx, 0x2C) != 0) {
        if (WPA_U32(ctx, 0x30) == now[1] && (now[0] - WPA_U32(ctx, 0x2C)) <= 4999999u)
            return 0;
        if (now[1] == WPA_U32(ctx, 0x28) - 1u)
            return 0;
    }

    if (WPA_S32(ctx, 0x48) == 0)
        wpa_send_mic_err_tkip(ctx, is_group);
    else if (WPA_S32(ctx, 0x48) == 1)
        wpa_send_mic_err_ccmp(ctx, is_group);

    if (WPA_U32(ctx, 0x24) != 0) {
        if ((WPA_U32(ctx, 0x28) == now[1] && (now[0] - WPA_U32(ctx, 0x24)) <= 59999999u) ||
            now[1] == WPA_U32(ctx, 0x28) - 1u) {
            wpa_notify(4, (int)0x80411082);
            return 0;
        }
    }
    sceKernelMemcpy(ctx + 0x24, now, 8);
    return 0;
}

static int wpa_alloc_event(u8 *ctx, WpaEvent **out)
{
    (void)ctx;
    *out = &g_wpa_event;
    return 0;
}

static int wpa_free_event(u8 *ctx, WpaEvent *ev)
{
    (void)ctx;
    (void)ev;
    return 0;
}

static int wpa_state_noop(u8 *ctx, WpaEvent *ev)
{
    (void)ctx;
    (void)ev;
    return 0;
}

static int wpa_state_start(u8 *ctx, WpaEvent *ev)
{
    (void)ev;
    if (WPA_S32(ctx, 0x44) == 0)
        wpa_send_start(ctx);
    WPA_S32(ctx, 0) = 2;
    return 0;
}

static int wpa_state_timeout(u8 *ctx, WpaEvent *ev)
{
    (void)ev;
    wpa_notify(4, (int)0x80411081);
    WPA_S32(ctx, 0) = 0;
    return -1;
}

static int wpa_state_stop(u8 *ctx, WpaEvent *ev)
{
    (void)ev;
    WPA_S32(ctx, 0) = 0;
    return -1;
}

typedef int (*WpaHandler)(u8 *ctx, WpaEvent *ev);

static const WpaHandler g_wpa_state_table[6][6] = {
    { wpa_state_noop, wpa_state_noop,  wpa_state_noop,   wpa_state_noop,    wpa_state_noop,     wpa_state_stop },
    { wpa_state_noop, wpa_state_start, wpa_handle_msg1,  wpa_state_timeout, wpa_state_noop,     wpa_state_stop },
    { wpa_state_noop, wpa_state_start, wpa_handle_msg1,  wpa_state_timeout, wpa_state_noop,     wpa_state_stop },
    { wpa_state_noop, wpa_state_start, wpa_handle_msg3,  wpa_state_timeout, wpa_state_noop,     wpa_state_stop },
    { wpa_state_noop, wpa_state_start, wpa_handle_group, wpa_state_timeout, wpa_state_noop,     wpa_state_stop },
    { wpa_state_noop, wpa_state_start, wpa_handle_group, wpa_state_noop,    wpa_handle_mic_err, wpa_state_stop },
};

static u32 wpa_alarm_cb(void *common)
{
    u8 *ctx = (u8 *)common;
    sceKernelSetEventFlag(WPA_S32(ctx, 0x10), 4u);
    return 0;
}

static int wpa_get_event(u8 *ctx, WpaEvent **out)
{
    u32 bits = 0;
    u8 *pkt = NULL;
    int pkt_len = 0;
    int ret;

    if (sceKernelPollEventFlag(WPA_S32(ctx, 0x10), 31u, 1u, &bits) >= 0) {
        if ((bits & 1u) != 0) {
            sceKernelClearEventFlag(WPA_S32(ctx, 0x10), ~1u);
            wpa_alloc_event(ctx, out);
            (*out)->type = 1;
            return 0;
        }
        if ((bits & 2u) != 0) {
            sceKernelClearEventFlag(WPA_S32(ctx, 0x10), ~2u);
            wpa_alloc_event(ctx, out);
            (*out)->type = 5;
            return 0;
        }
        if ((bits & 4u) != 0) {
            sceKernelClearEventFlag(WPA_S32(ctx, 0x10), ~4u);
            wpa_alloc_event(ctx, out);
            (*out)->type = 3;
            return 0;
        }
        if ((bits & 8u) != 0) {
            sceKernelClearEventFlag(WPA_S32(ctx, 0x10), ~8u);
            wpa_alloc_event(ctx, out);
            (*out)->type = 4;
            (*out)->arg1 = (void *)0;
            return 0;
        }
        if ((bits & 16u) != 0) {
            sceKernelClearEventFlag(WPA_S32(ctx, 0x10), ~16u);
            wpa_alloc_event(ctx, out);
            (*out)->type = 4;
            (*out)->arg1 = (void *)1;
            return 0;
        }
        return 0;
    }

    ret = eapol_bpf_recv_wpa(WPA_S32(ctx, 0x14), ctx + 0x5C,
                             WPA_U32(ctx, 0x15C), ctx + 0x50, &pkt, &pkt_len);
    if (ret < 0) {
        sceKernelDelayThread(1000000);
        return -1;
    }
    if (ret == 0)
        return -1;

    wpa_alloc_event(ctx, out);
    (*out)->type = 2;
    (*out)->arg1 = ctx;
    (*out)->pkt = pkt;
    (*out)->pkt_len = pkt_len;
    return 0;
}

static int wpa_supplicant_thread(SceSize args, void *argp)
{
    u8 *ctx = NULL;
    u8 prng_out[112];
    u8 seed[52];
    u32 iter;
    WpaEvent *ev;

    if (args != 4u || argp == NULL)
        goto exit_thread;

    ctx = *(u8 **)argp;
    if (WPA_U32(ctx, 0x20) != 0) {
        SceUID alarm = sceKernelSetAlarm(WPA_U32(ctx, 0x20), wpa_alarm_cb, ctx);
        if (alarm < 0)
            goto exit_thread;
        WPA_S32(ctx, 0x18) = alarm;
    }

    sceKernelMemset(prng_out, 0, sizeof(prng_out));
    sceKernelMemset(seed, 0, sizeof(seed));
    sceKernelMemcpy(seed, ctx + 0x50, 6);
    sceKernelGetSystemTime(seed + 8);

    for (iter = 0; iter < 32u; iter++) {
        *(u32 *)(seed + 48) = iter;
        wpa_prf(NULL, 0, "Init Counter", 12, seed, 52, prng_out, 32);
    }
    sceKernelMemcpy(ctx + 0x2CC, prng_out, 32);

    WPA_S32(ctx, 0) = 1;
    sceKernelSetEventFlag(WPA_S32(ctx, 0x10), 1u);

    for (;;) {
        int r;
        if (wpa_get_event(ctx, &ev) < 0)
            continue;
        if ((u32)ev->type >= 6u) {
            wpa_free_event(ctx, ev);
            continue;
        }
        r = g_wpa_state_table[WPA_S32(ctx, 0)][ev->type](ctx, ev);
        wpa_free_event(ctx, ev);
        if (r < 0)
            break;
    }

exit_thread:
    wpa_notify(1, 0);
    if (ctx != NULL && WPA_S32(ctx, 0x18) != 0)
        sceKernelCancelAlarm(WPA_S32(ctx, 0x18));
    sceKernelExitThread(0);
    return 0;
}

static int wpa_set_config_impl(const int *cfg)
{
    u8 pmk_tmp[64];
    int auth = cfg[0];
    int cipher = cfg[1];

    sceKernelMemset(g_wpa_ctx_buf, 0, sizeof(g_wpa_ctx_buf));
    sceKernelMemcpy(g_wpa_ctx_buf + 0x3DC, cfg, 112);
    WPA_S32(g_wpa_ctx_buf, 4) = 1;

    if (auth == 0) {
        const u8 *pmk_src;
        if (cfg[11] == 1) {
            sceKernelMemcpy(g_wpa_ctx_buf + 0x310, cfg + 12, 64);
            wpa_pbkdf2_sha1((const char *)(g_wpa_ctx_buf + 0x310),
                            g_wpa_ctx_buf + 0x3E8, g_wpa_ctx_buf[0x3E4], pmk_tmp);
            pmk_src = pmk_tmp;
        } else {
            pmk_src = (const u8 *)(cfg + 12);
        }
        sceKernelMemcpy(g_wpa_ctx_buf + 0x350, pmk_src, 32);
        WPA_S32(g_wpa_ctx_buf, 0x44) = 0;
        WPA_S32(g_wpa_ctx_buf, 0x30C) = cfg[11];
    } else if (auth == 1) {
        sceKernelMemcpy(g_wpa_ctx_buf + 0x350, cfg + 11, 32);
        WPA_S32(g_wpa_ctx_buf, 0x44) = 1;
    }

    if (cipher == 0) {
        WPA_S32(g_wpa_ctx_buf, 0x48) = 0;
    } else if (cipher == 1) {
        WPA_S32(g_wpa_ctx_buf, 0x48) = 1;
    } else {
        WPA_S32(g_wpa_ctx_buf, 4) = 0;
        return (int)0x80411080;
    }
    return 0;
}

static int wpa_start_impl(const char *ifname, void (*cb)(void *, int),
                          int stack, int prio, int timeout, const void *bss)
{
    int ret = (int)0x80411083;
    SceUID evf;
    SceUID thid;
    u8 pmk_tmp[64];
    void *ctx_ptr = g_wpa_ctx_buf;

    (void)ifname;

    if (WPA_S32(g_wpa_ctx_buf, 4) != 0) {
        evf = sceKernelCreateEventFlag("SceNetWpaSupplicantEvf", 0, 0, NULL);
        ret = evf;
        if (evf >= 0) {
            WPA_S32(g_wpa_ctx_buf, 0x10) = evf;
            sceKernelMemcpy(g_wpa_ctx_buf + 0x268, bss, 92);
            sceKernelMemcpy(g_wpa_ctx_buf + 0x56, g_wpa_ctx_buf + 0x268, 6);
            *(void (**)(void *, int))(g_wpa_ctx_buf + 0x4C) = cb;
            WPA_S32(g_wpa_ctx_buf, 0x20) = timeout;

            if (WPA_S32(g_wpa_ctx_buf, 0x3DC) == 0) {
                u8 bss_ssid_len = g_wpa_ctx_buf[0x26F];
                if (bss_ssid_len != g_wpa_ctx_buf[0x3E4] ||
                    sceNet_lib_E0A81C7C(g_wpa_ctx_buf + 0x270,
                                        g_wpa_ctx_buf + 0x3E8, bss_ssid_len) != 0) {
                    wpa_pbkdf2_sha1((const char *)(g_wpa_ctx_buf + 0x310),
                                    g_wpa_ctx_buf + 0x270, bss_ssid_len, pmk_tmp);
                    sceKernelMemcpy(g_wpa_ctx_buf + 0x350, pmk_tmp, 32);
                }
            }

            thid = sceKernelCreateThread("SceNetWpaSupplicant", wpa_supplicant_thread,
                                         prio, stack, 0, NULL);
            ret = thid;
            if (thid >= 0) {
                WPA_S32(g_wpa_ctx_buf, 0x0C) = thid;
                ret = sceKernelStartThread(thid, 4, &ctx_ptr);
                if (ret >= 0)
                    return 0;
            }
        }
    }

    if (WPA_S32(g_wpa_ctx_buf, 0x14) != 0) {
        eapol_bpf_close(WPA_S32(g_wpa_ctx_buf, 0x14));
        WPA_S32(g_wpa_ctx_buf, 0x14) = 0;
    }
    if (WPA_S32(g_wpa_ctx_buf, 0x10) != 0) {
        sceKernelDeleteEventFlag(WPA_S32(g_wpa_ctx_buf, 0x10));
        WPA_S32(g_wpa_ctx_buf, 0x10) = 0;
    }
    if (WPA_S32(g_wpa_ctx_buf, 0x0C) != 0) {
        sceKernelDeleteThread(WPA_S32(g_wpa_ctx_buf, 0x0C));
        WPA_S32(g_wpa_ctx_buf, 0x0C) = 0;
    }
    return ret;
}

static int wpa_stop_impl(void)
{
    u32 timeout = 5000000u;
    SceUID thid = WPA_S32(g_wpa_ctx_buf, 0x0C);
    SceUID evf = WPA_S32(g_wpa_ctx_buf, 0x10);

    if (thid != 0) {
        if (sceKernelSetEventFlag(evf, 2u) < 0 ||
            sceKernelWaitThreadEnd(thid, &timeout) < 0) {
            sceKernelTerminateThread(thid);
        }
        sceKernelDeleteThread(thid);
        sceNetFreeThreadinfo(thid);
    }
    WPA_S32(g_wpa_ctx_buf, 0x0C) = 0;

    if (WPA_S32(g_wpa_ctx_buf, 0x14) != 0) {
        eapol_bpf_close(WPA_S32(g_wpa_ctx_buf, 0x14));
        WPA_S32(g_wpa_ctx_buf, 0x14) = 0;
    }
    if (evf != 0) {
        sceKernelDeleteEventFlag(evf);
        WPA_S32(g_wpa_ctx_buf, 0x10) = 0;
    }
    WPA_S32(g_wpa_ctx_buf, 4) = 0;
    *(void **)(g_wpa_ctx_buf + 0x4C) = NULL;
    return 0;
}

int wpa_set_config(void *cfg)
{
    return wpa_set_config_impl((const int *)cfg);
}

int wpa_start(const char *ifname, void (*cb)(void *, int), int stack,
              int prio, int timeout, void *bss)
{
    return wpa_start_impl(ifname, cb, stack, prio, timeout, bss);
}

int wpa_stop(void)
{
    wpa_stop_impl();
    return 0;
}

int wpa_notify_mic_err_unicast(void)
{
    SceUID evf = WPA_S32(g_wpa_ctx_buf, 0x10);
    if (evf != 0)
        sceKernelSetEventFlag(evf, 8u);
    return 0;
}

int wpa_notify_mic_err_multicast(void)
{
    SceUID evf = WPA_S32(g_wpa_ctx_buf, 0x10);
    if (evf != 0)
        sceKernelSetEventFlag(evf, 16u);
    return 0;
}
