#include "pspnet_adhocctl_internal.h"

int _sce_pspnet_adhocctl_handle_ifevent(struct SceNetAdhocctlCtx *ctx)
{
    int ev, reason;
    while (1) {
        int res = sceNet_lib_D1BE2CE9("wlan", &ev, &reason);
        if (res == (int)0x80410184) {
            u32 out;
            res = sceKernelWaitEventFlag(ctx->evfid, 8, 1, &out, NULL);
            if (res < 0)
                return res;
            sceKernelClearEventFlag(ctx->evfid, ~8u);
            continue;
        }
        if (res < 0)
            return res;
        if (ev == 8) {
            if (ctx->state == 5)
                ctx->flags |= 0x10;
            return 0;
        }
        if (ev == 9)
            return 0;
    }
}

/* 0x3d0c */
int _sce_pspnet_adhocctl_poll_ifevent(struct SceNetAdhocctlCtx *ctx)
{
    int ev, reason;
    return adhocctlWaitWlanEvent(ctx, &ev, &reason);
}

/* 0x3d2c */
int adhocctlCheckCancelFlag(struct SceNetAdhocctlCtx *ctx)
{
    u32 out;
    int res = sceKernelPollEventFlag(ctx->evfid, 8, 1, &out);
    if (res == (int)0x800201af)
        return 0;
    if (res < 0)
        return res;
    if (out & 8) {
        sceKernelClearEventFlag(ctx->evfid, ~8u);
        int ev, reason;
        res = adhocctlWaitWlanEvent(ctx, &ev, &reason);
    }
    return res;
}

/* 0x3db4 */
int adhocctlWaitWlanEvent(struct SceNetAdhocctlCtx *ctx, int *ev, int *reason)
{
    int ret = 0;
    while (1) {
        int res = sceNet_lib_D1BE2CE9("wlan", ev, reason);
        if (res == (int)0x80410184)
            break;
        if (res < 0) {
            ret = res;
            break;
        }
        if (*ev == 5) {
            if (!(ctx->flags & 2)) {
                ctx->flags |= 2;
                ret = 0x80410b0e;
            }
        } else if (*ev < 6) {
            if (*ev == 4) {
                if (ctx->state == 1)
                    ctx->pending_error = 0x80410b83;
                ret = 0x80410b03;
            }
        } else if (*ev == 7) {
            if (ctx->state == 1) {
                sceNetAdhocAuth_lib_2E6AA271();
                ctx->pending_error = 0x80410b84;
            }
            adhocctlNotifyGameModeHandlers(5, 0);
            res = _sce_pspnet_adhocctl_handle_ifevent(ctx);
            if (res < 0)
                ret = res;
            else if (ctx->state != 5)
                ret = 0x80410b0f;
        }
    }
    if (sceKernelPollEventFlag(ctx->evfid, 0x42, 1, NULL) == 0)
        ret = 0;
    return ret;
}

/* 0x3f34 */
