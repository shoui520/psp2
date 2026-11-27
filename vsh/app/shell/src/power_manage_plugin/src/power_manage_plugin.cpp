/*
 * power_manage_plugin.cpp: SceShell power_manage_plugin lifecycle, power/USB/controller/network
 * callbacks, screen state machine, and PowerManageEventServer.
 * 3.65-CEX: 0x81234950 - 0x81236C74
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <wchar.h>
#include <kernel/processmgr.h>
#include <kernel/threadmgr.h>
#include <appmgr/appmgr.h>
#include <av_config/av_config.h>
#include <ctrl/ctrl.h>
#include <power/power.h>
extern "C" {
#include <usbd/usbd.h>
}
#include <vshbridge/vshbridge.h>
#include <common_gui_dialog/common_gui_dialog.h>
#include <framework.h>
#include <plugin.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_display.h>
#include <system/system.h>
#include <thread/thread.h>
#include <widget/w_plane.h>

#include "shell.h"
#include "plugins.h"
#include "boot.h"
#include "bgdl_api.h"
#include "src/livespace_db/lsdb_keys.h"
#include "src/netctl/netctl_internal.h"
#include "power_manage_internal.h"

extern "C" {
extern int sceKernelCreateCallback(const char *name, SceUInt32 attr,
				   SceKernelCallbackFunction func, void *userData);
extern int sceKernelDeleteCallback(SceUID cbid);
extern SceUID sceKernelGetProcessId(void);
extern SceUInt64 sceKernelGetProcessTimeWide(void);
extern int sceAppMgrLaunchAppByNameForShell(SceUInt32 flags, const char *name, const char *param);
extern int sceAppMgrRestoreBgmSettingForShell(void);
extern int sceAppMgrRestoreDisplaySettingForShell(void);
extern int sceAppMgrSetPowerSaveMode(int enable, int suspendApps);
extern int sceWlanGetWakeOnTargetProcess(void);
extern int vshCtrlRegisterNotifyCallBack(SceUID cbid);
extern int vshCtrlUnregisterNotifyCallBack(SceUID cbid);
extern int vshCtrlGetActiveControllerPort(void);
extern int vshCtrlIsBdRemoconConnected(void);
}

extern int consoleInfoFileWriteInt64(int index, SceUInt64 value, int mode);

namespace power_manage {

PowerManage *g_powerManage = NULL;
paf::Plugin *g_powerManagePlugin = NULL;

static PowerManageInterface s_powerManageInterface;
static AuthInterface s_authInterface;

static const SceUInt32 kPowerManagePageHash = 0xB408FB0BU;	/* -1274463157 */
static const SceUInt32 kNoticeIconHash = 0x2617665DU;		/* 639083309 */
static const SceUInt32 kLowBatteryTextureHash = 0x14FAB6DEU;	/* 351975134 */
static const SceUInt32 kHighTempMessageHash = 0x816BA07FU;	/* -2123652993 */
static const SceUInt32 kPowerNoticeHash = 0x13BD7346U;		/* 331182918 */
static const SceUInt32 kUsbNoticeHash = 0xD827255BU;		/* -668534053 */
static const SceUInt32 kCtrlNoticeHash = 0xD38E2C5AU;		/* -745669414 */
static const SceUInt32 kWirelessLowBattHash = 0x63D772FDU;	/* 1675029245 */
static const SceUInt32 kNoControllerNoticeHash = 0x9110E988U;	/* -1861189080 */

static const SceInt32 kTimerIdControllerCheck = 0x01000000;
static const SceInt32 kTimerIdIduShutdown = 0x01000001;
static const SceInt32 kTimerIdPeriodic = 0x02000000;

static const int kPowerSwModeHold = 2;
static const int kPowerSwModeNormal = 4;
static const int kPowerWakeFactorRtcAlarm = 14;
static const int kPowerWakeFactorPowerOff = 23;

static const int kTopmenuSlotHideForLock = 74;		/* +0x128 (296) */
static const int kTopmenuSlotUnlockComplete = 75;	/* +0x12C (300) */
static const int kTopmenuSlotOverlayBegin = 52;		/* +0xD0 (208) */
static const int kTopmenuSlotOverlayEnd = 53;		/* +0xD4 (212) */

static const int kImposeSlotOpenPsMenu = 3;		/* +0x0C (12) */
static const int kImposeSlotClosePsMenu = 4;		/* +0x10 (16) */
static const int kImposeSlotScreenshot = 6;		/* +0x18 (24) */

static void powerManageNoop(void)
{
}

static void onIduShutdownTimer(int32_t eventType, paf::ui::Handler *handler,
			       paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	shellShutdownRequest(0, 0);
}

static void onCheckControllerConnected(int32_t eventType, paf::ui::Handler *handler,
				       paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;
	if (vshCtrlGetActiveControllerPort() == 0 && vshCtrlIsBdRemoconConnected() == 0) {
		g_powerManage->ShowImposeNotice(kNoControllerNoticeHash, L"");
		g_powerManage->m_noControllerNoticeShown = true;
	}
}

static void onPeriodicTimer(int32_t eventType, paf::ui::Handler *handler,
			    paf::ui::Event *event, void *userData)
{
	(void)eventType;
	(void)handler;
	(void)event;
	(void)userData;

	if (shellStartupGetIduMode() == 1)
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);

	if (paf::system::Capability::HasWirelessController()) {
		int activeMask = vshCtrlGetActiveControllerPort();
		for (int port = 1; port <= 4; ++port) {
			SceUInt32 portBit = (SceUInt32)(1 << port);
			bool isLow = false;
			if ((activeMask & (int)portBit) != 0) {
				SceUInt8 batteryLevel = 0;
				if (sceCtrlGetBatteryInfo(port, &batteryLevel) == 0 &&
				    batteryLevel != 0 && batteryLevel <= 2) {
					isLow = true;
				}
			}
			if (isLow) {
				if ((g_powerManage->m_lowBatteryControllerMask & portBit) == 0) {
					g_powerManage->m_lowBatteryControllerMask |= portBit;
					wchar_t portLabel[4];
					sce_paf_swprintf(portLabel, 4, L"%d", port);
					g_powerManage->ShowImposeNotice(kWirelessLowBattHash,
									portLabel);
				}
			} else if ((g_powerManage->m_lowBatteryControllerMask & portBit) != 0) {
				g_powerManage->m_lowBatteryControllerMask &= ~portBit;
			}
		}

		if (g_powerManage->m_noControllerNoticeShown &&
		    (activeMask != 0 || vshCtrlIsBdRemoconConnected() != 0)) {
			paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
			if (imposePlugin != NULL) {
				ImposePluginInterface *imposeIface =
					(ImposePluginInterface *)imposePlugin->GetInterface(1);
				if (imposeIface != NULL) {
					char noticeName[32];
					sce_paf_snprintf(noticeName, sizeof(noticeName),
							 "power_manage_plugin_%08x",
							 kNoControllerNoticeHash);
					imposeIface->removeNotice(noticeName);
				}
			}
			g_powerManage->m_noControllerNoticeShown = false;
		}
	}

	shellRtcUtilSync(false);
	shellParentalControlUpdate(!g_powerManage->m_isSuspended, false);
}

static void onProcessEvent(int eventCode, const void *eventBuf, SceSize eventSize,
			   void *userData)
{
	(void)eventCode;
	(void)eventSize;
	(void)userData;
	const ShellAppEvent *appEvent = (const ShellAppEvent *)eventBuf;
	if (appEvent == NULL)
		return;

	if (PowerManage::IsWakeAppName(appEvent->titleId) == 0)
		return;

	g_powerManage->m_wakeAppName = "";
	sceAppMgrSetPowerSaveMode(1, 1);
	shellHomeUpdateRequest(2);
	shellHomeUpdateRequest(0);
	g_powerManage->m_powerSaveMode = true;

	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	if (topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIface =
			(ShellPluginInterface *)topmenuPlugin->GetInterface(1);
		if (topmenuIface != NULL)
			topmenuIface->func[kTopmenuSlotHideForLock]();
	}
	PowerManage::SetScreenState(SCREEN_STATE_KEYLOCK);
}

static void onLeaveIduMode(int eventCode, const void *eventBuf, SceSize eventSize,
			   void *userData)
{
	(void)eventCode;
	(void)eventBuf;
	(void)eventSize;
	(void)userData;
	g_powerManage->m_page->KillTimer(kTimerIdControllerCheck);
}

static int onPowerCallback(int notifyId, int notifyCount, int powerInfo, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)common;

	g_powerManage->SetIduTimerEnabled((powerInfo & 0x1000) != 0);

	if ((powerInfo & 0x860000) != 0) {
		if ((powerInfo & 0x20000) != 0)
			shellEventServerSend(13, NULL, 0);

		if (shellStartupGetIduMode() != 1) {
			if (!g_powerManage->m_pendingResumeDisplayOn) {
				bool staySuspended = false;
				bool skipKeyLock = false;

				if ((powerInfo & 0x60000) != 0) {
					g_powerManage->m_wakeAppName = "";
					g_powerManage->m_pendingLaunchApp = "";
					g_powerManage->m_pendingLaunchArg = "";

					if ((powerInfo & 0x400000) != 0) {
						staySuspended = true;
					} else if ((powerInfo & 0x20000) != 0) {
						g_powerManage->m_pendingResumeDisplayOn = true;
					}

					if ((vshPowerGetWakeupFactor() & 0x7F) == kPowerWakeFactorRtcAlarm) {
						skipKeyLock = true;
					} else {
						SceUID wakePid = sceWlanGetWakeOnTargetProcess();
						SceAppMgrAppStatus frontStatus;
						if (wakePid != -1 &&
						    sceAppMgrGetStatusByAppId(-4, &frontStatus) == 0 &&
						    wakePid == frontStatus.processId) {
							if (paf::system::Model::GetType() !=
							    paf::system::Model::Type_Dolce) {
								g_powerManage->m_wakeAppName =
									frontStatus.appName;
							}
							if (shellStartupGetIduMode() == 1 ||
							    shellStartupIsTestMode() ||
							    g_powerManage->m_powerSwHoldCount > 0) {
								sceKernelPowerTick(
									SCE_KERNEL_POWER_TICK_DEFAULT);
								paf::thread::Sleep(33);
							} else {
								vshPowerSetPowerSwMode(kPowerSwModeHold);
								sceKernelPowerTick(
									SCE_KERNEL_POWER_TICK_DEFAULT);
								paf::thread::Sleep(33);
								vshPowerSetPowerSwMode(
									kPowerSwModeNormal);
							}
						}
					}

					shellBgdlHold();
					if (g_powerManage->m_statusDialogId >= 0) {
						shellStateWatcherCloseDialog(
							g_powerManage->m_statusDialogId);
						g_powerManage->m_statusDialogId = -1;
					}
					g_powerManage->m_powerSaveMode = true;
				}

				if (!staySuspended) {
					if (shellBootIsLaunchRestricted() ||
					    !g_powerManage->m_wakeAppName.empty() ||
					    (paf::system::Model::GetType() ==
					     paf::system::Model::Type_Dolce &&
					     !g_powerManage->m_dolceLockEnabled)) {
						skipKeyLock = true;
					}

					if (paf::system::Capability::HasWirelessController()) {
						g_powerManage->m_page->SetTimerListener(
							kTimerIdControllerCheck, 3000.0f,
							new paf::ui::EventCBListener(
								onCheckControllerConnected, NULL));
					}
				}

				paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
				if (imposePlugin != NULL) {
					ShellPluginInterface *imposeIface =
						(ShellPluginInterface *)imposePlugin->GetInterface(1);
					if (imposeIface != NULL)
						imposeIface->func[kImposeSlotClosePsMenu]();
				}

				g_powerManage->m_powerMenu.Close();
				g_powerManage->SetSuspendedState(staySuspended);

				if (skipKeyLock) {
					PowerManage::TurnDisplayOnAndNotify();
					g_powerManage->UnlockToTopMenu();
					if (paf::system::Model::GetType() !=
					    paf::system::Model::Type_Dolce &&
					    shellBootHasAppStack()) {
						sceKernelPowerTick(
							SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
						sceAVConfigSetAutoSuspend2(600);
					}
				} else {
					paf::Plugin *topmenuPlugin =
						paf::Plugin::Find("topmenu_plugin");
					if (topmenuPlugin != NULL) {
						ShellPluginInterface *topmenuIface =
							(ShellPluginInterface *)
								topmenuPlugin->GetInterface(1);
						if (topmenuIface != NULL)
							topmenuIface->func[kTopmenuSlotHideForLock]();
					}
					PowerManage::SetScreenState(SCREEN_STATE_KEYLOCK);
				}
			}
			if ((powerInfo & 0x40000) != 0)
				g_powerManage->m_pendingResumeDisplayOn = false;
		}
		return 0;
	}

	if ((powerInfo & 0x10000) != 0) {
		if (shellStartupGetIduMode() != 1)
			shellBgdlHold();
		return 0;
	}

	if ((powerInfo & 0x400000) != 0) {
		if (shellStartupGetIduMode() == 1) {
			scePowerRequestDisplayOff();
			paf::thread::Sleep(33);
			scePowerRequestDisplayOn();
			sceAppMgrLaunchAppByName(0x20000, "NPXS10010", NULL);
		} else if (!g_powerManage->m_isSuspended) {
			g_powerManage->m_wakeAppName = "";
			g_powerManage->m_pendingLaunchApp = "";
			g_powerManage->m_pendingLaunchArg = "";

			paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
			if (imposePlugin != NULL) {
				ShellPluginInterface *imposeIface =
					(ShellPluginInterface *)imposePlugin->GetInterface(1);
				if (imposeIface != NULL)
					imposeIface->func[kImposeSlotClosePsMenu]();
			}

			g_powerManage->m_powerMenu.Close();
			g_powerManage->SetSuspendedState(true);

			bool suspendApps = (powerInfo < 0) || g_powerManage->m_powerSaveMode;
			sceAppMgrSetPowerSaveMode(1, suspendApps ? 1 : 0);
			if (suspendApps)
				shellHomeUpdateRequest(2);
			shellHomeUpdateRequest(0);
			g_powerManage->m_powerSaveMode = suspendApps;

			paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
			if (topmenuPlugin != NULL) {
				ShellPluginInterface *topmenuIface =
					(ShellPluginInterface *)topmenuPlugin->GetInterface(1);
				if (topmenuIface != NULL)
					topmenuIface->func[kTopmenuSlotHideForLock]();
			}
			PowerManage::SetScreenState(SCREEN_STATE_KEYLOCK);
		}
		return 0;
	}

	if ((powerInfo & 0x4000000) != 0) {
		if (shellStartupGetIduMode() != 1) {
			paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
			if (imposePlugin != NULL) {
				ShellPluginInterface *imposeIface =
					(ShellPluginInterface *)imposePlugin->GetInterface(1);
				if (imposeIface != NULL) {
					void (*takeShot)(int) = (void (*)(int))
						imposeIface->func[kImposeSlotScreenshot];
					takeShot(0);
				}
			}
		}
		return 0;
	}

	if ((powerInfo & 0x40000000) != 0) {
		if (shellStartupGetIduMode() != 1 && !shellStartupIsTestMode() &&
		    g_powerManage->m_powerButtonLockCount == 0) {
			if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce)
				shellShutdownRequest(0, 0);
			else
				g_powerManage->m_powerMenu.Open();
		}
		return 0;
	}

	if ((powerInfo & 0x10000000) != 0) {
		if (shellStartupGetIduMode() == 1)
			return 0;
		if (!shellStartupIsTestMode()) {
			if (g_powerManage->m_psButtonLockCount == 0) {
				paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
				if (imposePlugin != NULL) {
					ShellPluginInterface *imposeIface =
						(ShellPluginInterface *)
							imposePlugin->GetInterface(1);
					if (imposeIface != NULL)
						imposeIface->func[kImposeSlotOpenPsMenu]();
				}
			}
			return 0;
		}
		g_powerManage->m_eventServer.Dispatch(0);
		return 0;
	}

	if ((powerInfo & 0x20000000) != 0) {
		if (shellStartupIsTestMode())
			return 0;
		g_powerManage->m_eventServer.Dispatch(0);
		return 0;
	}

	if ((powerInfo & 0x400) != 0) {
		paf::math::v4 iconPos(0.0f, 0.0f, 0.0f, 0.0f);
		g_powerManage->OpenStatusDialog(SceCommonGuiDialog_7AEA192C, 0,
						kLowBatteryTextureHash, iconPos);
	} else if ((powerInfo & 0x200) != 0) {
		paf::math::v4 iconPos(0.0f, 0.0f, 0.0f, 0.0f);
		g_powerManage->OpenStatusDialog(SceCommonGuiDialog_52A53037,
						kHighTempMessageHash, 0, iconPos);
	} else if ((powerInfo & 0x800) != 0) {
		g_powerManage->ShowImposeNotice(kPowerNoticeHash, L"");
	}
	return 0;
}

static int onUsbCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)common;
	g_powerManage->ShowImposeNotice(kUsbNoticeHash, L"");
	return 0;
}

static int onCtrlCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)notifyArg;
	(void)common;
	g_powerManage->ShowImposeNotice(kCtrlNoticeHash, L"");
	return 0;
}

static void onNetCtlEvent(int prevState, int curState, int event, int errorCode, int eventFlags,
			  void *userData)
{
	(void)errorCode;
	(void)eventFlags;
	(void)userData;
	if (curState == 4 && (event == 6 || prevState == 5)) {
		if (!g_powerManage->m_netConnected) {
			g_powerManage->m_netConnected = true;
			if (!g_powerManage->m_isSuspended) {
				SceNetCtlInfo info;
				if (SceShell_7AD86CE1(SCE_NET_CTL_INFO_DEVICE, &info) >= 0 &&
				    info.device != 2) {
					shellNpPushResumeNotify(sceKernelGetProcessId(), 0);
				}
			}
		}
	} else if (prevState != 0 && curState == 0 && g_powerManage->m_netConnected) {
		g_powerManage->m_netConnected = false;
	}
}

/* --- Interface table functions --- */

static void ifaceAddPowerEventListener(PowerEventCallback callback, void *userData)
{
	g_powerManage->m_eventServer.AddListener(0, callback, userData);
}

static void ifaceRemovePowerEventListener(PowerEventCallback callback, void *userData)
{
	g_powerManage->m_eventServer.RemoveListener(0, callback, userData);
}

void powerManagePushKeyHandler(PowerKeyCallback callback, void *userData)
{
	g_powerManage->m_eventServer.PushKeyHandler(callback, userData);
}

void powerManagePopKeyHandler(PowerKeyCallback callback, void *userData)
{
	g_powerManage->m_eventServer.PopKeyHandler(callback, userData);
}

static void ifaceReloadWallpaper(void)
{
	g_powerManage->m_wallpaperMgr.Reload();
}

void powerManageLockPsButton(void)
{
	++g_powerManage->m_psButtonLockCount;
}

void powerManageUnlockPsButton(void)
{
	--g_powerManage->m_psButtonLockCount;
}

static void ifaceLockPowerButton(void)
{
	++g_powerManage->m_powerButtonLockCount;
}

static void ifaceUnlockPowerButton(void)
{
	--g_powerManage->m_powerButtonLockCount;
}

static void ifaceHoldPowerSwitch(void)
{
	if (shellStartupGetIduMode() != 1 && !shellStartupIsTestMode()) {
		if (g_powerManage->m_powerSwHoldCount == 0)
			vshPowerSetPowerSwMode(kPowerSwModeHold);
		++g_powerManage->m_powerSwHoldCount;
	}
}

static void ifaceReleasePowerSwitch(void)
{
	if (shellStartupGetIduMode() != 1 && !shellStartupIsTestMode()) {
		--g_powerManage->m_powerSwHoldCount;
		if (g_powerManage->m_powerSwHoldCount == 0)
			vshPowerSetPowerSwMode(kPowerSwModeNormal);
	}
}

static void ifacePowerTickAndSleep(void)
{
	if (shellStartupGetIduMode() == 1 || shellStartupIsTestMode() ||
	    g_powerManage->m_powerSwHoldCount > 0) {
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
		paf::thread::Sleep(33);
	} else {
		vshPowerSetPowerSwMode(kPowerSwModeHold);
		sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DEFAULT);
		paf::thread::Sleep(33);
		vshPowerSetPowerSwMode(kPowerSwModeNormal);
	}
}

void powerManageRequestDisplayOff(void)
{
	g_powerManage->m_powerSaveMode = true;
	scePowerRequestDisplayOff();
}

static void ifaceOnBootComplete(void)
{
	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	ShellPluginInterface *topmenuIface = NULL;
	if (topmenuPlugin != NULL) {
		topmenuIface = (ShellPluginInterface *)topmenuPlugin->GetInterface(1);
		if (topmenuIface != NULL)
			topmenuIface->func[kTopmenuSlotHideForLock]();
	}

	int wakeFactor = (int)(vshPowerGetWakeupFactor() & 0x7F);
	if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce ||
	    shellStartupNeedsInitialSetup() || shellStartupIsPmMode() ||
	    shellStartupGetIduMode() == 1 || shellStartupIsTestMode() ||
	    wakeFactor == kPowerWakeFactorRtcAlarm || wakeFactor == kPowerWakeFactorPowerOff) {
		bool hasPendingApp = !g_powerManage->m_pendingLaunchApp.empty();
		paf::string launchApp = g_powerManage->m_pendingLaunchApp;
		PowerManage::SetScreenState(SCREEN_STATE_UNLOCKED);
		if (topmenuIface != NULL) {
			void (*onUnlock)(bool, const char *) =
				(void (*)(bool, const char *))
					topmenuIface->func[kTopmenuSlotUnlockComplete];
			onUnlock(hasPendingApp, launchApp.c_str());
			if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce) {
				void (*overlayBegin)(void *, int, int, int, int) =
					(void (*)(void *, int, int, int, int))
						topmenuIface->func[kTopmenuSlotOverlayBegin];
				void (*overlayEnd)(void *, int) =
					(void (*)(void *, int))
						topmenuIface->func[kTopmenuSlotOverlayEnd];
				overlayBegin(g_powerManage, 230, 1, 0, 0);
				overlayEnd(g_powerManage, 230);
			}
		}
	} else {
		PowerManage::SetScreenState(SCREEN_STATE_KEYLOCK);
	}
}

static void ifaceScheduleControllerCheck(void)
{
	if (paf::system::Capability::HasWirelessController()) {
		g_powerManage->m_page->SetTimerListener(
			kTimerIdControllerCheck, 3000.0f,
			new paf::ui::EventCBListener(onCheckControllerConnected, NULL));
	}
}

static void ifaceUnlockAndLaunchApp(bool markWakeApp, const char *appName,
				    const char *launchArg)
{
	if (g_powerManage->m_currentScreen != NULL) {
		if (markWakeApp)
			g_powerManage->m_wakeAppName = (appName != NULL) ? appName : "";
		g_powerManage->m_pendingLaunchApp = (appName != NULL) ? appName : "";
		g_powerManage->m_pendingLaunchArg = (launchArg != NULL) ? launchArg : "";
		g_powerManage->m_currentScreen->Unlock(markWakeApp);
	} else if (appName != NULL) {
		int appId = sceAppMgrLaunchAppByNameForShell(0x20000, appName, launchArg);
		if (appId != -1)
			shellEventServerSend(11, NULL, (SceSize)appId);
	}
}

static void ifaceWakeAndLaunchForegroundApp(const char *appName, const char *launchArg)
{
	SceAppMgrAppStatus frontStatus;
	if (sceAppMgrGetStatusByAppId(-4, &frontStatus) == 0 &&
	    sce_paf_strncmp(appName, frontStatus.appName, 32) == 0) {
		if (KeyLockBlankPage::IsOpen()) {
			sceAVConfigDisplayOn();
			sceAppMgrSetPowerSaveMode(0, 0);
			g_powerManage->m_powerSaveMode = false;
		}
		sceAppMgrLaunchAppByName(0x20000, appName, launchArg);
	}
}

static void ifaceOpenPowerMenu(void)
{
	g_powerManage->m_powerMenu.Open();
}

static void ifaceClosePowerMenu(void)
{
	g_powerManage->m_powerMenu.Close();
}

static paf::ui::Widget *ifaceGetPage(void)
{
	return g_powerManage->m_page;
}

static void ifaceSetDolceLockEnabled(bool enabled)
{
	if (enabled) {
		if (!g_powerManage->m_dolceLockEnabled)
			g_powerManage->m_dolceLockEnabled = true;
	} else if (g_powerManage->m_dolceLockEnabled) {
		g_powerManage->m_dolceLockEnabled = false;
		if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce &&
		    g_powerManage->m_currentScreen != NULL) {
			g_powerManage->UnlockToTopMenu();
		}
	}
}

static int ifaceLockIfWakeApp(const char *titleId)
{
	if (PowerManage::IsWakeAppName(titleId) == 0)
		return 0;

	g_powerManage->m_wakeAppName = "";
	sceAppMgrSetPowerSaveMode(1, 1);
	shellHomeUpdateRequest(2);
	shellHomeUpdateRequest(0);
	g_powerManage->m_powerSaveMode = true;

	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	if (topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIface =
			(ShellPluginInterface *)topmenuPlugin->GetInterface(1);
		if (topmenuIface != NULL)
			topmenuIface->func[kTopmenuSlotHideForLock]();
	}
	PowerManage::SetScreenState(SCREEN_STATE_KEYLOCK);
	return 1;
}

static int ifaceIsWakeApp(const char *titleId)
{
	return PowerManage::IsWakeAppName(titleId);
}

static bool ifaceIsSuspended(void)
{
	return g_powerManage->m_isSuspended;
}

static AuthInterface *ifaceGetAuthInterface(void)
{
	return &s_authInterface;
}

/* --- PowerManageEventServer --- */

PowerManageEventServer::PowerManageEventServer()
{
	AddListener(0, OnDispatchKeyHandler, &m_keyStack);
}

PowerManageEventServer::~PowerManageEventServer()
{
	RemoveListener(0, OnDispatchKeyHandler, &m_keyStack);
	for (paf::map<int, PowerEventListenerList *>::iterator it = m_listeners.begin();
	     it != m_listeners.end(); ++it) {
		delete it->second;
	}
	m_listeners.clear();
}

void PowerManageEventServer::AddListener(int eventCode, PowerEventCallback callback,
					 void *userData)
{
	PowerEventListenerEntry *entry = new PowerEventListenerEntry(callback, userData);
	paf::map<int, PowerEventListenerList *>::iterator it = m_listeners.find(eventCode);
	if (it == m_listeners.end()) {
		PowerEventListenerList *list = new PowerEventListenerList();
		list->m_entries.push_back(entry);
		m_listeners[eventCode] = list;
		return;
	}
	PowerEventListenerList *list = it->second;
	if (list != NULL)
		list->m_entries.push_back(entry);
}

void PowerManageEventServer::RemoveListener(int eventCode, PowerEventCallback callback,
					    void *userData)
{
	paf::map<int, PowerEventListenerList *>::iterator mapIt = m_listeners.find(eventCode);
	if (mapIt == m_listeners.end() || mapIt->second == NULL)
		return;

	PowerEventListenerList *list = mapIt->second;
	for (paf::list<PowerEventListenerEntry *>::iterator it = list->m_entries.begin();
	     it != list->m_entries.end(); ++it) {
		PowerEventListenerEntry *entry = *it;
		if (entry->m_callback == callback && entry->m_userData == userData) {
			if (list->m_dispatching) {
				entry->m_pendingRemove = true;
			} else {
				list->m_entries.erase(it);
				delete entry;
			}
			return;
		}
	}
}

void PowerManageEventServer::Dispatch(int eventCode)
{
	paf::map<int, PowerEventListenerList *>::iterator mapIt = m_listeners.find(eventCode);
	if (mapIt == m_listeners.end() || mapIt->second == NULL)
		return;

	PowerEventListenerList *list = mapIt->second;
	list->m_dispatching = true;
	for (paf::list<PowerEventListenerEntry *>::iterator it = list->m_entries.begin();
	     it != list->m_entries.end(); ++it) {
		PowerEventListenerEntry *entry = *it;
		if (entry != NULL)
			entry->Notify(eventCode, NULL, 0);
	}
	list->m_dispatching = false;

	for (paf::list<PowerEventListenerEntry *>::iterator it = list->m_entries.begin();
	     it != list->m_entries.end();) {
		PowerEventListenerEntry *entry = *it;
		if (entry == NULL || entry->m_pendingRemove) {
			delete entry;
			it = list->m_entries.erase(it);
		} else {
			++it;
		}
	}
}

void PowerManageEventServer::PushKeyHandler(PowerKeyCallback callback, void *userData)
{
	PowerKeyHandlerEntry entry;
	entry.callback = callback;
	entry.userData = userData;
	m_keyStack.push_back(entry);
}

void PowerManageEventServer::PopKeyHandler(PowerKeyCallback callback, void *userData)
{
	for (paf::vector<PowerKeyHandlerEntry>::iterator it = m_keyStack.begin();
	     it != m_keyStack.end(); ++it) {
		if (it->callback == callback && it->userData == userData) {
			m_keyStack.erase(it);
			return;
		}
	}
}

void PowerManageEventServer::OnDispatchKeyHandler(int eventCode, const void *eventBuf,
						  SceSize eventSize, void *userData)
{
	(void)eventCode;
	(void)eventBuf;
	(void)eventSize;
	paf::vector<PowerKeyHandlerEntry> *stack =
		(paf::vector<PowerKeyHandlerEntry> *)userData;
	if (!stack->empty()) {
		PowerKeyHandlerEntry &top = stack->back();
		if (top.callback != NULL)
			top.callback(top.userData);
	}
}

/* --- PowerManage --- */

PowerManage::PowerManage(paf::Plugin *plugin)
	: m_plugin(plugin),
	  m_page(NULL),
	  m_currentState(SCREEN_STATE_UNLOCKED),
	  m_targetState(SCREEN_STATE_UNLOCKED),
	  m_currentScreen(NULL),
	  m_psButtonLockCount(0),
	  m_powerButtonLockCount(0),
	  m_isSuspended(false),
	  m_iduTimerPaused(false),
	  m_lowBatteryControllerMask(0),
	  m_netConnected(false),
	  m_dolceLockEnabled(false),
	  m_pendingResumeDisplayOn(false),
	  m_powerSaveMode(false),
	  m_noControllerNoticeShown(false),
	  m_lastResumeTime(0),
	  m_statusDialogId(-1),
	  m_powerSwHoldCount(0),
	  m_powerCallbackId(-1),
	  m_usbCallbackId(-1),
	  m_ctrlCallbackId(-1),
	  m_netCtlCallbackId(-1)
{
	g_powerManage = this;

	s_powerManageInterface.addPowerEventListener = ifaceAddPowerEventListener;
	s_powerManageInterface.removePowerEventListener = ifaceRemovePowerEventListener;
	s_powerManageInterface.pushKeyHandler = powerManagePushKeyHandler;
	s_powerManageInterface.popKeyHandler = powerManagePopKeyHandler;
	s_powerManageInterface.reloadWallpaper = ifaceReloadWallpaper;
	s_powerManageInterface.lockPsButton = powerManageLockPsButton;
	s_powerManageInterface.unlockPsButton = powerManageUnlockPsButton;
	s_powerManageInterface.lockPowerButton = ifaceLockPowerButton;
	s_powerManageInterface.unlockPowerButton = ifaceUnlockPowerButton;
	s_powerManageInterface.holdPowerSwitch = ifaceHoldPowerSwitch;
	s_powerManageInterface.releasePowerSwitch = ifaceReleasePowerSwitch;
	s_powerManageInterface.reservedHookA = powerManageNoop;
	s_powerManageInterface.reservedHookB = powerManageNoop;
	s_powerManageInterface.powerTickAndSleep = ifacePowerTickAndSleep;
	s_powerManageInterface.requestDisplayOff = powerManageRequestDisplayOff;
	s_powerManageInterface.onBootComplete = ifaceOnBootComplete;
	s_powerManageInterface.scheduleControllerCheck = ifaceScheduleControllerCheck;
	s_powerManageInterface.unlockAndLaunchApp = ifaceUnlockAndLaunchApp;
	s_powerManageInterface.wakeAndLaunchForegroundApp = ifaceWakeAndLaunchForegroundApp;
	s_powerManageInterface.openPowerMenu = ifaceOpenPowerMenu;
	s_powerManageInterface.closePowerMenu = ifaceClosePowerMenu;
	s_powerManageInterface.getPage = ifaceGetPage;
	s_powerManageInterface.setDolceLockEnabled = ifaceSetDolceLockEnabled;
	s_powerManageInterface.lockIfWakeApp = ifaceLockIfWakeApp;
	s_powerManageInterface.isWakeApp = ifaceIsWakeApp;
	s_powerManageInterface.isSuspended = ifaceIsSuspended;
	s_powerManageInterface.getAuthInterface = ifaceGetAuthInterface;

	s_authInterface.startAuth = authServiceStart;
	s_authInterface.cancelAuth = authServiceCancel;
	s_authInterface.isPasscodeEnabled = authServiceIsPasscodeEnabled;
	s_authInterface.reloadRegistry = authServiceReloadRegistry;

	paf::Plugin::PageOpenParam openParam;
	openParam.option = 7;
	m_page = plugin->PageOpen(paf::IDParam(kPowerManagePageHash), openParam);
	m_page->SetTimerListener(kTimerIdPeriodic, 1000.0f,
				 new paf::ui::EventCBListener(onPeriodicTimer, NULL));

	m_powerCallbackId = sceKernelCreateCallback("SceShellPower", 0, onPowerCallback, NULL);
	scePowerRegisterCallback(m_powerCallbackId);

	m_usbCallbackId = sceKernelCreateCallback("SceShellUsb", 0, onUsbCallback, NULL);
	sceUsbdRegisterCallback(m_usbCallbackId, 1);

	m_ctrlCallbackId = sceKernelCreateCallback("SceShellCtrl", 0, onCtrlCallback, NULL);
	vshCtrlRegisterNotifyCallBack(m_ctrlCallbackId);

	SceShell_2E0B6FA6(onNetCtlEvent, NULL, &m_netCtlCallbackId);
	SceShell_83D16CA8(SHELL_APP_EVENT_ENDED, onProcessEvent, NULL);
	SceShell_83D16CA8(SHELL_APP_EVENT_DEACTIVATED, onProcessEvent, NULL);

	if (shellStartupGetIduMode() == 1)
		SceShell_7FE4E732(8, onLeaveIduMode, NULL);

	SetIduTimerEnabled(true);

	SceNetCtlInfo ipInfo;
	if (SceShell_7AD86CE1(SCE_NET_CTL_INFO_IP_ADDRESS, &ipInfo) == 0 && !m_netConnected) {
		m_netConnected = true;
		if (!m_isSuspended) {
			SceNetCtlInfo devInfo;
			if (SceShell_7AD86CE1(SCE_NET_CTL_INFO_DEVICE, &devInfo) >= 0 &&
			    devInfo.device != 2) {
				shellNpPushResumeNotify(sceKernelGetProcessId(), 0);
			}
		}
	}

	SetScreenState(SCREEN_STATE_UNLOCKED);
	authServiceCreate();
	authServiceReloadRegistry(true);
}

PowerManage::~PowerManage()
{
	SceShell_B4D888D5(m_netCtlCallbackId);
	vshCtrlUnregisterNotifyCallBack(m_ctrlCallbackId);
	sceKernelDeleteCallback(m_ctrlCallbackId);
	sceUsbdUnregisterCallback(m_usbCallbackId);
	sceKernelDeleteCallback(m_usbCallbackId);
	scePowerUnregisterCallback(m_powerCallbackId);
	sceKernelDeleteCallback(m_powerCallbackId);
	authServiceDestroy();
	delete m_currentScreen;
	m_currentScreen = NULL;
	g_powerManage = NULL;
}

void PowerManage::SetIduTimerEnabled(bool enabled)
{
	if (enabled) {
		if (!m_iduTimerPaused) {
			if (shellStartupGetIduMode() == 1)
				m_page->KillTimer(kTimerIdControllerCheck);
			m_iduTimerPaused = true;
		}
	} else if (m_iduTimerPaused) {
		if (shellStartupGetIduMode() == 1) {
			m_page->SetTimerListener(kTimerIdIduShutdown, 180000.0f,
						 new paf::ui::EventCBListener(onIduShutdownTimer,
									      NULL));
		}
		m_iduTimerPaused = false;
	}
}

void PowerManage::SetScreenState(int state)
{
	if (state == SCREEN_STATE_KEYLOCK)
		shellZoomReset();

	PowerManage *self = g_powerManage;
	self->m_targetState = state;
	if (self->m_currentScreen != NULL) {
		if (self->m_currentState == state && !self->m_currentScreen->IsDragging()) {
			self->m_currentScreen->OnResume();
			return;
		}
		delete self->m_currentScreen;
		self->m_currentScreen = NULL;
		if (state == SCREEN_STATE_UNLOCKED && !self->m_pendingLaunchApp.empty()) {
			int appId = sceAppMgrLaunchAppByNameForShell(
				0x20000, self->m_pendingLaunchApp.c_str(),
				self->m_pendingLaunchArg.c_str());
			if (appId != -1)
				shellEventServerSend(11, NULL, (SceSize)appId);
			self->m_pendingLaunchApp = "";
			self->m_pendingLaunchArg = "";
		}
	}

	self->m_currentState = state;
	if (state == SCREEN_STATE_KEYLOCK) {
		self->m_currentScreen = new KeyLockScreen(self->m_plugin);
	} else if (state == SCREEN_STATE_PASSCODE) {
		self->m_currentScreen = new PasscodeLockScreen(self->m_plugin);
	} else if (state == SCREEN_STATE_UNLOCKED) {
		KeyLockBlankPage::Close();
	}
}

void PowerManage::SetPowerSaveMode(bool enablePowerSave, bool suspendApps)
{
	if (enablePowerSave) {
		if (g_powerManage->m_powerSaveMode)
			suspendApps = true;
		sceAppMgrSetPowerSaveMode(1, suspendApps ? 1 : 0);
	} else if (!suspendApps) {
		sceAppMgrSetPowerSaveMode(0, 0);
	}

	if (enablePowerSave) {
		if (suspendApps)
			shellHomeUpdateRequest(2);
		shellHomeUpdateRequest(0);
	}
	g_powerManage->m_powerSaveMode = suspendApps;
}

int PowerManage::IsWakeAppName(const char *titleId)
{
	if (!g_powerManage->m_pendingLaunchApp.empty())
		return 0;
	paf::string candidate = (titleId != NULL) ? titleId : "";
	return (g_powerManage->m_wakeAppName == candidate) ? 1 : 0;
}

void PowerManage::TurnDisplayOnAndNotify()
{
	sceAVConfigDisplayOn();
	sceAppMgrRestoreDisplaySettingForShell();
	shellEventServerSend(14, NULL, 0);
}

void PowerManage::UnlockToTopMenu()
{
	sceAppMgrRestoreBgmSettingForShell();
	sceAppMgrSetPowerSaveMode(0, 0);
	m_powerSaveMode = false;

	bool hasPendingApp = !m_pendingLaunchApp.empty();
	paf::string launchApp = m_pendingLaunchApp;
	SetScreenState(SCREEN_STATE_UNLOCKED);

	paf::Plugin *topmenuPlugin = paf::Plugin::Find("topmenu_plugin");
	if (topmenuPlugin != NULL) {
		ShellPluginInterface *topmenuIface =
			(ShellPluginInterface *)topmenuPlugin->GetInterface(1);
		if (topmenuIface != NULL) {
			void (*onUnlock)(bool, const char *) =
				(void (*)(bool, const char *))
					topmenuIface->func[kTopmenuSlotUnlockComplete];
			onUnlock(hasPendingApp, launchApp.c_str());
		}
	}
}

void PowerManage::SetSuspendedState(bool suspended)
{
	if (suspended) {
		if (!m_isSuspended) {
			SceUInt64 now = sceKernelGetProcessTimeWide();
			::consoleInfoFileWriteInt64(1, (SceUInt64)(now - m_lastResumeTime), 0);
			m_isSuspended = true;
			++paf::graph::s_wait_vblank_count;
		}
	} else {
		vshPowerSetPowerSwMode(kPowerSwModeNormal);
		ShellMediaInfoRequest mediaReq;
		memset(&mediaReq, 0, sizeof(mediaReq));
		mediaReq.type = 3;
		shellMediaInfoWrite(&mediaReq, false);

		if (m_isSuspended) {
			m_lastResumeTime = sceKernelGetProcessTimeWide();
			if (paf::graph::s_wait_vblank_count != 0)
				--paf::graph::s_wait_vblank_count;
			m_isSuspended = false;
			if (m_netConnected) {
				SceNetCtlInfo devInfo;
				if (SceShell_7AD86CE1(SCE_NET_CTL_INFO_DEVICE, &devInfo) >= 0 &&
				    devInfo.device != 2) {
					shellNpPushResumeNotify(sceKernelGetProcessId(), 0);
				}
			}
		}
	}
}

void PowerManage::ShowImposeNotice(SceUInt32 messageHash, const wchar_t *formatArg)
{
	paf::Plugin *imposePlugin = paf::Plugin::Find("impose_plugin");
	if (imposePlugin == NULL)
		return;

	ImposePluginInterface *imposeIface =
		(ImposePluginInterface *)imposePlugin->GetInterface(1);
	if (imposeIface == NULL)
		return;

	ImposeNotice notice;
	char noticeName[32];
	sce_paf_snprintf(noticeName, sizeof(noticeName), "power_manage_plugin_%08x", messageHash);
	notice.name = noticeName;
	notice.iconHash = kNoticeIconHash;

	const wchar_t *rawText = m_plugin->GetString(paf::IDParam(messageHash));
	paf::wstring formatted = shellFormatText(rawText, formatArg);
	notice.message = formatted.c_str();
	imposeIface->showNotice(&notice);
}

void PowerManage::OpenStatusDialog(const CommonGuiDialogLayout &layout, SceUInt32 messageHash,
				   SceUInt32 textureHash, const paf::math::v4 &iconPos)
{
	if (m_statusDialogId >= 0)
		return;

	CommonGuiLayoutDialogParam dialogParam;
	dialogParam.dimmer = 2;
	dialogParam.layout = layout;
	if (messageHash != 0)
		dialogParam.text = m_plugin->GetString(paf::IDParam(messageHash));

	m_statusDialogId = shellStateWatcherOpenDialog(&dialogParam, true, 2);
	if (textureHash != 0 && m_statusDialogId >= 0) {
		paf::ui::Widget *hostWidget = SceCommonGuiDialog_8947FC23(m_statusDialogId, 1);
		if (hostWidget != NULL) {
			paf::ui::Plane *iconPlane = new paf::ui::Plane(hostWidget, NULL);
			if (iconPlane != NULL) {
				paf::intrusive_ptr<paf::graph::Surface> iconTex =
					m_plugin->GetTexture(paf::IDParam(textureHash));
				iconPlane->SetTexture(iconTex, 0, 0);
				iconPlane->SetPos(iconPos);
				paf::math::v4 texSize((float)iconTex->GetWidth(),
						      (float)iconTex->GetHeight(), 0.0f, 0.0f);
				iconPlane->SetSize(texSize);
			}
		}
	}
}

} /* namespace power_manage */

/* --- Plugin entrypoints (declared in plugins.h) --- */

void powerManagePluginSetParam(paf::Plugin::InitParam2 *param)
{
	param->draw_priority = 22;
	param->surface_pool_size = 655360;
}

void powerManagePluginInit(paf::Plugin *plugin)
{
	power_manage::g_powerManagePlugin = plugin;
	plugin->SetInterface(1, &power_manage::s_powerManageInterface);
}

void powerManagePluginStart(paf::Plugin *plugin)
{
	new power_manage::PowerManage(plugin);
}

void powerManagePluginStop(paf::Plugin *plugin)
{
	(void)plugin;
	delete power_manage::g_powerManage;
}

void powerManagePluginExit(paf::Plugin *plugin)
{
	(void)plugin;
}
