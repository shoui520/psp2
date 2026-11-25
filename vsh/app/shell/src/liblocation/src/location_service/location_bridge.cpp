/*
 * location_bridge.cpp: provider interval controller for liblocation
 * (0x8118E748-0x8118E8AC, sti 0x8118E83A). The file and class names are ours (PLACEHOLDER).
 */
#include "location_service.h"

extern void locationSettingGetMethod(void *setting, SceUInt32 *lmethod, SceUInt32 *hmethod);
extern SceUInt32 locationDecodeInterval(const SceUInt32 *lmethod);
extern int locationLoggerGetInterval(void);

struct LocationProviderBase {
	virtual void vfn0();
	virtual void vfn1();
	virtual void vfn2();
	virtual void vfn3();
	virtual void vfn4();
	virtual void vfn5();
	virtual void vfn6();
	virtual void vfn7();
	virtual int SetInterval(int interval);
};

/* 0x8118E748-0x8118E83A */
class LocationIntervalCtrl {
public:
	LocationIntervalCtrl(void *posMgr, void *arg2, void *handleMgr);
	virtual ~LocationIntervalCtrl();
	virtual void OnUnused();
	virtual int SetProvider(LocationProviderBase *provider);
	virtual int UpdateInterval(int flag);

	void *m_posMgr;			/* +4 */
	void *m_handleMgr;		/* +8 */
	LocationProviderBase *m_provider; /* +12 */
	void *m_arg3;			/* +16 */
	int m_interval;			/* +20 */
	int m_flags;			/* +24 */
};

/* 0x8118E748 */
LocationIntervalCtrl::LocationIntervalCtrl(void *posMgr, void *arg2, void *handleMgr)
	: m_posMgr(posMgr)
	, m_handleMgr(handleMgr)
	, m_provider(NULL)
	, m_arg3(arg2)
	, m_interval(0)
	, m_flags(0)
{
}

/* 0x8118E768 / 0x8118E776 */
LocationIntervalCtrl::~LocationIntervalCtrl()
{
}

/* 0x8118E78A */
void LocationIntervalCtrl::OnUnused()
{
}

/* 0x8118E78C */
int LocationIntervalCtrl::SetProvider(LocationProviderBase *provider)
{
	m_provider = provider;
	if (provider != NULL)
		return provider->SetInterval(m_interval);
	return 0;
}

/* 0x8118E7A2 */
int LocationIntervalCtrl::UpdateInterval(int flag)
{
	SceUInt32 lmethod = 0;
	SceUInt32 hmethod = 0;

	void *setting = *(void **)((char *)m_posMgr + 24);
	locationSettingGetMethod(setting, &lmethod, &hmethod);

	if (flag != 0) {
		m_flags = 16;
		return 0;
	}

	int interval = 5000;
	if (*(int *)((char *)m_handleMgr + 2352) <= 0) {
		SceUInt32 req = locationDecodeInterval(&lmethod);
		void *logger = *(void **)((char *)m_posMgr + 32);
		int logInterval;
		if (logger != NULL && (logInterval = locationLoggerGetInterval()) != 0) {
			interval = 1000 * logInterval;
		} else {
			if (req < 5000)
				req = 5000;
			interval = (int)req;
		}
	}

	if (interval != m_interval) {
		m_interval = interval;
		if (m_provider != NULL)
			return m_provider->SetInterval(interval);
	}
	return 0;
}
