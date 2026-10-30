/* SceGps: the user API's syscalls, which copy the arguments in and out. */
#include "gps_internal.h"

int sceGpsOpen(SceUID callback);
int sceGpsStart(unsigned mode);
int sceGpsStop(void);
int sceGpsClose(void);
int sceGpsIoctl(unsigned command, void *data, unsigned size);
int sceGpsSelectDevice(unsigned device);
int sceGpsResumeCallback(void);
int sceGpsGetState(unsigned *state);
int sceGpsGetData(GpsLocation *location, GpsSatellites *satellites);
int sceGpsGetDeviceInfo(GpsDeviceInfo *info);
int sceGpsIsDevice(void);

#define GPS_IOCTL_MAX	0x640

int _sceGpsOpen(int callback)
{
	return sceGpsOpen(callback);
}

int _sceGpsClose(void)
{
	return sceGpsClose();
}

int _sceGpsStart(unsigned mode)
{
	return sceGpsStart(mode);
}

int _sceGpsStop(void)
{
	return sceGpsStop();
}

int _sceGpsIoctl(unsigned command, void *data, unsigned size, const unsigned *args)
{
	uint8_t buffer[GPS_IOCTL_MAX];
	unsigned input[2];
	int ret, copy;

	ret = sceKernelCopyFromUser(input, args, sizeof(input));
	if (ret < 0)
		return ret;
	if (!data)
		return sceGpsIoctl(command, NULL, size);
	if (input[0] > GPS_IOCTL_MAX)
		return (int)SCE_KERNEL_ERROR_NO_MEMORY;
	memset(buffer, 0, sizeof(buffer));
	ret = sceKernelCopyFromUser(buffer, data, input[0]);
	if (ret < 0)
		return ret;
	ret = sceGpsIoctl(command, buffer, size);
	copy = sceKernelCopyToUser(data, buffer, input[0]);
	return copy < 0 ? copy : ret;
}

int _sceGpsSelectDevice(unsigned device)
{
	return sceGpsSelectDevice(device);
}

int _sceGpsResumeCallback(void)
{
	return sceGpsResumeCallback();
}

int _sceGpsGetState(unsigned *state)
{
	unsigned value = 0;
	int ret, copy;

	if (!state)
		return sceGpsGetState(NULL);
	ret = sceGpsGetState(&value);
	copy = sceKernelCopyToUser(state, &value, sizeof(value));
	return copy < 0 ? copy : ret;
}

int _sceGpsGetData(void *location, void *satellites)
{
	GpsLocation locationValue;
	GpsSatellites satellitesValue;
	int ret, copy;

	if (location)
		memset(&locationValue, 0, sizeof(locationValue));
	if (satellites)
		memset(&satellitesValue, 0, sizeof(satellitesValue));
	ret = sceGpsGetData(location ? &locationValue : NULL, satellites ? &satellitesValue : NULL);
	if (location) {
		copy = sceKernelCopyToUser(location, &locationValue, sizeof(locationValue));
		if (copy < 0)
			return copy;
	}
	if (satellites) {
		copy = sceKernelCopyToUser(satellites, &satellitesValue, sizeof(satellitesValue));
		if (copy < 0)
			ret = copy;
	}
	return ret;
}

int _sceGpsGetDeviceInfo(void *info)
{
	GpsDeviceInfo value = { 0, 0 };
	int ret, copy;

	if (!info)
		return sceGpsGetDeviceInfo(NULL);
	ret = sceGpsGetDeviceInfo(&value);
	copy = sceKernelCopyToUser(info, &value, sizeof(value));
	return copy < 0 ? copy : ret;
}

int _sceGpsIsDevice(void)
{
	return sceGpsIsDevice();
}
