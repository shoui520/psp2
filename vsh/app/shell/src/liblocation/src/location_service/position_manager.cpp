/*
 * position_manager.cpp: PositionManager, the core of SceShell's liblocation service
 * (provider module loading/unloading, Open/Reopen/Close/GetLocation/GetHeading/
 * GetPermissionStatus, GPS flying start, Hubeny distance calculation, and debug command
 * dispatch; 0x8118B9A0-0x8118E748). Sony's class name ("PositionManager::...") comes from
 * its log strings; internal helper names are ours (PLACEHOLDER).
 */
#include "location_internal.h"
#include <math.h>
#include <power.h>
#include <kernel/iofilemgr.h>
#include <kernel/modulemgr.h>
#include <appmgr/appmgr.h>
#include <regmgr/regmgr.h>
#include <vshbridge/vshbridge.h>
#include <std/string.h>
#include <std/new>
#include <system/system.h>

using namespace paf;

extern "C" {
SceUInt64 sceRtcGetAccumulativeTime(void);
float sce_paf_sinf(float x);
float sce_paf_cosf(float x);
int SceLibLocationProvider_0CBB4331(int arg);
int SceLibLocationProvider_2A97FD02(void);
LocationProvider *SceLibLocationProvider_836F5953(
	int (*cb)(const SceLocationLocationInfo *, int, SceUInt32, const SceRtcTick *));
int SceLibLocationProvider_8A796686(LocationProvider *provider);
LocationProvider *SceLibLocationDolceProvide_67156291(
	int (*cb)(const SceLocationLocationInfo *, int, SceUInt32, const SceRtcTick *));
int SceLibLocationDolceProvide_AF76021F(LocationProvider *provider);
}

/* globals shared across liblocation */
LocationLogger *g_locationLogger;				/* (0x815496B8) */
LocationSetting *g_locationConfig;				/* (0x815496C0) */

/* the PositionManager singleton (0x81544CE4) */
static PositionManager *s_positionManager;

/* (0x8118B9A0) */
int PositionConfigRefreshTask::OnNotify()
{
	if (m_pm != NULL && m_pm->m_config != NULL)
		m_pm->m_config->ReloadAgePermit();
	return 1;
}

/* (0x8118B9B2) */
int PositionPowerRegTask::OnNotify()
{
	if (m_registered == 0) {
		m_cbId = sceKernelCreateCallback("SceLocationPowerCallback", 0,
						 PositionManager::PowerCallback, NULL);
		if (m_cbId >= 0) {
			if (scePowerRegisterCallback(m_cbId) == 0) {
				m_registered = 1;
				return 0;
			}
			sceKernelDeleteCallback(m_cbId);
			m_cbId = -1;
		}
	}
	return 1;
}

/* (0x8118B9FC) */
int PositionManager::UpdateFlyingStart()
{
	DriverManager *dev = locationDeviceGetInstance();

	if (dev == NULL)
		return 0;
	if (!m_config->HasGps())
		return 0;
	if (m_config->IsEmulationMode())
		return 0;
	if (!m_config->IsDeviceEnabled())
		return 0;

	SceUInt64 now = sceRtcGetAccumulativeTime();
	SceUInt64 lastTime = dev->m_gpsCtrl.m_lastStartedTime;

	if (now - lastTime <= m_config->m_flyingStartInterval)
		return 0;
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "GPS Flying Start\n");
	return dev->m_flyingStart.Trigger();
}

/* (0x8118BA84) */
PositionManager *locationGetContext(void)
{
	return s_positionManager;
}

/* (0x8118BA90) */
int PositionManager::PowerCallback(int notifyId, int notifyCount, int powerInfo, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)common;
	if ((powerInfo & 0x800000) != 0 && (powerInfo & 0x20000) != 0) {
		SceUInt32 factor = vshPowerGetWakeupFactor() & 0x7F;

		if ((factor == 0 || factor == 18) && s_positionManager != NULL &&
		    s_positionManager->m_flyingCount != 0)
			s_positionManager->UpdateFlyingStart();
	}
	return 0;
}

/* (0x8118BAC8) */
int PositionManager::Init(NotifyEventFn locCb, NotifyHeadingFn headCb)
{
	int ret = 0;

	m_driverCtrl = new (g_locationHeap) DriverController(this);
	if (m_driverCtrl == NULL)
		ret = SCE_LOCATION_ERROR_NO_MEMORY;

	if (ret == 0) {
		m_handleMgr = new (g_locationHeap) ClientManager(this);
		if (m_handleMgr == NULL)
			ret = SCE_LOCATION_ERROR_NO_MEMORY;
		else
			ret = m_handleMgr->Init();
	}

	if (ret == 0) {
		m_notifyEventCb = locCb;
		m_notifyHeadingCb = headCb;
		m_susWatcher.Init(m_handleMgr);
		g_locationManager->RegisterTask(&m_susWatcher);

		m_headingMgr = new (g_locationHeap) HeadingManager(this);
		if (m_headingMgr == NULL)
			ret = SCE_LOCATION_ERROR_NO_MEMORY;
		else
			ret = m_headingMgr->Init();
	}

	if (ret == 0) {
		m_bridge = new (g_locationHeap) LocationIntervalCtrl(this, m_headingMgr,
								     m_handleMgr);
		if (m_bridge == NULL)
			ret = SCE_LOCATION_ERROR_NO_MEMORY;
	}

	if (ret == 0) {
		m_config = new (g_locationHeap) LocationSetting();
		if (m_config == NULL) {
			ret = SCE_LOCATION_ERROR_NO_MEMORY;
		} else {
			m_config->Init();
			g_locationConfig = m_config;
		}
	}

	if (ret == 0) {
		m_permMgr = new (g_locationHeap) LocationPrivacySettings(m_config);
		if (m_permMgr == NULL)
			ret = SCE_LOCATION_ERROR_NO_MEMORY;
		else
			ret = m_permMgr->Init();
	}

	if (ret == 0) {
		ret = m_scanMode.Init();
		if (ret == 0)
			m_scanMode.m_listeners.push_back(this);
	}

	if (ret == 0)
		ret = locationGpsInit();

	if (ret == 0) {
		m_susWatcher.m_listeners.push_back(&m_config->m_regWatcher);
		m_susWatcher.m_listeners.push_back(m_headingMgr);

		g_locationManager->RegisterTask(&m_configRefreshTask);
		m_configRefreshTask.Schedule(86400000);
		ret = g_locationManager->RegisterTask(&m_powerRegTask);
		m_powerRegTask.Schedule(1000);

		m_logger = new (g_locationHeap) LocationLogger();
		if (m_logger != NULL) {
			g_locationLogger = m_logger;
			m_logger->Init();
		}
		if (ret != 0)
			locationGpsTerm();
	}

	if (ret != 0) {
		m_scanMode.Term();
		if (m_permMgr != NULL) {
			delete m_permMgr;
			m_permMgr = NULL;
		}
		if (m_config != NULL) {
			delete m_config;
			m_config = NULL;
		}
		if (m_bridge != NULL) {
			delete m_bridge;
			m_bridge = NULL;
		}
		if (m_headingMgr != NULL) {
			m_headingMgr->Term();
			delete m_headingMgr;
			m_headingMgr = NULL;
		}
		if (m_handleMgr != NULL) {
			delete m_handleMgr;
			m_handleMgr = NULL;
		}
		if (m_driverCtrl != NULL) {
			delete m_driverCtrl;
			m_driverCtrl = NULL;
		}
	}
	return ret;
}

/* (0x8118BE24) */
__attribute__((noinline))
PositionManager::PositionManager()
{
	m_providerLoaded = 0;
	m_powerRegTask.m_registered = 0;
	m_susWatcher.m_suspended = 0;
	m_susWatcher.m_mgr = NULL;
	m_susWatcher.m_slotIndex = 0;
	m_susWatcher.m_mutexResult = -1;
	m_susWatcher.m_handleMgr = NULL;
	m_susWatcher.m_provider = NULL;
	m_configRefreshTask.m_mgr = NULL;
	m_configRefreshTask.m_slotIndex = 0;
	m_configRefreshTask.m_pm = this;
	m_powerRegTask.m_mgr = NULL;
	m_powerRegTask.m_slotIndex = 0;
	m_powerRegTask.m_pm = this;
	m_scanMode.m_apScanMode = 0;
	m_scanMode.m_mutexResult = -1;
	m_provider = NULL;
	m_handleMgr = NULL;
	m_headingMgr = NULL;
	m_driverCtrl = NULL;
	m_bridge = NULL;
	m_config = NULL;
	m_permMgr = NULL;
	m_logger = NULL;
	m_notifyEventCb = NULL;
	m_notifyHeadingCb = NULL;
	m_providerModId = -1;
	sce_paf_memset(m_flyingTitles, 0, sizeof(m_flyingTitles));
	m_flyingCount = 0;
}

/* (0x8118BEE0) */
int locationCoreInit(PositionManager::NotifyEventFn locCb,
		     PositionManager::NotifyHeadingFn headCb)
{
	if (s_positionManager != NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	PositionManager *pm = new (g_locationHeap) PositionManager();

	s_positionManager = pm;
	if (pm == NULL)
		return SCE_LOCATION_ERROR_NO_MEMORY;

	int ret = pm->Init(locCb, headCb);

	if (ret != 0) {
		delete s_positionManager;
		s_positionManager = NULL;
	}
	return ret;
}

/* (0x8118BF66) */
int locationCoreTerm(void)
{
	if (s_positionManager != NULL) {
		delete s_positionManager;
		s_positionManager = NULL;
	}
	return 0;
}

/* (0x8118BF8C) */
LocationPrivacySettings *PositionManager::GetPermMgr()
{
	return m_permMgr;
}

/* (0x8118BF90) */
LocationIntervalCtrl *PositionManager::GetBridge()
{
	return m_bridge;
}

/* (0x8118BF96) */
int PositionManager::LoadProvider()
{
	if (m_providerLoaded != 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	int status = 0;
	SceUID modId;

	if (system::Model::GetType() == 0) {
		void *args[6];

		args[0] = g_locationHeap;
		args[1] = g_locationManager;
		args[2] = m_logger;
		args[3] = m_config;
		args[4] = g_locationXtra;
		args[5] = locationDeviceGetInstance();
		modId = sceKernelLoadStartModule("vs0:vsh/shell/liblocation_provider.suprx",
						 sizeof(args), args, 0, NULL, &status);
	} else {
		void *args[3];

		args[0] = g_locationHeap;
		args[1] = g_locationManager;
		args[2] = m_logger;
		modId = sceKernelLoadStartModule("vs0:vsh/shell/liblocation_dolce_provider.suprx",
						 sizeof(args), args, 0, NULL, &status);
	}
	m_providerModId = modId;
	if (modId < 0)
		return modId;

	int ret = 0;

	if (system::Model::GetType() == 0)
		ret = SceLibLocationProvider_0CBB4331(0);
	if (ret != 0) {
		sceKernelStopUnloadModule(m_providerModId, 0, NULL, 0, NULL, &status);
		return ret;
	}

	LocationProvider *prov;

	if (system::Model::GetType() != 0)
		prov = SceLibLocationDolceProvide_67156291(ProviderEventCB);
	else
		prov = SceLibLocationProvider_836F5953(ProviderEventCB);
	m_provider = prov;
	if (prov == NULL) {
		if (system::Model::GetType() == 0)
			SceLibLocationProvider_2A97FD02();
		sceKernelStopUnloadModule(m_providerModId, 0, NULL, 0, NULL, &status);
		return SCE_LOCATION_ERROR_NO_MEMORY;
	}
	m_providerLoaded = 1;
	m_bridge->SetProvider(prov);
	sceKernelLockLwMutex(&m_susWatcher.m_mutex, 1, NULL);
	m_susWatcher.m_provider = m_provider;
	sceKernelUnlockLwMutex(&m_susWatcher.m_mutex, 1);
	if (system::Model::GetType() == 0)
		m_provider->SetHeadingMgr(m_headingMgr);
	return 0;
}

/* (0x8118C108) */
int PositionManager::UpdateHeadingState()
{
	int ret = 0;
	SceUInt32 hmethod = m_config->m_methodState.m_hmethod & SCE_LOCATION_HMETHOD_MASK;

	if (m_headingMgr->IsMotionStarted()) {
		if (m_headingMgr->IsMagnetometerEnabled() && hmethod == 0)
			ret = m_headingMgr->DisableMagnetometer();
		else if (!m_headingMgr->IsMagnetometerEnabled() && hmethod != 0)
			ret = m_headingMgr->EnableMagnetometer();
		if (hmethod == 0 && (m_provider == NULL || !m_provider->NeedsHeading())) {
			m_headingMgr->StopPolling();
			m_bridge->UpdateInterval(1);
			return m_headingMgr->StopMotion();
		}
	} else if (hmethod != 0 || (m_provider != NULL && m_provider->NeedsHeading())) {
		ret = m_headingMgr->StartMotion();
		if (ret == 0) {
			if (hmethod != 0)
				ret = m_headingMgr->EnableMagnetometer();
			if (ret == 0) {
				m_bridge->UpdateInterval(1);
				return m_headingMgr->StartPolling();
			}
		}
	}
	return ret;
}

/* (0x8118C1AE) */
int PositionManager::UpdateEmuMode(SceUInt32 *lmethod)
{
	unsigned char mode;

	if (m_config->IsEmulationMode()) {
		mode = 8;
	} else if (m_config->HasGps()) {
		switch (*lmethod & SCE_LOCATION_LMETHOD_MASK) {
		case SCE_LOCATION_LMETHOD_NONE:
			mode = 0;
			break;
		case SCE_LOCATION_LMETHOD_AGPS_AND_3G_AND_WIFI:
			mode = 23;
			break;
		case SCE_LOCATION_LMETHOD_3G_AND_WIFI:
			mode = 6;
			break;
		case SCE_LOCATION_LMETHOD_WIFI:
			mode = 2;
			break;
		case SCE_LOCATION_LMETHOD_3G:
			mode = 4;
			break;
		case SCE_LOCATION_LMETHOD_GPS:
			mode = 1;
			if (g_locationLogger != NULL &&
			    g_locationLogger->GetXtraLocalFileOnly() != 1 &&
			    g_locationLogger->GetXtraAssistEnabled() != 0 &&
			    (g_locationLogger->GetXtraSourceMode() != 0 ||
			     g_locationLogger->GetForceXtraDownload() == 1))
				mode = 17;
			break;
		case SCE_LOCATION_LMETHOD_GPS_EMULATION:
			mode = 8;
			break;
		default:
			mode = 0;
			break;
		}
	} else {
		mode = 2;
	}
	return m_driverCtrl->SetPowerStatus(mode);
}

/* (0x8118C250) */
int PositionManager::StopHeadingCallback(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::StopHeadingCallback() START\n");

	SceUInt32 hmethod = 0;
	int ret = m_handleMgr->GetMethod(appId, handle, NULL, &hmethod);

	if (ret == 0) {
		if ((hmethod & SCE_LOCATION_HMETHOD_MASK) != 0)
			ret = m_handleMgr->StopHeadingCallback(appId, handle);
		else
			ret = SCE_LOCATION_ERROR_INVALID_HEADING_METHOD;
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::StopHeadingCallback() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118C2EE) */
int PositionManager::StopLocationCallback(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::StopLocationCallback() START\n");

	SceUInt32 lmethod = 0;
	int ret = m_handleMgr->GetMethod(appId, handle, &lmethod, NULL);

	if (ret == 0) {
		ret = m_handleMgr->CheckPermission(handle);
		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			if ((lmethod & SCE_LOCATION_LMETHOD_MASK) != 0) {
				ret = m_handleMgr->StopLocationCallback(appId, handle);
				if (ret == 0) {
					ret = m_permMgr->ProviderStop(m_provider);
					if (ret == 0)
						m_bridge->UpdateInterval(0);

					LocationConfirmMgr *confirm = locationConfirmGetInstance();

					if (confirm != NULL &&
					    confirm->GetStatus(appId, handle) ==
						    SCE_LOCATION_DIALOG_STATUS_RUNNING) {
						confirm->Abort(appId, handle);
						m_handleMgr->SetPendingConfirm(handle, 0);
					}
				}
			} else {
				ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
			}
		}
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::StopPositionCallback() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118C3E4) */
int PositionManager::CancelGetLocation(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::CancelGetLocation() START\n");

	int ret;

	if (m_handleMgr->IsValidHandle(appId, handle)) {
		ret = m_handleMgr->CheckPermission(handle);
		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			if ((m_handleMgr->GetTypeFlags(appId, handle) & 2) != 0)
				locationStatsCountWifiHistoryFailed();

			unsigned char pending = 0;

			ret = m_handleMgr->GetPendingConfirm(handle, &pending);
			if (pending != 0) {
				LocationConfirmMgr *confirm = locationConfirmGetInstance();

				if (confirm != NULL)
					ret = confirm->Abort(appId, handle);
				m_handleMgr->SetPendingConfirm(handle, 0);
			} else {
				ret = m_handleMgr->CancelGetLocation(handle, 3);
				if (ret == 0) {
					ret = m_permMgr->ProviderStop(m_provider);
					if (ret == 0 && m_scanMode.m_apScanMode == 1) {
						unsigned char hasWh = 0;

						ret = m_handleMgr->HasActiveWifiHistory(&hasWh);
						if ((ret | hasWh) == 0)
							ret = m_scanMode.SetMode(0);
					}
				}
			}
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::CancelGetLocation() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118C508) */
int PositionManager::Close(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::Close() START\n");

	int flags = m_handleMgr->GetStateFlags(appId, handle);

	if ((flags & 4) != 0)
		CancelGetLocation(appId, handle);
	if ((flags & 8) != 0)
		StopLocationCallback(appId, handle);
	if ((flags & 0x10) != 0)
		StopHeadingCallback(appId, handle);

	m_headingMgr->SaveLastLocation();

	int ret = m_handleMgr->Close(appId, handle);

	if (ret == 0) {
		SceUInt32 lmethod = 0;
		SceUInt32 hmethod = 0;

		ret = m_handleMgr->GetActiveMethods(&lmethod, &hmethod);
		if (ret == 0) {
			m_config->SetLMethod(&lmethod);
			m_config->SetHMethod(&hmethod);
			m_config->m_methodState.m_flags = (int)(lmethod & 0x100);
		}

		int r = UpdateEmuMode(&m_config->m_methodState.m_lmethod);

		if (r != 0)
			ret = r;
		r = UpdateHeadingState();
		if (r != 0)
			ret = r;

		int permRet = 0;

		if (m_providerLoaded != 0) {
			if (!m_permMgr->IsGloballyEnabled() || !m_permMgr->HasAnyAllowedApp())
				permRet = m_permMgr->ProviderSuspend(m_provider);
			else
				permRet = m_permMgr->ProviderResume(m_provider);
		}
		if (permRet != 0)
			ret = permRet;

		if (m_handleMgr->GetCount() > 0 && m_providerLoaded == 0 &&
		    ((m_config->m_methodState.m_lmethod & SCE_LOCATION_LMETHOD_MASK) != 0 ||
		     (m_config->m_methodState.m_hmethod & SCE_LOCATION_HMETHOD_MASK) != 0)) {
			r = LoadProvider();
			if (r != 0)
				ret = r;
		}
		if (g_locationManager != NULL)
			g_locationManager->ReleaseActive();
		else
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::Close() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118C70C) */
int PositionManager::ValidateLMethod(SceUInt32 *lmethod)
{
	if ((!vshSblAimgrIsDEX() && !vshSblAimgrIsTool() || system::Model::GetType() == 1) &&
	    (*lmethod & SCE_LOCATION_LMETHOD_MASK) == SCE_LOCATION_LMETHOD_GPS_EMULATION)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;

	SceUInt32 m = *lmethod & SCE_LOCATION_LMETHOD_MASK;

	if (m != SCE_LOCATION_LMETHOD_NONE) {
		if (m_config->IsEmulationMode())
			m = SCE_LOCATION_LMETHOD_GPS_EMULATION;
		else if (!m_config->HasGps() && m != SCE_LOCATION_LMETHOD_GPS_EMULATION &&
			 m != SCE_LOCATION_LMETHOD_WIFI)
			m = SCE_LOCATION_LMETHOD_WIFI;
	}
	*lmethod = (*lmethod & ~SCE_LOCATION_LMETHOD_MASK) | m;
	if (m == SCE_LOCATION_LMETHOD_GPS_EMULATION) {
		SceIoStat stat;

		if (sceIoGetstat(m_driverCtrl->GetEmulationFile(), &stat) == 0)
			return 0;
		return SCE_LOCATION_DBGERROR_EMULATION_FILE_ERROR;
	}
	return 0;
}

/* (0x8118C7C0) */
int PositionManager::Open(int appId, SceLocationHandle *handle, SceUInt32 *lmethod,
			  SceUInt32 *hmethod, SceUInt32 *mode)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(
			8, "PositionManager::Open() START lmethod:0x%08x initMode:0x%08x\n",
			*lmethod & SCE_LOCATION_LMETHOD_MASK, *mode & 0xFFFFFF00u);

	int ret = m_config->ReloadAgePermit();

	if (ret == 0) {
		if (vshSblAimgrIsDEX() || vshSblAimgrIsTool())
			m_config->LoadDebugPermission();

		SceUInt32 lm = *lmethod;

		ret = ValidateLMethod(&lm);
		if (ret == 0) {
			ret = m_handleMgr->Open(appId, handle, &lm, hmethod, mode);
			if (ret == 0) {
				SceUInt32 activeL = 0;
				SceUInt32 activeH = 0;

				ret = m_handleMgr->GetActiveMethods(&activeL, &activeH);
				if (ret != 0) {
					Close(appId, *handle);
				} else {
					m_config->SetLMethod(&activeL);
					m_config->SetHMethod(&activeH);
					m_config->m_methodState.m_flags = (int)(activeL & 0x100);
					if (g_locationManager != NULL) {
						g_locationManager->AcquireActive();

						bool loadedNow = false;
						SceUInt32 lmBits =
							m_config->m_methodState.m_lmethod &
							SCE_LOCATION_LMETHOD_MASK;
						SceUInt32 hmBits =
							m_config->m_methodState.m_hmethod &
							SCE_LOCATION_HMETHOD_MASK;

						if (m_handleMgr->GetCount() > 0 &&
						    m_providerLoaded == 0 &&
						    (lmBits | hmBits) != 0) {
							ret = LoadProvider();
							loadedNow = (ret >= 0);
						}
						if (loadedNow)
							m_susWatcher.Schedule(16);
						m_susWatcher.CheckState();
						if (ret == 0) {
							ret = UpdateHeadingState();
							if (ret == 0) {
								if (m_providerLoaded != 0) {
									if (!m_permMgr->IsGloballyEnabled() ||
									    !m_permMgr->HasAnyAllowedApp())
										ret = m_permMgr->ProviderSuspend(
											m_provider);
									else
										ret = m_permMgr->ProviderResume(
											m_provider);
								}
								if (ret == 0)
									ret = UpdateEmuMode(
										&m_config->m_methodState
											 .m_lmethod);
							}
						}
						if (ret != 0)
							Close(appId, *handle);
					} else {
						ret = SCE_LOCATION_ERROR_FATAL_ERROR;
						Close(appId, *handle);
					}

					if (ret == 0 && (*mode & 0xFFFFFF00u) == 0 &&
					    (*lmethod & SCE_LOCATION_LMETHOD_MASK) != 0 &&
					    m_handleMgr->CheckPermission(*handle) ==
						    SCE_LOCATION_INFO_DENIED_BY_USER) {
						bool canConfirm = false;

						if (m_handleMgr->NeedsConfirm(*handle,
									      &canConfirm) == 0 &&
						    canConfirm) {
							LocationConfirmMgr *confirm =
								locationConfirmGetInstance();

							if (confirm != NULL) {
								if (g_locationLogger != NULL)
									g_locationLogger->Log(
										8,
										"PositionManager::Open() ConfirmStart\n");
								confirm->Show(appId, *handle, 0, 0,
									      NULL, false);
								m_handleMgr->SetPendingConfirm(
									*handle, 1);
								m_handleMgr->SetPendingRequest(
									*handle, 0, 128, NULL);
								ret = SCE_LOCATION_INFO_DENIED_BY_USER;
							}
						}
					}
				}
			}
		}
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::Open() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118CA5C) */
int PositionManager::Reopen(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
			    SceUInt32 *hmethod)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::Reopen() START\n");

	int ret;

	if (m_handleMgr->IsValidHandle(appId, handle)) {
		int flags = m_handleMgr->GetStateFlags(appId, handle);

		if ((flags & 4) != 0)
			CancelGetLocation(appId, handle);
		if ((flags & 0x40) != 0)
			CancelGetLocation(appId, handle);
		if ((flags & 8) != 0)
			StopLocationCallback(appId, handle);
		if ((flags & 0x10) != 0)
			StopHeadingCallback(appId, handle);

		if (vshSblAimgrIsDEX() || vshSblAimgrIsTool())
			m_config->LoadDebugPermission();

		SceUInt32 lm = *lmethod;

		ret = ValidateLMethod(&lm);
		if (ret == 0) {
			if ((lm & SCE_LOCATION_LMETHOD_MASK) != 0 &&
			    m_handleMgr->CheckPermission(handle) ==
				    SCE_LOCATION_ERROR_DISABLE_APPLICATION)
				ret = SCE_LOCATION_ERROR_DISABLE_APPLICATION;
			if (ret != SCE_LOCATION_ERROR_DISABLE_APPLICATION) {
				ret = m_handleMgr->Reopen(appId, handle, &lm, hmethod);
				if (ret == 0) {
					SceUInt32 activeL = 0;
					SceUInt32 activeH = 0;

					ret = m_handleMgr->GetActiveMethods(&activeL, &activeH);
					if (ret == 0) {
						m_config->SetLMethod(&activeL);
						m_config->SetHMethod(&activeH);
						m_config->m_methodState.m_flags =
							(int)(activeL & 0x100);
					}

					bool loadedNow = false;
					SceUInt32 lmBits = m_config->m_methodState.m_lmethod &
							   SCE_LOCATION_LMETHOD_MASK;
					SceUInt32 hmBits = m_config->m_methodState.m_hmethod &
							   SCE_LOCATION_HMETHOD_MASK;

					if (m_handleMgr->GetCount() > 0 && m_providerLoaded == 0 &&
					    (lmBits | hmBits) != 0) {
						int lr = LoadProvider();

						loadedNow = (lr >= 0);
						if (lr != 0)
							ret = lr;
					}
					if (m_providerLoaded != 0) {
						int pr;

						if (!m_permMgr->IsGloballyEnabled() ||
						    !m_permMgr->HasAnyAllowedApp())
							pr = m_permMgr->ProviderSuspend(m_provider);
						else
							pr = m_permMgr->ProviderResume(m_provider);
						if (pr != 0)
							ret = pr;
					}
					if (loadedNow)
						m_susWatcher.Schedule(16);
					m_susWatcher.CheckState();

					int hr = UpdateHeadingState();

					if (hr != 0)
						ret = hr;

					int er = UpdateEmuMode(&m_config->m_methodState.m_lmethod);

					if (er != 0)
						ret = er;

					SceUInt32 mode = 0;

					m_handleMgr->GetMode(appId, handle, &mode);
					if (ret == 0 && (mode & 0xFFFFFF00u) == 0 &&
					    (*lmethod & SCE_LOCATION_LMETHOD_MASK) != 0 &&
					    m_handleMgr->CheckPermission(handle) ==
						    SCE_LOCATION_INFO_DENIED_BY_USER) {
						bool canConfirm = false;

						if (m_handleMgr->NeedsConfirm(handle,
									      &canConfirm) == 0 &&
						    canConfirm) {
							LocationConfirmMgr *confirm =
								locationConfirmGetInstance();

							if (confirm != NULL) {
								if (g_locationLogger != NULL)
									g_locationLogger->Log(
										8,
										"PositionManager::Reopen() ConfirmStart\n");
								confirm->Show(appId, handle, 0, 0,
									      NULL, false);
								m_handleMgr->SetPendingConfirm(
									handle, 1);
								m_handleMgr->SetPendingRequest(
									handle, 0, 128, NULL);
								ret = SCE_LOCATION_INFO_DENIED_BY_USER;
							}
						}
					}
				}
			}
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::Reopen() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118CDE8) */
int PositionManager::GetMethod(int appId, SceLocationHandle handle, SceUInt32 *lmethod,
			       SceUInt32 *hmethod)
{
	if (lmethod == NULL && hmethod == NULL)
		return SCE_LOCATION_ERROR_INVALID_ADDRESS;
	if (!m_handleMgr->IsValidHandle(appId, handle))
		return SCE_LOCATION_ERROR_INVALID_HANDLE;

	SceUInt32 lm = 0;
	SceUInt32 hm = 0;

	m_config->GetMethod(&lm, NULL);
	m_handleMgr->GetMethod(appId, handle, NULL, &hm);
	if (lmethod != NULL) {
		int r = m_handleMgr->CheckPermission(handle);

		if (r != SCE_LOCATION_INFO_DENIED_BY_USER && r != 0)
			return r;
		*lmethod = lm;
	}
	if (hmethod != NULL)
		*hmethod = hm;
	return 0;
}

/* (0x8118CE9E) */
int PositionManager::ComparePriority(const SceUInt32 *lhs, const SceUInt32 *rhs)
{
	if (locationDecodePriority(lhs) != locationDecodePriority(rhs))
		return (locationDecodePriority(lhs) < locationDecodePriority(rhs)) ? 1 : -1;
	return 0;
}

/* (0x8118CEDC) */
int PositionManager::IsAppActive(int appId, bool *outActive)
{
	SceAppMgrAppStatus status;
	int ret = sceAppMgrGetStatusByAppId(appId, &status);

	if (ret == 0)
		*outActive = (status.activated != 0 && status.processRunRequested != 0);
	return ret;
}

/* (0x8118CF20) */
int PositionManager::GetQueryLocation(int appId, SceLocationHandle handle, int unused,
				      SceRtcTick timestamp, SceUInt32 timeout,
				      SceLocationLocationInfo *info, bool option)
{
	(void)unused;
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetQueryLocation() START\n");

	SceUInt32 lmethod = 0;
	int ret;

	if (m_handleMgr->GetMethod(appId, handle, &lmethod, NULL) != 0) {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	} else {
		ret = m_handleMgr->CheckPermission(handle);
		if (ret != 0) {
			if (ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
				if ((lmethod & SCE_LOCATION_LMETHOD_MASK) != 0) {
					SceUInt32 mode = 0;

					m_handleMgr->GetMode(appId, handle, &mode);
					if ((mode & 0xFFFFFF00u) == SCE_LOCATION_MODE_CONFIRM_AUTO) {
						bool active = false;
						bool canConfirm = false;
						int stRet = IsAppActive(appId, &active);

						if (m_handleMgr->NeedsConfirm(handle,
									      &canConfirm) != 0 ||
						    !canConfirm || stRet != 0 || !active) {
							m_notifyEventCb(
								handle,
								SCE_LOCATION_INFO_DENIED_BY_USER,
								7);
						} else if (!option) {
							LocationConfirmMgr *confirm =
								locationConfirmGetInstance();

							if (confirm != NULL) {
								if (g_locationLogger != NULL)
									g_locationLogger->Log(
										8,
										"PositionManager::GetQueryLocation() ConfirmStart\n");
								confirm->Show(appId, handle, 0, 0,
									      NULL, false);
								m_handleMgr->SetPendingConfirm(
									handle, 1);
								m_handleMgr->SetPendingRequest(
									handle, (int)timeout, 8,
									&timestamp);
								m_notifyEventCb(
									handle,
									SCE_LOCATION_INFO_DENIED_BY_USER,
									6);
							}
						}
					}
				} else {
					ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
				}
			}
		} else if ((lmethod & SCE_LOCATION_LMETHOD_MASK) != 0) {
			if (m_provider != NULL) {
				SceAppMgrAppStatus status;

				ret = sceAppMgrGetStatusByAppId(appId, &status);
				if (ret == 0) {
					ret = m_handleMgr->StartGetLocation(handle, timeout, 4,
									    &timestamp);
					if (ret == 0) {
						ret = m_provider->QueryLocation(
							handle, status.appName, &timestamp,
							option ? 1 : 0);
						if (ret != 0) {
							int qstate = 0;

							m_handleMgr->GetPendingCmd(handle, &qstate);
							if (option || qstate != 8)
								m_handleMgr->CancelGetLocation(
									handle, 4);
							else
								m_handleMgr->SetPendingError(handle,
											     ret);
						} else {
							ret = SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID;
						}
					}
				}
			}
		} else {
			ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
		}
	}
	locationInitLocationInfo(info);
	info->timestamp = timestamp;
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetQueryLocation() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D164) */
int PositionManager::GetQueryLocationCache(int appId, SceLocationHandle handle,
					   SceLocationLocationInfo *info)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetQueryLocationCache() START\n");

	int ret = m_handleMgr->CheckPermission(handle);

	if (ret == 0) {
		if (m_provider != NULL) {
			if ((m_handleMgr->GetStateFlags(appId, handle) & 0x40) != 0) {
				ret = m_handleMgr->GetQueryResult(handle, info);

				int cr = m_handleMgr->CancelGetLocation(handle, 4);

				if (cr != 0)
					ret = cr;
				if (ret != SCE_LOCATION_ERROR_FATAL_ERROR &&
				    ret != SCE_LOCATION_INFO_NETWORK_NOT_ACCESSED) {
					locationStatsCountQueryRequested();
					if (ret == 0)
						locationStatsCountQuerySucceeded();
				}
			}
		} else {
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetQueryLocationCache() END(0x%x)\n",
				      ret);
	return ret;
}

/* (0x8118D220) */
int PositionManager::CancelQueryLocation(int appId, SceLocationHandle handle)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::CancelQueryLocation() START\n");

	int ret;

	if (m_handleMgr->IsValidHandle(appId, handle)) {
		ret = m_handleMgr->CheckPermission(handle);
		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			unsigned char pending = 0;

			m_handleMgr->GetPendingConfirm(handle, &pending);
			if (pending != 0) {
				LocationConfirmMgr *confirm = locationConfirmGetInstance();

				if (confirm != NULL)
					ret = confirm->Abort(appId, handle);
				m_handleMgr->SetPendingConfirm(handle, 0);
			} else {
				ret = m_handleMgr->CancelGetLocation(handle, 4);
				if (ret == 0)
					ret = m_provider->CancelQueryLocation(handle);
			}
		}
	} else {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::CancelQueryLocation() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D306) */
int PositionManager::GetLocation(int appId, SceLocationHandle handle, SceUInt32 timeout,
				 SceLocationLocationInfo *info, bool wifiHistory)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetPosition() START\n");

	SceUInt32 lmethod = 0;
	int ret;

	if (m_handleMgr->GetMethod(appId, handle, &lmethod, NULL) != 0) {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
	} else {
		ret = m_handleMgr->CheckPermission(handle);
		if (ret != 0) {
			if (ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
				if ((lmethod & SCE_LOCATION_LMETHOD_MASK) == 0) {
					ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
				} else {
					SceUInt32 mode = 0;

					m_handleMgr->GetMode(appId, handle, &mode);
					if ((mode & 0xFFFFFF00u) ==
					    SCE_LOCATION_MODE_CONFIRM_AUTO) {
						bool active = false;
						bool canConfirm = false;
						int stRet = IsAppActive(appId, &active);

						if (m_handleMgr->NeedsConfirm(handle,
									      &canConfirm) != 0 ||
						    !canConfirm || stRet != 0 || !active) {
							m_notifyEventCb(
								handle,
								SCE_LOCATION_INFO_DENIED_BY_USER,
								7);
						} else {
							LocationConfirmMgr *confirm =
								locationConfirmGetInstance();

							if (confirm != NULL) {
								if (g_locationLogger != NULL)
									g_locationLogger->Log(
										8,
										"PositionManager::GetLocation() ConfirmStart\n");
								confirm->Show(appId, handle, 0, 0,
									      NULL, false);
								m_handleMgr->SetPendingConfirm(
									handle, 1);
								m_handleMgr->SetPendingRequest(
									handle, (int)timeout,
									wifiHistory ? 4 : 1, NULL);
								m_notifyEventCb(
									handle,
									SCE_LOCATION_INFO_DENIED_BY_USER,
									6);
							}
						}
					}
				}
			}
		} else if ((lmethod & SCE_LOCATION_LMETHOD_MASK) == 0) {
			ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
		} else {
			SceUInt32 mode = 0;

			m_handleMgr->GetMode(appId, handle, &mode);
			if (((mode & 0xFF) != 0 || (mode & 0xF0000) == 0x10000) &&
			    !m_config->IsDeviceEnabled()) {
				ret = SCE_LOCATION_INFO_DISABLE_DEVICE;
			} else if (m_provider == NULL) {
				ret = SCE_LOCATION_ERROR_FATAL_ERROR;
			} else {
				if (wifiHistory && m_scanMode.m_apScanMode == 0)
					ret = m_scanMode.SetMode(1);

				if (ret == 0) {
					ret = m_provider->GetLocation(info);
					if (wifiHistory)
						locationStatsCountWifiHistoryCalled();
					if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID) {
						SceUInt32 reqType = wifiHistory ? 2 : 1;
						int sr = m_handleMgr->StartGetLocation(
							handle, timeout, reqType, NULL);

						if (sr == 0) {
							m_bridge->UpdateInterval(0);
							sr = m_permMgr->ProviderStart(m_provider);
							if (sr != 0) {
								m_handleMgr->CancelGetLocation(
									handle, reqType);
								ret = sr;
							}
						} else {
							ret = sr;
						}
					} else if (wifiHistory) {
						unsigned char hasWh = 0;
						int wr = m_handleMgr->HasActiveWifiHistory(&hasWh);

						if ((wr | hasWh) == 0)
							wr = m_scanMode.SetMode(0);
						if (wr != 0)
							ret = wr;
					}
				}
			}
		}
	}

	if (ret != 0)
		locationInitLocationInfo(info);
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetPosition() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D60C) */
int PositionManager::GetLocationCache(int appId, SceLocationHandle handle,
				      SceLocationLocationInfo *info, bool wifiHistory,
				      bool option)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetLocationCache() START\n");

	int ret = m_handleMgr->CheckPermission(handle);

	if (ret == 0) {
		if (m_provider != NULL) {
			if ((m_handleMgr->GetStateFlags(appId, handle) & 0x20) != 0 &&
			    m_handleMgr->CancelGetLocation(handle, 3) == 0 &&
			    m_permMgr->ProviderStop(m_provider) == 0)
				m_bridge->UpdateInterval(0);

			SceUInt32 mode = 0;

			m_handleMgr->GetMode(appId, handle, &mode);
			if (((mode & 0xFF) != 0 || (mode & 0xF0000) == 0x10000) &&
			    !m_config->IsDeviceEnabled())
				ret = SCE_LOCATION_INFO_DISABLE_DEVICE;
			else
				ret = m_provider->GetLocationCache(info);

			if (wifiHistory) {
				if (ret == SCE_LOCATION_INFO_UNDETERMINED_LOCATION) {
					SceAppMgrAppStatus status;
					int stRet = sceAppMgrGetStatusByAppId(appId, &status);

					if (stRet != 0) {
						ret = stRet;
					} else {
						int hr = m_provider->SaveHistory(
							status.appName, &info->timestamp,
							option ? 1 : 0);

						if (hr != 0)
							ret = hr;
					}
					locationStatsCountWifiHistoryFailed();
				}

				unsigned char hasWh = 0;
				int wr = m_handleMgr->HasActiveWifiHistory(&hasWh);

				if ((wr | hasWh) == 0)
					wr = m_scanMode.SetMode(0);
				if (wr < 0)
					ret = wr;
			}
		} else {
			ret = SCE_LOCATION_ERROR_FATAL_ERROR;
		}
	}
	if (ret != 0 && ret != SCE_LOCATION_INFO_UNDETERMINED_LOCATION)
		locationInitLocationInfo(info);
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetLocationCache() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D790) */
int PositionManager::StartLocationCallback(int appId, SceLocationHandle handle, SceUInt32 distance)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::StartLocationCallback() START\n");

	SceUInt32 lmethod = 0;
	int ret = m_handleMgr->GetMethod(appId, handle, &lmethod, NULL);

	if (ret == 0) {
		ret = m_handleMgr->CheckPermission(handle);
		if (ret == 0 || ret == SCE_LOCATION_INFO_DENIED_BY_USER) {
			if ((lmethod & SCE_LOCATION_LMETHOD_MASK) != 0) {
				ret = m_handleMgr->StartLocationCallback(appId, handle, distance);
				if (ret == 0) {
					m_bridge->UpdateInterval(0);
					ret = m_permMgr->ProviderStart(m_provider);
					if (m_logger != NULL)
						m_logger->ResetDebugInfo();
				}
			} else {
				ret = SCE_LOCATION_ERROR_INVALID_LOCATION_METHOD;
			}
		}
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::StartLocationCallback() END(0x%x)\n",
				      ret);
	return ret;
}

/* (0x8118D866) */
int PositionManager::GetHeading(int appId, SceLocationHandle handle, SceLocationHeadingInfo *info)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetHeading() START\n");

	SceUInt32 hmethod = 0;
	int ret = m_handleMgr->GetMethod(appId, handle, NULL, &hmethod);

	if (ret == 0) {
		if ((hmethod & SCE_LOCATION_HMETHOD_MASK) != 0) {
			float prevHeading = 0.0f;

			ret = m_handleMgr->GetCachedHeading(handle, &prevHeading);
			if (ret == 0) {
				ret = m_headingMgr->GetHeading(&hmethod, info);
				if (ret == 0)
					m_handleMgr->SetCachedHeading(handle, info->heading);
			}
		} else {
			ret = SCE_LOCATION_ERROR_INVALID_HEADING_METHOD;
		}
	}
	if (ret != 0)
		locationInitHeadingInfo(info);
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetHeading() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118D936) */
int PositionManager::StartHeadingCallback(int appId, SceLocationHandle handle, float angle)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::StartHeadingCallback() START\n");

	SceUInt32 hmethod = 0;
	int ret = m_handleMgr->GetMethod(appId, handle, NULL, &hmethod);

	if (ret == 0) {
		if ((hmethod & SCE_LOCATION_HMETHOD_MASK) != 0)
			ret = m_handleMgr->StartHeadingCallback(appId, handle, angle);
		else
			ret = SCE_LOCATION_ERROR_INVALID_HEADING_METHOD;
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::StartHeadingCallback() END(0x%x)\n",
				      ret);
	return ret;
}

/* (0x8118D9E4) */
int PositionManager::GetPermission(int appId, SceLocationHandle handle,
				   SceLocationPermissionInfo *info)
{
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetPermissionStatus() START\n");

	int ret;

	if (!m_handleMgr->IsValidHandle(appId, handle)) {
		ret = SCE_LOCATION_ERROR_INVALID_HANDLE;
		sce_paf_memset(info, 0, sizeof(*info));
	} else {
		int permRet = m_handleMgr->CheckPermission(handle);

		if (permRet != SCE_LOCATION_INFO_DENIED_BY_USER && permRet != 0) {
			ret = permRet;
		} else {
			info->parentalstatus = m_config->GetParental();
			info->mainstatus = m_config->IsServiceEnabled() ? 1 : 0;

			int ar = m_handleMgr->GetHandlePermission(handle, &info->applicationstatus);

			ret = (ar == 0) ? permRet : ar;

			bool canConfirm = false;
			int cr = m_handleMgr->NeedsConfirm(handle, &canConfirm);

			info->updated = (cr != 0 || canConfirm) ? 1 : 0;
		}
		if (ret != SCE_LOCATION_INFO_DENIED_BY_USER && ret != 0)
			sce_paf_memset(info, 0, sizeof(*info));
	}
	if (g_locationLogger != NULL)
		g_locationLogger->Log(8, "PositionManager::GetPermissionStatus() END(0x%x)\n", ret);
	return ret;
}

/* (0x8118DAE4) */
int PositionManager::NotifyHeadingUpdate(int result, SceUInt32 elapsed)
{
	return m_handleMgr->NotifyHeadingUpdate(result, elapsed);
}

/* (0x8118DAEE) */
int PositionManager::NotifyEvent(SceLocationHandle handle, int result, int eventType)
{
	if (result == SCE_LOCATION_INFO_TIMEOUT)
		m_provider->CancelQueryLocation(handle);
	return m_notifyEventCb(handle, result, eventType);
}

/* (0x8118DB20) */
int PositionManager::NotifyHeading(SceLocationHandle handle, int unused)
{
	return m_notifyHeadingCb(handle, unused);
}

/* (0x8118DB2E) */
double PositionManager::CalcDistance(const SceLocationLocationInfo *fromPos,
				     const SceLocationLocationInfo *toPos)
{
	double dlon = fromPos->longitude - toPos->longitude;

	if (fabsf((float)dlon) > 180.0f) {
		if (fromPos->longitude > toPos->longitude)
			dlon = fromPos->longitude - (toPos->longitude + 360.0);
		else
			dlon = (fromPos->longitude + 360.0) - toPos->longitude;
	}

	const double pi = 3.14159265358979323846;
	double avgLat = (fromPos->latitude + toPos->latitude) * 0.5 * pi / 180.0;
	double dlat = (fromPos->latitude - toPos->latitude) * pi / 180.0;

	dlon = dlon * pi / 180.0;

	double sinLat = sce_paf_sinf((float)avgLat);
	double w = sqrtf((float)(1.0 - 0.006694380022900788 * sinLat * sinLat));
	double m = 6335439.32708387 / (w * w * w);
	double n = 6378137.0 / w;
	double dx = n * dlon * sce_paf_cosf((float)avgLat);
	double dy = m * dlat;

	return sqrtf((float)(dx * dx + dy * dy));
}

/* (0x8118DC8A) */
int PositionManager::SetAppPermByTitleId(const char *titleId, int status)
{
	int ret = m_permMgr->SetPermission(titleId, status, -1);

	if (ret == 0) {
		signed char slot = m_permMgr->FindSlot(titleId);

		ret = m_handleMgr->OnPrivacySlotChanged(slot);
		if (ret == 0) {
			if (sce_paf_strncmp(titleId, "NPXS10004", 32) == 0) {
				char alt[32];

				sce_paf_strncpy(alt, "NPXS10095", 32);
				ret = m_permMgr->SetPermission(alt, status, -1);
				if (ret != 0)
					return ret;
				slot = m_permMgr->FindSlot(alt);
				ret = m_handleMgr->OnPrivacySlotChanged(slot);
			}
			if (ret == 0 && m_providerLoaded != 0) {
				if (!m_permMgr->IsGloballyEnabled() ||
				    !m_permMgr->HasAnyAllowedApp())
					return m_permMgr->ProviderSuspend(m_provider);
				return m_permMgr->ProviderResume(m_provider);
			}
		}
	}
	return ret;
}

/* (0x8118DD5C) */
int PositionManager::ResetAllPerms(int val)
{
	int ret = m_permMgr->SetAllPermissions(val);

	if (ret == 0) {
		ret = m_handleMgr->OnGlobalPermissionChanged();
		if (ret == 0 && m_providerLoaded != 0) {
			if (!m_permMgr->IsGloballyEnabled() || !m_permMgr->HasAnyAllowedApp())
				return m_permMgr->ProviderSuspend(m_provider);
			return m_permMgr->ProviderResume(m_provider);
		}
	}
	return ret;
}

/* (0x8118DDA6) */
int PositionManager::GetMainPerm(int *out)
{
	*out = m_config->IsServiceEnabled() ? 1 : 0;
	return 0;
}

/* (0x8118DDB6) */
int PositionManager::SetMainPerm(int val)
{
	if (m_config->m_enableLocale != 1)
		return 0;

	int ret = m_config->SetServiceEnabled(val);

	if (ret == 0) {
		if (m_providerLoaded != 0) {
			if (!m_permMgr->IsGloballyEnabled() || !m_permMgr->HasAnyAllowedApp())
				ret = m_permMgr->ProviderSuspend(m_provider);
			else
				ret = m_permMgr->ProviderResume(m_provider);
		}
		int r = m_handleMgr->ClearPermissionErrors();

		if (r != 0)
			return r;
	}
	return ret;
}

/* (0x8118DE0C) */
int PositionManager::GetParentalPerm(int *out)
{
	*out = m_config->GetParental();
	return 0;
}

/* (0x8118DE1C) */
int PositionManager::SetParentalPerm(int val)
{
	int ret = m_config->SetParental(val);

	if (ret == 0) {
		if (m_providerLoaded != 0) {
			if (!m_permMgr->IsGloballyEnabled() || !m_permMgr->HasAnyAllowedApp())
				ret = m_permMgr->ProviderSuspend(m_provider);
			else
				ret = m_permMgr->ProviderResume(m_provider);
		}
		int r = m_handleMgr->ClearPermissionErrors();

		if (r != 0)
			return r;
	}
	return ret;
}

/* (0x8118DE68) */
int PositionManager::GetGlobalPerm(int *out)
{
	*out = m_config->m_enableLocale;
	return 0;
}

/* (0x8118DE72) */
int PositionManager::GetAppPermSetting(int *out)
{
	*out = m_config->m_agePermit;
	return 0;
}

/* (0x8118DE7C) */
int PositionManager::ReloadConfig()
{
	return m_config->ReloadAgePermit();
}

/* (0x8118DE86) */
int PositionManager::SetHandlePermission(SceLocationHandle handle, int status)
{
	int ret = m_handleMgr->CheckPermission(handle);

	if (ret == SCE_LOCATION_INFO_DENIED_BY_USER || ret == 0) {
		ret = m_handleMgr->SetHandlePermission(handle, status);
		if (ret == 0 && m_providerLoaded != 0) {
			if (!m_permMgr->IsGloballyEnabled() || !m_permMgr->HasAnyAllowedApp())
				return m_permMgr->ProviderSuspend(m_provider);
			return m_permMgr->ProviderResume(m_provider);
		}
	}
	return ret;
}

/* (0x8118DEE0) */
int PositionManager::GetHandleStatus(int appId, SceLocationHandle handle)
{
	if (m_handleMgr->IsValidHandle(appId, handle))
		return m_handleMgr->CheckPermission(handle);
	return SCE_LOCATION_ERROR_INVALID_HANDLE;
}

/* (0x8118DF06) */
int PositionManager::SetGpsEmulationFile(const char *path)
{
	if (vshSblAimgrIsDEX() || vshSblAimgrIsTool())
		return m_driverCtrl->SetEmulationFile(path);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118DF32) */
int PositionManager::ConfirmHandle(int appId, SceLocationHandle handle)
{
	return m_handleMgr->ConfirmHandle(appId, handle);
}

/* (0x8118DF3C) */
int PositionManager::GetValue(SceUInt32 *out)
{
	return m_config->GetValue(out);
}

/* (0x8118DF46) */
int PositionManager::FlyingStartResume(const char *titleId)
{
	for (int i = 0; i < 8; i++) {
		if (sce_paf_strncmp(titleId, m_flyingTitles[i], 32) == 0)
			return 0;
	}
	for (int i = 0; i < 8; i++) {
		if (m_flyingTitles[i][0] == '\0') {
			sce_paf_memcpy(m_flyingTitles[i], titleId, 32);
			m_flyingCount++;
			return 0;
		}
	}
	return 0;
}

/* (0x8118DFA6) */
int PositionManager::FlyingStartLaunch(const char *titleId)
{
	for (int i = 0; i < 8; i++) {
		if (sce_paf_strncmp(titleId, m_flyingTitles[i], 32) == 0) {
			sce_paf_memset(m_flyingTitles[i], 0, 32);
			m_flyingCount--;
			return 0;
		}
	}
	return 0;
}

/* (0x8118DFF0) */
int PositionManager::IsHandlePending(SceLocationHandle handle, unsigned char *out)
{
	return m_handleMgr->GetPendingConfirm(handle, out);
}

/* (0x8118DFFA) */
int PositionManager::SetHandlePending(SceLocationHandle handle, unsigned char pending)
{
	return m_handleMgr->SetPendingConfirm(handle, pending);
}

/* (0x8118E006) */
int PositionManager::ResumePending(SceLocationHandle handle)
{
	int appId = 0;
	int timeout = 0;
	int kind = 0;
	SceRtcTick ts;
	int eventType = 0;
	int ret = m_handleMgr->GetPendingRequest(&appId, handle, &timeout, &kind, &ts);

	if (ret == 0) {
		SceLocationLocationInfo info;

		switch (kind) {
		case 1:
			ret = GetLocation(appId, handle, (SceUInt32)timeout, &info, false);
			if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID)
				eventType = 5;
			break;
		case 4:
			ret = GetLocation(appId, handle, (SceUInt32)timeout, &info, true);
			if (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID)
				eventType = 5;
			break;
		case 8:
			ret = GetQueryLocation(appId, handle, 0, ts, (SceUInt32)timeout, &info,
					       false);
			eventType = (ret == SCE_LOCATION_ERROR_INTERNAL_FILE_INVALID) ? 5 : 3;
			break;
		case 64:
			ret = 0;
			eventType = 1;
			break;
		case 128:
			ret = 0;
			eventType = 9;
			break;
		default:
			break;
		}
		m_notifyEventCb(handle, ret, eventType);
	}
	return ret;
}

/* (0x8118E11A) */
int PositionManager::PollActive()
{
	if (m_handleMgr != NULL)
		return m_handleMgr->AreAllAppsInactive();
	return 1;
}

/* (0x8118E12C) */
int PositionManager::DbgClearUr0Files()
{
	if (m_logger != NULL)
		return m_logger->ClearUr0Files();
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E144) */
int PositionManager::DbgGetGpsSatelliteCount(int *out)
{
	if (m_logger != NULL) {
		*out = m_logger->GetGpsSatelliteCount();
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E160) */
int PositionManager::DbgGetWifiApCount(int *out)
{
	if (m_logger != NULL) {
		*out = m_logger->GetWifiApCount();
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E17C) */
int PositionManager::DbgSetLogMask(int mask)
{
	if (m_logger != NULL) {
		sceRegMgrSetKeyInt("/CONFIG/LOCATION", "enable_log", mask);
		if (mask != 0)
			return m_logger->StartLog((SceUInt32)mask);
		return m_logger->StopLog();
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E1BA) */
int PositionManager::DbgGetLogMask(int *out)
{
	if (m_logger != NULL) {
		*out = (int)m_logger->GetLogMask();
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E1D6) */
int PositionManager::DbgSetXtraAssistEnabled(int val)
{
	if (m_logger != NULL) {
		m_logger->SetXtraAssistEnabled(val);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E1EE) */
int PositionManager::DbgSetFakeWifiAs3g(int val)
{
	if (m_logger != NULL)
		return m_logger->SetFakeWifiAs3g(val);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E206) */
int PositionManager::DbgSetLogFileIndex(int idx)
{
	if (m_logger != NULL)
		return m_logger->SetFileIndex(idx);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E21E) */
int PositionManager::DbgGetCallbackTag(char *out)
{
	if (m_logger != NULL) {
		sce_paf_strncpy(out, m_logger->GetCallbackTag(), 11);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E242) */
int PositionManager::DbgTakeSensorActiveMask(unsigned char *out)
{
	if (m_logger != NULL) {
		*out = (unsigned char)m_logger->TakeSensorActiveMask();
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E25E) */
int PositionManager::DbgGetLogIoError(int *out)
{
	if (m_logger != NULL) {
		*out = (m_logger->m_ioError != 0) ? 1 : 0;
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E27A) */
int PositionManager::DbgSetIntervalOverrideSec(int val)
{
	if (m_logger != NULL) {
		m_logger->SetIntervalOverrideSec(val);
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E292) */
int PositionManager::DbgGetIntervalOverrideSec(int *out)
{
	if (m_logger != NULL) {
		*out = m_logger->GetIntervalOverrideSec();
		return 0;
	}
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E2AE) */
int PositionManager::DbgResetDebugPermission()
{
	if (m_logger != NULL)
		return m_logger->ResetDebugPermission();
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E2C6) */
int PositionManager::DbgGetXtraState(int *out)
{
	if (m_logger == NULL)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
	*out = m_logger->GetXtraState();
	return 0;
}

/* (0x8118E2E2) */
int PositionManager::DbgSetXtraLocalFileOnly(int val)
{
	if (m_logger != NULL)
		return m_logger->SetXtraLocalFileOnly(val);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E2FA) */
int PositionManager::DbgUnloadProvider()
{
	if (m_handleMgr->GetCount() != 0)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
	if (m_provider == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	m_permMgr->ProviderResume(m_provider);
	sceKernelLockLwMutex(&m_susWatcher.m_mutex, 1, NULL);
	m_susWatcher.m_provider = NULL;
	sceKernelUnlockLwMutex(&m_susWatcher.m_mutex, 1);
	m_bridge->SetProvider(NULL);

	if (system::Model::GetType() == 0) {
		SceLibLocationProvider_8A796686(m_provider);
		SceLibLocationProvider_2A97FD02();
	} else {
		SceLibLocationDolceProvide_AF76021F(m_provider);
	}

	SceUID modId = m_providerModId;
	int status = 0;

	m_providerLoaded = 0;
	m_provider = NULL;
	int ret = sceKernelStopUnloadModule(modId, 0, NULL, 0, NULL, &status);

	m_driverCtrl->SetPowerStatus(0);
	return ret;
}

/* (0x8118E39E) */
int PositionManager::DbgSetDopBounds(const float *inBounds)
{
	if (m_logger != NULL)
		return m_logger->SetDopBounds(inBounds);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E3B6) */
int PositionManager::DbgGetDopInfo(float *outDop)
{
	if (m_logger != NULL)
		return m_logger->GetDopInfo(outDop);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E3CE) */
int PositionManager::DbgCountUr0Files(int *out)
{
	if (m_logger != NULL)
		return m_logger->CountUr0Files(out);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E3E6) */
int PositionManager::DbgGetGpsSnapshot(void *out)
{
	if (m_logger != NULL)
		return m_logger->GetGpsSnapshot(out);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E3FE) */
int PositionManager::DbgSyncDatFiles(int fromUx0)
{
	if (m_logger != NULL)
		return m_logger->SyncDatFiles(fromUx0);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E416) */
int PositionManager::DbgSetForceXtraDownload(int val)
{
	if (m_logger != NULL)
		return m_logger->SetForceXtraDownload(val);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E42E) */
int PositionManager::DbgSetXtraSourceMode(int val)
{
	if (m_logger != NULL)
		return m_logger->SetXtraSourceMode(val);
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E446) */
int PositionManager::DbgExportHistory()
{
	if (m_logger != NULL)
		return m_logger->ExportHistoryFiles();
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E45E) */
int PositionManager::DbgImportHistory()
{
	if (m_logger != NULL)
		return m_logger->ImportHistoryFiles();
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E476) */
int PositionManager::DbgClearHistory()
{
	if (m_logger != NULL)
		return m_logger->ClearHistoryFiles();
	return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;
}

/* (0x8118E48E) */
int PositionManager::DbgGetCellInfo(int *outRadioIf, int *outCellInfo)
{
	if (m_logger == NULL)
		return SCE_LOCATION_ERROR_DISABLE_DEBUG_API;

	int ret = m_logger->GetCellInfo(outRadioIf, outCellInfo);
	LocationDebugInfo *debugInfo = m_logger->m_debugInfo;

	if (debugInfo != NULL) {
		debugInfo->cellRadioIf = 0;
		sce_paf_memset(debugInfo->cellInfo, 0, sizeof(debugInfo->cellInfo));
	}
	return ret;
}

/* (0x8118E4C0) */
int PositionManager::DbgSetGpsTimeoutMs(int val)
{
	m_config->m_gpsTimeoutMs = val;
	return 0;
}

/* (0x8118E4C8) */
int PositionManager::DbgSetFlyingStartIntervalSec(int sec)
{
	m_config->m_flyingStartInterval = (unsigned int)(1000000 * sec);
	return 0;
}

/* (0x8118E4E2, 0x8118E65E) */
PositionManager::~PositionManager()
{
	g_locationManager->UnregisterTask(&m_configRefreshTask);
	g_locationManager->UnregisterTask(&m_powerRegTask);
	if (m_powerRegTask.m_cbId >= 0) {
		scePowerUnregisterCallback(m_powerRegTask.m_cbId);
		sceKernelDeleteCallback(m_powerRegTask.m_cbId);
		m_powerRegTask.m_cbId = -1;
	}
	if (m_providerLoaded != 0 && m_provider != NULL) {
		m_permMgr->ProviderResume(m_provider);
		sceKernelLockLwMutex(&m_susWatcher.m_mutex, 1, NULL);
		m_susWatcher.m_provider = NULL;
		sceKernelUnlockLwMutex(&m_susWatcher.m_mutex, 1);
		m_bridge->SetProvider(NULL);
		if (system::Model::GetType() == 0) {
			SceLibLocationProvider_8A796686(m_provider);
			SceLibLocationProvider_2A97FD02();
		} else {
			SceLibLocationDolceProvide_AF76021F(m_provider);
		}

		int status = 0;

		m_providerLoaded = 0;
		m_provider = NULL;
		sceKernelStopUnloadModule(m_providerModId, 0, NULL, 0, NULL, &status);
		m_driverCtrl->SetPowerStatus(0);
	}
	locationGpsTerm();
	if (m_logger != NULL) {
		m_logger->Term();
		delete m_logger;
	}
	g_locationManager->UnregisterTask(&m_susWatcher);
	if (m_permMgr != NULL)
		delete m_permMgr;
	if (m_config != NULL)
		delete m_config;
	if (m_bridge != NULL)
		delete m_bridge;
	if (m_headingMgr != NULL)
		delete m_headingMgr;
	if (m_handleMgr != NULL)
		delete m_handleMgr;
	if (m_driverCtrl != NULL)
		delete m_driverCtrl;
}

/* (0x8118E66E) */
int PositionManager::ProviderEventCB(const SceLocationLocationInfo *info, int result,
				     SceUInt32 elapsed, const SceRtcTick *queryTs)
{
	if (s_positionManager != NULL)
		return s_positionManager->m_handleMgr->NotifyIfPositionConditionMet(
			info, result, elapsed, queryTs);
	return SCE_LOCATION_ERROR_FATAL_ERROR;
}

/* (0x8118E6A2) */
int PositionManager::OnModeChange(int prev, int next)
{
	if (m_provider != NULL) {
		if (prev == 1 && next == 0)
			return m_provider->StopWifiHistory();
		if (prev == 0 && next == 1)
			return m_provider->StartWifiHistory();
	}
	return 0;
}
