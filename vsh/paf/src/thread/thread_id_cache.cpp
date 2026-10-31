/* paf::thread::ThreadIDCache: each of paf's threads records its own ID here. */
#include <thread/thread.h>

namespace paf {
namespace thread {

static SceUID s_thread_id[1];

void ThreadIDCache::SetCurrentType(Type type)
{
	s_thread_id[type] = sceKernelGetThreadId();
}

SceUID ThreadIDCache::Get(Type type)
{
	return s_thread_id[type];
}

bool ThreadIDCache::Check(Type type)
{
	return s_thread_id[type] == sceKernelGetThreadId();
}

}
}
