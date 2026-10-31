/* Descriptive filename; these callbacks have no source-file debug records. */
#include "hid_internal.h"
#include <sysmem/uid_guid.h>
#include <sysmem/sysmem.h>
#include <threadmgr/threadmgr.h>
#include <intrmgr/syscall.h>
#include <string.h>
#include <hid/hid.h>

HidHintCallbacks hidMouseCallbacks;
unsigned int hidCallbacksInitialized;
HidHintCallbacks hidKeyboardCallbacks;
HidHintCallbacks hidConsumerCallbacks;

#define RELEASE_PROCESS_CALLBACK(head) do { \
	link = (head).link.next; \
	while (link != &(head).link) { \
		if (pid == ((HidHintCallback *)link)->pid) { \
			previous = link->prev; \
			previous->next = link->next; \
			link->next->prev = previous; \
			hidFreeMemory((HidHintCallback *)link); \
			break; \
		} \
		link = link->next; \
	} \
} while (0)

void hidReleaseProcessCallbacks(SceUID pid)
{
	ListNode *link, *previous;

	if (hidCallbacksInitialized != 0) {
		RELEASE_PROCESS_CALLBACK(hidMouseCallbacks.enumeration);
		RELEASE_PROCESS_CALLBACK(hidMouseCallbacks.read);
		RELEASE_PROCESS_CALLBACK(hidKeyboardCallbacks.enumeration);
		RELEASE_PROCESS_CALLBACK(hidKeyboardCallbacks.read);
		RELEASE_PROCESS_CALLBACK(hidConsumerCallbacks.enumeration);
		RELEASE_PROCESS_CALLBACK(hidConsumerCallbacks.read);
	}
}

#undef RELEASE_PROCESS_CALLBACK

void hidNotifyMouseEnumeration(void)
{
	ListNode *link;
	HidHintCallback *entry;
	SceUID callbackId;

	if (hidCallbacksInitialized == 0)
		return;
	for (link = hidMouseCallbacks.enumeration.link.next;
	     link != &hidMouseCallbacks.enumeration.link; link = link->next) {
		entry = (HidHintCallback *)link;
		hidIsProcessStopped(entry->pid);
		callbackId = entry->callbackId;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
}

void hidNotifyMouseRead(void)
{
	ListNode *link;
	HidHintCallback *entry;
	SceUID callbackId;

	if (hidCallbacksInitialized == 0)
		return;
	for (link = hidMouseCallbacks.read.link.next;
	     link != &hidMouseCallbacks.read.link; link = link->next) {
		entry = (HidHintCallback *)link;
		if (hidIsProcessStopped(entry->pid) == 0) {
			callbackId = entry->callbackId;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(callbackId, 0);
		}
	}
}

void hidNotifyKeyboardEnumeration(void)
{
	ListNode *link;
	HidHintCallback *entry;
	SceUID callbackId;

	if (hidCallbacksInitialized == 0)
		return;
	for (link = hidKeyboardCallbacks.enumeration.link.next;
	     link != &hidKeyboardCallbacks.enumeration.link; link = link->next) {
		entry = (HidHintCallback *)link;
		hidIsProcessStopped(entry->pid);
		callbackId = entry->callbackId;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
}

void hidNotifyKeyboardRead(void)
{
	ListNode *link;
	HidHintCallback *entry;
	SceUID callbackId;

	if (hidCallbacksInitialized == 0)
		return;
	for (link = hidKeyboardCallbacks.read.link.next;
	     link != &hidKeyboardCallbacks.read.link; link = link->next) {
		entry = (HidHintCallback *)link;
		if (hidIsProcessStopped(entry->pid) == 0) {
			callbackId = entry->callbackId;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(callbackId, 0);
		}
	}
}

void hidNotifyConsumerEnumeration(void)
{
	ListNode *link;
	HidHintCallback *entry;
	SceUID callbackId;

	if (hidCallbacksInitialized == 0)
		return;
	for (link = hidConsumerCallbacks.enumeration.link.next;
	     link != &hidConsumerCallbacks.enumeration.link; link = link->next) {
		entry = (HidHintCallback *)link;
		hidIsProcessStopped(entry->pid);
		callbackId = entry->callbackId;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
}

void hidNotifyConsumerRead(void)
{
	ListNode *link;
	HidHintCallback *entry;
	SceUID callbackId;

	if (hidCallbacksInitialized == 0)
		return;
	for (link = hidConsumerCallbacks.read.link.next;
	     link != &hidConsumerCallbacks.read.link; link = link->next) {
		entry = (HidHintCallback *)link;
		if (hidIsProcessStopped(entry->pid) == 0) {
			callbackId = entry->callbackId;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(callbackId, 0);
		}
	}
}

#define INITIALIZE_HINT_CALLBACKS() do { \
	hidMouseCallbacks.enumeration.link.next = &hidMouseCallbacks.enumeration.link; \
	hidMouseCallbacks.enumeration.link.prev = &hidMouseCallbacks.enumeration.link; \
	hidMouseCallbacks.read.link.next = &hidMouseCallbacks.read.link; \
	hidMouseCallbacks.read.link.prev = &hidMouseCallbacks.read.link; \
	hidKeyboardCallbacks.enumeration.link.next = &hidKeyboardCallbacks.enumeration.link; \
	hidKeyboardCallbacks.enumeration.link.prev = &hidKeyboardCallbacks.enumeration.link; \
	hidKeyboardCallbacks.read.link.next = &hidKeyboardCallbacks.read.link; \
	hidKeyboardCallbacks.read.link.prev = &hidKeyboardCallbacks.read.link; \
	hidConsumerCallbacks.enumeration.link.next = &hidConsumerCallbacks.enumeration.link; \
	hidConsumerCallbacks.enumeration.link.prev = &hidConsumerCallbacks.enumeration.link; \
	hidConsumerCallbacks.read.link.next = &hidConsumerCallbacks.read.link; \
	hidConsumerCallbacks.read.link.prev = &hidConsumerCallbacks.read.link; \
	hidMouseCallbacks.enumeration.callbackId = -1; \
	hidMouseCallbacks.enumeration.pid = -1; \
	hidMouseCallbacks.read.callbackId = -1; \
	hidMouseCallbacks.read.pid = -1; \
	hidKeyboardCallbacks.enumeration.callbackId = -1; \
	hidKeyboardCallbacks.enumeration.pid = -1; \
	hidKeyboardCallbacks.read.callbackId = -1; \
	hidKeyboardCallbacks.read.pid = -1; \
	hidConsumerCallbacks.enumeration.callbackId = -1; \
	hidConsumerCallbacks.enumeration.pid = -1; \
	hidConsumerCallbacks.read.callbackId = -1; \
	hidConsumerCallbacks.read.pid = -1; \
	hidCallbacksInitialized = 1; \
} while (0)

int SceHidForDriver_141CA100(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;
	HidHintCallback *entry;

	if (result != 0)
		return result;
	if (hidCallbacksInitialized != 0) {
		for (link = hidMouseCallbacks.enumeration.link.next;
		     link != &hidMouseCallbacks.enumeration.link; link = link->next) {
			if (callbackId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
	} else {
		INITIALIZE_HINT_CALLBACKS();
	}
	entry = hidAllocateMemory(sizeof(*entry));
	if (entry != NULL) {
		memset(entry, 0, sizeof(*entry));
		previous = hidMouseCallbacks.enumeration.link.prev;
		entry->link.next = &hidMouseCallbacks.enumeration.link;
		entry->link.prev = previous;
		previous->next = &entry->link;
		entry->callbackId = callbackId;
		entry->pid = SCE_KERNEL_PROCESS_ID;
		hidMouseCallbacks.enumeration.link.prev = &entry->link;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
unlock:
	hidUnlock();
	return result;
}

int sceHidMouseRegisterEnumHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId, pid;
	int result;
	ListNode *link, *previous;
	HidHintCallback *entry;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized == 0)
			INITIALIZE_HINT_CALLBACKS();
		pid = sceKernelGetProcessId();
		for (link = hidMouseCallbacks.enumeration.link.next;
		     link != &hidMouseCallbacks.enumeration.link; link = link->next) {
			if (kernelId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
		entry = hidAllocateMemory(sizeof(*entry));
		if (entry != NULL) {
			memset(entry, 0, sizeof(*entry));
			previous = hidMouseCallbacks.enumeration.link.prev;
			entry->link.next = &hidMouseCallbacks.enumeration.link;
			entry->link.prev = previous;
			previous->next = &entry->link;
			entry->callbackId = kernelId;
			entry->pid = pid;
			hidMouseCallbacks.enumeration.link.prev = &entry->link;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(kernelId, 0);
		}
unlock:
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_9AFBB275(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;

	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidMouseCallbacks.enumeration.link.next;
			     link != &hidMouseCallbacks.enumeration.link; link = link->next) {
				if (callbackId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	return result;
}

int sceHidMouseUnregisterEnumHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId;
	int result;
	ListNode *link, *previous;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidMouseCallbacks.enumeration.link.next;
			     link != &hidMouseCallbacks.enumeration.link; link = link->next) {
				if (kernelId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_41690428(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;
	HidHintCallback *entry;

	if (result != 0)
		return result;
	if (hidCallbacksInitialized != 0) {
		for (link = hidMouseCallbacks.read.link.next;
		     link != &hidMouseCallbacks.read.link; link = link->next) {
			if (callbackId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
	} else {
		INITIALIZE_HINT_CALLBACKS();
	}
	entry = hidAllocateMemory(sizeof(*entry));
	if (entry != NULL) {
		memset(entry, 0, sizeof(*entry));
		previous = hidMouseCallbacks.read.link.prev;
		entry->link.next = &hidMouseCallbacks.read.link;
		entry->link.prev = previous;
		previous->next = &entry->link;
		entry->callbackId = callbackId;
		entry->pid = SCE_KERNEL_PROCESS_ID;
		hidMouseCallbacks.read.link.prev = &entry->link;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
unlock:
	hidUnlock();
	return result;
}

int sceHidMouseRegisterReadHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId, pid;
	int result;
	ListNode *link, *previous;
	HidHintCallback *entry;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized == 0)
			INITIALIZE_HINT_CALLBACKS();
		pid = sceKernelGetProcessId();
		for (link = hidMouseCallbacks.read.link.next;
		     link != &hidMouseCallbacks.read.link; link = link->next) {
			if (kernelId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
		entry = hidAllocateMemory(sizeof(*entry));
		if (entry != NULL) {
			memset(entry, 0, sizeof(*entry));
			previous = hidMouseCallbacks.read.link.prev;
			entry->link.next = &hidMouseCallbacks.read.link;
			entry->link.prev = previous;
			previous->next = &entry->link;
			entry->callbackId = kernelId;
			entry->pid = pid;
			hidMouseCallbacks.read.link.prev = &entry->link;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(kernelId, 0);
		}
unlock:
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_63776265(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;

	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidMouseCallbacks.read.link.next;
			     link != &hidMouseCallbacks.read.link; link = link->next) {
				if (callbackId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	return result;
}

int sceHidMouseUnregisterReadHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId;
	int result;
	ListNode *link, *previous;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidMouseCallbacks.read.link.next;
			     link != &hidMouseCallbacks.read.link; link = link->next) {
				if (kernelId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_21A23FCD(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;
	HidHintCallback *entry;

	if (result != 0)
		return result;
	if (hidCallbacksInitialized != 0) {
		for (link = hidKeyboardCallbacks.enumeration.link.next;
		     link != &hidKeyboardCallbacks.enumeration.link; link = link->next) {
			if (callbackId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
	} else {
		INITIALIZE_HINT_CALLBACKS();
	}
	entry = hidAllocateMemory(sizeof(*entry));
	if (entry != NULL) {
		memset(entry, 0, sizeof(*entry));
		previous = hidKeyboardCallbacks.enumeration.link.prev;
		entry->link.next = &hidKeyboardCallbacks.enumeration.link;
		entry->link.prev = previous;
		previous->next = &entry->link;
		entry->callbackId = callbackId;
		entry->pid = SCE_KERNEL_PROCESS_ID;
		hidKeyboardCallbacks.enumeration.link.prev = &entry->link;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
unlock:
	hidUnlock();
	return result;
}

int sceHidKeyboardRegisterEnumHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId, pid;
	int result;
	ListNode *link, *previous;
	HidHintCallback *entry;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized == 0)
			INITIALIZE_HINT_CALLBACKS();
		pid = sceKernelGetProcessId();
		for (link = hidKeyboardCallbacks.enumeration.link.next;
		     link != &hidKeyboardCallbacks.enumeration.link; link = link->next) {
			if (kernelId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
		entry = hidAllocateMemory(sizeof(*entry));
		if (entry != NULL) {
			memset(entry, 0, sizeof(*entry));
			previous = hidKeyboardCallbacks.enumeration.link.prev;
			entry->link.next = &hidKeyboardCallbacks.enumeration.link;
			entry->link.prev = previous;
			previous->next = &entry->link;
			entry->callbackId = kernelId;
			entry->pid = pid;
			hidKeyboardCallbacks.enumeration.link.prev = &entry->link;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(kernelId, 0);
		}
unlock:
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_69DEE02F(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;

	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidKeyboardCallbacks.enumeration.link.next;
			     link != &hidKeyboardCallbacks.enumeration.link;
			     link = link->next) {
				if (callbackId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	return result;
}

int sceHidKeyboardUnregisterEnumHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId;
	int result;
	ListNode *link, *previous;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidKeyboardCallbacks.enumeration.link.next;
			     link != &hidKeyboardCallbacks.enumeration.link;
			     link = link->next) {
				if (kernelId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_B0B22694(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;
	HidHintCallback *entry;

	if (result != 0)
		return result;
	if (hidCallbacksInitialized != 0) {
		for (link = hidKeyboardCallbacks.read.link.next;
		     link != &hidKeyboardCallbacks.read.link; link = link->next) {
			if (callbackId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
	} else {
		INITIALIZE_HINT_CALLBACKS();
	}
	entry = hidAllocateMemory(sizeof(*entry));
	if (entry != NULL) {
		memset(entry, 0, sizeof(*entry));
		previous = hidKeyboardCallbacks.read.link.prev;
		entry->link.next = &hidKeyboardCallbacks.read.link;
		entry->link.prev = previous;
		previous->next = &entry->link;
		entry->callbackId = callbackId;
		entry->pid = SCE_KERNEL_PROCESS_ID;
		hidKeyboardCallbacks.read.link.prev = &entry->link;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
unlock:
	hidUnlock();
	return result;
}

int sceHidKeyboardRegisterReadHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId, pid;
	int result;
	ListNode *link, *previous;
	HidHintCallback *entry;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized == 0)
			INITIALIZE_HINT_CALLBACKS();
		pid = sceKernelGetProcessId();
		for (link = hidKeyboardCallbacks.read.link.next;
		     link != &hidKeyboardCallbacks.read.link; link = link->next) {
			if (kernelId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
		entry = hidAllocateMemory(sizeof(*entry));
		if (entry != NULL) {
			memset(entry, 0, sizeof(*entry));
			previous = hidKeyboardCallbacks.read.link.prev;
			entry->link.next = &hidKeyboardCallbacks.read.link;
			entry->link.prev = previous;
			previous->next = &entry->link;
			entry->callbackId = kernelId;
			entry->pid = pid;
			hidKeyboardCallbacks.read.link.prev = &entry->link;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(kernelId, 0);
		}
unlock:
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_050351F2(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;

	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidKeyboardCallbacks.read.link.next;
			     link != &hidKeyboardCallbacks.read.link; link = link->next) {
				if (callbackId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	return result;
}

int sceHidKeyboardUnregisterReadHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId;
	int result;
	ListNode *link, *previous;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidKeyboardCallbacks.read.link.next;
			     link != &hidKeyboardCallbacks.read.link; link = link->next) {
				if (kernelId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_7F444D9C(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;
	HidHintCallback *entry;

	if (result != 0)
		return result;
	if (hidCallbacksInitialized != 0) {
		for (link = hidConsumerCallbacks.enumeration.link.next;
		     link != &hidConsumerCallbacks.enumeration.link; link = link->next) {
			if (callbackId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
	} else {
		INITIALIZE_HINT_CALLBACKS();
	}
	entry = hidAllocateMemory(sizeof(*entry));
	if (entry != NULL) {
		memset(entry, 0, sizeof(*entry));
		previous = hidConsumerCallbacks.enumeration.link.prev;
		entry->link.next = &hidConsumerCallbacks.enumeration.link;
		entry->link.prev = previous;
		previous->next = &entry->link;
		entry->callbackId = callbackId;
		entry->pid = SCE_KERNEL_PROCESS_ID;
		hidConsumerCallbacks.enumeration.link.prev = &entry->link;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
unlock:
	hidUnlock();
	return result;
}

int sceHidConsumerControlRegisterEnumHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId, pid;
	int result;
	ListNode *link, *previous;
	HidHintCallback *entry;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized == 0)
			INITIALIZE_HINT_CALLBACKS();
		pid = sceKernelGetProcessId();
		for (link = hidConsumerCallbacks.enumeration.link.next;
		     link != &hidConsumerCallbacks.enumeration.link; link = link->next) {
			if (kernelId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
		entry = hidAllocateMemory(sizeof(*entry));
		if (entry != NULL) {
			memset(entry, 0, sizeof(*entry));
			previous = hidConsumerCallbacks.enumeration.link.prev;
			entry->link.next = &hidConsumerCallbacks.enumeration.link;
			entry->link.prev = previous;
			previous->next = &entry->link;
			entry->callbackId = kernelId;
			entry->pid = pid;
			hidConsumerCallbacks.enumeration.link.prev = &entry->link;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(kernelId, 0);
		}
unlock:
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_6FFC9302(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;

	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidConsumerCallbacks.enumeration.link.next;
			     link != &hidConsumerCallbacks.enumeration.link; link = link->next) {
				if (callbackId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	return result;
}

int sceHidConsumerControlUnregisterEnumHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId;
	int result;
	ListNode *link, *previous;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidConsumerCallbacks.enumeration.link.next;
			     link != &hidConsumerCallbacks.enumeration.link; link = link->next) {
				if (kernelId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_710B353C(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;
	HidHintCallback *entry;

	if (result != 0)
		return result;
	if (hidCallbacksInitialized != 0) {
		for (link = hidConsumerCallbacks.read.link.next;
		     link != &hidConsumerCallbacks.read.link; link = link->next) {
			if (callbackId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
	} else {
		INITIALIZE_HINT_CALLBACKS();
	}
	entry = hidAllocateMemory(sizeof(*entry));
	if (entry != NULL) {
		memset(entry, 0, sizeof(*entry));
		previous = hidConsumerCallbacks.read.link.prev;
		entry->link.next = &hidConsumerCallbacks.read.link;
		entry->link.prev = previous;
		previous->next = &entry->link;
		entry->callbackId = callbackId;
		entry->pid = SCE_KERNEL_PROCESS_ID;
		hidConsumerCallbacks.read.link.prev = &entry->link;
		if (hidGetFlags(0x10) == 0)
			sceKernelNotifyCallback(callbackId, 0);
	}
unlock:
	hidUnlock();
	return result;
}

int sceHidConsumerControlRegisterReadHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId, pid;
	int result;
	ListNode *link, *previous;
	HidHintCallback *entry;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized == 0)
			INITIALIZE_HINT_CALLBACKS();
		pid = sceKernelGetProcessId();
		for (link = hidConsumerCallbacks.read.link.next;
		     link != &hidConsumerCallbacks.read.link; link = link->next) {
			if (kernelId == ((HidHintCallback *)link)->callbackId)
				goto unlock;
		}
		entry = hidAllocateMemory(sizeof(*entry));
		if (entry != NULL) {
			memset(entry, 0, sizeof(*entry));
			previous = hidConsumerCallbacks.read.link.prev;
			entry->link.next = &hidConsumerCallbacks.read.link;
			entry->link.prev = previous;
			previous->next = &entry->link;
			entry->callbackId = kernelId;
			entry->pid = pid;
			hidConsumerCallbacks.read.link.prev = &entry->link;
			if (hidGetFlags(0x10) == 0)
				sceKernelNotifyCallback(kernelId, 0);
		}
unlock:
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_3EC4E469(SceUID callbackId)
{
	int result = hidLock();
	ListNode *link, *previous;

	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidConsumerCallbacks.read.link.next;
			     link != &hidConsumerCallbacks.read.link; link = link->next) {
				if (callbackId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	return result;
}

int sceHidConsumerControlUnregisterReadHintCallback(SceUID callbackId)
{
	SceUInt32 syscallState = sceSyscallEnter();
	SceUID kernelId;
	int result;
	ListNode *link, *previous;

	kernelId = scePUIDtoGUID(0, callbackId);
	if (kernelId < 0) {
		sceSyscallLeave(syscallState);
		return kernelId;
	}
	result = hidLock();
	if (result == 0) {
		if (hidCallbacksInitialized != 0) {
			for (link = hidConsumerCallbacks.read.link.next;
			     link != &hidConsumerCallbacks.read.link; link = link->next) {
				if (kernelId == ((HidHintCallback *)link)->callbackId) {
					previous = link->prev;
					previous->next = link->next;
					link->next->prev = previous;
					hidFreeMemory((HidHintCallback *)link);
					break;
				}
			}
		}
		hidUnlock();
	}
	sceSyscallLeave(syscallState);
	return result;
}

int SceHidForDriver_8DE66028(SceUID callbackId)
{
	return (int)0x803B0004;
}

int sceHidControllerRegisterEnumHintCallback(SceUID callbackId)
{
	return (int)0x803B0004;
}

int SceHidForDriver_FC1C743A(SceUID callbackId)
{
	return (int)0x803B0004;
}

int sceHidControllerUnregisterEnumHintCallback(SceUID callbackId)
{
	return (int)0x803B0004;
}

int SceHidForDriver_D658B068(SceUID callbackId)
{
	return (int)0x803B0004;
}

int sceHidControllerRegisterReadHintCallback(SceUID callbackId)
{
	return (int)0x803B0004;
}

int SceHidForDriver_42B43C21(SceUID callbackId)
{
	return (int)0x803B0004;
}

int sceHidControllerUnregisterReadHintCallback(SceUID callbackId)
{
	return (int)0x803B0004;
}

#define COUNT_HINT_CALLBACKS(first, head) do { \
	(first) = (head).link.next; \
	listCount = 0; \
	for (link = (first); link != &(head).link; link = link->next) \
		++listCount; \
	total += listCount; \
} while (0)

#define DUMP_HINT_CALLBACKS(first, head, device, hint) do { \
	for (link = (first); link != &(head).link;) { \
		HidHintCallbackInfo info = {0}; \
		++copied; \
		info.size = sizeof(info); \
		info.deviceType = (device); \
		info.hintType = (hint); \
		info.callbackId = ((HidHintCallback *)link)->callbackId; \
		info.pid = ((HidHintCallback *)link)->pid; \
		if (size < copied * sizeof(info)) \
			goto out; \
		link = link->next; \
		__builtin_prefetch((void *)((uintptr_t)output + 6 * sizeof(*output))); \
		__builtin_memcpy(output, &info, sizeof(info)); \
		++output; \
	} \
} while (0)

int hidDumpLists(void *buffer, SceSize size, SceUInt32 *count)
{
	HidHintCallbackInfo *output = buffer;
	ListNode *mouseEnumeration, *mouseRead, *keyboardEnumeration;
	ListNode *keyboardRead, *consumerEnumeration, *consumerRead, *link;
	unsigned int copied = 0;
	int listCount, total = 0;

	if (size > sizeof(*output) - 1 && buffer == NULL)
		return (int)0x80020005;
	if (hidCallbacksInitialized == 0)
		return 0;
	COUNT_HINT_CALLBACKS(mouseEnumeration, hidMouseCallbacks.enumeration);
	COUNT_HINT_CALLBACKS(mouseRead, hidMouseCallbacks.read);
	COUNT_HINT_CALLBACKS(keyboardEnumeration, hidKeyboardCallbacks.enumeration);
	COUNT_HINT_CALLBACKS(keyboardRead, hidKeyboardCallbacks.read);
	COUNT_HINT_CALLBACKS(consumerEnumeration, hidConsumerCallbacks.enumeration);
	COUNT_HINT_CALLBACKS(consumerRead, hidConsumerCallbacks.read);
	if (size != 0 || buffer != NULL) {
		DUMP_HINT_CALLBACKS(mouseEnumeration, hidMouseCallbacks.enumeration, 1, 1);
		DUMP_HINT_CALLBACKS(mouseRead, hidMouseCallbacks.read, 1, 2);
		DUMP_HINT_CALLBACKS(keyboardEnumeration, hidKeyboardCallbacks.enumeration, 2, 1);
		DUMP_HINT_CALLBACKS(keyboardRead, hidKeyboardCallbacks.read, 2, 2);
		DUMP_HINT_CALLBACKS(consumerEnumeration, hidConsumerCallbacks.enumeration, 3, 1);
		DUMP_HINT_CALLBACKS(consumerRead, hidConsumerCallbacks.read, 3, 2);
		if (count != NULL)
			*count = copied;
	}
out:
	return total;
}

#undef DUMP_HINT_CALLBACKS
#undef COUNT_HINT_CALLBACKS
