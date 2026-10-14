#include "library.h"

/* Default operation-table entries; the originals contain no debug names. */

int MsproalDefaultReadAttributes(struct MsproalHandle *handle, uint32_t sector, int count,
	const struct MsifDmaDescriptor *transfer)
{
	return -7;
}

int MsproalDefaultRead(struct MsproalHandle *handle, uint32_t sector, int count,
	const struct MsifDmaDescriptor *transfer)
{
	return -7;
}

int MsproalDefaultWrite(struct MsproalHandle *handle, uint32_t sector, int count,
	const struct MsifDmaDescriptor *transfer)
{
	return -7;
}

int MsproalDefaultGetModelName(struct MsproalHandle *handle, char *name)
{
	return -7;
}

int MsproalDefaultSetPowerClass(struct MsproalHandle *handle, int powerClass)
{
	return -7;
}

int MsproalDefaultFormat(struct MsproalHandle *handle, int mode)
{
	return -7;
}

int MsproalDefaultMount(struct MsproalHandle *handle, int busMode)
{
	return -7;
}

int MsproalDefaultSleep(struct MsproalHandle *handle)
{
	return -7;
}

int MsproalDefaultStopAndCheckStatus(struct MsproalHandle *handle)
{
	return -7;
}

int MsproalDefaultStop(struct MsproalHandle *handle)
{
	return -7;
}

int MsproalDefaultReset(struct MsproalHandle *handle)
{
	return -7;
}

const struct MsproalOperations gMsproalDefaultOperations = {
	.readAttributes = MsproalDefaultReadAttributes,
	.read = MsproalDefaultRead,
	.write = MsproalDefaultWrite,
	.getModelName = MsproalDefaultGetModelName,
	.setPowerClass = MsproalDefaultSetPowerClass,
	.format = MsproalDefaultFormat,
	.mount = MsproalDefaultMount,
	.sleep = MsproalDefaultSleep,
	.stopAndCheckStatus = MsproalDefaultStopAndCheckStatus,
	.reset = MsproalDefaultReset,
	.stop = MsproalDefaultStop,
};
