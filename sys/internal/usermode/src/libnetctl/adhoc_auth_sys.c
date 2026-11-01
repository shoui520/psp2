/*
 * SceNetCtl ad-hoc authentication, the system side: the key semaphore, the callout thread that
 * runs the timers, the authentication thread and its mailbox (a message pipe).
 */

#include "adhoc_auth.h"

SceUID g_adhocAuthKeySema = -1;
SceUID g_adhocAuthCalloutThread = 0;
SceUID g_adhocAuthCalloutMutex = 0;
SceUID g_adhocAuthCalloutCond = 0;
int g_adhocAuthCalloutInitialized = 0;
int g_adhocAuthCalloutStop = 0;
SceUID g_adhocAuthMbx = -1;

NetCtlAdhocAuthCallout *g_adhocAuthCalloutList;

__attribute__((noinline))
int netCtlAdhocAuthKeyInit(void)
{
	int ret;

	if (netCtlAdhocAuthGcInit() < 0)
		return NETCTL_ERROR_ADHOC_AUTH_FAILED;
	ret = sceKernelCreateSema("SceNetAdhocAuth", 0, 1, 1, NULL);
	if (ret < 0)
		return ret;
	g_adhocAuthKeySema = ret;
	return 0;
}

__attribute__((noinline))
int netCtlAdhocAuthKeyTerm(void)
{
	if (g_adhocAuthKeySema >= 0)
		sceKernelDeleteSema(g_adhocAuthKeySema);
	g_adhocAuthKeySema = -1;
	return netCtlAdhocAuthGcTerm();
}

__attribute__((noinline))
int netCtlAdhocAuthCalloutInit(SceSize stackSize, int priority)
{
	int ret;

	if (g_adhocAuthCalloutInitialized > 0)
		return NETCTL_ERROR_ADHOC_AUTH_CALLOUT_RUNNING;
	g_adhocAuthCalloutStop = 0;
	g_adhocAuthCalloutList = NULL;
	ret = sceKernelCreateMutex("SceNetAdhocAuthCallout", SCE_KERNEL_MUTEX_ATTR_RECURSIVE, 0, NULL);
	if (ret < 0)
		return ret;
	g_adhocAuthCalloutMutex = ret;
	ret = sceKernelCreateCond("SceNetAdhocAuthCallout", 0, g_adhocAuthCalloutMutex, NULL);
	if (ret >= 0) {
		g_adhocAuthCalloutCond = ret;
		ret = sceKernelCreateThread("SceNetAdhocAuthCallout", netCtlAdhocAuthCalloutThread,
					    priority, stackSize, 0, 0, NULL);
		if (ret >= 0) {
			g_adhocAuthCalloutThread = ret;
			ret = sceKernelStartThread(g_adhocAuthCalloutThread, 0, NULL);
			if (ret >= 0) {
				g_adhocAuthCalloutInitialized = 1;
				return 0;
			}
			sceKernelDeleteThread(g_adhocAuthCalloutThread);
			g_adhocAuthCalloutThread = -1;
		}
		sceKernelDeleteCond(g_adhocAuthCalloutCond);
		g_adhocAuthCalloutCond = -1;
	}
	sceKernelDeleteMutex(g_adhocAuthCalloutMutex);
	g_adhocAuthCalloutMutex = -1;
	return ret;
}

__attribute__((noinline))
void netCtlAdhocAuthCalloutTerm(void)
{
	if (g_adhocAuthCalloutInitialized == 0)
		return;
	g_adhocAuthCalloutStop = 1;
	sceKernelSignalCond(g_adhocAuthCalloutCond);
	sceKernelWaitThreadEnd(g_adhocAuthCalloutThread, NULL, NULL);
	sceKernelDeleteThread(g_adhocAuthCalloutThread);
	sceKernelDeleteCond(g_adhocAuthCalloutCond);
	g_adhocAuthCalloutCond = -1;
	sceKernelDeleteMutex(g_adhocAuthCalloutMutex);
	g_adhocAuthCalloutMutex = -1;
	g_adhocAuthCalloutInitialized = 0;
}

__attribute__((noinline))
int netCtlAdhocAuthCalloutAdd(NetCtlAdhocAuthCallout *callout, SceUInt64 delay,
			      void (*func)(void *arg), void *arg)
{
	NetCtlAdhocAuthCallout *c, *prev;
	SceUInt64 now;

	if (g_adhocAuthCalloutInitialized == 0)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	sceClibMemset(callout, 0, sizeof(*callout));
	now = sceKernelGetProcessTimeWide();
	callout->func = func;
	callout->arg = arg;
	callout->time = now + delay;
	sceKernelLockMutex(g_adhocAuthCalloutMutex, 1, NULL);
	for (c = g_adhocAuthCalloutList; c != NULL; c = c->next) {
		if (callout == c) {
			sceKernelUnlockMutex(g_adhocAuthCalloutMutex, 1);
			return NETCTL_ERROR_ADHOC_AUTH_CALLOUT_QUEUED;
		}
	}
	prev = NULL;
	for (c = g_adhocAuthCalloutList; c != NULL; c = c->next) {
		if (callout->time <= c->time)
			break;
		prev = c;
	}
	callout->next = c;
	if (prev != NULL)
		prev->next = callout;
	else
		g_adhocAuthCalloutList = callout;
	sceKernelSignalCond(g_adhocAuthCalloutCond);
	sceKernelUnlockMutex(g_adhocAuthCalloutMutex, 1);
	return 0;
}

__attribute__((noinline))
int netCtlAdhocAuthCalloutRemove(NetCtlAdhocAuthCallout *callout, int *removed)
{
	NetCtlAdhocAuthCallout *c, *prev;
	int found;

	if (g_adhocAuthCalloutInitialized == 0)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	sceKernelLockMutex(g_adhocAuthCalloutMutex, 1, NULL);
	prev = NULL;
	for (c = g_adhocAuthCalloutList; c != NULL; c = c->next) {
		if (callout == c)
			break;
		prev = c;
	}
	if (c == NULL) {
		found = 0;
	} else {
		if (prev != NULL)
			prev->next = callout->next;
		else
			g_adhocAuthCalloutList = callout->next;
		found = 1;
	}
	if (removed != NULL)
		*removed = found;
	sceKernelUnlockMutex(g_adhocAuthCalloutMutex, 1);
	return 0;
}

__attribute__((noinline))
int netCtlAdhocAuthCalloutThread(SceSize argSize, void *argBlock)
{
	NetCtlAdhocAuthCallout *c;
	SceUInt64 now;
	SceUInt32 wait, timeout;

	sceKernelLockMutex(g_adhocAuthCalloutMutex, 1, NULL);
	do {
		for (;;) {
			now = sceKernelGetProcessTimeWide();
			c = g_adhocAuthCalloutList;
			if (c == NULL || now < c->time)
				break;
			g_adhocAuthCalloutList = c->next;
			sceKernelUnlockMutex(g_adhocAuthCalloutMutex, 1);
			c->func(c->arg);
			sceKernelLockMutex(g_adhocAuthCalloutMutex, 1, NULL);
		}
		wait = 0;
		if (g_adhocAuthCalloutList != NULL)
			wait = g_adhocAuthCalloutList->time - now;
		timeout = wait;
		if (g_adhocAuthCalloutStop > 0)
			break;
		if (wait != 0)
			sceKernelWaitCond(g_adhocAuthCalloutCond, &timeout);
		else
			sceKernelWaitCond(g_adhocAuthCalloutCond, NULL);
	} while (g_adhocAuthCalloutStop <= 0);
	sceKernelUnlockMutex(g_adhocAuthCalloutMutex, 1);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

__attribute__((noinline))
int netCtlAdhocAuthThread(SceSize argSize, void *argBlock)
{
	NetCtlAdhocAuth *auth;
	NetCtlAdhocAuthPeer *peer;
	NetCtlAdhocAuthMessage message;
	int received;

	SceNetInternal_0DFC23D6(sceKernelGetProcessId(), 1, 0);
	auth = *(NetCtlAdhocAuth **)argBlock;
	while (auth->stop == 0) {
		if (netCtlAdhocAuthReceive(auth, &message, &received, NULL) < 0)
			break;
		if (received == 0)
			continue;
		switch (message.type) {
		case NETCTL_ADHOC_AUTH_MESSAGE_RESEND:
		case NETCTL_ADHOC_AUTH_MESSAGE_KEEPALIVE:
		case NETCTL_ADHOC_AUTH_MESSAGE_TIMEOUT:
			peer = netCtlAdhocAuthPeerFind(auth, message.from.sin_addr);
			if (peer == NULL)
				continue;
			break;
		default:
			peer = NULL;
			break;
		}
		switch (message.type) {
		case NETCTL_ADHOC_AUTH_MESSAGE_RESEND:
			netCtlAdhocAuthResend(auth, peer);
			break;
		case NETCTL_ADHOC_AUTH_MESSAGE_BEACON:
			auth->flags &= ~NETCTL_ADHOC_AUTH_FLAG_BEACON_QUEUED;
			netCtlAdhocAuthBeacon(auth);
			break;
		case NETCTL_ADHOC_AUTH_MESSAGE_KEEPALIVE:
			netCtlAdhocAuthKeepalive(auth, peer);
			break;
		case NETCTL_ADHOC_AUTH_MESSAGE_TIMEOUT:
			netCtlAdhocAuthTimeout(auth, peer);
			break;
		case NETCTL_ADHOC_AUTH_MESSAGE_STOP:
			goto end;
		}
	}
end:
	netCtlAdhocAuthShutdown(auth);
	SceNetInternal_0DFC23D6(sceKernelGetProcessId(), 0, 0);
	netCtlAdhocAuthPeerPoolTerm();
	netCtlAdhocAuthDeleteCommId();
	sceNetSocketClose(auth->recvSocket);
	sceNetSocketClose(auth->sendSocket);
	netCtlAdhocAuthMbxCancel();
	return 0;
}

__attribute__((noinline))
int netCtlAdhocAuthMbxCreate(const char *name)
{
	int ret;

	ret = sceKernelCreateMsgPipe(name, 64, 0x10c, 0x200, NULL);
	if (ret >= 0)
		g_adhocAuthMbx = ret;
	return ret;
}

__attribute__((noinline))
int netCtlAdhocAuthMbxDelete(void)
{
	int ret = 0;

	if (g_adhocAuthMbx > 0) {
		ret = sceKernelDeleteMsgPipe(g_adhocAuthMbx);
		g_adhocAuthMbx = -1;
	}
	return ret;
}

__attribute__((noinline))
int netCtlAdhocAuthMbxSend(const NetCtlAdhocAuthMessage *message, SceUInt32 waitMode)
{
	return sceKernelSendMsgPipe(g_adhocAuthMbx, message, sizeof(*message), waitMode | 1, NULL,
				    NULL);
}

__attribute__((noinline))
int netCtlAdhocAuthMbxReceive(NetCtlAdhocAuthMessage *message)
{
	NetCtlAdhocAuthMessage received;
	int ret;

	ret = sceKernelTryReceiveMsgPipe(g_adhocAuthMbx, &received, sizeof(received), 1, NULL);
	if (ret >= 0) {
		if (message != NULL)
			sceClibMemcpy_safe(message, &received, sizeof(received));
		ret = 0;
	}
	return ret;
}

__attribute__((noinline))
int netCtlAdhocAuthMbxCancel(void)
{
	return sceKernelCancelMsgPipe(g_adhocAuthMbx, NULL, NULL);
}
