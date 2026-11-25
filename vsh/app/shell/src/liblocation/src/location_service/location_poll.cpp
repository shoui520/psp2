/*
 * location_poll.cpp: suspend/resume watcher task ("SceLocationSusWatcher") and WiFi scan mode
 * notifier ("SceLocationScanMode") (0x81190FEC-0x811912F0).
 * The file and class names are ours (PLACEHOLDER).
 */
#include "location_service.h"
#include <std/vector>

extern void *g_locationLogger;					/* 0x815496B8 */
extern void locationLoggerPrintf(void *logger, int level, const char *fmt, ...);
extern int locationHandleMgrGetActiveCount(void *handleMgr, int *outCount); /* 0x81192C40 */

struct LocationSusListener {
	virtual void OnResume();	/* +0 */
	virtual void OnSuspend();	/* +4 */
};

struct LocationSusProvider {
	virtual void vfn0();
	virtual void vfn1();
	virtual void vfn2();
	virtual void vfn3();
	virtual void vfn4();
	virtual int Resume();		/* +20 */
	virtual int Suspend();		/* +24 */
};

class LocationSusWatcher {
public:
	virtual ~LocationSusWatcher();		/* 0x81190FEC / 0x81191032 */
	virtual int OnNotify();			/* 0x81191194 */

	int Init(void *handleMgr);		/* 0x8119107E */
	int CheckState();			/* 0x811910A8 */

	int m_queued;				/* +4 */
	int m_active;				/* +8 */
	int m_mutexResult;			/* +12 */
	SceKernelLwMutexWork m_mutex;		/* +16 */
	unsigned char m_suspended;		/* +48 */
	void *m_handleMgr;			/* +52 */
	LocationSusProvider *m_provider;	/* +56 */
	paf::vector<LocationSusListener *> m_listeners; /* +60 */
};

/* 0x81190FEC / 0x81191032 */
LocationSusWatcher::~LocationSusWatcher()
{
	if (m_mutexResult == 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}
}

/* 0x8119107E */
int LocationSusWatcher::Init(void *handleMgr)
{
	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceLocationSusWatcher", 2, 0, NULL);
	m_handleMgr = handleMgr;
	return m_mutexResult;
}

/* 0x811910A8 */
int LocationSusWatcher::CheckState()
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret == 0) {
		if (m_handleMgr != NULL && m_provider != NULL) {
			int activeCount = 0;
			locationHandleMgrGetActiveCount(m_handleMgr, &activeCount);
			if (m_suspended != 0 && activeCount > 0) {
				for (paf::vector<LocationSusListener *>::iterator it = m_listeners.begin();
				     it != m_listeners.end(); ++it) {
					(*it)->OnSuspend();
				}
				if (m_provider != NULL) {
					m_provider->Suspend();
					m_suspended = 0;
				}
			} else if (m_suspended == 0 && activeCount == 0) {
				if (m_provider != NULL) {
					m_provider->Resume();
					m_suspended = 1;
				}
				for (paf::vector<LocationSusListener *>::iterator it = m_listeners.begin();
				     it != m_listeners.end(); ++it) {
					(*it)->OnResume();
				}
			}
		}
		sceKernelUnlockLwMutex(&m_mutex, 1);
	}
	return ret;
}

/* 0x81191194 */
int LocationSusWatcher::OnNotify()
{
	CheckState();
	return 1;
}

struct LocationScanModeListener {
	virtual int OnModeChange(int prevMode, int newMode);
};

class LocationScanMode {
public:
	int Init();				/* 0x811911A0 */
	int Term();				/* 0x811911CC */
	int SetMode(int mode);			/* 0x811911E2 */

	int m_apScanMode;			/* +0 */
	paf::vector<LocationScanModeListener *> m_listeners; /* +4 */
	int m_mutexResult;			/* +20 */
	SceKernelLwMutexWork m_mutex;		/* +24 */
};

/* 0x811911A0 */
int LocationScanMode::Init()
{
	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceLocationScanMode", 2, 0, NULL);
	if (m_mutexResult < 0)
		return m_mutexResult;
	return 0;
}

/* 0x811911CC */
int LocationScanMode::Term()
{
	if (m_mutexResult == 0)
		return sceKernelDeleteLwMutex(&m_mutex);
	return 0;
}

/* 0x811911E2 */
int LocationScanMode::SetMode(int mode)
{
	int ret = sceKernelLockLwMutex(&m_mutex, 1, NULL);
	if (ret != 0)
		return ret;

	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 8, "m_apScanMode is %d, mode is %d\n",
				     m_apScanMode, mode);

	if (m_apScanMode == 0) {
		if (mode != 0) {
			for (int i = 0; i < (int)m_listeners.size(); i++) {
				ret = m_listeners[i]->OnModeChange(m_apScanMode, mode);
				if (ret != 0)
					break;
			}
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 8,
						     "_notifyModeChange() returned 0x%x\n", ret);
		}
		if (ret == 0)
			m_apScanMode = mode;
	} else if (m_apScanMode == 1) {
		if (mode != 1) {
			for (int i = 0; i < (int)m_listeners.size(); i++) {
				ret = m_listeners[i]->OnModeChange(m_apScanMode, mode);
				if (ret != 0)
					break;
			}
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 8,
						     "_notifyModeChange() returned 0x%x\n", ret);
		}
		if (ret == 0)
			m_apScanMode = mode;
	} else {
		ret = SCE_LOCATION_ERROR_FATAL_ERROR;
	}

	int uret = sceKernelUnlockLwMutex(&m_mutex, 1);
	if (uret < 0)
		return uret;
	return ret;
}
