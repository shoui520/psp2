/* SceGps: the user API's argument checks, between the syscalls and their bodies.
 * Internal names unknown: named after the user API they serve.
 */
#include "gps_internal.h"

int sceGpsOpen(SceUID callback)
{
	return openCallback(callback);
}

int sceGpsStart(unsigned mode)
{
	return startAcquisition(mode);
}

int sceGpsStop(void)
{
	return stopAcquisition();
}

int sceGpsClose(void)
{
	return closeCallback();
}

int sceGpsIoctl(unsigned command, void *data, unsigned size)
{
	if (!data || !size)
		return GPS_ERROR(32);
	return requestIoctl(command, data, size);
}

int sceGpsSelectDevice(unsigned device)
{
	return chooseDevice(device);
}

int sceGpsResumeCallback(void)
{
	return resumeNotices();
}

int sceGpsGetState(unsigned *state)
{
	if (!state)
		return GPS_ERROR(32);
	return readState(state);
}

int sceGpsGetData(GpsLocation *location, GpsSatellites *satellites)
{
	if (!location || !satellites)
		return GPS_ERROR(32);
	return readLocation(location, satellites);
}

int sceGpsGetDeviceInfo(GpsDeviceInfo *info)
{
	if (!info)
		return GPS_ERROR(32);
	return readInfo(info);
}

int sceGpsIsDevice(void)
{
	return queryDevice();
}
