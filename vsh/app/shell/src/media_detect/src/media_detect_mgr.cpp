/*
 * SceShell media_detect: SceShellMediaDetectMgr
 * (0x811C8CD4 - 0x811C99E4)
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/list>
#include <std/map>
#include <std/string.h>
#include <kernel.h>
#include <thread/thread.h>

extern "C" {
extern int vshIoUmount(int id, int force, int opt1, int opt2);
extern int vshIoCreateMountEvent(int id, int flags);
extern int vshIoDeleteMountEvent(int eventId);
extern int vshIoCreateErrorEvent(int id, int flags, void *outInfo);
extern int vshIoDeleteErrorEvent(int eventId);
extern int vshPowerRequestColdReset(void);
extern int sceKernelProcessPowerUnlock(int type);
extern int sceKernelPowerTick(int type);
extern int sceKernelRegisterCallbackToEvent(SceUID eventId, SceUID cbId);
extern int sceKernelUnregisterCallbackFromEvent(SceUID eventId, SceUID cbId);
extern int sceSblSsMgr_ReadNvs(int index, void *buf, SceSize size);	/* 0x81417CF8 */
extern int sceSblSsMgr_WriteNvs(int index, const void *buf, SceSize size); /* 0x81417D0C */
}

typedef void (*MediaDetectCallbackFn)(int eventType, void *arg, void *userData);

class MediaDetectListener {
public:
	MediaDetectListener(MediaDetectCallbackFn cb, void *userData)
		: m_cb(cb), m_userData(userData), m_removed(0) {}
	virtual ~MediaDetectListener() {}
	virtual void Invoke(int eventType, void *arg, int unused)
	{
		(void)unused;
		if (m_cb != NULL)
			m_cb(eventType, arg, m_userData);
	}

	MediaDetectCallbackFn m_cb;				/* +4 */
	void *m_userData;					/* +8 */
	SceUInt8 m_removed;					/* +12 */
};

class MediaDetectListenerList {
public:
	MediaDetectListenerList() : m_dispatching(0) {}
	virtual ~MediaDetectListenerList()
	{
		for (paf::list<MediaDetectListener *>::iterator it = m_list.begin();
		     it != m_list.end(); ++it) {
			delete *it;
		}
	}

	paf::list<MediaDetectListener *> m_list;		/* +4 */
	SceUInt8 m_dispatching;					/* +16 */
};

class MediaDetectMgr {
public:
	MediaDetectMgr(SceUInt8 checkMediaSwitch);		/* 0x811C8D7C */
	virtual ~MediaDetectMgr();				/* 0x811C90C8 / 0x811C9216 */

	static int Create(SceUInt8 checkMediaSwitch);		/* 0x811C9070 */
	static MediaDetectMgr *GetInstance();			/* 0x811C90BA */

	int DispatchEvent(int eventType, void *arg, MediaDetectCallbackFn directCb); /* 0x811C9226 */
	int RegisterCallback(int eventType, MediaDetectCallbackFn cb, void *userData); /* 0x811C9328 */
	int UnregisterCallback(int eventType, MediaDetectCallbackFn cb, void *userData); /* 0x811C9520 */
	int MaskEvent(int eventBit);				/* 0x811C9622 */
	int UnmaskEvent(int eventBit);				/* 0x811C964A */
	SceUInt8 IsInternalStorage();				/* 0x811C9674 */
	int IsGroMounted();					/* 0x811C967A */
	int HandleEvent(SceUID eventId, SceInt16 eventBits);	/* 0x811C9686 */

	static int EventCallback(int notifyId, int notifyCount, int notifyArg, void *userData); /* 0x811C99D6 */

private:
	paf::map<int, MediaDetectListenerList *> m_listeners;	/* +4 */
	paf::thread::Mutex m_mutex;				/* +16 */
	SceUID m_cbUxMount;					/* +48 */
	SceUID m_cbGroMount;					/* +52 */
	SceUID m_cbIoError;					/* +56 */
	SceUID m_evUxMount;					/* +60 */
	SceUID m_evGroMount;					/* +64 */
	SceUID m_evIoErr[5];					/* +68..+84 */
	SceUInt8 m_hasInternalStorage;				/* +88 */
	SceUInt8 m_useInternalUx0;				/* +89 */
	int m_uxState;						/* +92 */
	int m_xmcState;						/* +96 */
	int m_groState;						/* +100 */
	SceUInt32 m_ioErrorInfo[7];				/* +104..+128 */
	SceUInt32 m_eventMask;					/* +132 */
};

static MediaDetectMgr *s_mediaDetectMgr = NULL;			/* 0x81544E14 */

/* 0x811C8CD4 */
extern "C" int SceShell_548A3111(int eventType, MediaDetectCallbackFn cb, void *userData)
{
	MediaDetectMgr *mgr = MediaDetectMgr::GetInstance();
	if (mgr != NULL)
		return mgr->RegisterCallback(eventType, cb, userData);
	return (int)0x80AF0001;
}

/* 0x811C8CFE */
extern "C" int SceShell_F197032C(int eventType, MediaDetectCallbackFn cb, void *userData)
{
	MediaDetectMgr *mgr = MediaDetectMgr::GetInstance();
	if (mgr != NULL)
		return mgr->UnregisterCallback(eventType, cb, userData);
	return (int)0x80AF0001;
}

/* 0x811C8D28 */
int mediaDetectMaskEvent(int eventBit)
{
	MediaDetectMgr *mgr = MediaDetectMgr::GetInstance();
	if (mgr != NULL)
		return mgr->MaskEvent(eventBit);
	return (int)0x80AF0001;
}

/* 0x811C8D46 */
int mediaDetectUnmaskEvent(int eventBit)
{
	MediaDetectMgr *mgr = MediaDetectMgr::GetInstance();
	if (mgr != NULL)
		return mgr->UnmaskEvent(eventBit);
	return (int)0x80AF0001;
}

/* 0x811C8D64 */
extern "C" int SceShell_6A6123E3(void)
{
	MediaDetectMgr *mgr = MediaDetectMgr::GetInstance();
	if (mgr != NULL)
		return mgr->IsGroMounted();
	return 0;
}

/* 0x811C8D7C */
MediaDetectMgr::MediaDetectMgr(SceUInt8 checkMediaSwitch)
	: m_mutex("SceShellMediaDetectMgr", 0),
	  m_cbUxMount(-1), m_cbGroMount(-1), m_cbIoError(-1),
	  m_evUxMount(-1), m_evGroMount(-1),
	  m_hasInternalStorage(0), m_useInternalUx0(0),
	  m_uxState(-1), m_xmcState(-1), m_groState(-1),
	  m_eventMask(0)
{
	for (int i = 0; i < 5; ++i)
		m_evIoErr[i] = -1;
	for (int i = 0; i < 7; ++i)
		m_ioErrorInfo[i] = 0;

	char devName[20];
	sce_paf_strcpy(devName, "xmc-lp-act-entire");
	if (sceIoDevctl("sdstor0:", 3, devName, 18, NULL, 0) == 0) {
		SceUID fd = sceIoOpen("sdstor0:int-lp-ign-userext", 1, 0);
		if (fd >= 0) {
			sceIoClose(fd);
			m_hasInternalStorage = 1;
			SceUInt8 nvsFlag[8];
			if (sceSblSsMgr_ReadNvs(5, nvsFlag, 1) == 0) {
				m_useInternalUx0 = ((nvsFlag[0] & 1) == 0);
				if (checkMediaSwitch != 0) {
					SceUID xmcFd = sceIoOpen("sdstor0:xmc-lp-ign-entire", 1, 0);
					if (xmcFd >= 0)
						sceIoClose(xmcFd);
					if ((m_useInternalUx0 != 0 && xmcFd >= 0) ||
					    (m_useInternalUx0 == 0 && xmcFd < 0)) {
						if (m_useInternalUx0 != 0) {
							vshIoUmount(0x800, 1, 0, 0);
							nvsFlag[0] |= 1;
						} else {
							nvsFlag[0] &= (SceUInt8)~1;
						}
						if (sceSblSsMgr_WriteNvs(5, nvsFlag, 1) == 0) {
							vshPowerRequestColdReset();
							sceKernelProcessPowerUnlock(0);
							while (1)
								paf::thread::Sleep(100);
						}
					}
				}
			}
		}
	}

	SceIoStat statBuf;
	m_cbUxMount = sceKernelCreateCallback("SceShellMediaDetectMgrUxMount", 0, EventCallback, this);
	if (m_cbUxMount >= 0) {
		int mountId = (m_useInternalUx0 != 0) ? 0xE00 : 0x800;
		m_evUxMount = vshIoCreateMountEvent(mountId, 0x300);
		if (m_evUxMount >= 0 &&
		    sceKernelRegisterCallbackToEvent(m_evUxMount, m_cbUxMount) == 0 &&
		    sceIoGetstat("ux0:", &statBuf) == 0) {
			m_uxState = 1;
			if (m_useInternalUx0 == 0)
				m_xmcState = 5;
		}
	}

	m_cbGroMount = sceKernelCreateCallback("SceShellMediaDetectMgrGroMount", 0, EventCallback, this);
	if (m_cbGroMount >= 0) {
		m_evGroMount = vshIoCreateMountEvent(0x900, 0x301);
		if (m_evGroMount >= 0 &&
		    sceKernelRegisterCallbackToEvent(m_evGroMount, m_cbGroMount) == 0 &&
		    sceIoGetstat("gro0:", &statBuf) == 0) {
			m_groState = 9;
		}
	}

	m_cbIoError = sceKernelCreateCallback("SceShellMediaDetectMgrIoError", 0, EventCallback, this);
	if (m_cbIoError >= 0) {
		static const int kErrorDevices[5] = { 0x200, 0x300, 0x400, 0x600, 0x800 };
		for (int i = 0; i < 5; ++i) {
			m_evIoErr[i] = vshIoCreateErrorEvent(kErrorDevices[i], 1, m_ioErrorInfo);
			if (m_evIoErr[i] >= 0)
				sceKernelRegisterCallbackToEvent(m_evIoErr[i], m_cbIoError);
		}
	}
}

/* 0x811C9070 */
int MediaDetectMgr::Create(SceUInt8 checkMediaSwitch)
{
	if (s_mediaDetectMgr != NULL)
		return (int)0x80AF0020;
	s_mediaDetectMgr = new MediaDetectMgr(checkMediaSwitch);
	if (s_mediaDetectMgr != NULL)
		return 0;
	return (int)0x80AF0022;
}

/* 0x811C90BA */
MediaDetectMgr *MediaDetectMgr::GetInstance()
{
	return s_mediaDetectMgr;
}

/* 0x811C90C8 / 0x811C9216 */
MediaDetectMgr::~MediaDetectMgr()
{
	if (m_cbIoError >= 0) {
		for (int i = 4; i >= 0; --i) {
			if (m_evIoErr[i] >= 0) {
				sceKernelUnregisterCallbackFromEvent(m_evIoErr[i], m_cbIoError);
				vshIoDeleteErrorEvent(m_evIoErr[i]);
			}
		}
		sceKernelDeleteCallback(m_cbIoError);
	}
	if (m_cbGroMount >= 0) {
		if (m_evGroMount >= 0) {
			sceKernelUnregisterCallbackFromEvent(m_evGroMount, m_cbGroMount);
			vshIoDeleteMountEvent(m_evGroMount);
		}
		sceKernelDeleteCallback(m_cbGroMount);
	}
	if (m_cbUxMount >= 0) {
		if (m_evUxMount >= 0) {
			sceKernelUnregisterCallbackFromEvent(m_evUxMount, m_cbUxMount);
			vshIoDeleteMountEvent(m_evUxMount);
		}
		sceKernelDeleteCallback(m_cbUxMount);
	}
	while (m_listeners.size() != 0) {
		paf::map<int, MediaDetectListenerList *>::iterator it = m_listeners.begin();
		delete it->second;
		m_listeners.erase(it);
	}
}

/* 0x811C9226 */
int MediaDetectMgr::DispatchEvent(int eventType, void *arg, MediaDetectCallbackFn directCb)
{
	if ((m_eventMask & (1u << eventType)) != 0)
		return 0;
	if (directCb != NULL) {
		directCb(eventType, arg, NULL);
		return 0;
	}
	paf::map<int, MediaDetectListenerList *>::iterator mit = m_listeners.find(eventType);
	if (mit != m_listeners.end()) {
		MediaDetectListenerList *group = mit->second;
		group->m_dispatching = 1;
		for (paf::list<MediaDetectListener *>::iterator it = group->m_list.begin();
		     it != group->m_list.end(); ++it) {
			if (*it != NULL)
				(*it)->Invoke(mit->first, arg, 0);
		}
		group->m_dispatching = 0;
		for (paf::list<MediaDetectListener *>::iterator it = group->m_list.begin();
		     it != group->m_list.end(); ) {
			MediaDetectListener *listener = *it;
			if (listener != NULL && listener->m_removed == 0) {
				++it;
			} else {
				delete listener;
				it = group->m_list.erase(it);
			}
		}
	}
	return 0;
}

/* 0x811C9328 */
int MediaDetectMgr::RegisterCallback(int eventType, MediaDetectCallbackFn cb, void *userData)
{
	m_mutex.Lock();
	MediaDetectListener *listener = new MediaDetectListener(cb, userData);
	paf::map<int, MediaDetectListenerList *>::iterator mit = m_listeners.find(eventType);
	if (mit == m_listeners.end()) {
		MediaDetectListenerList *group = new MediaDetectListenerList();
		group->m_list.push_back(listener);
		m_listeners.insert(paf::pair<int, MediaDetectListenerList *>(eventType, group));
	} else {
		MediaDetectListenerList *group = mit->second;
		if (group != NULL) {
			bool found = false;
			for (paf::list<MediaDetectListener *>::iterator it = group->m_list.begin();
			     it != group->m_list.end(); ++it) {
				if (*it == listener) {
					found = true;
					break;
				}
			}
			if (!found)
				group->m_list.push_back(listener);
		}
	}

	if (cb != NULL) {
		if (eventType == 9 && m_groState == 9)
			DispatchEvent(9, NULL, cb);
		else if (eventType == 4 && m_xmcState == 4)
			DispatchEvent(4, NULL, cb);
		else if (eventType == 0 && m_uxState == 0)
			DispatchEvent(0, NULL, cb);
	}
	m_mutex.Unlock();
	return 0;
}

/* 0x811C9520 */
int MediaDetectMgr::UnregisterCallback(int eventType, MediaDetectCallbackFn cb, void *userData)
{
	m_mutex.Lock();
	int ret = 0;
	paf::map<int, MediaDetectListenerList *>::iterator mit = m_listeners.find(eventType);
	if (mit == m_listeners.end() || mit->second == NULL) {
		ret = -1;
	} else {
		MediaDetectListenerList *group = mit->second;
		MediaDetectListener *target = NULL;
		for (paf::list<MediaDetectListener *>::iterator it = group->m_list.begin();
		     it != group->m_list.end(); ++it) {
			if ((*it)->m_cb == cb && (*it)->m_userData == userData) {
				target = *it;
				break;
			}
		}
		bool erased = false;
		for (paf::list<MediaDetectListener *>::iterator it = group->m_list.begin();
		     it != group->m_list.end(); ++it) {
			if (*it == target) {
				if (group->m_dispatching != 0) {
					target->m_removed = 1;
				} else {
					group->m_list.erase(it);
					delete target;
				}
				erased = true;
				break;
			}
		}
		if (!erased)
			ret = -1;
	}
	m_mutex.Unlock();
	return ret;
}

/* 0x811C9622 */
int MediaDetectMgr::MaskEvent(int eventBit)
{
	m_mutex.Lock();
	m_eventMask |= (1u << eventBit);
	m_mutex.Unlock();
	return 0;
}

/* 0x811C964A */
int MediaDetectMgr::UnmaskEvent(int eventBit)
{
	m_mutex.Lock();
	m_eventMask &= ~(1u << eventBit);
	m_mutex.Unlock();
	return 0;
}

/* 0x811C9672 */
void mediaDetectNoop(void)
{
}

/* 0x811C9674 */
SceUInt8 MediaDetectMgr::IsInternalStorage()
{
	return m_useInternalUx0;
}

/* 0x811C967A */
int MediaDetectMgr::IsGroMounted()
{
	return (m_groState == 9);
}

/* 0x811C9686 */
int MediaDetectMgr::HandleEvent(SceUID eventId, SceInt16 eventBits)
{
	m_mutex.Lock();
	if (eventId == m_evUxMount) {
		int isInsert = eventBits & 0x100;
		int isRemove = eventBits & 0x200;
		if (m_useInternalUx0 == 0) {
			if (isInsert != 0) {
				sceKernelPowerTick(0);
				if (m_uxState == 1) {
					DispatchEvent(2, NULL, NULL);
					DispatchEvent(3, NULL, NULL);
				} else if (m_uxState == 0 || m_uxState == 2) {
					DispatchEvent(3, NULL, NULL);
				}
				DispatchEvent(0, NULL, NULL);
				m_uxState = 0;
			}
			if (isRemove != 0) {
				sceKernelPowerTick(0);
				if (m_uxState == 3)
					DispatchEvent(0, NULL, NULL);
				else if (m_uxState == 1)
					DispatchEvent(2, NULL, NULL);
				DispatchEvent(3, NULL, NULL);
				m_uxState = 3;
			}
		}
		if (isInsert != 0) {
			sceKernelPowerTick(0);
			if (m_xmcState == 5) {
				DispatchEvent(6, NULL, NULL);
				DispatchEvent(7, NULL, NULL);
			} else if (m_xmcState == 4 || m_xmcState == 6) {
				DispatchEvent(7, NULL, NULL);
			}
			DispatchEvent(4, NULL, NULL);
			m_xmcState = 4;
		}
		if (isRemove != 0) {
			sceKernelPowerTick(0);
			if (m_xmcState == 7)
				DispatchEvent(4, NULL, NULL);
			else if (m_xmcState == 5)
				DispatchEvent(6, NULL, NULL);
			DispatchEvent(7, NULL, NULL);
			m_xmcState = 7;
		}
	} else if (eventId == m_evGroMount) {
		if ((eventBits & 0x100) != 0) {
			sceKernelPowerTick(0);
			if (m_groState == 9) {
				DispatchEvent(10, NULL, NULL);
				DispatchEvent(11, NULL, NULL);
			} else if (m_groState == 8 || m_groState == 10) {
				DispatchEvent(11, NULL, NULL);
			}
			DispatchEvent(8, NULL, NULL);
			m_groState = 8;
		}
		if ((eventBits & 1) != 0) {
			if (m_groState == 9) {
				DispatchEvent(10, NULL, NULL);
				DispatchEvent(11, NULL, NULL);
				DispatchEvent(8, NULL, NULL);
			} else if (m_groState == 10) {
				DispatchEvent(11, NULL, NULL);
				DispatchEvent(8, NULL, NULL);
			} else if (m_groState != 8) {
				DispatchEvent(8, NULL, NULL);
			}
			DispatchEvent(9, NULL, NULL);
			m_groState = 9;
		}
		if ((eventBits & 0x200) != 0) {
			sceKernelPowerTick(0);
			if (m_groState == 11)
				DispatchEvent(8, NULL, NULL);
			else if (m_groState == 9)
				DispatchEvent(10, NULL, NULL);
			DispatchEvent(11, NULL, NULL);
			m_groState = 11;
		}
	} else {
		for (int i = 0; i < 5; ++i) {
			if (eventId == m_evIoErr[i]) {
				int errCode = (int)m_ioErrorInfo[1];
				DispatchEvent(12 + i, &errCode, NULL);
				break;
			}
		}
	}
	m_mutex.Unlock();
	return 0;
}

/* 0x811C99D6 */
int MediaDetectMgr::EventCallback(int notifyId, int notifyCount, int notifyArg, void *userData)
{
	(void)notifyCount;
	return ((MediaDetectMgr *)userData)->HandleEvent(notifyId, (SceInt16)notifyArg);
}
