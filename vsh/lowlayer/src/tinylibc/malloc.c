/*
 * SceTinyLibc's free, into the runtime's mspace (malloc is alloc.c's). No module that links the
 * runtime creates the mspace (their sceClibMspaceCreate imports are unused), so it is external.
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
