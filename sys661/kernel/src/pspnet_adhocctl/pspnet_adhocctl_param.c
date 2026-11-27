#include "pspnet_adhocctl_internal.h"

int make_ssid(struct SceNetAdhocctlCtx *ctx, char *ssid, int mode_char, const char *group_name)
{
    char mch = (char)mode_char;
    char tch;
    if (ctx->adhoc_type == 0)
        tch = 'A';
    else if (ctx->adhoc_type == 1)
        tch = 'X';
    else if (ctx->adhoc_type == 2)
        tch = 'S';
    else
        return 0x80410b04;

    char prefix[16];
    adhocctlCopySsidPrefix(prefix);
    sceKernelMemset(ssid, 0, 0x21);
    sceKernelMemcpy(ssid, prefix, 3);
    sceKernelMemcpy(ssid + 3, "_", 1);
    sceKernelMemcpy(ssid + 4, &tch, 1);
    sceKernelMemcpy(ssid + 5, ctx->adhoc_id, 9);
    sceKernelMemcpy(ssid + 14, "_", 1);
    sceKernelMemcpy(ssid + 15, &mch, 1);
    sceKernelMemcpy(ssid + 16, "_", 1);

    int pos = 17;
    if (group_name != NULL && group_name[0] != 0) {
        int i = 0;
        while (i < 8 && group_name[i] != 0) {
            sceKernelMemcpy(ssid + pos, &group_name[i], 1);
            i++;
            pos++;
        }
    }
    return pos;
}

/* 0x24ec */
int adhocctlBuildGroupSsidAndChannel(struct SceNetAdhocctlCtx *ctx, char *ssid, int *ch_out)
{
    int ch;
    sceKernelMemset(ssid, 0, 0x21);
    int res = sceUtilityGetSystemParamInt(2, &ch);
    if (res < 0)
        return res;
    if (ch == 6 || ch == 11) {
        *ch_out = ch;
    } else {
        if (ch != 1)
            ch = 11;
        *ch_out = ch;
    }
    if ((u32)(*ch_out - 1) >= 14)
        return 0x80410b81;
    return make_ssid(ctx, ssid, 'L', ctx->group_name);
}

/* 0x25b0 */
int adhocctlBuildGameModeSsid(struct SceNetAdhocctlCtx *ctx, char *ssid)
{
    sceKernelMemset(ssid, 0, 0x21);
    return make_ssid(ctx, ssid, 'G', ctx->group_name);
}

/* 0x2600 */
