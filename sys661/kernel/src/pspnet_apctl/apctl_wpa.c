#include "pspnet_apctl_internal.h"

/* =========================================================================
 * RC4 (OpenSSL-derived, placed at module offset 0x0000 in original PRX)
 * ========================================================================= */

void rc4_crypt(Rc4Key *key, u32 len, const u8 *in, u8 *out)
{
    u32 x = key->x;
    u32 y = key->y;
    u32 *s = key->s;
    u32 n = len >> 3;

    while (n > 0) {
        u32 tx, ty;
#define RC4_STEP(idx)                                   \
        do {                                            \
            x = (x + 1u) & 0xFFu;                       \
            tx = s[x];                                  \
            y = (tx + y) & 0xFFu;                       \
            ty = s[y];                                  \
            s[x] = ty;                                  \
            s[y] = tx;                                  \
            out[(idx)] = (u8)(in[(idx)] ^ s[(tx + ty) & 0xFFu]); \
        } while (0)
        RC4_STEP(0);
        RC4_STEP(1);
        RC4_STEP(2);
        RC4_STEP(3);
        RC4_STEP(4);
        RC4_STEP(5);
        RC4_STEP(6);
        RC4_STEP(7);
        in += 8;
        out += 8;
        n--;
    }

    len &= 7u;
    if (len > 0) {
        u32 tx, ty;
        u32 idx = 0;
        while (idx < len) {
            RC4_STEP(idx);
            idx++;
        }
#undef RC4_STEP
    }

    key->x = x;
    key->y = y;
}

void rc4_set_key(Rc4Key *key, int len, const u8 *data)
{
    u32 *s = key->s;
    u32 i;
    u32 id_key = 0;
    u32 id_state = 0;

    key->x = 0;
    key->y = 0;
    for (i = 0; i < 256u; i++)
        s[i] = i;

    for (i = 0; i < 256u; i++) {
        u32 tmp = s[i];
        id_state = (data[id_key] + tmp + id_state) & 0xFFu;
        id_key++;
        if ((int)id_key == len)
            id_key = 0;
        s[i] = s[id_state];
        s[id_state] = tmp;
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
    void *context;
    u8 *pkt;
    int pkt_len;
} WpaEvent;

typedef struct WpaNotifyMsg {
    int code;
    u8 key[32];
} WpaNotifyMsg;

typedef struct WpaPrngSeed {
    u8 prefix[48];
    u32 counter;
} WpaPrngSeed;

typedef struct WpaConfig {
    int auth_mode;          /* +0x00 */
    int cipher;             /* +0x04 */
    u8 ssid_len;            /* +0x08 */
    u8 pad_ssid[3];         /* +0x09 */
    u8 ssid[32];            /* +0x0C */
    int psk_type;           /* +0x2C (word 11) */
    u8 key_material[64];    /* +0x30 (word 12) */
} WpaConfig; /* 112 bytes */

typedef struct WpaSupplicantCtx {
    int state;                          /* 0x000 */
    int initialized;                    /* 0x004 */
    int flags;                          /* 0x008 (bit 0 = connected notified) */
    SceUID thid;                        /* 0x00C */
    SceUID evf;                         /* 0x010 */
    int bpf_fd;                         /* 0x014 */
    SceUID alarm_id;                    /* 0x018 */
    int pad_alarm;                      /* 0x01C */
    u32 timeout_us;                     /* 0x020 */
    u32 last_mic_err_lo;                /* 0x024 */
    u32 last_mic_err_hi;                /* 0x028 */
    u32 last_gtk_lo;                    /* 0x02C */
    u32 last_gtk_hi;                    /* 0x030 */
    u8 pad_time[16];                    /* 0x034 */
    int auth_mode;                      /* 0x044 (0=PSK, 1=802.1X) */
    int cipher;                         /* 0x048 (0=TKIP, 1=CCMP) */
    void (*callback)(void *, int);      /* 0x04C */
    u8 sta_mac[6];                      /* 0x050 */
    u8 bssid[6];                        /* 0x056 */
    u8 rx_buf[256];                     /* 0x05C */
    u32 rx_buf_size;                    /* 0x15C */
    u8 pad_tx[2];                       /* 0x160 */
    u8 tx_frame[256];                   /* 0x162 */
    u8 pad_bss[6];                      /* 0x262 */
    u8 bss_info[92];                    /* 0x268 (+7=ssid_len, +8=ssid, +0x51=caps) */
    u8 replay_counter[8];               /* 0x2C4 */
    u8 prng_counter[32];                /* 0x2CC */
    u8 snonce[32];                      /* 0x2EC */
    int psk_passphrase_type;            /* 0x30C */
    char passphrase[64];                /* 0x310 */
    u8 pmk[32];                         /* 0x350 */
    u8 ptk[64];                         /* 0x370 (kck=+0, kek=+16, tk=+32) */
    u8 gtk[32];                         /* 0x3B0 */
    u8 *kck_ptr;                        /* 0x3D0 */
    u8 *kek_ptr;                        /* 0x3D4 */
    u8 *tk_ptr;                         /* 0x3D8 */
    WpaConfig config;                   /* 0x3DC (112 bytes) */
} WpaSupplicantCtx; /* 1100 bytes */

static WpaSupplicantCtx g_wpa_ctx;
static WpaEvent g_wpa_event;

static int wpa_notify(int code, int err)
{
    void (*cb)(void *, int) = g_wpa_ctx.callback;
    if (cb != NULL) {
        WpaNotifyMsg msg;
        sceKernelMemset(&msg, 0, sizeof(msg));
        msg.code = code;
        if (code == 2)
            sceKernelMemcpy(msg.key, g_wpa_ctx.tk_ptr, 32);
        else if (code == 3)
            sceKernelMemcpy(msg.key, g_wpa_ctx.gtk, 32);
        cb(&msg, err);
    }
    return 0;
}

static int wpa_eapol_send(int bpf_fd, u8 *frame, const u8 *src_mac, const u8 *dst_mac,
                          u8 type, u16 len)
{
    return eapol_bpf_send(bpf_fd, frame, src_mac, dst_mac, 0, 1, type, len);
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

static inline __attribute__((always_inline))
void wpa_derive_ptk_common(WpaSupplicantCtx *ctx, const u8 *pkt, int ptk_len)
{
    u8 snonce_tmp[112];
    u8 seed_nonce[16];
    u8 ptk_data[76];
    u8 ptk_out[144];
    int lbl_len;

    sceKernelMemcpy(seed_nonce, ctx->sta_mac, 6);
    sceKernelGetSystemTime(seed_nonce + 8);
    lbl_len = sceNet_lib_D8722983("Init Counter");
    wpa_prf(ctx->prng_counter, 32u, "Init Counter", lbl_len, seed_nonce, 16, snonce_tmp, 32);
    sceKernelMemcpy(ctx->snonce, snonce_tmp, 32);

    sceKernelMemcpy(ptk_data + 0,  wpa_min_bytes(ctx->sta_mac, ctx->bssid, 6), 6);
    sceKernelMemcpy(ptk_data + 6,  wpa_max_bytes(ctx->sta_mac, ctx->bssid, 6), 6);
    sceKernelMemcpy(ptk_data + 12, wpa_min_bytes(ctx->snonce, pkt + 31, 32), 32);
    sceKernelMemcpy(ptk_data + 44, wpa_max_bytes(ctx->snonce, pkt + 31, 32), 32);

    lbl_len = sceNet_lib_D8722983("Pairwise key expansion");
    wpa_prf(ctx->pmk, 32u, "Pairwise key expansion", lbl_len, ptk_data, 76, ptk_out, ptk_len);
    sceKernelMemcpy(ctx->ptk, ptk_out, (SceSize)ptk_len);

    ctx->kck_ptr = ctx->ptk;
    ctx->kek_ptr = ctx->ptk + 16;
    ctx->tk_ptr = ctx->ptk + 32;
}

static void wpa_derive_ptk_tkip(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    wpa_derive_ptk_common(ctx, pkt, 64);
}

static void wpa_derive_ptk_ccmp(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    wpa_derive_ptk_common(ctx, pkt, 48);
}

static void wpa_decrypt_gtk_tkip(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    u8 rc4_key[32];
    sceKernelMemcpy(rc4_key, pkt + 63, 16);
    sceKernelMemcpy(rc4_key + 16, ctx->kek_ptr, 16);
    wpa_rc4_decrypt(rc4_key, 32, pkt + 113, 32u, ctx->gtk);
}

static void wpa_decrypt_gtk_ccmp(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    u8 gtk_tmp[32];
    u16 key_data_len = bswap16((u16)((pkt[112] << 8) | pkt[111]));
    wpa_aes_unwrap(ctx->kek_ptr, 16, pkt + 113, key_data_len, gtk_tmp);
    sceKernelMemcpy(ctx->gtk, gtk_tmp, 32);
}

static int wpa_notify_ptk(void) { return wpa_notify(2, 0); }
static int wpa_notify_gtk(void) { return wpa_notify(3, 0); }

static int wpa_send_start(WpaSupplicantCtx *ctx)
{
    u8 *tx = ctx->tx_frame;
    tx[14] = 1;
    tx[15] = 1;
    tx[16] = 0;
    tx[17] = 0;
    return wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 1, 0);
}

static int wpa_send_msg2_tkip(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    u8 *tx = ctx->tx_frame;
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
    sceKernelMemcpy(tx + 31, ctx->snonce, 32);
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
    if (ctx->auth_mode == 0)
        sceKernelMemcpy(tx + 133, &g_wpa_akm_psk, 4);
    else if (ctx->auth_mode == 1)
        sceKernelMemcpy(tx + 133, &g_wpa_akm_8021x, 4);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_md5(ctx->kck_ptr, 16u, tx + 14, 123, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 3, 0x77u);
    ctx->state = 3;
    return 0;
}

static int wpa_send_msg4_tkip(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    u8 *tx = ctx->tx_frame;
    u16 key_info;
    u16 key_info_copy;
    u16 body_len;
    u16 key_info_be;
    u8 mic[48];

    sceKernelMemset(tx, 0, 113);
    tx[14] = 1;
    tx[15] = 3;
    body_len = bswap16(0x5Fu);
    tx[16] = (u8)body_len;
    tx[17] = (u8)(body_len >> 8);
    tx[18] = 0xFE;
    sceKernelMemset(&key_info, 0, 2);
    key_info = (u16)((key_info & ~7u) | 0x0109u);
    sceKernelMemcpy(&key_info_copy, &key_info, 2);
    key_info_be = bswap16(key_info_copy);
    tx[19] = (u8)key_info_be;
    tx[20] = (u8)(key_info_be >> 8);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemset(tx + 111, 0, 2);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_md5(ctx->kck_ptr, 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 3, 0x5Fu);
    ctx->state = 4;
    return 4;
}

static int wpa_send_group2_tkip(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    u8 *tx = ctx->tx_frame;
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
    wpa_hmac_md5(ctx->kck_ptr, 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 3, 0x5Fu);
    if (ctx->alarm_id != 0) {
        sceKernelCancelAlarm(ctx->alarm_id);
        ctx->alarm_id = 0;
    }
    if ((ctx->flags & 1) == 0) {
        ctx->flags |= 1;
        wpa_notify(0, 0);
    }
    ctx->state = 5;
    return 0;
}

static int wpa_send_msg2_ccmp(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    u8 *tx = ctx->tx_frame;
    u16 key_info;
    u16 key_info_copy;
    u16 body_len;
    u16 key_info_be;
    u8 mic[48];
    int ret;

    sceKernelMemset(tx, 0, 137);
    tx[14] = 1;
    tx[15] = 3;
    body_len = bswap16(0x77u);
    tx[16] = (u8)body_len;
    tx[17] = (u8)(body_len >> 8);
    tx[18] = 0xFE;
    sceKernelMemset(&key_info, 0, 2);
    key_info = (u16)((key_info & ~7u) | 0x010Au);
    sceKernelMemcpy(&key_info_copy, &key_info, 2);
    key_info_be = bswap16(key_info_copy);
    tx[19] = (u8)key_info_be;
    tx[20] = (u8)(key_info_be >> 8);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemcpy(tx + 31, ctx->snonce, 32);
    key_info_copy = bswap16(0x18u);
    sceKernelMemcpy(tx + 111, &key_info_copy, 2);
    tx[113] = 0xDD;
    tx[114] = 22;
    sceKernelMemcpy(tx + 115, &g_wpa_oui_tag, 4);
    tx[119] = 1;
    tx[120] = 0;
    if ((ctx->bss_info[0x51] & 4) != 0)
        sceKernelMemcpy(tx + 121, &g_wpa_oui_tkip, 4);
    else
        sceKernelMemcpy(tx + 121, &g_wpa_oui_ccmp, 4);
    tx[125] = 1;
    tx[126] = 0;
    sceKernelMemcpy(tx + 127, &g_wpa_oui_ccmp, 4);
    tx[131] = 1;
    tx[132] = 0;
    if (ctx->auth_mode == 0)
        sceKernelMemcpy(tx + 133, &g_wpa_akm_psk, 4);
    else if (ctx->auth_mode == 1)
        sceKernelMemcpy(tx + 133, &g_wpa_akm_8021x, 4);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_sha1(ctx->kck_ptr, 16u, tx + 14, 123, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    ret = wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 3, 0x77u);
    ctx->state = 3;
    return ret;
}

static int wpa_send_msg4_ccmp(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    u8 *tx = ctx->tx_frame;
    u16 key_info;
    u16 key_info_copy;
    u16 body_len;
    u16 key_info_be;
    u8 mic[48];

    sceKernelMemset(tx, 0, 113);
    tx[14] = 1;
    tx[15] = 3;
    body_len = bswap16(0x5Fu);
    tx[16] = (u8)body_len;
    tx[17] = (u8)(body_len >> 8);
    tx[18] = 0xFE;
    sceKernelMemset(&key_info, 0, 2);
    key_info = (u16)((key_info & ~7u) | 0x010Au);
    sceKernelMemcpy(&key_info_copy, &key_info, 2);
    key_info_be = bswap16(key_info_copy);
    tx[19] = (u8)key_info_be;
    tx[20] = (u8)(key_info_be >> 8);
    sceKernelMemcpy(tx + 23, pkt + 23, 8);
    sceKernelMemset(tx + 21, 0, 2);
    sceKernelMemset(tx + 111, 0, 2);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_sha1(ctx->kck_ptr, 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 3, 0x5Fu);
    ctx->state = 4;
    return 4;
}

static int wpa_send_group2_ccmp(WpaSupplicantCtx *ctx, const u8 *pkt)
{
    u8 *tx = ctx->tx_frame;
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
    wpa_hmac_sha1(ctx->kck_ptr, 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);

    wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 3, 0x5Fu);
    if (ctx->alarm_id != 0) {
        sceKernelCancelAlarm(ctx->alarm_id);
        ctx->alarm_id = 0;
    }
    if ((ctx->flags & 1) == 0) {
        ctx->flags |= 1;
        wpa_notify(0, 0);
    }
    ctx->state = 5;
    return 0;
}

static int wpa_send_mic_err_tkip(WpaSupplicantCtx *ctx, int is_group)
{
    u8 *tx = ctx->tx_frame;
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
    wpa_inc_counter(ctx->replay_counter, 8);
    sceKernelMemcpy(tx + 23, ctx->replay_counter, 8);

    sceKernelMemset(mic, 0, 36);
    wpa_hmac_md5(ctx->kck_ptr, 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);
    return wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 3, 0x5Fu);
}

static int wpa_send_mic_err_ccmp(WpaSupplicantCtx *ctx, int is_group)
{
    u8 *tx = ctx->tx_frame;
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
    wpa_hmac_sha1(ctx->kck_ptr, 16u, tx + 14, 99, mic);
    sceKernelMemcpy(tx + 95, mic, 16);
    return wpa_eapol_send(ctx->bpf_fd, tx, ctx->sta_mac, ctx->bssid, 3, 0x5Fu);
}

static int wpa_handle_msg1(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    const BpfHdr *bh = (const BpfHdr *)ev->pkt;
    u8 *pkt = ev->pkt + bh->hdrlen;
    u16 eapol_len = (u16)((pkt[17] << 8) | pkt[16]);
    u16 key_info;
    u16 key_len;

    if (eapol_len < 0x5Fu)
        return 0;

    sceKernelMemcpy(ctx->replay_counter, pkt + 23, 8);
    if (pkt[18] != 0xFE)
        return 0;

    key_info = bswap16((u16)((pkt[20] << 8) | pkt[19]));
    if (((key_info & 7u) - 1u) < 2u && (key_info & 0x0DC8u) == 0x0088u) {
        key_len = bswap16((u16)((pkt[22] << 8) | pkt[21]));
        if ((key_info & 7u) == 1u) {
            if (ctx->cipher == 0 && key_len == 32u) {
                wpa_derive_ptk_tkip(ctx, pkt);
                wpa_send_msg2_tkip(ctx, pkt);
            }
        } else if ((key_info & 7u) == 2u) {
            if (ctx->cipher == 1 && key_len == 16u) {
                wpa_derive_ptk_ccmp(ctx, pkt);
                wpa_send_msg2_ccmp(ctx, pkt);
            }
        }
    }
    return 0;
}

static int wpa_handle_msg3(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    const BpfHdr *bh = (const BpfHdr *)ev->pkt;
    u8 *pkt = ev->pkt + bh->hdrlen;
    u16 eapol_len = (u16)((pkt[17] << 8) | pkt[16]);
    u16 key_info;
    u8 rx_mic[16];
    u8 calc_mic[48];

    if (eapol_len < 0x5Fu)
        return 0;

    key_info = bswap16((u16)((pkt[20] << 8) | pkt[19]));
    if (((key_info & 7u) - 1u) >= 2u) {
        if (ctx->auth_mode == 0)
            wpa_send_start(ctx);
        ctx->state = 2;
        return 0;
    }

    if ((key_info & 0x0D88u) != 0x0188u) {
        if ((key_info & 0x0DC8u) == 0x0088u)
            return wpa_handle_msg1(ctx, ev);
        if (ctx->auth_mode == 0)
            wpa_send_start(ctx);
        ctx->state = 2;
        return 0;
    }

    if ((key_info & 7u) == 1u) {
        sceKernelMemcpy(rx_mic, pkt + 95, 16);
        sceKernelMemset(pkt + 95, 0, 16);
        wpa_hmac_md5(ctx->kck_ptr, 16u, pkt + 14,
                     bswap16(eapol_len) + 4, calc_mic);
        if (wpa_memcmp(rx_mic, calc_mic, 16) < 0)
            return 0;
        if (wpa_max_bytes(pkt + 23, ctx->replay_counter, 8) == pkt + 23 &&
            ctx->cipher == 0 &&
            bswap16((u16)((pkt[22] << 8) | pkt[21])) == 32u) {
            wpa_send_msg4_tkip(ctx, pkt);
            wpa_notify_ptk();
        }
    } else if ((key_info & 7u) == 2u) {
        sceKernelMemcpy(rx_mic, pkt + 95, 16);
        sceKernelMemset(pkt + 95, 0, 16);
        wpa_hmac_sha1(ctx->kck_ptr, 16u, pkt + 14,
                      bswap16(eapol_len) + 4, calc_mic);
        if (wpa_memcmp(rx_mic, calc_mic, 16) < 0)
            return 0;
        if (wpa_max_bytes(pkt + 23, ctx->replay_counter, 8) == pkt + 23 &&
            ctx->cipher == 1 &&
            bswap16((u16)((pkt[22] << 8) | pkt[21])) == 16u) {
            wpa_send_msg4_ccmp(ctx, pkt);
            wpa_notify_ptk();
        }
    }

    sceKernelMemcpy(ctx->replay_counter, pkt + 23, 8);
    return 0;
}

static int wpa_handle_group(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    const BpfHdr *bh = (const BpfHdr *)ev->pkt;
    u8 *pkt = ev->pkt + bh->hdrlen;
    u16 eapol_len = (u16)((pkt[17] << 8) | pkt[16]);
    u16 key_info;
    u8 rx_mic[16];
    u8 calc_mic[48];
    u8 prev_gtk[32];
    u32 now[2];

    if (eapol_len < 0x5Fu)
        return 0;

    key_info = bswap16((u16)((pkt[20] << 8) | pkt[19]));
    if (((key_info & 7u) - 1u) >= 2u) {
        if (ctx->auth_mode == 0)
            wpa_send_start(ctx);
        ctx->state = 2;
        return 0;
    }

    if ((key_info & 0x0F88u) != 0x0380u) {
        if ((key_info & 0x0D88u) == 0x0188u)
            return wpa_handle_msg3(ctx, ev);
        if ((key_info & 0x0DC8u) == 0x0088u)
            return wpa_handle_msg1(ctx, ev);
        if (ctx->auth_mode == 0)
            wpa_send_start(ctx);
        ctx->state = 2;
        return 0;
    }

    if (sceKernelGetSystemTime(now) >= 0) {
        ctx->last_gtk_lo = now[0];
        ctx->last_gtk_hi = now[1];
    }

    if ((key_info & 7u) == 1u) {
        sceKernelMemcpy(rx_mic, pkt + 95, 16);
        sceKernelMemset(pkt + 95, 0, 16);
        wpa_hmac_md5(ctx->kck_ptr, 16u, pkt + 14, 131, calc_mic);
        if (wpa_memcmp(rx_mic, calc_mic, 16) < 0)
            return 0;
        if (wpa_max_bytes(pkt + 23, ctx->replay_counter, 8) == pkt + 23) {
            sceKernelMemcpy(prev_gtk, ctx->gtk, 32);
            wpa_decrypt_gtk_tkip(ctx, pkt);
            if (sceNet_lib_E0A81C7C(prev_gtk, ctx->gtk, 32) != 0)
                wpa_notify_gtk();
            if (ctx->cipher == 0 &&
                bswap16((u16)((pkt[22] << 8) | pkt[21])) == 32u)
                wpa_send_group2_tkip(ctx, pkt);
        }
    } else if ((key_info & 7u) == 2u) {
        sceKernelMemcpy(rx_mic, pkt + 95, 16);
        sceKernelMemset(pkt + 95, 0, 16);
        wpa_hmac_sha1(ctx->kck_ptr, 16u, pkt + 14,
                      bswap16(eapol_len) + 4, calc_mic);
        if (wpa_memcmp(rx_mic, calc_mic, 16) < 0)
            return 0;
        if (wpa_max_bytes(pkt + 23, ctx->replay_counter, 8) == pkt + 23) {
            u16 key_len;
            sceKernelMemcpy(prev_gtk, ctx->gtk, 32);
            wpa_decrypt_gtk_ccmp(ctx, pkt);
            if (sceNet_lib_E0A81C7C(prev_gtk, ctx->gtk, 32) != 0)
                wpa_notify_gtk();
            key_len = bswap16((u16)((pkt[22] << 8) | pkt[21]));
            if (ctx->cipher == 1 && (key_len == 16u || key_len == 32u))
                wpa_send_group2_ccmp(ctx, pkt);
        }
    }

    sceKernelMemcpy(ctx->replay_counter, pkt + 23, 8);
    return 0;
}

static int wpa_handle_mic_err(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    u32 now[2];
    int is_group = (int)ev->context;

    if (ctx->cipher != 0 || sceKernelGetSystemTime(now) < 0)
        return 0;

    if (ctx->last_gtk_lo != 0) {
        if (ctx->last_gtk_hi == now[1] && (now[0] - ctx->last_gtk_lo) <= 4999999u)
            return 0;
        if (now[1] == ctx->last_mic_err_hi - 1u)
            return 0;
    }

    if (ctx->cipher == 0)
        wpa_send_mic_err_tkip(ctx, is_group);
    else if (ctx->cipher == 1)
        wpa_send_mic_err_ccmp(ctx, is_group);

    if (ctx->last_mic_err_lo != 0) {
        if ((ctx->last_mic_err_hi == now[1] && (now[0] - ctx->last_mic_err_lo) <= 59999999u) ||
            now[1] == ctx->last_mic_err_hi - 1u) {
            wpa_notify(4, (int)0x80411082);
            return 0;
        }
    }
    sceKernelMemcpy(&ctx->last_mic_err_lo, now, 8);
    return 0;
}

static int wpa_alloc_event(WpaSupplicantCtx *ctx, WpaEvent **out)
{
    (void)ctx;
    *out = &g_wpa_event;
    return 0;
}

static int __attribute__((noinline))
wpa_free_event(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    (void)ctx;
    (void)ev;
    return 0;
}

static int wpa_state_noop(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    (void)ctx;
    (void)ev;
    return 0;
}

static int wpa_state_start(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    (void)ev;
    if (ctx->auth_mode == 0)
        wpa_send_start(ctx);
    ctx->state = 2;
    return 0;
}

static int wpa_state_timeout(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    (void)ev;
    wpa_notify(4, (int)0x80411081);
    ctx->state = 0;
    return -1;
}

static int wpa_state_stop(WpaSupplicantCtx *ctx, WpaEvent *ev)
{
    (void)ev;
    ctx->state = 0;
    return -1;
}

typedef int (*WpaHandler)(WpaSupplicantCtx *ctx, WpaEvent *ev);

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
    WpaSupplicantCtx *ctx = (WpaSupplicantCtx *)common;
    sceKernelSetEventFlag(ctx->evf, 4u);
    return 0;
}

static int wpa_get_event(WpaSupplicantCtx *ctx, WpaEvent **out)
{
    u32 bits;
    u8 *pkt;
    int pkt_len;
    int ret;
    WpaEvent *ev;

    if (sceKernelPollEventFlag(ctx->evf, 31u, 1u, &bits) >= 0) {
        if ((bits & 1u) != 0) {
            sceKernelClearEventFlag(ctx->evf, ~1u);
            wpa_alloc_event(ctx, out);
            (*out)->type = 1;
            return 0;
        }
        if ((bits & 2u) != 0) {
            sceKernelClearEventFlag(ctx->evf, ~2u);
            wpa_alloc_event(ctx, out);
            (*out)->type = 5;
            return 0;
        }
        if ((bits & 4u) != 0) {
            sceKernelClearEventFlag(ctx->evf, ~4u);
            wpa_alloc_event(ctx, out);
            (*out)->type = 3;
            return 0;
        }
        if ((bits & 8u) != 0) {
            sceKernelClearEventFlag(ctx->evf, ~8u);
            wpa_alloc_event(ctx, out);
            ev = *out;
            ev->type = 4;
            ev->context = (void *)0;
            return 0;
        }
        if ((bits & 16u) != 0) {
            sceKernelClearEventFlag(ctx->evf, ~16u);
            wpa_alloc_event(ctx, out);
            ev = *out;
            ev->type = 4;
            ev->context = (void *)1;
            return 0;
        }
        return 0;
    }

    ret = eapol_bpf_recv_wpa(ctx->bpf_fd, ctx->rx_buf,
                             ctx->rx_buf_size, ctx->sta_mac, &pkt, &pkt_len);
    if (ret < 0) {
        sceKernelDelayThread(1000000);
        return -1;
    }
    if (ret == 0)
        return -1;

    wpa_alloc_event(ctx, out);
    ev = *out;
    ev->type = 2;
    ev->context = ctx;
    ev->pkt = pkt;
    ev->pkt_len = pkt_len;
    return 0;
}

static int wpa_supplicant_thread(SceSize args, void *argp)
{
    WpaSupplicantCtx *ctx = NULL;
    u8 prng_out[112];
    WpaPrngSeed seed;
    u32 iter;
    WpaEvent *ev;

    do {
        if (args != 4u || argp == NULL)
            break;

        ctx = *(WpaSupplicantCtx **)argp;
        if (ctx->timeout_us != 0) {
            SceUID alarm = sceKernelSetAlarm(ctx->timeout_us, wpa_alarm_cb, ctx);
            if (alarm < 0)
                break;
            ctx->alarm_id = alarm;
        }

        sceKernelMemset(prng_out, 0, sizeof(prng_out));
        sceKernelMemset(&seed, 0, sizeof(seed));
        sceKernelMemcpy(seed.prefix, ctx->sta_mac, 6);
        sceKernelGetSystemTime(seed.prefix + 8);

        for (iter = 0; iter < 32u; iter++) {
            seed.counter = iter;
            wpa_prf(NULL, 0, "Init Counter", 12, (const u8 *)&seed, sizeof(seed), prng_out, 32);
        }
        sceKernelMemcpy(ctx->prng_counter, prng_out, 32);

        ctx->state = 1;
        sceKernelSetEventFlag(ctx->evf, 1u);

        for (;;) {
            int r;
            if (wpa_get_event(ctx, &ev) < 0)
                continue;
            if ((u32)ev->type >= 6u) {
                wpa_free_event(ctx, ev);
                continue;
            }
            r = g_wpa_state_table[ctx->state][ev->type](ctx, ev);
            wpa_free_event(ctx, ev);
            if (r < 0)
                break;
        }
    } while (0);

    wpa_notify(1, 0);
    if (ctx != NULL && ctx->alarm_id != 0)
        sceKernelCancelAlarm(ctx->alarm_id);
    sceKernelExitThread(0);
    return 0;
}

static int wpa_set_config_impl(const WpaConfig *cfg)
{
    u8 pmk_tmp[64];
    int auth = cfg->auth_mode;
    int cipher = cfg->cipher;

    sceKernelMemset(&g_wpa_ctx, 0, sizeof(g_wpa_ctx));
    sceKernelMemcpy(&g_wpa_ctx.config, cfg, sizeof(WpaConfig));
    g_wpa_ctx.initialized = 1;

    if (auth == 0) {
        const u8 *pmk_src;
        if (cfg->psk_type == 1) {
            sceKernelMemcpy(g_wpa_ctx.passphrase, cfg->key_material, 64);
            wpa_pbkdf2_sha1(g_wpa_ctx.passphrase,
                            g_wpa_ctx.config.ssid, g_wpa_ctx.config.ssid_len, pmk_tmp);
            pmk_src = pmk_tmp;
        } else {
            pmk_src = cfg->key_material;
        }
        sceKernelMemcpy(g_wpa_ctx.pmk, pmk_src, 32);
        g_wpa_ctx.auth_mode = 0;
        g_wpa_ctx.psk_passphrase_type = cfg->psk_type;
    } else if (auth == 1) {
        sceKernelMemcpy(g_wpa_ctx.pmk, &cfg->psk_type, 32);
        g_wpa_ctx.auth_mode = 1;
    }

    if (cipher == 0) {
        g_wpa_ctx.cipher = 0;
    } else if (cipher == 1) {
        g_wpa_ctx.cipher = 1;
    } else {
        g_wpa_ctx.initialized = 0;
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
    WpaSupplicantCtx *ctx_ptr = &g_wpa_ctx;

    (void)ifname;

    if (g_wpa_ctx.initialized != 0) {
        evf = sceKernelCreateEventFlag("SceNetWpaSupplicantEvf", 0, 0, NULL);
        ret = evf;
        if (evf >= 0) {
            g_wpa_ctx.evf = evf;
            sceKernelMemcpy(g_wpa_ctx.bss_info, bss, 92);
            sceKernelMemcpy(g_wpa_ctx.bssid, g_wpa_ctx.bss_info, 6);
            g_wpa_ctx.callback = cb;
            g_wpa_ctx.timeout_us = (u32)timeout;

            if (g_wpa_ctx.config.auth_mode == 0) {
                u8 bss_ssid_len = g_wpa_ctx.bss_info[7];
                if (bss_ssid_len != g_wpa_ctx.config.ssid_len ||
                    sceNet_lib_E0A81C7C(g_wpa_ctx.bss_info + 8,
                                        g_wpa_ctx.config.ssid, bss_ssid_len) != 0) {
                    wpa_pbkdf2_sha1(g_wpa_ctx.passphrase,
                                    g_wpa_ctx.bss_info + 8, bss_ssid_len, pmk_tmp);
                    sceKernelMemcpy(g_wpa_ctx.pmk, pmk_tmp, 32);
                }
            }

            thid = sceKernelCreateThread("SceNetWpaSupplicant", wpa_supplicant_thread,
                                         prio, stack, 0, NULL);
            ret = thid;
            if (thid >= 0) {
                g_wpa_ctx.thid = thid;
                ret = sceKernelStartThread(thid, 4, &ctx_ptr);
                if (ret >= 0)
                    return 0;
            }
        }
    }

    if (g_wpa_ctx.bpf_fd != 0) {
        eapol_bpf_close(g_wpa_ctx.bpf_fd);
        g_wpa_ctx.bpf_fd = 0;
    }
    if (g_wpa_ctx.evf != 0) {
        sceKernelDeleteEventFlag(g_wpa_ctx.evf);
        g_wpa_ctx.evf = 0;
    }
    if (g_wpa_ctx.thid != 0) {
        sceKernelDeleteThread(g_wpa_ctx.thid);
        g_wpa_ctx.thid = 0;
    }
    return ret;
}

static int wpa_stop_impl(void)
{
    u32 timeout = 5000000u;
    SceUID thid = g_wpa_ctx.thid;
    SceUID evf = g_wpa_ctx.evf;

    if (thid != 0) {
        if (sceKernelSetEventFlag(evf, 2u) < 0 ||
            sceKernelWaitThreadEnd(thid, &timeout) < 0) {
            sceKernelTerminateThread(thid);
        }
        sceKernelDeleteThread(thid);
        sceNetFreeThreadinfo(thid);
    }
    g_wpa_ctx.thid = 0;

    if (g_wpa_ctx.bpf_fd != 0) {
        eapol_bpf_close(g_wpa_ctx.bpf_fd);
        g_wpa_ctx.bpf_fd = 0;
    }
    if (evf != 0) {
        sceKernelDeleteEventFlag(evf);
        g_wpa_ctx.evf = 0;
    }
    g_wpa_ctx.initialized = 0;
    g_wpa_ctx.callback = NULL;
    return 0;
}

int wpa_set_config(void *cfg)
{
    return wpa_set_config_impl((const WpaConfig *)cfg);
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
    SceUID evf = g_wpa_ctx.evf;
    if (evf != 0)
        sceKernelSetEventFlag(evf, 8u);
    return 0;
}

int wpa_notify_mic_err_multicast(void)
{
    SceUID evf = g_wpa_ctx.evf;
    if (evf != 0)
        sceKernelSetEventFlag(evf, 16u);
    return 0;
}
