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

typedef struct WlanScanReq {
    u32 scan_type;            /* +0 */
    u8 pad_ch[6];             /* +4 */
    u8 channels[14];          /* +10 */
    u8 ssid_len;              /* +24 */
    u8 pad_ssid[3];           /* +25 */
    char ssid[32];            /* +28 */
    u32 probe_count;          /* +60 */
    u32 min_ch_time;          /* +64 */
    u32 max_ch_time;          /* +68 */
    u32 scan_timeout;         /* +72 */
} WlanScanReq; /* 76 bytes */

typedef struct WlanScanEntry {
    struct WlanScanEntry *next; /* +0 */
    u8 bssid[6];                /* +4 */
    u8 channel;                 /* +10 */
    u8 ssid_len;                /* +11 */
    char ssid[32];              /* +12 */
    u32 bss_type;               /* +44 */
    u8 mid_ie[18];              /* +48 */
    u16 caps;                   /* +66 */
    u8 mid_tail[8];             /* +68 */
    u8 rssi;                    /* +76 */
    u8 sec_pad[7];              /* +77 */
    u8 wpa_present;             /* +84 */
    u8 wpa_unicast;             /* +85 */
    u8 wpa_multicast;           /* +86 */
    u8 wpa_akm;                 /* +87 */
    u8 rsn_pad[2];              /* +88 */
    u8 rsn_present;             /* +90 */
    u8 pad_end;                 /* +91 */
} WlanScanEntry;

typedef struct WlanDrvInfo {
    u8 bssid[6];              /* +0 */
    char ssid[33];            /* +6 */
    u8 ssid_len;              /* +39 */
    char name[64];            /* +40 */
    u8 pad_name[2];           /* +104 */
    u16 rssi_caps;            /* +106 */
    u8 channel;               /* +108 */
    u8 pad_ch[3];             /* +109 */
    u32 assoc_mode;           /* +112 */
    u32 security;             /* +116 */
    u32 pad_sec;              /* +120 */
    u32 ip_addr;              /* +124 */
    u32 netmask;              /* +128 */
    u32 gateway;              /* +132 */
    u32 dns_a;                /* +136 */
    u32 dns_b;                /* +140 */
} WlanDrvInfo; /* 144 bytes */

typedef struct ApctlBssInfo {
    u8 bssid[6];              /* +0 (4032) */
    u8 channel;               /* +6 (4038) */
    u8 ssid_len;              /* +7 (4039) */
    char ssid[32];            /* +8 (4040) */
    u32 assoc_mode;           /* +40 (4072) */
    u8 mid[28];               /* +44 (4076) */
    u16 rssi_caps;            /* +72 (4104) */
    u8 tail[18];              /* +74 (4106) */
} ApctlBssInfo; /* 92 bytes */

typedef struct ApctlDhcpCfg {
    u32 dns_flag;
    char dns_a[16];
    char dns_b[16];
    char hostname[256];
    u32 mcast_flag;
    u32 req_ip;
} ApctlDhcpCfg; /* 300 bytes */

typedef struct ApctlPppoeCfg {
    u32 dns_flag;
    char dns_a[16];
    char dns_b[16];
    char user[128];
    char pass[128];
} ApctlPppoeCfg; /* 292 bytes */

typedef struct Apctl8021xCfg {
    u32 eap_type;
    u8 bssid[6];
    char user[128];
    char pass[128];
    u8 pad[2];
} Apctl8021xCfg; /* 268 bytes */

typedef struct ApctlBtdunCfg {
    u32 device_id;
    char init_cmd[65];
    char dial_num[33];
    char user[128];
    char pass[128];
    u8 pad[2];
    u32 dns_flag;
    char dns_a[16];
    char dns_b[16];
    void (*log_cb)(void);
} ApctlBtdunCfg; /* 400 bytes */

typedef struct ApctlWpaCfg {
    u32 auth_mode;
    u32 cipher;
    u8 ssid_len;
    u8 pad_ssid[3];
    char ssid[32];
    u32 key_type;
    u8 key[64];
} ApctlWpaCfg; /* 112 bytes */

/* Main Apctl context (5408 bytes = 0x1520, matching original offsets) */
typedef struct ApctlCtx {
    char ifname[16];          /* 0x0000 (0) */
    int state;                /* 0x0010 (16) */
    int event;                /* 0x0014 (20) */
    SceUID thid;              /* 0x0018 (24) */
    SceUID evf;               /* 0x001C (28) */
    int mtx_inited;           /* 0x0020 (32) */
    u32 mtx[8];               /* 0x0024 (36) */
    int prio;                 /* 0x0044 (68) */
    u8 scan_a_ssid[32];       /* 0x0048 (72) */
    u32 scan_a_ssid_len;      /* 0x0068 (104) */
    u8 scan_a_buf[1920];      /* 0x006C (108) */
    u32 scan_a_len;           /* 0x07EC (2028) */
    u8 scan_b_ssid[32];       /* 0x07F0 (2032) */
    u32 scan_b_ssid_len;      /* 0x0810 (2064) */
    u8 scan_b_buf[1920];      /* 0x0814 (2068) */
    u32 scan_b_len;           /* 0x0F94 (3988) */
    int config_id;            /* 0x0F98 (3992) */
    int last_error;           /* 0x0F9C (3996) */
    int auth_mode;            /* 0x0FA0 (4000) */
    u32 flags;                /* 0x0FA4 (4004) */
    u32 extra_stack;          /* 0x0FA8 (4008) */
    u8 signal_strength;       /* 0x0FAC (4012) */
    u8 pad_sig[3];            /* 0x0FAD (4013) */
    int roam_enabled;         /* 0x0FB0 (4016) */
    u32 roam_start_sec;       /* 0x0FB4 (4020) */
    void (*btdun_at_cb)(void); /* 0x0FB8 (4024) */
    u32 btdun_at_gp;          /* 0x0FBC (4028) */
    ApctlBssInfo bss;         /* 0x0FC0 (4032, 92 bytes) */
    u32 ip_params[5];         /* 0x101C (4124: ip, mask, gw, dns_a, dns_b) */
    char cfg_name[64];        /* 0x1030 (4144) */
    char cfg_ssid[36];        /* 0x1070 (4208) */
    int cfg_security;         /* 0x1094 (4244) */
    u8 cfg_wep_key[16];       /* 0x1098 (4248) */
    int cfg_eap_mode;         /* 0x10A8 (4264) */
    int cfg_ip_mode;          /* 0x10AC (4268) */
    union {
        char pppoe_user[128];
        struct {
            char ip[16];
            char mask[16];
            char gw[16];
            char rest[80];
        } static_ip;
    } cfg_addr;               /* 0x10B0 (4272) */
    char cfg_pppoe_pass[128]; /* 0x1130 (4400) */
    int cfg_dns_flag;         /* 0x11B0 (4528) */
    char cfg_dns_a[16];       /* 0x11B4 (4532) */
    char cfg_dns_b[16];       /* 0x11C4 (4548) */
    int cfg_proxy_use;        /* 0x11D4 (4564) */
    char cfg_proxy_url[128];  /* 0x11D8 (4568) */
    u16 cfg_proxy_port;       /* 0x1258 (4696) */
    u16 cfg_proxy_pad;        /* 0x125A (4698) */
    int cfg_eap_type;         /* 0x125C (4700) */
    char cfg_eap_user[128];   /* 0x1260 (4704) */
    char cfg_eap_pass[128];   /* 0x12E0 (4832) */
    int cfg_wpa_key_type;     /* 0x1360 (4960) */
    u8 cfg_wpa_key[64];       /* 0x1364 (4964) */
    int cfg_browser_flag;     /* 0x13A4 (5028) */
    int cfg_wifisvc_flag;     /* 0x13A8 (5032) */
    u32 cfg_wifisvc_ip;       /* 0x13AC (5036) */
    int cfg_dev_type;         /* 0x13B0 (5040) */
    int cfg_btdun_id;         /* 0x13B4 (5044) */
    char cfg_btdun_init[65];  /* 0x13B8 (5048) */
    char cfg_btdun_dial[33];  /* 0x13F9 (5113) */
    u8 cfg_btdun_pad[2];      /* 0x141A (5146) */
    char cfg_extra_str[257];  /* 0x141C (5148) */
    u8 tail_pad[3];           /* 0x151D (5405) */
} ApctlCtx; /* 5408 bytes */

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
static int apctl_scan_a(ApctlCtx *ctx);
static int apctl_scan_b(ApctlCtx *ctx);
static u32 apctl_scan_b_in_state(ApctlCtx *ctx);
static void apctl_poll_scan_b_clear(ApctlCtx *ctx);
static int apctl_load_default_config(ApctlCtx *ctx);
static int apctl_wlan_clear(void);
static int apctl_save_ip_param(ApctlCtx *ctx, const void *ip);
static int apctl_core_init(int stack_size, int prio);
static int apctl_core_term(void);
static int apctl_add_handler_internal(void *fn, void *arg, int *id_out, int table);
static int apctl_del_handler_internal(int id, int table);
static int apctl_notify_all(ApctlCtx *ctx, int old_s, int new_s, int ev, int err);
static int apctl_notify_lib(ApctlCtx *ctx, int old_s, int new_s, int ev, int err);
static int apctl_notify_state_change(ApctlCtx *ctx, int prev_state);
static int apctl_connect_internal(int conn_idx, int mode, int extra_flag, int bt_id, const char *bt_init, void *bt_cb);
static int apctl_get_info_wrapper(int code, void *out, int allow_ext);
static int apctl_get_state_internal(int *state_out);
static int apctl_disconnect_internal(void);
static int apctl_req_scan_a(const char *ssid, u32 len);
static int apctl_req_scan_b(const char *ssid, u32 len);
static int apctl_get_bss_list_a(u32 *size_inout, void *buf);
static int apctl_get_bss_entry_a(int id, int code, void *out);
static int apctl_get_bss_list_b(u32 *size_inout, void *buf);
static int apctl_get_bss_entry_b(int id, int code, void *out);
static int apctl_set_roam_enabled(int enable);
static int apctl_get_extra_str(char **out_ptr);
static int apctl_set_extra_str(const char *str);
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
static int apctl_set_eap_flag_wrapper(ApctlCtx *ctx, int val);
static int apctl_dispatch_mic_err(ApctlCtx *ctx, int ev);
static int apctl_wpa_prepare(ApctlCtx *ctx);
static void apctl_stop_auth_clients(void);
static void apctl_stop_all_clients(ApctlCtx *ctx);
static void apctl_stop_btdun(void);

/* =========================================================================
 * Public API exports
 * ========================================================================= */

int sceNetApctlInit(u32 stackSize, int initPriority)
{
    int ret;
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited > 0)
        return APCTL_ERR_ALREADY_INIT;
    if (stackSize < 0x1400)
        return APCTL_ERR_STACK_TOO_SMALL;
    ret = apctl_core_init((int)stackSize, initPriority);
    if (ret < 0)
        return ret;
    g_apctl_inited = 1;
    return 0;
}

int sceNetApctlTerm(void)
{
    int ret;
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return 0;
    ret = apctl_core_term();
    if (ret >= 0)
        g_apctl_inited = 0;
    return ret;
}

int sceNetApctlAddHandler(void *handler, void *arg)
{
    int id = 0;
    int ret;
    if (_sceKernelCheckThreadStack() < 5088)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    ret = apctl_add_handler_internal(handler, arg, &id, 0);
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
    return apctl_req_scan_a(NULL, 0);
}

int sceNetApctl_lib_A9721D11(const char *ssid)
{
    u32 len;
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    len = (ssid != NULL) ? (u32)sceNet_lib_D8722983(ssid) : 0;
    return apctl_req_scan_a(ssid, len);
}

int sceNetApctlScanSSID2(const char *ssid)
{
    u32 len;
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    len = (ssid != NULL) ? (u32)sceNet_lib_D8722983(ssid) : 0;
    return apctl_req_scan_b(ssid, len);
}

int sceNetApctl_lib_0C7FFA5C(u32 *size_inout, void *buf)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (size_inout == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_list_a(size_inout, buf);
}

int sceNetApctlGetBSSDescIDList2(u32 *size_inout, void *buf)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (size_inout == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_list_b(size_inout, buf);
}

int sceNetApctl_lib_96BEB231(int id, int code, void *out)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (out == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_entry_a(id, code, out);
}

int sceNetApctlGetBSSDescEntry2(int id, int code, void *out)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (out == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_entry_b(id, code, out);
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
    int ret;
    if (_sceKernelCheckThreadStack() < 5088)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    ret = apctl_add_handler_internal(handler, arg, &id, 1);
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
    return apctl_req_scan_a(NULL, 0);
}

int sceNetApctl_6BDDCB8C(u32 *size_inout, void *buf)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (size_inout == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_list_a(size_inout, buf);
}

int sceNetApctl_04776994(int id, int code, void *out)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    if (out == NULL)
        return APCTL_ERR_INVALID_PARAM;
    return apctl_get_bss_entry_a(id, code, out);
}

int sceNetApctl_A7BB73DF(void *handler, void *arg)
{
    int id = 0;
    int ret;
    if (_sceKernelCheckThreadStack() < 5088)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    ret = apctl_add_handler_internal(handler, arg, &id, 2);
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
    return apctl_set_roam_enabled(enable);
}

int sceNetApctl_lib2_C20A144C(int connIndex, int extra_flag)
{
    if (_sceKernelCheckThreadStack() < 5088)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_connect_internal(connIndex, 2, extra_flag, 0, NULL, NULL);
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
    return apctl_get_extra_str(out_ptr);
}

int sceNetApctl_lib2_3895B348(const char *str)
{
    if (_sceKernelCheckThreadStack() < 5104)
        return APCTL_ERR_STACK_SMALL;
    if (g_apctl_inited == 0)
        return APCTL_ERR_NOT_INIT;
    return apctl_set_extra_str(str);
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
    WlanScanReq req;
    u32 status = 0;
    int i;

    sceKernelMemset(&req, 0, sizeof(req));
    if (ssid_len != 0) {
        sceKernelMemcpy(req.ssid, ssid, ssid_len);
        req.ssid_len = (u8)ssid_len;
    }
    req.scan_type = 1;
    for (i = 0; i < 14; i++)
        req.channels[i] = (u8)(i + 1);
    req.probe_count = 1;
    req.min_ch_time = 0;
    req.max_ch_time = 6;
    req.scan_timeout = 100;

    sceKernelMemset(buf, 0, *len_inout);
    return sceNet_lib_7BA3ED91(ifname, &req, len_inout, buf, &status);
}

static int __attribute__((noinline))
apctl_get_bss_list(ApctlCtx *ctx, u32 *size_inout, void *buf, u32 scan_len, void *scan_buf)
{
    u32 *prev = NULL;
    u32 count = 0;
    (void)ctx;

    if (scan_len != 0) {
        WlanScanEntry *cur = (WlanScanEntry *)scan_buf;
        int id = 1;
        while (cur != NULL) {
            if (cur->bss_type == 1) {
                if (buf != NULL) {
                    u32 *ent;
                    if (*size_inout < (count + 1) * 8)
                        break;
                    ent = (u32 *)buf + count * 2;
                    if (prev != NULL)
                        prev[0] = (u32)ent;
                    sceKernelMemset(ent, 0, 8);
                    ent[1] = (u32)id;
                    prev = ent;
                }
                count++;
            }
            cur = cur->next;
            id++;
        }
    }
    *size_inout = count * 8;
    return 0;
}

static int __attribute__((noinline))
apctl_get_bss_entry(ApctlCtx *ctx, int id, int code, void *out, u32 scan_len, void *scan_buf)
{
    WlanScanEntry *cur;
    int idx;
    (void)ctx;

    if ((u32)id >= 20)
        return APCTL_ERR_BAD_INDEX;
    if (scan_len == 0)
        return APCTL_ERR_NOT_FOUND;

    cur = (WlanScanEntry *)scan_buf;
    idx = 1;
    while (cur != NULL && idx < id) {
        cur = cur->next;
        idx++;
    }
    if (cur == NULL)
        return APCTL_ERR_NOT_FOUND;

    switch (code) {
    case 0:
        sceKernelMemcpy(out, cur->bssid, 6);
        return 0;
    case 1:
        sceKernelMemcpy(out, cur->ssid, 33);
        return 0;
    case 2:
        *(u32 *)out = cur->ssid_len;
        return 0;
    case 3:
        *(u8 *)out = cur->channel;
        return 0;
    case 4:
        *(u8 *)out = cur->rssi;
        return 0;
    case 5: {
        u32 sec;
        if ((cur->caps & 0x10) == 0) {
            sec = 0;
        } else if (cur->wpa_present == 1) {
            if ((cur->wpa_unicast & 4) && (cur->wpa_multicast & 4) && (cur->wpa_akm & 2))
                sec = 2;
            else if ((cur->wpa_unicast & 8) && (cur->wpa_multicast & 8) && (cur->wpa_akm & 2))
                sec = 4;
            else if (cur->wpa_unicast & 3)
                sec = 1;
            else
                sec = 3;
        } else {
            sec = (cur->rsn_present == 1) ? 3 : 1;
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
    switch (code) {
    case 0:
        sceNet_lib_80C9F02A((char *)out, ctx->cfg_name);
        return 0;
    case 1:
        sceKernelMemcpy(out, ctx->bss.bssid, 6);
        return 0;
    case 2:
        sceKernelMemcpy(out, ctx->bss.ssid, 32);
        return 0;
    case 3:
        *(u32 *)out = ctx->bss.ssid_len;
        return 0;
    case 4:
        switch (ctx->cfg_security) {
        case 0: *(u32 *)out = 0; break;
        case 1: case 2: *(u32 *)out = 1; break;
        case 3: *(u32 *)out = 2; break;
        case 4: *(u32 *)out = 3; break;
        default: break;
        }
        return 0;
    case 5:
        *(u8 *)out = ctx->signal_strength;
        return 0;
    case 6:
        *(u8 *)out = ctx->bss.channel;
        return 0;
    case 7:
        *(u8 *)out = (u8)ctx->cfg_eap_mode;
        return 0;
    case 8: case 9: case 10: case 11: case 12:
        if ((ctx->flags & 0x40) == 0)
            return APCTL_ERR_NO_IP;
        if (sceNetInetInetNtop(2, &ctx->ip_params[code - 8], (char *)out, 16) == NULL)
            return APCTL_ERR_INVALID_PARAM;
        return 0;
    case 13: *(u32 *)out = (u32)ctx->cfg_proxy_use; return 0;
    case 14: sceNet_lib_80C9F02A((char *)out, ctx->cfg_proxy_url); return 0;
    case 15: *(u16 *)out = ctx->cfg_proxy_port; return 0;
    case 16: *(u32 *)out = (u32)ctx->cfg_eap_type; return 0;
    case 17: *(u32 *)out = (u32)ctx->cfg_browser_flag; return 0;
    case 18: *(u32 *)out = (u32)ctx->cfg_wifisvc_flag; return 0;
    case 19:
        if (!allow_ext) return APCTL_ERR_INVALID_ID;
        *(u32 *)out = (u32)ctx->cfg_dev_type;
        return 0;
    case 20:
        if (!allow_ext) return APCTL_ERR_INVALID_ID;
        *(u32 *)out = (u32)ctx->cfg_btdun_id;
        return 0;
    case 21:
        if (!allow_ext) return APCTL_ERR_INVALID_ID;
        sceNet_lib_80C9F02A((char *)out, ctx->cfg_btdun_init);
        return 0;
    case 22:
        if (!allow_ext) return APCTL_ERR_INVALID_ID;
        sceNet_lib_80C9F02A((char *)out, ctx->cfg_btdun_dial);
        return 0;
    default:
        return APCTL_ERR_INVALID_ID;
    }
}

static int apctl_load_wlan_info(ApctlCtx *ctx)
{
    WlanDrvInfo info;
    int ret = sceWlanDrv_lib_CAC1FB0D(&info);
    if (ret < 0)
        return ret;

    sceKernelMemcpy(ctx->bss.bssid, info.bssid, 6);
    ctx->bss.assoc_mode = info.assoc_mode;
    ctx->bss.rssi_caps = info.rssi_caps;
    ctx->bss.channel = info.channel;
    ctx->bss.ssid_len = info.ssid_len;
    sceNet_lib_B5CE388A(ctx->bss.ssid, info.ssid, 32);
    sceNet_lib_B5CE388A(ctx->cfg_name, info.name, 64);
    ctx->ip_params[0] = info.ip_addr;
    ctx->ip_params[1] = info.netmask;
    ctx->ip_params[2] = info.gateway;
    ctx->ip_params[3] = info.dns_a;
    ctx->ip_params[4] = info.dns_b;
    ctx->signal_strength = (u8)info.rssi_caps;
    ctx->cfg_security = (int)info.security;
    sceNetInetInetNtop(2, &info.ip_addr, ctx->cfg_addr.static_ip.ip, 16);
    sceNetInetInetNtop(2, &info.netmask, ctx->cfg_addr.static_ip.mask, 16);
    sceNetInetInetNtop(2, &info.gateway, ctx->cfg_addr.static_ip.gw, 16);
    sceNetInetInetNtop(2, &info.dns_a, ctx->cfg_dns_a, 16);
    sceNetInetInetNtop(2, &info.dns_b, ctx->cfg_dns_b, 16);
    return 0;
}

static int apctl_check_roam(ApctlCtx *ctx)
{
    u8 sig;
    u8 scan_buf[192];
    u32 scan_len = sizeof(scan_buf);
    u32 ev = 0;
    WlanScanEntry *cur;
    int ret;

    ret = sceNet_lib_F355C73B(ctx->ifname, 5, &sig);
    if (ret < 0)
        return ret;
    ctx->signal_strength = sig;
    ctx->flags |= 0x80;
    ret = apctl_wlan_scan(ctx->ifname, ctx->bss.ssid, ctx->bss.ssid_len, scan_buf, &scan_len);
    ctx->flags &= ~0x80u;
    apctl_wait_events(ctx, 42, &ev, 0);
    if (ev & 2)
        return APCTL_ERR_CANCELED;
    if ((ev & 8) && ctx->last_error != APCTL_ERR_ROAM_CHECK)
        return ctx->last_error;
    if (ret < 0)
        return ret;

    cur = (WlanScanEntry *)scan_buf;
    while (cur != NULL) {
        if (cur->bss_type == 1 && cur->ssid_len == ctx->bss.ssid_len &&
            sceNet_lib_E0A81C7C(cur->ssid, ctx->bss.ssid, cur->ssid_len) == 0) {
            return (sceNet_lib_E0A81C7C(cur->bssid, ctx->bss.bssid, 6) != 0) ? 1 : 0;
        }
        cur = cur->next;
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

static int apctl_scan_a(ApctlCtx *ctx)
{
    int ret = sceNet_lib_5216CBF5(ctx->ifname);
    if (ret >= 0) {
        ctx->scan_a_len = 1920;
        ret = apctl_wlan_scan(ctx->ifname, ctx->scan_a_ssid, ctx->scan_a_ssid_len, ctx->scan_a_buf, &ctx->scan_a_len);
    }
    sceNet_lib_D2422E4D(ctx->ifname);
    return ret;
}

static int apctl_scan_b(ApctlCtx *ctx)
{
    int ret = sceNet_lib_5216CBF5(ctx->ifname);
    if (ret >= 0) {
        ctx->scan_b_len = 1920;
        ret = apctl_wlan_scan(ctx->ifname, ctx->scan_b_ssid, ctx->scan_b_ssid_len, ctx->scan_b_buf, &ctx->scan_b_len);
    }
    sceNet_lib_D2422E4D(ctx->ifname);
    return ret;
}

static u32 apctl_scan_b_in_state(ApctlCtx *ctx)
{
    u8 sig;
    int ret;
    if (sceNet_lib_F355C73B(ctx->ifname, 5, &sig) >= 0) {
        ctx->signal_strength = sig;
        ctx->flags |= 0x80;
    }
    ctx->scan_b_len = 1920;
    ret = apctl_wlan_scan(ctx->ifname, ctx->scan_b_ssid, ctx->scan_b_ssid_len, ctx->scan_b_buf, &ctx->scan_b_len);
    ctx->flags &= ~0x80u;
    apctl_notify_all(ctx, ctx->state, ctx->state, 11, ret);
    ctx->flags &= ~0x200u;
    return ctx->flags;
}

static void apctl_poll_scan_b_clear(ApctlCtx *ctx)
{
    u32 ev = 0;
    apctl_wait_events(ctx, 512, &ev, 0);
}

static int apctl_load_default_config(ApctlCtx *ctx)
{
    sceNet_lib_80C9F02A(ctx->cfg_name, "Net Config1");
    sceNet_lib_80C9F02A(ctx->cfg_ssid, "ABCDEFG");
    ctx->cfg_ip_mode = 1;
    ctx->cfg_security = 0;
    ctx->cfg_eap_mode = 0;
    ctx->cfg_dns_flag = 0;
    sceNet_lib_80C9F02A(ctx->cfg_dns_a, "192.168.11.1");
    sceNet_lib_80C9F02A(ctx->cfg_dns_b, "192.168.11.1");
    ctx->cfg_browser_flag = 1;
    ctx->cfg_wifisvc_flag = 0;
    ctx->cfg_proxy_use = 0;
    ctx->cfg_eap_type = 0;
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
    int ret;
    sceUtility_netparam_internal_61D0686E(0);
    ret = sceUtility_netparam_internal_6B1BC62C(0);
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

static int apctl_state_scan_b_idle(ApctlCtx *ctx, int *next_state, int *event)
{
    u32 ev = 0;
    int ret;
    sceNet_lib_B5CE388A(ctx->ifname, "wlan", 16);
    ret = apctl_wlan_attach(ctx, &ev);
    if (ret < 0) {
        *event = 6;
        ctx->last_error = ret;
    } else if (ev & 2) {
        *event = 5;
    } else {
        sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
        ret = sceNet_lib_AB7DD9A5(ctx->ifname, ctx->evf, 32);
        if (ret >= 0 && sceWlanGetSwitchState() != 0 && (ret = apctl_scan_b(ctx)) >= 0) {
            *event = 11;
        } else {
            if (ret >= 0)
                ret = APCTL_ERR_WLAN_SWITCH_OFF;
            *event = 6;
        }
        ctx->last_error = ret;
    }
    apctl_wlan_detach(ctx, *event);
    {
        int prev_ev = *event;
        if (*event != 5)
            *event = 11;
        *next_state = 0;
        ctx->flags &= ~0x200u;
        return prev_ev;
    }
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
        sceKernelMemset(ctx->cfg_name, 0, 1004);
        sceKernelMemset(ctx->cfg_extra_str, 0, 257);
        sceKernelWaitEventFlag(ctx->evf, 0xFFFFFFFFu, 33, &bits, NULL);
        if ((bits & 0x200) == 0)
            break;
        apctl_state_scan_b_idle(ctx, next_state, event);
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
    int ret;
    int set_err = 1;
    (void)prev;

    sceNet_lib_B5CE388A(ctx->ifname, "wlan", 16);
    do {
        ret = apctl_wlan_attach(ctx, &ev);
        if (ret < 0)
            break;
        if (ev & 2) {
            *event = 5;
            set_err = 0;
            break;
        }
        sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
        ret = sceNet_lib_AB7DD9A5(ctx->ifname, ctx->evf, 32);
        if (ret < 0)
            break;
        if (sceWlanGetSwitchState() == 0) {
            ret = APCTL_ERR_WLAN_SWITCH_OFF;
            break;
        }
        ret = apctl_scan_a(ctx);
        ev = 0;
        apctl_wait_events(ctx, 34, &ev, 0);
        if (ev != 0) {
            *event = (ev & 2) ? 5 : 6;
            set_err = 0;
            break;
        }
        if (ret >= 0) {
            *event = 2;
            set_err = 0;
        }
    } while (0);

    if (set_err) {
        *event = 6;
        ctx->last_error = ret;
    }
    *next_state = 0;
    ctx->flags |= 1;
    apctl_wlan_detach(ctx, *event);
    return 0;
}

static int apctl_state_btdun_joining(ApctlCtx *ctx, int prev, int *next_state, int *event)
{
    int ret;
    int ok = 0;
    (void)prev;

    sceNet_lib_B5CE388A(ctx->ifname, "btdun", 16);
    ret = sceBtdun_lib_412DA4A1();
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
                ok = 1;
            }
        }
    }
    if (!ok) {
        *next_state = 0;
        if (ret == APCTL_ERR_CANCELED) {
            *event = 5;
        } else {
            *event = 6;
            ctx->last_error = ret;
        }
    }
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
    int ret;
    int failed = 0;
    int ev_aborted = 0;
    int done = 0;

    ret = apctl_load_default_config(ctx);
    if (ret >= 0 && sceWlanGetSwitchState() != 0 && ctx->cfg_dev_type == 2)
        return apctl_state_btdun_joining(ctx, prev, next_state, event);

    do {
        if (ret < 0) {
            failed = 1;
            break;
        }
        if (sceWlanGetSwitchState() == 0) {
            ret = APCTL_ERR_WLAN_SWITCH_OFF;
            failed = 1;
            break;
        }

        sceNet_lib_B5CE388A(ctx->ifname, "wlan", 16);
        if ((ctx->flags & 0x10) == 0) {
            ret = apctl_wlan_attach(ctx, &ev);
            if (ret < 0) {
                failed = 1;
                break;
            }
            if (ev != 0) {
                ev_aborted = 1;
                break;
            }
        }

        sceNet_lib_AB7DD9A5(ctx->ifname, 0, 0);
        ret = sceNet_lib_AB7DD9A5(ctx->ifname, ctx->evf, 32);
        if (ret < 0) {
            failed = 1;
            break;
        }

        if (ctx->auth_mode == 2 || ctx->auth_mode == 4) {
            ret = apctl_wpa_prepare(ctx);
            if (ret < 0) {
                failed = 1;
                break;
            }
        }

        ev = 0;
        ret = apctl_wait_events(ctx, 42, &ev, 0);
        if (ev != 0) {
            ev_aborted = 1;
            break;
        }

        for (;;) {
            ret = apctl_load_wlan_info(ctx);
            apctl_poll_scan_b_clear(ctx);
            ev = 0;
            apctl_wait_events(ctx, 42, &ev, 0);
            if (ev != 0) {
                if (ev & 2) {
                    *next_state = 0;
                    *event = 5;
                    done = 1;
                    break;
                }
                if (ctx->last_error != APCTL_ERR_ROAM_CHECK) {
                    *next_state = 0;
                    *event = 6;
                    done = 1;
                    break;
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
                    failed = 1;
                    break;
                }
            } else {
                if ((u32)(ctx->cfg_security - 1) >= 2 || ret != (int)0x80410D09) {
                    failed = 1;
                    break;
                }
            }
        }
        if (done || failed)
            break;

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
    } while (0);

    if (ev_aborted) {
        *next_state = 0;
        *event = (ev & 2) ? 5 : 6;
    } else if (failed) {
        *next_state = 0;
        *event = 6;
        ctx->last_error = ret;
    }

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
    int ret;
    (void)prev;
    ret = apctl_setup_8021x(ctx);
    if (ret >= 0) {
        if (ctx->auth_mode == 4) {
            *next_state = 6;
        } else {
            apctl_set_eap_flag_wrapper(ctx, 1);
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
    int ret;
    (void)prev;
    ret = apctl_setup_wpa(ctx);
    if (ret >= 0) {
        if (ctx->auth_mode == 4)
            apctl_set_eap_flag_wrapper(ctx, 1);
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
            apctl_save_ip_param(ctx, &ctx->ip_params[0]);
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
        int roam;
        for (;;) {
            ev = 0;
            apctl_wait_events(ctx, 554, &ev, 1);
            if ((ev & 0x200) == 0)
                break;
            apctl_scan_b_in_state(ctx);
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
        roam = apctl_check_roam(ctx);
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
        apctl_notify_state_change(ctx, prev);
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

    do {
        ret = apctl_clients_init(ctx);
        if (ret < 0)
            break;

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
            break;
        ctx->mtx_inited = 1;

        ret = sceKernelCreateEventFlag("SceNetApctl", 0, 0, NULL);
        if (ret < 0)
            break;
        ctx->evf = ret;

        opt.size = 8;
        ret = sceKernelQueryMemoryInfo(ctx, &opt.mpid, NULL);
        if (ret < 0)
            break;

        ret = sceKernelCreateThread("SceNetApctl", apctl_thread, ctx->prio, stack_size, 0, &opt);
        if (ret < 0)
            break;
        ctx->thid = ret;

        ret = sceKernelStartThread(ctx->thid, 4, &ctx);
        if (ret < 0)
            break;

        ctx->extra_stack = (u32)(stack_size - 5120);
        if (opt.mpid == 5)
            ctx->flags |= 8;
        return 0;
    } while (0);

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

static inline __attribute__((always_inline))
ApctlHandler *get_handler_table(int table)
{
    if (table == 1) return g_handlers_lib;
    if (table == 2) return g_handlers_ext;
    return g_handlers_pub;
}

static int apctl_add_handler_internal(void *fn, void *arg, int *id_out, int table)
{
    ApctlHandler *tbl;
    int i;
    int ret = sceKernelLockLwMutex(g_apctl.mtx, 1, NULL);
    if (ret < 0)
        return ret;
    tbl = get_handler_table(table);
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

static int __attribute__((noinline))
apctl_notify_state_change(ApctlCtx *ctx, int prev_state)
{
    int err = 0;
    if (ctx->event == 6 || ctx->event == 11)
        err = ctx->last_error;
    return apctl_notify_all(ctx, prev_state, ctx->state, ctx->event, err);
}

static int apctl_connect_internal(int conn_idx, int mode, int extra_flag, int bt_id, const char *bt_init, void *bt_cb)
{
    (void)extra_flag;
    if ((g_apctl.flags & 1) == 0)
        return APCTL_ERR_BUSY;
    g_apctl.flags &= ~0x501u;
    if (mode == 2) {
        g_apctl.flags |= 0x100;
    } else if (mode == 3) {
        g_apctl.flags |= 0x400;
    } else if (mode == 4) {
        char buf[68];
        u32 val;
        int ret;
        sceUtility_netparam_internal_61D0686E(0);
        ret = sceUtility_netparam_internal_6B1BC62C(0);
        if (ret < 0)
            return ret;
        val = 2;
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

static int apctl_req_scan_a(const char *ssid, u32 len)
{
    if (len >= 33)
        return APCTL_ERR_SSID_TOO_LONG;
    if ((g_apctl.flags & 1) == 0)
        return APCTL_ERR_BUSY;
    g_apctl.flags &= ~1u;
    if (ssid != NULL) {
        sceKernelMemcpy(g_apctl.scan_a_ssid, ssid, len);
        g_apctl.scan_a_ssid_len = len;
    } else {
        sceKernelMemset(g_apctl.scan_a_ssid, 0, 32);
        g_apctl.scan_a_ssid_len = 0;
    }
    sceKernelSetEventFlag(g_apctl.evf, 4);
    return 0;
}

static int apctl_req_scan_b(const char *ssid, u32 len)
{
    if (len >= 33)
        return APCTL_ERR_SSID_TOO_LONG;
    if (g_apctl.flags & 0x200)
        return APCTL_ERR_BUSY;
    g_apctl.flags |= 0x200;
    if (ssid != NULL) {
        sceKernelMemcpy(g_apctl.scan_b_ssid, ssid, len);
        g_apctl.scan_b_ssid_len = len;
    } else {
        sceKernelMemset(g_apctl.scan_b_ssid, 0, 32);
        g_apctl.scan_b_ssid_len = 0;
    }
    sceKernelSetEventFlag(g_apctl.evf, 512);
    return 0;
}

static int __attribute__((noinline))
apctl_get_bss_list_a(u32 *size_inout, void *buf)
{
    return apctl_get_bss_list(&g_apctl, size_inout, buf, g_apctl.scan_a_len, g_apctl.scan_a_buf);
}

static int __attribute__((noinline))
apctl_get_bss_entry_a(int id, int code, void *out)
{
    return apctl_get_bss_entry(&g_apctl, id, code, out, g_apctl.scan_a_len, g_apctl.scan_a_buf);
}

static int __attribute__((noinline))
apctl_get_bss_list_b(u32 *size_inout, void *buf)
{
    return apctl_get_bss_list(&g_apctl, size_inout, buf, g_apctl.scan_b_len, g_apctl.scan_b_buf);
}

static int __attribute__((noinline))
apctl_get_bss_entry_b(int id, int code, void *out)
{
    return apctl_get_bss_entry(&g_apctl, id, code, out, g_apctl.scan_b_len, g_apctl.scan_b_buf);
}

static int __attribute__((noinline))
apctl_set_roam_enabled(int enable)
{
    g_apctl.roam_enabled = enable;
    return 0;
}

static int __attribute__((noinline))
apctl_get_extra_str(char **out_ptr)
{
    *out_ptr = g_apctl.cfg_extra_str;
    return 0;
}

static int __attribute__((noinline))
apctl_set_extra_str(const char *str)
{
    sceNet_lib_B5CE388A(g_apctl.cfg_extra_str, str, 257);
    g_apctl.cfg_extra_str[256] = '\0';
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
        case 11: {
            int wret = apctl_dispatch_mic_err(ctx, (int)ev);
            if (wret < 0) {
                *out_bits |= 8;
                ctx->last_error = wret;
                fatal++;
                break;
            }
            *out_bits &= ~0x20u;
            break;
        }
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
    if (ev == 1) {
        ApctlCtx *ctx = g_client_ctx;
        ctx->last_error = err;
        sceKernelSetEventFlag(ctx->evf, 8);
    } else if (ev == 0) {
        ApctlCtx *ctx = g_client_ctx;
        if (ctx->state == 5)
            sceKernelSetEventFlag(ctx->evf, 128);
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

static inline __attribute__((always_inline))
int apctl_wait_client_loop(ApctlCtx *ctx, u32 wait_mask, u32 done_bit)
{
    u32 ev = 0;
    for (;;) {
        int roam;
        for (;;) {
            ev = 0;
            apctl_wait_events(ctx, wait_mask, &ev, 1);
            if ((ev & 0x200) == 0)
                break;
            apctl_scan_b_in_state(ctx);
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
        roam = apctl_check_roam(ctx);
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
    u32 zero_a = 0, zero_b = 0;
    u32 clr_a, clr_b;
    ApctlDhcpCfg cfg;
    int ret;

    do {
        ret = sceNetInet_lib_AAF4895A(&zero_a, &zero_b);
        if (ret < 0)
            break;

        sceKernelMemset(&cfg, 0, sizeof(cfg));
        cfg.dns_flag = (u32)ctx->cfg_dns_flag;
        sceNet_lib_80C9F02A(cfg.dns_a, ctx->cfg_dns_a);
        sceNet_lib_80C9F02A(cfg.dns_b, ctx->cfg_dns_b);
        if (ctx->flags & 0x100)
            cfg.mcast_flag = 1;
        if (ctx->cfg_wifisvc_flag == 3)
            cfg.req_ip = ctx->cfg_wifisvc_ip;

        ret = dhcp_start("wlan", dhcp_cb, 0x4000, ctx->prio, 0, &cfg);
        if (ret < 0)
            break;

        ret = apctl_wait_client_loop(ctx, 570, 16);
        if (ret == 0) {
            g_dhcp_active = 1;
            return 0;
        }
        dhcp_stop();
    } while (0);

    clr_a = 0; clr_b = 0;
    sceNetInet_lib_AAF4895A(&clr_a, &clr_b);
    return ret;
}

static int apctl_setup_pppoe(ApctlCtx *ctx)
{
    u32 zero_a = 0, zero_b = 0;
    u32 clr_a, clr_b;
    ApctlPppoeCfg cfg;
    int ret;

    do {
        ret = sceNetInet_lib_AAF4895A(&zero_a, &zero_b);
        if (ret < 0)
            break;

        sceKernelMemset(&cfg, 0, sizeof(cfg));
        cfg.dns_flag = (u32)ctx->cfg_dns_flag;
        sceNet_lib_80C9F02A(cfg.dns_a, ctx->cfg_dns_a);
        sceNet_lib_80C9F02A(cfg.dns_b, ctx->cfg_dns_b);
        sceNet_lib_80C9F02A(cfg.user, ctx->cfg_addr.pppoe_user);
        sceNet_lib_80C9F02A(cfg.pass, ctx->cfg_pppoe_pass);

        ret = pppoe_start("wlan", pppoe_cb, 0x4000, ctx->prio, 0, &cfg);
        if (ret < 0)
            break;

        ret = apctl_wait_client_loop(ctx, 570, 16);
        if (ret == 0) {
            g_pppoe_active = 1;
            return 0;
        }
        pppoe_stop_signal();
        pppoe_stop_wait();
    } while (0);

    clr_a = 0; clr_b = 0;
    sceNetInet_lib_AAF4895A(&clr_a, &clr_b);
    return ret;
}

static int apctl_setup_8021x(ApctlCtx *ctx)
{
    u8 bssid[128];
    Apctl8021xCfg cfg;
    int ret = apctl_get_info_internal(ctx, ctx->state, 1, bssid, 1);
    if (ret < 0)
        return ret;
    sceKernelMemset(&cfg, 0, sizeof(cfg));
    cfg.eap_type = (u32)ctx->cfg_eap_type;
    sceKernelMemcpy(cfg.bssid, bssid, 6);
    sceNet_lib_80C9F02A(cfg.user, ctx->cfg_eap_user);
    sceNet_lib_80C9F02A(cfg.pass, ctx->cfg_eap_pass);

    ret = eap8021x_start("wlan", eap_cb, 0x4000, ctx->prio, 0, &cfg);
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
    int ret = wpa_start("wlan", wpa_cb, 0x4000, ctx->prio, 0, &ctx->bss);
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
    u32 zero_a = 0, zero_b = 0;
    ApctlBtdunCfg cfg;
    int ret = sceNetInet_lib_AAF4895A(&zero_a, &zero_b);
    if (ret < 0)
        return ret;
    sceKernelMemset(&cfg, 0, sizeof(cfg));
    cfg.device_id = (u32)ctx->cfg_btdun_id;
    sceKernelMemcpy(cfg.init_cmd, ctx->cfg_btdun_init, 65);
    if (ctx->flags & 0x800) {
        cfg.log_cb = apctl_btdun_at_trampoline;
    } else {
        sceKernelMemcpy(cfg.dial_num, ctx->cfg_btdun_dial, 33);
        sceKernelMemcpy(cfg.user, ctx->cfg_addr.pppoe_user, 128);
        sceKernelMemcpy(cfg.pass, ctx->cfg_pppoe_pass, 128);
        cfg.dns_flag = (u32)ctx->cfg_dns_flag;
        sceKernelMemcpy(cfg.dns_a, ctx->cfg_dns_a, 16);
        sceKernelMemcpy(cfg.dns_b, ctx->cfg_dns_b, 16);
    }
    ret = btdun_start(btdun_cb, 0, 0x4000, ctx->prio, &cfg);
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

static int __attribute__((noinline))
apctl_set_eap_flag_wrapper(ApctlCtx *ctx, int val)
{
    (void)ctx;
    eap8021x_set_eap_flag(val);
    return 0;
}

static int __attribute__((noinline))
apctl_dispatch_mic_err(ApctlCtx *ctx, int ev)
{
    (void)ctx;
    if (g_wpa_started != 0) {
        if (ev == 10)
            return wpa_notify_mic_err_unicast();
        if (ev == 11)
            return wpa_notify_mic_err_multicast();
    }
    return 0;
}

static int apctl_parse_static_ip(ApctlCtx *ctx, void *ip, void *mask, void *gw, u32 *dns_a, void *dns_b)
{
    int ret;
    if (sceNetInetInetPton(2, ctx->cfg_addr.static_ip.ip, ip) <= 0 ||
        sceNetInetInetPton(2, ctx->cfg_addr.static_ip.mask, mask) <= 0 ||
        sceNetInetInetPton(2, ctx->cfg_addr.static_ip.gw, gw) <= 0)
        return APCTL_ERR_INVALID_PARAM;
    sceKernelMemset(dns_a, 0, 4);
    sceKernelMemset(dns_b, 0, 4);
    ret = sceNetInetInetAton(ctx->cfg_dns_a, dns_a);
    if (*dns_a != 0)
        return sceNetInetInetAton(ctx->cfg_dns_b, dns_b);
    return ret;
}

static int apctl_setup_static_ip(ApctlCtx *ctx)
{
    u32 ip, mask, gw, dns_a, dns_b;
    u32 zero_a = 0, zero_b = 0;
    u32 clr_a, clr_b;
    int ret;

    do {
        ret = sceNetInet_lib_AAF4895A(&zero_a, &zero_b);
        if (ret < 0)
            break;
        ret = apctl_parse_static_ip(ctx, &ip, &mask, &gw, &dns_a, &dns_b);
        if (ret < 0)
            break;
        ret = sceNetInet_lib_4F68DB0E(ctx->ifname, &ip, NULL, &mask);
        if (ret >= 0) {
            sceNetInet_lib_7776A492(&gw);
            ret = sceNetInet_lib_AAF4895A(&dns_a, &dns_b);
            if (ret >= 0)
                sceNetInet_lib_A94A75E7(&ip);
        }
        if (ret < 0)
            break;
        g_static_ip_active = 1;
        return ret;
    } while (0);

    if (sceNetInet_lib_6A046357(ctx->ifname) >= 0) {
        g_static_ip_active = 0;
        clr_a = 0; clr_b = 0;
        sceNetInet_lib_AAF4895A(&clr_a, &clr_b);
    }
    return ret;
}

static int apctl_wpa_prepare(ApctlCtx *ctx)
{
    ApctlWpaCfg cfg;
    int ret;
    sceKernelMemset(&cfg, 0, sizeof(cfg));
    if ((u32)(ctx->cfg_security - 3) >= 2)
        return APCTL_ERR_WPA_CONFIG;
    cfg.auth_mode = 0;
    cfg.cipher = (ctx->cfg_security != 3) ? 1 : 0;
    cfg.key_type = (u32)ctx->cfg_wpa_key_type;
    sceKernelMemcpy(cfg.key, ctx->cfg_wpa_key, 64);
    sceKernelMemcpy(cfg.ssid, ctx->cfg_ssid, 32);
    cfg.ssid_len = (u8)sceNet_lib_D8722983(ctx->cfg_ssid);
    ret = wpa_set_config(&cfg);
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
    u32 zero_a = 0, zero_b = 0;
    if (g_static_ip_active > 0) {
        if (sceNetInet_lib_6A046357(ctx->ifname) >= 0) {
            g_static_ip_active = 0;
            sceNetInet_lib_AAF4895A(&zero_a, &zero_b);
        }
    }
    if (g_dhcp_active > 0) {
        dhcp_stop();
        g_dhcp_active = 0;
        zero_a = 0; zero_b = 0;
        sceNetInet_lib_AAF4895A(&zero_a, &zero_b);
    }
    if (g_pppoe_active > 0) {
        pppoe_stop_signal();
        pppoe_stop_wait();
        g_pppoe_active = 0;
        zero_a = 0; zero_b = 0;
        sceNetInet_lib_AAF4895A(&zero_a, &zero_b);
    }
    apctl_stop_auth_clients();
}
