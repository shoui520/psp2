/* paf::thread::Sleep: delays the calling thread. */
#include <thread/thread.h>

namespace paf {
namespace thread {

void Sleep(int milliseconds)
{
	sceKernelDelayThread(milliseconds * 1000);
}

}
}
