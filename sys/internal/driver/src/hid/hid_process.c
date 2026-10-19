/* Descriptive filename; these helpers have no debug source records. */
#include "hid_internal.h"
#include <threadmgr/threadmgr.h>
#include <acmgr/acmgr.h>
#include <ctrl/ctrl.h>
#include <string.h>

unsigned int hidProcessCount;
HidProcessLink hidProcessHead;
ListNode *hidProcessIterator;

int hidAddProcess(SceUID pid)
{
	HidProcessContext *process;
	HidProcessLink *link;
	ListNode *previous;
	unsigned int index;

	process = hidAllocateMemory(sizeof(*process));
	if (process == NULL)
		return 0;
	memset(process, 0, sizeof(*process));
	process->pid = pid;
	process->flags = 1;
	process->privilegeFlags = 0;
	if (SceSblACMgrForDriver_991FDC15(pid) != 0 ||
	    sceSblACMgrIsKernelProgram(pid) != 0 ||
	    SceSblACMgrForDriver_2E992B02(pid) != 0)
		process->privilegeFlags |= 1;
	if (sceSblACMgrIsSystemProgram(pid) != 0)
		process->privilegeFlags |= 2;
	process->keyboardIntercept.pending = 0;
	process->keyboardIntercept.enabled = 0;
	process->keyboardIntercept.pendingEnabled = 0;
	for (index = 0; index < 24; index++) {
		process->reports[index].readIndex = hidGetCollectionWriteIndex(index);
		process->reports[index].spinlock = 0;
	}
	if (hidProcessHead.link.next != NULL && hidProcessHead.link.prev != NULL) {
		link = hidAllocateMemory(sizeof(*link));
		if (link != NULL) {
			previous = hidProcessHead.link.prev;
			__builtin_memset(link, 0, sizeof(*link));
			hidProcessCount++;
			link->link.next = &hidProcessHead.link;
			link->link.prev = previous;
			previous->next = &link->link;
			hidProcessHead.link.prev = &link->link;
			link->process = process;
			return 1;
		}
	}
	hidFreeMemory(process);
	return 0;
}

int hidInitializeProcessList(void)
{
	SceUID pid;
	ListNode *link;
	HidProcessLink *entry = NULL;

	hidProcessHead.link.next = &hidProcessHead.link;
	hidProcessHead.link.prev = &hidProcessHead.link;
	pid = sceKernelGetProcessId();
	link = hidProcessHead.link.next;
	if (link != NULL && hidProcessHead.link.prev != NULL) {
		while (link != &hidProcessHead.link) {
			if (((HidProcessLink *)link)->process->pid == pid) {
				entry = (HidProcessLink *)link;
				break;
			}
			link = link->next;
		}
	}
	if (entry == NULL)
		return hidAddProcess(pid);
	return pid;
}

void hidResetProcessIteration(void)
{
	hidProcessIterator = hidProcessHead.link.next;
}

HidReportCursor *hidNextProcessReportCursor(unsigned int index)
{
	ListNode *link;
	HidProcessContext *process;
	HidReportCursor *cursor;

	if (index >= 24)
		return NULL;
	link = hidProcessIterator;
	if (link == NULL || link == &hidProcessHead.link)
		return NULL;
	process = ((HidProcessLink *)link)->process;
	cursor = process != NULL ? &process->reports[index] : NULL;
	hidProcessIterator = link->next;
	return cursor;
}

HidReportCursor *hidGetProcessReportCursor(unsigned int index)
{
	SceUID pid = sceKernelGetProcessId();
	ListNode *link = hidProcessHead.link.next;
	HidProcessContext *process;

	if (link == NULL || hidProcessHead.link.prev == NULL)
		return NULL;
	while (link != &hidProcessHead.link) {
		process = ((HidProcessLink *)link)->process;
		if (process->pid == pid) {
			if (index >= 24)
				return NULL;
			return &process->reports[index];
		}
		link = link->next;
	}
	return NULL;
}

int hidIsProcessStopped(SceUID pid)
{
	ListNode *link = hidProcessHead.link.next;
	HidProcessContext *process;

	if (link == NULL || hidProcessHead.link.prev == NULL)
		return 0;
	while (link != &hidProcessHead.link) {
		process = ((HidProcessLink *)link)->process;
		if (pid == process->pid)
			return (process->flags & 0x10) != 0;
		link = link->next;
	}
	return 0;
}

int hidHasProcess(SceUID pid)
{
	ListNode *link = hidProcessHead.link.next;

	if (link != NULL && hidProcessHead.link.prev != NULL) {
		while (link != &hidProcessHead.link) {
			if (pid == ((HidProcessLink *)link)->process->pid)
				return 1;
			link = link->next;
		}
	}
	return 0;
}

int hidGetProcessPrivileges(SceUID pid, unsigned int *privileges)
{
	ListNode *link = hidProcessHead.link.next;
	HidProcessContext *process;

	if (link != NULL && hidProcessHead.link.prev != NULL) {
		while (link != &hidProcessHead.link) {
			process = ((HidProcessLink *)link)->process;
			if (pid == process->pid) {
				*privileges = process->privilegeFlags;
				return 0;
			}
			link = link->next;
		}
	}
	return (int)0x803b0006;
}

int hidGetKeyboardIntercept(SceUID pid, HidKeyboardIntercept *intercept)
{
	ListNode *link = hidProcessHead.link.next;
	HidProcessLink *entry = NULL;

	if (link != NULL && hidProcessHead.link.prev != NULL) {
		while (link != &hidProcessHead.link) {
			entry = (HidProcessLink *)link;
			if (pid == entry->process->pid)
				break;
			entry = NULL;
			link = link->next;
		}
	}
	if (intercept == NULL)
		return (int)0x803b0001;
	if (entry == NULL || entry->process == NULL)
		return (int)0x803b0006;
	intercept->pending = entry->process->keyboardIntercept.pending;
	intercept->enabled = entry->process->keyboardIntercept.enabled;
	intercept->pendingEnabled = entry->process->keyboardIntercept.pendingEnabled;
	return 0;
}

int hidSetKeyboardIntercept(SceUID pid, HidKeyboardIntercept intercept)
{
	ListNode *link = hidProcessHead.link.next;
	HidProcessLink *entry;

	if (link != NULL && hidProcessHead.link.prev != NULL) {
		while (link != &hidProcessHead.link) {
			entry = (HidProcessLink *)link;
			if (pid == entry->process->pid) {
				entry->process->keyboardIntercept.pending = intercept.pending;
				entry->process->keyboardIntercept.enabled = intercept.enabled;
				entry->process->keyboardIntercept.pendingEnabled =
					intercept.pendingEnabled;
				return 0;
			}
			link = link->next;
		}
	}
	return (int)0x803b0006;
}

int hidProcessCreate(SceUID pid, ProcEventCreateParam *param, void *common)
{
	int result = hidLock();
	ListNode *link;

	if (result != 0)
		return result;
	link = hidProcessHead.link.next;
	if (link == NULL || hidProcessHead.link.prev == NULL ||
	    link == &hidProcessHead.link)
		goto add;
	while (pid != ((HidProcessLink *)link)->process->pid) {
		link = link->next;
		if (link == &hidProcessHead.link)
			goto add;
	}
	hidUnlock();
	return result;
add:
	hidAddProcess(pid);
	hidUnlock();
	return result;
}

int hidProcessExit(SceUID pid, ProcEventParam *param, void *common)
{
	int result = hidLock();
	ListNode *link, *previous;
	HidProcessLink *entry;
	HidProcessContext *process;

	if (result != 0)
		return result;
	link = hidProcessHead.link.next;
	if (link != NULL && hidProcessHead.link.prev != NULL) {
		while (link != &hidProcessHead.link) {
			entry = (HidProcessLink *)link;
			process = entry->process;
			if (pid == process->pid) {
				hidFreeMemory(process);
				if (hidProcessHead.link.next != NULL &&
				    hidProcessHead.link.prev != NULL &&
				    link != &hidProcessHead.link) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory(entry);
					hidProcessCount--;
				}
				break;
			}
			link = link->next;
		}
	}
	hidReleaseProcessCallbacks(pid);
	hidUnlock();
	return result;
}

int hidProcessKill(SceUID pid, ProcEventParam *param, void *common)
{
	int result = hidLock();
	ListNode *link, *previous;
	HidProcessLink *entry;
	HidProcessContext *process;

	if (result != 0)
		return result;
	link = hidProcessHead.link.next;
	if (link != NULL && hidProcessHead.link.prev != NULL) {
		while (link != &hidProcessHead.link) {
			entry = (HidProcessLink *)link;
			process = entry->process;
			if (pid == process->pid) {
				hidFreeMemory(process);
				if (hidProcessHead.link.next != NULL &&
				    hidProcessHead.link.prev != NULL &&
				    link != &hidProcessHead.link) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory(entry);
					hidProcessCount--;
				}
				break;
			}
			link = link->next;
		}
	}
	hidReleaseProcessCallbacks(pid);
	hidUnlock();
	return result;
}

int hidProcessStop(SceUID pid, int eventType, ProcEventParam *param,
		   void *common)
{
	ListNode *link;
	HidProcessLink *entry;
	HidProcessContext *process;
	int state;

	switch (eventType) {
	case 513:
		link = hidProcessHead.link.next;
		if (link != NULL && hidProcessHead.link.prev != NULL &&
		    link != &hidProcessHead.link) {
			while (pid != ((HidProcessLink *)link)->process->pid) {
				link = link->next;
				if (link == &hidProcessHead.link)
					break;
			}
		}
		return 0;
	case 514:
		link = hidProcessHead.link.next;
		if (link != NULL && hidProcessHead.link.prev != NULL) {
			while (link != &hidProcessHead.link) {
				process = ((HidProcessLink *)link)->process;
				if (pid == process->pid)
					return (process->flags & 0x10) != 0 ?
						0 : (int)0x803b0007;
				link = link->next;
			}
		}
		return (int)0x803b0007;
	case 512:
		link = hidProcessHead.link.next;
		if (link == NULL || hidProcessHead.link.prev == NULL)
			return (int)0x803b0005;
		while (link != &hidProcessHead.link) {
			entry = (HidProcessLink *)link;
			process = entry->process;
			if (pid == process->pid) {
				state = SceCpuForDriver_D32ACE9E(
					&process->spinlock);
				entry->process->flags |= 0x10;
				sceKernelSpinlockLowUnlockCpuResumeIntr(
					&entry->process->spinlock, state);
				return 0;
			}
			link = link->next;
		}
		return (int)0x803b0006;
	default:
		return 0;
	}
}
