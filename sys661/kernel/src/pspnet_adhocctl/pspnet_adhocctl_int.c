#include "pspnet_adhocctl_internal.h"

int adhocctl_call_handler(int event, int error)
{
    int lock_res = adhocctlLock();
    int i;
    for (i = 0; i < 4; i++) {
        if (g_handlers[i].func != NULL) {
            if (sceWlanDrv_lib_B7865DB5() == 0) {
                u32 saved_gp;
                __asm__ volatile ("move %0, $gp" : "=r"(saved_gp));
                __asm__ volatile ("move $gp, %0" : : "r"(g_handlers[i].gp));
                int rem = sceKernelCheckThreadStack() - g_ctx.min_stack;
                if (rem > 0) {
                    u32 alloc = (rem + 15) & ~15;
                    void * volatile stack_pad = __builtin_alloca(alloc);
                    (void)stack_pad;
                    g_handlers[i].func(event, error, g_handlers[i].arg);
                } else {
                    g_handlers[i].func(event, error, g_handlers[i].arg);
                }
                __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
            }
        }
    }
    for (i = 0; i < 4; i++) {
        if (g_lib_handlers[i].func != NULL) {
            u32 saved_gp;
            __asm__ volatile ("move %0, $gp" : "=r"(saved_gp));
            __asm__ volatile ("move $gp, %0" : : "r"(g_lib_handlers[i].gp));
            g_lib_handlers[i].func(event, error, g_lib_handlers[i].arg);
            __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
        }
    }
    adhocctlUnlock(lock_res);
    return 0;
}

/* 0x277c */
int _sce_pspnet_adhocctl_init(int stackSize, int priority, const void *adhoc_id)
{
    struct SceNetAdhocctlCtx *ctx = &g_ctx;
    sceWlanDrv_lib_768E275B();
    sceKernelMemset(ctx, 0, sizeof(*ctx));
    sceKernelMemset(g_handlers, 0, sizeof(g_handlers));
    sceKernelMemset(g_lib_handlers, 0, sizeof(g_lib_handlers));
    ctx->state = 0;

    int res = sceNetAdhocAuth_lib_86004235();
    if (res < 0)
        return res;

    sceKernelMemcpy(&ctx->adhoc_type, adhoc_id, 0x10);
    res = adhocctlCreateLock();
    if (res < 0) {
        sceNetAdhocAuth_lib_6074D8F1();
        return res;
    }

    ctx->evfid = sceKernelCreateEventFlag("SceNetAdhocctl", 0, 0, NULL);
    if (ctx->evfid < 0) {
        res = ctx->evfid;
        adhocctlDeleteLock();
        sceNetAdhocAuth_lib_6074D8F1();
        return res;
    }

    ctx->thid = sceKernelCreateThread("SceNetAdhocctl", adhocctl_thread, priority, stackSize, 0, NULL);
    if (ctx->thid < 0) {
        res = ctx->thid;
        sceKernelDeleteEventFlag(ctx->evfid);
        adhocctlDeleteLock();
        sceNetAdhocAuth_lib_6074D8F1();
        return res;
    }

    res = sceKernelStartThread(ctx->thid, 4, &ctx);
    if (res < 0) {
        sceKernelTerminateThread(ctx->thid);
        sceKernelDeleteThread(ctx->thid);
        sceKernelDeleteEventFlag(ctx->evfid);
        adhocctlDeleteLock();
        sceNetAdhocAuth_lib_6074D8F1();
        return res;
    }

    ctx->min_stack = stackSize - 0xc00;
    ctx->attach_retry = 5;
    return res;
}

/* 0x2914 */
int _sce_pspnet_adhocctl_add_handler(void (*func)(int, int, void *), void *arg, int *id_out)
{
    int lock_res = adhocctlLock();
    if (lock_res < 0)
        return lock_res;

    int i;
    for (i = 0; i < 4; i++) {
        if (g_handlers[i].func == NULL)
            break;
    }
    if (i == 4) {
        adhocctlUnlock(lock_res);
        return 0x80410b12;
    }

    g_handlers[i].id = i;
    g_handlers[i].func = func;
    g_handlers[i].arg = arg;
    if (sceKernelGetModuleGPByAddress(func, &g_handlers[i].gp) < 0) {
        u32 cur_gp;
        __asm__ volatile ("move %0, $gp" : "=r"(cur_gp));
        g_handlers[i].gp = cur_gp;
    }
    *id_out = i;
    adhocctlUnlock(lock_res);
    return 0;
}

/* 0x2a04 */
int adhocctlGetScanInfoInternal(int *buflen, void *buf)
{
    int lock_res = adhocctlLock();
    if (lock_res < 0)
        return lock_res;

    char id_match[16];
    sceKernelMemset(id_match, 0, 10);
    char tch;
    if (g_ctx.adhoc_type == 0)
        tch = 'A';
    else if (g_ctx.adhoc_type == 1)
        tch = 'X';
    else if (g_ctx.adhoc_type == 2)
        tch = 'S';
    else
        return 0x80410b81;

    sceKernelMemcpy(id_match, &tch, 1);
    sceKernelMemcpy(id_match + 1, g_ctx.adhoc_id, 9);

    char prefix[4];
    sceKernelMemset(prefix, 0, 4);
    adhocctlCopySsidPrefix(prefix);

    struct SceNetAdhocctlScanNode *node = (struct SceNetAdhocctlScanNode *)g_ctx.scan_buf;
    SceNetAdhocctlScanInfo *prev = NULL;
    SceNetAdhocctlScanInfo *cur = (SceNetAdhocctlScanInfo *)buf;
    int count = 0;
    int req_bytes = 0x1c;

    while (node != NULL) {
        if (node->ssid_len >= 0x10) {
            int plen = sceNet_lib_D8722983("PSP");
            if (sceNet_lib_E0A81C7C(id_match, node->ssid + plen + 1, 10) == 0 &&
                sceNet_lib_E0A81C7C(prefix, node->ssid, 3) == 0) {
                if (buf != NULL) {
                    if (*buflen < req_bytes)
                        break;
                    if (prev != NULL)
                        prev->next = cur;
                    cur->channel = node->channel;
                    u8 m = (u8)node->ssid[15];
                    if (m == 'L')
                        cur->mode = 0;
                    else
                        cur->mode = (m == 'G') ? 1 : -1;
                    sceKernelMemcpy(cur->name, node->ssid + 17, 8);
                    sceKernelMemcpy(cur->bssid, node->bssid, 6);
                    cur->next = NULL;
                    prev = cur;
                    cur++;
                }
                count++;
                req_bytes += 0x1c;
            }
        }
        node = node->next;
    }

    *buflen = count * 0x1c;
    adhocctlUnlock(lock_res);
    return 0;
}

/* 0x2c4c */
int adhocctlAddGameModeHandlerInternal(void (*func)(int, int, void *), void *arg, int *id_out)
{
    int lock_res = adhocctlLock();
    if (lock_res < 0)
        return lock_res;

    if (g_ctx.pending_error != 0) {
        int err = g_ctx.pending_error;
        adhocctlUnlock(lock_res);
        return err;
    }

    int i;
    for (i = 0; i < 4; i++) {
        if (g_lib_handlers[i].func == NULL)
            break;
    }
    if (i == 4) {
        adhocctlUnlock(lock_res);
        return 0x80410b12;
    }

    g_lib_handlers[i].id = i;
    g_lib_handlers[i].func = func;
    g_lib_handlers[i].arg = arg;
    if (sceKernelGetModuleGPByAddress(func, &g_lib_handlers[i].gp) < 0) {
        u32 cur_gp;
        __asm__ volatile ("move %0, $gp" : "=r"(cur_gp));
        g_lib_handlers[i].gp = cur_gp;
    }
    *id_out = i;
    adhocctlUnlock(lock_res);
    return 0;
}

/* 0x2d54 */
int adhocctlNotifyGameModeHandlers(int event, int error)
{
    int lock_res = adhocctlLock();
    int i;
    for (i = 0; i < 4; i++) {
        if (g_lib_handlers[i].func != NULL) {
            u32 saved_gp;
            __asm__ volatile ("move %0, $gp" : "=r"(saved_gp));
            __asm__ volatile ("move $gp, %0" : : "r"(g_lib_handlers[i].gp));
            g_lib_handlers[i].func(event, error, g_lib_handlers[i].arg);
            __asm__ volatile ("move $gp, %0" : : "r"(saved_gp));
        }
    }
    adhocctlUnlock(lock_res);
    return 0;
}

/* 0x2e04 */
int _sce_pspnet_adhocctl_term(void)
{
    if (g_ctx.flags & 8)
        return 0x80410b10;

    g_ctx.flags |= 8;
    sceKernelSetEventFlag(g_ctx.evfid, 0x200);
    int res = sceKernelWaitThreadEnd(g_ctx.thid, NULL);
    if (res >= 0) {
        sceNetFreeThreadinfo(g_ctx.thid);
        sceKernelTerminateThread(g_ctx.thid);
        sceKernelDeleteThread(g_ctx.thid);
        if (g_ctx.evfid >= 0) {
            sceKernelDeleteEventFlag(g_ctx.evfid);
            g_ctx.evfid = -1;
        }
        adhocctlDeleteLock();
        sceNetAdhocAuth_lib_6074D8F1();
        sceWlanDrv_lib_E093A922();
    }
    g_ctx.flags &= ~8u;
    return res;
}

/* 0x2ed4 */
int adhocctlRequestConnect(const char *name)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 1);
    int d = sceWlanDrv_lib_E47015D7();
    if (d > 0)
        sceKernelDelayThread(d);
    return 0;
}

/* 0x2f80 */
int adhocctlRequestCreate(const char *name)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 0x80);
    return 0;
}

/* 0x3014 */
int adhocctlRequestJoin(const void *scan_info)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    if (scan_info == NULL)
        return 0x80410b04;
    const SceNetAdhocctlScanInfo *info = (const SceNetAdhocctlScanInfo *)scan_info;
    sceKernelMemcpy(&g_ctx.join_next, scan_info, 0x1c);
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (info->name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, info->name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 0x100);
    return 0;
}

/* 0x30cc */
int _sce_pspnet_adhocctl_create_realtime_ibss(const char *name, int num_peers, const u8 *macs, const void *param, u32 timeout, int flag, int min_peers)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    if (name != NULL)
        sceKernelMemcpy(g_ctx.group_name, name, 8);
    else
        sceKernelMemset(g_ctx.group_name, 0, 8);
    g_gm_ctx.num_peers = num_peers;
    sceKernelMemcpy(g_gm_ctx.peers, macs, num_peers * 6);
    sceKernelMemcpy(g_gm_ctx.gm_param, param, 0x50);
    ((SceAdhocctlGameModeParam *)g_gm_ctx.gm_param)->key_len = 0x10;
    sceNetAdhocAuth_lib_312BD812(g_gm_ctx.gm_param + 0x30, 0x10);
    g_gm_ctx.min_peers = min_peers;
    g_gm_ctx.timeout = timeout;
    g_gm_ctx.flag = flag;
    sceKernelSetEventFlag(g_ctx.evfid, 0x10);
    return 0;
}

/* 0x31f0 */
int _sce_pspnet_adhocctl_join_realtime_ibss(const char *name, const u8 *gc_mac, const void *param, u32 timeout, int flag)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    if (name != NULL)
        sceKernelMemcpy(g_ctx.group_name, name, 8);
    else
        sceKernelMemset(g_ctx.group_name, 0, 8);
    g_gm_ctx.num_peers = 0;
    sceKernelMemcpy(g_gm_ctx.peers[0], gc_mac, 6);
    sceKernelMemcpy(g_gm_ctx.gm_param, param, 0x50);
    g_gm_ctx.flag = flag;
    g_gm_ctx.timeout = timeout;
    sceKernelSetEventFlag(g_ctx.evfid, 0x20);
    return 0;
}

/* 0x32e0 */
int adhocctlRequestScan(void)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.scan_buf, 0, 0x780);
    sceKernelSetEventFlag(g_ctx.evfid, 4);
    return 0;
}

/* 0x3344 */
int adhocctlRequestDisconnect(void)
{
    if (g_ctx.state == 3)
        return 0x80410b0b;
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelSetEventFlag(g_ctx.evfid, 2);
    return 0;
}

/* 0x33a4 */
int adhocctlRequestExitGameMode(void)
{
    if (g_ctx.state == 1)
        return 0x80410b0b;
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelSetEventFlag(g_ctx.evfid, 0x40);
    return 0;
}

/* 0x3404 */
int _sce_pspnet_adhocctl_del_handler(int id)
{
    int lock_res = adhocctlLock();
    if (lock_res < 0)
        return lock_res;
    int res = 0;
    if (g_handlers[id].func == NULL)
        res = 0x80410b06;
    else
        g_handlers[id].func = NULL;
    adhocctlUnlock(lock_res);
    return res;
}

/* 0x3470 */
int _sce_pspnet_adhocctl_get_state(int *state)
{
    *state = g_ctx.state;
    return 0;
}

/* 0x3484 */
int adhocctlGetAdhocIdInternal(void *adhoc_id)
{
    sceKernelMemcpy(adhoc_id, &g_ctx.adhoc_type, 0x10);
    return 0;
}

/* 0x34b0 */
int _sce_pspnet_adhocctl_get_param(void *param)
{
    SceNetAdhocctlParameter *out = (SceNetAdhocctlParameter *)param;
    u8 bssid[0x60];
    if (g_ctx.state == 0)
        return 0x80410b09;
    int res = sceNet_lib_A55C914F("wlan", bssid);
    if (res < 0)
        return res;
    sceKernelMemcpy(out->bssid, bssid, 6);
    out->channel = g_ctx.channel;
    sceKernelMemcpy(out->group_name, g_ctx.group_name, 8);
    return sceUtilityGetSystemParamString(1, out->nickname, 0x80);
}

/* 0x3544 */
int adhocctlGetGameModeInfoInternal(void *info)
{
    if (g_ctx.state != 3)
        return 0x80410b0c;
    *(int *)info = g_gm_ctx.num_peers;
    sceKernelMemcpy((u8 *)info + 4, g_gm_ctx.peers, g_gm_ctx.num_peers * 6);
    return 0;
}

/* 0x35b8 */
int adhocctlDelGameModeHandlerInternal(int id)
{
    int lock_res = adhocctlLock();
    if (lock_res < 0)
        return lock_res;
    int res = 0;
    if (g_lib_handlers[id].func == NULL)
        res = 0x80410b06;
    else
        g_lib_handlers[id].func = NULL;
    adhocctlUnlock(lock_res);
    return res;
}

/* 0x3624 */
int adhocctlGetGameModeParamInternal(void *param)
{
    if (g_ctx.state != 3)
        return 0x80410b0c;
    sceKernelMemcpy(param, g_gm_ctx.gm_param, 0x50);
    return 0;
}

/* 0x3680 */
int adhocctlRequestConnectRaw(const char *name)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 0x800);
    return 0;
}

/* 0x3714 */
int adhocctlRequestSuspend(void)
{
    return scePowerRequestSuspend();
}

/* 0x3730 */
int adhocctlRequestCreateRaw(const char *name)
{
    if (g_ctx.flags & 1)
        return 0x80410b10;
    g_ctx.flags |= 1;
    sceKernelMemset(g_ctx.group_name, 0, 8);
    if (name != NULL)
        sceNet_lib_B5CE388A(g_ctx.group_name, name, 8);
    sceKernelSetEventFlag(g_ctx.evfid, 0x400);
    return 0;
}

/* 0x37c4 */
int adhocctl_thread(u32 arglen, void *argp)
{
    (void)arglen;
    struct SceNetAdhocctlCtx *ctx = *(struct SceNetAdhocctlCtx **)argp;
    while (1) {
        u32 bits = 0;
        sceKernelWaitEventFlag(ctx->evfid, 0xfff, 1, &bits, NULL);
        if (bits & 0x200) {
            sceKernelClearEventFlag(ctx->evfid, ~0x200u);
            if (ctx->state == 1)
                adhocctlDisconnectWlan(ctx);
            else if (ctx->state == 3)
                adhocctlCleanupGameMode(ctx, &g_gm_ctx);
            return 0;
        }
        if (bits & 8) {
            sceKernelClearEventFlag(ctx->evfid, ~8u);
            if (ctx->state == 1 || ctx->state == 4 || ctx->state == 5) {
                int res = _sce_pspnet_adhocctl_poll_ifevent(ctx);
                if (res < 0) {
                    adhocctlDisconnectWlan(ctx);
                    adhocctl_call_handler(0, res);
                    ctx->flags &= ~1u;
                    continue;
                }
                if (ctx->state == 4) {
                    adhocctlDetachWlan(ctx);
                    adhocctl_call_handler(6, 0);
                } else if (ctx->state == 5) {
                    adhocctlDetachWlan(ctx);
                    int ev = 7;
                    if (ctx->flags & 0x10) {
                        ctx->flags &= ~0x10u;
                        ev = 8;
                    }
                    adhocctl_call_handler(ev, 0);
                }
                if (ctx->flags & 0x10)
                    ctx->flags &= ~0x10u;
            } else if (ctx->state == 3) {
                int res = _sce_pspnet_adhocctl_poll_ifevent(ctx);
                if (res < 0) {
                    adhocctlCleanupGameMode(ctx, &g_gm_ctx);
                    adhocctl_call_handler(0, res);
                    ctx->flags &= ~1u;
                    continue;
                }
            }
        }
        {
            int err_res = 0;
            int has_err = 0;
            if (bits & 1) {
                sceKernelClearEventFlag(ctx->evfid, ~1u);
                err_res = adhocctlDoConnect(ctx);
                if (err_res < 0) has_err = 1;
                else adhocctl_call_handler(1, 0);
            }
            if (!has_err && (bits & 2)) {
                sceKernelClearEventFlag(ctx->evfid, ~2u);
                adhocctlDisconnectWlan(ctx);
                adhocctl_call_handler(2, 0);
            }
            if (!has_err && (bits & 4)) {
                sceKernelClearEventFlag(ctx->evfid, ~4u);
                err_res = adhocctlDoScan(ctx);
                if (err_res < 0) has_err = 1;
                else adhocctl_call_handler(3, 0);
            }
            if (!has_err && (bits & 0x10)) {
                sceKernelClearEventFlag(ctx->evfid, ~0x10u);
                err_res = _sce_pspnet_adhocctl_create_realtime_ibss_80211(ctx, &g_gm_ctx);
                if (err_res < 0) has_err = 1;
                else adhocctl_call_handler(4, 0);
            }
            if (!has_err && (bits & 0x20)) {
                sceKernelClearEventFlag(ctx->evfid, ~0x20u);
                err_res = _sce_pspnet_adhocctl_join_realtime_ibss_80211(ctx, &g_gm_ctx);
                if (err_res < 0) has_err = 1;
                else adhocctl_call_handler(4, 0);
            }
            if (!has_err && (bits & 0x40)) {
                sceKernelClearEventFlag(ctx->evfid, ~0x40u);
                adhocctlCleanupGameMode(ctx, &g_gm_ctx);
                adhocctl_call_handler(2, 0);
            }
            if (!has_err && (bits & 0x80)) {
                sceKernelClearEventFlag(ctx->evfid, ~0x80u);
                err_res = adhocctlDoCreate(ctx);
                if (err_res < 0) has_err = 1;
                else adhocctl_call_handler(1, 0);
            }
            if (!has_err && (bits & 0x100)) {
                sceKernelClearEventFlag(ctx->evfid, ~0x100u);
                err_res = adhocctlDoJoin(ctx);
                if (err_res < 0) has_err = 1;
                else adhocctl_call_handler(1, 0);
            }
            if (!has_err && (bits & 0x400)) {
                sceKernelClearEventFlag(ctx->evfid, ~0x400u);
                err_res = adhocctlDoConnectRaw(ctx);
                if (err_res < 0) has_err = 1;
                else adhocctlNotifyGameModeHandlers(5, 1);
            }
            if (!has_err && (bits & 0x800)) {
                sceKernelClearEventFlag(ctx->evfid, ~0x800u);
                err_res = adhocctlDoCreateRaw(ctx);
                if (err_res < 0) has_err = 1;
                else adhocctlNotifyGameModeHandlers(5, 1);
            }
            if (has_err) {
                int poll_res = adhocctlCheckCancelFlag(ctx);
                if (poll_res < 0)
                    err_res = poll_res;
                adhocctl_call_handler(0, err_res);
            }
        }
        ctx->flags &= ~1u;
    }
}

/* 0x3bf4 */
