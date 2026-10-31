/* paf::thread::Priority: the priorities paf creates its own threads with. */
#include <thread/thread.h>

namespace paf {
namespace thread {

Priority::Priority()
{
	high = SCE_KERNEL_DEFAULT_PRIORITY_USER + 15;
	normal = SCE_KERNEL_DEFAULT_PRIORITY_USER + 20;
	low = SCE_KERNEL_DEFAULT_PRIORITY_USER + 24;
}

static Priority s_priority;

void SetPriority(const Priority &priority)
{
	s_priority = priority;
}

Priority *GetPriority()
{
	return &s_priority;
}

}
}
