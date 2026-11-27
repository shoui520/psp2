#include "pspnet_adhocctl_internal.h"

void adhocctlLoadSsidPrefix(char *prefix)
{
    u8 reg_param[0x110];
    int reg_hd, cat_hd, key_hd, type;
    u32 size;

    sceKernelMemset(reg_param, 0, 0x110);
    ((SceAdhocctlRegParam *)reg_param)->regtype = 1;
    ((SceAdhocctlRegParam *)reg_param)->version = 1;
    ((SceAdhocctlRegParam *)reg_param)->reg_flags = 1;

    if (sceRegOpenRegistry(reg_param, 2, &reg_hd) >= 0) {
        if (sceRegOpenCategory(reg_hd, "/CONFIG/NETWORK/ADHOC", 2, &cat_hd) >= 0) {
            if (sceRegGetKeyInfo(cat_hd, "ssid_prefix", &key_hd, &type, &size) >= 0 &&
                size == 4 && type == 3) {
                if (sceRegGetKeyValue(cat_hd, key_hd, prefix, 4) >= 0) {
                    sceRegCloseCategory(cat_hd);
                    sceRegCloseRegistry(reg_hd);
                    return;
                }
            }
            sceRegCloseCategory(cat_hd);
        }
        sceRegCloseRegistry(reg_hd);
    }

    int len = sceNet_lib_D8722983("PSP");
    sceKernelMemcpy(prefix, "PSP", len);
}

/* 0x12c0 */
int adhocctlDoConnect(struct SceNetAdhocctlCtx *ctx)
{
    int retry = 0;
    int res;
    int err;

    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    while (1) {
        res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    do {
        char ssid[0x24];
        char nickname[0x80];
        u32 auth_opt[5];
        int ch;

        res = sceNet_lib_5216CBF5("wlan");
        if (res < 0)
            break;
        res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
        if (res < 0)
            break;

        sceKernelMemset(ssid, 0, 0x21);
        res = adhocctlBuildGroupSsidAndChannel(ctx, ssid, &ch);
        if (res < 0)
            break;
        if (sceWlanGetSwitchState() == 0) {
            res = 0x80410b03;
            break;
        }

        sceNet_lib_D5B64E37("wlan", ssid, res);
        res = adhocctlCheckCancelFlag(ctx);
        if ((((u32)res >> 16) & 0xfff) == 0x41) {
            if (res >= 0)
                return res;
            break;
        }
        if (res < 0)
            break;

        res = sceUtilityGetSystemParamString(1, nickname, 0x80);
        if (res < 0)
            break;
        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;

        auth_opt[0] = 1000000;
        auth_opt[1] = 500000;
        auth_opt[2] = 5;
        auth_opt[3] = 30000000;
        auth_opt[4] = 300000000;
        res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
        if (res < 0)
            break;

        ctx->state = 1;
        ctx->channel = ch;
        if (res >= 0)
            return res;
    } while (0);

    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x1534 */
int adhocctlDoScan(struct SceNetAdhocctlCtx *ctx)
{
    int lock_res = 0;
    int retry = 0;
    int res;

    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    ctx->state = 2;
    while (1) {
        res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e) {
            ctx->state = 0;
            adhocctlUnlock(lock_res);
            ctx->pending_error = 0;
            return res;
        }
        if (retry >= ctx->attach_retry) {
            res = 0x80410b05;
            ctx->state = 0;
            adhocctlUnlock(lock_res);
            ctx->pending_error = 0;
            return res;
        }
        sceKernelDelayThread(1000000);
    }

    res = sceNet_lib_5216CBF5("wlan");
    if (res < 0) {
        sceWlanDevDetach();
        ctx->state = 0;
        adhocctlUnlock(lock_res);
        ctx->pending_error = 0;
        return res;
    }

    do {
        u8 scan_req[0x4c];
        int ch = 0;
        int err = 0;
        int i;

        res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
        if (res < 0)
            break;
        if (sceWlanGetSwitchState() == 0) {
            res = 0x80410b03;
            break;
        }

        lock_res = adhocctlLock();
        sceKernelMemset(scan_req, 0, 0x4c);
        sceKernelMemset(ctx->scan_buf, 0, 0x780);
        ((SceAdhocctlScanReq *)scan_req)->bss_type = 2;

        res = sceUtilityGetSystemParamInt(2, &ch);
        if (res < 0)
            break;
        if (ch != 6 && ch != 11) {
            if (ch != 1)
                ch = 0;
        }

        for (i = 0; i < 14; i++)
            scan_req[0x0a + i] = 0;

        if (ch == 0) {
            scan_req[0x0a] = 11;
            scan_req[0x0b] = 6;
            scan_req[0x0c] = 1;
        } else {
            scan_req[0x0a] = (u8)ch;
        }

        ctx->scan_buf_len = 0x780;
        ((SceAdhocctlScanReq *)scan_req)->scan_type = 1;
        ((SceAdhocctlScanReq *)scan_req)->min_dwell = 0;
        ((SceAdhocctlScanReq *)scan_req)->max_dwell = 6;
        ((SceAdhocctlScanReq *)scan_req)->probe_delay = 100;
        sceNet_lib_7BA3ED91("wlan", scan_req, &ctx->scan_buf_len, ctx->scan_buf, &err);
        res = adhocctlCheckCancelFlag(ctx);
        if ((((u32)res >> 16) & 0xfff) != 0x41)
            res = 0;
    } while (0);

    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    ctx->state = 0;
    adhocctlUnlock(lock_res);
    ctx->pending_error = 0;
    return res;
}

/* 0x17b4 */
int adhocctlDoScanQuiet(struct SceNetAdhocctlCtx *ctx)
{
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    u8 scan_req[0x4c];
    u8 scan_buf[0x60];
    sceKernelMemset(scan_req, 0, 0x4c);
    sceKernelMemset(scan_buf, 0, 0x60);
    ((SceAdhocctlScanReq *)scan_req)->bss_type = 2;
    scan_req[0x0a] = 1;
    int i;
    for (i = 1; i < 14; i++)
        scan_req[0x0a + i] = 0;

    ((SceAdhocctlScanReq *)scan_req)->scan_type = 1;
    ((SceAdhocctlScanReq *)scan_req)->min_dwell = 0;
    ((SceAdhocctlScanReq *)scan_req)->max_dwell = 6;
    ((SceAdhocctlScanReq *)scan_req)->probe_delay = 100;
    int buflen = 0x60;
    int err = 0;
    int scan_res = sceNet_lib_7BA3ED91("wlan", scan_req, &buflen, scan_buf, &err);
    int res = adhocctlCheckCancelFlag(ctx);
    if ((((u32)res >> 16) & 0xfff) == 0x41)
        return res;
    return (scan_res < 0) ? scan_res : 0;
}

/* 0x18a8 */
int adhocctlDoCreate(struct SceNetAdhocctlCtx *ctx)
{
    int retry = 0;
    int res;
    int err;

    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    while (1) {
        res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    do {
        char ssid[0x24];
        u8 param[0x70];
        char nickname[0x80];
        u32 auth_opt[5];
        int ch;
        int ssid_len;

        res = sceNet_lib_5216CBF5("wlan");
        if (res < 0)
            break;
        res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
        if (res < 0)
            break;
        res = adhocctlDoScanQuiet(ctx);
        if (res < 0)
            break;

        sceKernelMemset(ssid, 0, 0x21);
        ssid_len = adhocctlBuildGroupSsidAndChannel(ctx, ssid, &ch);
        if (ssid_len < 0) {
            res = ssid_len;
            break;
        }
        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;

        sceKernelMemset(param, 0, 0x70);
        sceKernelMemcpy(param + 8, ssid, ssid_len);
        param[6] = (u8)ch;
        param[7] = (u8)ssid_len;
        ((SceAdhocctlBssParam *)param)->bss_type = 2;
        ((SceAdhocctlBssParam *)param)->beacon_period = 100;
        ((SceAdhocctlBssParam *)param)->capability = 0x22;
        err = 0;
        sceNet_lib_03164B12("wlan", param, &err);
        res = adhocctlCheckCancelFlag(ctx);
        if ((((u32)res >> 16) & 0xfff) == 0x41) {
            if (res >= 0)
                return res;
            break;
        }
        if (res < 0)
            break;

        res = adhocctlDoScanQuiet(ctx);
        if (res < 0)
            break;
        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;

        res = sceUtilityGetSystemParamString(1, nickname, 0x80);
        if (res < 0)
            break;
        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;

        auth_opt[0] = 1000000;
        auth_opt[1] = 500000;
        auth_opt[2] = 5;
        auth_opt[3] = 30000000;
        auth_opt[4] = 300000000;
        res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
        if (res < 0)
            break;

        ctx->state = 1;
        ctx->channel = ch;
        return res;
    } while (0);

    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x1b9c */
int adhocctlDoJoin(struct SceNetAdhocctlCtx *ctx)
{
    int retry = 0;
    int res;
    int err;

    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    while (1) {
        res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    do {
        u8 param[0x70];
        char nickname[0x80];
        u32 auth_opt[5];
        u8 *entry;
        int idx = -1;
        int sys_ch = 0;
        int i;

        res = sceNet_lib_5216CBF5("wlan");
        if (res < 0)
            break;
        res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
        if (res < 0)
            break;

        for (i = 0; i < 20; i++) {
            if (sceNet_lib_E0A81C7C(ctx->scan_buf + i * 0x60 + 4, ctx->join_bssid, 6) == 0) {
                idx = i;
                break;
            }
        }
        if (idx < 0) {
            res = 0x80410b0a;
            break;
        }

        res = sceUtilityGetSystemParamInt(2, &sys_ch);
        if (res < 0)
            break;
        if (sys_ch != 0 && sys_ch != ctx->join_channel) {
            res = 0x80410b11;
            break;
        }
        if (sceWlanGetSwitchState() == 0) {
            res = 0x80410b03;
            break;
        }

        sceKernelMemset(param, 0, 0x70);
        entry = ctx->scan_buf + idx * 0x60;
        sceKernelMemcpy(param, entry + 4, 0x5c);
        err = 0;
        sceNet_lib_389728AB("wlan", param, &err);
        res = adhocctlCheckCancelFlag(ctx);
        if ((((u32)res >> 16) & 0xfff) == 0x41) {
            if (res >= 0)
                return res;
            break;
        }
        if (res < 0)
            break;

        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;

        res = sceUtilityGetSystemParamString(1, nickname, 0x80);
        if (res < 0)
            break;
        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;

        auth_opt[0] = 1000000;
        auth_opt[1] = 500000;
        auth_opt[2] = 5;
        auth_opt[3] = 30000000;
        auth_opt[4] = 300000000;
        res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
        if (res < 0)
            break;

        ctx->state = 1;
        ctx->channel = entry[0x0a];
        return res;
    } while (0);

    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x1ec8 */
int adhocctlDoConnectRaw(struct SceNetAdhocctlCtx *ctx)
{
    int retry = 0;
    int res;

    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    while (1) {
        res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    do {
        char ssid[0x30];
        SceNetAdhocctlWakeParam hd_param;
        int ch;

        res = sceNet_lib_5216CBF5("wlan");
        if (res < 0)
            break;
        res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
        if (res < 0)
            break;

        res = adhocctlBuildGroupSsidAndChannel(ctx, ssid, &ch);
        if (res < 0)
            break;

        sceKernelMemset(&hd_param, 0, sizeof(hd_param));
        hd_param.flags = 0;
        hd_param.enable = 1;
        hd_param.channel = (u8)ch;
        sceKernelMemcpy(hd_param.ssid, ssid, res);

        if (sceWlanGetSwitchState() == 0) {
            res = 0x80410b03;
            break;
        }

        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;
        res = sceWlanSetHostDiscover(1, &hd_param);
        if (res < 0)
            break;

        ctx->state = 4;
        ctx->channel = ch;
        return res;
    } while (0);

    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x20ac */
int adhocctlDoCreateRaw(struct SceNetAdhocctlCtx *ctx)
{
    int retry = 0;
    int res;

    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    while (1) {
        res = sceWlanDevAttach();
        retry++;
        if (res == 0 || res == (int)0x80410d0f)
            break;
        if (res < 0 && res != (int)0x80410d0e)
            return res;
        if (retry >= ctx->attach_retry)
            return 0x80410b05;
        sceKernelDelayThread(1000000);
    }

    ctx->flags &= ~2u;
    do {
        char ssid[0x30];
        SceNetAdhocctlWakeParam wu_param;
        int ch;

        res = sceNet_lib_5216CBF5("wlan");
        if (res < 0)
            break;
        res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
        if (res < 0)
            break;

        res = adhocctlBuildGroupSsidAndChannel(ctx, ssid, &ch);
        if (res < 0)
            break;

        sceKernelMemset(&wu_param, 0, sizeof(wu_param));
        wu_param.flags = 0;
        wu_param.enable = 1;
        wu_param.channel = (u8)ch;
        sceKernelMemcpy(wu_param.ssid, ssid, res);

        if (sceWlanGetSwitchState() == 0) {
            res = 0x80410b03;
            break;
        }

        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;
        res = sceWlanSetWakeUp(1, &wu_param);
        if (res < 0)
            break;
        res = scePowerRequestSuspend();
        if (res < 0) {
            sceWlanSetWakeUp(0, &wu_param);
            break;
        }

        ctx->state = 5;
        ctx->channel = ch;
        return res;
    } while (0);

    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x22b8 */
void adhocctlDisconnectWlan(struct SceNetAdhocctlCtx *ctx)
{
    int err = 0;
    sceNetAdhocAuth_lib_2E6AA271();
    sceWlanSetHostDiscover(0, NULL);
    sceWlanSetWakeUp(0, NULL);
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_DA02F383("wlan", &err);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    ctx->state = 0;
    ctx->pending_error = 0;
}

/* 0x233c */
void adhocctlDetachWlan(struct SceNetAdhocctlCtx *ctx)
{
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    ctx->state = 0;
}

/* 0x2390 */
