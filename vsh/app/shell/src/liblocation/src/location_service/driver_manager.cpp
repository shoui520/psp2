/*
 * driver_manager.cpp: DriverManager (GPS, WiFi, 3G/BBMC, and GPS-emulation hardware bridge;
 * 0x811932A4-0x81193DF4, sti 0x81193D84). Sony's module tag ("[driver_man]") comes from its
 * log strings.
 */
#include "location_service.h"
#include <libnetctl.h>
#include <std/string.h>
#include <std/new>

extern "C" {
int SceNetCtl_AB1462C5(int arg);
int SceNetCtl_6A56FC3A(int scanType, int param, int flags);
int SceNetCtl_A840C74C(int outBuf, int maxCount);
int SceNetCtl_1A254169(int cmd, int arg, int opt1, int opt2);
int sceBbmcSubscribeFeatureId(int featureId, int subId, int cbId);
int sceBbmcUnsubscribeFeature(int handle);
int sceBbmcClearEvent(int featureId, int eventMask);
int sceBbmcResumeSubscriberCallback(int handle);
int sceBbmcGpsCellGetLocationInfoStart(int handle);
int sceBbmcGpsCellGetLocationInfoDone(int handle, void *outInfo);
}

extern void *g_locationManager;				/* 0x815496B4 */
extern void *g_locationLogger;				/* 0x815496B8 */
extern void *g_locationConfig;				/* 0x815496C0 */
extern void locationLoggerPrintf(void *logger, int level, const char *fmt, ...);
extern int locationLoggerSetField8(void *logger, int val); /* 0x811896EC */
extern int locationLoggerGetDopBounds(void *logger, float *outBounds); /* 0x8118979C */
extern int locationLoggerSetDopInfo(void *logger, const void *dop); /* 0x811897DC */

extern int locationTaskSchedule(void *task, unsigned int delayMs); /* 0x8118A780 */
extern int locationTaskCancel(void *task);		/* 0x8118A800 */
extern int locationMgrRegisterTask(void *mgr, void *task); /* 0x8118AACE */
extern int locationMgrUnregisterTask(void *mgr, void *task); /* 0x8118AB62 */

extern int gpsCtrlInit(void *gpsCtrl);			/* 0x81194160 */
extern int gpsCtrlTerm(void *gpsCtrl);			/* 0x811941EE */
extern int gpsCtrlAddListener(void *gpsCtrl, void *listener); /* 0x81194294 */
extern int gpsCtrlRemoveListener(void *gpsCtrl, void *listener); /* 0x811942C4 */
extern int flyingStartInit(void *fs);			/* 0x81194B56 */
extern int flyingStartTerm(void *fs);			/* 0x81194BB4 */

extern int gpsXpsInit(void *xps);			/* 0x81197138 */
extern int gpsXpsTerm(void *xps);			/* 0x8119719E */
extern int gpsXpsOpen(void *xps, SceUID cbId);		/* 0x811971EE */
extern int gpsXpsStart(void *xps, int mode);		/* 0x8119725E */
extern int gpsXpsStop(void *xps);			/* 0x811972CE */
extern int gpsXpsClose(void *xps);			/* 0x81197330 */
extern int gpsXpsGetData(void *xps, void *data, int arg); /* 0x8119739A */
extern int gpsXpsGetSatellites(void *xps, void *sat);	/* 0x811973D8 */
extern int gpsAssistInit(void);				/* 0x811977E0 */
extern int gpsAssistTerm(void);				/* 0x8119785E */
extern int gpsAssistAck(void);				/* 0x8119798C */
extern void *gpsAssistGetInstance(void);		/* 0x811979BC */
extern int gpsAssistStart(void *assist);		/* 0x81197AB6 */

extern int gpsEmuInit(const char *path);		/* 0x81373368 */
extern int gpsEmuTerm(void);				/* 0x81373370 */
extern int gpsEmuOpen(void);				/* 0x81373378 */
extern int gpsEmuStart(int mode);			/* 0x813733A4 */
extern int gpsEmuStop(void);				/* 0x813733D0 */
extern int gpsEmuClose(void);				/* 0x813733F8 */
extern int gpsEmuGetSatellites(void *sat);		/* 0x81373420 */
extern int gpsEmuGetData(void *data, int arg);		/* 0x8137344E */
extern int gpsEmuSetParam(int param);			/* 0x813735CC */
extern int gpsEmuGetStatus(void);			/* 0x81413CB0 */
extern int gpsDrvAckEvent(void);			/* 0x8144FDE0 */
extern int gpsDrvSetParam(int param);			/* 0x8144FDF8 */
extern int gpsDrvGetStatusRaw(void);			/* 0x8144FE00 */

struct LocationConfigIface {
	virtual void vfn0();
	virtual int IsWifiEnabled();			/* +4 */
	virtual void vfn2();
	virtual void vfn3();
	virtual int Has3G();				/* +16 */
	virtual void vfn5();
	virtual void vfn6();
	virtual int IsApInvisible();			/* +28 */
};

class DriverNetCtlTask {
public:
	virtual ~DriverNetCtlTask() {}
	virtual int OnNotify();				/* 0x811932A4 */

	int m_queued;					/* +4 */
	int m_active;					/* +8 */
};

class DriverManager {
public:
	DriverManager();				/* 0x81193396 */
	virtual ~DriverManager();			/* 0x81193C66 / 0x81193D74 */

	int Init();					/* 0x811932B0 */
	int SetGpsPower(unsigned char on);		/* 0x811935BA */
	int SetWifiPower(unsigned char on);		/* 0x81193606 */
	int Set3GPower(unsigned char on);		/* 0x81193646 */
	int SetEmuPower(unsigned char on);		/* 0x81193692 */
	int GpsOpen(SceUID xpsCbId);			/* 0x811936E2 */
	int GpsStart(int mode);				/* 0x8119378A */
	int GpsStop();					/* 0x811937C0 */
	int GpsClose();					/* 0x81193840 */
	int GpsGetData(void *data, int arg);		/* 0x81193890 */
	int GpsGetSatellites(void *sat);		/* 0x811939CA */
	int GpsGetStatus();				/* 0x811939FA */
	int GpsSetParam(int param);			/* 0x81193A10 */
	int SetEmuFile(const char *path);		/* 0x81193A28 */
	int WifiScanStart(int scanType, int param);	/* 0x81193AAC */
	int WifiGetScanInfo(int outBuf, int maxCount);	/* 0x81193AF2 */
	int WifiExtra(int cmd, int arg);		/* 0x81193B44 */
	int BbmcSubscribe(int cbId);			/* 0x81193B68 */
	int BbmcUnsubscribe(int handle);		/* 0x81193B90 */
	int BbmcClearEvent(int handle, int mask);	/* 0x81193BA2 */
	int BbmcResumeCallback(int handle);		/* 0x81193BAE */
	int BbmcGetCellStart(int handle);		/* 0x81193BB8 */
	int BbmcGetCellDone(void *outInfo);		/* 0x81193C1C */
	void SetAssistPower(unsigned char on);		/* 0x81193C28 */

	static int GpsCallback(int notifyId, int notifyCount, int notifyArg, void *common); /* 0x81193A50 */

	unsigned char m_gpsPower;			/* +4 */
	SceUID m_xpsCbId;				/* +8 */
	SceUID m_gpsCbId;				/* +12 */
	int m_gpsMutexRes;				/* +16 */
	int m_pad20;					/* +20 */
	SceKernelLwMutexWork m_gpsMutex;		/* +24..+55 */
	unsigned char m_wifiReserved56;			/* +56 */
	unsigned char m_wifiPower;			/* +57 */
	int m_wifiMutexRes;				/* +60 */
	SceKernelLwMutexWork m_wifiMutex;		/* +64..+95 */
	unsigned char m_3gPower;			/* +96 */
	int m_3gMutexRes;				/* +100 */
	SceKernelLwMutexWork m_3gMutex;			/* +104..+135 */
	int m_bbmcHandle;				/* +136 */
	unsigned char m_emuPower;			/* +140 */
	unsigned char m_assistPower;			/* +141 */
	DriverNetCtlTask m_netCtlTask;			/* +144..+155 */
	int m_pad156;					/* +156 */
	char m_gpsCtrlBuf[128];				/* +160..+287 */
	char m_gpsXpsBuf[96];				/* +288..+383 */
	char m_flyingStartBuf[104];			/* +384..+487 */
};

static DriverManager *s_driverManager;			/* 0x81544CE8 */
static char s_emuFilePath[256];				/* 0x81566DA8 */

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
	gpsCtrlInit(m_gpsCtrlBuf);
	gpsXpsInit(m_gpsXpsBuf);
	flyingStartInit(m_flyingStartBuf);
	gpsAssistInit();
	gpsCtrlAddListener(m_gpsCtrlBuf, gpsAssistGetInstance());
	locationMgrRegisterTask(g_locationManager, &m_netCtlTask);
	return 0;
}

/* 0x81193396 */
DriverManager::DriverManager()
{
	m_assistPower = 0;
	m_3gPower = 0;
	m_wifiPower = 0;
	m_wifiReserved56 = 0;
	m_gpsPower = 0;
	m_emuPower = 0;
	m_netCtlTask.m_queued = 0;
	m_netCtlTask.m_active = 0;
	m_xpsCbId = -1;
	m_gpsCbId = -1;
	m_gpsMutexRes = -1;
	m_wifiMutexRes = -1;
	m_bbmcHandle = 0;
	m_3gMutexRes = -1;
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
		locationMgrUnregisterTask(g_locationManager, &s_driverManager->m_netCtlTask);
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
			locationLoggerPrintf(g_locationLogger, 2,
					     "[driver_man] changing gps power %d\n", on);
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
				locationTaskSchedule(&m_netCtlTask, 100);
			else
				locationTaskCancel(&m_netCtlTask);
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
			LocationConfigIface *cfg = (LocationConfigIface *)g_locationConfig;
			if (cfg != NULL && cfg->Has3G() != 0)
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
			locationLoggerPrintf(g_locationLogger, 2,
					     "[GPS] sceGpsEmuOpen()=0x%08x\n", ret);
	} else {
		if (m_gpsPower == 1) {
			ret = gpsXpsOpen(m_gpsXpsBuf, cbId);
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 2,
						     "[GPS] sceGpsOpen()=0x%08x\n", ret);
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
		return gpsXpsStart(m_gpsXpsBuf, mode | 0x10);
	return (int)0x8039000C;
}

/* 0x811937C0 */
int DriverManager::GpsStop()
{
	if (m_emuPower == 1) {
		int ret = gpsEmuStop();
		gpsXpsStop(m_gpsXpsBuf);
		return ret;
	}
	int ret = gpsXpsStop(m_gpsXpsBuf);
	if (ret == (int)0x80390004)
		ret = (int)0x80390002;
	if (g_locationLogger != NULL) {
		unsigned char zeros[16];
		sce_paf_memset(zeros, 0, sizeof(zeros));
		locationLoggerSetDopInfo(g_locationLogger, zeros);
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
		gpsXpsClose(m_gpsXpsBuf);
	} else {
		ret = gpsXpsClose(m_gpsXpsBuf);
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
int DriverManager::GpsGetData(void *data, int arg)
{
	if (m_emuPower == 1)
		return gpsEmuGetData(data, arg);
	if (m_gpsPower != 1)
		return (int)0x8039000C;

	int ret = gpsXpsGetData(m_gpsXpsBuf, data, arg);
	if (g_locationLogger != NULL) {
		float dop[4];
		dop[0] = *(float *)((char *)data + 16);
		dop[1] = *(float *)((char *)data + 20);
		dop[2] = *(float *)((char *)data + 24);
		dop[3] = 0.0f;
		locationLoggerSetDopInfo(g_locationLogger, dop);
		locationLoggerPrintf(g_locationLogger, 2,
				     "[GPS] DOP INFO hdop[%f] vdop[%f] pdop[%f] Circular[%f]\n",
				     (double)dop[0], (double)dop[1], (double)dop[2], (double)dop[3]);
		float bounds[8];
		locationLoggerGetDopBounds(g_locationLogger, bounds);
		if ((bounds[0] < dop[0] && bounds[1] > dop[0]) ||
		    (bounds[4] < dop[2] && bounds[5] > dop[2]) ||
		    (bounds[2] < dop[1] && bounds[3] > dop[1])) {
			*((unsigned char *)data + 30) = 0;
		}
	}
	return ret;
}

/* 0x811939CA */
int DriverManager::GpsGetSatellites(void *sat)
{
	if (m_emuPower == 1)
		return gpsEmuGetSatellites(sat);
	if (m_gpsPower == 1)
		return gpsXpsGetSatellites(m_gpsXpsBuf, sat);
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
			locationLoggerPrintf(g_locationLogger, 2,
					     "[GPS] GpsCallback SendToXps notify=0x%08x\n",
					     notifyArg);
		if (s_driverManager->m_xpsCbId >= 0)
			sceKernelNotifyCallback(s_driverManager->m_xpsCbId, notifyArg);
	}
	return 0;
}

/* 0x81193AAC */
int DriverManager::WifiScanStart(int scanType, int param)
{
	LocationConfigIface *cfg = (LocationConfigIface *)g_locationConfig;
	if (m_wifiPower == 1 && cfg != NULL && cfg->IsWifiEnabled() != 0) {
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
	LocationConfigIface *cfg = (LocationConfigIface *)g_locationConfig;
	if (ret < 0 || cfg->IsApInvisible() != 0) {
		if (g_locationLogger != NULL)
			locationLoggerPrintf(
				g_locationLogger, 2,
				"[WIFI] sceNetCtlInetGetScanInfoBssidIpcInt() returns [0x%x] reporting as 0 AP.\n",
				ret);
		return 0;
	}
	return ret;
}

/* 0x81193B44 */
int DriverManager::WifiExtra(int cmd, int arg)
{
	if (m_wifiPower == 1)
		return SceNetCtl_1A254169(cmd, arg, 0, 0);
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
		unsigned char dummy[328];
		sce_paf_memset(dummy, 0, sizeof(dummy));
		ret = sceBbmcGpsCellGetLocationInfoDone(handle, dummy);
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
			gpsAssistStart(gpsAssistGetInstance());
		} else {
			if (g_locationLogger != NULL)
				locationLoggerSetField8(g_locationLogger, 1);
			*((unsigned char *)gpsAssistGetInstance() + 2580) = 0;
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
	gpsCtrlRemoveListener(m_gpsCtrlBuf, gpsAssistGetInstance());
	gpsAssistTerm();
	flyingStartTerm(m_flyingStartBuf);
	gpsXpsTerm(m_gpsXpsBuf);
	gpsCtrlTerm(m_gpsCtrlBuf);
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
