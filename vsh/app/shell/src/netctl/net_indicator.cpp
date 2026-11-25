/*
 * net_indicator.cpp: network activity indicator poller on PAF's main thread
 * (0x8121CFE2 - 0x8121D18C).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <common/main_thread_call_list.h>
#include <timer/timer.h>
#include "../indicator_plugin/src/indicator_interface.h"

#define NET_INDICATOR_ERR_ALREADY_INIT	-2136014816	/* 0x80AF0020 */
#define NET_INDICATOR_ERR_NO_MEMORY	-2136014814	/* 0x80AF0022 */
#define NET_INDICATOR_HOLD_MS		1000u

struct SceNetPsSenderList {
	SceUID pids[16];
	int count;
};

extern "C" {
extern int SceNetInternal_A9F2277C(SceNetPsSenderList *list, int flags);
}

extern int netCtlApiIsAnyProcessAllowed(const SceUID *pids, SceUInt32 count);

class NetActivityIndicator {
public:
	NetActivityIndicator() : m_activeCount(0), m_holdMs(0)
	{
		paf::common::MainThreadCallList::Register(OnTick, this);
	}

	virtual ~NetActivityIndicator()
	{
		paf::common::MainThreadCallList::Unregister(OnTick, this);
	}

	void Poll();
	static void OnTick(void *userdata);

private:
	int m_activeCount;		/* +0x04 */
	SceUInt32 m_holdMs;		/* +0x08 */
};

static NetActivityIndicator *s_netActivityIndicator = NULL;	/* 0x81545148 */

/* 0x8121CFE2 */
int shellMainThreadPollerCreate(void)
{
	if (s_netActivityIndicator != NULL)
		return NET_INDICATOR_ERR_ALREADY_INIT;

	s_netActivityIndicator = new NetActivityIndicator();
	if (s_netActivityIndicator == NULL)
		return NET_INDICATOR_ERR_NO_MEMORY;

	return 0;
}

/* 0x8121D08E */
void NetActivityIndicator::Poll()
{
	SceNetPsSenderList senderList;

	if (SceNetInternal_A9F2277C(&senderList, 0) != 0)
		return;

	int isActive = 0;
	if (senderList.count > 0)
		isActive = netCtlApiIsAnyProcessAllowed(senderList.pids, (SceUInt32)senderList.count);

	if (isActive == 1) {
		if (m_holdMs == 0) {
			if (m_activeCount != 0) {
				m_activeCount++;
			} else {
				paf::Plugin *plugin = paf::Plugin::Find("indicator_plugin");
				if (plugin != NULL) {
					IndicatorPluginInterface *iface =
						(IndicatorPluginInterface *)plugin->GetInterface(1);
					if (iface != NULL && iface->SetValue4(1) == 0)
						m_activeCount++;
				}
			}
		}
		m_holdMs = NET_INDICATOR_HOLD_MS;
	} else if (m_holdMs != 0) {
		SceUInt32 deltaMs = (SceUInt32)(paf::Timer::s_tick_unit *
						paf::Timer::s_tick_interval * 1000.0f);
		if (m_holdMs <= deltaMs) {
			if (m_activeCount != 0) {
				if (m_activeCount != 1) {
					m_activeCount--;
				} else {
					paf::Plugin *plugin = paf::Plugin::Find("indicator_plugin");
					if (plugin != NULL) {
						IndicatorPluginInterface *iface =
							(IndicatorPluginInterface *)plugin->GetInterface(1);
						if (iface != NULL && iface->SetValue4(0) == 0)
							m_activeCount--;
					}
				}
			}
			m_holdMs = 0;
		} else {
			m_holdMs -= deltaMs;
		}
	}
}

/* 0x8121D182 */
void NetActivityIndicator::OnTick(void *userdata)
{
	NetActivityIndicator *self = (NetActivityIndicator *)userdata;
	self->Poll();
}
