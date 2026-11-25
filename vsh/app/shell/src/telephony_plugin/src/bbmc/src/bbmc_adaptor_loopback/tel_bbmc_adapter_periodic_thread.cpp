/*
 * tel_bbmc_adapter_periodic_thread.cpp: 100ms periodic tick thread for the
 * BBMC loopback simulator (3.65-CEX: 0x813D965C..0x813D982B).
 */
#include "../../tel_bbmc_adapter.h"

namespace telephony {

static const int kLoopbackPeriodicThreadPriority = 0x1000010E;
static const int kLoopbackTickSleepMs = 100;
static const int kLoopbackPeriodicDivider = 10;
static const SceUInt32 kLoopbackAutoSmsTicks = 100U;

TelBbmcAdapterPeriodicThread::TelBbmcAdapterPeriodicThread(const char *name, SceSize stackSize)
	: TelThread(name, kLoopbackPeriodicThreadPriority, stackSize),
	  m_core(NULL),
	  m_autoRecvSms(false)
{
}

TelBbmcAdapterPeriodicThread::~TelBbmcAdapterPeriodicThread()
{
}

void TelBbmcAdapterPeriodicThread::StartPeriodic(TelMessageThread *targetThread,
						 TelBbmcAdapterLoopbackCore *core,
						 bool autoRecvSms)
{
	m_core = core;
	m_autoRecvSms = autoRecvSms;
	StartThread(targetThread);
}

int TelBbmcAdapterPeriodicThread::Run(void *userArg)
{
	TelMessageThread *targetThread = static_cast<TelMessageThread *>(userArg);
	int periodicSeq = 0;
	int tickDivider = 0;
	SceUInt32 autoSmsCounter = 0;
	bool autoRecv = m_autoRecvSms;

	while (!IsThreadCanceled()) {
		m_core->Step();

		TelBbmcNotifyItem item;
		while (m_core->PopEvent(&item).IsSuccess()) {
			TelEventInfo event(item.eventId, item.eventArg, -1);
			targetThread->PostMessage(event);
		}

		if (tickDivider >= kLoopbackPeriodicDivider) {
			TelEventInfo tickEvent(TEL_BBMC_EVENT_LOOPBACK_PERIODIC, periodicSeq, -1);
			targetThread->PostMessage(tickEvent);
			tickDivider = 0;
			++periodicSeq;
		} else {
			++tickDivider;
		}

		if (autoRecv) {
			if (autoSmsCounter > kLoopbackAutoSmsTicks) {
				TelEventInfo smsEvent(TEL_BBMC_EVENT_LOOPBACK_AUTO_SMS, -1, -1);
				targetThread->PostMessage(smsEvent);
				autoSmsCounter = 0;
			}
			++autoSmsCounter;
		}

		TelThread::SleepMs(kLoopbackTickSleepMs);
	}

	return 0;
}

} /* namespace telephony */
