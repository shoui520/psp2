#include "apctl_internal.h"

/* Error codes */
#define APCTL_ERR_STACK_SMALL       ((int)0x80410005)
#define APCTL_ERR_ALREADY_INIT      ((int)0x80410A01)
#define APCTL_ERR_INVALID_ID        ((int)0x80410A02)
#define APCTL_ERR_INVALID_PARAM     ((int)0x80410A03)
#define APCTL_ERR_NOT_CONNECTED     ((int)0x80410A05)
#define APCTL_ERR_WLAN_SWITCH_OFF   ((int)0x80410A06)
#define APCTL_ERR_DISCONNECTED      ((int)0x80410A07)
#define APCTL_ERR_DISASSOC          ((int)0x80410A08)
#define APCTL_ERR_NOT_FOUND         ((int)0x80410A09)
#define APCTL_ERR_SUSPEND           ((int)0x80410A0A)
#define APCTL_ERR_TIMEOUT           ((int)0x80410A0B)
#define APCTL_ERR_NO_IP             ((int)0x80410A0C)
#define APCTL_ERR_NOT_INIT          ((int)0x80410A0D)
#define APCTL_ERR_STACK_TOO_SMALL   ((int)0x80410A0E)
#define APCTL_ERR_TOO_MANY_HANDLERS ((int)0x80410A0F)
#define APCTL_ERR_BUSY              ((int)0x80410A80)
#define APCTL_ERR_BAD_INDEX         ((int)0x80410A82)
#define APCTL_ERR_CANCELED          ((int)0x80410A83)
#define APCTL_ERR_INVALID_IP_MODE   ((int)0x80410A84)
#define APCTL_ERR_SSID_TOO_LONG     ((int)0x80410A85)
#define APCTL_ERR_WPA_CONFIG        ((int)0x80410A88)
#define APCTL_ERR_ROAM_CHECK        ((int)0x80410A89)

typedef struct ApctlHandler {
    int id;
    void (*fn)(int old_state, int new_state, int event, int err, void *arg);
    void *arg;
    u32 gp;
} ApctlHandler;

/* Main Apctl context (5408 bytes = 0x1520, matching original offsets) */
typedef struct ApctlCtx {
    char ifname[16];          /* 0x0000 (0) */
    int state;                /* 0x0010 (4) */
    int event;                /* 0x0014 (5) */
    SceUID thid;              /* 0x0018 (6) */
    SceUID evf;               /* 0x001C (7) */
    int mtx_inited;           /* 0x0020 (8) */
    u32 mtx[8];               /* 0x0024 (9..16) */
    int prio;                 /* 0x0044 (17) */
    u8 scan1_ssid[32];        /* 0x0048 (18..25) */
    u32 scan1_ssid_len;       /* 0x0068 (26) */
    u8 scan1_buf[1920];       /* 0x006C (27..506) */
    u32 scan1_len;            /* 0x07EC (507) */
    u8 scan2_ssid[32];        /* 0x07F0 (508..515) */
    u32 scan2_ssid_len;       /* 0x0810 (516) */
    u8 scan2_buf[1920];       /* 0x0814 (517..996) */
    u32 scan2_len;            /* 0x0F94 (997) */
    int config_id;            /* 0x0F98 (998) */
    int last_error;           /* 0x0F9C (999) */
    int auth_mode;            /* 0x0FA0 (1000) */
    u32 flags;                /* 0x0FA4 (1001) */
    u32 extra_stack;          /* 0x0FA8 (1002) */
    u32 pad_fac;              /* 0x0FAC (1003) */
    int roam_enabled;         /* 0x0FB0 (1004) */
    u32 roam_start_sec;       /* 0x0FB4 (1005) */
    void (*btdun_at_cb)(void); /* 0x0FB8 (1006) */
    u32 btdun_at_gp;          /* 0x0FBC (1007) */
    u8 bss_info[92];          /* 0x0FC0..0x101B (1008..1030) */
    u32 ip_addr;              /* 0x101C (1031) */
    u32 netmask;              /* 0x1020 (1032) */
    u32 gateway;              /* 0x1024 (1033) */
    u32 primary_dns;          /* 0x1028 (1034) */
    u32 secondary_dns;        /* 0x102C (1035) */
    char cfg_name[64];        /* 0x1030 (1036..1051) */
    char cfg_ssid[36];        /* 0x1070 (1052..1060) */
    int cfg_security;         /* 0x1094 (1061) */
    u8 cfg_wep_pad[16];       /* 0x1098 (1062..1065) */
    int cfg_eap_pad;          /* 0x10A8 (1066) */
    int cfg_ip_mode;          /* 0x10AC (1067) */
    char cfg_pppoe_user[128]; /* 0x10B0 (1068..1099) */
    char cfg_pppoe_pass[128]; /* 0x1130 (1100..1131) */
    int cfg_dns_flag;         /* 0x11B0 (1132) */
    char cfg_dns1[16];        /* 0x11B4 (1133..1136) */
    char cfg_dns2[16];        /* 0x11C4 (1137..1140) */
    int cfg_proxy_use;        /* 0x11D4 (1141) */
    char cfg_proxy_url[128];  /* 0x11D8 (1142..1173) */
    u16 cfg_proxy_port;       /* 0x1258 */
    u16 cfg_proxy_pad;        /* 0x125A */
    int cfg_eap_type;         /* 0x125C (1175) */
    char cfg_eap_user[128];   /* 0x1260 (1176..1207) */
    char cfg_eap_pass[128];   /* 0x12E0 (1208..1239) */
    int cfg_wpa_key_type;     /* 0x1360 (1240) */
    u8 cfg_wpa_key[64];       /* 0x1364 (1241..1256) */
    int cfg_browser_flag;     /* 0x13A4 (1257) */
    int cfg_wifisvc_flag;     /* 0x13A8 (1258) */
    u32 cfg_wifisvc_ip;       /* 0x13AC (1259) */
    int cfg_dev_type;         /* 0x13B0 (1260) */
    int cfg_btdun_id;         /* 0x13B4 (1261) */
    char cfg_btdun_init[65];  /* 0x13B8 (1262..1278) */
    char cfg_btdun_dial[33];  /* 0x13F9 */
    u8 cfg_btdun_pad[2];
    char cfg_extra_str[257];  /* 0x141C (1287..) */
    u8 tail_pad[3];
} ApctlCtx;

static int g_apctl_inited = 0;
static ApctlCtx g_apctl;
static ApctlHandler g_handlers_pub[2];
static ApctlHandler g_handlers_lib[2];
static ApctlHandler g_handlers_ext[2];

/* Client active flags */
static ApctlCtx *g_client_ctx = NULL;
static int g_static_ip_active = 0;
static int g_dhcp_active = 0;
static int g_pppoe_active = 0;
static int g_eap_active = 0;
static int g_wpa_active = 0;
static int g_wpa_started = 0;
static int g_btdun_active = 0;

/* Forward declarations */
static int apctl_wlan_attach(ApctlCtx *ctx, u32 *ev_out);
static int apctl_wlan_scan(const char *ifname, const void *ssid, u32 ssid_len, void *buf, u32 *len_inout);
static int apctl_get_bss_list(ApctlCtx *ctx, u32 *size_inout, void *buf, u32 scan_len, void *scan_buf);
static int apctl_get_bss_entry(ApctlCtx *ctx, int id, int code, void *out, u32 scan_len, void *scan_buf);
static int apctl_get_info_internal(ApctlCtx *ctx, int state, int code, void *out, int allow_ext);
static int apctl_load_wlan_info(ApctlCtx *ctx);
static int apctl_check_roam(ApctlCtx *ctx);
static int apctl_wlan_detach(ApctlCtx *ctx, int event);
static int apctl_scan1(ApctlCtx *ctx);
static int apctl_scan2(ApctlCtx *ctx);
static void apctl_scan2_in_state(ApctlCtx *ctx);
static void apctl_poll_scan2_clear(ApctlCtx *ctx);
static int apctl_load_default_config(ApctlCtx *ctx);
static int apctl_wlan_clear(void);
static int apctl_save_ip_param(ApctlCtx *ctx, const void *ip);
static int apctl_core_init(int stack_size, int prio);
static int apctl_core_term(void);
static int apctl_add_handler_internal(void *fn, void *arg, int *id_out, int table);
static int apctl_del_handler_internal(int id, int table);
static int apctl_notify_all(ApctlCtx *ctx, int old_s, int new_s, int ev, int err);
static int apctl_notify_lib(ApctlCtx *ctx, int old_s, int new_s, int ev, int err);
static int apctl_connect_internal(int conn_idx, int mode, int arg2, int bt_id, const char *bt_init, void *bt_cb);
static int apctl_get_info_wrapper(int code, void *out, int allow_ext);
static int apctl_get_state_internal(int *state_out);
static int apctl_disconnect_internal(void);
static int apctl_req_scan1(const char *ssid, u32 len);
static int apctl_req_scan2(const char *ssid, u32 len);
static int apctl_wait_events(ApctlCtx *ctx, u32 mask, u32 *out_bits, int wait);
static int apctl_drain_wlan_events(ApctlCtx *ctx, u32 *out_bits);
static int apctl_clients_init(ApctlCtx *ctx);
static int apctl_clients_term(void);
static int apctl_setup_static_ip(ApctlCtx *ctx);
static int apctl_setup_dhcp(ApctlCtx *ctx);
static int apctl_setup_pppoe(ApctlCtx *ctx);
static int apctl_setup_8021x(ApctlCtx *ctx);
static int apctl_setup_wpa(ApctlCtx *ctx);
static int apctl_setup_btdun_dial(ApctlCtx *ctx);
static int apctl_setup_btdun_ip(ApctlCtx *ctx);
static int apctl_wpa_prepare(ApctlCtx *ctx);
static void apctl_stop_auth_clients(void);
static void apctl_stop_all_clients(ApctlCtx *ctx);
static void apctl_stop_btdun(void);

/* =========================================================================
 * Public API exports
 * ========================================================================= */

int sceNetApctlInit(u32 stackSize, int initPriority)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited > 0)
        return APCTL_ERR_ALREADY_INIT;
    if (stackSize < 0x1400)
        return APCTL_ERR_STACK_TOO_SMALL;
    int ret = apctl_core_init((int)stackSize, initPriority);
    if (ret < 0)
        return ret;
    g_apctl_inited = 1;
    return 0;
}

int sceNetApctlTerm(void)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return 0;
    int ret = apctl_core_term();
    if (ret >= 0)
        g_apctl_inited = 0;
    return ret;
}

int sceNetApctlAddHandler(void *handler, void *arg)
{
    int id = 0;
    if (_sceKernelCheckThreadStack() < 5088)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    int ret = apctl_add_handler_internal(handler, arg, &id, 0);
    return (ret >= 0) ? id : ret;
}

int sceNetApctlDelHandler(int handlerId)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_del_handler_internal(handlerId, 0);
}

int sceNetApctlConnect(int connIndex)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_connect_internal(connIndex, 1, 0, 0, NULL, NULL);
}

int sceNetApctlDisconnect(void)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_disconnect_internal();
}

int sceNetApctl_lib_B3CF6849(void)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_req_scan1(NULL, 0);
}

int sceNetApctl_lib_A9721D11(const char *ssid)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    u32 len = (ssid != NULL) ? (u32)sceNet_lib_D8722983(ssid) : 0;
    return apctl_req_scan1(ssid, len);
}

int sceNetApctlScanSSID2(const char *ssid)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    u32 len = (ssid != NULL) ? (u32)sceNet_lib_D8722983(ssid) : 0;
    return apctl_req_scan2(ssid, len);
}

int sceNetApctl_lib_0C7FFA5C(u32 *size_inout, void *buf)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (size_inout == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_list(&g_apctl, size_inout, buf, g_apctl.scan1_len, g_apctl.scan1_buf);
}

int sceNetApctlGetBSSDescIDList2(u32 *size_inout, void *buf)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (size_inout == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_list(&g_apctl, size_inout, buf, g_apctl.scan2_len, g_apctl.scan2_buf);
}

int sceNetApctl_lib_96BEB231(int id, int code, void *out)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (out == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_entry(&g_apctl, id, code, out, g_apctl.scan1_len, g_apctl.scan1_buf);
}

int sceNetApctlGetBSSDescEntry2(int id, int code, void *out)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (out == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_entry(&g_apctl, id, code, out, g_apctl.scan2_len, g_apctl.scan2_buf);
}

int sceNetApctlGetInfo(int code, void *info)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (info == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_info_wrapper(code, info, 0);
}

int sceNetApctl_lib2_4C19731F(int code, void *info)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (info == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_info_wrapper(code, info, 1);
}

int sceNetApctl_lib2_6D802E3F(int bt_id, const char *bt_init, void *bt_cb)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (bt_init == NULL || bt_cb == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_connect_internal(0, 4, 0, bt_id, bt_init, bt_cb);
}

int sceNetApctlGetState(int *state)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (state == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_state_internal(state);
}

int sceNetApctl_lib_7CFAB990(void *handler, void *arg)
{
    int id = 0;
    if (_sceKernelCheckThreadStack() < 5088)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    int ret = apctl_add_handler_internal(handler, arg, &id, 1);
    return (ret >= 0) ? id : ret;
}

int sceNetApctl_lib_E11BAFAB(int handlerId)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_del_handler_internal(handlerId, 1);
}

int sceNetApctl_E9B2E5E6(void)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_req_scan1(NULL, 0);
}

int sceNetApctl_6BDDCB8C(u32 *size_inout, void *buf)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (size_inout == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_list(&g_apctl, size_inout, buf, g_apctl.scan1_len, g_apctl.scan1_buf);
}

int sceNetApctl_04776994(int id, int code, void *out)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (out == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_entry(&g_apctl, id, code, out, g_apctl.scan1_len, g_apctl.scan1_buf);
}

int sceNetApctl_A7BB73DF(void *handler, void *arg)
{
    int id = 0;
    if (_sceKernelCheckThreadStack() < 5088)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    int ret = apctl_add_handler_internal(handler, arg, &id, 2);
    return (ret >= 0) ? id : ret;
}

int sceNetApctl_6F5D2981(int handlerId)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_del_handler_internal(handlerId, 2);
}

int sceNetApctl_lib_C519EDCD(int enable)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    g_apctl.roam_enabled = enable;
    return 0;
}

int sceNetApctl_lib2_C20A144C(int connIndex, int arg2)
{
    if (_sceKernelCheckThreadStack() < 5088)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_connect_internal(connIndex, 2, arg2, 0, NULL, NULL);
}

int sceNetApctl_lib2_9D352B37(int connIndex)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_connect_internal(connIndex, 3, 0, 0, NULL, NULL);
}

int sceNetApctl_lib2_69745F0A(char **out_ptr)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    *out_ptr = g_apctl.cfg_extra_str;
    return 0;
}

int sceNetApctl_lib2_3895B348(const char *str)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    sceNet_lib_B5CE388A(g_apctl.cfg_extra_str, str, 257);
    g_apctl.cfg_extra_str[256] = '\0';
    return 0;
}

/* =========================================================================
 * Internal WLAN / Scan / Info helpers
 * ========================================================================= */

static int apctl_wlan_attach(ApctlCtx *ctx, u32 *ev_out)
{
    int tries = 0;
    if (ev_out != NULL)
        *ev_out = 0;

    while (tries < 5) {
        u32 ret = (u32)sceWlanDevAttach();
        if (ret == 0 || ret == 0x80410D0Fu)
            return 0;
        tries++;
        if ((s32)ret < 0 && ret != 0x80410D0Eu)
            return (int)ret;
        if (sceKernelPollEventFlag(ctx->evf, 2, 33, ev_out) == 0 && (*ev_out & 2))
            return 0;
        sceKernelDelayThread(1000000);
    }
    return APCTL_ERR_TIMEOUT;
}

static int apctl_wlan_scan(const char *ifname, const void *ssid, u32 ssid_len, void *buf, u32 *len_inout)
{
    u8 req[76];
    u32 status = 0;
    int i;

    sceKernelMemset(req, 0, sizeof(req));
    if (ssid_len != 0) {
        sceKernelMemcpy(req + 28, ssid, ssid_len);
        req[24] = (u8)ssid_len;
    }
    *(u32 *)req = 1;
    for (i = 0; i < 14; i++)
        req[10 + i] = (u8)(i + 1);
    *(u32 *)(req + 60) = 1;
    *(u32 *)(req + 64) = 0;
    *(u32 *)(req + 68) = 6;
    *(u32 *)(req + 72) = 100;

    sceKernelMemset(buf, 0, *len_inout);
    return sceNet_lib_7BA3ED91(ifname, req, len_inout, buf, &status);
}

static int apctl_get_bss_list(ApctlCtx *ctx, u32 *size_inout, void *buf, u32 scan_len, void *scan_buf)
{
    (void)ctx;
    u32 *prev = NULL;
    u32 count = 0;
    if (scan_len != 0) {
        u32 *cur = (u32 *)scan_buf;
        int id = 1;
        while (cur != NULL) {
            if (cur[11] == 1) {
                if (buf != NULL) {
                    if (*size_inout < (count + 1) * 8)
                        break;
                    u32 *ent = (u32 *)((u8 *)buf + count * 8);
                    if (prev != NULL)
                        prev[0] = (u32)ent;
                    sceKernelMemset(ent, 0, 8);
                    ent[1] = (u32)id;
                    prev = ent;
                }
                count++;
            }
            cur = (u32 *)cur[0];
            id++;
        }
    }
    *size_inout = count * 8;
    return 0;
}

static int apctl_get_bss_entry(ApctlCtx *ctx, int id, int code, void *out, u32 scan_len, void *scan_buf)
{
    (void)ctx;
    if ((u32)id >= 20)
        return APCTL_ERR_BAD_INDEX;
    if (scan_len == 0)
        return APCTL_ERR_NOT_FOUND;

    u8 *cur = (u8 *)scan_buf;
    int idx = 1;
    while (cur != NULL && idx < id) {
        cur = *(u8 **)cur;
        idx++;
    }
    if (cur == NULL)
        return APCTL_ERR_NOT_FOUND;

    switch (code) {
    case 0:
        sceKernelMemcpy(out, cur + 4, 6);
        return 0;
    case 1:
        sceKernelMemcpy(out, cur + 12, 33);
        return 0;
    case 2:
        *(u32 *)out = cur[11];
        return 0;
    case 3:
        *(u8 *)out = cur[10];
        return 0;
    case 4:
        *(u8 *)out = cur[76];
        return 0;
    case 5: {
        u32 sec;
        if ((*(u16 *)(cur + 66) & 0x10) == 0) {
            sec = 0;
        } else if (cur[84] == 1) {
            if ((cur[85] & 4) && (cur[86] & 4) && (cur[87] & 2))
                sec = 2;
            else if ((cur[85] & 8) && (cur[86] & 8) && (cur[87] & 2))
                sec = 4;
            else if (cur[85] & 3)
                sec = 1;
            else
                sec = 3;
        } else {
            sec = (cur[90] == 1) ? 3 : 1;
        }
        *(u32 *)out = sec;
        return 0;
    }
    default:
        return APCTL_ERR_INVALID_ID;
    }
}

static int apctl_get_info_internal(ApctlCtx *ctx, int state, int code, void *out, int allow_ext)
{
    (void)state;
    u8 *base = (u8 *)ctx;
    switch (code) {
    case 0:
        sceNet_lib_80C9F02A((char *)out, (const char *)(base + 4144));
        return 0;
    case 1:
        sceKernelMemcpy(out, base + 4032, 6);
        return 0;
    case 2:
        sceKernelMemcpy(out, base + 4040, 32);
        return 0;
    case 3:
        *(u32 *)out = base[4039];
        return 0;
    case 4:
        switch (*(u32 *)(base + 4244)) {
        case 0: *(u32 *)out = 0; break;
        case 1: case 2: *(u32 *)out = 1; break;
        case 3: *(u32 *)out = 2; break;
        case 4: *(u32 *)out = 3; break;
        default: break;
        }
        return 0;
    case 5:
        *(u8 *)out = base[4012];
        return 0;
    case 6:
        *(u8 *)out = base[4038];
        return 0;
    case 7:
        *(u8 *)out = base[4264];
        return 0;
    case 8: case 9: case 10: case 11: case 12: {
        static const u32 offs[5] = { 4124, 4128, 4132, 4136, 4140 };
        if ((ctx->flags & 0x40) == 0)
            return APCTL_ERR_NO_IP;
        if (sceNetInetInetNtop(2, base + offs[code - 8], (char *)out, 16) == NULL)
            return APCTL_ERR_INVALID_PARAM;
        return 0;
    }
    case 13: *(u32 *)out = *(u32 *)(base + 4564); return 0;
    case 14: sceNet_lib_80C9F02A((char *)out, (const char *)(base + 4568)); return 0;
    case 15: *(u16 *)out = *(u16 *)(base + 4696); return 0;
    case 16: *(u32 *)out = *(u32 *)(base + 4700); return 0;
    case 17: *(u32 *)out = *(u32 *)(base + 5028); return 0;
    case 18: *(u32 *)out = *(u32 *)(base + 5032); return 0;
    case 19:
        if (!allow_ext) return APCTL_ERR_INVALID_ID;
        *(u32 *)out = *(u32 *)(base + 5040);
        return 0;
    case 20:
        if (!allow_ext) return APCTL_ERR_INVALID_ID;
        *(u32 *)out = *(u32 *)(base + 5044);
        return 0;
    case 21:
        if (!allow_ext) return APCTL_ERR_INVALID_ID;
        sceNet_lib_80C9F02A((char *)out, (const char *)(base + 5048));
        return 0;
    case 22:
        if (!allow_ext) return APCTL_ERR_INVALID_ID;
        sceNet_lib_80C9F02A((char *)out, (const char *)(base + 5113));
        return 0;
    default:
        return APCTL_ERR_INVALID_ID;
    }
}

static int apctl_load_wlan_info(ApctlCtx *ctx)
{
    u8 info[144];
    u8 *base = (u8 *)ctx;
    int ret = sceWlanDrv_lib_CAC1FB0D(info);
    if (ret < 0)
        return ret;

    sceKernelMemcpy(base + 4032, info, 6);
    *(u32 *)(base + 4072) = *(u32 *)(info + 112);
    *(u16 *)(base + 4104) = *(u16 *)(info + 106);
    base[4038] = info[108];
    base[4039] = info[39];
    sceNet_lib_B5CE388A((char *)(base + 4040), (const char *)(info + 6), 32);
    sceNet_lib_B5CE388A((char *)(base + 4144), (const char *)(info + 40), 64);
    *(u32 *)(base + 4124) = *(u32 *)(info + 124);
    *(u32 *)(base + 4128) = *(u32 *)(info + 128);
    *(u32 *)(base + 4132) = *(u32 *)(info + 132);
    *(u32 *)(base + 4136) = *(u32 *)(info + 136);
    *(u32 *)(base + 4140) = *(u32 *)(info + 140);
    base[4012] = info[106];
    *(u32 *)(base + 4244) = *(u32 *)(info + 116);
    sceNetInetInetNtop(2, info + 124, (char *)(base + 4272), 16);
    sceNetInetInetNtop(2, info + 128, (char *)(base + 4288), 16);
    sceNetInetInetNtop(2, info + 132, (char *)(base + 4304), 16);
    sceNetInetInetNtop(2, info + 136, (char *)(base + 4532), 16);
    sceNetInetInetNtop(2, info + 140, (char *)(base + 4548), 16);
    return 0;
}

static int apctl_check_roam(ApctlCtx *ctx)
{
    u8 *base = (u8 *)ctx;
    u8 sig;
    u8 scan_buf[192];
    u32 scan_len = sizeof(scan_buf);
    u32 ev = 0;

    int ret = sceNet_lib_F355C73B(ctx->ifname, 5, &sig);
    if (ret < 0)
        return ret;
    base[4012] = sig;
    ctx->flags |= 0x80;
    ret = apctl_wlan_scan(ctx->ifname, base + 4040, base[4039], scan_buf, &scan_len);
    ctx->flags &= ~0x80u;
    apctl_wait_events(ctx, 42, &ev, 0);
    if (ev & 2)
        return APCTL_ERR_CANCELED;
    if ((ev & 8) && ctx->last_error != APCTL_ERR_ROAM_CHECK)
        return ctx->last_error;
    if (ret < 0)
        return ret;

    u8 *cur = scan_buf;
    while (cur != NULL) {
        if (*(u32 *)(cur + 44) == 1 && cur[11] == base[4039] &&
            sceNet_lib_E0A81C7C(cur + 12, base + 4040, cur[11]) == 0) {
            return (sceNet_lib_E0A81C7C(cur + 4, base + 4032, 6) != 0) ? 1 : 0;
        }
        cur = *(u8 **)cur;
    }
    return 0;
}

static void apctl_wait_suspend_resume(ApctlCtx *ctx)
{
    u32 bits, ev, arg;
    for (;;) {
        if (sceKernelWaitEventFlag(ctx->evf, 32, 33, &bits, NULL) < 0)
            return;
        for (;;) {
            int ret = sceNet_lib_D1BE2CE9(ctx->ifname, &ev, &arg);
            if (ret == (int)0x80410184)
                break;
            if (ret < 0)
                return;
            if (ev == 8) {
                ctx->flags &= ~2u;
                return;
            }
        }
    }
}

static int apctl_wlan_detach(ApctlCtx *ctx, int event)
{
    if (event == 6 && (ctx->flags & 0xA) == 2)
        apctl_wait_suspend_resume(ctx);
    sceWlanDevDetach();
    return 0;
}

static int apctl_scan1(ApctlCtx *ctx)
{
    int ret = sceNet_lib_5216CBF5(ctx->ifname);
    if (ret >= 0) {
        ctx->scan1_len = 1920;
        ret = apctl_wlan_scan(ctx->ifname, ctx->scan1_ssid, ctx->scan1_ssid_len, ctx->scan1_buf, &ctx->scan1_len);
    }
    sceNet_lib_D2422E4D(ctx->ifname);
    return ret;
}

static int apctl_scan2(ApctlCtx *ctx)
{
    int ret = sceNet_lib_5216CBF5(ctx->ifname);
    if (ret >= 0) {
        ctx->scan2_len = 1920;
        ret = apctl_wlan_scan(ctx->ifname, ctx->scan2_ssid, ctx->scan2_ssid_len, ctx->scan2_buf, &ctx->scan2_len);
    }
    sceNet_lib_D2422E4D(ctx->ifname);
    return ret;
}

static void apctl_scan2_in_state(ApctlCtx *ctx)
{
    u8 sig;
    if (sceNet_lib_F355C73B(ctx->ifname, 5, &sig) >= 0) {
        ((u8 *)ctx)[4012] = sig;
        ctx->flags |= 0x80;
    }
    ctx->scan2_len = 1920;
    int ret = apctl_wlan_scan(ctx->ifname, ctx->scan2_ssid, ctx->scan2_ssid_len, ctx->scan2_buf, &ctx->scan2_len);
    ctx->flags &= ~0x80u;
    apctl_notify_all(ctx, ctx->state, ctx->state, 11, ret);
    ctx->flags &= ~0x200u;
}

static void apctl_poll_scan2_clear(ApctlCtx *ctx)
{
    u32 ev = 0;
    apctl_wait_events(ctx, 512, &ev, 0);
}

static int apctl_load_default_config(ApctlCtx *ctx)
{
    u8 *base = (u8 *)ctx;
    sceNet_lib_80C9F02A((char *)(base + 4144), "Net Config1");
    sceNet_lib_80C9F02A((char *)(base + 4208), "ABCDEFG");
    *(u32 *)(base + 4268) = 1;
    *(u32 *)(base + 4244) = 0;
    *(u32 *)(base + 4264) = 0;
    *(u32 *)(base + 4528) = 0;
    sceNet_lib_80C9F02A((char *)(base + 4532), "192.168.11.1");
    sceNet_lib_80C9F02A((char *)(base + 4548), "192.168.11.1");
    *(u32 *)(base + 5028) = 1;
    *(u32 *)(base + 5032) = 0;
    *(u32 *)(base + 4564) = 0;
    *(u32 *)(base + 4700) = 0;
    ctx->auth_mode = 0;
    return 0;
}

static int apctl_wlan_clear(void)
{
    return sceWlanDrv_lib_80B7162E();
}

static int apctl_save_ip_param(ApctlCtx *ctx, const void *ip)
{
    char buf[128];
    sceUtility_netparam_internal_61D0686E(0);
    int ret = sceUtility_netparam_internal_6B1BC62C(0);
    if (ret < 0)
        return ret;
    ret = sceUtility_netparam_internal_4CB183A4(ctx->config_id, 0);
    if (ret < 0)
        return ret;
    if (sceNetInetInetNtop(2, ip, buf, 16) == NULL)
        buf[0] = '\0';
    ret = sceUtility_netparam_internal_239F260D(28, buf);
    if (ret < 0)
        return ret;
    return sceUtility_netparam_internal_4CB183A4(0, ctx->config_id);
}

/* =========================================================================
 * State machine handlers & thread
 * ========================================================================= */

static int apctl_state_scan2_idle(ApctlCtx *ctx, int *next_state, int *event)
{
    u32 ev = 0;
    sceNet_lib_B5CE388A(ctx->ifname, "wlan", 16);
    int ret = apctl_wlan_attach(ctx, &ev);
    if (ret < 0) {
        *event = 6;
        ctx->last_error = ret;
    } else if (ev & 2) {
        *event = 5;
    } else {
        sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
        ret = sceNet_lib_AB7DD9A5(ctx->ifname, ctx->evf, 32);
        if (ret >= 0 && sceWlanGetSwitchState() != 0 && (ret = apctl_scan2(ctx)) >= 0) {
            *event = 11;
        } else {
            if (ret >= 0)
                ret = APCTL_ERR_WLAN_SWITCH_OFF;
            *event = 6;
        }
        ctx->last_error = ret;
    }
    apctl_wlan_detach(ctx, *event);
    if (*event != 5)
        *event = 11;
    *next_state = 0;
    ctx->flags &= ~0x200u;
    return 0;
}

static int apctl_state_disconnected(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    u32 bits = 0;
    (void)prev;
    for (;;) {
        ctx->flags &= ~2u;
        sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
        sceKernelClearEventFlag(ctx->evf, 583);
        ctx->roam_start_sec = 0;
        ctx->flags &= 0xFFFFF28Fu;
        ctx->last_error = 0;
        sceKernelMemset((u8 *)ctx + 4144, 0, 1004);
        sceKernelMemset((u8 *)ctx + 5148, 0, 257);
        sceKernelWaitEventFlag(ctx->evf, 0xFFFFFFFFu, 33, &bits, NULL);
        if ((bits & 0x200) == 0)
            break;
        apctl_state_scan2_idle(ctx, next_state, event);
        if (*event == 11) {
            apctl_notify_all(ctx, ctx->state, *next_state, 11, ctx->last_error);
        } else if (*event == 5) {
            bits |= 2;
        }
        bits &= ~0x200u;
        if (bits != 0)
            break;
    }
    if (bits & 0x40) {
        ctx->flags |= 4;
    } else if (bits & 2) {
        *next_state = 0;
        *event = 5;
        ctx->flags |= 1;
    } else if (bits & 4) {
        *next_state = 1;
        *event = 1;
    } else if (bits & 0x801) {
        if (bits & 0x800)
            ctx->flags |= 0x800;
        *next_state = 2;
        *event = 0;
    }
    return 0;
}

static int apctl_state_scanning(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    u32 ev = 0;
    (void)prev;
    sceNet_lib_B5CE388A(ctx->ifname, "wlan", 16);
    int ret = apctl_wlan_attach(ctx, &ev);
    if (ret >= 0) {
        if (ev & 2) {
            *event = 5;
            goto done;
        }
        sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
        ret = sceNet_lib_AB7DD9A5(ctx->ifname, ctx->evf, 32);
        if (ret >= 0) {
            if (sceWlanGetSwitchState() == 0) {
                ret = APCTL_ERR_WLAN_SWITCH_OFF;
            } else {
                ret = apctl_scan1(ctx);
                ev = 0;
                apctl_wait_events(ctx, 34, &ev, 0);
                if (ev != 0) {
                    *event = (ev & 2) ? 5 : 6;
                    goto done;
                }
                if (ret >= 0) {
                    *event = 2;
                    goto done;
                }
            }
        }
    }
    *event = 6;
    ctx->last_error = ret;
done:
    *next_state = 0;
    ctx->flags |= 1;
    apctl_wlan_detach(ctx, *event);
    return 0;
}

static int apctl_state_btdun_joining(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    (void)prev;
    sceNet_lib_B5CE388A(ctx->ifname, "btdun", 16);
    int ret = sceBtdun_lib_412DA4A1();
    if (ret >= 0) {
        sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
        ret = sceNet_lib_AB7DD9A5(ctx->ifname, ctx->evf, 32);
        if (ret >= 0) {
            ret = apctl_setup_btdun_dial(ctx);
            if (ret >= 0) {
                if (ctx->flags & 0x800) {
                    *next_state = 0;
                    *event = 12;
                } else {
                    *next_state = 3;
                    *event = 3;
                }
                goto check_fail;
            }
        }
    }
    *next_state = 0;
    if (ret == APCTL_ERR_CANCELED) {
        *event = 5;
    } else {
        *event = 6;
        ctx->last_error = ret;
    }
check_fail:
    if (*next_state == 0) {
        apctl_stop_btdun();
        sceBtdun_lib_523FA5A5();
        ctx->flags |= 1;
    }
    return 0;
}

static int apctl_state_joining(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    u32 ev = 0;
    int ret = apctl_load_default_config(ctx);
    if (ret < 0)
        goto fail;
    if (sceWlanGetSwitchState() == 0) {
        ret = APCTL_ERR_WLAN_SWITCH_OFF;
        goto fail;
    }
    if (ctx->cfg_dev_type == 2)
        return apctl_state_btdun_joining(ctx, prev, next_state, event);

    sceNet_lib_B5CE388A(ctx->ifname, "wlan", 16);
    if ((ctx->flags & 0x10) == 0) {
        ret = apctl_wlan_attach(ctx, &ev);
        if (ret < 0)
            goto fail;
        if (ev != 0)
            goto ev_abort;
    }

    sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
    ret = sceNet_lib_AB7DD9A5(ctx->ifname, ctx->evf, 32);
    if (ret < 0)
        goto fail;

    if (ctx->auth_mode == 2 || ctx->auth_mode == 4) {
        ret = apctl_wpa_prepare(ctx);
        if (ret < 0)
            goto fail;
    }

    ev = 0;
    ret = apctl_wait_events(ctx, 42, &ev, 0);
    if (ev != 0)
        goto ev_abort;

    for (;;) {
        ret = apctl_load_wlan_info(ctx);
        apctl_poll_scan2_clear(ctx);
        ev = 0;
        apctl_wait_events(ctx, 42, &ev, 0);
        if (ev != 0) {
            if (ev & 2) {
                *next_state = 0;
                *event = 5;
                goto finish;
            }
            if (ctx->last_error != APCTL_ERR_ROAM_CHECK) {
                *next_state = 0;
                *event = 6;
                goto finish;
            }
        }
        if (ret >= 0)
            break;
        if (ctx->flags & 0x10) {
            if ((ctx->flags & 0x20) == 0 ||
                (ret != (int)0x80410106 && ret != (int)0x80410107 &&
                 ret != (int)0x80410D07 && ret != (int)0x80410D09 &&
                 ret != (int)0x80410D11) ||
                sceNet_lib_75D9985C() >= ctx->roam_start_sec + 10) {
                ret = APCTL_ERR_DISCONNECTED;
                goto fail;
            }
        } else {
            if ((u32)(ctx->cfg_security - 1) >= 2 || ret != (int)0x80410D09)
                goto fail;
        }
    }

    if (ctx->auth_mode == 2) {
        *next_state = 6;
    } else if (ctx->auth_mode == 1 || ctx->auth_mode == 4) {
        *next_state = 5;
    } else if (ctx->flags & 0x40) {
        *next_state = 4;
        ctx->flags &= ~0x30u;
    } else {
        *next_state = 3;
    }
    *event = 3;
    goto finish;

ev_abort:
    *next_state = 0;
    *event = (ev & 2) ? 5 : 6;
    goto finish;

fail:
    *next_state = 0;
    *event = 6;
    ctx->last_error = ret;

finish:
    if (*next_state == 0) {
        ctx->flags &= ~0x30u;
        apctl_stop_all_clients(ctx);
        apctl_wlan_clear();
        apctl_wlan_detach(ctx, *event);
        ctx->flags |= 1;
    }
    return 0;
}

static int apctl_state_eap(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    (void)prev;
    int ret = apctl_setup_8021x(ctx);
    if (ret >= 0) {
        if (ctx->auth_mode == 4) {
            *next_state = 6;
        } else {
            eap8021x_set_eap_flag(1);
            if (ctx->flags & 0x40) {
                *next_state = 4;
                ctx->flags &= ~0x30u;
            } else {
                *next_state = 3;
            }
        }
        *event = 8;
    } else {
        *next_state = 0;
        if (ret == APCTL_ERR_CANCELED) {
            *event = 5;
        } else {
            *event = 6;
            ctx->last_error = ret;
        }
    }

    if (*next_state == 0) {
        if (*event == 6 && (ctx->last_error == APCTL_ERR_DISASSOC || ctx->last_error == APCTL_ERR_ROAM_CHECK)) {
            ctx->flags |= 0x10;
            apctl_stop_auth_clients();
            apctl_wlan_clear();
            *next_state = 2;
            *event = 10;
            return 0;
        }
        ctx->flags &= ~0x30u;
        apctl_stop_all_clients(ctx);
        apctl_wlan_clear();
        apctl_wlan_detach(ctx, *event);
        ctx->flags |= 1;
    }
    return 0;
}

static int apctl_state_wpa(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    (void)prev;
    int ret = apctl_setup_wpa(ctx);
    if (ret >= 0) {
        if (ctx->auth_mode == 4)
            eap8021x_set_eap_flag(1);
        if (ctx->flags & 0x40) {
            *next_state = 4;
            ctx->flags &= ~0x30u;
        } else {
            *next_state = 3;
        }
        *event = 9;
    } else {
        *next_state = 0;
        if (ret == APCTL_ERR_CANCELED) {
            *event = 5;
        } else {
            *event = 6;
            ctx->last_error = ret;
        }
    }

    if (*next_state == 0) {
        if (*event == 6 && (ctx->last_error == APCTL_ERR_DISASSOC || ctx->last_error == APCTL_ERR_ROAM_CHECK)) {
            ctx->flags |= 0x10;
            apctl_stop_auth_clients();
            apctl_wlan_clear();
            *next_state = 2;
            *event = 10;
            return 0;
        }
        ctx->flags &= ~0x30u;
        apctl_stop_all_clients(ctx);
        apctl_wlan_clear();
        apctl_wlan_detach(ctx, *event);
        ctx->flags |= 1;
    }
    return 0;
}

static int apctl_state_getting_ip(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    int ret;
    (void)prev;
    if (ctx->cfg_dev_type == 2) {
        ret = apctl_setup_btdun_ip(ctx);
    } else if (ctx->cfg_ip_mode == 0) {
        ret = apctl_setup_dhcp(ctx);
    } else if (ctx->cfg_ip_mode == 1) {
        ret = apctl_setup_static_ip(ctx);
    } else if (ctx->cfg_ip_mode == 2) {
        ret = apctl_setup_pppoe(ctx);
    } else {
        ret = APCTL_ERR_INVALID_IP_MODE;
    }

    if (ret < 0) {
        *next_state = 0;
        if (ret == APCTL_ERR_CANCELED) {
            *event = 5;
        } else {
            *event = 6;
            ctx->last_error = ret;
        }
    } else {
        *next_state = 4;
        *event = 4;
        if (ctx->cfg_wifisvc_flag == 3)
            apctl_save_ip_param(ctx, &ctx->ip_addr);
        ctx->flags = (ctx->flags & ~0x70u) | 0x40;
    }

    if (*next_state == 0) {
        if (*event == 6 && (ctx->last_error == APCTL_ERR_DISASSOC || ctx->last_error == APCTL_ERR_ROAM_CHECK)) {
            ctx->flags |= 0x10;
            apctl_stop_all_clients(ctx);
            apctl_wlan_clear();
            *next_state = 2;
            *event = 10;
            return 0;
        }
        ctx->flags &= ~0x30u;
        apctl_stop_all_clients(ctx);
        if (ctx->cfg_dev_type == 2) {
            sceBtdun_lib_523FA5A5();
        } else {
            apctl_wlan_clear();
            apctl_wlan_detach(ctx, *event);
        }
        ctx->flags |= 1;
    }
    return 0;
}

static int apctl_state_got_ip(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    u32 ev = 0;
    (void)prev;
    ctx->roam_start_sec = 0;

    for (;;) {
        for (;;) {
            ev = 0;
            apctl_wait_events(ctx, 554, &ev, 1);
            if ((ev & 0x200) == 0)
                break;
            apctl_scan2_in_state(ctx);
            ev &= ~0x200u;
            if (ev != 0)
                break;
        }
        if (ev & 2) {
            *event = 5;
            break;
        }
        if ((ev & 8) == 0)
            break;
        if (ctx->last_error == APCTL_ERR_DISASSOC) {
            ctx->flags |= 0x10;
            apctl_stop_auth_clients();
            apctl_wlan_clear();
            *next_state = 2;
            *event = 10;
            return 0;
        }
        if (ctx->last_error != APCTL_ERR_ROAM_CHECK) {
            *event = 6;
            break;
        }
        int roam = apctl_check_roam(ctx);
        if (roam == APCTL_ERR_CANCELED) {
            *event = 5;
            break;
        }
        if (roam == 0)
            continue;
        if (roam > 0) {
            if ((ctx->flags & 0x20) == 0) {
                ctx->flags |= 0x20;
                ctx->roam_start_sec = sceNet_lib_75D9985C();
            }
            ctx->flags |= 0x10;
            apctl_stop_auth_clients();
            apctl_wlan_clear();
            *next_state = 2;
            *event = 10;
            return 0;
        }
        *event = 6;
        break;
    }

    ctx->flags &= ~0x30u;
    apctl_stop_all_clients(ctx);
    if (ctx->cfg_dev_type == 2) {
        sceBtdun_lib_523FA5A5();
    } else {
        apctl_wlan_clear();
        apctl_wlan_detach(ctx, *event);
    }
    *next_state = 0;
    ctx->flags |= 1;
    return 0;
}

typedef int (*ApctlStateFn)(ApctlCtx *, int, int *, int *);
static const ApctlStateFn g_apctl_states[7] = {
    apctl_state_disconnected,
    apctl_state_scanning,
    apctl_state_joining,
    apctl_state_getting_ip,
    apctl_state_got_ip,
    apctl_state_eap,
    apctl_state_wpa
};

static int apctl_thread(SceSize args, void *argp)
{
    ApctlCtx *ctx = *(ApctlCtx **)argp;
    int prev = 0;
    int next = 0, ev = 0;
    (void)args;

    for (;;) {
        g_apctl_states[ctx->state](ctx, prev, &next, &ev);
        if (ctx->flags & 4)
            break;
        prev = ctx->state;
        ctx->state = next;
        ctx->event = ev;
        int err = (ev == 6 || ev == 11) ? ctx->last_error : 0;
        apctl_notify_all(ctx, prev, ctx->state, ctx->event, err);
    }
    sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
    return 0;
}

/* =========================================================================
 * Core lifecycle, handlers, event flags
 * ========================================================================= */

static int apctl_core_init(int stack_size, int prio)
{
    struct { u32 size; SceUID mpid; } opt;
    ApctlCtx *ctx = &g_apctl;
    int ret;

    ret = apctl_clients_init(ctx);
    if (ret < 0)
        goto fail;

    sceKernelMemset(g_handlers_pub, 0, sizeof(g_handlers_pub));
    sceKernelMemset(g_handlers_lib, 0, sizeof(g_handlers_lib));
    sceKernelMemset(g_handlers_ext, 0, sizeof(g_handlers_ext));
    sceKernelMemset(ctx, 0, sizeof(ApctlCtx));
    ctx->thid = -1;
    ctx->evf = -1;
    ctx->flags |= 1;
    ctx->prio = prio;
    ctx->roam_enabled = 1;
    sceNet_lib_B5CE388A(ctx->ifname, "wlan", 16);

    ret = sceKernelCreateLwMutex(ctx->mtx, "SceNetApctl", 0x200, 0, NULL);
    if (ret < 0)
        goto fail;
    ctx->mtx_inited = 1;

    ret = sceKernelCreateEventFlag("SceNetApctl", 0, 0, NULL);
    if (ret < 0)
        goto fail;
    ctx->evf = ret;

    opt.size = 8;
    ret = sceKernelQueryMemoryInfo(ctx, &opt.mpid, NULL);
    if (ret < 0)
        goto fail;

    ret = sceKernelCreateThread("SceNetApctl", apctl_thread, ctx->prio, stack_size, 0, &opt);
    if (ret < 0)
        goto fail;
    ctx->thid = ret;

    ret = sceKernelStartThread(ctx->thid, 4, &ctx);
    if (ret < 0)
        goto fail;

    ctx->extra_stack = (u32)(stack_size - 5120);
    if (opt.mpid == 5)
        ctx->flags |= 8;
    return 0;

fail:
    if (ctx->thid >= 0) {
        sceKernelTerminateThread(ctx->thid);
        sceKernelDeleteThread(ctx->thid);
        sceNetFreeThreadinfo(ctx->thid);
        ctx->thid = -1;
    }
    if (ctx->evf >= 0) {
        sceKernelDeleteEventFlag(ctx->evf);
        ctx->evf = -1;
    }
    if (ctx->mtx_inited == 1) {
        sceKernelDeleteLwMutex(ctx->mtx);
        ctx->mtx_inited = 0;
    }
    apctl_clients_term();
    return ret;
}

static int apctl_core_term(void)
{
    if ((g_apctl.flags & 1) == 0)
        sceNetApctlDisconnect();
    sceKernelSetEventFlag(g_apctl.evf, 64);
    if (g_apctl.thid >= 0) {
        int ret = sceKernelWaitThreadEnd(g_apctl.thid, NULL);
        if (ret < 0)
            return ret;
        sceKernelDeleteThread(g_apctl.thid);
        sceNetFreeThreadinfo(g_apctl.thid);
        g_apctl.thid = -1;
    }
    if (g_apctl.evf >= 0) {
        sceKernelDeleteEventFlag(g_apctl.evf);
        g_apctl.evf = -1;
    }
    if (g_apctl.mtx_inited == 1) {
        sceKernelDeleteLwMutex(g_apctl.mtx);
        g_apctl.mtx_inited = 0;
    }
    apctl_clients_term();
    return 0;
}

static ApctlHandler *get_handler_table(int table)
{
    if (table == 1) return g_handlers_lib;
    if (table == 2) return g_handlers_ext;
    return g_handlers_pub;
}

static int apctl_add_handler_internal(void *fn, void *arg, int *id_out, int table)
{
    int ret = sceKernelLockLwMutex(g_apctl.mtx, 1, NULL);
    if (ret < 0)
        return ret;
    ApctlHandler *tbl = get_handler_table(table);
    int i;
    for (i = 0; i < 2; i++) {
        if (tbl[i].fn == NULL)
            break;
    }
    if (i == 2) {
        ret = APCTL_ERR_TOO_MANY_HANDLERS;
    } else {
        tbl[i].id = i;
        tbl[i].fn = (void (*)(int, int, int, int, void *))fn;
        tbl[i].arg = arg;
        if (sceKernelGetModuleGPByAddress(fn, &tbl[i].gp) < 0)
            tbl[i].gp = 0;
        *id_out = i;
    }
    sceKernelUnlockLwMutex(g_apctl.mtx, 1);
    return ret;
}

static int apctl_del_handler_internal(int id, int table)
{
    int ret = sceKernelLockLwMutex(g_apctl.mtx, 1, NULL);
    if (ret < 0)
        return ret;
    if (id < 0 || id >= 2) {
        ret = APCTL_ERR_BAD_INDEX;
    } else {
        ApctlHandler *tbl = get_handler_table(table);
        if (tbl[id].fn != NULL)
            tbl[id].fn = NULL;
        else
            ret = APCTL_ERR_NOT_FOUND;
    }
    sceKernelUnlockLwMutex(g_apctl.mtx, 1);
    return ret;
}

static int apctl_notify_all(ApctlCtx *ctx, int old_s, int new_s, int ev, int err)
{
    int i;
    sceKernelLockLwMutex(ctx->mtx, 1, NULL);
    for (i = 0; i < 2; i++) {
        if (g_handlers_pub[i].fn != NULL) {
            _sceKernelCheckThreadStack();
            g_handlers_pub[i].fn(old_s, new_s, ev, err, g_handlers_pub[i].arg);
        }
    }
    for (i = 0; i < 2; i++) {
        if (g_handlers_lib[i].fn != NULL) {
            _sceKernelCheckThreadStack();
            g_handlers_lib[i].fn(old_s, new_s, ev, err, g_handlers_lib[i].arg);
        }
    }
    for (i = 0; i < 2; i++) {
        if (g_handlers_ext[i].fn != NULL) {
            _sceKernelCheckThreadStack();
            g_handlers_ext[i].fn(old_s, new_s, ev, err, g_handlers_ext[i].arg);
        }
    }
    sceKernelUnlockLwMutex(ctx->mtx, 1);
    return 0;
}

static int apctl_notify_lib(ApctlCtx *ctx, int old_s, int new_s, int ev, int err)
{
    int i;
    sceKernelLockLwMutex(ctx->mtx, 1, NULL);
    for (i = 0; i < 2; i++) {
        if (g_handlers_lib[i].fn != NULL) {
            _sceKernelCheckThreadStack();
            g_handlers_lib[i].fn(old_s, new_s, ev, err, g_handlers_lib[i].arg);
        }
    }
    sceKernelUnlockLwMutex(ctx->mtx, 1);
    return 0;
}

static int apctl_connect_internal(int conn_idx, int mode, int arg2, int bt_id, const char *bt_init, void *bt_cb)
{
    (void)arg2;
    if ((g_apctl.flags & 1) == 0)
        return APCTL_ERR_BUSY;
    g_apctl.flags &= ~0x501u;
    if (mode == 2) {
        g_apctl.flags |= 0x100;
    } else if (mode == 3) {
        g_apctl.flags |= 0x400;
    } else if (mode == 4) {
        char buf[68];
        sceUtility_netparam_internal_61D0686E(0);
        int ret = sceUtility_netparam_internal_6B1BC62C(0);
        if (ret < 0)
            return ret;
        u32 val = 2;
        ret = sceUtility_netparam_internal_239F260D(17, &val);
        if (ret < 0)
            return ret;
        val = (u32)bt_id;
        ret = sceUtility_netparam_internal_239F260D(29, &val);
        if (ret < 0)
            return ret;
        sceNet_lib_B5CE388A(buf, bt_init, 65);
        buf[64] = '\0';
        ret = sceUtility_netparam_internal_239F260D(30, buf);
        if (ret < 0)
            return ret;
        g_apctl.btdun_at_cb = (void (*)(void))bt_cb;
        if (sceKernelGetModuleGPByAddress(bt_cb, &g_apctl.btdun_at_gp) < 0)
            g_apctl.btdun_at_gp = 0;
        conn_idx = 0;
    }
    g_apctl.config_id = conn_idx;
    sceKernelSetEventFlag(g_apctl.evf, (mode == 4) ? 2048 : 1);
    return 0;
}

static int apctl_get_info_wrapper(int code, void *out, int allow_ext)
{
    int state = 0;
    int ret = apctl_get_state_internal(&state);
    if (ret < 0)
        return ret;
    if (state == 2) {
        if ((g_apctl.flags & 0x10) == 0)
            return APCTL_ERR_NOT_CONNECTED;
        return apctl_get_info_internal(&g_apctl, state, code, out, allow_ext);
    }
    if (state < 2 || state >= 7)
        return APCTL_ERR_NOT_CONNECTED;
    return apctl_get_info_internal(&g_apctl, state, code, out, allow_ext);
}

static int apctl_get_state_internal(int *state_out)
{
    *state_out = g_apctl.state;
    return 0;
}

static void apctl_btdun_at_trampoline(void)
{
    if (g_apctl.btdun_at_cb != NULL)
        g_apctl.btdun_at_cb();
}

static int apctl_disconnect_internal(void)
{
    sceKernelSetEventFlag(g_apctl.evf, 2);
    return 0;
}

static int apctl_req_scan1(const char *ssid, u32 len)
{
    if (len >= 33)
        return APCTL_ERR_SSID_TOO_LONG;
    if ((g_apctl.flags & 1) == 0)
        return APCTL_ERR_BUSY;
    g_apctl.flags &= ~1u;
    if (ssid != NULL) {
        sceKernelMemcpy(g_apctl.scan1_ssid, ssid, len);
        g_apctl.scan1_ssid_len = len;
    } else {
        sceKernelMemset(g_apctl.scan1_ssid, 0, 32);
        g_apctl.scan1_ssid_len = 0;
    }
    sceKernelSetEventFlag(g_apctl.evf, 4);
    return 0;
}

static int apctl_req_scan2(const char *ssid, u32 len)
{
    if (len >= 33)
        return APCTL_ERR_SSID_TOO_LONG;
    if (g_apctl.flags & 0x200)
        return APCTL_ERR_BUSY;
    g_apctl.flags |= 0x200;
    if (ssid != NULL) {
        sceKernelMemcpy(g_apctl.scan2_ssid, ssid, len);
        g_apctl.scan2_ssid_len = len;
    } else {
        sceKernelMemset(g_apctl.scan2_ssid, 0, 32);
        g_apctl.scan2_ssid_len = 0;
    }
    sceKernelSetEventFlag(g_apctl.evf, 512);
    return 0;
}

static int apctl_wait_events(ApctlCtx *ctx, u32 mask, u32 *out_bits, int wait)
{
    int ret = 0;
    do {
        if (wait <= 0) {
            int r = sceKernelPollEventFlag(ctx->evf, mask, 33, out_bits);
            if (r == (int)0x800201AF)
                *out_bits = 0;
            if (r == 0 && (*out_bits & 0x20)) {
                ret = apctl_drain_wlan_events(ctx, out_bits);
                if (ret < 0)
                    break;
            }
        } else {
            int r = sceKernelWaitEventFlag(ctx->evf, mask, 33, out_bits, NULL);
            if (r == 0 && (*out_bits & 0x20)) {
                ret = apctl_drain_wlan_events(ctx, out_bits);
                if (ret < 0)
                    break;
            }
        }
    } while (*out_bits == 0 && wait > 0);
    return ret;
}

static int apctl_drain_wlan_events(ApctlCtx *ctx, u32 *out_bits)
{
    u32 ev, arg;
    int fatal = 0;
    ctx->last_error = 0;

    for (;;) {
        int ret = sceNet_lib_D1BE2CE9(ctx->ifname, &ev, &arg);
        if (ret == (int)0x80410184)
            break;
        if (ret < 0) {
            if (fatal == 0)
                *out_bits &= ~0x20u;
            return ret;
        }
        if (ctx->last_error == APCTL_ERR_SUSPEND)
            continue;
        switch (ev) {
        case 4:
            *out_bits |= 8;
            ctx->last_error = APCTL_ERR_WLAN_SWITCH_OFF;
            fatal++;
            break;
        case 5:
            if (ctx->last_error != APCTL_ERR_WLAN_SWITCH_OFF) {
                *out_bits |= 8;
                ctx->last_error = APCTL_ERR_DISCONNECTED;
                fatal++;
            }
            break;
        case 6:
        case 14:
            if (ctx->last_error != APCTL_ERR_WLAN_SWITCH_OFF &&
                (ev == 14 || ctx->last_error != APCTL_ERR_DISCONNECTED)) {
                *out_bits |= 8;
                ctx->last_error = APCTL_ERR_DISASSOC;
                fatal++;
            }
            break;
        case 7:
            *out_bits |= 8;
            ctx->last_error = APCTL_ERR_SUSPEND;
            ctx->flags |= 2;
            fatal++;
            apctl_notify_lib(ctx, ctx->state, ctx->state, 7, 0);
            break;
        case 8:
            ctx->flags &= ~2u;
            *out_bits &= ~0x20u;
            break;
        case 10:
        case 11:
            if (g_wpa_started != 0) {
                int wret = (ev == 10) ? wpa_notify_mic_err_unicast() : wpa_notify_mic_err_multicast();
                if (wret < 0) {
                    *out_bits |= 8;
                    ctx->last_error = wret;
                    fatal++;
                    break;
                }
            }
            *out_bits &= ~0x20u;
            break;
        case 13:
            if (ctx->roam_enabled == 0) {
                *out_bits &= ~0x20u;
            } else if (ctx->last_error == 0) {
                *out_bits |= 8;
                ctx->last_error = APCTL_ERR_ROAM_CHECK;
                fatal++;
            }
            break;
        default:
            *out_bits &= ~0x20u;
            break;
        }
    }
    if (fatal == 0)
        *out_bits &= ~0x20u;
    return 0;
}

/* =========================================================================
 * Protocol client callbacks & orchestration
 * ========================================================================= */

static int apctl_clients_init(ApctlCtx *ctx)
{
    g_client_ctx = ctx;
    g_static_ip_active = 0;
    g_dhcp_active = 0;
    g_pppoe_active = 0;
    g_eap_active = 0;
    g_wpa_active = 0;
    g_wpa_started = 0;
    g_btdun_active = 0;
    return 0;
}

static int apctl_clients_term(void)
{
    return 0;
}

static void dhcp_cb(int ev, int err)
{
    if (ev == 0) {
        if (g_client_ctx->state == 3)
            sceKernelSetEventFlag(g_client_ctx->evf, 16);
    } else if (ev == 4) {
        g_client_ctx->last_error = err;
        sceKernelSetEventFlag(g_client_ctx->evf, 8);
    }
}

static void pppoe_cb(int ev, int err)
{
    if (ev == 1) {
        if (g_client_ctx->state == 3)
            sceKernelSetEventFlag(g_client_ctx->evf, 16);
    } else if (ev == 3) {
        g_client_ctx->last_error = err;
        sceKernelSetEventFlag(g_client_ctx->evf, 8);
    }
}

static void eap_cb(int ev, int err)
{
    if (ev == 0) {
        if (g_client_ctx->state == 5)
            sceKernelSetEventFlag(g_client_ctx->evf, 128);
    } else if (ev == 1) {
        g_client_ctx->last_error = err;
        sceKernelSetEventFlag(g_client_ctx->evf, 8);
    }
}

static void wpa_cb(void *msg_v, int err)
{
    u32 *msg = (u32 *)msg_v;
    u8 key[32];
    switch (msg[0]) {
    case 0:
        if (g_client_ctx->state == 6)
            sceKernelSetEventFlag(g_client_ctx->evf, 256);
        break;
    case 2:
        sceKernelMemcpy(key, msg + 1, 32);
        sceNet_lib_AB375964("wlan", key);
        break;
    case 3:
        sceKernelMemcpy(key, msg + 1, 32);
        sceNet_lib_8DA79EB0("wlan", key);
        break;
    case 4:
        g_client_ctx->last_error = err;
        sceKernelSetEventFlag(g_client_ctx->evf, 8);
        break;
    default:
        break;
    }
}

static void btdun_cb(int ev, int err)
{
    if (ev == 0) {
        sceKernelSetEventFlag(g_client_ctx->evf, 1024);
    } else if (ev == 1) {
        sceKernelSetEventFlag(g_client_ctx->evf, 16);
    } else if (ev == 3) {
        g_client_ctx->last_error = err;
        sceKernelSetEventFlag(g_client_ctx->evf, 8);
    }
}

static int apctl_wait_client_loop(ApctlCtx *ctx, u32 wait_mask, u32 done_bit)
{
    u32 ev = 0;
    for (;;) {
        for (;;) {
            ev = 0;
            apctl_wait_events(ctx, wait_mask, &ev, 1);
            if ((ev & 0x200) == 0)
                break;
            apctl_scan2_in_state(ctx);
            ev &= ~0x200u;
            if (ev != 0)
                break;
        }
        if (ev & 2)
            return APCTL_ERR_CANCELED;
        if ((ev & 8) == 0)
            return 0;
        if (ctx->last_error != APCTL_ERR_ROAM_CHECK)
            return ctx->last_error;
        int roam = apctl_check_roam(ctx);
        if (roam < 0)
            return roam;
        if (roam != 0) {
            if ((ctx->flags & 0x20) == 0) {
                ctx->flags |= 0x20;
                ctx->roam_start_sec = sceNet_lib_75D9985C();
            }
            return ctx->last_error;
        }
        if (ev & done_bit)
            return 0;
    }
}

static int apctl_setup_dhcp(ApctlCtx *ctx)
{
    u32 zero1 = 0, zero2 = 0;
    u8 cfg[300];
    int ret = sceNetInet_lib_AAF4895A(&zero1, &zero2);
    if (ret < 0)
        goto fail;

    sceKernelMemset(cfg, 0, sizeof(cfg));
    *(u32 *)cfg = (u32)ctx->cfg_dns_flag;
    sceNet_lib_80C9F02A((char *)(cfg + 4), ctx->cfg_dns1);
    sceNet_lib_80C9F02A((char *)(cfg + 20), ctx->cfg_dns2);
    if (ctx->flags & 0x100)
        *(u32 *)(cfg + 292) = 1;
    if (ctx->cfg_wifisvc_flag == 3)
        *(u32 *)(cfg + 296) = ctx->cfg_wifisvc_ip;

    ret = dhcp_start("wlan", dhcp_cb, 0x4000, ctx->prio, 0, cfg);
    if (ret < 0)
        goto fail;

    ret = apctl_wait_client_loop(ctx, 570, 16);
    if (ret == 0) {
        g_dhcp_active = 1;
        return 0;
    }
    dhcp_stop();
fail:
    zero1 = 0; zero2 = 0;
    sceNetInet_lib_AAF4895A(&zero1, &zero2);
    return ret;
}

static int apctl_setup_pppoe(ApctlCtx *ctx)
{
    u32 zero1 = 0, zero2 = 0;
    u8 cfg[292];
    int ret = sceNetInet_lib_AAF4895A(&zero1, &zero2);
    if (ret < 0)
        goto fail;

    sceKernelMemset(cfg, 0, sizeof(cfg));
    *(u32 *)cfg = (u32)ctx->cfg_dns_flag;
    sceNet_lib_80C9F02A((char *)(cfg + 4), ctx->cfg_dns1);
    sceNet_lib_80C9F02A((char *)(cfg + 20), ctx->cfg_dns2);
    sceNet_lib_80C9F02A((char *)(cfg + 36), ctx->cfg_pppoe_user);
    sceNet_lib_80C9F02A((char *)(cfg + 164), ctx->cfg_pppoe_pass);

    ret = pppoe_start("wlan", pppoe_cb, 0x4000, ctx->prio, 0, cfg);
    if (ret < 0)
        goto fail;

    ret = apctl_wait_client_loop(ctx, 570, 16);
    if (ret == 0) {
        g_pppoe_active = 1;
        return 0;
    }
    pppoe_stop_signal();
    pppoe_stop_wait();
fail:
    zero1 = 0; zero2 = 0;
    sceNetInet_lib_AAF4895A(&zero1, &zero2);
    return ret;
}

static int apctl_setup_8021x(ApctlCtx *ctx)
{
    u8 bssid[128];
    u8 cfg[268];
    int ret = apctl_get_info_internal(ctx, ctx->state, 1, bssid, 1);
    if (ret < 0)
        return ret;
    sceKernelMemset(cfg, 0, sizeof(cfg));
    *(u32 *)cfg = (u32)ctx->cfg_eap_type;
    sceKernelMemcpy(cfg + 4, bssid, 6);
    sceNet_lib_80C9F02A((char *)(cfg + 10), ctx->cfg_eap_user);
    sceNet_lib_80C9F02A((char *)(cfg + 138), ctx->cfg_eap_pass);

    ret = eap8021x_start("wlan", eap_cb, 0x4000, ctx->prio, 0, cfg);
    if (ret < 0)
        return ret;

    ret = apctl_wait_client_loop(ctx, 682, 128);
    if (ret == 0) {
        g_eap_active = 1;
        return 0;
    }
    eap8021x_stop();
    return ret;
}

static int apctl_setup_wpa(ApctlCtx *ctx)
{
    int ret = wpa_start("wlan", wpa_cb, 0x4000, ctx->prio, 0, ctx->bss_info);
    if (ret < 0)
        return ret;
    g_wpa_started = 1;
    ret = apctl_wait_client_loop(ctx, 810, 256);
    if (ret == 0) {
        g_wpa_active = 1;
        return 0;
    }
    wpa_stop();
    return ret;
}

static int apctl_setup_btdun_dial(ApctlCtx *ctx)
{
    u32 zero1 = 0, zero2 = 0;
    u8 cfg[400];
    int ret = sceNetInet_lib_AAF4895A(&zero1, &zero2);
    if (ret < 0)
        return ret;
    sceKernelMemset(cfg, 0, sizeof(cfg));
    *(u32 *)cfg = (u32)ctx->cfg_btdun_id;
    sceKernelMemcpy(cfg + 4, ctx->cfg_btdun_init, 65);
    if (ctx->flags & 0x800) {
        *(void (**)(void))(cfg + 396) = apctl_btdun_at_trampoline;
    } else {
        sceKernelMemcpy(cfg + 69, ctx->cfg_btdun_dial, 33);
        sceKernelMemcpy(cfg + 102, ctx->cfg_pppoe_user, 128);
        sceKernelMemcpy(cfg + 230, ctx->cfg_pppoe_pass, 128);
        *(u32 *)(cfg + 360) = (u32)ctx->cfg_dns_flag;
        sceKernelMemcpy(cfg + 364, ctx->cfg_dns1, 16);
        sceKernelMemcpy(cfg + 380, ctx->cfg_dns2, 16);
    }
    ret = btdun_start(btdun_cb, 0, 0x4000, ctx->prio, cfg);
    if (ret < 0)
        return ret;
    ret = apctl_wait_client_loop(ctx, 1546, 1024);
    if (ret == 0) {
        g_btdun_active = 1;
        return 0;
    }
    btdun_stop();
    return ret;
}

static int apctl_setup_btdun_ip(ApctlCtx *ctx)
{
    int ret = apctl_wait_client_loop(ctx, 570, 16);
    if (ret < 0)
        btdun_stop();
    return ret;
}

static int apctl_parse_static_ip(ApctlCtx *ctx, void *ip, void *mask, void *gw, u32 *dns1, void *dns2)
{
    u8 *base = (u8 *)ctx;
    if (sceNetInetInetPton(2, (const char *)(base + 4272), ip) <= 0 ||
        sceNetInetInetPton(2, (const char *)(base + 4288), mask) <= 0 ||
        sceNetInetInetPton(2, (const char *)(base + 4304), gw) <= 0)
        return APCTL_ERR_INVALID_PARAM;
    sceKernelMemset(dns1, 0, 4);
    sceKernelMemset(dns2, 0, 4);
    int ret = sceNetInetInetAton((const char *)(base + 4532), dns1);
    if (*dns1 != 0)
        return sceNetInetInetAton((const char *)(base + 4548), dns2);
    return ret;
}

static int apctl_setup_static_ip(ApctlCtx *ctx)
{
    u32 ip, mask, gw, dns1, dns2;
    u32 zero1 = 0, zero2 = 0;
    int ret = sceNetInet_lib_AAF4895A(&zero1, &zero2);
    if (ret < 0)
        goto fail;
    ret = apctl_parse_static_ip(ctx, &ip, &mask, &gw, &dns1, &dns2);
    if (ret < 0)
        goto fail;
    ret = sceNetInet_lib_4F68DB0E(ctx->ifname, &ip, NULL, &mask);
    if (ret >= 0) {
        sceNetInet_lib_7776A492(&gw);
        ret = sceNetInet_lib_AAF4895A(&dns1, &dns2);
        if (ret >= 0)
            sceNetInet_lib_A94A75E7(&ip);
    }
    if (ret < 0)
        goto fail;
    g_static_ip_active = 1;
    return 0;

fail:
    if (sceNetInet_lib_6A046357(ctx->ifname) >= 0) {
        g_static_ip_active = 0;
        zero1 = 0; zero2 = 0;
        sceNetInet_lib_AAF4895A(&zero1, &zero2);
    }
    return ret;
}

static int apctl_wpa_prepare(ApctlCtx *ctx)
{
    u8 cfg[112];
    sceKernelMemset(cfg, 0, sizeof(cfg));
    if ((u32)(ctx->cfg_security - 3) >= 2)
        return APCTL_ERR_WPA_CONFIG;
    *(u32 *)(cfg + 0) = 0;
    *(u32 *)(cfg + 4) = (ctx->cfg_security != 3) ? 1 : 0;
    *(u32 *)(cfg + 44) = (u32)ctx->cfg_wpa_key_type;
    sceKernelMemcpy(cfg + 48, ctx->cfg_wpa_key, 64);
    sceKernelMemcpy(cfg + 12, ctx->cfg_ssid, 32);
    cfg[8] = (u8)sceNet_lib_D8722983(ctx->cfg_ssid);
    int ret = wpa_set_config(cfg);
    return (ret < 0) ? ret : 0;
}

static void apctl_stop_btdun(void)
{
    btdun_stop();
    g_btdun_active = 0;
}

static void apctl_stop_auth_clients(void)
{
    if (g_eap_active > 0) {
        eap8021x_stop();
        g_eap_active = 0;
    }
    if (g_wpa_active > 0) {
        wpa_stop();
        g_wpa_started = 0;
        g_wpa_active = 0;
    }
    if (g_btdun_active > 0) {
        btdun_stop();
        g_btdun_active = 0;
    }
}

static void apctl_stop_all_clients(ApctlCtx *ctx)
{
    u32 z1 = 0, z2 = 0;
    if (g_static_ip_active > 0) {
        if (sceNetInet_lib_6A046357(ctx->ifname) >= 0) {
            g_static_ip_active = 0;
            sceNetInet_lib_AAF4895A(&z1, &z2);
        }
    }
    if (g_dhcp_active > 0) {
        dhcp_stop();
        g_dhcp_active = 0;
        z1 = 0; z2 = 0;
        sceNetInet_lib_AAF4895A(&z1, &z2);
    }
    if (g_pppoe_active > 0) {
        pppoe_stop_signal();
        pppoe_stop_wait();
        g_pppoe_active = 0;
        z1 = 0; z2 = 0;
        sceNetInet_lib_AAF4895A(&z1, &z2);
    }
    apctl_stop_auth_clients();
}
