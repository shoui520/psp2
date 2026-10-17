/* Ordered lockup history snapshots; native process and record counts wrap. */
#include "lockup-history.h"
#include "registerdump_sce.h"
#include <sysmem/sysmem.h>
#include <psp2kern/kernel/threadmgr.h>
#include <sysmem/sysclib.h>

/* Kernel NID FE39AEAC is not the four-argument Annex K SDK routine.
 * Native Sysmem consumes destination/source/limit and returns destination.
 */
extern char *GpuBoundedStringCopy(char *destination, const char *source,
				unsigned limit) __asm__("strncpy_s");

/* Native record copies access six words, including the 64-bit timestamp. */
typedef unsigned GpuHistoryWord __attribute__((may_alias));

struct GpuLockupHistory *volatile lockupHistory;
struct GpuLockupHistory *volatile orderedLockupHistory;

int sceGpuCollectFrameNumSceneIndexInfo(SceGpuRegisterDump *dump);
int ExtractRegisterDumpFrameInfo(const SceGpuRegisterDump *dump,
		unsigned *first, unsigned *second);

void RecordGpuLockup(SceGpuRegisterDump *dump, unsigned reason,
		SceUID processUid, unsigned analysisDetail)
{
	struct GpuLockupHistory *history;
	struct GpuLockupProcessHistory *process;
	struct GpuLockupRecord *record;
	unsigned processCount, processIndex, recordIndex;
	unsigned first, second;
	const char *name;
	int nameLength;

	history = lockupHistory;
	processCount = history->processCount;
	processIndex = processCount ? (processCount - 1) & 7 : 0;
	if (history->processes[processIndex].processUid != processUid) {
		if (processCount)
			processIndex = (processIndex + 1) & 7;
		history->processCount = processCount + 1;
		if (processUid != -1) {
			name = ksceGUIDName(processUid);
			if (name) {
				nameLength = strnlen(name, 31);
				if (nameLength > 0)
					GpuBoundedStringCopy(lockupHistory->processes[
						processIndex].processName, name, nameLength + 1);
			}
		}
		process = &lockupHistory->processes[processIndex];
		process->recordCount = 0;
		process->processUid = processUid;
	}
	process = &lockupHistory->processes[processIndex];
	recordIndex = process->recordCount & 7;
	record = &process->records[recordIndex];
	record->timestamp = ksceKernelGetSystemTimeWide();
	record = &lockupHistory->processes[processIndex].records[recordIndex];
	record->reason = reason;
	sceGpuCollectFrameNumSceneIndexInfo(dump);
	first = 0;
	second = 0;
	ExtractRegisterDumpFrameInfo(dump, &first, &second);
	process = &lockupHistory->processes[processIndex];
	record = &process->records[recordIndex];
	record->frameInfoFirst = first;
	record->analysisDetail = analysisDetail;
	record->frameInfoSecond = second;
	if (!process->recordCount)
		process->firstRecord = process->records[0];
	process->recordCount++;
}

unsigned SceGpuEs4ForDriver_395F7689(void)
{
	struct GpuLockupHistory *history = lockupHistory;

	if (!history)
		return 0x810;
	return history->processCount ? 0x810 : 0;
}

unsigned SceGpuEs4ForDriver_8A3929D2(void *destination, unsigned size)
{
	unsigned processCount, firstProcess, process;

	if (!size)
		return 0;
	processCount = lockupHistory->processCount;
	if (!processCount) {
		memset(destination, 0, size);
		return 0;
	}
	firstProcess = processCount > 7 ? processCount - 8 : 0;
	orderedLockupHistory->processCount = processCount;
	for (process = firstProcess; ; process++) {
		struct GpuLockupHistory *history;
		volatile struct GpuLockupProcessHistory *source;
		volatile struct GpuLockupProcessHistory *output;
		unsigned recordCount, firstRecord, record, nameLength;

		history = lockupHistory;
		if (process >= history->processCount)
			break;
		source = &history->processes[process & 7];
		output = &orderedLockupHistory->processes[process - firstProcess];
		recordCount = source->recordCount;
		firstRecord = recordCount > 7 ? recordCount - 8 : 0;
		output->recordCount = source->recordCount;
		output->processUid = source->processUid;
		output->firstRecord = source->firstRecord;
		nameLength = strnlen((const char *)source->processName, 31);
		GpuBoundedStringCopy((char *)output->processName,
				    (const char *)source->processName, nameLength + 1);
		for (record = firstRecord; record < recordCount; record++) {
			const volatile GpuHistoryWord *inputWords;
			const volatile GpuHistoryWord *end;
			volatile GpuHistoryWord *outputWords;

			inputWords = (const volatile GpuHistoryWord *)
				&lockupHistory->processes[process & 7].records[record & 7];
			outputWords = (volatile GpuHistoryWord *)
				&orderedLockupHistory->processes[process - firstProcess]
				.records[record - firstRecord];
			end = inputWords + 6;
			do {
				*outputWords++ = *inputWords++;
			} while (inputWords != end);
		}
	}
	if (!destination)
		return 0x804c0000;
	memset(destination, 0, size);
	memcpy(destination, orderedLockupHistory, size);
	return 0;
}
