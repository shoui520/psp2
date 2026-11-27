#include "pspnet_resolver_net.h"

int g_initialized;
int g_next_rid;
ResolverNode *g_resolver_list;

int _sce_pspnet_resolver_create_ctx(void *buf, SceSize buflen, int *rid)
{
    ResolverNode *node;
    int intr, id, start_id;

    node = sceNetIfhandle_lib_15CFE3C0(sizeof(ResolverNode));
    if (node == 0)
        return 0x80410407;

    sceKernelMemset(node, 0, sizeof(ResolverNode));
    intr = sceKernelCpuSuspendIntr();
    start_id = g_next_rid;
    id = (start_id == 0x20) ? 1 : (start_id + 1);

    while (id != start_id) {
        ResolverNode *cur = g_resolver_list;
        while (cur != 0) {
            if (cur->rid == id)
                break;
            cur = cur->next;
        }
        if (cur == 0) {
            g_next_rid = id;
            node->buflen = buflen;
            node->next = g_resolver_list;
            node->rid = id;
            node->buf = buf;
            g_resolver_list = node;
            *rid = id;
            sceKernelCpuResumeIntr(intr);
            return 0;
        }
        id = (id == 0x20) ? 1 : (id + 1);
    }

    sceKernelCpuResumeIntr(intr);
    sceNetIfhandle_lib_76BAD213(node);
    return 0x80410406;
}

int _sce_pspnet_resolver_delete_ctx(int rid)
{
    int intr;
    ResolverNode *node, *prev = 0;

    intr = sceKernelCpuSuspendIntr();
    node = g_resolver_list;
    while (node != 0) {
        if (node->rid == rid)
            break;
        prev = node;
        node = node->next;
    }
    if (node == 0) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410408;
    }
    if ((node->flags & 6) == 2) {
        sceKernelCpuResumeIntr(intr);
        return 0x80410409;
    }
    if (prev != 0)
        prev->next = node->next;
    else
        g_resolver_list = g_resolver_list->next;

    if (node->async != 0) {
        if (node->async->hostname != 0)
            sceNetIfhandle_lib_76BAD213(node->async->hostname);
        sceNetIfhandle_lib_76BAD213(node->async);
    }
    sceNetIfhandle_lib_76BAD213(node);
    sceKernelCpuResumeIntr(intr);
    return 0;
}

int _sce_pspnet_resolver_init(void)
{
    int ret = _sce_pspnet_resolver_net_init();
    if (ret < 0) {
        _sce_pspnet_resolver_net_term();
        return ret;
    }
    g_resolver_list = 0;
    g_next_rid = 0;
    return 0;
}

int _sce_pspnet_resolver_term(void)
{
    _sce_pspnet_resolver_net_term();
    return 0;
}

ResolverNode *_sce_pspnet_resolver_get_resolver_ctx(int rid, ResolverNode **prev_out)
{
    ResolverNode *cur = g_resolver_list;
    ResolverNode *prev = 0;

    while (cur != 0) {
        if (cur->rid == rid)
            break;
        prev = cur;
        cur = cur->next;
    }
    if (prev_out != 0)
        *prev_out = prev;
    return cur;
}

int _sce_pspnet_resolver_strncasecmp(const char *lhs, const char *rhs, SceSize n)
{
    SceSize i;
    int leftCh, rightCh;

    if (n == 0)
        return 0;

    for (i = 0; i < n; i++) {
        leftCh = *lhs;
        if (sceNet_lib_750F705D(leftCh) & 1)
            leftCh = (s8)(leftCh + 0x20);
        rightCh = *rhs++;
        if (sceNet_lib_750F705D(rightCh) & 1)
            rightCh = (s8)(rightCh + 0x20);
        if (leftCh != rightCh) {
            leftCh = *lhs;
            if (sceNet_lib_750F705D(leftCh) & 1)
                leftCh = (s8)(leftCh + 0x20);
            rightCh = rhs[-1];
            if (sceNet_lib_750F705D(rightCh) & 1)
                rightCh = (s8)(rightCh + 0x20);
            return (leftCh & 0xff) - (rightCh & 0xff);
        }
        if (*(const u8 *)lhs == 0)
            return 0;
        lhs++;
    }
    return 0;
}

