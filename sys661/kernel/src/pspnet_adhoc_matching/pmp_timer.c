#include "pmpvar.h"

int __attribute__((noinline))
matchingSetCtrlRetryAlarm(MatchingCtx *ctx, MatchingPeer *peer)
{
    int intr = sceKernelCpuSuspendIntr();
    if (peer->ctrl_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->ctrl_alarm_id);
        peer->ctrl_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    peer->ctrl_alarm_id = sceKernelSetAlarm(ctx->msg_delay, matchingCtrlRetryAlarmCb, peer);
    return peer->ctrl_alarm_id;
}

/* 0x003580 */
int __attribute__((noinline))
matchingSetDataRetryAlarm(MatchingCtx *ctx, MatchingPeer *peer)
{
    int intr = sceKernelCpuSuspendIntr();
    if (peer->data_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->data_alarm_id);
        peer->data_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    peer->data_alarm_id = sceKernelSetAlarm(ctx->msg_delay, matchingDataRetryAlarmCb, peer);
    return peer->data_alarm_id;
}

/* 0x0035f8 */
int __attribute__((noinline))
matchingSetKeepAliveAlarm(MatchingCtx *ctx, MatchingPeer *peer)
{
    u32 delay = (ctx->mode == MATCHING_MODE_CLIENT) ? peer->peer_ping_delay : ctx->ping_delay;
    int intr = sceKernelCpuSuspendIntr();
    if (peer->ctrl_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->ctrl_alarm_id);
        peer->ctrl_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    peer->ctrl_alarm_id = sceKernelSetAlarm(delay, matchingKeepAliveAlarmCb, peer);
    return peer->ctrl_alarm_id;
}

/* 0x003684 */
int __attribute__((noinline))
matchingSetHelloAlarm(MatchingCtx *ctx, u32 delay)
{
    int intr = sceKernelCpuSuspendIntr();
    if (ctx->hello_alarm_id >= 0) {
        sceKernelCancelAlarm(ctx->hello_alarm_id);
        ctx->hello_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    ctx->hello_alarm_id = sceKernelSetAlarm(delay, matchingHelloAlarmCb, ctx);
    return ctx->hello_alarm_id;
}

/* 0x0036fc */
int __attribute__((noinline))
matchingCancelPeerAlarms(MatchingPeer *peer)
{
    int intr = sceKernelCpuSuspendIntr();
    if (peer->ctrl_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->ctrl_alarm_id);
        peer->ctrl_alarm_id = -1;
    }
    if (peer->data_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->data_alarm_id);
        peer->data_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x00376c */
int __attribute__((noinline))
matchingCancelDataAlarm(MatchingPeer *peer)
{
    int intr = sceKernelCpuSuspendIntr();
    if (peer->data_alarm_id >= 0) {
        sceKernelCancelAlarm(peer->data_alarm_id);
        peer->data_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x0037c0 */
int __attribute__((noinline))
matchingCancelHelloAlarm(MatchingCtx *ctx)
{
    int intr = sceKernelCpuSuspendIntr();
    if (ctx->hello_alarm_id >= 0) {
        sceKernelCancelAlarm(ctx->hello_alarm_id);
        ctx->hello_alarm_id = -1;
    }
    sceKernelCpuResumeIntr(intr);
    return 0;
}

/* 0x003814 */
u32 matchingCtrlRetryAlarmCb(void *common)
{
    MatchingPeer *peer = (MatchingPeer *)common;
    if ((peer->ctrl_alarm_msg.pending_flag & 1u) == 0) {
        peer->ctrl_alarm_msg.pending_flag |= 1u;
        peer->ctrl_alarm_msg.msg_type = MBX_MSG_CTRL_RETRY;
        peer->ctrl_alarm_msg.peer = peer;
        sceKernelSendMbx(peer->event_mbx, &peer->ctrl_alarm_msg);
    }
    peer->ctrl_alarm_id = -1;
    return 0;
}

/* 0x003874 */
u32 matchingDataRetryAlarmCb(void *common)
{
    MatchingPeer *peer = (MatchingPeer *)common;
    if ((peer->data_alarm_msg.pending_flag & 1u) == 0) {
        peer->data_alarm_msg.pending_flag |= 1u;
        peer->data_alarm_msg.msg_type = MBX_MSG_DATA_RETRY;
        peer->data_alarm_msg.peer = peer;
        sceKernelSendMbx(peer->event_mbx, &peer->data_alarm_msg);
    }
    peer->data_alarm_id = -1;
    return 0;
}

/* 0x0038d4 */
u32 matchingKeepAliveAlarmCb(void *common)
{
    MatchingPeer *peer = (MatchingPeer *)common;
    if ((peer->ctrl_alarm_msg.pending_flag & 1u) == 0) {
        peer->ctrl_alarm_msg.pending_flag |= 1u;
        peer->ctrl_alarm_msg.msg_type = MBX_MSG_KEEPALIVE;
        peer->ctrl_alarm_msg.peer = peer;
        sceKernelSendMbx(peer->event_mbx, &peer->ctrl_alarm_msg);
    }
    peer->ctrl_alarm_id = -1;
    return 0;
}

/* 0x003934 */
u32 matchingHelloAlarmCb(void *common)
{
    MatchingCtx *ctx = (MatchingCtx *)common;
    if ((ctx->hello_msg.pending_flag & 1u) == 0) {
        ctx->hello_msg.pending_flag |= 1u;
        ctx->hello_msg.msg_type = MBX_MSG_HELLO;
        ctx->hello_msg.peer = NULL;
        sceKernelSendMbx(ctx->event_mbx, &ctx->hello_msg);
    }
    ctx->hello_alarm_id = -1;
    return 0;
}

/* 0x003994 */
