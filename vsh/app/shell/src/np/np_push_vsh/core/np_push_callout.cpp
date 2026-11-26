/*
 * np_push_callout.cpp: SceNpPush timer callout context wrapping SceNpCommon's event loop.
 * Sony path: vsh/app/common/src/np/np_push_vsh/core/np_push_callout.cpp
 * 3.65-CEX: 0x813AF124..0x813AF209
 */
#include "../np_push_internal.h"

namespace sce {
namespace Np {
namespace Push {

static NpPushEventLoopHost *s_calloutContext = NULL;

/* 0x813AF124: InitCallout */
int InitCallout(void)
{
	if (s_calloutContext != NULL)
		return SCE_NP_PUSH_ERROR_ALREADY_INITIALIZED;

	void *eventLoop = npCommonGetEventLoop();
	NP_PUSH_ASSERT(eventLoop != NULL, "InitCallout",
		       "core\\np_push_callout.cpp", 46, "eventLoop != NULL");

	void *rawMem = SceNpCommonPs4_0C922423(sizeof(NpPushEventLoopHost), GetAllocator());
	if (rawMem == NULL)
		return SCE_NP_PUSH_ERROR_OUT_OF_MEMORY;

	NpPushEventLoopHost *host = SceNpCommonPs4_326EF8D2(
		static_cast<NpPushEventLoopHost *>(rawMem));
	int ret = SceNpCommonPs4_70073BD1(host, eventLoop);
	if (ret < 0) {
		typedef void (*DestructorFn)(NpPushEventLoopHost *);
		DestructorFn deleteFn = reinterpret_cast<DestructorFn>(host->vtable[1]);
		deleteFn(host);
		return ret;
	}

	s_calloutContext = host;
	return 0;
}

/* 0x813AF1BC: TermCallout */
void TermCallout(void)
{
	if (s_calloutContext == NULL)
		return;

	typedef void (*DestructorFn)(NpPushEventLoopHost *);
	DestructorFn deleteFn = reinterpret_cast<DestructorFn>(s_calloutContext->vtable[1]);
	deleteFn(s_calloutContext);
	s_calloutContext = NULL;
}

/* 0x813AF1FC: GetCalloutContext */
NpPushEventLoopHost *GetCalloutContext(void)
{
	return s_calloutContext;
}

} /* namespace Push */
} /* namespace Np */
} /* namespace sce */
