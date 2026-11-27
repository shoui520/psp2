#include "pspnet_adhocctl_internal.h"

const u8 g_channels[4] = { 1, 6, 11, 0 };

const u32 g_gm_templates[4][20] = {
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0x00010000, 0
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0x00000100, 0x080f0004, 0x00010000, 0x000c0000
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0x00000040, 0x080f0010, 0x00010000, 0x000c0000
    },
    {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0x00000100, 0x080f0010, 0x00010000, 0x001c0000
    }
};


int g_initialized;                              /* 0x6730 */
char g_ssid_prefix[4];                          /* 0x6734 */
struct SceNetAdhocctlCtx g_ctx;                 /* 0x6738..0x6f0f */
struct SceNetAdhocctlGameModeCtx g_gm_ctx;      /* 0x6f10..0x6fcf */
struct SceNetAdhocctlHandlerEntry g_handlers[4];     /* 0x6fd0..0x700f */
struct SceNetAdhocctlHandlerEntry g_lib_handlers[4]; /* 0x7010..0x704f */
struct SceNetAdhocctlChannelLoad g_ch_load[3];  /* 0x7050..0x7073 */
int g_saved_power_save;                         /* 0x7074 */
u8 g_ptp_buf[0x400];                            /* 0x7078..0x7477 */
u8 g_gm_scan_buf[0xc00];                        /* 0x7478..0x8077 */
int g_mutex_created;                            /* 0x8078 */
u8 g_mutex_work[0x20];                          /* 0x807c..0x809b */


/* 0x0000 */
int sceNetAdhocctlInit(int stackSize, int priority, const void *adhoc_id)
{
    if (sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    if (g_initialized > 0)
        return 0x80410b07;
    if ((u32)stackSize < 0xc00) {
        g_initialized = 0;
        return 0x80410b13;
    }
    if ((u32)stackSize == 0 || priority < 0 || adhoc_id == NULL) {
        g_initialized = 0;
        return 0x80410b04;
    }

    const SceNetAdhocctlProductCode *prod = (const SceNetAdhocctlProductCode *)adhoc_id;
    u32 type = prod->type;
    if (type >= 3) {
        g_initialized = 0;
        return 0x80410b04;
    }

    const char *id_str = prod->product;
    int i;
    for (i = 0; i < 9; i++) {
        if (id_str[i] == 0) {
            g_initialized = 0;
            return 0x80410b04;
        }
    }

    if (type == 1) {
        KprintfForUser("***********************************************\n");
        KprintfForUser("******** WARNING from pspnet_adhocctl  ********\n");
        KprintfForUser("***********************************************\n");
        KprintfForUser("* SCE_NET_ADHOCCTL_ADHOCTYPE_RESERVED is specified.\n");
        KprintfForUser("* This adhoc_id is for debug only.\n");
        KprintfForUser("* Don't use this adhoc_id for release program.\n");
        KprintfForUser("***********************************************\n");
    }

    adhocctlLoadSsidPrefix(g_ssid_prefix);
    int res = _sce_pspnet_adhocctl_init(stackSize, priority, adhoc_id);
    if (res < 0) {
        g_initialized = 0;
        return res;
    }
    g_initialized = 1;
    return res;
}

/* 0x0174 */
int sceNetAdhocctlJoin(const void *scan_info)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (scan_info == NULL)
        return 0x80410b04;

    const SceNetAdhocctlScanInfo *info = (const SceNetAdhocctlScanInfo *)scan_info;
    const char *group_name = info->name;
    if (group_name != NULL) {
        int i = 0;
        while (i < 8 && group_name[i] != 0) {
            int f = sceNet_lib_750F705D(group_name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return adhocctlRequestJoin(scan_info);
}

/* 0x0238 */
int sceNetAdhocctlGetPeerList(int *buflen, void *buf)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if ((u32)buf & 3)
        return 0x80410b14;
    if (buflen == NULL)
        return 0x80410b04;

    if (sceKernelGetCompiledSdkVersion() > 0x03010110) {
        if (buf == NULL) {
            if (*buflen >= 0) {
                *buflen = 0;
                return sceNetAdhocAuth_lib_BD144DA6(buflen, buf);
            }
            return sceNetAdhocAuth_lib_BD144DA6(buflen, buf);
        }
    }
    if (*buflen < 0)
        return 0x80410b04;
    return sceNetAdhocAuth_lib_BD144DA6(buflen, buf);
}

/* 0x0304 */
int sceNetAdhocctlGetPeerInfo(const u8 *mac, int size, void *peer_info)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (mac == NULL || peer_info == NULL || size < 0)
        return 0x80410b04;
    return sceNetAdhocAuth_lib_CF4D9BED(mac, size, peer_info);
}

/* 0x03a8 */
int sceNetAdhocctlGetAddrByName(const char *name, int *buflen, void *buf)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if ((u32)buf & 3)
        return 0x80410b14;
    if (name == NULL || buflen == NULL)
        return 0x80410b04;

    if (sceKernelGetCompiledSdkVersion() > 0x03010110) {
        if (buf == NULL) {
            if (*buflen >= 0) {
                *buflen = 0;
                return sceNetAdhocAuth_lib_1F9A90B8(name, buflen, buf);
            }
            return sceNetAdhocAuth_lib_1F9A90B8(name, buflen, buf);
        }
    }
    if (*buflen < 0)
        return 0x80410b04;
    return sceNetAdhocAuth_lib_1F9A90B8(name, buflen, buf);
}

/* 0x0490 */
void adhocctlCopySsidPrefix(char *dst)
{
    sceKernelMemcpy(dst, g_ssid_prefix, 3);
}

/* 0x04b4 */
int sceNetAdhocctlTerm(void)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0;

    int res = _sce_pspnet_adhocctl_term();
    if (res >= 0)
        g_initialized = 0;
    return res;
}

/* 0x0518 */
int sceNetAdhocctlConnect(const char *name)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name != NULL) {
        int i = 0;
        while (i < 8 && name[i] != 0) {
            int f = sceNet_lib_750F705D(name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return adhocctlRequestConnect(name);
}

/* 0x05c0 */
int sceNetAdhocctlCreate(const char *name)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name != NULL) {
        int i = 0;
        while (i < 8 && name[i] != 0) {
            int f = sceNet_lib_750F705D(name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return adhocctlRequestCreate(name);
}

/* 0x0668 */
int adhocctlCreateGameModeInternal(const char *name, int game_type, int num_peers, const u8 *macs, u32 timeout, int flag, int min_peers)
{
    if (g_initialized == 0)
        return 0x80410b08;
    if (name == NULL || (u32)(game_type - 1) >= 3 || min_peers < 2 || num_peers < min_peers || num_peers < 2 || num_peers >= 17 || macs == NULL)
        return 0x80410b04;

    int i = 0;
    while (i < 8 && name[i] != 0) {
        int f = sceNet_lib_750F705D(name[i]);
        i++;
        if ((f & 7) == 0)
            return 0x80410b04;
    }
    if (i < 0)
        return i;

    const u8 *tmpl = (const u8 *)g_gm_templates[game_type];
    if (tmpl[0x44] < num_peers)
        return 0x80410b04;

    return _sce_pspnet_adhocctl_create_realtime_ibss(name, num_peers, macs, tmpl, timeout, flag, min_peers);
}

/* 0x07d0 */
int sceNetAdhocctlCreateEnterGameMode(const char *name, int game_type, int num_peers, const u8 *macs, u32 timeout, int flag)
{
    if (sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    return adhocctlCreateGameModeInternal(name, game_type, num_peers, macs, timeout, flag, num_peers);
}

/* 0x086c */
int sceNetAdhocctl_B0B80E80(const char *name, int game_type, int min_peers, int num_peers, const u8 *macs, u32 timeout, int flag)
{
    if (sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    return adhocctlCreateGameModeInternal(name, game_type, num_peers, macs, timeout, flag, min_peers);
}

/* 0x0914 */
int sceNetAdhocctlJoinEnterGameMode(const char *name, const u8 *gc_mac, u32 timeout, int flag)
{
    if (sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name == NULL || gc_mac == NULL)
        return 0x80410b04;

    int i = 0;
    while (i < 8 && name[i] != 0) {
        int f = sceNet_lib_750F705D(name[i]);
        i++;
        if ((f & 7) == 0)
            return 0x80410b04;
    }
    if (i < 0)
        return i;

    return _sce_pspnet_adhocctl_join_realtime_ibss(name, gc_mac, g_gm_templates[0], timeout, flag);
}

/* 0x0a08 */
int sceNetAdhocctlScan(void)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    return adhocctlRequestScan();
}

/* 0x0a5c */
int sceNetAdhocctlDisconnect(void)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    return adhocctlRequestDisconnect();
}

/* 0x0ab0 */
int sceNetAdhocctlExitGameMode(void)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    return adhocctlRequestExitGameMode();
}

/* 0x0b04 */
int sceNetAdhocctlAddHandler(void (*func)(int, int, void *), void *arg)
{
    int id;
    if (sceKernelCheckThreadStack() < 0xbe0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (func == NULL)
        return 0x80410b04;
    int res = _sce_pspnet_adhocctl_add_handler(func, arg, &id);
    return (res < 0) ? res : id;
}

/* 0x0b90 */
int sceNetAdhocctlDelHandler(int id)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    return _sce_pspnet_adhocctl_del_handler(id);
}

/* 0x0bf0 */
int sceNetAdhocctlGetState(int *state)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (state == NULL)
        return 0x80410b04;
    return _sce_pspnet_adhocctl_get_state(state);
}

/* 0x0c5c */
int sceNetAdhocctlGetAdhocId(void *adhoc_id)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (adhoc_id == NULL)
        return 0x80410b04;
    return adhocctlGetAdhocIdInternal(adhoc_id);
}

/* 0x0cc8 */
int sceNetAdhocctlGetNameByAddr(const u8 *mac, char *name)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (mac == NULL || name == NULL)
        return 0x80410b04;
    return sceNetAdhocAuth_lib_76F26AB0(mac, name);
}

/* 0x0d50 */
int sceNetAdhocctlGetParameter(void *param)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (param == NULL)
        return 0x80410b04;
    return _sce_pspnet_adhocctl_get_param(param);
}

/* 0x0dbc */
int sceNetAdhocctlGetScanInfo(int *buflen, void *buf)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if ((u32)buf & 3)
        return 0x80410b14;
    return adhocctlGetScanInfoInternal(buflen, buf);
}

/* 0x0e3c */
int sceNetAdhocctlGetGameModeInfo(void *info)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (info == NULL)
        return 0x80410b04;
    return adhocctlGetGameModeInfoInternal(info);
}

/* 0x0ea8 */
int sceNetAdhocctl_lib_EFA6AEAF(const void *param, int num_peers, const u8 *macs, u32 timeout, int flag)
{
    if (g_initialized == 0)
        return 0x80410b08;
    if (param == NULL)
        return 0x80410b04;
    _sce_pspnet_adhocctl_create_realtime_ibss(NULL, num_peers, macs, param, timeout, flag | 1, num_peers);
    return 0;
}

/* 0x0f18 */
int sceNetAdhocctl_lib_1593C05D(const void *param, const u8 *gc_mac, u32 timeout, int flag)
{
    if (g_initialized == 0)
        return 0x80410b08;
    if (param == NULL)
        return 0x80410b04;
    _sce_pspnet_adhocctl_join_realtime_ibss(NULL, gc_mac, param, timeout, flag | 1);
    return 0;
}

/* 0x0f84 */
int sceNetAdhocctl_lib_F8BABD85(void (*func)(int, int, void *), void *arg)
{
    int id;
    if (g_initialized == 0)
        return 0x80410b08;
    if (func == NULL)
        return 0x80410b04;
    int res = adhocctlAddGameModeHandlerInternal(func, arg, &id);
    return (res < 0) ? res : id;
}

/* 0x0fd8 */
int sceNetAdhocctl_lib_1C679240(int id)
{
    if (g_initialized == 0)
        return 0x80410b08;
    return adhocctlDelGameModeHandlerInternal(id);
}

/* 0x1008 */
int sceNetAdhocctl_lib_CDEA7ACB(void *param)
{
    if (g_initialized == 0)
        return 0x80410b08;
    if (param == NULL)
        return 0x80410b04;
    return adhocctlGetGameModeParamInternal(param);
}

/* 0x1044 */
int sceNetAdhocctl_lib_1A2B6D03(const char *name)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name != NULL) {
        int i = 0;
        while (i < 8 && name[i] != 0) {
            int f = sceNet_lib_750F705D(name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return adhocctlRequestConnectRaw(name);
}

/* 0x10e8 */
int sceNetAdhocctl_lib_1572422C(void)
{
    return adhocctlRequestSuspend();
}

/* 0x1104 */
int sceNetAdhocctl_lib_F6057CC4(const char *name)
{
    if (sceKernelCheckThreadStack() < 0xbf0)
        return 0x80410005;
    if (g_initialized == 0)
        return 0x80410b08;
    if (name != NULL) {
        int i = 0;
        while (i < 8 && name[i] != 0) {
            int f = sceNet_lib_750F705D(name[i]);
            i++;
            if ((f & 7) == 0)
                return 0x80410b04;
        }
        if (i < 0)
            return i;
    }
    return adhocctlRequestCreateRaw(name);
}

/* 0x11ac */
