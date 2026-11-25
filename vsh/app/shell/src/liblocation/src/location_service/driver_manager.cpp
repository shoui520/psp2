/*
 * driver_manager.cpp: DriverManager (GPS, WiFi, 3G/BBMC, and GPS-emulation hardware bridge;
 * 0x811932A4-0x81193DF4, sti 0x81193D84). Sony's module tag ("[driver_man]") comes from its
 * log strings.
 */
#include "location_internal.h"
#include <libnetctl.h>
#include <std/string.h>
#include <std/new>

extern "C" {
int SceNetCtl_AB1462C5(int arg);
int SceNetCtl_6A56FC3A(int scanType, int param, int flags);
int SceNetCtl_A840C74C(int outBuf, int maxCount);
int SceNetCtl_1A254169(void *bssid, void *info, void *extra, int *extraSize);
int sceBbmcSubscribeFeatureId(int featureId, int subId, int cbId);
int sceBbmcUnsubscribeFeature(int handle);
int sceBbmcClearEvent(int featureId, int eventMask);
int sceBbmcResumeSubscriberCallback(int handle);
int sceBbmcGpsCellGetLocationInfoStart(int handle);
int sceBbmcGpsCellGetLocationInfoDone(int handle, void *outInfo);
}

extern int gpsEmuInit(const char *path);			/* 0x81373368 */
extern int gpsEmuTerm(void);					/* 0x81373370 */
extern int gpsEmuOpen(void);					/* 0x81373378 */
extern int gpsEmuStart(int mode);				/* 0x813733A4 */
extern int gpsEmuStop(void);					/* 0x813733D0 */
extern int gpsEmuClose(void);					/* 0x813733F8 */
extern int gpsEmuGetSatellites(int *outStatus);			/* 0x81373420 */
extern int gpsEmuGetData(GpsLocation *data, GpsSatellites *satellites); /* 0x8137344E */
extern int gpsEmuSetParam(int param);				/* 0x813735CC */
extern int gpsEmuGetStatus(void);				/* 0x81413CB0 */
extern int gpsDrvAckEvent(void);				/* 0x8144FDE0 */
extern int gpsDrvSetParam(int param);				/* 0x8144FDF8 */
extern int gpsDrvGetStatusRaw(void);				/* 0x8144FE00 */

static DriverManager *s_driverManager;				/* 0x81544CE8 */
static char s_emuFilePath[256];					/* 0x81566DA8 */

/* 0x811932A4 */
int DriverNetCtlTask::OnNotify()
{
	sceNetCtlCheckCallback();
	return 1;
}

/* 0x811932B0 */
int DriverManager::Init()
{
	m_gpsMutexRes = sceKernelCreateLwMutex(&m_gpsMutex, "SceLocationGpsState", 2, 0, NULL);
	if (m_gpsMutexRes != 0)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	m_emuPower = 0;
	m_gpsPower = 0;
	m_assistPower = 0;

	m_wifiMutexRes = sceKernelCreateLwMutex(&m_wifiMutex, "SceLocationWifiState", 2, 0, NULL);
	if (m_wifiMutexRes != 0) {
		sceKernelDeleteLwMutex(&m_gpsMutex);
		m_gpsMutexRes = -1;
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}
	m_wifiPower = 0;

	m_3gMutexRes = sceKernelCreateLwMutex(&m_3gMutex, "SceLocation3GState", 2, 0, NULL);
	if (m_3gMutexRes != 0) {
		sceKernelDeleteLwMutex(&m_wifiMutex);
		m_wifiMutexRes = -1;
		sceKernelDeleteLwMutex(&m_gpsMutex);
		m_gpsMutexRes = -1;
		return SCE_LOCATION_ERROR_FATAL_ERROR;
	}

	m_3gPower = 0;
	m_gpsCtrl.Init();
	m_gpsXps.Init();
	m_flyingStart.Init();
	gpsAssistInit();
	m_gpsCtrl.AddListener(gpsAssistGetInstance());
	g_locationManager->RegisterTask(&m_netCtlTask);
	return 0;
}

/* 0x81193396 */
DriverManager::DriverManager()
{
	m_assistPower = 0;
	m_3gPower = 0;
	m_wifiPower = 0;
	m_wifiInitialized = 0;
	m_gpsPower = 0;
	m_emuPower = 0;
	m_netCtlTask.m_mgr = NULL;
	m_netCtlTask.m_slotIndex = 0;
	m_xpsCbId = -1;
	m_gpsCbId = -1;
	m_gpsMutexRes = -1;
	m_wifiMutexRes = -1;
	m_bbmcHandle = 0;
	m_3gMutexRes = -1;
	m_gpsCtrl.m_sm.m_mgr = &m_gpsCtrl;
	m_gpsCtrl.m_pollTask.m_gpsCtrl = &m_gpsCtrl;
	m_gpsCtrl.m_createCbTask.m_gpsCtrl = &m_gpsCtrl;
	m_gpsXps.m_gpsCtrl = &m_gpsCtrl;
	m_gpsXps.m_sm.m_ctrl = &m_gpsXps;
	m_gpsXps.m_notifyTask.m_ctrl = &m_gpsXps;
	m_flyingStart.m_gpsCtrl = &m_gpsCtrl;
	m_flyingStart.m_gpsXps = &m_gpsXps;
	m_flyingStart.m_sm.m_ctrl = &m_flyingStart;
	m_flyingStart.m_timerTask.m_sm = &m_flyingStart.m_sm;
}

/* 0x81193500 */
int locationGpsInit(void)
{
	if (s_driverManager != NULL)
		return 0;

	DriverManager *mgr = new (g_locationHeap) DriverManager();
	s_driverManager = mgr;
	if (mgr == NULL)
		return SCE_LOCATION_ERROR_NO_MEMORY;

	int ret = mgr->Init();
	if (ret != 0) {
		delete s_driverManager;
		s_driverManager = NULL;
	}
	return ret;
}

/* 0x81193570 */
void locationGpsTerm(void)
{
	if (s_driverManager != NULL) {
		g_locationManager->UnregisterTask(&s_driverManager->m_netCtlTask);
		delete s_driverManager;
		s_driverManager = NULL;
	}
}

/* 0x811935AE */
DriverManager *locationDeviceGetInstance(void)
{
	return s_driverManager;
}

/* 0x811935BA */
int DriverManager::SetGpsPower(unsigned char on)
{
	int ret = sceKernelLockLwMutex(&m_gpsMutex, 1, NULL);
	if (ret == 0) {
		m_gpsPower = on;
		if (g_locationLogger != NULL)
			g_locationLogger->Log(2, "[driver_man] changing gps power %d\n", on);
		int uret = sceKernelUnlockLwMutex(&m_gpsMutex, 1);
		if (uret < 0)
			return uret;
	}
	return ret;
}

/* 0x81193606 */
int DriverManager::SetWifiPower(unsigned char on)
{
	int ret = sceKernelLockLwMutex(&m_wifiMutex, 1, NULL);
	if (ret == 0) {
		if (m_wifiPower != on) {
			m_wifiPower = on;
			if (on != 0)
				m_netCtlTask.Schedule(100);
			else
				m_netCtlTask.Cancel();
		}
		return sceKernelUnlockLwMutex(&m_wifiMutex, 1);
	}
	return ret;
}

/* 0x81193646 */
int DriverManager::Set3GPower(unsigned char on)
{
	int ret = sceKernelLockLwMutex(&m_3gMutex, 1, NULL);
	if (ret == 0) {
		if (m_3gPower != on) {
			if (g_locationConfig != NULL && g_locationConfig->HasGps() != 0)
				m_3gPower = on;
			else
				m_3gPower = 0;
		}
		return sceKernelUnlockLwMutex(&m_3gMutex, 1);
	}
	return ret;
}

/* 0x81193692 */
int DriverManager::SetEmuPower(unsigned char on)
{
	int ret = 0;
	if (m_emuPower == 0) {
		if (on == 1) {
			ret = gpsEmuInit(s_emuFilePath);
			if (ret == 0)
				m_emuPower = 1;
		}
	} else {
		ret = gpsEmuTerm();
		if (ret == 0) {
			if (on == 1)
				return gpsEmuInit(s_emuFilePath);
			m_emuPower = 0;
		}
	}
	return ret;
}

/* 0x811936E2 */
int DriverManager::GpsOpen(SceUID xpsCbId)
{
	SceUID cbId = m_gpsCbId;
	if (cbId < 0) {
		cbId = sceKernelCreateCallback("SceLocationGpsCB", 0, GpsCallback, NULL);
		m_gpsCbId = cbId;
		if (cbId < 0)
			return cbId;
	}

	m_xpsCbId = xpsCbId;
	int ret;
	if (m_emuPower == 1) {
		ret = gpsEmuOpen();
		if (g_locationLogger != NULL)
			g_locationLogger->Log(2, "[GPS] sceGpsEmuOpen()=0x%08x\n", ret);
	} else {
		if (m_gpsPower == 1) {
			ret = m_gpsXps.Open(cbId);
			if (g_locationLogger != NULL)
				g_locationLogger->Log(2, "[GPS] sceGpsOpen()=0x%08x\n", ret);
			if (ret == 0)
				return ret;
		} else {
			ret = (int)0x80390002;
		}
		sceKernelDeleteCallback(m_gpsCbId);
		m_xpsCbId = -1;
		m_gpsCbId = -1;
	}
	return ret;
}

/* 0x8119378A */
int DriverManager::GpsStart(int mode)
{
	if (m_emuPower == 1)
		return gpsEmuStart(mode);
	if (m_gpsPower == 1)
		return m_gpsXps.Start(mode | 0x10);
	return (int)0x8039000C;
}

/* 0x811937C0 */
int DriverManager::GpsStop()
{
	if (m_emuPower == 1) {
		int ret = gpsEmuStop();
		m_gpsXps.Stop();
		return ret;
	}
	int ret = m_gpsXps.Stop();
	if (ret == (int)0x80390004)
		ret = (int)0x80390002;
	if (g_locationLogger != NULL) {
		float zeros[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
		g_locationLogger->SetDopInfo(zeros);
	}
	gpsAssistAck();
	gpsDrvAckEvent();
	return ret;
}

/* 0x81193840 */
int DriverManager::GpsClose()
{
	int ret;
	if (m_emuPower == 1) {
		ret = gpsEmuClose();
		m_gpsXps.Close();
	} else {
		ret = m_gpsXps.Close();
		if (ret == (int)0x80390004)
			ret = (int)0x80390002;
	}
	if (m_gpsCbId >= 0) {
		sceKernelDeleteCallback(m_gpsCbId);
		m_xpsCbId = -1;
		m_gpsCbId = -1;
	}
	return ret;
}

/* 0x81193890 */
int DriverManager::GpsGetData(GpsLocation *data, GpsSatellites *satellites)
{
	if (m_emuPower == 1)
		return gpsEmuGetData(data, satellites);
	if (m_gpsPower != 1)
		return (int)0x8039000C;

	int ret = m_gpsXps.GetData(data, satellites);
	if (g_locationLogger != NULL) {
		float dop[4];
		dop[0] = data->hdop;
		dop[1] = data->vdop;
		dop[2] = data->pdop;
		dop[3] = 0.0f;
		g_locationLogger->SetDopInfo(dop);
		g_locationLogger->Log(2,
				      "[GPS] DOP INFO hdop[%f] vdop[%f] pdop[%f] Circular[%f]\n",
				      (double)dop[0], (double)dop[1], (double)dop[2],
				      (double)dop[3]);
		float bounds[8];
		g_locationLogger->GetDopBounds(bounds);
		if ((bounds[0] < dop[0] && bounds[1] > dop[0]) ||
		    (bounds[4] < dop[2] && bounds[5] > dop[2]) ||
		    (bounds[2] < dop[1] && bounds[3] > dop[1])) {
			data->noClock = 0;
		}
	}
	return ret;
}

/* 0x811939CA */
int DriverManager::GpsGetSatellites(int *outStatus)
{
	if (m_emuPower == 1)
		return gpsEmuGetSatellites(outStatus);
	if (m_gpsPower == 1)
		return m_gpsXps.GetStatus(outStatus);
	return (int)0x8039000C;
}

/* 0x811939FA */
int DriverManager::GpsGetStatus()
{
	if (m_emuPower == 1)
		return gpsEmuGetStatus();
	return gpsDrvGetStatusRaw();
}

/* 0x81193A10 */
int DriverManager::GpsSetParam(int param)
{
	if (m_emuPower == 1)
		return gpsEmuSetParam(param);
	return gpsDrvSetParam(param);
}

/* 0x81193A28 */
int DriverManager::SetEmuFile(const char *path)
{
	sce_paf_memset(s_emuFilePath, 0, sizeof(s_emuFilePath));
	sce_paf_memcpy(s_emuFilePath, path, sizeof(s_emuFilePath));
	return 0;
}

/* 0x81193A50 */
int DriverManager::GpsCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)common;
	unsigned int hi = (unsigned int)notifyArg & 0xFFFF0000U;
	if (hi != 0x11000000U && hi != 0x12000000U && hi != 0x13000000U && hi != 0x00060000U) {
		if (g_locationLogger != NULL)
			g_locationLogger->Log(2, "[GPS] GpsCallback SendToXps notify=0x%08x\n",
					      notifyArg);
		if (s_driverManager->m_xpsCbId >= 0)
			sceKernelNotifyCallback(s_driverManager->m_xpsCbId, notifyArg);
	}
	return 0;
}

/* 0x81193AAC */
int DriverManager::WifiScanStart(int scanType, int param)
{
	if (m_wifiPower == 1 && g_locationConfig != NULL && g_locationConfig->IsAdhocAllowed()) {
		int ret = SceNetCtl_AB1462C5(4);
		if (ret == 0)
			return SceNetCtl_6A56FC3A(scanType, param, 0);
		return ret;
	}
	return (int)0x80412101;
}

/* 0x81193AF2 */
int DriverManager::WifiGetScanInfo(int outBuf, int maxCount)
{
	if (m_wifiPower != 1)
		return 0;

	int ret = SceNetCtl_A840C74C(outBuf, maxCount);
	if (ret < 0 || g_locationConfig->IsApInvisible() != 0) {
		if (g_locationLogger != NULL)
			g_locationLogger->Log(
				2,
				"[WIFI] sceNetCtlInetGetScanInfoBssidIpcInt() returns [0x%x] reporting as 0 AP.\n",
				ret);
		return 0;
	}
	return ret;
}

/* 0x81193B44 */
int DriverManager::WifiGetScanInfoByBssid(void *bssid, void *info)
{
	if (m_wifiPower == 1)
		return SceNetCtl_1A254169(bssid, info, NULL, NULL);
	return (int)0x80412110;
}

/* 0x81193B68 */
int DriverManager::BbmcSubscribe(int cbId)
{
	if (m_3gPower != 1)
		return (int)0x80330006;

	int ret = sceBbmcSubscribeFeatureId(3, 6, cbId);
	if (ret > 0)
		m_bbmcHandle = ret;
	return ret;
}

/* 0x81193B90 */
int DriverManager::BbmcUnsubscribe(int handle)
{
	int ret = sceBbmcUnsubscribeFeature(handle);
	m_bbmcHandle = 0;
	return ret;
}

/* 0x81193BA2 */
int DriverManager::BbmcClearEvent(int handle, int mask)
{
	return sceBbmcClearEvent(handle, mask);
}

/* 0x81193BAE */
int DriverManager::BbmcResumeCallback(int handle)
{
	return sceBbmcResumeSubscriberCallback(handle);
}

/* 0x81193BB8 */
int DriverManager::BbmcGetCellStart(int handle)
{
	if (m_3gPower != 1)
		return (int)0x80330006;

	int ret = sceBbmcGpsCellGetLocationInfoStart(handle);
	if (ret == (int)0x80330006) {
		BbmcGpsCellInfo cellInfo;
		sce_paf_memset(&cellInfo, 0, sizeof(cellInfo));
		ret = sceBbmcGpsCellGetLocationInfoDone(handle, &cellInfo);
		if (ret == 0)
			return sceBbmcGpsCellGetLocationInfoStart(handle);
	}
	return ret;
}

/* 0x81193C1C */
int DriverManager::BbmcGetCellDone(void *outInfo)
{
	return sceBbmcGpsCellGetLocationInfoDone(m_bbmcHandle, outInfo);
}

/* 0x81193C28 */
void DriverManager::SetAssistPower(unsigned char on)
{
	if (on != m_assistPower) {
		m_assistPower = on;
		if (on == 1) {
			gpsAssistGetInstance()->StartXtraDownload();
		} else {
			if (g_locationLogger != NULL)
				g_locationLogger->SetXtraAssistEnabled(1);
			gpsAssistGetInstance()->m_enabled = 0;
		}
	}
}

/* 0x81193C66 / 0x81193D74 */
DriverManager::~DriverManager()
{
	if (m_emuPower == 1) {
		gpsEmuTerm();
		m_emuPower = 0;
	}
	m_gpsCtrl.RemoveListener(gpsAssistGetInstance());
	gpsAssistTerm();
	m_flyingStart.Term();
	m_gpsXps.Term();
	m_gpsCtrl.Term();
	if (m_gpsMutexRes == 0)
		sceKernelDeleteLwMutex(&m_gpsMutex);
	if (m_wifiMutexRes == 0)
		sceKernelDeleteLwMutex(&m_wifiMutex);
	if (m_3gMutexRes == 0)
		sceKernelDeleteLwMutex(&m_3gMutex);
	if (m_gpsCbId >= 0) {
		sceKernelDeleteCallback(m_gpsCbId);
		m_xpsCbId = -1;
		m_gpsCbId = -1;
	}
}
