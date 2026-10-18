/*
 * SDK include compatibility: SceThreadmgr's interface is threadmgr/threadmgr.h (and the headers
 * of its parts). A vitasdk header that includes this one, or one of vitasdk's
 * psp2kern/kernel/threadmgr/ headers, gets it.
 */
#pragma once

#include <threadmgr/threadmgr.h>
#include <threadmgr/mutex.h>
#include <threadmgr/lw_mutex.h>
#include <threadmgr/threadmgr_callback.h>
#include <threadmgr/threadmgr_event.h>
#include <threadmgr/threadmgr_timer.h>
#ifndef __cplusplus	/* C only, like their static asserts */
#include <threadmgr/fast_mutex_object.h>
#include <threadmgr/threadmgr_debugger.h>
#endif
