/*
 * Saving and restoring the interrupted CPU state around an exception.
 *
 * The entry stubs in excpmgr_initial_*.S store r0-r12, the banked user SP/LR,
 * the return address and SPSR into a SceExcpmgrContext on the exception-mode
 * stack and call their C entry policies. Context return and debug-register
 * state are C, shared with the secure-world KBL build in
 * excpmgr_context.h. Sony wrote all of this as ARM assembly, so none of
 * these functions has a debug-record name: the names are descriptive.
 */
#include "excpmgr_context_private.h"

/* The banked return boundary rearms trace after selecting the Abort stack. */
#define excpRearmTrace() ((void)0)
#include "excpmgr_context_return.h"
#undef excpRearmTrace
#include "excpmgr_context_debug.h"
#include "excpmgr_context_copy.h"
