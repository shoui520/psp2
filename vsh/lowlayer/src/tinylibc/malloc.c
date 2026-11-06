/*
 * SceTinyLibc's allocator: free into the runtime's mspace. trigger_util's malloc, which makes
 * the mspace, is not written yet, so the mspace is external.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "tinylibc.h"
#include "tinylibc_internal.h"

__attribute__((nocommon)) void *g_tinyLibcMspace;

void free(void *ptr)
{
	sceClibMspaceFree(g_tinyLibcMspace, ptr);
}
