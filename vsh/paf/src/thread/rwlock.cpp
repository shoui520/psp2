/*
 * paf::thread::RWLock on one event flag. Bit 2 is a mutex over the reader count
 * and the writer; bit 1 is set while no reader holds the lock, so a writer that
 * takes both bits excludes everyone, and keeps bit 2 until it unlocks.
 */
#include <thread/thread.h>

namespace paf {
namespace thread {

enum {
	RWLOCK_NO_READERS = 1,
	RWLOCK_GUARD = 2
};

RWLock::RWLock(const char *name, int option)
{
	m_readers = 0;
	m_evf = sceKernelCreateEventFlag(name, SCE_KERNEL_EVF_ATTR_MULTI,
					 RWLOCK_NO_READERS | RWLOCK_GUARD, NULL);
}

RWLock::~RWLock()
{
	sceKernelDeleteEventFlag(m_evf);
}

void RWLock::ReaderLock()
{
	sceKernelWaitEventFlag(m_evf, RWLOCK_GUARD, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
	if (++m_readers == 1)
		sceKernelClearEventFlag(m_evf, ~RWLOCK_NO_READERS);
	sceKernelSetEventFlag(m_evf, RWLOCK_GUARD);
}

void RWLock::ReaderUnlock()
{
	sceKernelWaitEventFlag(m_evf, RWLOCK_GUARD, SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
	if (--m_readers == 0)
		sceKernelSetEventFlag(m_evf, RWLOCK_NO_READERS);
	sceKernelSetEventFlag(m_evf, RWLOCK_GUARD);
}

void RWLock::WriterLock()
{
	sceKernelWaitEventFlag(m_evf, RWLOCK_NO_READERS | RWLOCK_GUARD,
			       SCE_KERNEL_EVF_WAITMODE_CLEAR_PAT, NULL, NULL);
	sceKernelSetEventFlag(m_evf, RWLOCK_NO_READERS);
}

void RWLock::WriterUnlock()
{
	sceKernelSetEventFlag(m_evf, RWLOCK_GUARD);
}

}
}
