#include "pspnet_resolver_net.h"

static int resolverPollAsync(int rid, int *out, int is_nonblock);

int sceNetResolverStartNtoA(int rid, const char *hostname, u32 *addr, unsigned int timeout, int retry)
{
    int intr;
    ResolverNode *node;

    if (sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    if ((hostname == 0) | (addr == 0))
        return 0x80410403;
    if (sceNet_lib_D8722983(hostname) == 0 || sceNet_lib_D8722983(hostname) >= 0x100)
        return 0x80410418;

    intr = sceKernelCpuSuspendIntr();
    node = _sce_pspnet_resolver_get_resolver_ctx(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if (node->flags & 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410409;
    }
    node->flags = (node->flags | 2) & ~4U;
    if (node->async != 0) {
        if (node->async->hostname != 0)
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
        sceNetIfhandle_lib_76BAD213(node->async);
        node->async = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return _sce_pspnet_resolver_do_resolv(node, 0, hostname, 0, addr, timeout, retry);
}

int sceNetResolverStartNtoAAsync(int rid, const char *hostname, u32 *addr, unsigned int timeout, int retry)
{
    int intr, ret = 0;
    ResolverNode *node;

    if (sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    if ((hostname == 0) | (addr == 0))
        return 0x80410403;
    if (sceNet_lib_D8722983(hostname) == 0 || sceNet_lib_D8722983(hostname) >= 0x100)
        return 0x80410418;

    intr = sceKernelCpuSuspendIntr();
    do {
        node = _sce_pspnet_resolver_get_resolver_ctx(rid, 0);
        if (node == 0) {
            sceKernelCpuResumeIntr(intr);
            ret = 0x80410408;
            break;
        }
        if (node->flags & 2) {
            sceKernelCpuResumeIntr(intr);
            ret = 0x80410409;
            break;
        }
        node->flags |= 6;
        if (node->async != 0) {
            if (node->async->hostname != 0) {
                sceNetIfhandle_lib_76BAD213(node->async->hostname);
                node->async->hostname = 0;
            }
        } else {
            node->async = sceNetIfhandle_lib_15CFE3C0(sizeof(ResolverAsyncCtx));
            if (node->async == 0) {
                ret = 0x80410407;
                break;
            }
        }
        sceKernelMemset(node->async, 0, sizeof(ResolverAsyncCtx));
        node->async->addr_ptr = addr;
        node->async->timeout = timeout;
        node->async->retry = retry;
        node->async->type = 0;
        node->async->hostname = sceNetIfhandle_lib_15CFE3C0(sceNet_lib_D8722983(hostname) + 1);
        if (node->async->hostname == 0) {
            sceNetIfhandle_lib_76BAD213(node->async);
            node->async = 0;
            ret = 0x80410407;
        } else {
            sceNet_lib_80C9F02A(node->async->hostname, hostname);
        }
    } while (0);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNetResolverStartAtoN(int rid, const u32 *addr, char *hostname, SceSize hostname_len, unsigned int timeout, int retry)
{
    int intr;
    ResolverNode *node;

    if (sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    if ((hostname == 0) | (addr == 0))
        return 0x80410403;

    intr = sceKernelCpuSuspendIntr();
    node = _sce_pspnet_resolver_get_resolver_ctx(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if (node->flags & 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410409;
    }
    node->flags = (node->flags | 2) & ~4U;
    if (node->async != 0) {
        if (node->async->hostname != 0)
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
        sceNetIfhandle_lib_76BAD213(node->async);
        node->async = 0;
    }
    sceKernelCpuResumeIntr(intr);
    return _sce_pspnet_resolver_do_resolv(node, 1, hostname, hostname_len, (void *)addr, timeout, retry);
}

int sceNetResolverStartAtoNAsync(int rid, const u32 *addr, char *hostname, SceSize hostname_len, unsigned int timeout, int retry)
{
    int intr, ret = 0;
    ResolverNode *node;

    if (sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    if ((hostname == 0) | (addr == 0))
        return 0x80410403;

    intr = sceKernelCpuSuspendIntr();
    do {
        node = _sce_pspnet_resolver_get_resolver_ctx(rid, 0);
        if (node == 0) {
            sceKernelCpuResumeIntr(intr);
            ret = 0x80410408;
            break;
        }
        if (node->flags & 2) {
            sceKernelCpuResumeIntr(intr);
            ret = 0x80410409;
            break;
        }
        node->flags |= 6;
        if (node->async != 0) {
            if (node->async->hostname != 0) {
                sceNetIfhandle_lib_76BAD213(node->async->hostname);
                node->async->hostname = 0;
            }
        } else {
            node->async = sceNetIfhandle_lib_15CFE3C0(sizeof(ResolverAsyncCtx));
            if (node->async == 0) {
                ret = 0x80410407;
                break;
            }
        }
        sceKernelMemset(node->async, 0, sizeof(ResolverAsyncCtx));
        node->async->type = 1;
        node->async->timeout = timeout;
        node->async->retry = retry;
        sceKernelMemcpy(&node->async->addr_ptr, addr, 4);
        node->async->out_name = hostname;
        node->async->out_len = hostname_len;
    } while (0);
    sceKernelCpuResumeIntr(intr);
    return ret;
}

int sceNetResolverStop(int rid)
{
    int intr;
    ResolverNode *node;

    if (sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;

    intr = sceKernelCpuSuspendIntr();
    node = _sce_pspnet_resolver_get_resolver_ctx(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if ((node->flags & 2) == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x8041040a;
    }
    node->flags |= 1;
    sceKernelCpuResumeIntr(intr);
    if (node->sock >= 0)
        sceNetInetSocketAbort(node->sock);
    return 0;
}

int sceNetResolverInit(void)
{
    int ret;

    if (sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (g_initialized > 0) {
        _sce_pspnet_resolver_term();
        return 0x80000020;
    }
    ret = _sce_pspnet_resolver_init();
    if (ret < 0) {
        _sce_pspnet_resolver_term();
        return ret;
    }
    g_initialized = 1;
    return 0;
}

int sceNetResolverTerm(void)
{
    if (sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (g_initialized != 0) {
        _sce_pspnet_resolver_term();
        g_initialized = 0;
    }
    return 0;
}

int sceNetResolverCreate(int *rid, void *buf, SceSize buflen)
{
    if (sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if ((rid == 0) | (buf == 0))
        return 0x80410403;
    if (buflen == 0)
        return 0x80410404;
    return _sce_pspnet_resolver_create_ctx(buf, buflen, rid);
}

int sceNetResolverDelete(int rid)
{
    if (sceKernelCheckThreadStack() < 0x13f0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    return _sce_pspnet_resolver_delete_ctx(rid);
}

int sceNetResolverPollAsync(int rid, int *out)
{
    if (sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    return resolverPollAsync(rid, out, 0);
}

int sceNetResolverWaitAsync(int rid, int *out)
{
    if (sceKernelCheckThreadStack() < 0x13e0)
        return 0x80410005;
    if (rid < 0)
        return 0x80410405;
    return resolverPollAsync(rid, out, 1);
}

static int resolverPollAsync(int rid, int *out, int is_nonblock)
{
    int intr, ret;
    ResolverNode *node;

    (void)out;
    intr = sceKernelCpuSuspendIntr();
    node = _sce_pspnet_resolver_get_resolver_ctx(rid, 0);
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if ((node->flags & 4) == 0 || node->async == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410419;
    }
    if ((node->flags & 2) == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x8041041a;
    }
    sceKernelCpuResumeIntr(intr);

    if (is_nonblock == 1) {
        if (node->sock > 0) {
            int val = 0;
            if (sceNetInetSetsockopt(node->sock, 0xffff, 0x1009, &val, 4) < 0)
                return 0x8041040f;
            node->flags &= ~8U;
        } else {
            node->flags &= ~4U;
        }
    }

    ResolverAsyncCtx *async = node->async;
    if (async->type == 0) {
        ret = _sce_pspnet_resolver_do_resolv(node, 0, async->hostname, 0, async->addr_ptr, async->timeout, async->retry);
    } else if (async->type == 1) {
        ret = _sce_pspnet_resolver_do_resolv(node, 1, async->out_name, async->out_len, &async->addr_ptr, async->timeout, async->retry);
    } else {
        ret = 0x80410419;
    }

    if (ret > 0)
        return 0;
    if (ret == (int)0x80410481)
        return 1;
    return ret;
}

