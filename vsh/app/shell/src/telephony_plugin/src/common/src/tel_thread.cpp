/*
 * tel_thread.cpp: TelThread wrapper around paf::thread::Thread
 * (3.65-CEX: 0x813E4470..0x813E4575).
 */
#include <stddef.h>
#include <scetypes.h>
#include "../tel_common.h"

namespace telephony {

/* 0x813E4470 */
TelThread::TelThread(const char *name, int priority, SceSize stackSize)
	: paf::thread::Thread(priority, stackSize, name, NULL),
	  m_threadName(name), m_userArg(NULL), m_exitResult(0)
{
}

/* 0x813E44A0 */
TelThread::~TelThread()
{
	m_exitResult = 0;
}

/* 0x813E4562 */
void TelThread::EntryFunction()
{
	m_exitResult = Run(m_userArg);
}

/* 0x813E44E4 */
TelephonyResult TelThread::StartThread(void *userArg)
{
	if (IsAlive())
		return TelephonyResult(TEL_ERR_ALREADY_INIT);

	m_userArg = userArg;
	m_exitResult = 0;
	if (Start() != 0)
		return TelephonyResult(TEL_ERR_THREAD_START);
	return TelephonyResult(TEL_OK);
}

/* 0x813E452A */
TelephonyResult TelThread::StopThread()
{
	Cancel();
	JoinThread();
	return TelephonyResult(TEL_OK);
}

/* 0x813E4542 */
void TelThread::CancelThread()
{
	Cancel();
}

/* 0x813E454A */
void TelThread::JoinThread()
{
	Join();
}

/* 0x813E4552 */
void TelThread::SleepMs(SceUInt32 msec)
{
	paf::thread::Sleep(msec);
}

/* 0x813E455A */
bool TelThread::IsThreadCanceled()
{
	return IsCanceled();
}

} /* namespace telephony */
