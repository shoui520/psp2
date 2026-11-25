/*
 * location_bridge.cpp: provider interval controller for liblocation
 * (0x8118E748-0x8118E8AC, sti 0x8118E83A).
 */
#include "location_internal.h"

/* 0x8118E748 */
LocationIntervalCtrl::LocationIntervalCtrl(PositionManager *posMgr, HeadingManager *headingMgr,
					   ClientManager *handleMgr)
	: m_posMgr(posMgr)
	, m_handleMgr(handleMgr)
	, m_provider(NULL)
	, m_headingMgr(headingMgr)
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
int LocationIntervalCtrl::SetProvider(LocationProvider *provider)
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

	m_posMgr->m_config->GetMethod(&lmethod, &hmethod);

	if (flag != 0) {
		m_flags = 16;
		return 0;
	}

	int interval = 5000;

	if (m_handleMgr->m_activeGetCount <= 0) {
		SceUInt32 req = locationDecodeInterval(&lmethod);
		LocationLogger *logger = m_posMgr->m_logger;
		int logInterval;

		if (logger != NULL && (logInterval = logger->GetIntervalOverrideSec()) != 0) {
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
