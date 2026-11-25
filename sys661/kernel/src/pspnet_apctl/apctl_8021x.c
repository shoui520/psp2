#include "apctl_internal.h"

static const u8 g_eapol_pae_mac[6] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x03 };

static BpfInsn g_eap8021x_bpf_insns[10] = {
    { 0x0028, 0, 0, 0x0000000C },
    { 0x0015, 0, 7, 0x0000888E },
    { 0x0030, 0, 0, 0x0000000F },
    { 0x0015, 0, 5, 0x00000000 },
    { 0x0020, 0, 0, 0x00000000 },
    { 0x0015, 0, 3, 0x00000000 },
    { 0x0028, 0, 0, 0x00000004 },
    { 0x0015, 0, 1, 0x00000000 },
    { 0x0006, 0, 0, 0xFFFFFFFF },
    { 0x0006, 0, 0, 0x00000000 },
};

static BpfProgram g_eap8021x_bpf_prog = { 10, g_eap8021x_bpf_insns };

/* 802.1X EAPOL BPF helpers (shared with apctl_wpa.c) */

int eapol_bpf_open(const char *ifname, BpfProgram *prog, const u8 *mac,
                   u8 type_filter, int bufsize, u32 timeout_us)
{
    int fd;
    int ret;
    int opt_val;
    int req_bufsize;
    u32 mac_hi;
    u16 mac_lo;
    u32 tv[2];
    char ifreq[32];

    req_bufsize = bufsize;
    fd = sceNet_lib_3B617AA0();
    if (fd < 0)
        return (int)0x80410E80;

    opt_val = 1;
    if (sceNet_lib_E1F4696F(fd, 0x80044270u, &opt_val) < 0) {
        ret = (int)0x80410E81;
    } else if (sceNet_lib_E1F4696F(fd, 0xC0044266u, &req_bufsize) < 0) {
        ret = (int)0x80410E82;
    } else {
        sceKernelMemset(ifreq, 0, 32);
        sceNet_lib_B5CE388A(ifreq, ifname, 16);
        ret = (int)0x80410E83;
        if (sceNet_lib_E1F4696F(fd, 0x8020426Cu, ifreq) >= 0) {
            prog->insns[3].k = type_filter;
            sceKernelMemcpy(&mac_hi, mac, 4);
            sceKernelMemcpy(&mac_lo, mac + 4, 2);
            prog->insns[7].k = bswap16(mac_lo);
            prog->insns[5].k = bswap32(mac_hi);
            ret = (int)0x80410E84;
            if (sceNet_lib_E1F4696F(fd, 0x80084267u, prog) >= 0) {
                if (timeout_us == 0)
                    return fd;
                tv[0] = timeout_us / 1000000u;
                tv[1] = timeout_us % 1000000u;
                ret = (int)0x80410E85;
                if (sceNet_lib_E1F4696F(fd, 0x8008426Du, tv) >= 0)
                    return fd;
            }
        }
    }

    sceNet_lib_DB88F458(fd);
    return ret;
}

int eapol_bpf_send(int fd, u8 *frame, const u8 *src_mac, const u8 *dst_mac,
                   int write_hdr, u8 ver, u8 type, u16 len)
{
    const u8 *dst = (dst_mac != NULL) ? dst_mac : g_eapol_pae_mac;
    u16 be_val;
    int ret;

    sceKernelMemcpy(frame, dst, 6);
    sceKernelMemcpy(frame + 6, src_mac, 6);
    be_val = bswap16(0x888Eu);
    sceKernelMemcpy(frame + 12, &be_val, 2);
    if (write_hdr > 0) {
        sceKernelMemcpy(frame + 14, &ver, 1);
        sceKernelMemcpy(frame + 15, &type, 1);
        be_val = bswap16(len);
        sceKernelMemcpy(frame + 16, &be_val, 2);
    }
    ret = sceNet_lib_C431A214(fd, frame, (u32)len + 18u);
    if (ret < 0)
        return (int)0x80410E86;
    return ret;
}

int eapol_bpf_parse(int fd, u8 *pkt, u32 pkt_len, const u8 *local_mac,
                    int check_ver, u8 *out_type, u8 **out_body,
                    u8 **out_next, int *out_valid)
{
    u32 caplen;
    u16 hdrlen;
    u8 *next_pkt;
    u8 *eth;
    u16 ethertype;
    u16 body_len;

    (void)fd;
    (void)check_ver;

    if (pkt_len < 20u) {
        *out_next = NULL;
        return 0;
    }

    caplen = *(u32 *)(pkt + 8);
    hdrlen = *(u16 *)(pkt + 16);
    next_pkt = pkt + ((hdrlen + caplen + 3u) & ~3u);

    if (caplen < 18u) {
        *out_next = next_pkt;
        *out_valid = 0;
        return 0;
    }

    eth = pkt + hdrlen;
    if (sceNet_lib_E0A81C7C(eth, g_eapol_pae_mac, 6) != 0 &&
        sceNet_lib_E0A81C7C(eth, local_mac, 6) != 0) {
        *out_next = next_pkt;
        *out_valid = 0;
        return 0;
    }

    if (sceNet_lib_E0A81C7C(eth + 6, local_mac, 6) != 0) {
        sceKernelMemcpy(&ethertype, eth + 12, 2);
        if (bswap16(ethertype) == 0x888Eu) {
            u8 eapol_type = eth[15];
            if (eapol_type < 4u) {
                *out_type = eapol_type;
                sceKernelMemcpy(&body_len, eth + 16, 2);
                body_len = bswap16(body_len);
                if (caplen - 18u >= body_len) {
                    *out_body = eth + 18;
                    *out_valid = 1;
                    *out_next = next_pkt;
                    return body_len;
                }
            }
        }
    }

    *out_next = next_pkt;
    *out_valid = 0;
    return 0;
}

int eapol_bpf_close(int fd)
{
    sceNet_lib_DB88F458(fd);
    return 0;
}

int eapol_bpf_read(int fd, void *buf, u32 len)
{
    int ret = sceNet_lib_B6FC0A5B(fd, buf, len);
    if (ret < 0)
        return (int)0x80410E87;
    return ret;
}

void apctl_hmac_sha1(const u8 *data, int data_len, u8 *key, u32 key_len, u8 *out)
{
    u8 sha_ctx[96];
    u8 key_ctx[96];
    u8 ipad[80];
    u8 opad[80];
    int i;

    if (key_len > 64u) {
        sceKernelUtilsSha1BlockInit(key_ctx);
        sceKernelUtilsSha1BlockUpdate(key_ctx, key, key_len);
        sceKernelUtilsSha1BlockResult(key_ctx, key);
        key_len = 20;
    }

    sceKernelMemset(ipad, 0, 65);
    sceKernelMemset(opad, 0, 65);
    sceKernelMemcpy(ipad, key, key_len);
    sceKernelMemcpy(opad, key, key_len);

    for (i = 0; i < 64; i++) {
        ipad[i] ^= 0x36;
        opad[i] ^= 0x5C;
    }

    sceKernelUtilsSha1BlockInit(sha_ctx);
    sceKernelUtilsSha1BlockUpdate(sha_ctx, ipad, 64);
    sceKernelUtilsSha1BlockUpdate(sha_ctx, data, (u32)data_len);
    sceKernelUtilsSha1BlockResult(sha_ctx, out);

    sceKernelUtilsSha1BlockInit(sha_ctx);
    sceKernelUtilsSha1BlockUpdate(sha_ctx, opad, 64);
    sceKernelUtilsSha1BlockUpdate(sha_ctx, out, 20);
    sceKernelUtilsSha1BlockResult(sha_ctx, out);
}

void apctl_hmac_md5(const u8 *data, int data_len, u8 *key, u32 key_len, u8 *out)
{
    u8 md5_ctx[96];
    u8 key_ctx[96];
    u8 ipad[80];
    u8 opad[80];
    int i;

    if (key_len > 64u) {
        sceKernelUtilsMd5BlockInit(key_ctx);
        sceKernelUtilsMd5BlockUpdate(key_ctx, key, key_len);
        sceKernelUtilsMd5BlockResult(key_ctx, key);
        key_len = 16;
    }

    sceKernelMemset(ipad, 0, 65);
    sceKernelMemset(opad, 0, 65);
    sceKernelMemcpy(ipad, key, key_len);
    sceKernelMemcpy(opad, key, key_len);

    for (i = 0; i < 64; i++) {
        ipad[i] ^= 0x36;
        opad[i] ^= 0x5C;
    }

    sceKernelUtilsMd5BlockInit(md5_ctx);
    sceKernelUtilsMd5BlockUpdate(md5_ctx, ipad, 64);
    sceKernelUtilsMd5BlockUpdate(md5_ctx, data, (u32)data_len);
    sceKernelUtilsMd5BlockResult(md5_ctx, out);

    sceKernelUtilsMd5BlockInit(md5_ctx);
    sceKernelUtilsMd5BlockUpdate(md5_ctx, opad, 64);
    sceKernelUtilsMd5BlockUpdate(md5_ctx, out, 16);
    sceKernelUtilsMd5BlockResult(md5_ctx, out);
}

int eapol_bpf_recv_wpa(int fd, u8 *buf, u32 bufsize, const u8 *mac,
                       u8 **out_pkt, int *out_len)
{
    int read_len;
    u8 *cur;
    int rem;
    u8 eapol_type;
    u8 *body;
    u8 *next_pkt;
    int valid;
    int ret;

    read_len = eapol_bpf_read(fd, buf, bufsize);
    if (read_len <= 0)
        return read_len;

    cur = buf;
    rem = read_len;
    while (rem > 0) {
        ret = eapol_bpf_parse(fd, cur, (u32)rem, mac, 1, &eapol_type, &body, &next_pkt, &valid);
        if (ret < 0 || rem < ret)
            return ret;
        if (next_pkt == NULL || rem < (int)(next_pkt - cur)) {
            *out_pkt = cur;
            *out_len = ret;
            return ret;
        }
        rem -= (int)(next_pkt - *out_pkt);
        cur = next_pkt;
        *out_len = 0;
        *out_pkt = next_pkt;
    }
    return ret;
}

/* =========================================================================
 * IEEE 802.1X / EAP-MD5 Supplicant State Machine
 * ========================================================================= */

typedef struct Eap8021xPktNode {
    struct Eap8021xPktNode *next;
    int in_use;
    int len;
    u8 data[1500];
} Eap8021xPktNode; /* 1512 bytes (378 words) */

typedef struct EapMethodOps {
    u8 type;
    u8 vendor_id[3];
    u32 vendor_type;
    int enabled;
    int (*init)(void *ctx);
    int (*check)(void *ctx);
    int (*process)(void *ctx);
    int (*build_resp)(void *ctx);
    int reserved;
} EapMethodOps; /* 32 bytes */

typedef struct Eap8021xContext {
    char ifname[16];          /* 0x0000 */
    u8 local_mac[6];          /* 0x0010 */
    u8 pad16[2];              /* 0x0016 */
    int bpf_fd;               /* 0x0018 (word 6) */
    u8 tx_frame[1500];        /* 0x001C (word 7..381) */
    u8 rx_buf[1504];          /* 0x05F8 (word 382..757) -> note node pool starts at word 757 (0xBD4) */
} Eap8021xContext;

/*
 * Flat word-indexed state buffer matching the original 14148-byte layout
 * (0x145B4 .. 0x17CF8) so every cross-machine flag offset is exact.
 */
static u8 g_eap8021x_ctx_buf[14148];
static int g_eap8021x_started = 0;

/* MD5 method state (0x17D00 .. 0x17D70) */
static u8 g_eap_md5_ctx[96];
static u8 g_eap_md5_digest[16];

#define CTX_W(c, idx)   (((int *)(c))[(idx)])
#define CTX_U8(c, off)  (((u8 *)(c))[(off)])
#define CTX_U16(c, off) (*(u16 *)(((u8 *)(c)) + (off)))
#define CTX_U32(c, off) (*(u32 *)(((u8 *)(c)) + (off)))
#define CTX_S32(c, off) (*(int *)(((u8 *)(c)) + (off)))

static void eap8021x_notify(void *ctx, int event, int err)
{
    void (*cb)(int, int) = *(void (**)(int, int))((u8 *)ctx + 9100);
    if (cb != NULL)
        cb(event, err);
}

static int *eap8021x_dequeue_pkt(int *ctx)
{
    int *head = (int *)ctx[2269];
    if ((int *)ctx[2270] == head)
        ctx[2270] = 0;
    if (head != NULL)
        ctx[2269] = *head;
    ctx[2276] = 0;
    return head;
}

static int eap8021x_tick_timers(int *ctx)
{
    if (ctx[2344] != 0) ctx[2344]--;
    if (ctx[2345] != 0) ctx[2345]--;
    if (ctx[2346] != 0) ctx[2346]--;
    if (ctx[2347] != 0) ctx[2347]--;
    if (ctx[3134] != 0) ctx[3134]--;
    return 0;
}

static u32 eap8021x_alarm_cb(void *common)
{
    eap8021x_tick_timers((int *)common);
    return 1000000;
}

/* Forward declarations for state machines */
static int pae_init(int *ctx);
static int pae_reset_done(int *ctx);
static int pae_is_init_done(int *ctx);
static int backend_init(int *ctx);
static int backend_reset_done(int *ctx);
static int backend_is_init_done(int *ctx);
static int eap_init(int *ctx);
static int eap_reset_done(int *ctx);
static int eap_is_init_done(int *ctx);
static int eap_md5_register(u8 *ctx);
static int eap_parse_packet(u8 *ctx);
static int eap_find_method(u8 *ctx, const u8 *req_type, int *out_idx);
static int eap_build_nak(u8 *ctx, int method_idx, int has_pref);

static int eap8021x_init_machines(int *ctx)
{
    sceKernelMemset(ctx + 2344, 0, 84);
    ctx[2353] = 2; /* portControl = Auto */
    ctx[2354] = 1; /* portEnabled = 1 */
    pae_init(ctx);
    backend_init(ctx);
    eap_init(ctx);
    pae_reset_done(ctx);
    backend_reset_done(ctx);
    eap_reset_done(ctx);
    ctx[2352] = 1; /* initialize = 1 */
    return 0;
}

static int eap8021x_check_init_complete(int *ctx)
{
    if (pae_is_init_done(ctx) &&
        backend_is_init_done(ctx) &&
        eap_is_init_done(ctx)) {
        ctx[2352] = 0;
        return 1;
    }
    return 0;
}

/* --- PAE Supplicant state machine --- */

static int pae_init(int *ctx)
{
    sceKernelMemset(ctx + 2365, 0, 44);
    ctx[2372] = 3; /* maxStart */
    ctx[2371] = 5; /* startPeriod */
    ctx[2370] = 5; /* heldPeriod */
    return 0;
}

static int pae_reset_done(int *ctx)
{
    CTX_S32(ctx, 9496) = 0;
    return 0;
}

static int pae_is_init_done(int *ctx)
{
    return CTX_S32(ctx, 9496) == 1;
}

static int pae_enter_logoff(int *ctx)
{
    eapol_bpf_send(ctx[6], (u8 *)(ctx + 7), (const u8 *)(ctx + 4),
                   (const u8 *)(ctx + 2278), 1, 1, 2, 0);
    ctx[2366] = 1; /* logoffSent */
    ctx[2359] = 0; /* suppPortStatus = Unauthorized */
    return 0;
}

static int pae_enter_disconnected(int *ctx)
{
    ctx[2366] = 0;
    ctx[2368] = 0; /* startCount */
    ctx[2367] = 2; /* sPortMode = Auto */
    ctx[2357] = 1; /* suppAbort */
    ctx[2359] = 0;
    return 0;
}

static int pae_enter_connecting(int *ctx)
{
    ctx[2347] = ctx[2371]; /* startWhen = startPeriod */
    ctx[2368]++;
    ctx[2349] = 0;         /* eapolEap = 0 */
    eapol_bpf_send(ctx[6], (u8 *)(ctx + 7), (const u8 *)(ctx + 4),
                   (const u8 *)(ctx + 2278), 1, 1, 1, 0);
    return 0;
}

static int pae_enter_authenticating(int *ctx)
{
    ctx[2368] = 0;
    ctx[2360] = 1; /* suppStart */
    ctx[2361] = 0; /* suppSuccess */
    ctx[2358] = 0; /* suppFail */
    ctx[2362] = 0; /* suppTimeout */
    ctx[2364] = 0; /* keyRun */
    ctx[2363] = 0; /* keyDone */
    return 0;
}

static int pae_enter_held(int *ctx)
{
    ctx[2359] = 0;
    ctx[2345] = ctx[2370]; /* heldWhile = heldPeriod */
    return 0;
}

static int pae_enter_authenticated(int *ctx)
{
    ctx[2359] = 1;
    return 0;
}

static int pae_enter_restart(int *ctx)
{
    ctx[2365] = 1; /* eapRestart */
    return 0;
}

static int pae_enter_s_force_auth(int *ctx)
{
    ctx[2367] = 1;
    ctx[2359] = 1;
    return 0;
}

static int pae_enter_s_force_unauth(int *ctx)
{
    ctx[2359] = 0;
    ctx[2367] = 0;
    eapol_bpf_send(ctx[6], (u8 *)(ctx + 7), (const u8 *)(ctx + 4),
                   (const u8 *)(ctx + 2278), 1, 1, 2, 0);
    return 0;
}

static int pae_state_logoff(int *ctx)
{
    if (ctx[2369] == 0) {
        ctx[2373] = 1;
        pae_enter_disconnected(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int pae_state_disconnected(int *ctx)
{
    ctx[2373] = 2;
    pae_enter_connecting(ctx);
    ctx[2274] = 1;
    return 0;
}

static int pae_state_connecting(int *ctx)
{
    int start_when = ctx[2347];
    if (start_when == 0) {
        if ((u32)ctx[2368] < (u32)ctx[2372]) {
            ctx[2373] = 2;
            pae_enter_connecting(ctx);
            ctx[2274] = 1;
            return 0;
        }
        if (ctx[2356] == 1) {
            ctx[2373] = 5;
            pae_enter_authenticated(ctx);
            ctx[2274] = 1;
            return 0;
        }
    }
    if (ctx[2350] == 1 || ctx[2348] == 1) {
        ctx[2373] = 3;
        pae_enter_authenticating(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2349] == 1) {
        ctx[2373] = 6;
        pae_enter_restart(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (start_when == 0 && (u32)ctx[2368] >= (u32)ctx[2372] && ctx[2356] == 0) {
        ctx[2373] = 4;
        pae_enter_held(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int pae_state_authenticating(int *ctx)
{
    if (ctx[2361] == 1 && ctx[2356] == 1) {
        ctx[2373] = 5;
        pae_enter_authenticated(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2358] == 1 || (ctx[2363] == 1 && ctx[2356] == 0)) {
        ctx[2373] = 4;
        pae_enter_held(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2362] == 1) {
        ctx[2373] = 2;
        pae_enter_connecting(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int pae_state_held(int *ctx)
{
    if (ctx[2345] == 0) {
        ctx[2373] = 2;
        pae_enter_connecting(ctx);
        ctx[2274] = 1;
    } else if (ctx[2349] == 1) {
        ctx[2373] = 6;
        pae_enter_restart(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int pae_state_authenticated(int *ctx)
{
    if (ctx[2349] == 1 && ctx[2356] == 1) {
        ctx[2375] = 1;
        eap8021x_notify(ctx, 2, 0);
        ctx[2373] = 6;
        pae_enter_restart(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2356] == 0) {
        ctx[2373] = 1;
        pae_enter_disconnected(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int pae_state_restart(int *ctx)
{
    if (ctx[2365] == 0) {
        ctx[2373] = 3;
        pae_enter_authenticating(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int pae_state_s_force_auth(int *ctx) { (void)ctx; return 0; }
static int pae_state_s_force_unauth(int *ctx) { (void)ctx; return 0; }

static int (*const g_pae_state_table[9])(int *) = {
    pae_state_logoff,
    pae_state_disconnected,
    pae_state_connecting,
    pae_state_authenticating,
    pae_state_held,
    pae_state_authenticated,
    pae_state_restart,
    pae_state_s_force_auth,
    pae_state_s_force_unauth,
};

static int pae_step(int *ctx)
{
    int port_ctrl;

    if (ctx[2369] == 1 && ctx[2366] == 0 && ctx[2352] != 1 && ctx[2354] != 0) {
        ctx[2373] = 0;
        pae_enter_logoff(ctx);
        ctx[2274] = 1;
        return 0;
    }

    port_ctrl = ctx[2353];
    if (port_ctrl == 1 && ctx[2367] != 1 && ctx[2352] != 1 && ctx[2354] != 0) {
        ctx[2373] = 7;
        pae_enter_s_force_auth(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (port_ctrl == 0 && ctx[2367] != 0 && ctx[2352] != 1 && ctx[2354] != 0) {
        ctx[2373] = 8;
        pae_enter_s_force_unauth(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if ((port_ctrl == 2 && ctx[2367] != 2) || ctx[2352] == 1 || ctx[2354] == 0) {
        ctx[2373] = 1;
        pae_enter_disconnected(ctx);
        ctx[2274] = 1;
        ctx[2374] = 1;
        eap8021x_check_init_complete(ctx);
        return 0;
    }

    g_pae_state_table[ctx[2373]](ctx);
    return 0;
}

/* --- Supplicant Backend state machine --- */

static int backend_init(int *ctx)
{
    sceKernelMemset(ctx + 2376, 0, 3032);
    ctx[3131] = 5; /* authPeriod */
    return 0;
}

static int backend_reset_done(int *ctx)
{
    ctx[3133] = 0;
    return 0;
}

static int backend_is_init_done(int *ctx)
{
    return ctx[3133] == 1;
}

static int backend_enter_request(int *ctx)
{
    int *pkt = eap8021x_dequeue_pkt(ctx);
    if (pkt != NULL) {
        ctx[2344] = 0; /* authWhile = 0 */
        sceKernelMemcpy(ctx + 2379, pkt + 3, (SceSize)pkt[2]);
        ctx[2754] = pkt[2]; /* eapReqDataLen */
        pkt[1] = 0;
        ctx[2377] = 1; /* eapReq = 1 */
    }
    return 0;
}

static int backend_enter_response(int *ctx)
{
    sceKernelMemcpy((u8 *)ctx + 46, ctx + 2755, (SceSize)ctx[3130]);
    eapol_bpf_send(ctx[6], (u8 *)(ctx + 7), (const u8 *)(ctx + 4),
                   (const u8 *)(ctx + 2278), 1, 1, 0, (u16)ctx[3130]);
    ctx[2378] = 0; /* eapResp = 0 */
    return 0;
}

static int backend_enter_success(int *ctx)
{
    int ev = (ctx[2375] == 1) ? 3 : 0;
    ctx[2361] = 1; /* suppSuccess */
    ctx[2364] = 1; /* keyRun */
    eap8021x_notify(ctx, ev, 0);
    ctx[2375] = 0;
    return 0;
}

static int backend_enter_eap_success(u8 *ctx)
{
    CTX_S32(ctx, 9400) = 1;
    return 0;
}

static int backend_enter_fail(u8 *ctx)
{
    CTX_S32(ctx, 9432) = 1;
    eap8021x_notify(ctx, 1, (int)0x80410F01);
    return 0;
}

static int backend_enter_timeout(u8 *ctx)
{
    CTX_S32(ctx, 9448) = 1;
    return 0;
}

static int backend_enter_idle(u8 *ctx)
{
    CTX_S32(ctx, 9440) = 0;
    CTX_S32(ctx, 9396) = 0;
    return 0;
}

static int backend_noop(int *ctx) { (void)ctx; return 0; }

static int backend_enter_initialize(u8 *ctx)
{
    CTX_S32(ctx, 9428) = 0; /* suppAbort = 0 */
    return 0;
}

static int backend_enter_receive(int *ctx)
{
    ctx[2376] = 0;         /* eapNoResp = 0 */
    ctx[2344] = ctx[3131]; /* authWhile = authPeriod */
    ctx[2349] = 0;         /* eapolEap = 0 */
    return 0;
}

static int backend_state_request(int *ctx)
{
    if (ctx[2348] == 1) {
        ctx[3132] = 3;
        backend_enter_fail((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2376] == 1) {
        ctx[3132] = 7;
        backend_enter_receive(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2378] == 1) {
        ctx[3132] = 1;
        backend_enter_response(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2350] == 1) {
        ctx[3132] = 2;
        backend_enter_success(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int backend_state_response(int *ctx)
{
    ctx[3132] = 7;
    backend_enter_receive(ctx);
    ctx[2274] = 1;
    return 0;
}

static int backend_state_to_idle(int *ctx)
{
    ctx[3132] = 5;
    backend_enter_idle((u8 *)ctx);
    ctx[2274] = 1;
    return 0;
}

static int backend_state_idle(int *ctx)
{
    if (ctx[2348] == 1 && ctx[2360] == 1) {
        ctx[3132] = 3;
        backend_enter_fail((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2349] == 1 && ctx[2360] == 1) {
        ctx[3132] = 0;
        backend_enter_request(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2350] == 1 && ctx[2360] == 1) {
        ctx[3132] = 2;
        backend_enter_success(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int backend_state_receive(int *ctx)
{
    if (ctx[2349] == 1) {
        ctx[3132] = 0;
        backend_enter_request(ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2348] == 1) {
        ctx[3132] = 3;
        backend_enter_fail((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2344] == 0) {
        ctx[3132] = 4;
        backend_enter_timeout((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2350] == 1) {
        ctx[3132] = 2;
        backend_enter_success(ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int (*const g_backend_state_table[8])(int *) = {
    backend_state_request,
    backend_state_response,
    backend_state_to_idle,
    backend_state_to_idle,
    backend_state_to_idle,
    backend_state_idle,
    backend_state_to_idle,
    backend_state_receive,
};

static int backend_step(int *ctx)
{
    if (ctx[2352] == 1 || ctx[2357] == 1) {
        ctx[3132] = 6;
        backend_enter_initialize((u8 *)ctx);
        ctx[3133] = 1;
        ctx[2274] = 1;
        eap8021x_check_init_complete(ctx);
    } else {
        g_backend_state_table[ctx[3132]](ctx);
    }
    return 0;
}

/* --- EAP Peer state machine (RFC 4137) --- */

static int eap_init(int *ctx)
{
    sceKernelMemset((u8 *)ctx + 12536, 0, 1612);
    CTX_S32(ctx, 12552) = 10; /* clientTimeout */
    eap_md5_register((u8 *)ctx);
    return 0;
}

static int eap_reset_done(int *ctx) { (void)ctx; return 0; }
static int eap_is_init_done(int *ctx) { (void)ctx; return 1; }

static int eap_enter_initialize(u8 *ctx)
{
    CTX_S32(ctx, 12548) = 1; /* allowNotifications */
    CTX_U8(ctx, 12556) = 0;  /* selectedMethod = NONE */
    CTX_S32(ctx, 12536) = CTX_S32(ctx, 12552); /* idleWhile = clientTimeout */
    CTX_U16(ctx, 12568) = 256; /* lastId = NONE */
    CTX_S32(ctx, 9392) = 0;  /* eapFail = 0 */
    CTX_S32(ctx, 9400) = 0;  /* eapSuccess = 0 */
    CTX_S32(ctx, 12564) = 0; /* methodState = NONE */
    CTX_S32(ctx, 9460) = 0;  /* eapRestart = 0 */
    CTX_S32(ctx, 14076) = 0; /* decision = FAIL */
    return 0;
}

static int eap_build_resp_header(u8 *ctx, u8 type, u16 body_len)
{
    u16 total = (u16)(body_len + 5);
    u16 be_len = bswap16(total);
    ctx[11020] = 2; /* EAP Response */
    ctx[11021] = ctx[14092]; /* reqId */
    ctx[11024] = type;
    CTX_S32(ctx, 12520) = total;
    sceKernelMemcpy(ctx + 11022, &be_len, 2);
    return 0;
}

static int eap_enter_disabled(u8 *ctx) { (void)ctx; return 0; }

static int eap_enter_received(u8 *ctx)
{
    eap_parse_packet(ctx);
    return 0;
}

static int eap_enter_get_method(u8 *ctx)
{
    int pref_idx = 0;
    int idx = eap_find_method(ctx, ctx + 14093, &pref_idx);
    if (idx < 0) {
        if (idx == (int)0x80410F81)
            eap_build_nak(ctx, 0, 0);
        else
            eap_build_nak(ctx, pref_idx, 1);
    } else {
        EapMethodOps *ops = (EapMethodOps *)(ctx + 14116 + 32 * idx);
        sceKernelMemcpy(ctx + 12556, ctx + 14093, 8);
        CTX_S32(ctx, 12564) = 1; /* methodState = INIT */
        ops->init(ctx);
    }
    return 0;
}

static int eap_enter_method(u8 *ctx)
{
    int idx = eap_find_method(ctx, ctx + 14093, NULL);
    EapMethodOps *ops = (EapMethodOps *)(ctx + 14116 + 32 * idx);
    int ignore = ops->check(ctx);
    CTX_S32(ctx, 14104) = ignore;
    if (ignore == 0) {
        ops->process(ctx);
        ops->build_resp(ctx);
    }
    return 0;
}

static int eap_enter_send_response(u8 *ctx)
{
    int len = CTX_S32(ctx, 12520);
    CTX_U16(ctx, 12568) = ctx[14092]; /* lastId = reqId */
    sceKernelMemcpy(ctx + 12570, ctx + 11020, (SceSize)len);
    CTX_S32(ctx, 14072) = len;
    CTX_S32(ctx, 9512) = 1;  /* eapResp = 1 */
    CTX_S32(ctx, 9508) = 0;  /* eapReq = 0 */
    CTX_S32(ctx, 12536) = CTX_S32(ctx, 12552); /* idleWhile = clientTimeout */
    return 0;
}

static int eap_enter_discard(u8 *ctx)
{
    CTX_S32(ctx, 9504) = 1; /* eapNoResp = 1 */
    CTX_S32(ctx, 9508) = 0; /* eapReq = 0 */
    return 0;
}

static int eap_enter_identity(u8 *ctx)
{
    u16 id_len = (u16)sceNet_lib_D8722983((const char *)(ctx + 9118));
    u16 total = (u16)(id_len + 5);
    u16 be_len = bswap16(total);
    sceKernelMemcpy(ctx + 11025, ctx + 9118, id_len);
    ctx[11020] = 2;
    ctx[11021] = ctx[14092];
    ctx[11024] = 1; /* Identity */
    CTX_S32(ctx, 12520) = total;
    sceKernelMemcpy(ctx + 11022, &be_len, 2);
    return 0;
}

static int eap_enter_notification(u8 *ctx)
{
    u16 be_len = bswap16(5u);
    ctx[11020] = 2;
    ctx[11021] = ctx[14092];
    ctx[11024] = 2; /* Notification */
    CTX_S32(ctx, 12520) = 5;
    sceKernelMemcpy(ctx + 11022, &be_len, 2);
    return 0;
}

static int eap_enter_retransmit(u8 *ctx)
{
    int len = CTX_S32(ctx, 14072);
    sceKernelMemcpy(ctx + 11020, ctx + 12570, (SceSize)len);
    CTX_S32(ctx, 12520) = len;
    return 0;
}

static int eap_enter_failure(u8 *ctx)
{
    CTX_S32(ctx, 9392) = 1; /* eapFail = 1 */
    return 0;
}

static int eap_state_disabled(int *ctx)
{
    if (ctx[2354] == 1) {
        ctx[3527] = 1;
        eap_enter_initialize((u8 *)ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int eap_state_to_idle(int *ctx)
{
    ctx[3527] = 2;
    backend_noop(ctx);
    ctx[2274] = 1;
    return 0;
}

static int eap_state_idle(int *ctx)
{
    if (ctx[2377] == 1) {
        ctx[3527] = 3;
        eap_enter_received((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    if ((ctx[3135] == 1 && ctx[3519] != 0) || (ctx[3134] == 0 && ctx[3519] == 2)) {
        ctx[3527] = 11;
        backend_enter_eap_success((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[3136] == 1 || (ctx[3134] == 0 && ctx[3519] == 2) ||
        (ctx[3135] == 1 && ctx[3141] != 2 && ctx[3519] == 0)) {
        ctx[3527] = 12;
        eap_enter_failure((u8 *)ctx);
        ctx[2274] = 1;
    }
    return 0;
}

static int eap_state_received(int *ctx)
{
    u8 *c = (u8 *)ctx;
    int rx_success = CTX_S32(c, 14084);
    int rx_failure = CTX_S32(c, 14088);
    int rx_req     = CTX_S32(c, 14080);
    int decision   = CTX_S32(c, 14076);
    int mstate     = CTX_S32(c, 12564);

    if (rx_success == 1 && decision != 0) {
        CTX_S32(c, 14108) = 11;
        backend_enter_eap_success(c);
        CTX_S32(c, 9096) = 1;
        return 0;
    }
    if (mstate != 2 &&
        ((rx_failure == 1 && decision != 2) || (rx_success == 1 && decision == 0))) {
        CTX_S32(c, 14108) = 12;
        eap_enter_failure(c);
        CTX_S32(c, 9096) = 1;
        return 0;
    }
    if (rx_req == 1 && c[14092] == CTX_U16(c, 12568)) {
        CTX_S32(c, 14108) = 10;
        eap_enter_retransmit(c);
        CTX_S32(c, 9096) = 1;
        return 0;
    }
    if (rx_req == 1 && c[14092] != CTX_U16(c, 12568) &&
        c[14093] == 2 && CTX_S32(c, 12548) == 1) {
        CTX_S32(c, 14108) = 9;
        eap_enter_notification(c);
        CTX_S32(c, 9096) = 1;
        return 0;
    }
    if (rx_req == 1 && c[14092] != CTX_U16(c, 12568) &&
        c[12556] == 0 && c[14093] == 1) {
        CTX_S32(c, 14108) = 8;
        eap_enter_identity(c);
        CTX_S32(c, 9096) = 1;
        return 0;
    }
    if (rx_req == 1 && c[14092] != CTX_U16(c, 12568) &&
        c[12556] == 0 && c[14093] != 1 && c[14093] != 2) {
        CTX_S32(c, 14108) = 4;
        eap_enter_get_method(c);
        CTX_S32(c, 9096) = 1;
        return 0;
    }
    if (rx_req == 1 && c[14092] != CTX_U16(c, 12568) &&
        sceNet_lib_E0A81C7C(c + 14093, c + 12556, 8) == 0 &&
        mstate != 4) {
        CTX_S32(c, 14108) = 5;
        eap_enter_method(c);
        CTX_S32(c, 9096) = 1;
        return 0;
    }
    CTX_S32(c, 14108) = 7;
    eap_enter_discard(c);
    CTX_S32(c, 9096) = 1;
    return 0;
}

static int eap_state_get_method(int *ctx)
{
    u8 *c = (u8 *)ctx;
    if (sceNet_lib_E0A81C7C(c + 12556, c + 14093, 8) != 0) {
        CTX_S32(c, 14108) = 6;
        eap_enter_send_response(c);
    } else {
        CTX_S32(c, 14108) = 5;
        eap_enter_method(c);
    }
    CTX_S32(c, 9096) = 1;
    return 0;
}

static int eap_state_method(int *ctx)
{
    if (ctx[3526] == 1) {
        ctx[3527] = 7;
        eap_enter_discard((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[3141] == 4 && ctx[3519] == 0) {
        ctx[3527] = 12;
        eap_enter_failure((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    ctx[3527] = 6;
    eap_enter_send_response((u8 *)ctx);
    ctx[2274] = 1;
    return 0;
}

static int eap_state_to_send_response(int *ctx)
{
    ctx[3527] = 6;
    eap_enter_send_response((u8 *)ctx);
    ctx[2274] = 1;
    return 0;
}

static int eap_state_noop(int *ctx) { (void)ctx; return 0; }

static int (*const g_eap_state_table[13])(int *) = {
    eap_state_disabled,
    eap_state_to_idle,
    eap_state_idle,
    eap_state_received,
    eap_state_get_method,
    eap_state_method,
    eap_state_to_idle,
    eap_state_to_idle,
    eap_state_to_send_response,
    eap_state_to_send_response,
    eap_state_to_send_response,
    eap_state_noop,
    eap_state_noop,
};

static int eap_step(int *ctx)
{
    if (ctx[2354] == 0) {
        ctx[3527] = 0;
        eap_enter_disabled((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    if (ctx[2365] == 1 && ctx[2354] == 1) {
        ctx[3527] = 1;
        eap_enter_initialize((u8 *)ctx);
        ctx[2274] = 1;
        return 0;
    }
    g_eap_state_table[ctx[3527]](ctx);
    return 0;
}

static int eap8021x_step_all(int *ctx)
{
    for (;;) {
        pae_step(ctx);
        backend_step(ctx);
        eap_step(ctx);
        if (ctx[2274] <= 0)
            break;
        ctx[2274] = 0;
    }
    return 0;
}

static int eap_parse_packet(u8 *ctx)
{
    u8 *pkt = ctx + 9516;
    u32 pkt_len = CTX_U32(ctx, 11016);
    u16 eap_len;
    u8 code;

    CTX_S32(ctx, 14080) = 0;
    CTX_S32(ctx, 14084) = 0;
    CTX_S32(ctx, 14088) = 0;

    if (pkt_len < 4u)
        return 0;

    sceKernelMemcpy(&eap_len, pkt + 2, 2);
    eap_len = bswap16(eap_len);
    if (eap_len < 4u || pkt_len < eap_len)
        return 0;

    ctx[14092] = pkt[1]; /* id */
    code = pkt[0];
    if (code == 1) {
        if (eap_len >= 5u) {
            if (pkt[4] == 254) {
                if (eap_len < 11u)
                    return 0;
                sceKernelMemcpy(ctx + 14093, pkt + 4, 8);
            } else {
                sceKernelMemset(ctx + 14093, 0, 8);
                ctx[14093] = pkt[4];
            }
            CTX_S32(ctx, 14080) = 1;
        }
    } else if (code == 3) {
        CTX_S32(ctx, 14084) = 1;
    } else if (code == 4) {
        CTX_S32(ctx, 14088) = 1;
    }
    return 0;
}

static int eap_find_method(u8 *ctx, const u8 *req_type, int *out_idx)
{
    int idx = 0;
    EapMethodOps *ops;

    if (CTX_S32(ctx, 14124) == CTX_S32(ctx, 9108)) {
        if (out_idx != NULL)
            *out_idx = 0;
    } else {
        idx = 1;
    }

    if (idx == 1)
        return (int)0x80410F81;

    ops = (EapMethodOps *)(ctx + 14116 + 32 * idx);
    if (ops->type == 254) {
        if (sceNet_lib_E0A81C7C(ops->vendor_id, req_type + 1, 3) != 0 ||
            sceNet_lib_E0A81C7C(&ops->vendor_type, req_type + 4, 4) != 0)
            return (int)0x80410F82;
        return idx;
    }

    if (ops->type == req_type[0])
        return idx;
    return (int)0x80410F82;
}

static int eap_build_nak(u8 *ctx, int method_idx, int has_pref)
{
    u16 body_len = 0;
    u16 total;
    u16 be_len;

    if (has_pref != 0) {
        EapMethodOps *ops = (EapMethodOps *)(ctx + 14116 + 32 * method_idx);
        ctx[11025] = ops->type;
        body_len = 1;
    }
    total = (u16)(body_len + 5);
    be_len = bswap16(total);
    ctx[11020] = 2;
    ctx[11021] = ctx[14092];
    ctx[11024] = 3; /* Nak */
    CTX_S32(ctx, 12520) = total;
    sceKernelMemcpy(ctx + 11022, &be_len, 2);
    return 0;
}

/* --- EAP-MD5 method --- */

static int eap_md5_init(void *ctx)
{
    (void)ctx;
    sceKernelMemset(g_eap_md5_ctx, 0, 112);
    return 0;
}

static int eap_md5_check(void *ctx_ptr)
{
    u8 *ctx = (u8 *)ctx_ptr;
    u16 eap_len;
    sceKernelMemcpy(&eap_len, ctx + 9518, 2);
    eap_len = bswap16(eap_len);
    if (eap_len < 7u)
        return 1;
    return (eap_len < (u32)ctx[9521] + 6u) ? 1 : 0;
}

static int eap_md5_process(void *ctx_ptr)
{
    u8 *ctx = (u8 *)ctx_ptr;
    u8 val_size = ctx[9521];
    const u8 *challenge = (val_size != 0) ? (ctx + 9522) : NULL;
    int pass_len;

    sceKernelUtilsMd5BlockInit(g_eap_md5_ctx);
    sceKernelUtilsMd5BlockUpdate(g_eap_md5_ctx, ctx + 14092, 1);
    pass_len = sceNet_lib_D8722983((const char *)(ctx + 9246));
    sceKernelUtilsMd5BlockUpdate(g_eap_md5_ctx, ctx + 9246, (u32)pass_len);
    if (val_size != 0)
        sceKernelUtilsMd5BlockUpdate(g_eap_md5_ctx, challenge, val_size);
    sceKernelUtilsMd5BlockResult(g_eap_md5_ctx, g_eap_md5_digest);

    CTX_S32(ctx, 12548) = 1; /* allowNotifications */
    CTX_S32(ctx, 12564) = 4; /* methodState = DONE */
    CTX_S32(ctx, 14076) = 1; /* decision = COND_SUCC */
    return 1;
}

static int eap_md5_build_resp(void *ctx_ptr)
{
    u8 *ctx = (u8 *)ctx_ptr;
    ctx[11025] = 16;
    sceKernelMemcpy(ctx + 11026, g_eap_md5_digest, 16);
    return eap_build_resp_header(ctx, 4, 17);
}

static int eap_md5_register(u8 *ctx)
{
    EapMethodOps *ops = (EapMethodOps *)(ctx + 14116);
    sceKernelMemset(ops, 0, sizeof(EapMethodOps));
    ops->type = 4; /* MD5-Challenge */
    ops->enabled = 1;
    ops->init = eap_md5_init;
    ops->check = eap_md5_check;
    ops->process = eap_md5_process;
    ops->build_resp = eap_md5_build_resp;
    ops->reserved = 0;
    return 0;
}

static int eap8021x_thread(SceSize args, void *argp)
{
    int *ctx = *(int **)argp;
    SceUID alarm;
    int err;
    u8 eapol_type;
    u8 *body;
    u8 *next_pkt;
    int valid;

    (void)args;

    alarm = sceKernelSetAlarm(1000000, eap8021x_alarm_cb, ctx);
    if (alarm < 0) {
        eap8021x_notify(ctx, 1, alarm);
        if (ctx[2273] > 0)
            sceKernelCancelAlarm(ctx[2273]);
        return 0;
    }

    ctx[2273] = alarm;
    eap8021x_init_machines(ctx);
    ctx[2276] = 0;

    for (;;) {
        if (ctx[2269] != 0) {
            ctx[2276]++;
        } else {
            int rlen = eapol_bpf_read(ctx[6], ctx + 382, 1500);
            if (rlen < 0) {
                err = rlen;
                eap8021x_notify(ctx, 1, err);
                if (ctx[2273] > 0)
                    sceKernelCancelAlarm(ctx[2273]);
                return 0;
            }
            {
                u8 *cur = (u8 *)(ctx + 382);
                u32 rem = (u32)rlen;
                while (rem != 0) {
                    int plen = eapol_bpf_parse(ctx[6], cur, rem, (const u8 *)(ctx + 4),
                                               1, &eapol_type, &body, &next_pkt, &valid);
                    if (plen < 0 || rem < (u32)plen)
                        break;
                    if (valid > 0 && eapol_type == 0 && plen != 0) {
                        int slot;
                        for (slot = 0; slot < 4; slot++) {
                            if (ctx[758 + 378 * slot] == 0)
                                break;
                        }
                        if (slot != 4) {
                            int *node = &ctx[757 + 378 * slot];
                            sceKernelMemset(node, 0, 1512);
                            node[1] = 1;
                            node[2] = plen;
                            sceKernelMemcpy(node + 3, body, (SceSize)plen);
                            if (ctx[2270] != 0)
                                *(int *)ctx[2270] = (int)node;
                            else
                                ctx[2269] = (int)node;
                            ctx[2270] = (int)node;
                        }
                    }
                    if (next_pkt == NULL || rem < (u32)(next_pkt - cur))
                        break;
                    rem -= (u32)(next_pkt - cur);
                    cur = next_pkt;
                }
            }
        }

        if (ctx[2269] != 0) {
            if ((u32)ctx[2276] >= 101u)
                sceKernelDelayThread(500000);
            ctx[2349] = 1; /* eapolEap = 1 */
        }

        eap8021x_step_all(ctx);
        if (ctx[2271] != 0)
            break;
    }

    eap8021x_notify(ctx, 4, 0);
    if (ctx[2273] > 0)
        sceKernelCancelAlarm(ctx[2273]);
    return 0;
}

int eap8021x_start(const char *ifname, void (*cb)(int, int), int stack,
                   int prio, int unused, void *cfg)
{
    void *ctx_ptr = g_eap8021x_ctx_buf;
    int ret;
    int fd;
    SceUID thid;

    (void)unused;

    if (g_eap8021x_started > 0)
        return (int)0x80410F80;

    sceKernelMemset(g_eap8021x_ctx_buf, 0, sizeof(g_eap8021x_ctx_buf));
    sceKernelMemcpy(g_eap8021x_ctx_buf + 9108, cfg, 268);
    sceNet_lib_B5CE388A((char *)g_eap8021x_ctx_buf, ifname, 16);
    *(void (**)(int, int))(g_eap8021x_ctx_buf + 9100) = cb;

    ret = sceNetGetLocalEtherAddr(g_eap8021x_ctx_buf + 16);
    if (ret >= 0) {
        fd = eapol_bpf_open((const char *)g_eap8021x_ctx_buf, &g_eap8021x_bpf_prog,
                            g_eap8021x_ctx_buf + 16, 0, 1500, 500000u);
        ret = fd;
        if (fd >= 0) {
            CTX_S32(g_eap8021x_ctx_buf, 24) = fd;
            thid = sceKernelCreateThread("sceNet8021xStateMachine", eap8021x_thread,
                                         prio, stack, 0, NULL);
            ret = thid;
            if (thid >= 0) {
                CTX_S32(g_eap8021x_ctx_buf, 9088) = thid;
                ret = sceKernelStartThread(thid, 4, &ctx_ptr);
                if (ret >= 0) {
                    g_eap8021x_started = 1;
                    return 0;
                }
            }
        }
    }

    if (CTX_S32(g_eap8021x_ctx_buf, 9088) >= 0)
        sceKernelDeleteThread(CTX_S32(g_eap8021x_ctx_buf, 9088));
    if (CTX_S32(g_eap8021x_ctx_buf, 24) >= 0)
        eapol_bpf_close(CTX_S32(g_eap8021x_ctx_buf, 24));
    return ret;
}

int eap8021x_stop(void)
{
    SceUID thid = CTX_S32(g_eap8021x_ctx_buf, 9088);
    int fd = CTX_S32(g_eap8021x_ctx_buf, 24);

    if (thid >= 0) {
        CTX_S32(g_eap8021x_ctx_buf, 9476) = 1; /* userLogoff = 1 */
        sceKernelDelayThread(100000);
        CTX_S32(g_eap8021x_ctx_buf, 9084) = 1; /* stop flag */
        sceKernelWaitThreadEnd(thid, NULL);
        sceKernelDeleteThread(thid);
        sceNetFreeThreadinfo(thid);
    }
    if (fd >= 0)
        eapol_bpf_close(fd);
    g_eap8021x_started = 0;
    return 0;
}

int eap8021x_set_eap_flag(int val)
{
    CTX_S32(g_eap8021x_ctx_buf, 9424) = val;
    return 0;
}
