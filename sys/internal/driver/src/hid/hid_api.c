/* Descriptive filename; these exports have no source-file debug records. */
#include "hid_internal.h"
#include <intrmgr/syscall.h>
#include <sysmem/sysmem.h>
#include <psp2/kernel/threadmgr/thread.h>
#include <string.h>

int ksceHidMouseEnumerate(SceUID *handle, int count)
{
	int result, copied, index;
	HidCollection *collection;

	if (handle == NULL)
		return (int)UINT32_C(0x803b0001);
	result = hidLock();
	copied = result;
	if (result == 0) {
		if (count >= 8)
			count = 8;
		for (index = 0; index < count; index++) {
			collection = hidCollections[index];
			if (collection != NULL && collection->device != NULL)
				handle[copied++] = collection->id;
		}
		hidUnlock();
		return copied;
	}
	return result;
}

int sceHidMouseEnumerate(SceUID *handle, int count)
{
	SceUID handles[8];
	HidCollection *collection;
	unsigned int syscall;
	int result, copied = 0, index;

	syscall = sceSyscallEnter();
	if (handle == NULL) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0001);
	}
	result = hidLock();
	if (result == 0) {
		if (count >= 8)
			count = 8;
		for (index = 0; index < count; index++) {
			collection = hidCollections[index];
			if (collection != NULL && collection->device != NULL)
				handles[copied++] = collection->id;
		}
		if (copied != 0)
			result = SceSysmemForDriver_6D88EF8A(handle, handles, copied * sizeof(*handles));
		hidUnlock();
	}
	sceSyscallLeave(syscall);
	return result < 0 ? result : copied;
}

#define FIND_MOUSE_COLLECTION(number) \
	collection = hidCollections[number]; \
	if (collection != NULL && collection->id == handle) \
		goto found;

int ksceHidMouseRead(SceUID handle, SceHidMouseReport *reports, int nReports)
{
	HidCollection *collection;
	int result;

	if (handle <= 0)
		return (int)UINT32_C(0x803b0002);
	if (reports == NULL || nReports < 0)
		return (int)UINT32_C(0x803b0001);
	result = hidLock();
	if (result != 0)
		return result;
	FIND_MOUSE_COLLECTION(0);
	FIND_MOUSE_COLLECTION(1);
	FIND_MOUSE_COLLECTION(2);
	FIND_MOUSE_COLLECTION(3);
	FIND_MOUSE_COLLECTION(4);
	FIND_MOUSE_COLLECTION(5);
	FIND_MOUSE_COLLECTION(6);
	FIND_MOUSE_COLLECTION(7);
	hidUnlock();
	return (int)UINT32_C(0x803b0003);
found:
	if (nReports >= 16)
		nReports = 16;
	result = hidReadCollectionReports(collection, reports, nReports);
	hidUnlock();
	return result;
}

int sceHidMouseRead(SceUInt32 handle, SceHidMouseReport *reports, int nReports)
{
	HidCollection *collection;
	SceHidMouseReport *buffer;
	unsigned int syscall;
	int status, copied = 0;

	syscall = sceSyscallEnter();
	if ((int)handle <= 0) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0002);
	}
	if (reports == NULL || nReports < 0) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0001);
	}
	status = hidLock();
	if (status != 0)
		goto leave;
	FIND_MOUSE_COLLECTION(0);
	FIND_MOUSE_COLLECTION(1);
	FIND_MOUSE_COLLECTION(2);
	FIND_MOUSE_COLLECTION(3);
	FIND_MOUSE_COLLECTION(4);
	FIND_MOUSE_COLLECTION(5);
	FIND_MOUSE_COLLECTION(6);
	FIND_MOUSE_COLLECTION(7);
	status = (int)UINT32_C(0x803b0003);
	goto unlock;
found:
	if (nReports >= 16)
		nReports = 16;
	buffer = hidAllocateMemory(nReports * sizeof(*buffer));
	if (buffer != NULL) {
		copied = hidReadCollectionReports(collection, buffer, nReports);
		status = copied;
		if (copied > 0)
			status = SceSysmemForDriver_6D88EF8A(reports, buffer, copied * sizeof(*buffer));
		hidFreeMemory(buffer);
	}
unlock:
	hidUnlock();
leave:
	sceSyscallLeave(syscall);
	return status < 0 ? status : copied;
}

#undef FIND_MOUSE_COLLECTION

int ksceHidKeyboardEnumerate(SceUID *handle, int count)
{
	int result, copied, index;
	HidCollection *collection;

	if (handle == NULL)
		return (int)UINT32_C(0x803b0001);
	result = hidLock();
	copied = result;
	if (result == 0) {
		if (count >= 8)
			count = 8;
		for (index = 0; index < count; index++) {
			collection = hidCollections[index + 8];
			if (collection != NULL && collection->device != NULL)
				handle[copied++] = collection->id;
		}
		hidUnlock();
		return copied;
	}
	return result;
}

int sceHidKeyboardEnumerate(SceUID *handle, int count)
{
	SceUID handles[8];
	HidCollection *collection;
	unsigned int syscall;
	int result, copied = 0, index;

	syscall = sceSyscallEnter();
	if (handle == NULL) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0001);
	}
	result = hidLock();
	if (result == 0) {
		if (count >= 8)
			count = 8;
		for (index = 0; index < count; index++) {
			collection = hidCollections[index + 8];
			if (collection != NULL && collection->device != NULL)
				handles[copied++] = collection->id;
		}
		if (copied != 0)
			result = SceSysmemForDriver_6D88EF8A(handle, handles, copied * sizeof(*handles));
		hidUnlock();
	}
	sceSyscallLeave(syscall);
	return result < 0 ? result : copied;
}

#define FIND_KEYBOARD_COLLECTION(number) \
	collection = hidCollections[number + 8]; \
	if (collection != NULL && collection->id == handle) \
		goto found;

int ksceHidKeyboardRead(SceUID handle, SceHidKeyboardReport *reports, int nReports)
{
	HidCollection *collection;
	int result;

	if (handle <= 0)
		return (int)UINT32_C(0x803b0002);
	if (reports == NULL || nReports < 0)
		return (int)UINT32_C(0x803b0001);
	result = hidLock();
	if (result != 0)
		return result;
	FIND_KEYBOARD_COLLECTION(0);
	FIND_KEYBOARD_COLLECTION(1);
	FIND_KEYBOARD_COLLECTION(2);
	FIND_KEYBOARD_COLLECTION(3);
	FIND_KEYBOARD_COLLECTION(4);
	FIND_KEYBOARD_COLLECTION(5);
	FIND_KEYBOARD_COLLECTION(6);
	FIND_KEYBOARD_COLLECTION(7);
	hidUnlock();
	return (int)UINT32_C(0x803b0003);
found:
	if (nReports >= 16)
		nReports = 16;
	result = hidReadCollectionReports(collection, reports, nReports);
	hidUnlock();
	return result;
}

int sceHidKeyboardRead(SceUInt32 handle, SceHidKeyboardReport *reports, int nReports)
{
	HidCollection *collection;
	SceHidKeyboardReport *buffer;
	unsigned int syscall;
	int status, copied = 0;

	syscall = sceSyscallEnter();
	if ((int)handle <= 0) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0002);
	}
	if (reports == NULL || nReports < 0) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0001);
	}
	status = hidLock();
	if (status != 0)
		goto leave;
	FIND_KEYBOARD_COLLECTION(0);
	FIND_KEYBOARD_COLLECTION(1);
	FIND_KEYBOARD_COLLECTION(2);
	FIND_KEYBOARD_COLLECTION(3);
	FIND_KEYBOARD_COLLECTION(4);
	FIND_KEYBOARD_COLLECTION(5);
	FIND_KEYBOARD_COLLECTION(6);
	FIND_KEYBOARD_COLLECTION(7);
	status = (int)UINT32_C(0x803b0003);
	goto unlock;
found:
	if (nReports >= 16)
		nReports = 16;
	buffer = hidAllocateMemory(nReports * sizeof(*buffer));
	if (buffer != NULL) {
		copied = hidReadCollectionReports(collection, buffer, nReports);
		status = copied;
		if (copied > 0)
			status = SceSysmemForDriver_6D88EF8A(reports, buffer, copied * sizeof(*buffer));
		hidFreeMemory(buffer);
	}
unlock:
	hidUnlock();
leave:
	sceSyscallLeave(syscall);
	return status < 0 ? status : copied;
}

#undef FIND_KEYBOARD_COLLECTION

/* The original embeds this report snapshot in both peek entry points. */
#define PEEK_COLLECTION_REPORTS(destination, capCount) do { \
	HidReportCursor *cursor; \
	HidReportRouting *routing; \
	unsigned int index, privileges = 0; \
	SceUID pid; \
	int intrState, position, copyIndex; \
	for (index = 0; index != 24; index++) { \
		if (hidCollections[index] == collection) \
			break; \
	} \
	if (index == 24) \
		index = (unsigned int)-1; \
	cursor = hidGetProcessReportCursor(index); \
	copied = 0; \
	if (cursor == NULL) \
		break; \
	if (collection->type == 1) { \
		routing = &hidRouting[0]; \
	} else if (collection->type == 2) { \
		routing = &hidRouting[1]; \
	} else if (collection->type == 3) { \
		routing = &hidRouting[2]; \
	} else { \
		routing = NULL; \
	} \
	pid = sceKernelGetProcessId(); \
	hidGetProcessPrivileges(pid, &privileges); \
	intrState = SceCpuForDriver_D32ACE9E(&cursor->spinlock); \
	copied = cursor->peekCount; \
	if ((capCount) && copied >= 16) \
		copied = 16; \
	if (copied >= nReports) \
		copied = nReports; \
	position = cursor->readIndex + cursor->pendingCount - copied; \
	if (position < 0) \
		position += 16; \
	sceKernelSpinlockLowUnlockCpuResumeIntr(&cursor->spinlock, intrState); \
	for (copyIndex = 0; copyIndex < copied; copyIndex++) { \
		HidReportEntry *entry = &collection->reports[position]; \
		switch (collection->type) { \
		case 1: \
			((SceHidMouseReport *)(destination))[copyIndex] = entry->report.mouse; \
			break; \
		case 2: { \
			SceHidKeyboardReport *output = (destination); \
			if ((privileges & 1) || \
			    (pid == routing->ownerPid && (int)entry->flags >= 0)) { \
				output[copyIndex] = entry->report.keyboard; \
			} else { \
				__builtin_prefetch(&output[copyIndex + 3].modifiers[1]); \
				memset(&output[copyIndex], 0, sizeof(*output)); \
				output[copyIndex].error = 0; \
				output[copyIndex].modifiers[0] = 0; \
				output[copyIndex].modifiers[1] = entry->report.keyboard.modifiers[1]; \
				output[copyIndex].timestamp = entry->report.keyboard.timestamp; \
			} \
			break; \
		} \
		case 3: \
			((SceHidConsumerControlReport *)(destination))[copyIndex] = \
				entry->report.consumer; \
			break; \
		} \
		if (++position > 15) \
			position = 0; \
	} \
} while (0)

#define FIND_KEYBOARD_COLLECTION(number) \
	collection = hidCollections[number + 8]; \
	if (collection != NULL && collection->id == handle) \
		goto found;

int ksceHidKeyboardPeek(SceUID handle, SceHidKeyboardReport *reports, int nReports)
{
	HidCollection *collection;
	int result, copied;

	if (handle <= 0)
		return (int)UINT32_C(0x803b0002);
	if (reports == NULL || nReports < 0)
		return (int)UINT32_C(0x803b0001);
	result = hidLock();
	if (result != 0)
		return result;
	FIND_KEYBOARD_COLLECTION(0);
	FIND_KEYBOARD_COLLECTION(1);
	FIND_KEYBOARD_COLLECTION(2);
	FIND_KEYBOARD_COLLECTION(3);
	FIND_KEYBOARD_COLLECTION(4);
	FIND_KEYBOARD_COLLECTION(5);
	FIND_KEYBOARD_COLLECTION(6);
	FIND_KEYBOARD_COLLECTION(7);
	copied = (int)UINT32_C(0x803b0003);
	goto unlock;
found:
	PEEK_COLLECTION_REPORTS(reports, 1);
unlock:
	hidUnlock();
	return copied;
}

int sceHidKeyboardPeek(SceUInt32 handle, SceHidKeyboardReport *reports, int nReports)
{
	HidCollection *collection;
	SceHidKeyboardReport *buffer;
	unsigned int syscall;
	int status, copied = 0;

	syscall = sceSyscallEnter();
	if ((int)handle <= 0) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0002);
	}
	if (reports == NULL || nReports < 0) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0001);
	}
	status = hidLock();
	if (status != 0)
		goto leave;
	FIND_KEYBOARD_COLLECTION(0);
	FIND_KEYBOARD_COLLECTION(1);
	FIND_KEYBOARD_COLLECTION(2);
	FIND_KEYBOARD_COLLECTION(3);
	FIND_KEYBOARD_COLLECTION(4);
	FIND_KEYBOARD_COLLECTION(5);
	FIND_KEYBOARD_COLLECTION(6);
	FIND_KEYBOARD_COLLECTION(7);
	status = (int)UINT32_C(0x803b0003);
	goto unlock;
found:
	if (nReports >= 16)
		nReports = 16;
	buffer = hidAllocateMemory(nReports * sizeof(*buffer));
	if (buffer != NULL) {
		PEEK_COLLECTION_REPORTS(buffer, 0);
		status = copied;
		if (copied > 0)
			status = SceSysmemForDriver_6D88EF8A(reports, buffer, copied * sizeof(*buffer));
		hidFreeMemory(buffer);
	}
unlock:
	hidUnlock();
leave:
	sceSyscallLeave(syscall);
	return status < 0 ? status : copied;
}

#undef FIND_KEYBOARD_COLLECTION
#undef PEEK_COLLECTION_REPORTS

#define FIND_KEYBOARD_COLLECTION(number) \
	collection = hidCollections[number + 8]; \
	if (collection != NULL && collection->id == handle) \
		goto found;

int ksceHidKeyboardClear(SceUID handle)
{
	HidCollection *collection;
	int result;

	if (handle <= 0)
		return (int)UINT32_C(0x803b0002);
	result = hidLock();
	if (result != 0)
		return result;
	FIND_KEYBOARD_COLLECTION(0);
	FIND_KEYBOARD_COLLECTION(1);
	FIND_KEYBOARD_COLLECTION(2);
	FIND_KEYBOARD_COLLECTION(3);
	FIND_KEYBOARD_COLLECTION(4);
	FIND_KEYBOARD_COLLECTION(5);
	FIND_KEYBOARD_COLLECTION(6);
	FIND_KEYBOARD_COLLECTION(7);
	hidUnlock();
	return (int)UINT32_C(0x803b0003);
found:
	result = hidClearCollectionReports(collection);
	hidUnlock();
	return result;
}

int sceHidKeyboardClear(SceUInt32 handle)
{
	HidCollection *collection;
	unsigned int syscall;
	int result;

	syscall = sceSyscallEnter();
	if ((int)handle <= 0) {
		result = (int)UINT32_C(0x803b0002);
		goto leave;
	}
	result = hidLock();
	if (result != 0)
		goto leave;
	FIND_KEYBOARD_COLLECTION(0);
	FIND_KEYBOARD_COLLECTION(1);
	FIND_KEYBOARD_COLLECTION(2);
	FIND_KEYBOARD_COLLECTION(3);
	FIND_KEYBOARD_COLLECTION(4);
	FIND_KEYBOARD_COLLECTION(5);
	FIND_KEYBOARD_COLLECTION(6);
	FIND_KEYBOARD_COLLECTION(7);
	result = (int)UINT32_C(0x803b0003);
	goto unlock;
found:
	result = hidClearCollectionReports(collection);
unlock:
	hidUnlock();
leave:
	sceSyscallLeave(syscall);
	return result;
}

#undef FIND_KEYBOARD_COLLECTION

int SceHidForDriver_0B7D27CF(SceUID pid)
{
	int result = hidLock();

	if (result != 0)
		return result;
	if (hidHasProcess(pid)) {
		if (hidRouting[1].ownerPid == pid) {
			if (hidRouting[1].pendingOwner)
				hidRouting[1].pendingOwner = 0;
		} else if (hidRouting[1].allKeysReleased) {
			hidRouting[1].ownerPid = pid;
		} else {
			hidRouting[1].pendingOwnerPid = pid;
			hidRouting[1].pendingOwner = 1;
		}
	} else {
		result = (int)UINT32_C(0x803b0006);
	}
	hidUnlock();
	return result;
}

int SceHidForDriver_EB323EC6(SceUID *pid)
{
	if (pid == NULL)
		return (int)UINT32_C(0x803b0001);
	if (hidLock() == 0) {
		*pid = hidRouting[1].ownerPid;
		hidUnlock();
	}
	return 0;
}

int ksceHidKeyboardSetIntercept(unsigned int enable)
{
	HidKeyboardIntercept intercept = {0};
	SceUID pid;

	if (enable > 1)
		return (int)UINT32_C(0x803b0001);
	pid = sceKernelGetProcessId();
	if (hidLock() == 0) {
		if (hidGetKeyboardIntercept(pid, &intercept) >= 0) {
			if (intercept.enabled == enable) {
				if (intercept.pending)
					intercept.pending = 0;
			} else if (hidRouting[1].ownerPid != pid || hidRouting[1].allKeysReleased) {
				intercept.enabled = enable;
			} else {
				intercept.pendingEnabled = enable;
				intercept.pending = 1;
			}
			hidSetKeyboardIntercept(pid, intercept);
		}
		hidUnlock();
	}
	return 0;
}

int sceHidKeyboardSetIntercept(unsigned int enable)
{
	HidKeyboardIntercept intercept;
	SceUID pid;
	unsigned int syscall;
	int result;

	syscall = sceSyscallEnter();
	if (enable > 1) {
		result = (int)UINT32_C(0x803b0001);
		goto leave;
	}
	pid = sceKernelGetProcessId();
	result = hidLock();
	if (result == 0) {
		result = hidGetKeyboardIntercept(pid, &intercept);
		if (result >= 0) {
			if (intercept.enabled == enable) {
				if (intercept.pending)
					intercept.pending = 0;
			} else if (hidRouting[1].ownerPid != pid || hidRouting[1].allKeysReleased) {
				intercept.enabled = enable;
			} else {
				intercept.pendingEnabled = enable;
				intercept.pending = 1;
			}
			result = hidSetKeyboardIntercept(pid, intercept);
		}
		hidUnlock();
	}
leave:
	sceSyscallLeave(syscall);
	return result;
}

int ksceHidKeyboardGetIntercept(int *enable)
{
	HidKeyboardIntercept intercept;
	SceUID pid;
	int result;

	if (enable == NULL)
		return (int)UINT32_C(0x803b0001);
	pid = sceKernelGetProcessId();
	result = hidLock();
	if (result == 0) {
		result = hidGetKeyboardIntercept(pid, &intercept);
		*enable = intercept.enabled == 1;
		hidUnlock();
	}
	return result;
}

int sceHidKeyboardGetIntercept(int *enable)
{
	HidKeyboardIntercept intercept;
	SceUID pid;
	unsigned int syscall;
	int result, value;

	syscall = sceSyscallEnter();
	if (enable == NULL) {
		result = (int)UINT32_C(0x803b0001);
		goto leave;
	}
	intercept = (HidKeyboardIntercept){0};
	pid = sceKernelGetProcessId();
	result = hidLock();
	if (result == 0) {
		result = hidGetKeyboardIntercept(pid, &intercept);
		if (result >= 0) {
			value = intercept.enabled == 1;
			result = SceSysmemForDriver_6D88EF8A(enable, &value, sizeof(value));
		}
		hidUnlock();
	}
leave:
	sceSyscallLeave(syscall);
	return result;
}

int ksceHidConsumerControlEnumerate(SceUID *handle, int count)
{
	int result, copied, index;
	HidCollection *collection;

	if (handle == NULL)
		return (int)UINT32_C(0x803b0001);
	result = hidLock();
	copied = result;
	if (result == 0) {
		if (count >= 8)
			count = 8;
		for (index = 0; index < count; index++) {
			collection = hidCollections[index + 16];
			if (collection != NULL && collection->device != NULL)
				handle[copied++] = collection->id;
		}
		hidUnlock();
		return copied;
	}
	return result;
}

int sceHidConsumerControlEnumerate(SceUID *handle, int count)
{
	SceUID handles[8];
	HidCollection *collection;
	unsigned int syscall;
	int result, copied = 0, index;

	syscall = sceSyscallEnter();
	if (handle == NULL) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0001);
	}
	result = hidLock();
	if (result == 0) {
		if (count >= 8)
			count = 8;
		for (index = 0; index < count; index++) {
			collection = hidCollections[index + 16];
			if (collection != NULL && collection->device != NULL)
				handles[copied++] = collection->id;
		}
		if (copied != 0)
			result = SceSysmemForDriver_6D88EF8A(handle, handles, copied * sizeof(*handles));
		hidUnlock();
	}
	sceSyscallLeave(syscall);
	return result < 0 ? result : copied;
}

#define FIND_CONSUMER_COLLECTION(number) \
	collection = hidCollections[number + 16]; \
	if (collection != NULL && collection->id == handle) \
		goto found;

int ksceHidConsumerControlRead(SceUID handle, SceHidConsumerControlReport *reports,
			       int nReports)
{
	HidCollection *collection;
	int result;

	if (handle <= 0)
		return (int)UINT32_C(0x803b0002);
	if (reports == NULL || nReports < 0)
		return (int)UINT32_C(0x803b0001);
	result = hidLock();
	if (result != 0)
		return result;
	FIND_CONSUMER_COLLECTION(0);
	FIND_CONSUMER_COLLECTION(1);
	FIND_CONSUMER_COLLECTION(2);
	FIND_CONSUMER_COLLECTION(3);
	FIND_CONSUMER_COLLECTION(4);
	FIND_CONSUMER_COLLECTION(5);
	FIND_CONSUMER_COLLECTION(6);
	FIND_CONSUMER_COLLECTION(7);
	hidUnlock();
	return (int)UINT32_C(0x803b0003);
found:
	if (nReports >= 16)
		nReports = 16;
	result = hidReadCollectionReports(collection, reports, nReports);
	hidUnlock();
	return result;
}

int sceHidConsumerControlRead(SceUInt32 handle, SceHidConsumerControlReport *reports,
			      int nReports)
{
	HidCollection *collection;
	SceHidConsumerControlReport *buffer;
	unsigned int syscall;
	int status, copied = 0;

	syscall = sceSyscallEnter();
	if ((int)handle <= 0) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0002);
	}
	if (reports == NULL || nReports < 0) {
		sceSyscallLeave(syscall);
		return (int)UINT32_C(0x803b0001);
	}
	status = hidLock();
	if (status != 0)
		goto leave;
	FIND_CONSUMER_COLLECTION(0);
	FIND_CONSUMER_COLLECTION(1);
	FIND_CONSUMER_COLLECTION(2);
	FIND_CONSUMER_COLLECTION(3);
	FIND_CONSUMER_COLLECTION(4);
	FIND_CONSUMER_COLLECTION(5);
	FIND_CONSUMER_COLLECTION(6);
	FIND_CONSUMER_COLLECTION(7);
	status = (int)UINT32_C(0x803b0003);
	goto unlock;
found:
	if (nReports >= 16)
		nReports = 16;
	buffer = hidAllocateMemory(nReports * sizeof(*buffer));
	if (buffer != NULL) {
		copied = hidReadCollectionReports(collection, buffer, nReports);
		status = copied;
		if (copied > 0)
			status = SceSysmemForDriver_6D88EF8A(reports, buffer, copied * sizeof(*buffer));
		hidFreeMemory(buffer);
	}
unlock:
	hidUnlock();
leave:
	sceSyscallLeave(syscall);
	return status < 0 ? status : copied;
}

#undef FIND_CONSUMER_COLLECTION
