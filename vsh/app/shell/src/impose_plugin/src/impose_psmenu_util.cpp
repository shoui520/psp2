/*
 * impose_psmenu_util.cpp: what the PS menu's parts share (0x81159DF0-0x8115B498): the jobs
 * that open the application menu and wait for it, the menu's event flag, the flight mode's job
 * and its error dialog, the wireless switches' settings, and the incoming dialog server, which
 * keeps the applications' sceIncomingDialog requests for the impose plugin's dialog. The file's
 * and the names are ours (PLACEHOLDER) where Sony's strings do not give them.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <std/stdlib.h>
#include <common/shared_ptr.h>
#include <job/job.h>
#include <thread/thread.h>
#include <inputdevice/inputdevice.h>
#include <widget/core/handler.h>
#include <widget/w_scene.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <kernel/error.h>
#include <appmgr.h>
#include <appmgr/appmgr.h>
#include <regmgr/regmgr.h>
#include <power/power.h>
#include <bt/bt.h>
#include <bt/bt_event.h>
#include <wlanbt/wlanbt.h>
#include <shell_svc/shell_util.h>
#include <ipmi/session.h>
#include "../../applauncher_plugin/src/applauncher.h"
#include "../../../plugins.h"
#include "../../../shell.h"
#include "impose.h"

using namespace paf;

/*
 * The menu's event flag (PLACEHOLDER names): the menu asks the application menu to open (0x10)
 * and is answered (0x20 or 0x40: opened), asks whether it is ready (0x80; 0x100 ready, 0x200
 * not), and tells it of its closing (2), the vibration (4 on, 8 off) and Wi-Fi (0x400 on, 0x800
 * off).
 */
#define PSMENU_EVENT_CLOSE		0x2
#define PSMENU_EVENT_VIBRATION_ON	0x4
#define PSMENU_EVENT_VIBRATION_OFF	0x8
#define PSMENU_EVENT_OPEN		0x10
#define PSMENU_EVENT_OPEN_DONE		0x20
#define PSMENU_EVENT_OPEN_FAILED	0x40
#define PSMENU_EVENT_READY		0x80
#define PSMENU_EVENT_READY_DONE		0x100
#define PSMENU_EVENT_READY_FAILED	0x200
#define PSMENU_EVENT_WLAN_ON		0x400
#define PSMENU_EVENT_WLAN_OFF		0x800

/* the waits for the answers, in microseconds (PLACEHOLDER names) */
#define PSMENU_OPEN_TIMEOUT		1000000
#define PSMENU_READY_TIMEOUT		300000
#define PSMENU_READY_RETRY_DELAY	1000000

/* the flight mode's timers and waits (PLACEHOLDER names) */
#define FLIGHT_MODE_CALLBACK_TIMER	(ui::Handler::CB_INTERVAL | 0x1001)
#define FLIGHT_MODE_ERROR_TIMER		(ui::Handler::CB_INTERVAL | 0x1002)
#define FLIGHT_MODE_CALLBACK_INTERVAL	100.0f
#define FLIGHT_MODE_ERROR_TIMEOUT	5000.0f
#define FLIGHT_MODE_DRIVER_TIMEOUT	20000

/* the flight mode job's errors (PLACEHOLDER names) */
#define FLIGHT_MODE_ERROR_BT_TIMEOUT	((int)0x80108F00)
#define FLIGHT_MODE_ERROR_WLAN_TIMEOUT	((int)0x80108F01)
#define FLIGHT_MODE_ERROR_WLAN		((int)0x80108F03)

/* the Bluetooth driver's events the job reads (PLACEHOLDER names) */
#define FLIGHT_MODE_BT_EVENT_MASK	0x200000ULL
#define FLIGHT_MODE_BT_EVENT_CONFIG	0x07
#define FLIGHT_MODE_BT_EVENT_POWER	0x15
#define FLIGHT_MODE_BT_EVENTS		2
#define FLIGHT_MODE_BT_READ_AGAIN	((int)0x802F0C04)

/* the configuration the drivers are asked for: on, off; the flight mode's off and on */
#define FLIGHT_MODE_RADIO_ON		1
#define FLIGHT_MODE_RADIO_FLIGHT_OFF	2
#define FLIGHT_MODE_RADIO_FLIGHT_ON	3

/* the error dialog's text and the shell's parts it locks (PLACEHOLDER names) */
#define FLIGHT_MODE_MSG_ERROR		0x2F35EFB3
#define FLIGHT_MODE_ERROR_LOCK		0x17
#define FLIGHT_MODE_ERROR_EVENT_CLOSED	1

/* indicator_plugin's Bluetooth mark (PLACEHOLDER name) */
#define FLIGHT_MODE_INDICATOR_BLUETOOTH	7

/* the menu's event flag (0x815448F8) */
static SceUID s_eventFlag = -1;

/* the scene and the dialog of the flight mode's error (0x815448FC, 0x81544900) */
static ui::Scene *s_errorScene = NULL;
static SceInt32 s_errorDialog = -1;

/* the applications while which the switches cannot be used (0x81544904) */
static const char *s_appNames[21] = {
	"NPXS10015", "NPXS10040", "NPXS10041", "NPXS10042", "NPXS10043", "NPXS10044",
	"NPXS10045", "NPXS10046", "NPXS10047", "NPXS10048", "NPXS10049", "NPXS10050",
	"NPXS10051", "NPXS10052", "NPXS10053", "NPXS10054", "NPXS10055", "NPXS10056",
	"NPXS10057", "NPXS10058", "NPXS10059",
};

/* none of them is running (0x81544958) */
static bool s_switchesUsable = true;

/* the incoming dialog server: its lock, the last request's number and the requests, one per
   application (PLACEHOLDER names) */
struct ShellIncomingDialogServer {
	ShellIncomingDialogServer() : mutex("SceIncomingDialogServer", 0), sequence(0)
	{
		for (SceUInt32 i = 0; i < 8; i++) {
			ShellIncomingDialogRequest *request = &requests[i];

			request->param = NULL;
			sce_paf_memset(request, 0, sizeof(*request));
			request->state = 0;
			request->processId = -1;
		}
	}

	thread::Mutex mutex;
	SceUInt32 sequence;
	ShellIncomingDialogRequest requests[8];
};

/* (0x8154495C) */
static ShellIncomingDialogServer *s_incomingDialogServer = NULL;

/* (0x81159DF0) */
ImposePsMenuOpenAppMenu::ImposePsMenuOpenAppMenu()
	: job::JobItem("ImposePsMenuOpenAppMenu", NULL)
{
}

/* the application menu asked to open: 1 opened, 2 not, 3 no answer (0x81159E14) */
int ImposePsMenuOpenAppMenu::Run()
{
	SceUInt32 bits;
	SceUInt32 timeout;
	SceInt32 ret;

	if (s_eventFlag == -1) {
		sce_paf_printf_dummy();
		return 0;
	}

	sceKernelSetEventFlag(s_eventFlag, PSMENU_EVENT_OPEN);
	timeout = PSMENU_OPEN_TIMEOUT;
	ret = sceKernelWaitEventFlag(s_eventFlag, PSMENU_EVENT_OPEN_DONE | PSMENU_EVENT_OPEN_FAILED,
				     SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				     &bits, &timeout);
	if (ret < 0) {
		sceKernelClearEventFlag(s_eventFlag, ~PSMENU_EVENT_OPEN);
		if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			return 3;
		sce_paf_printf_dummy();
		return 0;
	}

	return (bits & PSMENU_EVENT_OPEN_FAILED) != 0 ? 2 : 1;
}

void ImposePsMenuOpenAppMenu::Finish(int result)
{
}

/* (0x81159EB4) */
ImposePsMenuCheckAppMenuReady::ImposePsMenuCheckAppMenuReady()
	: job::JobItem("ImposePsMenuCheckAppMenuReady", NULL)
{
}

/* whether the application menu is ready: 1 ready, 2 no answer, 3 not yet (0x81159ED8) */
int ImposePsMenuCheckAppMenuReady::Run()
{
	SceUInt32 bits;
	SceUInt32 timeout;
	SceInt32 ret;

	if (s_eventFlag == -1) {
		sce_paf_printf_dummy();
		return 0;
	}

	timeout = PSMENU_READY_TIMEOUT;
	sceKernelSetEventFlag(s_eventFlag, PSMENU_EVENT_READY);
	ret = sceKernelWaitEventFlag(s_eventFlag,
				     PSMENU_EVENT_READY_DONE | PSMENU_EVENT_READY_FAILED,
				     SCE_KERNEL_EVF_WAITMODE_OR | SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT,
				     &bits, &timeout);
	if (ret < 0) {
		sceKernelClearEventFlag(s_eventFlag, ~PSMENU_EVENT_READY);
		if (ret == SCE_KERNEL_ERROR_WAIT_TIMEOUT)
			return 2;
		sce_paf_printf_dummy();
		return 0;
	}

	if ((bits & PSMENU_EVENT_READY_DONE) != 0)
		return 1;
	if (IsCanceled()) {
		sce_paf_printf_dummy();
		return 0;
	}

	sceKernelDelayThread(PSMENU_READY_RETRY_DELAY);
	return 3;
}

void ImposePsMenuCheckAppMenuReady::Finish(int result)
{
}

/* (0x81159F9A) */
void imposePsMenuEventCreate(void)
{
	s_eventFlag = sceKernelCreateEventFlag("SceShellUtilImposeEvent",
					       SCE_KERNEL_ATTR_MULTI | SCE_KERNEL_ATTR_OPENABLE, 0,
					       NULL);
}

/* (0x81159FBC) */
void imposePsMenuEventDelete(void)
{
	SceInt32 ret;

	if (s_eventFlag == -1)
		return;

	ret = sceKernelCancelEventFlag(s_eventFlag, 0, NULL);
	if (ret != 0)
		sce_paf_printf_dummy();
	ret = sceKernelDeleteEventFlag(s_eventFlag);
	if (ret != 0)
		sce_paf_printf_dummy();
}

/* (0x81159FF6) */
void imposePsMenuEventClear(void)
{
	if (s_eventFlag != -1 && sceKernelClearEventFlag(s_eventFlag, 0xFFFFFFFF) != 0)
		sce_paf_printf_dummy();
}

/* (0x8115A01A) */
void imposePsMenuEventSetClose(void)
{
	if (s_eventFlag != -1)
		sceKernelSetEventFlag(s_eventFlag, PSMENU_EVENT_CLOSE);
}

/* (0x8115A034) */
void imposePsMenuEventSetVibration(SceUInt8 on)
{
	if (s_eventFlag == -1)
		return;

	if (on) {
		sceKernelClearEventFlag(s_eventFlag, ~PSMENU_EVENT_VIBRATION_OFF);
		sceKernelSetEventFlag(s_eventFlag, PSMENU_EVENT_VIBRATION_ON);
	} else {
		sceKernelClearEventFlag(s_eventFlag, ~PSMENU_EVENT_VIBRATION_ON);
		sceKernelSetEventFlag(s_eventFlag, PSMENU_EVENT_VIBRATION_OFF);
	}
}

/* (0x8115A080) */
void imposePsMenuEventSetWlan(bool on)
{
	if (s_eventFlag == -1)
		return;

	if (on) {
		sceKernelClearEventFlag(s_eventFlag, ~PSMENU_EVENT_WLAN_OFF);
		sceKernelSetEventFlag(s_eventFlag, PSMENU_EVENT_WLAN_ON);
	} else {
		sceKernelClearEventFlag(s_eventFlag, ~PSMENU_EVENT_WLAN_ON);
		sceKernelSetEventFlag(s_eventFlag, PSMENU_EVENT_WLAN_OFF);
	}
}

/* the flight mode done with an error: its timer stopped, the power let go (0x8115A0D0) */
static void imposeFlightModeErrorEnd(void)
{
	if (s_errorScene->m_map_t_listener.find(FLIGHT_MODE_ERROR_TIMER) !=
	    s_errorScene->m_map_t_listener.end())
		s_errorScene->KillIntervalEvent(FLIGHT_MODE_ERROR_TIMER);

	SceShellUtil_8F2F143D(0);
	scePowerCancelRequest();
	sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
}

static void imposeFlightModeCheckCallback(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data);
static SceInt32 imposeFlightModeBtCallback(SceUID notifyId, SceInt32 notifyCount,
					   SceInt32 notifyArg, void *pCommon);
static SceInt32 imposeFlightModeWlanCallback(SceUID notifyId, SceInt32 notifyCount,
					     SceInt32 notifyArg, void *pCommon);

/* (0x8115A13A) */
ImposeFlightModeJob::ImposeFlightModeJob(ui::Scene *scene, char on, SceInt32 dialog)
	: job::JobItem("SceImposeFlightModeJob", NULL), m_scene(scene), m_on(on),
	  m_dialog(dialog), m_btCallback(0), m_wlanCallback(0),
	  m_mutex("SceImposeFlightModeWait", 0), m_wait(m_mutex, "SceImposeFlightModeWait")
{
	if (scene != NULL)
		scene->SetIntervalEvent(FLIGHT_MODE_CALLBACK_TIMER, FLIGHT_MODE_CALLBACK_INTERVAL,
					imposeFlightModeCheckCallback);

	m_btCallback = sceKernelCreateCallback("SceImposeBtDriverEventCallback", 0,
					       imposeFlightModeBtCallback, this);
	sceBtRegisterCallback(m_btCallback, FLIGHT_MODE_BT_EVENT_MASK);
	m_wlanCallback = sceKernelCreateCallback("SceImposeWlanDriverEventCallback", 0,
						 imposeFlightModeWlanCallback, this);
	sceWlanRegisterCallback(m_wlanCallback);
}

/* (0x8115A248) */
ImposeFlightModeJob::~ImposeFlightModeJob()
{
	if (m_scene->m_map_t_listener.find(FLIGHT_MODE_CALLBACK_TIMER) !=
	    m_scene->m_map_t_listener.end())
		m_scene->KillIntervalEvent(FLIGHT_MODE_CALLBACK_TIMER);

	sceBtUnregisterCallback(m_btCallback);
	sceKernelDeleteCallback(m_btCallback);
	m_btCallback = 0;
	sceWlanUnregisterCallback();
	sceKernelDeleteCallback(m_wlanCallback);
	m_wlanCallback = 0;
	m_scene = NULL;
	m_dialog = -1;
}

/* the drivers' callbacks run in the scene's thread (0x8115A308) */
static void imposeFlightModeCheckCallback(int32_t type, ui::Handler *self, ui::Event *e,
					  void *data)
{
	sceKernelCheckCallback();
}

/* the Bluetooth driver's events: the radio switched or failed (0x8115A310) */
static SceInt32 imposeFlightModeBtCallback(SceUID notifyId, SceInt32 notifyCount,
					   SceInt32 notifyArg, void *pCommon)
{
	ImposeFlightModeJob *job = (ImposeFlightModeJob *)pCommon;
	SceBtEvent events[FLIGHT_MODE_BT_EVENTS];
	int count;

	do {
		job->m_mutex.Lock();
		count = sceBtReadEvent(events, FLIGHT_MODE_BT_EVENTS);
		SceBtEvent *event = events;

		for (int i = 0; i < count; i++, event++) {
			if (event->id == FLIGHT_MODE_BT_EVENT_POWER) {
				SceUInt32 argument = event->argument;

				if ((argument & 0x40) != 0) {
					job->m_wait.Notify(sceBtGetLastError());
					break;
				}
				if ((argument & 0x2) != 0 && (argument & 0x20) == 0) {
					job->m_wait.Notify(0);
					ShellPluginInterface *indicator = (ShellPluginInterface *)
						applauncherPluginGetInterface("indicator_plugin", 1);

					((void (*)(int))indicator->func[FLIGHT_MODE_INDICATOR_BLUETOOTH])(0);
					break;
				}
				if ((argument & 0x12) == 0) {
					job->m_wait.Notify(0);
					ShellPluginInterface *indicator = (ShellPluginInterface *)
						applauncherPluginGetInterface("indicator_plugin", 1);

					((void (*)(int))indicator->func[FLIGHT_MODE_INDICATOR_BLUETOOTH])(1);
					break;
				}
			} else if (event->id == FLIGHT_MODE_BT_EVENT_CONFIG) {
				int config = sceBtGetConfiguration();

				if (config < 0) {
					job->m_wait.Notify(config);
					break;
				}
				if ((config & 1) != 0) {
					job->m_wait.Notify(sceBtGetLastError());
					break;
				}
			}
		}
		job->m_mutex.Unlock();
	} while (count >= FLIGHT_MODE_BT_EVENTS || count == FLIGHT_MODE_BT_READ_AGAIN);

	return 0;
}

/* the Wi-Fi driver's events (0x8115A42C) */
static SceInt32 imposeFlightModeWlanCallback(SceUID notifyId, SceInt32 notifyCount,
					     SceInt32 notifyArg, void *pCommon)
{
	ImposeFlightModeJob *job = (ImposeFlightModeJob *)pCommon;

	job->m_mutex.Lock();
	if ((notifyArg & 0x40) != 0)
		job->m_wait.Notify(-1);
	else if ((notifyArg & 0x2) != 0 && (notifyArg & 0x8) == 0)
		job->m_wait.Notify(0);
	else if ((notifyArg & 0x2) == 0)
		job->m_wait.Notify(0);
	job->m_mutex.Unlock();

	return 0;
}

/* Bluetooth, then Wi-Fi, then the modem switched (0x8115A488) */
int ImposeFlightModeJob::Run()
{
	int config;
	int request;
	int ret;
	bool wait;

	m_mutex.Lock();
	config = sceBtGetConfiguration();
	if (config < 0) {
		m_mutex.Unlock();
		return config;
	}
	request = m_on == '1' ? FLIGHT_MODE_RADIO_FLIGHT_ON : FLIGHT_MODE_RADIO_FLIGHT_OFF;
	ret = sceBtSetConfiguration(request);
	if (ret < 0) {
		m_mutex.Unlock();
		return ret;
	}
	wait = false;
	if (request == FLIGHT_MODE_RADIO_FLIGHT_ON)
		wait = (config & 0x9) != 0;
	if ((request == FLIGHT_MODE_RADIO_FLIGHT_OFF && (config & 0x80) != 0) || wait) {
		if (m_wait.Wait(FLIGHT_MODE_DRIVER_TIMEOUT) != 0) {
			m_mutex.Unlock();
			return FLIGHT_MODE_ERROR_BT_TIMEOUT;
		}
		config = m_wait.m_result;
		if (config < 0) {
			m_mutex.Unlock();
			return config;
		}
	}
	m_mutex.Unlock();

	m_mutex.Lock();
	ret = sceWlanSetConfiguration(m_on == '1' ? FLIGHT_MODE_RADIO_FLIGHT_ON :
				      FLIGHT_MODE_RADIO_FLIGHT_OFF);
	if (ret != 0) {
		m_mutex.Unlock();
		return ret;
	}
	if (m_wait.Wait(FLIGHT_MODE_DRIVER_TIMEOUT) != 0) {
		sceWlanGetConfiguration();
		m_mutex.Unlock();
		return FLIGHT_MODE_ERROR_WLAN_TIMEOUT;
	}
	if (m_wait.m_result != 0) {
		m_mutex.Unlock();
		return FLIGHT_MODE_ERROR_WLAN;
	}
	m_mutex.Unlock();

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)
		applauncherPluginGetInterface("telephony_plugin", 1);

	if (iface != NULL && iface->getModem() != NULL) {
		TelephonyModem *modem = iface->getModem();

		if (modem->vtable->needsInitialCheck(modem) != 0) {
			bool on = m_on == '1';
			TelephonyPluginInterface *telephony = (TelephonyPluginInterface *)
				applauncherPluginGetInterface("telephony_plugin", 1);

			if (telephony != NULL && telephony->getService() != NULL) {
				TelephonyService *service = telephony->getService();

				service->vtable->setFlightMode(service, on ? 1 : 0).GetCode();
			}
		}
	}

	return 0;
}

/*
 * The flight mode's error shown (PLACEHOLDER names): a job waits, the shell's menus locked,
 * until the shell is ready for the dialog, which closes itself after a while.
 */
class ImposeFlightModeErrorJob : public job::JobItem {
public:
	ImposeFlightModeErrorJob(ui::Scene *scene, SceInt32 error)
		: job::JobItem("SceImposeFlightModeErrorJob", NULL), m_scene(scene), m_error(error),
		  m_mutex("SceImposeFlightModeError", 0), m_wait(m_mutex, "SceImposeFlightModeError")
	{
	}

	virtual ~ImposeFlightModeErrorJob()
	{
	}

	virtual int Run();
	virtual void Finish(int result);

	ui::Scene *m_scene;
	SceInt32 m_error;
	thread::Mutex m_mutex;
	ImposeFlightModeWait m_wait;
};

class ImposeFlightModeErrorDialog {
public:
	ImposeFlightModeErrorDialog(ui::Scene *scene, SceInt32 error);
};

/* the dialog's OK closes it */
class ImposeFlightModeErrorHandler : public CommonGuiDialogHandler {
public:
	ImposeFlightModeErrorHandler(ImposeFlightModeErrorDialog *dialog) : m_dialog(dialog)
	{
	}

	virtual void operator()(SceInt32 value, SceInt32 button);

	ImposeFlightModeErrorDialog *m_dialog;
};

/* (0x8115A622) */
void ImposeFlightModeJob::Finish(int result)
{
	if (m_dialog >= 0) {
		SceCommonGuiDialog_BC311DC0(m_dialog);
		m_dialog = -1;
	}
	inputdevice::Unlock(0x7F);

	if (result == 0) {
		scePowerCancelRequest();
		sceKernelPowerUnlock(SCE_KERNEL_POWER_TICK_DEFAULT);
	} else {
		common::SharedPtr<job::JobItem> job(new ImposeFlightModeErrorJob(m_scene, result));

		if (job.get() != NULL)
			job::JobQueue::default_queue->Enqueue(job);
	}
}

/* the shell's answer to the lock (0x8115A776) */
static void imposeFlightModeErrorEvent(int result, int event, int param, void *pUserData)
{
	if (event == FLIGHT_MODE_ERROR_EVENT_CLOSED)
		((ImposeFlightModeErrorJob *)pUserData)->m_wait.Notify(result);
}

/* (0x8115A788) */
int ImposeFlightModeErrorJob::Run()
{
	m_mutex.Lock();
	sceShellUtilRegisterEventHandler(imposeFlightModeErrorEvent, this);
	sceShellUtilLock(FLIGHT_MODE_ERROR_LOCK);
	m_wait.Wait(0);
	sceShellUtilRegisterEventHandler(NULL, NULL);
	m_mutex.Unlock();

	return 0;
}

static void imposeFlightModeErrorTimeout(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data);

/* (0x8115A7C6) */
ImposeFlightModeErrorDialog::ImposeFlightModeErrorDialog(ui::Scene *scene, SceInt32 error)
{
	s_errorScene = scene;
	s_errorDialog = -1;

	CommonGuiMessageDialogParam param;

	param.resourcePlugin = Plugin::Find("impose_plugin");
	param.handler = new ImposeFlightModeErrorHandler(this);
	param.type = error;
	param.buttonCount = 1;
	param.defaultButton = 1;
	param.cancelButton = 2;
	param.messageId = FLIGHT_MODE_MSG_ERROR;
	param.useLayout = true;
	param.layout = SceCommonGuiDialog_B04C8C1A;
	param.priority = 7;
	s_errorDialog = SceCommonGuiDialog_20B141F1(&param);

	s_errorScene->SetIntervalEvent(FLIGHT_MODE_ERROR_TIMER, FLIGHT_MODE_ERROR_TIMEOUT,
				       imposeFlightModeErrorTimeout);
}

/* (0x8115A99A) */
void ImposeFlightModeErrorJob::Finish(int result)
{
	new ImposeFlightModeErrorDialog(m_scene, m_error);
}

/* the dialog closed after a while (0x8115A9B2) */
static void imposeFlightModeErrorTimeout(int32_t type, ui::Handler *self, ui::Event *e,
					 void *data)
{
	SceCommonGuiDialog_BC311DC0(s_errorDialog);
	s_errorDialog = -1;
	imposeFlightModeErrorEnd();
}

/* (0x8115A9CE) */
void ImposeFlightModeErrorHandler::operator()(SceInt32 value, SceInt32 button)
{
	SceCommonGuiDialog_BC311DC0(value);
	imposeFlightModeErrorEnd();
}

static void imposeAppEvent(int code, const void *data, SceSize size, void *arg);

/* (0x8115A9DE) */
void imposePsMenuAppEventAdd(void)
{
	SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, imposeAppEvent, NULL);
	SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, imposeAppEvent, NULL);
}

/* (0x8115A9FE) */
void imposePsMenuAppEventRemove(void)
{
	SceShell_E3D3A162(SHELL_APP_EVENT_DEACTIVATED, imposeAppEvent, NULL);
	SceShell_E3D3A162(SHELL_APP_EVENT_ACTIVATED, imposeAppEvent, NULL);
}

/* the settings application quit, to show the switches' new state */
static inline void imposeQuitSettings(void)
{
	SceAppMgrAppStatus status;

	if (sceAppMgrGetStatusByName("NPXS10015", &status) == 0)
		sceAppMgrQuitApp(status.appId);
}

/* (0x8115AA1E) */
bool imposeSetFlightMode(bool on)
{
	if (sceRegMgrSetKeyInt("/CONFIG/SYSTEM", "flight_mode", on ? 1 : 0) != 0)
		return false;

	imposeQuitSettings();
	return true;
}

/* (0x8115AA80) */
bool imposeWlanIsLocked(void)
{
	return (sceWlanGetConfiguration() & 0x200) != 0;
}

/* (0x8115AA94) */
bool imposeSetWlan(bool on)
{
	if (imposeWlanIsLocked() || sceWlanSetConfiguration(on ? 1 : 0) != 0)
		return false;

	imposeQuitSettings();
	return sceRegMgrSetKeyInt("/CONFIG/NET", "wifi_flag", on ? 1 : 0) == 0;
}

/* (0x8115AB10) */
bool imposeHasMobile(void)
{
	int state = 0;
	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)
		applauncherPluginGetInterface("telephony_plugin", 1);

	if (iface != NULL && iface->getModem() != NULL) {
		TelephonyModem *modem = iface->getModem();

		state = modem->vtable->needsInitialCheck(modem);
	}

	return state == 1;
}

/* (0x8115AB46) */
bool imposeMobileIsLocked(void)
{
	return (sceWlanGetConfiguration() & 0x400) != 0;
}

/* (0x8115AB5A) */
bool imposeSetMobile(bool on)
{
	if (imposeMobileIsLocked() || !imposeHasMobile())
		return false;

	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)
		applauncherPluginGetInterface("telephony_plugin", 1);

	if (iface != NULL && iface->getService() != NULL) {
		TelephonyService *service = iface->getService();

		service->vtable->setDataEnabled(service, on).GetCode();
	}

	imposeQuitSettings();
	return true;
}

/* (0x8115AC0C) */
bool imposeSetBluetooth(bool on)
{
	if (sceBtSetConfiguration(on ? 1 : 0) < 0)
		return false;

	imposeQuitSettings();
	return sceRegMgrSetKeyInt("/CONFIG/BT", "bt_enable", on ? 1 : 0) == 0;
}

/* (0x8115AC7C) */
bool imposeIsFlightMode(void)
{
	int value = 0;

	sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "flight_mode", &value);
	return value == 1;
}

/* (0x8115ACC2) */
bool imposeIsWlanOn(void)
{
	return (sceWlanGetConfiguration() & 0x9) != 0;
}

/* (0x8115ACD6) */
bool imposeIsMobileOn(void)
{
	if (!imposeHasMobile())
		return false;

	SceUInt8 enabled = 0;
	SceUInt8 flightMode = 0;
	TelephonyPluginInterface *iface = (TelephonyPluginInterface *)
		applauncherPluginGetInterface("telephony_plugin", 1);

	if (iface != NULL && iface->getService() != NULL) {
		TelephonyService *service = iface->getService();
		TelephonyResult result = service->vtable->getDataEnabled(service, &enabled);

		result.GetCode();
		service = iface->getService();
		result = service->vtable->getFlightMode(service, &flightMode);
		result.GetCode();
	}

	return enabled != 0 && flightMode == 0;
}

/* (0x8115AD9C) */
bool imposeIsBluetoothOn(void)
{
	return (sceBtGetConfiguration() & 0x9) != 0;
}

/* (0x8115ADB0) */
bool imposeSwitchesUsable(void)
{
	return s_switchesUsable;
}

/* the switches unusable while an application of the list is in front (0x8115ADBC) */
static void imposeAppEvent(int code, const void *data, SceSize size, void *arg)
{
	const ShellAppEvent *event = (const ShellAppEvent *)data;

	for (SceUInt32 i = 0; i < 21; i++) {
		if (sce_paf_strcmp(event->titleId, s_appNames[i]) == 0) {
			switch (code) {
			case SHELL_APP_EVENT_ACTIVATED:
				s_switchesUsable = false;
				break;
			case SHELL_APP_EVENT_DEACTIVATED:
				s_switchesUsable = true;
				break;
			}
			return;
		}
	}
}

/* the incoming dialog server's errors (PLACEHOLDER names) */
#define INCOMING_DIALOG_ERROR_NOT_READY		((int)0x80106200)
#define INCOMING_DIALOG_ERROR_NOT_INITIALIZED	((int)0x80106201)
#define INCOMING_DIALOG_ERROR_BUSY		((int)0x80106203)
#define INCOMING_DIALOG_ERROR_ALREADY_OPEN	((int)0x80106204)

/* a request's states (PLACEHOLDER names); a request done keeps its result */
#define INCOMING_DIALOG_STATE_NONE		0
#define INCOMING_DIALOG_STATE_PENDING		1
#define INCOMING_DIALOG_STATE_TAKEN		2

/* the dialog's time on screen when the application gives none, in seconds */
#define INCOMING_DIALOG_DEFAULT_TIMEOUT		10

/* the launch argument's room */
#define INCOMING_DIALOG_PARAM_SIZE		960

/* topmenu_plugin's function that brings the application's dialog to the front (PLACEHOLDER
   name) */
#define TOPMENU_PLUGIN_FUNC_SWITCH_TO_DIALOG	63

/* the session's process, or the shell's own without one */
static inline SceUID incomingDialogProcessId(IPMI::Session *session)
{
	return session != NULL ? session->getProcessId() : sceKernelGetProcessId();
}

/* the process's request, or NULL */
static inline ShellIncomingDialogRequest *incomingDialogFind(ShellIncomingDialogServer *server,
							       SceUID processId)
{
	for (SceUInt32 i = 0; i < 8; i++) {
		if (server->requests[i].processId == processId)
			return &server->requests[i];
	}
	return NULL;
}

/* an application's request made (sceIncomingDialogInitialize) (0x8115AE10) */
int shellIncomingDialogServerInitialize(IPMI::Session *session)
{
	ShellIncomingDialogServer *server = s_incomingDialogServer;

	if (server == NULL)
		return INCOMING_DIALOG_ERROR_NOT_READY;

	SceUID processId = incomingDialogProcessId(session);

	server->mutex.Lock();
	if (incomingDialogFind(server, processId) != NULL) {
		server->mutex.Unlock();
		return INCOMING_DIALOG_ERROR_BUSY;
	}

	ShellIncomingDialogRequest *request = incomingDialogFind(server, -1);

	if (request->param != NULL)
		delete[] request->param;
	sce_paf_memset(request, 0, sizeof(*request));
	request->state = INCOMING_DIALOG_STATE_NONE;
	request->processId = processId;
	server->mutex.Unlock();
	return 0;
}

/* (0x8115AEBC) */
int shellIncomingDialogServerInitializeSelf(void)
{
	return shellIncomingDialogServerInitialize(NULL);
}

/* an application's request freed (sceIncomingDialogFinish) (0x8115AEC6) */
int shellIncomingDialogServerFinish(IPMI::Session *session)
{
	ShellIncomingDialogServer *server = s_incomingDialogServer;

	if (server == NULL)
		return INCOMING_DIALOG_ERROR_NOT_READY;

	SceUID processId = incomingDialogProcessId(session);

	server->mutex.Lock();

	ShellIncomingDialogRequest *request = incomingDialogFind(server, processId);

	if (request == NULL) {
		server->mutex.Unlock();
		return INCOMING_DIALOG_ERROR_NOT_INITIALIZED;
	}
	if (request->state == INCOMING_DIALOG_STATE_TAKEN) {
		server->mutex.Unlock();
		return INCOMING_DIALOG_ERROR_BUSY;
	}

	if (request->param != NULL)
		delete[] request->param;
	sce_paf_memset(request, 0, sizeof(*request));
	request->state = INCOMING_DIALOG_STATE_NONE;
	request->processId = -1;
	server->mutex.Unlock();
	return 0;
}

/* (0x8115AF70) */
int shellIncomingDialogServerFinishSelf(void)
{
	return shellIncomingDialogServerFinish(NULL);
}

/* an application's dialog asked for (sceIncomingDialogOpen) (0x8115AF7A) */
int shellIncomingDialogServerOpen(IPMI::Session *session, const SceIncomingDialogParam *dialogParam,
				  const char *param)
{
	ShellIncomingDialogServer *server = s_incomingDialogServer;

	if (server == NULL)
		return INCOMING_DIALOG_ERROR_NOT_READY;

	SceUID processId = incomingDialogProcessId(session);

	server->mutex.Lock();

	ShellIncomingDialogRequest *request = incomingDialogFind(server, processId);

	if (request == NULL) {
		server->mutex.Unlock();
		return INCOMING_DIALOG_ERROR_NOT_INITIALIZED;
	}

	switch (request->state) {
	case INCOMING_DIALOG_STATE_PENDING:
	case INCOMING_DIALOG_STATE_TAKEN:
		server->mutex.Unlock();
		return INCOMING_DIALOG_ERROR_ALREADY_OPEN;
	case INCOMING_DIALOG_STATE_NONE:
	case 3:
	case 4:
	case 5:
	case 6:
		request->canceled = false;
		request->startTime = 0;
		*static_cast<SceIncomingDialogParam *>(request) = *dialogParam;
		request->state = INCOMING_DIALOG_STATE_PENDING;
		if (param != NULL && *param != '\0') {
			request->param = new char[INCOMING_DIALOG_PARAM_SIZE];
			if (request->param != NULL) {
				sce_paf_strncpy(request->param, param, INCOMING_DIALOG_PARAM_SIZE);
				request->param[INCOMING_DIALOG_PARAM_SIZE - 1] = '\0';
			}
		}
		if (request->timeout == 0)
			request->timeout = INCOMING_DIALOG_DEFAULT_TIMEOUT;
		request->sequence = ++server->sequence;
		server->mutex.Unlock();
		return 0;
	default:
		server->mutex.Unlock();
		return INCOMING_DIALOG_ERROR_NOT_READY;
	}
}

/* (0x8115B0A8) */
int shellIncomingDialogServerOpenSelf(const SceIncomingDialogParam *dialogParam)
{
	return shellIncomingDialogServerOpen(NULL, dialogParam, NULL);
}

/* an application's dialog closed (sceIncomingDialogClose) (0x8115B0B6) */
int shellIncomingDialogServerClose(IPMI::Session *session)
{
	ShellIncomingDialogServer *server = s_incomingDialogServer;

	if (server == NULL)
		return INCOMING_DIALOG_ERROR_NOT_READY;

	SceUID processId = incomingDialogProcessId(session);

	server->mutex.Lock();

	ShellIncomingDialogRequest *request = incomingDialogFind(server, processId);

	if (request == NULL) {
		server->mutex.Unlock();
		return INCOMING_DIALOG_ERROR_NOT_INITIALIZED;
	}

	request->canceled = true;
	server->mutex.Unlock();
	return 0;
}

/* an application's request's state (sceIncomingDialogGetStatus) (0x8115B12A) */
int shellIncomingDialogServerGetStatus(IPMI::Session *session, SceInt32 *state)
{
	ShellIncomingDialogServer *server = s_incomingDialogServer;

	if (server == NULL)
		return INCOMING_DIALOG_ERROR_NOT_READY;

	SceUID processId = incomingDialogProcessId(session);

	server->mutex.Lock();

	ShellIncomingDialogRequest *request = incomingDialogFind(server, processId);

	if (request == NULL) {
		server->mutex.Unlock();
		return 0;
	}

	*state = request->state;
	server->mutex.Unlock();
	return 0;
}

/* (0x8115B19A) */
SceInt32 shellIncomingDialogServerGetStatusSelf(void)
{
	SceInt32 state = 0;

	shellIncomingDialogServerGetStatus(NULL, &state);
	return state;
}

/* (0x8115B1CA) */
void shellIncomingDialogServerCreate(void)
{
	s_incomingDialogServer = new ShellIncomingDialogServer;
}

/* (0x8115B228) */
ShellIncomingDialogRequest *shellIncomingDialogServerTakeRequest(void)
{
	ShellIncomingDialogServer *server = s_incomingDialogServer;

	if (server == NULL)
		return NULL;

	server->mutex.Lock();

	ShellIncomingDialogRequest *oldest = NULL;

	for (SceUInt32 i = 0; i < 8; i++) {
		if (server->requests[i].processId != -1 &&
		    server->requests[i].state == INCOMING_DIALOG_STATE_PENDING &&
		    (oldest == NULL || oldest->sequence > server->requests[i].sequence))
			oldest = &server->requests[i];
	}
	if (oldest != NULL) {
		oldest->taken = true;
		oldest->state = INCOMING_DIALOG_STATE_TAKEN;
	}
	server->mutex.Unlock();
	return oldest;
}

/* (0x8115B294) */
void shellIncomingDialogServerReleaseRequest(ShellIncomingDialogRequest *request, int result)
{
	ShellIncomingDialogServer *server = s_incomingDialogServer;

	if (server == NULL)
		return;

	server->mutex.Lock();

	bool freeOnRelease = request->freeOnRelease;

	request->state = result;
	request->taken = false;
	if (freeOnRelease) {
		if (request->param != NULL)
			delete[] request->param;
		sce_paf_memset(request, 0, sizeof(*request));
		request->processId = -1;
		request->state = INCOMING_DIALOG_STATE_NONE;
	}
	server->mutex.Unlock();
}

/* the application's dialog brought to the front, in the main thread (0x8115B2E8) */
static void incomingDialogSwitchToDialog(void *arg)
{
	SceAppMgrAppStatus status;

	if (sceAppMgrGetStatusByAppId((int)arg, &status) == 0) {
		Plugin *plugin = Plugin::Find("topmenu_plugin");

		if (plugin != NULL) {
			ShellPluginInterface *topmenu = (ShellPluginInterface *)plugin->GetInterface(1);

			((void (*)(const char *))topmenu->func[TOPMENU_PLUGIN_FUNC_SWITCH_TO_DIALOG])(
				status.appName);
		}
	}
}

/* (sceIncomingDialogSwitchToDialog) (0x8115B332) */
int shellIncomingDialogServerSwitchToDialog(IPMI::Session *session)
{
	if (s_incomingDialogServer != NULL) {
		thread::CallQueue::main_thread_callqueue.Enqueue("SceIncomingDialogServer",
								 incomingDialogSwitchToDialog,
								 (void *)session->getAppId());
		return 0;
	}

	return INCOMING_DIALOG_ERROR_NOT_READY;
}

/* the methods 0x170000-0x170005, and the asynchronous ones, which it does not have, in the next
   file (0x8115B498, 0x8115B554; PLACEHOLDER names) */
int shellIncomingDialogServerSyncDispatch(IPMI::Session *session, SceUInt32 methodId,
					  const IPMI::DataInfo *inList, SceUInt32 inCount,
					  IPMI::BufferInfo *outList, SceUInt32 outCount);
int shellIncomingDialogServerAsyncDispatch(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *inList,
					   SceUInt32 inCount);

/* the server's handlers in the shell's table of them (0x81511F44) */

/* (0x8115B37A) */
int shellIncomingDialogServerOnSyncMethod(IPMI::Session *session, SceUInt32 methodId,
					  const IPMI::DataInfo *inList, SceUInt32 inCount,
					  IPMI::BufferInfo *outList, SceUInt32 outCount)
{
	return shellIncomingDialogServerSyncDispatch(session, methodId, inList, inCount, outList,
						     outCount);
}

/* (0x8115B38E) */
int shellIncomingDialogServerOnAsyncMethod(IPMI::Session *session, SceUInt32 requestId,
					   SceUInt32 methodId, const IPMI::DataInfo *inList,
					   SceUInt32 inCount)
{
	return shellIncomingDialogServerAsyncDispatch(session, requestId, methodId, inList,
						      inCount);
}

/* the application gone: its request freed, or cancelled while the dialog shows it
   (0x8115B39E) */
void shellIncomingDialogServerOnSessionClosed(IPMI::Session *session)
{
	ShellIncomingDialogServer *server = s_incomingDialogServer;

	if (server == NULL)
		return;

	SceUID processId = incomingDialogProcessId(session);

	server->mutex.Lock();

	ShellIncomingDialogRequest *request = incomingDialogFind(server, processId);

	if (request != NULL) {
		bool taken = request->taken;

		request->freeOnRelease = true;
		if (taken) {
			request->canceled = true;
		} else {
			if (request->param != NULL)
				delete[] request->param;
			sce_paf_memset(request, 0, sizeof(*request));
			request->state = INCOMING_DIALOG_STATE_NONE;
			request->processId = -1;
		}
	}
	server->mutex.Unlock();
}
