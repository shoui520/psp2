/*
 * SceShell mail_resident_plugin: plugin entry & heap allocator wrappers
 * 3.65-CEX: 0x811C6ECC - 0x811C6F94
 */

#include <stddef.h>
#include <scetypes.h>
#include <memory/heap_allocator.h>
#include <plugin.h>

#include "mail_resident.h"

static MailResidentApp *s_mailResidentApp = NULL;		/* 0x81544E08 */
static paf::memory::HeapAllocator *s_mailResidentHeap = NULL;	/* 0x81544E0C */

/* 0x811C6ECC */
void *mailResidentMalloc(SceSize len)
{
	return s_mailResidentHeap->sce_paf_malloc(len);
}

/* 0x811C6EE2 */
void *mailResidentRealloc(void *ptr, SceSize len)
{
	return s_mailResidentHeap->sce_paf_realloc(ptr, len);
}

/* 0x811C6EFA */
void mailResidentFree(void *ptr)
{
	s_mailResidentHeap->sce_paf_free(ptr);
}

/* 0x811C6F10 */
void mailResidentOnInit(void)
{
}

/* 0x811C6F12 */
void mailResidentPluginInit(paf::Plugin *plugin)
{
	s_mailResidentHeap = plugin->m_private_heap;
}

/* 0x811C6F22 */
void mailResidentPluginStart(paf::Plugin *plugin)
{
	(void)plugin;
	if (s_mailResidentApp == NULL) {
		s_mailResidentApp = new MailResidentApp();
		s_mailResidentApp->Start();
	}
}

/* 0x811C6F5E */
void mailResidentPluginStop(paf::Plugin *plugin)
{
	(void)plugin;
	if (s_mailResidentApp != NULL) {
		s_mailResidentApp->Stop();
		delete s_mailResidentApp;
		s_mailResidentApp = NULL;
	}
}

/* 0x811C6F90 */
void mailResidentOnExit(void)
{
}
