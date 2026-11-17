/*
 * The shell's network: the libraries started at boot (SceNet with the shell's memory and the
 * emulation the registry sets, the shell's NetCtl server and its IPC server, libnetctl given the
 * server's functions, STUN, UPnP, SSL and HTTP), and the functions the NetCtl server asks the
 * shell: which processes may use the network, the exclusive function it runs under, the WISPr
 * login, the mobile network the telephony plugin reports, and the notice that a connection is
 * restricted. The file's and every name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/threadmgr.h>
#include <libkernel/libkernel.h>
#include <appmgr/appmgr.h>
#include <appmgr/appmgr_notification.h>
#include <regmgr/regmgr.h>
#include <wlanbt/wlanbt.h>
#include <net.h>
#include <net/netemu.h>
#include <net_ps/netps_thread.h>
#include <libnet/netinternal.h>
#include <libnetctl.h>
#include <libnetctl/netctl_handlers.h>
#include <libssl.h>
#include <libhttp.h>
#include <libhttp/httpinternal.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <std/string>
#include <std/stdlib.h>
#include <std/string.h>
#include "shell.h"
#include "plugins.h"

using namespace paf;

/* the application whose status says which process is in front (appmgr's, name ours) */
#define SHELL_NET_APP_ID_FRONT		-6
/* the application whose status says which process a restriction notice is for */
#define SHELL_NET_APP_ID_NOTICE		-3

#define SHELL_NET_SLEEP_COUNT_MAX	4
#define SHELL_NET_THREAD_MAX		256

/* SceNetInternal_EE94BF52's policy bits */
#define SHELL_NET_POLICY_SUSPENDED	0x1
#define SHELL_NET_POLICY_BACKGROUND	0x2

/* the telephony modem's state query and the state that says the network is there */
#define SHELL_NET_TELEPHONY_QUERY	1
#define SHELL_NET_TELEPHONY_AVAILABLE	1
/* the telephony service's status asked for the mobile network */
#define SHELL_NET_TELEPHONY_STATUS	3

#define SHELL_NET_MEMORY_SIZE		0x8000
#define SHELL_NET_THREAD_PRIORITY	0x10000114
#define SHELL_NET_NP_ENV_SIZE		17

/* what the mobile network's name is refused with: no buffer, a buffer too small (libnetctl's
   codes; no SDK header names them) */
#define SHELL_NET_ERROR_INVALID_ADDR	-2143215228	/* 0x80412184 */
#define SHELL_NET_ERROR_INVALID_SIZE	-2143215202	/* 0x8041219e */

int s_wlanPreventProcess = -1;
int s_comPreventProcess = -1;
static ShellNetWisprCallback s_wisprCallback = NULL;

static char s_netMemory[SHELL_NET_MEMORY_SIZE];

/* may this thread (threadId -1: any of the process's) use the network now: the shell's own
   while its NetCtl server allows it, the front process, or a process the server links to
   the front one; ONLY_FRONT leaves out the policy checks */
static bool shellNetIsThreadAllowed(const SceNetPsThreadInfo *thread, int onlyFront)
{
	SceUID self = sceKernelGetProcessId();
	SceAppMgrAppStatus status;
	int policy;

	if (sceAppMgrGetStatusByAppId(SHELL_NET_APP_ID_FRONT, &status) < 0)
		return false;

	SceUID front = status.processId;

	if (front < 0)
		return false;
	if (thread->processId == self)
		return SceShell_871A8E92() > 0 ||
		       (front == self && shellNetCtlGetActiveCount() > 0);
	if (onlyFront != 0) {
		if (thread->processId == front)
			return true;
	} else {
		if (thread->threadId >= 0) {
			if (SceNetInternal_EF421A65(thread->threadId, &policy, 1) < 0)
				return false;
			if ((policy & SHELL_NET_POLICY_SUSPENDED) != 0)
				return false;
		}
		if (SceNetInternal_EE94BF52(thread->processId, &policy, 0) < 0 ||
		    (policy & SHELL_NET_POLICY_SUSPENDED) != 0 ||
		    (policy & SHELL_NET_POLICY_BACKGROUND) != 0)
			return false;
		if (thread->processId == front)
			return true;
	}
	return shellNetCtlIsProcessLinked(thread->processId, front) == 1;
}

static void shellNetHostGetProcessStatus(SceUID processId, int *allowed, int *isFront)
{
	SceUID self = sceKernelGetProcessId();
	SceAppMgrAppStatus status;
	SceUID front = -1;

	if (sceAppMgrGetStatusByAppId(SHELL_NET_APP_ID_FRONT, &status) >= 0)
		front = status.processId;
	if (allowed != NULL)
		*allowed = 1;
	if (isFront != NULL)
		*isFront = processId != self && front == processId;
}

/* is a process the stack counts (or PROCESSID, -1: any) connecting while allowed: the shell's
   own while its NetCtl server allows it, or one whose policy lets it use the network */
static int shellNetHostIsProcessConnecting(SceUID frontProcessId, SceUID processId)
{
	SceAppMgrAppStatus status;
	SceNetPsSleepCount counts[SHELL_NET_SLEEP_COUNT_MAX];
	int policy;
	SceUID front = -1;

	if (sceAppMgrGetStatusByAppId(SHELL_NET_APP_ID_FRONT, &status) >= 0)
		front = status.processId;

	int count = SceNetInternal_DEF2357F(counts, SHELL_NET_SLEEP_COUNT_MAX, 0);

	if (count < 0)
		return 0;
	for (int i = 0; i < count; i++) {
		if (frontProcessId != -1 && front == frontProcessId &&
		    (SceUID)counts[i].identifier != frontProcessId)
			continue;
		if (shellNetCtlGetProcessState(front) != 0 &&
		    front != (SceUID)counts[i].identifier)
			continue;

		SceUID id = counts[i].identifier;
		int threadPolicy;

		if (id == sceKernelGetProcessId() && SceShell_5928D835() > 0)
			return 1;
		if (SceNetInternal_EE94BF52(id, &threadPolicy, 0) >= 0 &&
		    (threadPolicy & SHELL_NET_POLICY_SUSPENDED) == 0)
			return 1;
	}
	if ((frontProcessId == -1 || processId == frontProcessId) && processId >= 0) {
		if (processId == sceKernelGetProcessId() && SceShell_5928D835() > 0)
			return 1;
		if (SceNetInternal_EE94BF52(processId, &policy, 0) >= 0 &&
		    (policy & SHELL_NET_POLICY_SUSPENDED) == 0)
			return 1;
	}
	return 0;
}

/* has the process (or any) a thread in the stack that may use the network */
static int shellNetHostHasNetworkThread(SceUID processId)
{
	SceNetPsThreadInfo threads[SHELL_NET_THREAD_MAX];
	int count = SceNetInternal_46E28336(threads, SHELL_NET_THREAD_MAX, 0);

	if (count < 0)
		return 0;
	if (count > SHELL_NET_THREAD_MAX)
		count = SHELL_NET_THREAD_MAX;
	const SceNetPsThreadInfo *thread = threads;

	for (int i = 0; i < count; i++, thread++) {
		if (shellNetIsThreadAllowed(thread, 0))
			return 1;
	}
	if (processId >= 0) {
		SceNetPsThreadInfo one;

		sceClibMemset(&one, 0, sizeof(one));
		one.processId = processId;
		one.threadId = -1;
		if (shellNetIsThreadAllowed(&one, 0))
			return 1;
	}
	return 0;
}

static int shellNetHostExclusiveBegin(int id, void *callback, void *arg)
{
	return shellExclusiveFuncNetBegin(id, callback, arg);
}

static int shellNetHostExclusiveEnd(int id)
{
	return shellExclusiveFuncNetEnd(id);
}

/* is one of the processes in front */
static int shellNetHostIsAnyProcessAllowed(const SceUID *processIds, SceUInt32 count)
{
	for (SceUInt32 i = 0; i < count; i++) {
		SceNetPsThreadInfo thread;

		sceClibMemset(&thread, 0, sizeof(thread));
		thread.processId = processIds[i];
		thread.threadId = -1;
		if (shellNetIsThreadAllowed(&thread, 1))
			return 1;
	}
	return 0;
}

static void shellNetOnWisprEvent(int event, int arg0, int arg1, int arg2, int arg3, int arg4)
{
	if (s_wisprCallback != NULL)
		s_wisprCallback(event, arg0, arg1, arg2, arg3, arg4);
}

/* the WISPr login started for the access point */
static int shellNetHostWisprStart(ShellNetWisprCallback callback, int id, const char *ssid,
				  const void *bssid, int arg)
{
	ShellNetWisprParam param;

	s_wisprCallback = callback;

	int ret = shellNetWisprInit(shellNetOnWisprEvent, SHELL_NET_MEMORY_SIZE,
				    SHELL_NET_THREAD_PRIORITY, arg);

	if (ret < 0)
		return ret;
	sceClibMemset(&param, 0, sizeof(param));
	sceClibStrlcpy(param.ssid, ssid, sizeof(param.ssid));
	sceClibMemcpy_safe(param.bssid, bssid, sizeof(param.bssid));
	return shellNetWisprStart(id, &param);
}

static void shellNetHostWisprStop(void)
{
	shellNetWisprStop();
	shellNetWisprTerm();
}

/* the mobile network's name, as the telephony plugin's modem reports it */
static int shellNetHostGetMobileNetworkName(char *name, SceSize size)
{
	if (name == NULL)
		return SHELL_NET_ERROR_INVALID_ADDR;
	if (size < 33)
		return SHELL_NET_ERROR_INVALID_SIZE;
	name[0] = '\0';

	Plugin *plugin = Plugin::Find("telephony_plugin");

	if (plugin == NULL)
		return SCE_NET_CTL_ERROR_NOT_AVAIL;

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return SCE_NET_CTL_ERROR_NOT_AVAIL;

	TelephonyModem *modem = iface->getModem();

	if (modem == NULL)
		return SCE_NET_CTL_ERROR_NOT_AVAIL;

	int query = SHELL_NET_TELEPHONY_QUERY;
	int ret = modem->vtable->getState(modem, &query);

	if (ret == SHELL_NET_TELEPHONY_AVAILABLE) {
		ret = modem->vtable->getNetworkName(modem, name);
		if (ret >= 0)
			return 0;
	}
	return ret;
}

static bool shellNetHostIsMobileNetworkAvailable(void)
{
	Plugin *plugin = Plugin::Find("telephony_plugin");

	if (plugin != NULL) {
		TelephonyPluginInterface *iface =
			(TelephonyPluginInterface *)plugin->GetInterface(1);

		if (iface != NULL) {
			TelephonyModem *modem = iface->getModem();

			if (modem != NULL) {
				int query = SHELL_NET_TELEPHONY_QUERY;

				return modem->vtable->getState(modem, &query) ==
				       SHELL_NET_TELEPHONY_AVAILABLE;
			}
		}
	}
	return false;
}

/* a connection was refused: Wi-Fi off (WLAN and MOBILE 1) or mobile data off (MOBILE 1); the
   notice is shown once per application, then the debug notification is sent */
static void shellNetHostNotifyComPrevented(int wlan, int mobile)
{
	int restriction;
	SceUInt8 dataEnabled;
	bool restricted = false;
	SceAppMgrAppStatus status;
	AppmgrNotificationRequest request;

	if (wlan == 0 && mobile == 0)
		return;
	restriction = 0;
	dataEnabled = 1;

	unsigned int config = sceWlanGetConfiguration();
	bool wlanOff = (config & 2) != 0;
	bool wlanOn = (config & 1) != 0;

	if (wlanOff == 1 && (wlan != 1 || !wlanOn))
		return;

	Plugin *telephony = Plugin::Find("telephony_plugin");

	if (telephony != NULL) {
		TelephonyPluginInterface *iface =
			(TelephonyPluginInterface *)telephony->GetInterface(1);

		if (iface != NULL) {
			TelephonyService *service = iface->getService();

			if (service != NULL) {
				int ret = service->vtable->getRestriction(service, &restriction).GetCode();

				if (ret >= 0)
					restricted = restriction == 1;
				service->vtable->getDataEnabled(service, &dataEnabled).GetCode();
			}
		}
	}
	if (wlan == 1 && mobile == 1) {
		if ((dataEnabled | wlanOn) == 0)
			return;
	} else if (wlan == 0 && mobile == 1) {
		if (restricted == 1 && dataEnabled == 0)
			return;
		if (!restricted)
			goto debug;
	}
	if (sceAppMgrGetStatusByAppId(SHELL_NET_APP_ID_NOTICE, &status) >= 0) {
		if (wlan == 1 && mobile == 1) {
			if (s_wlanPreventProcess == status.processId)
				goto debug;
			s_wlanPreventProcess = status.processId;
		} else if (wlan == 0 && mobile == 1) {
			if (s_comPreventProcess == status.processId)
				goto debug;
			s_comPreventProcess = status.processId;
		}
	}
	{
		ImposeNotice notice;
		Plugin *impose = Plugin::Find("impose_plugin");
		ImposePluginInterface *iface = NULL;

		if (impose != NULL)
			iface = (ImposePluginInterface *)impose->GetInterface(1);
		if (iface == NULL)
			return;
		sceClibMemset(&notice, 0, sizeof(notice));
		if (wlan == 1 && mobile == 1) {
			notice.name = "SceNetCtlWlanComPrevent";
			notice.message = iface->getString(
				IDParam("msg_notification_network_features_disabled_guide"));
		} else if (wlan == 0 && mobile == 1) {
			notice.name = "SceNetCtlComPrevent";
			notice.message = iface->getString(
				IDParam("msg_notification_mobile_network_feature_disabled_guide"));
		}
		iface->showNotice(&notice);
	}
debug:
	sceClibMemset(&request, 0, sizeof(request));
	sceClibSnprintf((char *)request.payload, sizeof(request.payload) - 1,
			"Power:\nR3147 -\n3G connection has been restricted.");
	sceAppMgrSendNotificationRequest(&request, 1);
}

/* the mobile network's status, as the telephony plugin's service reports it */
static int shellNetHostGetMobileNetworkStatus(int *status)
{
	int value[2];
	Plugin *plugin = Plugin::Find("telephony_plugin");

	if (plugin == NULL)
		return SCE_NET_CTL_ERROR_NOT_AVAIL;

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)plugin->GetInterface(1);

	if (iface == NULL)
		return SCE_NET_CTL_ERROR_NOT_AVAIL;

	TelephonyService *service = iface->getService();

	if (service == NULL)
		return SCE_NET_CTL_ERROR_NOT_AVAIL;

	int ret = service->vtable->getStatus(service, SHELL_NET_TELEPHONY_STATUS, value).GetCode();

	if (ret < 0)
		return ret;
	status[0] = value[0];
	status[1] = value[1];
	return 0;
}

static const ShellNetCtlHostFuncs s_netCtlHostFuncs = {
	shellNetHostGetProcessStatus,
	shellNetHostIsProcessConnecting,
	shellNetHostHasNetworkThread,
	shellNetHostExclusiveBegin,
	shellNetHostExclusiveEnd,
	shellNetHostIsAnyProcessAllowed,
	shellNetHostWisprStart,
	shellNetHostWisprStop,
	shellNetHostGetMobileNetworkName,
	shellNetHostIsMobileNetworkAvailable,
	shellNetHostNotifyComPrevented,
	shellNetHostGetMobileNetworkStatus,
};

static const NetCtlHandlers s_netCtlHandlers = {
	shellNetCtlInetGetInfo,
	shellNetCtlInetGetState,
	shellNetCtlHandle_E8E8DF18,
	shellNetCtlHandle_CDEBA500,
	shellNetCtlAdhocGetState,
	shellNetCtlHandle_0CC36330,
	shellNetCtlAdhocGetInAddr,
	shellNetCtlGetNatInfo,
	shellNetCtlHandle_F30365D6,
	shellNetCtlHandle_6A56FC3A,
	shellNetCtlHandle_47980B9B,
	shellNetCtlHandle_92EB6B1A,
	shellNetCtlHandle_A840C74C,
	shellNetCtlHandle_1A254169,
	shellNetCtlHandle_27932A1B,
	shellNetCtlHandle_484EE0B2,
	shellNetCtlHandle_A59146E1,
	shellNetCtlHandle_147B054C,
	shellNetCtlHandle_C15740E9,
	shellNetCtlHandle_CCF30B92,
	shellNetCtlGetIfStat,
	shellNetCtlGetIfStat,
	shellNetCtlHandle_777FE4C2,
	shellNetCtlHandle_8B2FEFAA,
};

static void shellNetOnAppEvent(int code, const void *data, SceSize size, void *arg);

void shellNetStart(void)
{
	SceNetInitParam param;
	RegMgrKeyInfo keys[2];
	char env[SHELL_NET_NP_ENV_SIZE];
	SceNetEmulationParam emulation;
	ShellNetUpnpParam upnp;

	if (SceShell_83D16CA8(SHELL_APP_EVENT_ENDED, shellNetOnAppEvent, NULL) < 0)
		return;
	sce_paf_memset(&param, 0, sizeof(param));
	param.memory = s_netMemory;
	param.size = sizeof(s_netMemory);
	param.flags = 0;
	if (sceNetInit(&param) < 0)
		return;

	sceClibMemset(keys, 0, sizeof(keys));
	keys[0].size = sizeof(keys[0]);
	keys[0].name = "emulation_if";
	keys[0].type = 0;
	keys[0].valueSize = sizeof(int);
	keys[1].size = sizeof(keys[1]);
	keys[1].name = "emulation_type";
	keys[1].type = 0;
	keys[1].valueSize = sizeof(int);
	if (sceRegMgrGetKeys("/CONFIG/NET", keys, 2) >= 0) {
		sceClibMemset(&emulation, 0, sizeof(emulation));
		emulation.option_number = keys[1].value;
		SceNetInternal_DF815D48(&emulation, keys[0].value);
	}
	SceNetInternal_EE1985D9(1);
	if (shellNetCtlInit(0x2000, SHELL_NET_THREAD_PRIORITY, 0x2000, SHELL_NET_THREAD_PRIORITY,
			    &s_netCtlHostFuncs) < 0)
		return;
	if (shellNetCtlIpcInit() < 0)
		return;
	SceNetCtl_C61F3E96(&s_netCtlHandlers);
	if (sceNetCtlInit() < 0)
		return;
	if (shellNetStunInit(0x4000, sce_paf_rand) < 0)
		return;
	if (sceRegMgrGetKeyStr("/CONFIG/NP", "env", env, sizeof(env)) < 0) {
		sceClibStrncpy(env, "np", sizeof(env));
		env[sizeof(env) - 1] = '\0';
	}
	if (shellNetUpnpInit(0x1000, SHELL_NET_THREAD_PRIORITY, env) < 0)
		return;
	sce_paf_memset(&upnp, 0, sizeof(upnp));
	upnp.enable = 1;
	if (shellNetUpnpSetParam(&upnp) < 0)
		return;
	if (sceSslInit(0x5F000) < 0)
		return;
	if (sceHttpInit(0x14000) < 0)
		return;
	SceHttp_855D5254(1);
}

/* an application ended: the NetCtl server forgets it, and its restriction notices may show
   again */
static void shellNetOnAppEvent(int code, const void *data, SceSize size, void *arg)
{
	const ShellAppEvent *event = (const ShellAppEvent *)data;

	if (code != SHELL_APP_EVENT_ENDED)
		return;
	shellNetCtlProcessEnded(event->processId);
	if (event->processId == s_wlanPreventProcess)
		s_wlanPreventProcess = -1;
	if (event->processId == s_comPreventProcess)
		s_comPreventProcess = -1;
}
