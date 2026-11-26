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
    int bpf_fd;
    int ret;
    int opt_val;
    int req_bufsize;
    u32 mac_hi;
    u16 mac_lo;
    u32 tv[2];
    char ifreq[32];

    req_bufsize = bufsize;
    bpf_fd = sceNet_lib_3B617AA0();
    if (bpf_fd < 0)
        return (int)0x80410E80;

    opt_val = 1;
    if (sceNet_lib_E1F4696F(bpf_fd, 0x80044270u, &opt_val) < 0) {
        ret = (int)0x80410E81;
    } else if (sceNet_lib_E1F4696F(bpf_fd, 0xC0044266u, &req_bufsize) < 0) {
        ret = (int)0x80410E82;
    } else {
        sceKernelMemset(ifreq, 0, 32);
        sceNet_lib_B5CE388A(ifreq, ifname, 16);
        ret = (int)0x80410E83;
        if (sceNet_lib_E1F4696F(bpf_fd, 0x8020426Cu, ifreq) >= 0) {
            prog->insns[3].k = type_filter;
            sceKernelMemcpy(&mac_hi, mac, 4);
            sceKernelMemcpy(&mac_lo, mac + 4, 2);
            prog->insns[7].k = bswap16(mac_lo);
            prog->insns[5].k = bswap32(mac_hi);
            ret = (int)0x80410E84;
            if (sceNet_lib_E1F4696F(bpf_fd, 0x80084267u, prog) >= 0) {
                if (timeout_us == 0)
                    return bpf_fd;
                tv[0] = timeout_us / 1000000u;
                tv[1] = timeout_us % 1000000u;
                ret = (int)0x80410E85;
                if (sceNet_lib_E1F4696F(bpf_fd, 0x8008426Du, tv) >= 0)
                    return bpf_fd;
            }
        }
    }

    sceNet_lib_DB88F458(bpf_fd);
    return ret;
}

int eapol_bpf_send(int bpf_fd, u8 *frame, const u8 *src_mac, const u8 *dst_mac,
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
    ret = sceNet_lib_C431A214(bpf_fd, frame, (u32)len + 18u);
    if (ret < 0)
        return (int)0x80410E86;
    return ret;
}

int eapol_bpf_parse(int bpf_fd, u8 *pkt, u32 pkt_len, const u8 *sta_mac,
                    int check_ver, u8 *out_type, u8 **out_body,
                    u8 **out_next, int *out_valid)
{
    const BpfHdr *bh = (const BpfHdr *)pkt;
    u32 caplen;
    u16 hdrlen;
    u8 *next_pkt;
    u8 *eth;
    u16 ethertype;
    u16 body_len;

    (void)bpf_fd;
    (void)check_ver;

    if (pkt_len < 20u) {
        *out_next = NULL;
        return 0;
    }

    caplen = bh->caplen;
    hdrlen = bh->hdrlen;
    next_pkt = pkt + ((hdrlen + caplen + 3u) & ~3u);

    if (caplen < 18u) {
        *out_next = next_pkt;
        *out_valid = 0;
        return 0;
    }

    eth = pkt + hdrlen;
    if (sceNet_lib_E0A81C7C(eth, g_eapol_pae_mac, 6) != 0 &&
        sceNet_lib_E0A81C7C(eth, sta_mac, 6) != 0) {
        *out_next = next_pkt;
        *out_valid = 0;
        return 0;
    }

    if (sceNet_lib_E0A81C7C(eth + 6, sta_mac, 6) != 0) {
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

int eapol_bpf_close(int bpf_fd)
{
    sceNet_lib_DB88F458(bpf_fd);
    return 0;
}

int eapol_bpf_read(int bpf_fd, void *buf, u32 len)
{
    int ret = sceNet_lib_B6FC0A5B(bpf_fd, buf, len);
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

int eapol_bpf_recv_wpa(int bpf_fd, u8 *buf, u32 bufsize, const u8 *mac,
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

    read_len = eapol_bpf_read(bpf_fd, buf, bufsize);
    if (read_len <= 0)
        return read_len;

    cur = buf;
    rem = read_len;
    while (rem > 0) {
        ret = eapol_bpf_parse(bpf_fd, cur, (u32)rem, mac, 1, &eapol_type, &body, &next_pkt, &valid);
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
} Eap8021xPktNode; /* 1512 bytes */

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

typedef struct Eap8021xConfig {
    int method_type;          /* +0x000 (9108) */
    u8 dst_mac[6];            /* +0x004 (9112) */
    char identity[128];       /* +0x00A (9118) */
    char password[128];       /* +0x08A (9246) */
    u8 pad_cfg[2];            /* +0x10A (9374) */
} Eap8021xConfig; /* 268 bytes */

typedef struct Eap8021xCtx {
    char ifname[16];                /* 0 */
    u8 sta_mac[6];                  /* 16 */
    u8 pad_mac[2];                  /* 22 */
    int bpf_fd;                     /* 24 */
    u8 tx_frame[1500];              /* 28 */
    u8 rx_buf[1500];                /* 1528 */
    Eap8021xPktNode pkt_pool[4];    /* 3028 */
    Eap8021xPktNode *pkt_head;      /* 9076 */
    Eap8021xPktNode *pkt_tail;      /* 9080 */
    int stop_flag;                  /* 9084 */
    SceUID thid;                    /* 9088 */
    SceUID alarm_id;                /* 9092 */
    int state_changed;              /* 9096 */
    void (*cb)(int, int);           /* 9100 */
    int idle_loops;                 /* 9104 */
    Eap8021xConfig cfg;             /* 9108 (268 bytes) */

    /* Common 802.1X state variables (9376..9459, 84 bytes) */
    int auth_while;                 /* 9376 */
    int held_while;                 /* 9380 */
    int quiet_while;                /* 9384 */
    int start_when;                 /* 9388 */
    int eap_fail;                   /* 9392 */
    int eapol_eap;                  /* 9396 */
    int eap_success;                /* 9400 */
    int port_valid;                 /* 9404 */
    int initialize;                 /* 9408 */
    int port_control;               /* 9412 */
    int port_enabled;               /* 9416 */
    int port_timers_pad;            /* 9420 */
    int eap_flag;                   /* 9424 */
    int supp_abort;                 /* 9428 */
    int supp_fail;                  /* 9432 */
    int supp_port_status;           /* 9436 */
    int supp_start;                 /* 9440 */
    int supp_success;               /* 9444 */
    int supp_timeout;               /* 9448 */
    int key_done;                   /* 9452 */
    int key_run;                    /* 9456 */

    /* PAE supplicant machine (9460..9503, 44 bytes) */
    int eap_restart;                /* 9460 */
    int logoff_sent;                /* 9464 */
    int s_port_mode;                /* 9468 */
    int start_count;                /* 9472 */
    int user_logoff;                /* 9476 */
    int held_period;                /* 9480 */
    int start_period;               /* 9484 */
    int max_start;                  /* 9488 */
    int pae_state;                  /* 9492 */
    int pae_init_done;              /* 9496 */
    int reauth_in_progress;         /* 9500 */

    /* Supplicant backend machine (9504..12535, 3032 bytes) */
    int eap_no_resp;                /* 9504 */
    int eap_req;                    /* 9508 */
    int eap_resp;                   /* 9512 */
    u8 eap_req_data[1500];          /* 9516 */
    u32 eap_req_data_len;           /* 11016 */
    u8 eap_resp_data[1500];         /* 11020 */
    int eap_resp_data_len;          /* 12520 */
    int auth_period;                /* 12524 */
    int backend_state;              /* 12528 */
    int backend_init_done;          /* 12532 */

    /* EAP peer machine (12536..14147, 1612 bytes) */
    int idle_while;                 /* 12536 */
    int alt_accept;                 /* 12540 */
    int alt_reject;                 /* 12544 */
    int allow_notifications;        /* 12548 */
    int client_timeout;             /* 12552 */
    u8 selected_method[8];          /* 12556 */
    int method_state;               /* 12564 */
    u16 last_id;                    /* 12568 */
    u8 last_resp_data[1500];        /* 12570 */
    u8 pad_last_resp[2];            /* 14070 */
    int last_resp_len;              /* 14072 */
    int decision;                   /* 14076 */
    int rx_req;                     /* 14080 */
    int rx_success;                 /* 14084 */
    int rx_failure;                 /* 14088 */
    u8 req_id;                      /* 14092 */
    u8 req_method[8];               /* 14093 */
    u8 pad_req_method[3];           /* 14101 */
    int ignore;                     /* 14104 */
    int eap_state;                  /* 14108 */
    int eap_init_done;              /* 14112 */
    EapMethodOps methods[1];        /* 14116 (32 bytes) */
} Eap8021xCtx; /* 14148 bytes */

static Eap8021xCtx g_eap8021x_ctx;
static int g_eap8021x_started = 0;

/* MD5 method state */
static u8 g_eap_md5_ctx[96];
static u8 g_eap_md5_digest[16];

static void eap8021x_notify(Eap8021xCtx *ctx, int event, int err)
{
    if (ctx->cb != NULL)
        ctx->cb(event, err);
}

static Eap8021xPktNode *eap8021x_dequeue_pkt(Eap8021xCtx *ctx)
{
    Eap8021xPktNode *head = ctx->pkt_head;
    if (ctx->pkt_tail == head)
        ctx->pkt_tail = NULL;
    if (head != NULL)
        ctx->pkt_head = head->next;
    ctx->idle_loops = 0;
    return head;
}

static int eap8021x_tick_timers(Eap8021xCtx *ctx)
{
    if (ctx->auth_while != 0) ctx->auth_while--;
    if (ctx->held_while != 0) ctx->held_while--;
    if (ctx->quiet_while != 0) ctx->quiet_while--;
    if (ctx->start_when != 0) ctx->start_when--;
    if (ctx->idle_while != 0) ctx->idle_while--;
    return 0;
}

static u32 eap8021x_alarm_cb(void *common)
{
    eap8021x_tick_timers((Eap8021xCtx *)common);
    return 1000000;
}

/* Forward declarations for state machines */
static int pae_init(Eap8021xCtx *ctx);
static int pae_reset_done(Eap8021xCtx *ctx);
static int pae_is_init_done(Eap8021xCtx *ctx);
static int backend_init(Eap8021xCtx *ctx);
static int backend_reset_done(Eap8021xCtx *ctx);
static int backend_is_init_done(Eap8021xCtx *ctx);
static int eap_init(Eap8021xCtx *ctx);
static int eap_reset_done(Eap8021xCtx *ctx);
static int eap_is_init_done(Eap8021xCtx *ctx);
static int eap_md5_register(Eap8021xCtx *ctx);
static int eap_parse_packet(Eap8021xCtx *ctx);
static int eap_find_method(Eap8021xCtx *ctx, const u8 *req_type, int *out_idx);
static int eap_build_nak(Eap8021xCtx *ctx, int method_idx, int has_pref);

static int eap8021x_init_machines(Eap8021xCtx *ctx)
{
    sceKernelMemset(&ctx->auth_while, 0, 84);
    ctx->port_control = 2; /* Auto */
    ctx->port_enabled = 1;
    pae_init(ctx);
    backend_init(ctx);
    eap_init(ctx);
    pae_reset_done(ctx);
    backend_reset_done(ctx);
    eap_reset_done(ctx);
    ctx->initialize = 1;
    return 0;
}

static int eap8021x_check_init_complete(Eap8021xCtx *ctx)
{
    if (pae_is_init_done(ctx) &&
        backend_is_init_done(ctx) &&
        eap_is_init_done(ctx)) {
        ctx->initialize = 0;
        return 1;
    }
    return 0;
}

/* --- PAE Supplicant state machine --- */

static int pae_init(Eap8021xCtx *ctx)
{
    sceKernelMemset(&ctx->eap_restart, 0, 44);
    ctx->max_start = 3;
    ctx->start_period = 5;
    ctx->held_period = 5;
    return 0;
}

static int pae_reset_done(Eap8021xCtx *ctx)
{
    ctx->pae_init_done = 0;
    return 0;
}

static int pae_is_init_done(Eap8021xCtx *ctx)
{
    return ctx->pae_init_done == 1;
}

static int pae_enter_logoff(Eap8021xCtx *ctx)
{
    eapol_bpf_send(ctx->bpf_fd, ctx->tx_frame, ctx->sta_mac,
                   ctx->cfg.dst_mac, 1, 1, 2, 0);
    ctx->logoff_sent = 1;
    ctx->supp_port_status = 0;
    return 0;
}

static int pae_enter_disconnected(Eap8021xCtx *ctx)
{
    ctx->logoff_sent = 0;
    ctx->start_count = 0;
    ctx->s_port_mode = 2;
    ctx->supp_abort = 1;
    ctx->supp_port_status = 0;
    return 0;
}

static int pae_enter_connecting(Eap8021xCtx *ctx)
{
    ctx->start_when = ctx->start_period;
    ctx->start_count++;
    ctx->eapol_eap = 0;
    eapol_bpf_send(ctx->bpf_fd, ctx->tx_frame, ctx->sta_mac,
                   ctx->cfg.dst_mac, 1, 1, 1, 0);
    return 0;
}

static int pae_enter_authenticating(Eap8021xCtx *ctx)
{
    ctx->start_count = 0;
    ctx->supp_start = 1;
    ctx->supp_success = 0;
    ctx->supp_fail = 0;
    ctx->supp_timeout = 0;
    ctx->key_run = 0;
    ctx->key_done = 0;
    return 0;
}

static int pae_enter_held(Eap8021xCtx *ctx)
{
    ctx->supp_port_status = 0;
    ctx->held_while = ctx->held_period;
    return 0;
}

static int pae_enter_authenticated(Eap8021xCtx *ctx)
{
    ctx->supp_port_status = 1;
    return 0;
}

static int pae_enter_restart(Eap8021xCtx *ctx)
{
    ctx->eap_restart = 1;
    return 0;
}

static int pae_enter_s_force_auth(Eap8021xCtx *ctx)
{
    ctx->s_port_mode = 1;
    ctx->supp_port_status = 1;
    return 0;
}

static int pae_enter_s_force_unauth(Eap8021xCtx *ctx)
{
    ctx->supp_port_status = 0;
    ctx->s_port_mode = 0;
    eapol_bpf_send(ctx->bpf_fd, ctx->tx_frame, ctx->sta_mac,
                   ctx->cfg.dst_mac, 1, 1, 2, 0);
    return 0;
}

static int (*g_pae_enter_table[9])(Eap8021xCtx *) = {
    pae_enter_logoff,
    pae_enter_disconnected,
    pae_enter_connecting,
    pae_enter_authenticating,
    pae_enter_held,
    pae_enter_authenticated,
    pae_enter_restart,
    pae_enter_s_force_auth,
    pae_enter_s_force_unauth,
};

static int pae_state_logoff(Eap8021xCtx *ctx)
{
    if (ctx->user_logoff == 0) {
        ctx->pae_state = 1;
        g_pae_enter_table[1](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int pae_state_disconnected(Eap8021xCtx *ctx)
{
    ctx->pae_state = 2;
    g_pae_enter_table[2](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int pae_state_connecting(Eap8021xCtx *ctx)
{
    int start_when = ctx->start_when;
    if (start_when == 0) {
        if ((u32)ctx->start_count < (u32)ctx->max_start) {
            ctx->pae_state = 2;
            g_pae_enter_table[2](ctx);
            ctx->state_changed = 1;
            return 0;
        }
        if (ctx->eap_flag == 1) {
            ctx->pae_state = 5;
            g_pae_enter_table[5](ctx);
            ctx->state_changed = 1;
            return 0;
        }
    }
    if (ctx->eap_success == 1 || ctx->eap_fail == 1) {
        ctx->pae_state = 3;
        g_pae_enter_table[3](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eapol_eap == 1) {
        ctx->pae_state = 6;
        g_pae_enter_table[6](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (start_when == 0 && (u32)ctx->start_count >= (u32)ctx->max_start && ctx->eap_flag == 0) {
        ctx->pae_state = 4;
        g_pae_enter_table[4](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int pae_state_authenticating(Eap8021xCtx *ctx)
{
    if (ctx->supp_success == 1 && ctx->eap_flag == 1) {
        ctx->pae_state = 5;
        g_pae_enter_table[5](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->supp_fail == 1 || (ctx->key_done == 1 && ctx->eap_flag == 0)) {
        ctx->pae_state = 4;
        g_pae_enter_table[4](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->supp_timeout == 1) {
        ctx->pae_state = 2;
        g_pae_enter_table[2](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int pae_state_held(Eap8021xCtx *ctx)
{
    if (ctx->held_while == 0) {
        ctx->pae_state = 2;
        g_pae_enter_table[2](ctx);
        ctx->state_changed = 1;
    } else if (ctx->eapol_eap == 1) {
        ctx->pae_state = 6;
        g_pae_enter_table[6](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int pae_state_authenticated(Eap8021xCtx *ctx)
{
    if (ctx->eapol_eap == 1 && ctx->eap_flag == 1) {
        ctx->reauth_in_progress = 1;
        eap8021x_notify(ctx, 2, 0);
        ctx->pae_state = 6;
        g_pae_enter_table[6](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eap_flag == 0) {
        ctx->pae_state = 1;
        g_pae_enter_table[1](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int pae_state_restart(Eap8021xCtx *ctx)
{
    if (ctx->eap_restart == 0) {
        ctx->pae_state = 3;
        g_pae_enter_table[3](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int pae_state_s_force_auth(Eap8021xCtx *ctx) { (void)ctx; return 0; }
static int pae_state_s_force_unauth(Eap8021xCtx *ctx) { (void)ctx; return 0; }

static int (*const g_pae_state_table[9])(Eap8021xCtx *) = {
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

static int pae_step(Eap8021xCtx *ctx)
{
    int port_ctrl;

    if (ctx->user_logoff == 1 && ctx->logoff_sent == 0 && ctx->initialize != 1 && ctx->port_enabled != 0) {
        ctx->pae_state = 0;
        pae_enter_logoff(ctx);
        ctx->state_changed = 1;
        return 0;
    }

    port_ctrl = ctx->port_control;
    if (port_ctrl == 1 && ctx->s_port_mode != 1 && ctx->initialize != 1 && ctx->port_enabled != 0) {
        ctx->pae_state = 7;
        pae_enter_s_force_auth(ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (port_ctrl == 0 && ctx->s_port_mode != 0 && ctx->initialize != 1 && ctx->port_enabled != 0) {
        ctx->pae_state = 8;
        pae_enter_s_force_unauth(ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if ((port_ctrl == 2 && ctx->s_port_mode != 2) || ctx->initialize == 1 || ctx->port_enabled == 0) {
        ctx->pae_state = 1;
        pae_enter_disconnected(ctx);
        ctx->state_changed = 1;
        ctx->pae_init_done = 1;
        eap8021x_check_init_complete(ctx);
        return 0;
    }

    g_pae_state_table[ctx->pae_state](ctx);
    return 0;
}

/* --- Supplicant Backend state machine --- */

static int backend_init(Eap8021xCtx *ctx)
{
    sceKernelMemset(&ctx->eap_no_resp, 0, 3032);
    ctx->auth_period = 5;
    return 0;
}

static int backend_reset_done(Eap8021xCtx *ctx)
{
    ctx->backend_init_done = 0;
    return 0;
}

static int backend_is_init_done(Eap8021xCtx *ctx)
{
    return ctx->backend_init_done == 1;
}

static int backend_enter_request(Eap8021xCtx *ctx)
{
    Eap8021xPktNode *pkt = eap8021x_dequeue_pkt(ctx);
    if (pkt != NULL) {
        ctx->auth_while = 0;
        sceKernelMemcpy(ctx->eap_req_data, pkt->data, (SceSize)pkt->len);
        ctx->eap_req_data_len = (u32)pkt->len;
        pkt->in_use = 0;
        ctx->eap_req = 1;
    }
    return 0;
}

static int backend_enter_response(Eap8021xCtx *ctx)
{
    sceKernelMemcpy(ctx->tx_frame + 18, ctx->eap_resp_data, (SceSize)ctx->eap_resp_data_len);
    eapol_bpf_send(ctx->bpf_fd, ctx->tx_frame, ctx->sta_mac,
                   ctx->cfg.dst_mac, 1, 1, 0, (u16)ctx->eap_resp_data_len);
    ctx->eap_resp = 0;
    return 0;
}

static int backend_enter_success(Eap8021xCtx *ctx)
{
    int ev = (ctx->reauth_in_progress == 1) ? 3 : 0;
    ctx->supp_success = 1;
    ctx->key_run = 1;
    eap8021x_notify(ctx, ev, 0);
    ctx->reauth_in_progress = 0;
    return 0;
}

static int backend_enter_eap_success(Eap8021xCtx *ctx)
{
    ctx->eap_success = 1;
    return 0;
}

static int backend_enter_fail(Eap8021xCtx *ctx)
{
    ctx->supp_fail = 1;
    eap8021x_notify(ctx, 1, (int)0x80410F01);
    return 0;
}

static int backend_enter_timeout(Eap8021xCtx *ctx)
{
    ctx->supp_timeout = 1;
    return 0;
}

static int backend_enter_idle(Eap8021xCtx *ctx)
{
    ctx->supp_start = 0;
    ctx->eapol_eap = 0;
    return 0;
}

static int backend_noop(Eap8021xCtx *ctx) { (void)ctx; return 0; }

static int backend_enter_initialize(Eap8021xCtx *ctx)
{
    ctx->supp_abort = 0;
    return 0;
}

static int backend_enter_receive(Eap8021xCtx *ctx)
{
    ctx->eap_no_resp = 0;
    ctx->auth_while = ctx->auth_period;
    ctx->eapol_eap = 0;
    return 0;
}

static int (*g_backend_enter_table[8])(Eap8021xCtx *) = {
    backend_enter_request,
    backend_enter_response,
    backend_enter_success,
    backend_enter_fail,
    backend_enter_timeout,
    backend_enter_idle,
    backend_enter_initialize,
    backend_enter_receive,
};

static int backend_state_request(Eap8021xCtx *ctx)
{
    if (ctx->eap_fail == 1) {
        ctx->backend_state = 3;
        g_backend_enter_table[3](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eap_no_resp == 1) {
        ctx->backend_state = 7;
        g_backend_enter_table[7](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eap_resp == 1) {
        ctx->backend_state = 1;
        g_backend_enter_table[1](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eap_success == 1) {
        ctx->backend_state = 2;
        g_backend_enter_table[2](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int backend_state_response(Eap8021xCtx *ctx)
{
    ctx->backend_state = 7;
    g_backend_enter_table[7](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int backend_state_success(Eap8021xCtx *ctx)
{
    ctx->backend_state = 5;
    g_backend_enter_table[5](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int backend_state_fail(Eap8021xCtx *ctx)
{
    ctx->backend_state = 5;
    g_backend_enter_table[5](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int backend_state_timeout(Eap8021xCtx *ctx)
{
    ctx->backend_state = 5;
    g_backend_enter_table[5](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int backend_state_idle(Eap8021xCtx *ctx)
{
    if (ctx->eap_fail == 1 && ctx->supp_start == 1) {
        ctx->backend_state = 3;
        g_backend_enter_table[3](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eapol_eap == 1 && ctx->supp_start == 1) {
        ctx->backend_state = 0;
        g_backend_enter_table[0](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eap_success == 1 && ctx->supp_start == 1) {
        ctx->backend_state = 2;
        g_backend_enter_table[2](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int backend_state_initialize(Eap8021xCtx *ctx)
{
    ctx->backend_state = 5;
    g_backend_enter_table[5](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int backend_state_receive(Eap8021xCtx *ctx)
{
    if (ctx->eapol_eap == 1) {
        ctx->backend_state = 0;
        g_backend_enter_table[0](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eap_fail == 1) {
        ctx->backend_state = 3;
        g_backend_enter_table[3](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->auth_while == 0) {
        ctx->backend_state = 4;
        g_backend_enter_table[4](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eap_success == 1) {
        ctx->backend_state = 2;
        g_backend_enter_table[2](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int (*const g_backend_state_table[8])(Eap8021xCtx *) = {
    backend_state_request,
    backend_state_response,
    backend_state_success,
    backend_state_fail,
    backend_state_timeout,
    backend_state_idle,
    backend_state_initialize,
    backend_state_receive,
};

static int backend_step(Eap8021xCtx *ctx)
{
    if (ctx->initialize == 1 || ctx->supp_abort == 1) {
        ctx->backend_state = 6;
        backend_enter_initialize(ctx);
        ctx->backend_init_done = 1;
        ctx->state_changed = 1;
        eap8021x_check_init_complete(ctx);
    } else {
        g_backend_state_table[ctx->backend_state](ctx);
    }
    return 0;
}

/* --- EAP Peer state machine (RFC 4137) --- */

static int eap_init(Eap8021xCtx *ctx)
{
    sceKernelMemset(&ctx->idle_while, 0, 1612);
    ctx->client_timeout = 10;
    eap_md5_register(ctx);
    return 0;
}

static int eap_reset_done(Eap8021xCtx *ctx) { (void)ctx; return 0; }
static int eap_is_init_done(Eap8021xCtx *ctx) { (void)ctx; return 1; }

static int eap_enter_initialize(Eap8021xCtx *ctx)
{
    ctx->allow_notifications = 1;
    ctx->selected_method[0] = 0;
    ctx->idle_while = ctx->client_timeout;
    ctx->last_id = 256;
    ctx->eap_fail = 0;
    ctx->eap_success = 0;
    ctx->method_state = 0;
    ctx->eap_restart = 0;
    ctx->decision = 0;
    return 0;
}

static int eap_build_resp_header(Eap8021xCtx *ctx, u8 type, u16 body_len)
{
    u16 total = (u16)(body_len + 5);
    u16 be_len = bswap16(total);
    ctx->eap_resp_data[0] = 2; /* EAP Response */
    ctx->eap_resp_data[1] = ctx->req_id;
    ctx->eap_resp_data[4] = type;
    ctx->eap_resp_data_len = total;
    sceKernelMemcpy(ctx->eap_resp_data + 2, &be_len, 2);
    return 0;
}

static int eap_enter_disabled(Eap8021xCtx *ctx) { (void)ctx; return 0; }

static int eap_enter_received(Eap8021xCtx *ctx)
{
    eap_parse_packet(ctx);
    return 0;
}

static int eap_enter_get_method(Eap8021xCtx *ctx)
{
    int pref_idx = 0;
    int idx = eap_find_method(ctx, ctx->req_method, &pref_idx);
    if (idx < 0) {
        if (idx == (int)0x80410F81)
            eap_build_nak(ctx, 0, 0);
        else
            eap_build_nak(ctx, pref_idx, 1);
    } else {
        EapMethodOps *ops = &ctx->methods[idx];
        sceKernelMemcpy(ctx->selected_method, ctx->req_method, 8);
        ctx->method_state = 1; /* INIT */
        ops->init(ctx);
    }
    return 0;
}

static int eap_enter_method(Eap8021xCtx *ctx)
{
    int idx = eap_find_method(ctx, ctx->selected_method, NULL);
    EapMethodOps *ops = &ctx->methods[idx];
    int ignore = ops->check(ctx);
    ctx->ignore = ignore;
    if (ignore == 0) {
        ops->process(ctx);
        ops->build_resp(ctx);
    }
    return 0;
}

static int eap_enter_send_response(Eap8021xCtx *ctx)
{
    ctx->last_id = ctx->req_id;
    sceKernelMemcpy(ctx->last_resp_data, ctx->eap_resp_data, (SceSize)ctx->eap_resp_data_len);
    ctx->last_resp_len = ctx->eap_resp_data_len;
    ctx->eap_resp = 1;
    ctx->eap_req = 0;
    ctx->idle_while = ctx->client_timeout;
    return 0;
}

static int eap_enter_discard(Eap8021xCtx *ctx)
{
    ctx->eap_no_resp = 1;
    ctx->eap_req = 0;
    return 0;
}

static int eap_enter_identity(Eap8021xCtx *ctx)
{
    u16 id_len = (u16)sceNet_lib_D8722983(ctx->cfg.identity);
    u16 total = (u16)(id_len + 5);
    u16 be_len = bswap16(total);
    sceKernelMemcpy(ctx->eap_resp_data + 5, ctx->cfg.identity, id_len);
    ctx->eap_resp_data[0] = 2;
    ctx->eap_resp_data[1] = ctx->req_id;
    ctx->eap_resp_data[4] = 1; /* Identity */
    ctx->eap_resp_data_len = total;
    sceKernelMemcpy(ctx->eap_resp_data + 2, &be_len, 2);
    return 0;
}

static int eap_enter_notification(Eap8021xCtx *ctx)
{
    u16 be_len = bswap16(5u);
    ctx->eap_resp_data[0] = 2;
    ctx->eap_resp_data[1] = ctx->req_id;
    ctx->eap_resp_data[4] = 2; /* Notification */
    ctx->eap_resp_data_len = 5;
    sceKernelMemcpy(ctx->eap_resp_data + 2, &be_len, 2);
    return 0;
}

static int eap_enter_retransmit(Eap8021xCtx *ctx)
{
    sceKernelMemcpy(ctx->eap_resp_data, ctx->last_resp_data, (SceSize)ctx->last_resp_len);
    ctx->eap_resp_data_len = ctx->last_resp_len;
    return 0;
}

static int eap_enter_failure(Eap8021xCtx *ctx)
{
    ctx->eap_fail = 1;
    return 0;
}

static int (*g_eap_enter_table[13])(Eap8021xCtx *) = {
    eap_enter_disabled,
    eap_enter_initialize,
    backend_noop,
    eap_enter_received,
    eap_enter_get_method,
    eap_enter_method,
    eap_enter_send_response,
    eap_enter_discard,
    eap_enter_identity,
    eap_enter_notification,
    eap_enter_retransmit,
    backend_enter_eap_success,
    eap_enter_failure,
};

static int eap_state_disabled(Eap8021xCtx *ctx)
{
    if (ctx->port_enabled == 1) {
        ctx->eap_state = 1;
        g_eap_enter_table[1](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int eap_state_initialize(Eap8021xCtx *ctx)
{
    ctx->eap_state = 2;
    g_eap_enter_table[2](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_idle(Eap8021xCtx *ctx)
{
    if (ctx->eap_req == 1) {
        ctx->eap_state = 3;
        g_eap_enter_table[3](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if ((ctx->alt_accept == 1 && ctx->decision != 0) || (ctx->idle_while == 0 && ctx->decision == 2)) {
        ctx->eap_state = 11;
        g_eap_enter_table[11](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->alt_reject == 1 || (ctx->idle_while == 0 && ctx->decision == 2) ||
        (ctx->alt_accept == 1 && ctx->method_state != 2 && ctx->decision == 0)) {
        ctx->eap_state = 12;
        g_eap_enter_table[12](ctx);
        ctx->state_changed = 1;
    }
    return 0;
}

static int eap_state_received(Eap8021xCtx *ctx)
{
    int rx_success = ctx->rx_success;
    int rx_failure = ctx->rx_failure;
    int rx_req     = ctx->rx_req;
    int decision   = ctx->decision;
    int mstate     = ctx->method_state;

    if (rx_success == 1 && decision != 0) {
        ctx->eap_state = 11;
        g_eap_enter_table[11](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (mstate != 2 &&
        ((rx_failure == 1 && decision != 2) || (rx_success == 1 && decision == 0))) {
        ctx->eap_state = 12;
        g_eap_enter_table[12](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (rx_req == 1 && ctx->req_id == ctx->last_id) {
        ctx->eap_state = 10;
        g_eap_enter_table[10](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (rx_req == 1 && ctx->req_id != ctx->last_id &&
        ctx->req_method[0] == 2 && ctx->allow_notifications == 1) {
        ctx->eap_state = 9;
        g_eap_enter_table[9](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (rx_req == 1 && ctx->req_id != ctx->last_id &&
        ctx->selected_method[0] == 0 && ctx->req_method[0] == 1) {
        ctx->eap_state = 8;
        g_eap_enter_table[8](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (rx_req == 1 && ctx->req_id != ctx->last_id &&
        ctx->selected_method[0] == 0 && ctx->req_method[0] != 1 && ctx->req_method[0] != 2) {
        ctx->eap_state = 4;
        g_eap_enter_table[4](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (rx_req == 1 && ctx->req_id != ctx->last_id &&
        sceNet_lib_E0A81C7C(ctx->req_method, ctx->selected_method, 8) == 0 &&
        mstate != 4) {
        ctx->eap_state = 5;
        g_eap_enter_table[5](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    ctx->eap_state = 7;
    g_eap_enter_table[7](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_get_method(Eap8021xCtx *ctx)
{
    if (sceNet_lib_E0A81C7C(ctx->selected_method, ctx->req_method, 8) != 0) {
        ctx->eap_state = 6;
        g_eap_enter_table[6](ctx);
    } else {
        ctx->eap_state = 5;
        g_eap_enter_table[5](ctx);
    }
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_method(Eap8021xCtx *ctx)
{
    if (ctx->ignore == 1) {
        ctx->eap_state = 7;
        g_eap_enter_table[7](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->method_state == 4 && ctx->decision == 0) {
        ctx->eap_state = 12;
        g_eap_enter_table[12](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    ctx->eap_state = 6;
    g_eap_enter_table[6](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_send_response(Eap8021xCtx *ctx)
{
    ctx->eap_state = 2;
    g_eap_enter_table[2](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_discard(Eap8021xCtx *ctx)
{
    ctx->eap_state = 2;
    g_eap_enter_table[2](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_identity(Eap8021xCtx *ctx)
{
    ctx->eap_state = 6;
    g_eap_enter_table[6](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_notification(Eap8021xCtx *ctx)
{
    ctx->eap_state = 6;
    g_eap_enter_table[6](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_retransmit(Eap8021xCtx *ctx)
{
    ctx->eap_state = 6;
    g_eap_enter_table[6](ctx);
    ctx->state_changed = 1;
    return 0;
}

static int eap_state_success(Eap8021xCtx *ctx) { (void)ctx; return 0; }
static int eap_state_failure(Eap8021xCtx *ctx) { (void)ctx; return 0; }

static int (*const g_eap_state_table[13])(Eap8021xCtx *) = {
    eap_state_disabled,
    eap_state_initialize,
    eap_state_idle,
    eap_state_received,
    eap_state_get_method,
    eap_state_method,
    eap_state_send_response,
    eap_state_discard,
    eap_state_identity,
    eap_state_notification,
    eap_state_retransmit,
    eap_state_success,
    eap_state_failure,
};

static int eap_step(Eap8021xCtx *ctx)
{
    if (ctx->port_enabled == 0) {
        ctx->eap_state = 0;
        g_eap_enter_table[0](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    if (ctx->eap_restart == 1 && ctx->port_enabled == 1) {
        ctx->eap_state = 1;
        g_eap_enter_table[1](ctx);
        ctx->state_changed = 1;
        return 0;
    }
    g_eap_state_table[ctx->eap_state](ctx);
    return 0;
}

static int eap8021x_step_all(Eap8021xCtx *ctx)
{
    for (;;) {
        pae_step(ctx);
        backend_step(ctx);
        eap_step(ctx);
        if (ctx->state_changed <= 0)
            break;
        ctx->state_changed = 0;
    }
    return 0;
}

static int eap_parse_packet(Eap8021xCtx *ctx)
{
    u8 *pkt = ctx->eap_req_data;
    u32 pkt_len = ctx->eap_req_data_len;
    u16 eap_len;
    u8 code;

    ctx->rx_req = 0;
    ctx->rx_success = 0;
    ctx->rx_failure = 0;

    if (pkt_len < 4u)
        return 0;

    sceKernelMemcpy(&eap_len, pkt + 2, 2);
    eap_len = bswap16(eap_len);
    if (eap_len < 4u || pkt_len < eap_len)
        return 0;

    ctx->req_id = pkt[1];
    code = pkt[0];
    if (code == 1) {
        if (eap_len >= 5u) {
            if (pkt[4] == 254) {
                if (eap_len < 11u)
                    return 0;
                sceKernelMemcpy(ctx->req_method, pkt + 4, 8);
            } else {
                sceKernelMemset(ctx->req_method, 0, 8);
                ctx->req_method[0] = pkt[4];
            }
            ctx->rx_req = 1;
        }
    } else if (code == 3) {
        ctx->rx_success = 1;
    } else if (code == 4) {
        ctx->rx_failure = 1;
    }
    return 0;
}

static int eap_find_method(Eap8021xCtx *ctx, const u8 *req_type, int *out_idx)
{
    int idx = 0;
    EapMethodOps *ops;

    if (ctx->methods[0].enabled == ctx->cfg.method_type) {
        if (out_idx != NULL)
            *out_idx = 0;
    } else {
        idx = 1;
    }

    if (idx == 1)
        return (int)0x80410F81;

    ops = &ctx->methods[idx];
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

static int eap_build_nak(Eap8021xCtx *ctx, int method_idx, int has_pref)
{
    u16 body_len = 0;
    u16 total;
    u16 be_len;

    if (has_pref != 0) {
        EapMethodOps *ops = &ctx->methods[method_idx];
        ctx->eap_resp_data[5] = ops->type;
        body_len = 1;
    }
    total = (u16)(body_len + 5);
    be_len = bswap16(total);
    ctx->eap_resp_data[0] = 2;
    ctx->eap_resp_data[1] = ctx->req_id;
    ctx->eap_resp_data[4] = 3; /* Nak */
    ctx->eap_resp_data_len = total;
    sceKernelMemcpy(ctx->eap_resp_data + 2, &be_len, 2);
    return 0;
}

/* --- EAP-MD5 method --- */

static int eap_md5_init(void *ctx)
{
    (void)ctx;
    return (int)sceKernelMemset(g_eap_md5_ctx, 0, 112);
}

static int eap_md5_check(void *ctx_ptr)
{
    Eap8021xCtx *ctx = (Eap8021xCtx *)ctx_ptr;
    u16 eap_len;
    sceKernelMemcpy(&eap_len, ctx->eap_req_data + 2, 2);
    eap_len = bswap16(eap_len);
    if (eap_len < 7u)
        return 1;
    return ((int)eap_len < (int)ctx->eap_req_data[5] + 6) ? 1 : 0;
}

static int eap_md5_process(void *ctx_ptr)
{
    Eap8021xCtx *ctx = (Eap8021xCtx *)ctx_ptr;
    u8 val_size = ctx->eap_req_data[5];
    const u8 *challenge = (val_size != 0) ? (ctx->eap_req_data + 6) : NULL;
    int pass_len;

    sceKernelUtilsMd5BlockInit(g_eap_md5_ctx);
    sceKernelUtilsMd5BlockUpdate(g_eap_md5_ctx, &ctx->req_id, 1);
    pass_len = sceNet_lib_D8722983(ctx->cfg.password);
    sceKernelUtilsMd5BlockUpdate(g_eap_md5_ctx, ctx->cfg.password, (u32)pass_len);
    if (val_size != 0)
        sceKernelUtilsMd5BlockUpdate(g_eap_md5_ctx, challenge, val_size);
    sceKernelUtilsMd5BlockResult(g_eap_md5_ctx, g_eap_md5_digest);

    ctx->allow_notifications = 1;
    ctx->method_state = 4; /* DONE */
    ctx->decision = 1; /* COND_SUCC */
    return 1;
}

static int eap_md5_build_resp(void *ctx_ptr)
{
    Eap8021xCtx *ctx = (Eap8021xCtx *)ctx_ptr;
    ctx->eap_resp_data[5] = 16;
    sceKernelMemcpy(ctx->eap_resp_data + 6, g_eap_md5_digest, 16);
    return eap_build_resp_header(ctx, 4, 17);
}

static int eap_md5_register(Eap8021xCtx *ctx)
{
    EapMethodOps *ops = &ctx->methods[0];
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
    Eap8021xCtx *ctx = *(Eap8021xCtx **)argp;
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
        if (ctx->alarm_id > 0)
            sceKernelCancelAlarm(ctx->alarm_id);
        return 0;
    }

    ctx->alarm_id = alarm;
    eap8021x_init_machines(ctx);
    ctx->idle_loops = 0;

    for (;;) {
        if (ctx->pkt_head != NULL) {
            ctx->idle_loops++;
        } else {
            int rlen = eapol_bpf_read(ctx->bpf_fd, ctx->rx_buf, 1500);
            if (rlen < 0) {
                err = rlen;
                eap8021x_notify(ctx, 1, err);
                if (ctx->alarm_id > 0)
                    sceKernelCancelAlarm(ctx->alarm_id);
                return 0;
            }
            {
                u8 *cur = ctx->rx_buf;
                u32 rem = (u32)rlen;
                while (rem != 0) {
                    int plen = eapol_bpf_parse(ctx->bpf_fd, cur, rem, ctx->sta_mac,
                                               1, &eapol_type, &body, &next_pkt, &valid);
                    if (plen < 0 || rem < (u32)plen)
                        break;
                    if (valid > 0 && eapol_type == 0 && plen != 0) {
                        int slot;
                        for (slot = 0; slot < 4; slot++) {
                            if (ctx->pkt_pool[slot].in_use == 0)
                                break;
                        }
                        if (slot != 4) {
                            Eap8021xPktNode *node = &ctx->pkt_pool[slot];
                            sceKernelMemset(node, 0, sizeof(Eap8021xPktNode));
                            node->in_use = 1;
                            node->len = plen;
                            sceKernelMemcpy(node->data, body, (SceSize)plen);
                            if (ctx->pkt_tail != NULL)
                                ctx->pkt_tail->next = node;
                            else
                                ctx->pkt_head = node;
                            ctx->pkt_tail = node;
                        }
                    }
                    if (next_pkt == NULL || rem < (u32)(next_pkt - cur))
                        break;
                    rem -= (u32)(next_pkt - cur);
                    cur = next_pkt;
                }
            }
        }

        if (ctx->pkt_head != NULL) {
            if ((u32)ctx->idle_loops >= 101u)
                sceKernelDelayThread(500000);
            ctx->eapol_eap = 1;
        }

        eap8021x_step_all(ctx);
        if (ctx->stop_flag != 0)
            break;
    }

    eap8021x_notify(ctx, 4, 0);
    if (ctx->alarm_id > 0)
        sceKernelCancelAlarm(ctx->alarm_id);
    return 0;
}

int eap8021x_start(const char *ifname, void (*cb)(int, int), int stack,
                   int prio, int unused, void *cfg)
{
    Eap8021xCtx *ctx_ptr = &g_eap8021x_ctx;
    int ret;
    int bpf_fd;
    SceUID thid;

    (void)unused;

    if (g_eap8021x_started > 0)
        return (int)0x80410F80;

    sceKernelMemset(&g_eap8021x_ctx, 0, sizeof(g_eap8021x_ctx));
    sceKernelMemcpy(&g_eap8021x_ctx.cfg, cfg, sizeof(g_eap8021x_ctx.cfg));
    sceNet_lib_B5CE388A(g_eap8021x_ctx.ifname, ifname, 16);
    g_eap8021x_ctx.cb = cb;

    ret = sceNetGetLocalEtherAddr(g_eap8021x_ctx.sta_mac);
    if (ret >= 0) {
        bpf_fd = eapol_bpf_open(g_eap8021x_ctx.ifname, &g_eap8021x_bpf_prog,
                            g_eap8021x_ctx.sta_mac, 0, 1500, 500000u);
        ret = bpf_fd;
        if (bpf_fd >= 0) {
            g_eap8021x_ctx.bpf_fd = bpf_fd;
            thid = sceKernelCreateThread("sceNet8021xStateMachine", eap8021x_thread,
                                         prio, stack, 0, NULL);
            ret = thid;
            if (thid >= 0) {
                g_eap8021x_ctx.thid = thid;
                ret = sceKernelStartThread(thid, 4, &ctx_ptr);
                if (ret >= 0) {
                    g_eap8021x_started = 1;
                    return 0;
                }
            }
        }
    }

    if (g_eap8021x_ctx.thid >= 0)
        sceKernelDeleteThread(g_eap8021x_ctx.thid);
    if (g_eap8021x_ctx.bpf_fd >= 0)
        eapol_bpf_close(g_eap8021x_ctx.bpf_fd);
    return ret;
}

int eap8021x_stop(void)
{
    SceUID thid = g_eap8021x_ctx.thid;
    int bpf_fd = g_eap8021x_ctx.bpf_fd;

    if (thid >= 0) {
        g_eap8021x_ctx.user_logoff = 1;
        sceKernelDelayThread(100000);
        g_eap8021x_ctx.stop_flag = 1;
        sceKernelWaitThreadEnd(thid, NULL);
        sceKernelDeleteThread(thid);
        sceNetFreeThreadinfo(thid);
    }
    if (bpf_fd >= 0)
        eapol_bpf_close(bpf_fd);
    g_eap8021x_started = 0;
    return 0;
}

int eap8021x_set_eap_flag(int val)
{
    g_eap8021x_ctx.eap_flag = val;
    return 0;
}
