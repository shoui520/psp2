/*
 * SceNetCtl: user side of the network control service.
 *
 * Most exports forward to the NetCtl service through ShellSvc requests
 * (netctl_ipc.c).  Service notifications arrive as ShellSvc events; this
 * library turns them into calls of the callbacks registered by the process.
 */

#include <psp2/rtc.h>
#include "netctl_internal.h"

/* service event ids: one id per notification, odd ids first, then even ids */
int g_netCtlEventIdsOdd[10] = { 1, 3, 5, 7, 9, 11, 13, 15, 17, 19 };
int g_netCtlEventIdsEven[10] = { 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 };

int g_netCtlIsInitialized;
static int g_netCtlMutexCreated;
static void (*g_netCtlAdhocDisconnectHook)(void);

/* callback tables: public inet, two private inet sets, public adhoc, two private adhoc sets */
static NetCtlCallbackSlot g_inetCallbacks[SCE_NET_CTL_CALLBACK_MAX];
static NetCtlCallbackSlot g_inetCallbacks2[SCE_NET_CTL_CALLBACK_MAX];
static NetCtlCallbackSlot g_inetCallbacks3[SCE_NET_CTL_CALLBACK_MAX];
static NetCtlCallbackSlot g_adhocCallbacks[SCE_NET_CTL_CALLBACK_MAX];
static NetCtlCallbackSlot g_adhocCallbacks2[SCE_NET_CTL_CALLBACK_MAX];
static NetCtlCallbackSlot g_adhocCallbacks3[SCE_NET_CTL_CALLBACK_MAX];

SceKernelLwMutexWork g_netCtlMutex;
static SceRtcTick g_netCtlResetTick;
static const NetCtlHandlers *g_netCtlHandlers;

int SceNetCtl_15BEEEF5(void);
int netCtlAdhocAuthInit(void);
int netCtlAdhocAuthTerm(void);
int netCtlAdhocAuthDisconnect(void);
int netCtlAdhocAuthGetPeerList(unsigned int *peerInfoNum, void *peerInfo);
int netCtlScanSetMutex(SceKernelLwMutexWork *mutex);
void netCtlScanClearMutex(void);
static void netCtlEventHandlerOdd(int eventId);
static void netCtlEventHandlerEven(int eventId);

int module_stop(SceSize args, void *argp)
{
	SceNetCtl_15BEEEF5();
	if (g_netCtlAdhocDisconnectHook != NULL)
		g_netCtlAdhocDisconnectHook();
	return 0;
}

/* name unknown: installs the override table */
int SceNetCtl_C61F3E96(const NetCtlHandlers *handlers)
{
	if (handlers == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	g_netCtlHandlers = handlers;
	return 0;
}

static inline __attribute__((always_inline)) void netCtlShutdown(void)
{
	int i;

	netCtlScanClearMutex();
	netCtlIpcTerm(SceShellSvc_B31E7F1C(), 0);
	for (i = 0; i < 10; i++)
		SceShellSvc_7CBF442B(g_netCtlEventIdsEven[i]);
	for (i = 0; i < 10; i++)
		SceShellSvc_7CBF442B(g_netCtlEventIdsOdd[i]);
	if (g_netCtlMutexCreated == 1) {
		sceKernelDeleteLwMutex(&g_netCtlMutex);
		g_netCtlMutexCreated = 0;
	}
	netCtlAdhocAuthTerm();
}

int sceNetCtlInit(void)
{
	int ret;
	unsigned int i;

	if (g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_TERMINATED;

	sceClibMemset(g_inetCallbacks, 0, sizeof(g_inetCallbacks));
	sceClibMemset(g_inetCallbacks2, 0, sizeof(g_inetCallbacks2));
	sceClibMemset(g_inetCallbacks3, 0, sizeof(g_inetCallbacks3));
	sceClibMemset(g_adhocCallbacks, 0, sizeof(g_adhocCallbacks));
	sceClibMemset(g_adhocCallbacks2, 0, sizeof(g_adhocCallbacks2));
	sceClibMemset(g_adhocCallbacks3, 0, sizeof(g_adhocCallbacks3));
	sceClibMemset(&g_netCtlResetTick, 0, sizeof(g_netCtlResetTick));
	g_netCtlMutexCreated = 0;

	ret = netCtlAdhocAuthInit();
	if (ret < 0)
		goto error;
	ret = sceKernelCreateLwMutex(&g_netCtlMutex, "SceNetCtl", 2, 0, NULL);
	if (ret < 0)
		goto error;
	g_netCtlMutexCreated = 1;

	for (i = 0; i < 10; i++) {
		ret = SceShellSvc_D943CE15(g_netCtlEventIdsOdd[i], netCtlEventHandlerOdd, NULL);
		if (ret < 0)
			goto error;
	}
	for (i = 0; i < 10; i++) {
		ret = SceShellSvc_D943CE15(g_netCtlEventIdsEven[i], netCtlEventHandlerEven, NULL);
		if (ret < 0)
			goto error;
	}
	ret = netCtlIpcInit(SceShellSvc_B31E7F1C(), 0);
	if (ret < 0)
		goto error;

	netCtlScanSetMutex(&g_netCtlMutex);
	g_netCtlIsInitialized = 1;
	return 0;

error:
	netCtlShutdown();
	return ret;
}

void sceNetCtlTerm(void)
{
	if (!g_netCtlIsInitialized)
		return;
	netCtlShutdown();
	g_netCtlIsInitialized = 0;
}

int sceNetCtlCheckCallback(void)
{
	int ret = 0;
	int i;

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	for (i = 0; i < 10; i++)
		SceShellSvc_FB03FBC9(g_netCtlEventIdsOdd[i]);
	return ret;
}

/* name unknown: checks the even (adhoc side) events */
int SceNetCtl_F081EFF9(void)
{
	int ret = 0;
	int i;

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	for (i = 0; i < 10; i++)
		SceShellSvc_FB03FBC9(g_netCtlEventIdsEven[i]);
	return ret;
}

/* name unknown: same as SceNetCtl_F081EFF9 */
int SceNetCtl_83184B6B(void)
{
	int i;
	int ret;

	if (!g_netCtlIsInitialized) {
		ret = SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	} else {
		for (i = 0; i < 10; i++)
			SceShellSvc_FB03FBC9(g_netCtlEventIdsEven[i]);
		ret = 0;
	}
	return ret;
}

/* name unknown: event of an inet notification type */
int SceNetCtl_AB1462C5(int type)
{
	int eventId;

	switch (type) {
	case 1:
		eventId = 9;
		break;
	case 2:
		eventId = 1;
		break;
	case 7:
		eventId = 3;
		break;
	case 3:
		eventId = 5;
		break;
	case 4:
		eventId = 7;
		break;
	case 5:
		eventId = 11;
		break;
	case 6:
		eventId = 13;
		break;
	default:
		return 0;
	}
	return SceShellSvc_1FAB9A1E(eventId);
}

/* name unknown: same as SceNetCtl_AB1462C5 */
int SceNetCtl_E03981D6(int type)
{
	int eventId;

	switch (type) {
	case 1:
		eventId = 9;
		break;
	case 2:
		eventId = 1;
		break;
	case 7:
		eventId = 3;
		break;
	case 3:
		eventId = 5;
		break;
	case 4:
		eventId = 7;
		break;
	case 5:
		eventId = 11;
		break;
	case 6:
		eventId = 13;
		break;
	default:
		return 0;
	}
	return SceShellSvc_1FAB9A1E(eventId);
}

/* name unknown: SceNetCtl_AB1462C5 for the even events */
int SceNetCtl_8E379B9C(int type)
{
	int eventId;

	switch (type) {
	case 1:
		eventId = 10;
		break;
	case 2:
		eventId = 2;
		break;
	case 7:
		eventId = 4;
		break;
	case 3:
		eventId = 6;
		break;
	case 4:
		eventId = 8;
		break;
	case 5:
		eventId = 12;
		break;
	case 6:
		eventId = 14;
		break;
	default:
		return 0;
	}
	return SceShellSvc_1FAB9A1E(eventId);
}

/* name unknown: event of an adhoc notification type */
int SceNetCtl_BC0563A0(int type)
{
	int eventId;

	switch (type) {
	case 1:
		eventId = 19;
		break;
	case 2:
		eventId = 15;
		break;
	case 3:
		eventId = 17;
		break;
	default:
		return 0;
	}
	return SceShellSvc_1FAB9A1E(eventId);
}

/* name unknown: SceNetCtl_BC0563A0 for the even events */
int SceNetCtl_E4B3142E(int type)
{
	int eventId;

	switch (type) {
	case 1:
		eventId = 20;
		break;
	case 2:
		eventId = 16;
		break;
	case 3:
		eventId = 18;
		break;
	default:
		return 0;
	}
	return SceShellSvc_1FAB9A1E(eventId);
}

int sceNetCtlInetGetResult(int eventType, int *errorCode)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (errorCode == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpcInetGetResult(SceShellSvc_B31E7F1C(), eventType, errorCode);
}

/* name unknown */
int SceNetCtl_300E8877(int a0, int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (value == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc20003(SceShellSvc_B31E7F1C(), a0, value);
}

/* name unknown: same request as SceNetCtl_300E8877 */
int SceNetCtl_C03B9655(int a0, int *value)
{
	int ret;

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	ret = SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (value != NULL)
		ret = netCtlIpc20003(SceShellSvc_B31E7F1C(), a0, value);
	return ret;
}

int sceNetCtlAdhocGetResult(int eventType, int *errorCode)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (errorCode == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpcAdhocGetResult(SceShellSvc_B31E7F1C(), eventType, errorCode);
}

/* name unknown */
int SceNetCtl_AC00BA34(int a0, int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (value == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc20005(SceShellSvc_B31E7F1C(), a0, value);
}

__attribute__((noinline))
int netCtlRegisterCallback(NetCtlCallbackSlot *slots, unsigned int count,
				  SceNetCtlCallback func, void *arg, int *cid)
{
	int ret;
	unsigned int i;

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (func == NULL || cid == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;

	ret = sceKernelLockLwMutex(&g_netCtlMutex, 1, NULL);
	if (ret >= 0) {
		for (i = 0; i < count; i++) {
			if (slots[i].func == NULL)
				break;
		}
		if (i == count) {
			ret = SCE_NET_CTL_ERROR_CALLBACK_MAX;
		} else {
			slots[i].func = func;
			slots[i].arg = arg;
			slots[i].cid = i;
			*cid = i;
		}
		sceKernelUnlockLwMutex(&g_netCtlMutex, 1);
	}
	return ret;
}

int sceNetCtlInetRegisterCallback(SceNetCtlCallback func, void *arg, int *cid)
{
	return netCtlRegisterCallback(g_inetCallbacks, SCE_NET_CTL_CALLBACK_MAX, func, arg, cid);
}

/* name unknown */
int SceNetCtl_AC326DB7(SceNetCtlCallback func, void *arg, int *cid)
{
	return netCtlRegisterCallback(g_inetCallbacks2, SCE_NET_CTL_CALLBACK_MAX, func, arg, cid);
}

/* name unknown */
int SceNetCtl_5AD6CBE8(SceNetCtlCallback func, void *arg, int *cid)
{
	return netCtlRegisterCallback(g_inetCallbacks3, SCE_NET_CTL_CALLBACK_MAX, func, arg, cid);
}

/* name unknown: same table as SceNetCtl_5AD6CBE8 */
int SceNetCtl_16268321(SceNetCtlCallback func, void *arg, int *cid)
{
	return netCtlRegisterCallback(g_inetCallbacks3, SCE_NET_CTL_CALLBACK_MAX, func, arg, cid);
}

int sceNetCtlAdhocRegisterCallback(SceNetCtlCallback func, void *arg, int *cid)
{
	return netCtlRegisterCallback(g_adhocCallbacks, SCE_NET_CTL_CALLBACK_MAX, func, arg, cid);
}

/* name unknown */
int SceNetCtl_713394C0(SceNetCtlCallback func, void *arg, int *cid)
{
	return netCtlRegisterCallback(g_adhocCallbacks2, SCE_NET_CTL_CALLBACK_MAX, func, arg, cid);
}

/* name unknown */
int SceNetCtl_A25FE027(SceNetCtlCallback func, void *arg, int *cid)
{
	return netCtlRegisterCallback(g_adhocCallbacks3, SCE_NET_CTL_CALLBACK_MAX, func, arg, cid);
}

static inline int netCtlUnregisterCallback(NetCtlCallbackSlot *slots, unsigned int count, int cid)
{
	int ret;

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (cid < 0 || (unsigned int)cid >= count)
		return SCE_NET_CTL_ERROR_INVALID_ID;
	ret = sceKernelLockLwMutex(&g_netCtlMutex, 1, NULL);
	if (ret >= 0) {
		if (slots[cid].func == NULL)
			ret = SCE_NET_CTL_ERROR_ID_NOT_FOUND;
		else
			slots[cid].func = NULL;
		sceKernelUnlockLwMutex(&g_netCtlMutex, 1);
	}
	return ret;
}

int sceNetCtlInetUnregisterCallback(int cid)
{
	return netCtlUnregisterCallback(g_inetCallbacks, SCE_NET_CTL_CALLBACK_MAX, cid);
}

/* name unknown */
int SceNetCtl_229EAA64(int cid)
{
	return netCtlUnregisterCallback(g_inetCallbacks2, SCE_NET_CTL_CALLBACK_MAX, cid);
}

/* name unknown */
int SceNetCtl_E99EE305(int cid)
{
	return netCtlUnregisterCallback(g_inetCallbacks3, SCE_NET_CTL_CALLBACK_MAX, cid);
}

/* name unknown: same table as SceNetCtl_E99EE305 */
int SceNetCtl_B0122008(int cid)
{
	return netCtlUnregisterCallback(g_inetCallbacks3, SCE_NET_CTL_CALLBACK_MAX, cid);
}

int sceNetCtlAdhocUnregisterCallback(int cid)
{
	return netCtlUnregisterCallback(g_adhocCallbacks, SCE_NET_CTL_CALLBACK_MAX, cid);
}

/* name unknown */
int SceNetCtl_3AB4D791(int cid)
{
	return netCtlUnregisterCallback(g_adhocCallbacks2, SCE_NET_CTL_CALLBACK_MAX, cid);
}

/* name unknown */
int SceNetCtl_E63C100B(int cid)
{
	return netCtlUnregisterCallback(g_adhocCallbacks3, SCE_NET_CTL_CALLBACK_MAX, cid);
}

/* name unknown */
int SceNetCtl_2CF683AD(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc20006(SceShellSvc_B31E7F1C(), 0);
}

/* name unknown */
int SceNetCtl_67DE0C31(int a0)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc20007(SceShellSvc_B31E7F1C(), a0);
}

/* name unknown: hands a network configuration to the service */
int SceNetCtl_17FB44C4(const NetCtlNetConfig *config)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (config == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc20008(SceShellSvc_B31E7F1C(), config, config->data1, config->size1,
			      config->data2, config->size2);
}

/* name unknown: SceNetCtl_17FB44C4 for a configuration whose reserved fields are clear */
int SceNetCtl_EA423A96(const NetCtlNetConfig *config)
{
	char zero8[8];
	char zero44[44];
	char zero16[16];
	char zero98[98];

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (config == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;

	sceClibMemset(zero98, 0, sizeof(zero98));
	sceClibMemset(zero8, 0, sizeof(zero8));
	sceClibMemset(zero16, 0, sizeof(zero16));
	sceClibMemset(zero44, 0, sizeof(zero44));
	if (config->unk71 != 0
	    || sceClibMemcmp(config->unk420, zero98, sizeof(zero98)) != 0
	    || sceClibMemcmp(config->unk482, zero98, sizeof(zero98)) != 0
	    || sceClibMemcmp(config->unk4E4, zero98, sizeof(zero98)) != 0
	    || sceClibMemcmp(config->unk546, zero98, sizeof(zero98)) != 0
	    || config->unk5A8 != 0
	    || config->unk5AC != 0
	    || sceClibMemcmp(config->unk5B0, zero8, sizeof(zero8)) != 0
	    || sceClibMemcmp(config->unk5B8, zero16, sizeof(zero16)) != 0
	    || config->unk5D8 != 0
	    || sceClibMemcmp(config->unk5DC, zero44, sizeof(zero44)) != 0)
		return NETCTL_ERROR_UNK_804121CD;
	return SceNetCtl_17FB44C4(config);
}

/* name unknown */
int SceNetCtl_54848654(const NetCtlNetConfig *config, int a1)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (config == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc20009(SceShellSvc_B31E7F1C(), config, config->data1, config->size1,
			      config->data2, config->size2, a1);
}

/* name unknown */
int SceNetCtl_C1677C65(const char *name)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2001B(SceShellSvc_B31E7F1C(), name);
}

/* name unknown */
int SceNetCtl_1581FABC(const char *name)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (name == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc2001C(SceShellSvc_B31E7F1C(), name);
}

/* name unknown */
int SceNetCtl_6FFC4D4E(const char *name)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (name == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc2001D(SceShellSvc_B31E7F1C(), name);
}

/* name unknown */
int SceNetCtl_D315EE4F(const char *name, int a1)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (name == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc2001E(SceShellSvc_B31E7F1C(), name, a1);
}

/* name unknown: installs the hook sceNetCtlAdhocDisconnect and module_stop call */
int SceNetCtl_326FEE96(void (*hook)(void))
{
	g_netCtlAdhocDisconnectHook = hook;
	return 0;
}

/* name unknown */
int SceNetCtl_DD96DA85(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2000A(SceShellSvc_B31E7F1C(), 0);
}

/* name unknown: SceNetCtl_DD96DA85 with the error folded into one return */
int SceNetCtl_54EBA0FB(void)
{
	int ret;

	ret = SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlIsInitialized)
		ret = netCtlIpc2000A(SceShellSvc_B31E7F1C(), 0);
	return ret;
}

int sceNetCtlAdhocDisconnect(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	netCtlAdhocAuthDisconnect();
	if (g_netCtlAdhocDisconnectHook != NULL)
		g_netCtlAdhocDisconnectHook();
	return netCtlIpcAdhocDisconnect(SceShellSvc_B31E7F1C(), 0);
}

int sceNetCtlInetGetInfo(int code, SceNetCtlInfo *info)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (info == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->inetGetInfo == NULL)
		return netCtlIpcInetGetInfo(SceShellSvc_B31E7F1C(), code, info);
	return g_netCtlHandlers->inetGetInfo(code, info);
}

/* name unknown: same as sceNetCtlInetGetInfo */
int SceNetCtl_C33D7374(int code, SceNetCtlInfo *info)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (info == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->inetGetInfo == NULL)
		return netCtlIpcInetGetInfo(SceShellSvc_B31E7F1C(), code, info);
	return g_netCtlHandlers->inetGetInfo(code, info);
}

/* name unknown: same as sceNetCtlInetGetInfo */
int SceNetCtl_D24E1CBB(int code, SceNetCtlInfo *info)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (info == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->inetGetInfo == NULL)
		return netCtlIpcInetGetInfo(SceShellSvc_B31E7F1C(), code, info);
	return g_netCtlHandlers->inetGetInfo(code, info);
}

int sceNetCtlInetGetState(int *state)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (state == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->inetGetState == NULL)
		return netCtlIpcInetGetState(SceShellSvc_B31E7F1C(), state);
	return g_netCtlHandlers->inetGetState(state);
}

/* name unknown */
int SceNetCtl_E8E8DF18(int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (value == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_E8E8DF18 == NULL)
		return netCtlIpc2000E(SceShellSvc_B31E7F1C(), value);
	return g_netCtlHandlers->handler_E8E8DF18(value);
}

/* name unknown: same as SceNetCtl_E8E8DF18 */
int SceNetCtl_BBAD980D(int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (value == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_E8E8DF18 == NULL)
		return netCtlIpc2000E(SceShellSvc_B31E7F1C(), value);
	return g_netCtlHandlers->handler_E8E8DF18(value);
}

/* name unknown: adhoc counterpart of sceNetCtlInetGetInfo */
int SceNetCtl_CDEBA500(int code, void *info)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (info == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_CDEBA500 == NULL)
		return netCtlIpc20021(SceShellSvc_B31E7F1C(), code, info);
	return g_netCtlHandlers->handler_CDEBA500(code, info);
}

int sceNetCtlAdhocGetState(int *state)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (state == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->adhocGetState == NULL)
		return netCtlIpcAdhocGetState(SceShellSvc_B31E7F1C(), state);
	return g_netCtlHandlers->adhocGetState(state);
}

/* name unknown */
int SceNetCtl_0CC36330(int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (value == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_0CC36330 == NULL)
		return netCtlIpc20022(SceShellSvc_B31E7F1C(), value);
	return g_netCtlHandlers->handler_0CC36330(value);
}

int sceNetCtlGetPhoneMaxDownloadableSize(SceInt64 *maxDownloadableSize)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (maxDownloadableSize == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (netCtlIpcGetPhoneMaxDownloadableSize(SceShellSvc_B31E7F1C(), maxDownloadableSize) < 0)
		*maxDownloadableSize = NETCTL_PHONE_DEFAULT_DOWNLOADABLE_SIZE;
	return 0;
}

/* name unknown: starts a scan for up to ten SSIDs */
int SceNetCtl_6A56FC3A(const char **ssids, SceUInt32 count, const NetCtlScanParam *param)
{
	SceUInt32 i;
	char *name;
	NetCtlScanParam scanParam;
	char names[NETCTL_SCAN_SSID_MAX][NETCTL_SSID_SIZE];

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (ssids == NULL && count != 0)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (count > NETCTL_SCAN_SSID_MAX)
		return NETCTL_ERROR_UNK_80412191;
	if (count != 0) {
		for (i = 0; i < count; i++) {
			if (sceClibStrnlen(ssids[i], NETCTL_SSID_SIZE) == 0
			    || sceClibStrnlen(ssids[i], NETCTL_SSID_SIZE) >= NETCTL_SSID_SIZE)
				return NETCTL_ERROR_UNK_80412190;
		}
	}
	name = names[0];
	for (i = 0; i < count; i++) {
		sceClibStrncpy(name, ssids[i], NETCTL_SSID_SIZE);
		name[NETCTL_SSID_SIZE - 1] = '\0';
		name += NETCTL_SSID_SIZE;
	}
	sceClibMemset(&scanParam, 0, sizeof(scanParam));
	scanParam.size = sizeof(scanParam);
	if (param != NULL) {
		sceClibMemcpy_safe(&scanParam, param, sizeof(scanParam));
		if (scanParam.size != sizeof(scanParam))
			return NETCTL_ERROR_UNK_8041219E;
	}
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_6A56FC3A == NULL)
		return netCtlIpc20010(SceShellSvc_B31E7F1C(), names, count, &scanParam,
				      (int)scanParam.data, scanParam.dataSize);
	return g_netCtlHandlers->handler_6A56FC3A(ssids, count, &scanParam);
}

/* name unknown: SceNetCtl_6A56FC3A for a single SSID */
int SceNetCtl_BB163832(const char *ssid, const NetCtlScanParam *param)
{
	const char *ssids[1];
	char zero8[8];
	char zero32[32];

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (ssid == NULL)
		return NETCTL_ERROR_UNK_80412190;
	ssids[0] = ssid;
	sceClibMemset(zero32, 0, sizeof(zero32));
	sceClibMemset(zero8, 0, sizeof(zero8));
	if (param != NULL) {
		if ((param->flags & ~3) != 0)
			return NETCTL_ERROR_UNK_804121CE;
		if (sceClibMemcmp(param->reserved08, zero32, sizeof(zero32)) != 0
		    || sceClibMemcmp(param->reserved30, zero8, sizeof(zero8)) != 0)
			return NETCTL_ERROR_UNK_804121CD;
	}
	return SceNetCtl_6A56FC3A(ssids, 1, param);
}

/* name unknown */
int SceNetCtl_47980B9B(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_47980B9B == NULL)
		return netCtlIpc20011(SceShellSvc_B31E7F1C(), 0);
	return g_netCtlHandlers->handler_47980B9B();
}

/* name unknown */
int SceNetCtl_92EB6B1A(int a0, void *buf)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (buf == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_92EB6B1A == NULL)
		return netCtlIpc20012(SceShellSvc_B31E7F1C(), a0, buf);
	return g_netCtlHandlers->handler_92EB6B1A(a0, buf);
}

/* name unknown: copies up to count scan results (6-byte BSSIDs) */
int SceNetCtl_A840C74C(void *buf, int count)
{
	int outCount;

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_A840C74C == NULL)
		return netCtlIpc20013(SceShellSvc_B31E7F1C(), buf, count, &outCount);
	return g_netCtlHandlers->handler_A840C74C(buf, count);
}

/* name unknown: details of one scan result */
int SceNetCtl_1A254169(void *bssid, void *info, void *extra, int *extraSize)
{
	int ret;
	int size = 0;
	int outSize;

	if (!g_netCtlIsInitialized) {
		ret = SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	} else if (bssid == NULL || info == NULL) {
		ret = SCE_NET_CTL_ERROR_INVALID_ADDR;
	} else if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_1A254169 == NULL) {
		if (extraSize != NULL && extra != NULL)
			size = *extraSize;
		ret = netCtlIpc20014(SceShellSvc_B31E7F1C(), bssid, info, extra, size, &outSize);
		if (ret >= 0 && extraSize != NULL)
			*extraSize = outSize;
	} else {
		ret = g_netCtlHandlers->handler_1A254169(bssid, info, (int)extra, extraSize);
	}
	return ret;
}

/* name unknown */
int SceNetCtl_27932A1B(void *info, void *extra, int *extraSize)
{
	int ret;
	int size = 0;
	int outSize;

	if (!g_netCtlIsInitialized) {
		ret = SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	} else if (info == NULL) {
		ret = SCE_NET_CTL_ERROR_INVALID_ADDR;
	} else if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_27932A1B == NULL) {
		if (extraSize != NULL && extra != NULL)
			size = *extraSize;
		ret = netCtlIpc20015(SceShellSvc_B31E7F1C(), info, extra, size, &outSize);
		if (ret >= 0 && extraSize != NULL)
			*extraSize = outSize;
	} else {
		ret = g_netCtlHandlers->handler_27932A1B(info, (int)extra, extraSize);
	}
	return ret;
}

/* name unknown: same as SceNetCtl_27932A1B */
int SceNetCtl_C81AD91B(void *info, void *extra, int *extraSize)
{
	int ret;
	int size = 0;
	int outSize;

	if (!g_netCtlIsInitialized) {
		ret = SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	} else if (info == NULL) {
		ret = SCE_NET_CTL_ERROR_INVALID_ADDR;
	} else if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_27932A1B == NULL) {
		if (extraSize != NULL && extra != NULL)
			size = *extraSize;
		ret = netCtlIpc20015(SceShellSvc_B31E7F1C(), info, extra, size, &outSize);
		if (ret >= 0 && extraSize != NULL)
			*extraSize = outSize;
	} else {
		ret = g_netCtlHandlers->handler_27932A1B(info, (int)extra, extraSize);
	}
	return ret;
}

/* name unknown */
int SceNetCtl_484EE0B2(void *buf, int count)
{
	int outCount;

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_484EE0B2 == NULL)
		return netCtlIpc20016(SceShellSvc_B31E7F1C(), buf, count, &outCount);
	return g_netCtlHandlers->handler_484EE0B2(buf, count);
}

/* name unknown */
int SceNetCtl_A59146E1(void *bssid, void *info, void *extra, int *extraSize)
{
	int ret;
	int size = 0;
	int outSize;

	if (!g_netCtlIsInitialized) {
		ret = SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	} else if (bssid == NULL || info == NULL) {
		ret = SCE_NET_CTL_ERROR_INVALID_ADDR;
	} else if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_A59146E1 == NULL) {
		if (extraSize != NULL && extra != NULL)
			size = *extraSize;
		ret = netCtlIpc20017(SceShellSvc_B31E7F1C(), bssid, info, extra, size, &outSize);
		if (ret >= 0 && extraSize != NULL)
			*extraSize = outSize;
	} else {
		ret = g_netCtlHandlers->handler_A59146E1(bssid, info, (int)extra, extraSize);
	}
	return ret;
}

/* name unknown */
int SceNetCtl_147B054C(int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (value == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_147B054C == NULL)
		return netCtlIpc20018(SceShellSvc_B31E7F1C(), value);
	return g_netCtlHandlers->handler_147B054C(value);
}

/* name unknown */
int SceNetCtl_C15740E9(int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (value == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_C15740E9 == NULL)
		return netCtlIpc20019(SceShellSvc_B31E7F1C(), value);
	return g_netCtlHandlers->handler_C15740E9(value);
}

/* name unknown */
int SceNetCtl_CCF30B92(int a0)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_CCF30B92 == NULL)
		return netCtlIpc2001A(SceShellSvc_B31E7F1C(), a0);
	return g_netCtlHandlers->handler_CCF30B92(a0);
}

int sceNetCtlAdhocGetPeerList(unsigned int *peerInfoNum, void *peerInfo)
{
	if (peerInfoNum == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlAdhocAuthGetPeerList(peerInfoNum, peerInfo);
}

int sceNetCtlAdhocGetInAddr(SceNetInAddr *inaddr)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (inaddr == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->adhocGetInAddr == NULL)
		return netCtlIpcAdhocGetInAddr(SceShellSvc_B31E7F1C(), inaddr);
	return g_netCtlHandlers->adhocGetInAddr(inaddr);
}

int sceNetCtlGetNatInfo(SceNetCtlNatInfo *natinfo)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (natinfo == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	switch (natinfo->size) {
	case sizeof(SceNetCtlNatInfo):
		break;
	default:
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	}
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->getNatInfo == NULL)
		return netCtlIpcGetNatInfo(SceShellSvc_B31E7F1C(), natinfo->size, natinfo);
	return g_netCtlHandlers->getNatInfo(natinfo);
}

/* name unknown: sceNetCtlGetNatInfo for a 0x28-byte structure */
int SceNetCtl_F30365D6(SceSize *info)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (info == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	switch (*info) {
	case 0x28:
		break;
	default:
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	}
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_F30365D6 == NULL)
		return netCtlIpc20025(SceShellSvc_B31E7F1C(), *info, info);
	return g_netCtlHandlers->handler_F30365D6(info);
}

/* name unknown */
int SceNetCtl_1AEEB480(int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (value == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc20026(SceShellSvc_B31E7F1C(), value);
}

/* name unknown */
int SceNetCtl_9629B72F(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc20027(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_8F09A038(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc20028(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_32A0BD7F(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc20029(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_AA63CAEC(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2002A(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_719DD1DA(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2002B(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_85C9CC02(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2002C(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_2792E1B6(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2002D(SceShellSvc_B31E7F1C());
}

/* name unknown: SceNetCtl_2792E1B6 with the error folded into one return */
int SceNetCtl_A0085E9F(void)
{
	int ret;

	ret = SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlIsInitialized)
		ret = netCtlIpc2002D(SceShellSvc_B31E7F1C());
	return ret;
}

/* name unknown */
int SceNetCtl_C2C30635(void *buf)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (buf == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc2002E(SceShellSvc_B31E7F1C(), buf);
}

/* name unknown */
int SceNetCtl_56C95F8F(int a0)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2002F(SceShellSvc_B31E7F1C(), a0);
}

int sceNetCtlGetIfStat(int device, SceNetCtlIfStat *ifstat)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (ifstat == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	switch (ifstat->size) {
	case sizeof(SceNetCtlIfStat):
		break;
	default:
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	}
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->getIfStat == NULL)
		return netCtlIpcGetIfStat(SceShellSvc_B31E7F1C(), device, ifstat->size, ifstat);
	return g_netCtlHandlers->getIfStat(device, ifstat);
}

/* name unknown: second interface statistics request */
int SceNetCtl_8F2C2053(int device, SceNetCtlIfStat *ifstat)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (ifstat == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	switch (ifstat->size) {
	case sizeof(SceNetCtlIfStat):
		break;
	default:
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	}
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_8F2C2053 == NULL)
		return netCtlIpc20031(SceShellSvc_B31E7F1C(), device, ifstat->size, ifstat);
	return g_netCtlHandlers->handler_8F2C2053(device, ifstat);
}

/* name unknown */
int SceNetCtl_0815CAC5(int a0)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc20032(SceShellSvc_B31E7F1C(), a0);
}

/* name unknown */
int SceNetCtl_2FB386D9(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc20033(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_777FE4C2(int a0, int a1)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_777FE4C2 == NULL)
		return netCtlIpc20034(SceShellSvc_B31E7F1C(), a0, a1);
	return g_netCtlHandlers->handler_777FE4C2(a0, a1);
}

/* name unknown */
int SceNetCtl_8B2FEFAA(int a0, int a1)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_8B2FEFAA == NULL)
		return netCtlIpc20035(SceShellSvc_B31E7F1C(), a0, a1);
	return g_netCtlHandlers->handler_8B2FEFAA(a0, a1);
}

/* name unknown: same as SceNetCtl_777FE4C2 */
int SceNetCtl_A7F4F4AB(int a0, int a1)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_777FE4C2 == NULL)
		return netCtlIpc20034(SceShellSvc_B31E7F1C(), a0, a1);
	return g_netCtlHandlers->handler_777FE4C2(a0, a1);
}

/* name unknown: same as SceNetCtl_8B2FEFAA */
int SceNetCtl_766E2AE1(int a0, int a1)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (g_netCtlHandlers == NULL || g_netCtlHandlers->handler_8B2FEFAA == NULL)
		return netCtlIpc20035(SceShellSvc_B31E7F1C(), a0, a1);
	return g_netCtlHandlers->handler_8B2FEFAA(a0, a1);
}

/* name unknown: stores up to 30 entries of 0x478 bytes */
int SceNetCtl_3C35D284(const void *entries, SceUInt32 count)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (entries == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (count == 0 || count > NETCTL_ENTRY_MAX)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	return netCtlIpc20036(SceShellSvc_B31E7F1C(), entries, count);
}

/* name unknown: reads entries by the id in their first word */
int SceNetCtl_1EA25873(NetCtlEntry *entries, SceUInt32 count)
{
	SceUInt32 i;
	int ids[NETCTL_ENTRY_MAX];

	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (entries == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	if (count == 0 || count > NETCTL_ENTRY_MAX)
		return SCE_NET_CTL_ERROR_INVALID_SIZE;
	for (i = 0; i < count; i++)
		ids[i] = entries[i].id;
	return netCtlIpc20037(SceShellSvc_B31E7F1C(), ids, entries, count);
}

/* name unknown */
int SceNetCtl_236DED99(const NetCtlEntry *entry)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (entry == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc20038(SceShellSvc_B31E7F1C(), entry);
}

/* name unknown */
int SceNetCtl_2F2A259D(NetCtlEntry *entry)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (entry == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc20039(SceShellSvc_B31E7F1C(), entry);
}

/* name unknown: latches the current network time as the statistics reset tick */
int SceNetCtl_21AD7A46(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	sceRtcGetCurrentNetworkTick(&g_netCtlResetTick);
	return 0;
}

/* name unknown: reads the tick latched by SceNetCtl_21AD7A46 */
int SceNetCtl_2B34DCCE(SceRtcTick *tick)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (tick == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	sceClibMemcpy_safe(tick, &g_netCtlResetTick, sizeof(*tick));
	return 0;
}

/* name unknown */
int SceNetCtl_2B84A67F(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2003A(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_25EE061C(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2003B(SceShellSvc_B31E7F1C());
}

/* name unknown */
int SceNetCtl_32237A58(int *value)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2003C(SceShellSvc_B31E7F1C(), value);
}

/* name unknown */
int SceNetCtl_57CC3812(int a0)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2003D(SceShellSvc_B31E7F1C(), a0);
}

/* name unknown */
int SceNetCtl_6D63C34B(void *buf)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	if (buf == NULL)
		return SCE_NET_CTL_ERROR_INVALID_ADDR;
	return netCtlIpc2000C(SceShellSvc_B31E7F1C(), buf);
}

/* name unknown */
int SceNetCtl_9D96400C(void)
{
	if (!g_netCtlIsInitialized)
		return SCE_NET_CTL_ERROR_NOT_INITIALIZED;
	return netCtlIpc2003E(SceShellSvc_B31E7F1C());
}

/* Calls every callback of the selected tables (see netCtlDispatchEvent). */
static void netCtlCallTables(NetCtlCallbackSlot *publicSlots, NetCtlCallbackSlot *firstSlots,
			     NetCtlCallbackSlot *secondSlots, int publicToo, int first,
			     int eventType, int publicEventType)
{
	unsigned int i;

	if (first == 1) {
		for (i = 0; i < SCE_NET_CTL_CALLBACK_MAX; i++) {
			if (firstSlots[i].func != NULL)
				firstSlots[i].func(eventType, firstSlots[i].arg);
		}
		if (publicToo == 1) {
			for (i = 0; i < SCE_NET_CTL_CALLBACK_MAX; i++) {
				if (publicSlots[i].func != NULL)
					publicSlots[i].func(publicEventType, publicSlots[i].arg);
			}
		}
	} else {
		for (i = 0; i < SCE_NET_CTL_CALLBACK_MAX; i++) {
			if (secondSlots[i].func != NULL)
				secondSlots[i].func(eventType, secondSlots[i].arg);
		}
	}
}

/*
 * Maps a service event to callback event types: events 1..14 go to the inet
 * tables, 15..20 to the adhoc tables.  Odd events reach the first private
 * table (and, for the public events, the public table); even events the
 * second private table.
 */
static int netCtlDispatchEvent(int eventId, int first)
{
	int publicToo = 0;
	int publicEventType = 0;
	int eventType;

	if (sceKernelLockLwMutex(&g_netCtlMutex, 1, NULL) < 0)
		return 0;
	switch (eventId) {
	case 1:
	case 2:
	case 15:
	case 16:
		eventType = 2;
		if (eventId == 15 || eventId == 16)
			goto adhoc;
		break;
	case 3:
	case 4:
		eventType = 7;
		break;
	case 5:
	case 6:
	case 17:
	case 18:
		eventType = 3;
		publicToo = 1;
		publicEventType = SCE_NET_CTL_EVENT_TYPE_DISCONNECT_REQ_FINISHED;
		if (eventId == 17 || eventId == 18)
			goto adhoc;
		break;
	case 7:
	case 8:
		eventType = 4;
		break;
	case 9:
	case 10:
	case 19:
	case 20:
		eventType = 1;
		publicToo = 1;
		publicEventType = SCE_NET_CTL_EVENT_TYPE_DISCONNECTED;
		if (eventId == 19 || eventId == 20)
			goto adhoc;
		break;
	case 11:
	case 12:
		eventType = 5;
		break;
	case 13:
	case 14:
		eventType = 6;
		publicToo = 1;
		publicEventType = SCE_NET_CTL_EVENT_TYPE_IPOBTAINED;
		break;
	default:
		goto out;
	}
	goto inet;
adhoc:
	netCtlCallTables(g_adhocCallbacks, g_adhocCallbacks2, g_adhocCallbacks3, publicToo, first,
			 eventType, publicEventType);
	goto out;
inet:
	netCtlCallTables(g_inetCallbacks, g_inetCallbacks2, g_inetCallbacks3, publicToo, first,
			 eventType, publicEventType);
out:
	sceKernelUnlockLwMutex(&g_netCtlMutex, 1);
	return 0;
}

static void netCtlEventHandlerOdd(int eventId)
{
	netCtlDispatchEvent(eventId, 1);
}

static void netCtlEventHandlerEven(int eventId)
{
	netCtlDispatchEvent(eventId, 0);
}
