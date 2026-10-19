#include <threadmgr/threadmgr.h>
#include "spkg_peripherals.h"
#include <deci4p_cpup/cpup.h>

extern int updateCpupContext;
extern int updateCpupUpdateStarted;
extern int updateCpupUpdateTriggered;

int CpUtilWriteFirmware(const void *buffer, uint64_t size, uint32_t version,
	unsigned int nodeType, UpdateUtilNodeStatus *status)
{
	uint32_t result = 0, progress = 0, detail = 0;
	unsigned int total = size;
	unsigned int remaining, offset, chunkSize, context;
	const uint8_t *data = buffer;

	if (!updateCpupConnected)
		return (int)0x800f0202;
	if (sceDeci4pCpupStartUpdateForDriver(version, total,
		(uint32_t *)&updateCpupContext))
		return (int)0x800f0205;
	updateCpupUpdateStarted = 1;
	if (!buffer || !total) {
		SceDeci4pCpupForDriver_02CDA82C(updateCpupContext);
		return (int)0x800f0216;
	}
	remaining = total;
	offset = 0;
	context = updateCpupContext;
	do {
		chunkSize = remaining >= 65536 ? 65536 : remaining;
		if (sceDeci4pCpupTransferUpdatePackageForDriver(context, offset, chunkSize, data)) {
			SceDeci4pCpupForDriver_02CDA82C(updateCpupContext);
			return (int)0x800f0205;
		}
		remaining -= chunkSize;
		offset += chunkSize;
		data += chunkSize;
		if (status) {
			status->progress = 3;
			status->written_rates = 100 - 100 * remaining / total;
			status->seq_no++;
			UpdateUtilUpdateNodeStatus(nodeType, status);
		}
		sceKernelDelayThread(10000);
	} while (remaining);
	if (sceDeci4pCpupTriggerUpdateForDriver(updateCpupContext)) {
		SceDeci4pCpupForDriver_02CDA82C(updateCpupContext);
		return (int)0x800f0205;
	}
	updateCpupUpdateTriggered = 1;
	while (!sceDeci4pCpupGetProgressForDriver(updateCpupContext, &result,
		&progress, &detail)) {
		if (!result)
			return 0;
		if ((int32_t)result < 0)
			break;
		sceKernelDelayThread(100000);
	}
	return (int)0x800f0205;
}
