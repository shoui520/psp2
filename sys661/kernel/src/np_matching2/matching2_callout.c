#include "np_matching2_internal.h"

typedef struct NpCalloutMessage {
    void *next;
    u8 priority;
    u8 reserved[3];
    void (*fn)(void *arg);
    void *arg;
} NpCalloutMessage;

static int npCalloutThreadEntry(SceSize args, void *argp)
{
    (void)args;
    (void)argp;
    while (g_np_matching2.initialized != 0) {
        void *msg_ptr = NULL;
        if (sceKernelPollMbx(g_np_matching2.callout_mbx, &msg_ptr) == 0 && msg_ptr != NULL) {
            NpCalloutMessage *msg = (NpCalloutMessage *)msg_ptr;
            if (msg->fn != NULL)
                msg->fn(msg->arg);
            npMatching2Free(msg);
        } else {
            sceKernelDelayThread(10000);
        }
    }
    return 0;
}

int npMatching2CalloutInit(int thread_prio, u32 stack_size)
{
    g_np_matching2.callout_mbx = sceKernelCreateMbx("SceNpCallout", 0, NULL);
    if (g_np_matching2.callout_mbx < 0)
        return (int)SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;

    int eff_prio = (thread_prio > 0) ? thread_prio : 40;
    g_np_matching2.callout_thread = sceKernelCreateThread(
        "SceNpCallout", (void *)npCalloutThreadEntry, eff_prio, stack_size, 0, NULL);
    if (g_np_matching2.callout_thread < 0) {
        sceKernelDeleteMbx(g_np_matching2.callout_mbx);
        g_np_matching2.callout_mbx = 0;
        return (int)SCE_NP_MATCHING2_ERROR_OUT_OF_MEMORY;
    }

    sceKernelStartThread(g_np_matching2.callout_thread, 0, NULL);
    return 0;
}

int npMatching2CalloutTerm(void)
{
    if (g_np_matching2.callout_thread > 0) {
        u32 timeout = 500000U;
        sceKernelWaitThreadEnd(g_np_matching2.callout_thread, &timeout);
        sceKernelDeleteThread(g_np_matching2.callout_thread);
        g_np_matching2.callout_thread = 0;
    }
    if (g_np_matching2.callout_mbx > 0) {
        sceKernelDeleteMbx(g_np_matching2.callout_mbx);
        g_np_matching2.callout_mbx = 0;
    }
    return 0;
}

int npMatching2TlsConnect(const char *host, u16 port, int *out_sock)
{
    if (host == NULL || out_sock == NULL)
        return (int)SCE_NP_MATCHING2_ERROR_INVALID_ARGUMENT;

    u8 res_buf[1024];
    int rid = 0;
    if (sceNetResolverCreate(&rid, res_buf, sizeof(res_buf)) < 0)
        return -1;

    u32 ip_addr = 0;
    int ret = sceNetResolverStartNtoAAsync(rid, host, &ip_addr, 2000000U, 2);
    if (ret == 0) {
        int poll_ret = 0;
        while (sceNetResolverPollAsync(rid, &poll_ret) == 1)
            sceKernelDelayThread(10000);
        ret = poll_ret;
    }
    sceNetResolverDelete(rid);
    if (ret < 0)
        return ret;

    int sock = sceNetInetSocket(2, 1, 0);
    if (sock < 0)
        return sceNetInetGetPspError();

    u8 sockaddr[16];
    sceKernelMemset(sockaddr, 0, sizeof(sockaddr));
    sockaddr[0] = 16;
    sockaddr[1] = 2;
    sockaddr[2] = (u8)(port >> 8);
    sockaddr[3] = (u8)port;
    sceKernelMemcpy(sockaddr + 4, &ip_addr, 4);

    if (sceNetInetConnect(sock, sockaddr, sizeof(sockaddr)) < 0) {
        int err = sceNetInetGetPspError();
        sceNetInetClose(sock);
        return err;
    }

    *out_sock = sock;
    return 0;
}
