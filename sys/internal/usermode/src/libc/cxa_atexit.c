/*
 * The C++ ABI's exit handlers: __cxa_atexit and __at_quick_exit register a function for a
 * module (its DSO handle), __cxa_finalize runs the module's exit handlers and drops its
 * quick-exit ones, quick_exit runs the main module's quick-exit handlers. An entry comes from
 * a fixed pool while one is free, else from malloc; the function is kept xored with the
 * pointer guard.
 */
#include <stddef.h>
#include <stdlib.h>
#include <yvals.h>
#include <kernel/processmgr.h>

#include "libc_internal.h"

#define ATEXIT_POOL_COUNT	64
#define QUICK_EXIT_POOL_COUNT	32

typedef struct AtexitEntry {
	struct AtexitEntry *next;
	unsigned int func;	/* the function ^ _Getptrguard() */
	void *arg;
	void *dso;		/* the module's DSO handle */
} AtexitEntry;

static AtexitEntry *atexitList = NULL;
static unsigned int atexitPoolUsed = 0;
static unsigned int atexitGeneration = 0;	/* changes with each registration */
static AtexitEntry *quickExitList = NULL;
static unsigned int quickExitPoolUsed = 0;
static unsigned int quickExitGeneration = 0;

static AtexitEntry quickExitPool[QUICK_EXIT_POOL_COUNT];
static AtexitEntry atexitPool[ATEXIT_POOL_COUNT];

int __cxa_atexit(void (*func)(void *), void *arg, void *dso)
{
	AtexitEntry *entry = NULL;
	unsigned int i;

	_Locksyslock(_LOCK_ATEXIT);
	if (atexitPoolUsed < ATEXIT_POOL_COUNT) {
		AtexitEntry *slot = atexitPool;

		for (i = 0; i < ATEXIT_POOL_COUNT; i++, slot++) {
			if (slot->func == 0) {
				atexitPoolUsed++;
				entry = slot;
				break;
			}
		}
	}
	if (entry == NULL) {
		entry = malloc(sizeof(*entry));
		if (entry == NULL) {
			_Unlocksyslock(_LOCK_ATEXIT);
			return 1;
		}
	}
	entry->next = atexitList;
	entry->func = (unsigned int)func ^ _Getptrguard();
	entry->arg = arg;
	if (dso == NULL)
		dso = &_Dso_handle_main;
	entry->dso = *(void **)dso;
	atexitList = entry;
	atexitGeneration++;
	_Unlocksyslock(_LOCK_ATEXIT);
	return 0;
}

int __aeabi_atexit(void *arg, void (*func)(void *), void *dso)
{
	return __cxa_atexit(func, arg, dso);
}

void __cxa_finalize(void *dso)
{
	if (dso != NULL) {
		AtexitEntry *prev = NULL;
		AtexitEntry *head;
		AtexitEntry *entry;
		AtexitEntry *next;
		unsigned int generation;
		unsigned int seen;

		_Locksyslock(_LOCK_ATEXIT);
		head = atexitList;
		generation = atexitGeneration;
		seen = generation;
		for (entry = head; entry != NULL; entry = next) {
			next = entry->next;
			if (entry->dso == *(void **)dso) {
				unsigned int func;
				void *arg;

				if (entry == head)
					atexitList = entry->next;
				else
					prev->next = next;
				func = entry->func;
				arg = entry->arg;
				if (entry <= &atexitPool[ATEXIT_POOL_COUNT - 1]
				    && &atexitPool[0] <= entry) {
					entry->func = 0;
					atexitPoolUsed--;
				} else {
					free(entry);
				}
				_Unlocksyslock(_LOCK_ATEXIT);
				((void (*)(void *))(func ^ _Getptrguard()))(arg);
				_Locksyslock(_LOCK_ATEXIT);
				generation = atexitGeneration;
				head = atexitList;
			} else {
				prev = entry;
			}
			if (seen != generation) {	/* a handler registered another: start over */
				next = head;
				seen = generation;
			}
		}
		_Unlocksyslock(_LOCK_ATEXIT);
	}
	if (dso != NULL) {
		AtexitEntry *prev = NULL;
		AtexitEntry *entry;
		AtexitEntry *next;

		_Locksyslock(_LOCK_ATEXIT);
		for (entry = quickExitList; entry != NULL; entry = next) {
			next = entry->next;
			if (entry->dso == *(void **)dso) {
				if (entry == quickExitList)
					quickExitList = entry->next;
				else
					prev->next = next;
				if (entry <= &quickExitPool[QUICK_EXIT_POOL_COUNT - 1]
				    && &quickExitPool[0] <= entry) {
					quickExitPoolUsed--;
					entry->func = 0;
				} else {
					free(entry);
				}
			} else {
				prev = entry;
			}
		}
		_Unlocksyslock(_LOCK_ATEXIT);
	}
}

int __at_quick_exit(void (*func)(void *), void *arg, void *dso)
{
	AtexitEntry *entry = NULL;
	unsigned int i;

	_Locksyslock(_LOCK_ATEXIT);
	if (quickExitPoolUsed < QUICK_EXIT_POOL_COUNT) {
		AtexitEntry *slot = quickExitPool;

		for (i = 0; i < QUICK_EXIT_POOL_COUNT; i++, slot++) {
			if (slot->func == 0) {
				quickExitPoolUsed++;
				entry = slot;
				break;
			}
		}
	}
	if (entry == NULL) {
		entry = malloc(sizeof(*entry));
		if (entry == NULL) {
			_Unlocksyslock(_LOCK_ATEXIT);
			return 1;
		}
	}
	entry->next = quickExitList;
	entry->func = (unsigned int)func ^ _Getptrguard();
	entry->arg = arg;
	if (dso == NULL)
		dso = &_Dso_handle_main;
	entry->dso = *(void **)dso;
	quickExitList = entry;
	quickExitGeneration++;
	_Unlocksyslock(_LOCK_ATEXIT);
	return 0;
}

void quick_exit(int status)
{
	AtexitEntry *head;
	AtexitEntry *entry;
	AtexitEntry *next;
	unsigned int generation;
	unsigned int seen;

	sceKernelCallAbortHandler(status == 0 ? SCE_LIBC_QUICK_EXIT_SUCCESS_CODE
					      : SCE_LIBC_QUICK_EXIT_FAILURE_CODE, NULL);
	_Locksyslock(_LOCK_ATEXIT);
	head = quickExitList;
	generation = quickExitGeneration;
	seen = generation;
	entry = head;
	if (entry != NULL) {
		AtexitEntry *prev = NULL;

		for (; entry != NULL; entry = next) {
			next = entry->next;
			if (entry->dso == _Dso_handle_main) {
				unsigned int func;
				void *arg;

				if (entry == head)
					quickExitList = entry->next;
				else
					prev->next = next;
				func = entry->func;
				arg = entry->arg;
				if (entry <= &quickExitPool[QUICK_EXIT_POOL_COUNT - 1]
				    && &quickExitPool[0] <= entry) {
					quickExitPoolUsed--;
					entry->func = 0;
				} else {
					free(entry);
				}
				_Unlocksyslock(_LOCK_ATEXIT);
				((void (*)(void *))(func ^ _Getptrguard()))(arg);
				_Locksyslock(_LOCK_ATEXIT);
				head = quickExitList;
				generation = quickExitGeneration;
			} else {
				prev = entry;
			}
			if (seen != generation) {
				next = head;
				seen = generation;
			}
		}
	}
	_Unlocksyslock(_LOCK_ATEXIT);
	_Exit(status);
}
