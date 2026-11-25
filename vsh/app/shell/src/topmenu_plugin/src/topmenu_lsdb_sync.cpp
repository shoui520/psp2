/*
 * topmenu_lsdb_sync.cpp: livespace_db client lifetime and main-thread hook
 * driven by topmenu_plugin (0x813C9AC0-0x813C9B30).
 */
#include "topmenu_internal.h"

namespace {

struct TopmenuLsdbSyncContext {
	char sentinel;
};

static TopmenuLsdbSyncContext *s_lsdbSyncContext;

static void topmenuLsdbSyncTick(void *userData)
{
	(void)userData;
}

} /* namespace */

/* 0x813C9AC0 */
int topmenuLsdbSyncInit(void)
{
	s_lsdbSyncContext = new TopmenuLsdbSyncContext();
	return shellLsdbClientCreate();
}

/* 0x813C9ADE */
void topmenuLsdbSyncTerm(void)
{
	shellLsdbClientDestroy();
	delete s_lsdbSyncContext;
	s_lsdbSyncContext = NULL;
}

/* 0x813C9AF6 */
void topmenuLsdbSyncStart(void)
{
	paf::common::MainThreadCallList::Register(topmenuLsdbSyncTick, s_lsdbSyncContext);
}

/* 0x813C9B14 */
void topmenuLsdbSyncStop(void)
{
	paf::common::MainThreadCallList::Unregister(topmenuLsdbSyncTick, s_lsdbSyncContext);
}
