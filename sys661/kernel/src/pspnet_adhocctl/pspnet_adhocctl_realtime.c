#include "pspnet_adhocctl_internal.h"

int _sce_pspnet_adhocctl_create_realtime_ibss_80211(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm)
{
    s64 start_time;
    u32 rem_timeout = 0;
    int res = 0;
    int err = 0;

    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    start_time = sceKernelGetSystemTimeWide();
    g_saved_power_save = -1;

    do {
        u8 param[0x70];
        char nickname[0x80];
        u32 auth_opt[5];
        u16 wparam[8];
        u8 ssid_len;
        u16 key_len, saved_klen, bp;
        int payload_len, total_len, retry_cnt, listen_id;
        u32 be_len;
        u8 *p;
        u16 be_klen, be_plen, be_two, be_a4, be_one;
        int i;

        while (1) {
            if (gm->timeout != 0) {
                s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
                if ((u64)elapsed >= (u64)gm->timeout) {
                    res = 0x80410b05;
                    break;
                }
                rem_timeout = gm->timeout - (u32)elapsed;
            } else {
                rem_timeout = 0;
            }
            res = sceWlanDevAttach();
            if (res == 0 || res == (int)0x80410d0f)
                break;
            if (res < 0 && res != (int)0x80410d0e)
                return res;
            sceKernelDelayThread(1000000);
        }
        if (res < 0 && res != (int)0x80410d0f)
            break;

        ctx->flags &= ~2u;
        res = sceNet_lib_5216CBF5("wlan");
        if (res < 0)
            break;
        res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
        if (res < 0)
            break;
        if (sceWlanGetSwitchState() == 0) {
            res = 0x80410b03;
            break;
        }

        if (!(gm->flag & 1)) {
            char ssid[0x30];
            u8 ch, offset;
            sceKernelMemset(ssid, 0, 0x21);
            res = adhocctlBuildGameModeSsid(ctx, ssid);
            if (res < 0)
                break;
            gm->gm_param[0x0c] = (u8)res;
            sceKernelMemcpy(gm->gm_param + 0x0e, ssid, res & 0xff);
            adhocctlSelectGameModeChannel(gm, &ch, &offset);
            res = adhocctlCheckCancelFlag(ctx);
            if ((((u32)res >> 16) & 0xfff) == 0x41) {
                if (res >= 0)
                    return res;
                break;
            }
            if (res < 0)
                break;
            gm->gm_param[0x46] = offset;
            gm->gm_param[0x0d] = ch;
        }

        if (gm->flag & 2) {
            res = adhocctlSetWlanPowerSave(4);
            if (res < 0)
                break;
            g_saved_power_save = res;
            res = sceWlanDrv_lib_5BAA1FE5(1);
            if (res < 0)
                break;
        }

        sceKernelMemset(param, 0, 0x70);
        ssid_len = gm->gm_param[0x0c];
        param[7] = ssid_len;
        sceKernelMemcpy(param + 8, gm->gm_param + 0x0e, ssid_len);
        param[6] = gm->gm_param[0x0d];
        ((SceAdhocctlBssParam *)param)->bss_type = 2;
        ((SceAdhocctlBssParam *)param)->beacon_period = ((SceAdhocctlGameModeParam *)gm->gm_param)->beacon_period;
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

        key_len = ((SceAdhocctlGameModeParam *)gm->gm_param)->key_len;
        payload_len = key_len + gm->num_peers * 6 + 19;
        total_len = key_len + gm->num_peers * 6 + 25;
        g_ptp_buf[0] = 1;
        g_ptp_buf[1] = 1;
        be_len = __builtin_bswap32(payload_len);
        sceKernelMemcpy(g_ptp_buf + 2, &be_len, 4);

        p = g_ptp_buf + 6;
        p[0] = 1;
        be_klen = __builtin_bswap16(key_len);
        sceKernelMemcpy(p + 1, &be_klen, 2);
        if (key_len != 0)
            sceKernelMemcpy(p + 3, gm->gm_param + 0x30, key_len);
        p += 3 + key_len;

        p[0] = 2;
        be_plen = __builtin_bswap16((u16)(gm->num_peers * 6));
        sceKernelMemcpy(p + 1, &be_plen, 2);
        for (i = 0; i < gm->num_peers; i++)
            sceKernelMemcpy(p + 3 + i * 6, gm->peers[i], 6);
        p += 3 + gm->num_peers * 6;

        p[0] = 3;
        be_two = __builtin_bswap16(2);
        sceKernelMemcpy(p + 1, &be_two, 2);
        be_a4 = __builtin_bswap16(((SceAdhocctlGameModeParam *)gm->gm_param)->port_be);
        sceKernelMemcpy(p + 3, &be_a4, 2);

        p[5] = 4;
        be_one = __builtin_bswap16(1);
        sceKernelMemcpy(p + 6, &be_one, 2);
        p[8] = gm->gm_param[0x46];

        p[9] = 5;
        sceKernelMemcpy(p + 10, &be_one, 2);
        p[12] = gm->gm_param[0x47];

        retry_cnt = (int)(gm->timeout / 200000) + 1;
        listen_id = sceNetAdhocPtpListen(gm->peers[0], 0x8001, 0x2000, 200000, retry_cnt, gm->num_peers - 1, 0);
        if (listen_id < 0) {
            res = listen_id;
            break;
        }

        res = adhocctlAcceptGameModePeers(gm, start_time, listen_id, g_ptp_buf, total_len);
        if (res < 0) {
            sceNetAdhocPtpClose(listen_id, 0);
            break;
        }
        res = sceNetAdhocPtpClose(listen_id, 0);
        if (res < 0)
            break;

        sceNetAdhocAuth_lib_6CE209A3();
        res = sceWlanDrv_lib_56F467CA(wparam);
        if (res < 0)
            break;
        bp = ((SceAdhocctlGameModeParam *)gm->gm_param)->beacon_period;
        wparam[0] = bp - 2;
        wparam[1] = 0xff;
        wparam[2] = (1000 / (int)bp) + 1;
        res = sceWlanDrv_lib_2D0FAE4E(wparam);
        if (res < 0)
            break;
        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;

        sceKernelMemcpy(gm->gm_param + 6, gm->peers[0], 6);
        gm->gm_param[6] |= 3;
        ((SceAdhocctlGameModeParam *)gm->gm_param)->active_flag = 1;
        gm->gm_param[0x44] = (u8)gm->num_peers;
        gm->gm_param[0x43] = 0;
        sceNetAdhocAuth_lib_72AAC6D3(((SceAdhocctlGameModeParam *)gm->gm_param)->key_len, ((SceAdhocctlGameModeParam *)gm->gm_param)->key);
        saved_klen = ((SceAdhocctlGameModeParam *)gm->gm_param)->key_len;
        ((SceAdhocctlGameModeParam *)gm->gm_param)->key_len = 0;
        err = 0;
        sceNet_lib_0D633F53("wlan", gm->gm_param, gm->peers[0], &err);
        ((SceAdhocctlGameModeParam *)gm->gm_param)->key_len = saved_klen;
        res = adhocctlCheckCancelFlag(ctx);
        if ((((u32)res >> 16) & 0xfff) == 0x41) {
            if (res >= 0)
                return res;
            break;
        }
        if (res < 0)
            break;

        ctx->state = 3;
        ctx->channel = gm->gm_param[0x0d];
        if (res >= 0)
            return res;
    } while (0);

    sceNetAdhocAuth_lib_72AAC6D3(0, NULL);
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    if (gm->flag & 2) {
        sceWlanDrv_lib_5BAA1FE5(0);
        if (g_saved_power_save >= 0)
            adhocctlSetWlanPowerSave(g_saved_power_save);
    }
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x46e0 */
int _sce_pspnet_adhocctl_join_realtime_ibss_80211(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm)
{
    int ptp_id = -1;
    s64 start_time;
    u32 rem_timeout = 0;
    int res = 0;
    int err = 0;

    if (ctx->state != 0)
        return 0x80410b02;
    if (sceWlanGetSwitchState() == 0)
        return 0x80410b03;

    start_time = sceKernelGetSystemTimeWide();
    g_saved_power_save = -1;

    do {
        u8 scan_req[0x4c];
        u8 join_param[0x70];
        char nickname[0x80];
        u32 auth_opt[5];
        u8 self_mac[16];
        u16 wparam[8];
        struct SceNetAdhocctlScanNode *found = NULL;
        u32 body_len;
        int tag_len, i, self_idx;
        void *val;
        u16 klen, bp, saved_klen;
        int scan_fail = 0;

        while (1) {
            if (gm->timeout != 0) {
                s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
                if ((u64)elapsed >= (u64)gm->timeout) {
                    res = 0x80410b05;
                    break;
                }
                rem_timeout = gm->timeout - (u32)elapsed;
            } else {
                rem_timeout = 0;
            }
            res = sceWlanDevAttach();
            if (res == 0 || res == (int)0x80410d0f)
                break;
            if (res < 0 && res != (int)0x80410d0e)
                return res;
            sceKernelDelayThread(1000000);
        }
        if (res < 0 && res != (int)0x80410d0f)
            break;

        ctx->flags &= ~2u;
        res = sceNet_lib_5216CBF5("wlan");
        if (res < 0)
            break;
        res = sceNet_lib_AB7DD9A5("wlan", ctx->evfid, 8);
        if (res < 0)
            break;
        if (sceWlanGetSwitchState() == 0) {
            res = 0x80410b03;
            break;
        }

        if (!(gm->flag & 1)) {
            char ssid[0x30];
            sceKernelMemset(ssid, 0, 0x21);
            res = adhocctlBuildGameModeSsid(ctx, ssid);
            if (res < 0)
                break;
            gm->gm_param[0x0c] = (u8)res;
            sceKernelMemcpy(gm->gm_param + 0x0e, ssid, res & 0xff);
        }

        if (gm->flag & 2) {
            res = adhocctlSetWlanPowerSave(4);
            if (res < 0)
                break;
            g_saved_power_save = res;
            res = sceWlanDrv_lib_5BAA1FE5(1);
            if (res < 0)
                break;
        }

        sceKernelMemset(scan_req, 0, 0x4c);
        sceKernelMemcpy(scan_req + 0x1c, gm->gm_param + 0x0e, gm->gm_param[0x0c]);
        scan_req[0x18] = gm->gm_param[0x0c];
        ((SceAdhocctlScanReq *)scan_req)->bss_type = 2;
        sceKernelMemcpy(scan_req + 0x0a, g_channels, 3);
        ((SceAdhocctlScanReq *)scan_req)->scan_type = 1;
        ((SceAdhocctlScanReq *)scan_req)->min_dwell = 0;
        ((SceAdhocctlScanReq *)scan_req)->max_dwell = 6;
        ((SceAdhocctlScanReq *)scan_req)->probe_delay = 100;

        while (1) {
            int buflen = 0xc00;
            struct SceNetAdhocctlScanNode *node;
            sceKernelMemset(g_gm_scan_buf, 0, 0xc00);
            err = 0;
            sceNet_lib_7BA3ED91("wlan", scan_req, &buflen, g_gm_scan_buf, &err);
            res = adhocctlCheckCancelFlag(ctx);
            if ((((u32)res >> 16) & 0xfff) == 0x41) {
                if (res >= 0)
                    return res;
                scan_fail = 1;
                break;
            }
            if (res < 0) {
                scan_fail = 1;
                break;
            }

            if (gm->timeout != 0) {
                s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
                if ((u64)elapsed >= (u64)gm->timeout) {
                    res = 0x80410b05;
                    scan_fail = 1;
                    break;
                }
                rem_timeout = gm->timeout - (u32)elapsed;
            } else {
                rem_timeout = 0;
            }

            if (buflen == 0)
                continue;

            node = (struct SceNetAdhocctlScanNode *)g_gm_scan_buf;
            while (node != NULL) {
                if (sceNet_lib_E0A81C7C(node->ssid, gm->gm_param + 0x0e, 0x20) == 0 && node->mode == 2) {
                    found = node;
                    break;
                }
                node = node->next;
            }
            if (found != NULL)
                break;
        }
        if (scan_fail)
            break;

        sceKernelMemset(join_param, 0, 0x70);
        sceKernelMemcpy(join_param, found->bssid, 0x5c);
        ((SceAdhocctlJoinParam *)join_param)->auth_flag = 1;
        err = 0;
        sceNet_lib_389728AB("wlan", join_param, &err);
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

        auth_opt[0] = (u32)-1;
        auth_opt[1] = 500000;
        auth_opt[2] = 5;
        auth_opt[3] = 30000000;
        auth_opt[4] = 300000000;
        res = sceNetAdhocAuth_lib_89F2A732("wlan", 0x30, 0x2000, auth_opt, 16, nickname);
        if (res < 0)
            break;

        res = sceNetGetLocalEtherAddr(self_mac);
        if (res < 0)
            break;

        while (1) {
            int retry_cnt = (int)(gm->timeout / 200000) + 1;
            ptp_id = sceNetAdhocPtpOpen(self_mac, 0x8001, gm->peers[0], 0x8001, 0x2000, 200000, retry_cnt, 0);
            if (ptp_id < 0) {
                res = ptp_id;
                break;
            }
            if (gm->timeout != 0) {
                s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
                if ((u64)elapsed >= (u64)gm->timeout) {
                    res = 0x80410b05;
                    break;
                }
                rem_timeout = gm->timeout - (u32)elapsed;
            } else {
                rem_timeout = 0;
            }
            res = sceNetAdhocPtpConnect(ptp_id, rem_timeout, 0);
            if (res >= 0)
                break;
            if (res != (int)0x80410718)
                break;
            sceNetAdhocPtpClose(ptp_id, 0);
            ptp_id = -1;
            sceKernelDelayThread(1000000);
        }
        if (res < 0)
            break;

        res = wait_for_read(gm, start_time, ptp_id, g_ptp_buf, 6);
        if (res < 0)
            break;
        if ((s8)g_ptp_buf[0] != 1 || g_ptp_buf[1] != g_ptp_buf[0]) {
            res = 0x80410b82;
            break;
        }

        sceKernelMemcpy(&body_len, g_ptp_buf + 2, 4);
        body_len = __builtin_bswap32(body_len);
        if ((int)body_len >= 0x3fb) {
            res = 0x80410b83;
            break;
        }

        res = wait_for_read(gm, start_time, ptp_id, g_ptp_buf + 6, body_len);
        if (res < 0)
            break;

        res = sceNetAdhocPtpClose(ptp_id, 0);
        ptp_id = -1;
        if (res < 0)
            break;

        val = get_taginfo(1, g_ptp_buf + 6, body_len, &tag_len);
        if (val == NULL || tag_len >= 17) {
            res = 0x80410b84;
            break;
        }
        klen = (u16)tag_len;
        ((SceAdhocctlGameModeParam *)gm->gm_param)->key_len = klen;
        if (klen != 0)
            sceKernelMemcpy(gm->gm_param + 0x30, val, klen);

        val = get_taginfo(2, g_ptp_buf + 6, body_len, &tag_len);
        if (val == NULL || tag_len < 12 || tag_len >= 97 || (tag_len % 6) != 0) {
            res = 0x80410b85;
            break;
        }
        gm->num_peers = tag_len / 6;
        for (i = 0; i < gm->num_peers; i++)
            sceKernelMemcpy(gm->peers[i], (u8 *)val + i * 6, 6);

        val = get_taginfo(3, g_ptp_buf + 6, body_len, &tag_len);
        if (val == NULL || tag_len != 2) {
            res = 0x80410b86;
            break;
        }
        sceKernelMemcpy(gm->gm_param + 0x40, val, 2);
        ((SceAdhocctlGameModeParam *)gm->gm_param)->port_be = __builtin_bswap16(((SceAdhocctlGameModeParam *)gm->gm_param)->port_be);

        val = get_taginfo(4, g_ptp_buf + 6, body_len, &tag_len);
        if (val == NULL || tag_len != 1) {
            res = 0x80410b87;
            break;
        }
        gm->gm_param[0x46] = *(u8 *)val;

        val = get_taginfo(5, g_ptp_buf + 6, body_len, &tag_len);
        if (val == NULL || tag_len != 1) {
            res = 0x80410b88;
            break;
        }
        gm->gm_param[0x47] = *(u8 *)val;

        sceNetAdhocAuth_lib_6CE209A3();
        res = sceWlanDrv_lib_56F467CA(wparam);
        if (res < 0)
            break;
        bp = found->beacon_period;
        wparam[0] = bp - 2;
        wparam[1] = 0xff;
        wparam[2] = (1000 / (int)*(u32 *)&found->beacon_period) + 1;
        res = sceWlanDrv_lib_2D0FAE4E(wparam);
        if (res < 0)
            break;
        res = adhocctlCheckCancelFlag(ctx);
        if (res < 0)
            break;

        sceKernelMemcpy(gm->gm_param + 6, gm->peers[0], 6);
        gm->gm_param[6] |= 3;
        ((SceAdhocctlGameModeParam *)gm->gm_param)->beacon_period = found->beacon_period;
        for (self_idx = 1; self_idx < gm->num_peers; self_idx++) {
            if (sceNet_lib_E0A81C7C(gm->peers[self_idx], self_mac, 6) == 0)
                break;
        }
        ((SceAdhocctlGameModeParam *)gm->gm_param)->active_flag = 1;
        gm->gm_param[0x43] = (u8)self_idx;
        gm->gm_param[0x44] = (u8)gm->num_peers;
        sceNetAdhocAuth_lib_72AAC6D3(((SceAdhocctlGameModeParam *)gm->gm_param)->key_len, ((SceAdhocctlGameModeParam *)gm->gm_param)->key);
        saved_klen = ((SceAdhocctlGameModeParam *)gm->gm_param)->key_len;
        ((SceAdhocctlGameModeParam *)gm->gm_param)->key_len = 0;
        err = 0;
        sceNet_lib_0D633F53("wlan", gm->gm_param, gm->peers[0], &err);
        ((SceAdhocctlGameModeParam *)gm->gm_param)->key_len = saved_klen;
        res = adhocctlCheckCancelFlag(ctx);
        if ((((u32)res >> 16) & 0xfff) == 0x41) {
            if (res >= 0)
                return res;
            break;
        }
        if (res < 0)
            break;

        ctx->state = 3;
        ctx->channel = found->channel;
        if (res >= 0)
            return res;
    } while (0);

    if (ptp_id >= 0)
        sceNetAdhocPtpClose(ptp_id, 0);

    sceNetAdhocAuth_lib_72AAC6D3(0, NULL);
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    if (gm->flag & 2) {
        sceWlanDrv_lib_5BAA1FE5(0);
        if (g_saved_power_save >= 0)
            adhocctlSetWlanPowerSave(g_saved_power_save);
    }
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    return res;
}

/* 0x50fc */
int adhocctlSelectGameModeChannel(struct SceNetAdhocctlGameModeCtx *gm, u8 *ch_out, u8 *offset_out)
{
    u8 scan_req[0x4c];
    u32 mask = 0;
    sceKernelMemset(scan_req, 0, 0x4c);
    ((SceAdhocctlScanReq *)scan_req)->bss_type = 2;
    sceKernelMemcpy(scan_req + 0x0a, g_channels, 3);
    ((SceAdhocctlScanReq *)scan_req)->scan_type = 1;
    ((SceAdhocctlScanReq *)scan_req)->min_dwell = 0;
    ((SceAdhocctlScanReq *)scan_req)->max_dwell = 6;
    ((SceAdhocctlScanReq *)scan_req)->probe_delay = 500;
    sceKernelMemset(g_gm_scan_buf, 0, 0xc00);
    int buflen = 0xc00;
    int err = 0;
    int res = sceNet_lib_7BA3ED91("wlan", scan_req, &buflen, g_gm_scan_buf, &err);
    if (res < 0)
        return res;

    sceKernelMemset(g_ch_load, 0, sizeof(g_ch_load));
    if (buflen != 0) {
        struct SceNetAdhocctlScanNode *node = (struct SceNetAdhocctlScanNode *)g_gm_scan_buf;
        while (node != NULL) {
            if (node->mode == 2 && node->gm_ie_type == 2) {
                int cidx;
                for (cidx = 0; cidx < 3; cidx++) {
                    if (node->channel == g_channels[cidx])
                        break;
                }
                if (cidx != 3) {
                    int start_off = node->gm_slot_start;
                    int n = node->gm_peer_count;
                    int step = node->gm_slot_step;
                    if (start_off >= 15 && start_off + step * (n - 1) < 256) {
                        int k;
                        for (k = 0; k < n; k++) {
                            int slot = (start_off + step * k - 15) / 8;
                            g_ch_load[cidx].bitmap |= (1u << slot);
                        }
                        g_ch_load[cidx].count += n;
                    }
                }
            }
            node = node->next;
        }
    }

    int sys_ch = 0;
    res = sceUtilityGetSystemParamInt(2, &sys_ch);
    if (res < 0)
        return res;
    if (sys_ch != 6 && sys_ch != 11) {
        if (sys_ch != 1)
            sys_ch = 0;
    }

    int i;
    for (i = 0; i < gm->num_peers; i++)
        mask |= (1u << i);

    while (1) {
        int best_idx = -1;
        int best_cnt = 0;
        for (i = 0; i < 3; i++) {
            if (g_ch_load[i].skip == 1)
                continue;
            if (sys_ch == 0 || g_channels[i] == sys_ch) {
                if (best_idx == -1 || g_ch_load[i].count < best_cnt) {
                    best_idx = i;
                    best_cnt = g_ch_load[i].count;
                }
            }
        }
        if (best_idx == -1)
            return 0x80410b0d;
        if (g_ch_load[best_idx].count >= 8 || g_ch_load[best_idx].count + gm->num_peers >= 17)
            return 0x80410b0d;

        int max_shift = 29 - gm->num_peers;
        int shift = 0;
        if (max_shift >= 0) {
            for (shift = 0; shift <= max_shift; shift++) {
                if ((g_ch_load[best_idx].bitmap & (mask << shift)) == 0)
                    break;
            }
        }
        if (shift > 29 - gm->num_peers) {
            g_ch_load[best_idx].skip = 1;
            continue;
        }
        *ch_out = g_channels[best_idx];
        *offset_out = (u8)(shift * 8 + 15);
        return res;
    }
}

/* 0x550c */
int adhocctlSetWlanPowerSave(int power_save)
{
    u8 buf[0x18];
    sceKernelMemset(buf, 0, 0x18);
    int res = sceNet_lib_B20F84F8("wlan", buf);
    if (res < 0)
        return res;
    int old = buf[0x11];
    buf[0x11] = (u8)power_save;
    res = sceNet_lib_AFA11338("wlan", buf);
    return (res < 0) ? res : old;
}

/* 0x5580 */
void adhocctlCleanupGameMode(struct SceNetAdhocctlCtx *ctx, struct SceNetAdhocctlGameModeCtx *gm)
{
    int err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    sceNetAdhocAuth_lib_72AAC6D3(0, NULL);
    sceNetAdhocAuth_lib_2E6AA271();
    sceNet_lib_AB7DD9A5("wlan", 0, 0);
    err = 0;
    sceNet_lib_DA02F383("wlan", &err);
    if (gm->flag & 2) {
        sceWlanDrv_lib_5BAA1FE5(0);
        if (g_saved_power_save >= 0)
            adhocctlSetWlanPowerSave(g_saved_power_save);
    }
    sceNet_lib_D2422E4D("wlan");
    sceWlanDevDetach();
    ctx->state = 0;
    ctx->pending_error = 0;
}

/* 0x5658 */
int adhocctlAcceptGameModePeers(struct SceNetAdhocctlGameModeCtx *gm, s64 start_time, int listen_id, const void *ie_buf, int ie_len)
{
    u8 accepted_macs[15][6];
    int accepted_cnt = 0;
    int res = 0;
    int client_id = -1;
    u8 *next_mac_slot = (u8 *)accepted_macs;

    sceKernelMemset(accepted_macs, 0, 0x5a);
    while (accepted_cnt < gm->num_peers - 1) {
        u32 rem = 0;
        u8 peer_mac[6];
        u16 peer_port;
        int idx, dup, j, slen;

        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                res = 0x80410b05;
                break;
            }
            rem = gm->timeout - (u32)elapsed;
        }
        res = sceNetAdhocPtpAccept(listen_id, peer_mac, &peer_port, rem, 0);
        if (res < 0)
            break;
        client_id = res;
        if (peer_port != 0x8001) {
            sceNetAdhocPtpClose(client_id, 0);
            client_id = -1;
            continue;
        }
        for (idx = 1; idx < gm->num_peers; idx++) {
            if (sceNet_lib_E0A81C7C(gm->peers[idx], peer_mac, 6) == 0)
                break;
        }
        if (idx == gm->num_peers) {
            sceNetAdhocPtpClose(client_id, 0);
            client_id = -1;
            continue;
        }
        dup = 0;
        for (j = 0; j < accepted_cnt; j++) {
            if (sceNet_lib_E0A81C7C(accepted_macs[j], peer_mac, 6) == 0) {
                sceNetAdhocPtpClose(client_id, 0);
                client_id = -1;
                dup = 1;
            }
        }
        if (dup)
            continue;

        rem = 0;
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                res = 0x80410b05;
                break;
            }
            rem = gm->timeout - (u32)elapsed;
        }
        slen = ie_len;
        res = sceNetAdhocPtpSend(client_id, ie_buf, &slen, rem, 0);
        if (res < 0)
            break;

        rem = 0;
        if (gm->timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)gm->timeout) {
                res = 0x80410b05;
                break;
            }
            rem = gm->timeout - (u32)elapsed;
        }
        if (rem > 3000000)
            rem = 3000000;
        res = sceNetAdhocPtpFlush(client_id, rem, 0);
        if (res < 0) {
            if (rem != 3000000 || (res != (int)0x80410715 && res != (int)0x8041070c))
                break;
        }
        res = sceNetAdhocPtpClose(client_id, 0);
        client_id = -1;
        if (res < 0)
            break;
        sceKernelMemcpy(next_mac_slot, peer_mac, 6);
        next_mac_slot += 6;
        accepted_cnt++;
    }

    if (client_id >= 0)
        sceNetAdhocPtpClose(client_id, 0);

    if (accepted_cnt + 1 >= gm->min_peers && res == (int)0x80410715)
        res = 0;
    return res;
}

/* 0x5a18 */
int wait_for_read(struct SceNetAdhocctlGameModeCtx *gm, s64 start_time, int ptp_id, void *buf, int len)
{
    int res = 0;
    u8 *p = (u8 *)buf;

    while (len > 0) {
        u32 timeout = gm->timeout;
        u32 rem = 0;
        if (timeout != 0) {
            s64 elapsed = sceKernelGetSystemTimeWide() - start_time;
            if ((u64)elapsed >= (u64)timeout)
                return 0x80410b05;
            rem = timeout - (u32)elapsed;
        }
        int rlen = len;
        res = sceNetAdhocPtpRecv(ptp_id, p, &rlen, rem, 0);
        if (res < 0)
            return res;
        len -= rlen;
        p += rlen;
    }
    return res;
}

/* 0x5b28 */
void *get_taginfo(int tag, void *buf, u32 len, int *tag_len_out)
{
    u8 *cur = (u8 *)buf;
    u8 *end = cur + len;
    u8 target = (u8)tag;

    while (cur < end) {
        u8 *lptr = cur + 1;
        if (end - lptr < 2)
            return NULL;
        u8 t = *cur;
        cur = lptr + 2;
        u16 tlen;
        sceKernelMemcpy(&tlen, lptr, 2);
        tlen = __builtin_bswap16(tlen);
        if (end - cur < (int)tlen)
            return NULL;
        if (t == target) {
            *tag_len_out = tlen;
            return cur;
        }
        cur += tlen;
    }
    return NULL;
}

/* 0x5bf4 */
int adhocctlCreateLock(void)
{
    int res = sceKernelCreateLwMutex(g_mutex_work, "SceNetAdhocctl", 0x200, 0, NULL);
    if (res < 0)
        return res;
    g_mutex_created = 1;
    return 0;
}

/* 0x5c44 */
void adhocctlDeleteLock(void)
{
    if (g_mutex_created != 0)
        sceKernelDeleteLwMutex(g_mutex_work);
    g_mutex_created = 0;
}

/* 0x5c88 */
int adhocctlLock(void)
{
    return sceKernelLockLwMutex(g_mutex_work, 1, NULL);
}

/* 0x5cb0 */
void adhocctlUnlock(int lock_res)
{
    if (lock_res == 0)
        sceKernelUnlockLwMutex(g_mutex_work, 1);
}
