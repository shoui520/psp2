/*
 * paf::thread::SyncCall: a caller posts a function and blocks until the thread
 * that polls Check() has run it. Bit 4 of the event flag admits one caller at a
 * time, bit 1 posts the call and bit 2 reports it done.
 */
#include <thread/thread.h>

namespace paf {
namespace thread {

enum {
	SYNCCALL_POSTED = 1,
	SYNCCALL_DONE = 2,
	SYNCCALL_IDLE = 4
};

SyncCall::SyncCall(const char *name)
{
	m_func = NULL;
	m_arg = NULL;
	m_result = 0;
	m_evf = sceKernelCreateEventFlag(name, SCE_KERNEL_EVF_ATTR_MULTI, SYNCCALL_IDLE, NULL);
}

SyncCall::~SyncCall()
{
	sceKernelDeleteEventFlag(m_evf);
}

void SyncCall::Check()
{
	if (sceKernelPollEventFlag(m_evf, SYNCCALL_POSTED, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL) >= 0) {
		m_result = m_func(m_arg);
		m_arg = NULL;
		m_func = NULL;
		sceKernelSetEventFlag(m_evf, SYNCCALL_DONE);
	}
}

/* On the main thread itself the function runs at once. */
int SyncCall::Call(int (*func)(void *), void *arg)
{
	if (ThreadIDCache::Check(ThreadIDCache::Type_Main))
		return func(arg);

	sceKernelWaitEventFlag(m_evf, SYNCCALL_IDLE, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
	m_func = func;
	m_arg = arg;
	m_result = 0;
	sceKernelSetEventFlag(m_evf, SYNCCALL_POSTED);
	sceKernelWaitEventFlag(m_evf, SYNCCALL_DONE, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);

	int result = m_result;

	sceKernelSetEventFlag(m_evf, SYNCCALL_IDLE);
	return result;
}

SyncCall SyncCall::main_thread_synccall("ScePafMainThreadSyncCall");

}
}
