/*
 * adhoc_auth_sys.c: authenticated peer key hash table, callout timer thread,
 * authentication worker thread, and mailbox for SceLibPspnetAdhoc.
 */

#include "pspnet_adhoc_internal.h"

SceUID g_adhocAuthKeySema = -1;
SceUID g_adhocAuthCalloutThread = 0;
SceUID g_adhocAuthCalloutMutex = 0;
SceUID g_adhocAuthCalloutCond = 0;
int g_adhocAuthCalloutInitialized = 0;
int g_adhocAuthCalloutStop = 0;
SceUID g_adhocAuthMbx = -1;

AdhocAuthKeyEntry g_adhocAuthKeyTable[PSPNET_ADHOC_AUTH_KEY_BUCKETS];
AdhocAuthCallout *g_adhocAuthCalloutList;

__attribute__((noinline))
int adhocAuthKeyInit(void)
{
	int i, ret;

	if (adhocAuthGcInit() < 0)
		return PSPNET_ADHOC_AUTH_ERROR_FAILED;
	for (i = 0; i < PSPNET_ADHOC_AUTH_KEY_BUCKETS; i++) {
		g_adhocAuthKeyTable[i].next = &g_adhocAuthKeyTable[i];
		g_adhocAuthKeyTable[i].prev = &g_adhocAuthKeyTable[i];
	}
	ret = sceKernelCreateSema("SceNetAdhocAuth", 0, 1, 1, NULL);
	if (ret < 0)
		return ret;
	g_adhocAuthKeySema = ret;
	return 0;
}

__attribute__((noinline))
int adhocAuthKeyTerm(void)
{
	if (g_adhocAuthKeySema >= 0)
		sceKernelDeleteSema(g_adhocAuthKeySema);
	g_adhocAuthKeySema = -1;
	return adhocAuthGcTerm();
}

AdhocAuthKeyEntry *adhocAuthKeyFind(const SceUChar8 *addr)
{
	AdhocAuthKeyEntry *head;
	AdhocAuthKeyEntry *cur;
	int found = 0;

	head = &g_adhocAuthKeyTable[addr[5] & (PSPNET_ADHOC_AUTH_KEY_BUCKETS - 1)];
	for (cur = head->next; cur != head; cur = cur->next) {
		if (sceClibMemcmp(&cur->addr, addr, sizeof(cur->addr)) == 0) {
			found = 1;
			break;
		}
	}
	if (found)
		return cur;
	return NULL;
}

__attribute__((noinline))
int adhocAuthKeyAdd(AdhocAuthKeyEntry *entry)
{
	AdhocAuthKeyEntry *head;

	if (adhocAuthKeyFind(entry->addr.data) != NULL)
		return PSPNET_ADHOC_AUTH_ERROR_ALREADY_EXISTS;
	head = &g_adhocAuthKeyTable[entry->addr.data[5] & (PSPNET_ADHOC_AUTH_KEY_BUCKETS - 1)];
	entry->next = head;
	entry->prev = head->prev;
	head->prev = entry;
	entry->prev->next = entry;
	return 0;
}

__attribute__((noinline))
int adhocAuthKeyRemove(AdhocAuthKeyEntry *entry)
{
	if (adhocAuthKeyFind(entry->addr.data) == NULL)
		return PSPNET_ADHOC_AUTH_ERROR_NOT_FOUND;
	entry->next->prev = entry->prev;
	entry->prev->next = entry->next;
	entry->next = entry;
	entry->prev = entry;
	return 0;
}

__attribute__((noinline))
int adhocAuthCalloutInit(SceSize stackSize, int priority)
{
	int ret;

	if (g_adhocAuthCalloutInitialized > 0)
		return PSPNET_ADHOC_AUTH_ERROR_CALLOUT_RUNNING;
	g_adhocAuthCalloutStop = 0;
	g_adhocAuthCalloutList = NULL;
	ret = sceKernelCreateMutex("SceNetAdhocAuthCallout", SCE_KERNEL_MUTEX_ATTR_RECURSIVE,
				   0, NULL);
	if (ret < 0)
		return ret;
	g_adhocAuthCalloutMutex = ret;
	ret = sceKernelCreateCond("SceNetAdhocAuthCallout", 0, g_adhocAuthCalloutMutex, NULL);
	if (ret >= 0) {
		g_adhocAuthCalloutCond = ret;
		ret = sceKernelCreateThread("SceNetAdhocAuthCallout", adhocAuthCalloutThread,
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
void adhocAuthCalloutTerm(void)
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
int adhocAuthCalloutAdd(AdhocAuthCallout *callout, SceUInt64 delay,
			void (*func)(void *arg), void *arg)
{
	AdhocAuthCallout *c, *prev;
	SceUInt64 now;

	if (g_adhocAuthCalloutInitialized == 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
	sceClibMemset(callout, 0, sizeof(*callout));
	now = sceKernelGetProcessTimeWide();
	callout->func = func;
	callout->arg = arg;
	callout->time = now + delay;
	sceKernelLockMutex(g_adhocAuthCalloutMutex, 1, NULL);
	for (c = g_adhocAuthCalloutList; c != NULL; c = c->next) {
		if (callout == c) {
			sceKernelUnlockMutex(g_adhocAuthCalloutMutex, 1);
			return PSPNET_ADHOC_AUTH_ERROR_CALLOUT_QUEUED;
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
int adhocAuthCalloutRemove(AdhocAuthCallout *callout, int *removed)
{
	AdhocAuthCallout *c, *prev;
	int found;

	if (g_adhocAuthCalloutInitialized == 0)
		return SCE_ERROR_NET_ADHOCCTL_NOT_INITIALIZED;
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
int adhocAuthCalloutThread(SceSize argSize, void *argBlock)
{
	AdhocAuthCallout *c;
	SceUInt64 now;
	SceUInt32 wait, timeout;

	(void)argSize;
	(void)argBlock;
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
		if (c != NULL)
			wait = (SceUInt32)(c->time - now);
		timeout = wait;
		if (g_adhocAuthCalloutStop > 0)
			break;
		if (wait == 0)
			sceKernelWaitCond(g_adhocAuthCalloutCond, NULL);
		else
			sceKernelWaitCond(g_adhocAuthCalloutCond, &timeout);
	} while (g_adhocAuthCalloutStop <= 0);
	sceKernelUnlockMutex(g_adhocAuthCalloutMutex, 1);
	SceThreadmgrCoredumpTime_sceKernelExitThread(0);
	return 0;
}

__attribute__((noinline))
int adhocAuthThread(SceSize argSize, void *argBlock)
{
	AdhocAuth *auth;
	AdhocAuthPeer *peer;
	AdhocAuthMessage message;
	int received;
	int stopRequested = 0;

	(void)argSize;
	SceNetInternal_0DFC23D6(sceKernelGetProcessId(), 1, 0);
	auth = *(AdhocAuth **)argBlock;
	while (!stopRequested && auth->stop == 0) {
		if (adhocAuthReceive(auth, &message, &received, NULL) < 0)
			break;
		if (received == 0)
			continue;
		switch (message.type) {
		case PSPNET_ADHOC_AUTH_MSG_RESEND:
		case PSPNET_ADHOC_AUTH_MSG_KEEPALIVE:
		case PSPNET_ADHOC_AUTH_MSG_TIMEOUT:
			peer = adhocAuthPeerFind(auth, message.addr.data);
			if (peer == NULL)
				continue;
			break;
		default:
			peer = NULL;
			break;
		}
		switch (message.type) {
		case PSPNET_ADHOC_AUTH_MSG_RESEND:
			adhocAuthResend(auth, peer);
			break;
		case PSPNET_ADHOC_AUTH_MSG_BEACON:
			auth->flags &= ~PSPNET_ADHOC_AUTH_FLAG_BEACON_QUEUED;
			adhocAuthBeacon(auth);
			break;
		case PSPNET_ADHOC_AUTH_MSG_KEEPALIVE:
			adhocAuthKeepalive(auth, peer);
			break;
		case PSPNET_ADHOC_AUTH_MSG_TIMEOUT:
			adhocAuthTimeout(auth, peer);
			break;
		case PSPNET_ADHOC_AUTH_MSG_STOP:
			stopRequested = 1;
			break;
		}
	}
	adhocAuthShutdown(auth);
	SceNetInternal_0DFC23D6(sceKernelGetProcessId(), 0, 0);
	sceNetSocketClose(auth->recvSocket);
	sceNetSocketClose(auth->sendSocket);
	adhocAuthPeerPoolTerm();
	adhocAuthMbxCancel();
	return 0;
}

__attribute__((noinline))
int adhocAuthMbxCreate(const char *name)
{
	int ret;

	ret = sceKernelCreateMsgPipe(name, 64, 0x10c, 0x180, NULL);
	if (ret >= 0)
		g_adhocAuthMbx = ret;
	return ret;
}

__attribute__((noinline))
int adhocAuthMbxDelete(void)
{
	int ret = 0;

	if (g_adhocAuthMbx > 0) {
		ret = sceKernelDeleteMsgPipe(g_adhocAuthMbx);
		g_adhocAuthMbx = -1;
	}
	return ret;
}

__attribute__((noinline))
int adhocAuthMbxSend(const AdhocAuthMessage *message, SceUInt32 waitMode)
{
	return sceKernelSendMsgPipe(g_adhocAuthMbx, message, sizeof(*message),
				    waitMode | 1, NULL, NULL);
}

__attribute__((noinline))
int adhocAuthMbxReceive(AdhocAuthMessage *message)
{
	AdhocAuthMessage received;
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
int adhocAuthMbxCancel(void)
{
	return sceKernelCancelMsgPipe(g_adhocAuthMbx, NULL, NULL);
}
