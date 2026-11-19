/*
 * applauncher_heap.cpp: the plugin's private heap kept (0x810C9BF0-0x810C9C7A). The file's and
 * the names are ours (PLACEHOLDER).
 */
#include "applauncher_home.h"
#include "applauncher.h"

/* (0x81555B18) */
paf::memory::HeapAllocator *g_applauncherHeap;

/* (0x810C9BF0) */
void applauncherHeapSet(void)
{
	g_applauncherHeap = g_applauncherPlugin->m_private_heap;
}

/* PLACEHOLDER: an empty function nothing calls (0x810C9C0A) */
void applauncherHeapReserved(void)
{
}
