/*
 * The allocator the XML parser takes its memory from. Initializing installs none, so the
 * parser keeps its own; uninitializing takes back one that was installed.
 */
#include <stddef.h>
#include <scetypes.h>
#include <bXCe/bxce_internal.h>
#include "mtp_internal.h"

static bXCe::MemAllocator *s_xmlAllocator = NULL;

extern "C" int mtpXmlAllocatorInitialize(void)
{
	return 0;
}

extern "C" int mtpXmlAllocatorUninitialize(void)
{
	if (s_xmlAllocator == NULL)
		return 0;
	ScebXCe_EB4E6C8D(NULL, NULL);
	delete s_xmlAllocator;
	s_xmlAllocator = NULL;
	return 0;
}
