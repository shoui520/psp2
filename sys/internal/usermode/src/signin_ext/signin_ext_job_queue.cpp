/*
 * SceSigninExt (vs0:sys/external/signin_ext.suprx):
 * SceSigninExtJobQueue — private heap allocator, surface pool, and
 * background JobQueue shared across Commerce2, RegMgr, and HTTP jobs.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <graphics/surface/surface_pool.h>
#include <job/job.h>
#include <memory/heap_allocator.h>
#include <plugin.h>
#include "signin_ext_internal.h"

extern "C" paf::memory::HeapAllocator *GetGlobalHeapAllocator(void);

using namespace paf;
using namespace paf::common;
using namespace paf::graph;
using namespace paf::job;
using namespace paf::memory;

static HeapAllocator *s_heapAllocator = NULL;
static SurfacePool *s_surfacePool = NULL;
static JobQueue *s_jobQueue = NULL;

void SigninExtJobQueueInit(void)
{
	Plugin *plugin = SigninExtGetPlugin();

	s_heapAllocator = plugin->m_private_heap;
	if (s_heapAllocator == NULL)
		s_heapAllocator = GetGlobalHeapAllocator();
	s_surfacePool = plugin->m_surface_pool;
	s_jobQueue = new JobQueue("SceSigninExtJobQueue", NULL);
	SigninExtHttpInit();
}

void SigninExtJobQueueTerm(void)
{
	SigninExtHttpTerm();
	if (s_jobQueue != NULL) {
		if (s_jobQueue->NumItems() != 0) {
			s_jobQueue->CancelAllItems();
			s_jobQueue->WaitEmpty();
		}
		delete s_jobQueue;
		s_jobQueue = NULL;
	}
	s_surfacePool = NULL;
	s_heapAllocator = NULL;
}

void *SigninExtAlloc(SceSize size)
{
	return s_heapAllocator->sce_paf_malloc(size);
}

void SigninExtFree(void *ptr)
{
	if (ptr != NULL)
		s_heapAllocator->sce_paf_free(ptr);
}

SharedPtr<Buffer> SigninExtAllocBuffer(SceSize size)
{
	return HeapAllocBuffer::Allocate(size, s_heapAllocator);
}

SurfacePool *SigninExtGetSurfacePool(void)
{
	return s_surfacePool;
}

JobQueue *SigninExtGetJobQueue(void)
{
	return s_jobQueue;
}
