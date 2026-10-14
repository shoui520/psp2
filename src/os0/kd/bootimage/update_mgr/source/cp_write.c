#include <psp2kern/kernel/threadmgr/thread.h>
#include "spkg_peripherals.h"

extern int updateCpupContext;
extern int updateCpupUpdateStarted;
extern int updateCpupUpdateTriggered;

int SceDeci4pCpupForDriver_DE07478F(uint32_t version, uint32_t size,
	uint32_t *context);
int SceDeci4pCpupForDriver_90339379(uint32_t context, uint32_t offset,
	SceSize size, const void *data);
int SceDeci4pCpupForDriver_02CDA82C(uint32_t context);
int SceDeci4pCpupForDriver_D78403C8(uint32_t context);
int SceDeci4pCpupForDriver_7998FFD8(uint32_t context, uint32_t *result,
	uint32_t *progress, uint32_t *detail);
int UpdateUtilUpdateNodeStatus(unsigned int nodeType, UpdateUtilNodeStatus *status);

int CpUtilWriteFirmware(const void *buffer, uint64_t size, uint32_t version,
	unsigned int nodeType, UpdateUtilNodeStatus *status)
{
	uint32_t result = 0, progress = 0, detail = 0;
	unsigned int total = size;
	unsigned int remaining, offset, chunkSize, context;
	const uint8_t *data = buffer;

	if (!updateCpupConnected)
		return (int)0x800f0202;
	if (SceDeci4pCpupForDriver_DE07478F(version, total,
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
		if (SceDeci4pCpupForDriver_90339379(context, offset, chunkSize, data)) {
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
	if (SceDeci4pCpupForDriver_D78403C8(updateCpupContext)) {
		SceDeci4pCpupForDriver_02CDA82C(updateCpupContext);
		return (int)0x800f0205;
	}
	updateCpupUpdateTriggered = 1;
	while (!SceDeci4pCpupForDriver_7998FFD8(updateCpupContext, &result,
		&progress, &detail)) {
		if (!result)
			return 0;
		if ((int32_t)result < 0)
			break;
		sceKernelDelayThread(100000);
	}
	return (int)0x800f0205;
}
