/* SceTinyLibc's malloc, from the runtime's mspace (malloc.c). The file's name is ours. */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include "tinylibc.h"
#include "tinylibc_internal.h"

void *malloc(size_t size)
{
	return sceClibMspaceMalloc(g_tinyLibcMspace, size);
}
