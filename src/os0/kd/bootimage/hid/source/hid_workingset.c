#include "hid_internal.h"
#include <sce/debug.h>

int ksceHidKeyboardEnumerate(SceUID *handles, int count);
int ksceHidKeyboardClear(SceUID handle);

int processResumeStart(SceUID pid, int eventType,
		       SceProcEventInvokeParam1 *param, int arg)
{
	SceUID handles[8];
	ListNode *link;
	HidProcessLink *entry;
	HidProcessContext *process;
	int state, count, index, result;

	switch (eventType) {
	case 0x20001:
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
	case 0x40000:
		link = hidProcessHead.link.next;
		if (link == NULL || hidProcessHead.link.prev == NULL)
			return (int)0x803b0005;
		while (link != &hidProcessHead.link) {
			entry = (HidProcessLink *)link;
			process = entry->process;
			if (pid == process->pid) {
				state = ksceKernelSpinlockLowLockCpuSuspendIntr(
					&process->spinlock);
				entry->process->flags &= ~0x100u;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(
					&entry->process->spinlock, state);
				break;
			}
			link = link->next;
		}
		link = hidProcessHead.link.next;
		if (link == NULL || hidProcessHead.link.prev == NULL)
			return (int)0x803b0005;
		while (link != &hidProcessHead.link) {
			entry = (HidProcessLink *)link;
			process = entry->process;
			if (pid == process->pid) {
				state = ksceKernelSpinlockLowLockCpuSuspendIntr(
					&process->spinlock);
				entry->process->flags &= ~0x10u;
				ksceKernelSpinlockLowUnlockCpuResumeIntr(
					&entry->process->spinlock, state);
				return 0;
			}
			link = link->next;
		}
		return (int)0x803b0006;
	case 0x20000:
		link = hidProcessHead.link.next;
		if (link != NULL && hidProcessHead.link.prev != NULL) {
			while (link != &hidProcessHead.link) {
				entry = (HidProcessLink *)link;
				process = entry->process;
				if (pid == process->pid) {
					state = ksceKernelSpinlockLowLockCpuSuspendIntr(
						&process->spinlock);
					entry->process->flags |= 0x100;
					ksceKernelSpinlockLowUnlockCpuResumeIntr(
						&entry->process->spinlock, state);
					break;
				}
				link = link->next;
			}
		}
		__builtin_memset(handles, 0, sizeof(handles));
		result = ksceHidKeyboardEnumerate(handles, 8);
		count = result;
		if (result < 0) {
			SCE_DBG_PRINTF(0, 454,
				"sceHidKeyboardEnumerate failed 0x%08x\n", result);
		} else if (count > 0) {
			for (index = 0; index != count; index++) {
				result = ksceHidKeyboardClear(handles[index]);
				if (result < 0)
					SCE_DBG_PRINTF(0, 462,
						"sceHidKeyboardClear failed 0x%08x\n",
						result);
			}
		}
		return result;
	default:
		return 0;
	}
}
