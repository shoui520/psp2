#include "atexit.h"
#include "paf_crt_mgr.h"
#include "../stdlib.h"
#include <stddef.h>

typedef struct PafAtexitEntry {
	struct PafAtexitEntry *next;
	void (*func)(void *);
	void *arg;
	void *dso;
} PafAtexitEntry;

typedef char PafAtexitEntrySizeCheck[sizeof(PafAtexitEntry) == 16 ? 1 : -1];

static PafAtexitEntry atexitEntries[8];
static PafAtexitEntry *atexitList;

int ScePafStdc_AD9500EF(void (*func)(void *), void *arg, void *dso)
{
	PafAtexitEntry *entry = NULL;
	void *handle;

	if (PafCrtManagerGet())
		PafCrtManagerLock(PafCrtManagerGet(), 1);
	else
		__breakpoint(0);
	for (unsigned int i = 0; i < 8; i++) {
		if (!atexitEntries[i].func) {
			entry = &atexitEntries[i];
			break;
		}
	}
	if (!entry)
		entry = sce_paf_malloc(sizeof(*entry));
	if (!entry) {
		if (PafCrtManagerGet())
			PafCrtManagerUnlock(PafCrtManagerGet(), 1);
		else
			__breakpoint(0);
		return 1;
	}
	handle = *(void **)dso;
	entry->func = func;
	entry->next = atexitList;
	entry->arg = arg;
	entry->dso = handle;
	atexitList = entry;
	if (PafCrtManagerGet())
		PafCrtManagerUnlock(PafCrtManagerGet(), 1);
	else
		__breakpoint(0);
	return 0;
}

int ScePafStdc_C95732BB(void *object, void (*destroy)(void *), void *dso)
{
	return ScePafStdc_AD9500EF(destroy, object, dso);
}

int ScePafStdc_F36BABE3(void *dso)
{
	PafCrtManager *manager;
	PafAtexitEntry *head, *lastHead, *previous = NULL, *entry;

	if (PafCrtManagerGet())
		PafCrtManagerLock(PafCrtManagerGet(), 1);
	else
		__breakpoint(0);
	if (dso) {
		head = lastHead = entry = atexitList;
		while (entry) {
			int matches = entry->dso == *(void **)dso;
			PafAtexitEntry *next = entry->next;

			if (matches) {
				void (*func)(void *);
				void *arg;
				unsigned int i;

				if (entry == head) {
					atexitList = next;
					lastHead = next;
				}
				if (previous)
					previous->next = next;
				if (PafCrtManagerGet())
					PafCrtManagerUnlock(PafCrtManagerGet(), 1);
				func = entry->func;
				arg = entry->arg;
				for (i = 0; i < 8; i++) {
					if (entry == &atexitEntries[i]) {
						entry->func = NULL;
						break;
					}
				}
				if (i == 8)
					sce_paf_free(entry);
				func(arg);
				if (PafCrtManagerGet())
					PafCrtManagerLock(PafCrtManagerGet(), 1);
				head = atexitList;
			} else {
				previous = entry;
			}
			entry = next;
			if (lastHead != head)
				entry = lastHead = head;
		}
	}
	manager = PafCrtManagerGet();
	if (manager)
		return PafCrtManagerUnlock(PafCrtManagerGet(), 1);
	return 0;
}
