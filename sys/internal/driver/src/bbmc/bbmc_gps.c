/*
 * The GPS provider: bbmc registers the modem's GPS with SceGps (device 1), passes its
 * start, stop, restart and ioctl requests to the manager (messages 93, 99, 94, 101), and
 * reports the PDS events qmi_solution.c decodes.
 */
#include <stddef.h>
#include <string.h>
#include <gps/gps.h>
#include <threadmgr/threadmgr.h>
#include "bbmc_control.h"
#include "bbmc_message.h"
#include "bbmc_gps.h"

BbmcGpsInfo g_bbmcGpsInfo;

/* name unknown: the driver's ioctl, answered here or run by the manager (message 101) */
static int bbmcGpsIoctl(unsigned command, void *data, unsigned size, unsigned state)
{
	BbmcControl *control;
	BbmcMessage *message;
	int result;

	if ((data == NULL || size == 0) && command != 0x53001000)
		return 0x80390020;
	control = bbmcGetControl();
	if (control == NULL)
		return 0x80330003;
	if (control->gpsProviderRouting != 0) {
		SceGpsForDriver_F2049C02(0, 0x60032);
		return 0;
	}

	switch (command) {
	case 0x5B001003:
		if (state != 3)
			return 0x8039000B;
		if (size != sizeof(GpsResetMasks))
			return 0x80390020;
		break;
	case 0xAB001000:
		if (size != sizeof(BbmcGpsNmea))
			return 0x80390020;
		memcpy(data, &g_bbmcGpsInfo.nmea, sizeof(BbmcGpsNmea));
		return 0;
	case 0xA3001000:
		if (size != 4)
			return 0x80390020;
		*(SceUInt32 *)data = g_bbmcGpsInfo.locationValidFields;
		return 0;
	case 0xAB001001:
		if (size != 4)
			return 0x80390020;
		*(SceUInt32 *)data = g_bbmcGpsInfo.extendedNmeaMode;
		return 0;
	case 0xAB001002:
		if (size != 4)
			return 0x80390020;
		*(QmiPdsWifiRequest *)data = g_bbmcGpsInfo.wifiRequest;
		return 0;
	case 0x5B001000:
		if (size != sizeof(GpsXtraData))
			return 0x80390020;
		break;
	case 0x53001000:
		if (size != 0)
			return 0x80390020;
		break;
	case 0x5B001001:
		if (size != sizeof(GpsAbsoluteTime))
			return 0x80390020;
		break;
	case 0x5B001002:
		if (size != sizeof(GpsWifiPosition))
			return 0x80390020;
		break;
	default:
		return 0x80390020;
	}

	message = msgAllocMsgPacket(control, 101);
	if (message == NULL)
		return 0x80330005;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1 || control->gpsIoctlPending != 0) {
		result = 0x80330006;
		bbmcLeaveCS(&control->mutex);
	} else {
		message->body.gpsRequest.code = command;
		message->body.gpsRequest.data = data;
		message->body.gpsRequest.size = size;
		control->gpsIoctlPending = 1;
		bbmcLeaveCS(&control->mutex);
		result = msgSendMsgPacket(control, message);
		if (result >= 0)
			return result;
	}
	msgFreeMsgPacket(control, message);
	return result;
}

/* name unknown: the driver's restart (message 94): tracking again, in mode 1 */
static int bbmcGpsRestart(void)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int result;

	if (control == NULL)
		return 0x80330003;
	message = msgAllocMsgPacket(control, 94);
	if (message == NULL)
		return 0x80330005;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		result = 0x80330006;
		bbmcLeaveCS(&control->mutex);
		goto fail;
	}
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		goto fail;
	lockFastMutex(&g_bbmcGpsInfo.lock);
	g_bbmcGpsInfo.trackingMode = 1;
	sceKernelUnlockFastMutex(&g_bbmcGpsInfo.lock);
	return result;

fail:
	msgFreeMsgPacket(control, message);
	return result;
}

/* name unknown: the driver's stop (message 99) */
static int bbmcGpsStop(void)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int result;

	if (control == NULL)
		return 0x80330003;
	if (control->gpsProviderRouting != 0) {
		SceGpsForDriver_F2049C02(0, 0x80031);
		return 0;
	}
	message = msgAllocMsgPacket(control, 99);
	if (message == NULL)
		return 0x80330005;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		result = 0x80330006;
		bbmcLeaveCS(&control->mutex);
		goto fail;
	}
	control->trackingState = 5;
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		goto fail;
	return result;

fail:
	msgFreeMsgPacket(control, message);
	return result;
}

/* name unknown: the driver's start (message 93), in mode 0 */
static int bbmcGpsStart(void)
{
	BbmcControl *control = bbmcGetControl();
	BbmcMessage *message;
	int result;

	if (control == NULL)
		return 0x80330003;
	if (control->gpsProviderRouting != 0) {
		SceGpsForDriver_F2049C02(0, 0x10030);
		return 0;
	}
	/* Sony's start does not check the allocation */
	message = msgAllocMsgPacket(control, 93);
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		result = 0x80330006;
		bbmcLeaveCS(&control->mutex);
		goto fail;
	}
	bbmcLeaveCS(&control->mutex);
	result = msgSendMsgPacket(control, message);
	if (result < 0)
		goto fail;
	lockFastMutex(&g_bbmcGpsInfo.lock);
	g_bbmcGpsInfo.trackingMode = 0;
	sceKernelUnlockFastMutex(&g_bbmcGpsInfo.lock);
	return result;

fail:
	msgFreeMsgPacket(control, message);
	return result;
}

int bbmcInitializeGps(void)
{
	BbmcControl *control = bbmcGetControl();
	GpsDriver driver;
	int result;

	if (control == NULL)
		return 0x80330003;
	memset(&g_bbmcGpsInfo, 0, sizeof(g_bbmcGpsInfo));
	sceKernelInitializeFastMutex(&g_bbmcGpsInfo.lock, "SceBbmcGpsInfLock", 2, NULL);
	driver.device = 1;
	driver.flags = 31;
	driver.start = bbmcGpsStart;
	driver.stop = bbmcGpsStop;
	driver.restart = bbmcGpsRestart;
	driver.ioctl = bbmcGpsIoctl;
	result = SceGpsForDriver_884A5867(&driver);
	bbmcEnterCS(&control->mutex);
	control->trackingState = 0;
	bbmcLeaveCS(&control->mutex);
	return result;
}

int bbmcFinalizeGps(void)
{
	int result = 0;

	if (SceGpsForDriver_54DBA451(1) == 0)
		result = SceGpsForDriver_7F001760(1);
	sceKernelFinalizeFastMutex(&g_bbmcGpsInfo.lock);
	return result;
}

/* name unknown: a PDS position: keep it as SceGps's location, with its valid fields */
int bbmcNotifyGpsPosition(QmiPdsPosition *position)
{
	BbmcControl *control = bbmcGetControl();

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return 0x80330006;
	}
	bbmcLeaveCS(&control->mutex);

	lockFastMutex(&g_bbmcGpsInfo.lock);
	memset(&g_bbmcGpsInfo.location, 0, sizeof(g_bbmcGpsInfo.location));
	g_bbmcGpsInfo.locationValidFields = 0;
	if (position->validFields & 1) {
		g_bbmcGpsInfo.locationValidFields = 1;
		g_bbmcGpsInfo.location.time = (SceDateTime){
			.year = position->year,
			.month = position->month,
			.day = position->dayOfMonth,
			.hour = position->hour,
			.minute = position->minute,
			.second = position->second,
			.microsecond = position->millisecond * 1000,
		};
	}
	if (position->validFields & 0x100000) {
		g_bbmcGpsInfo.locationValidFields |= 2;
		g_bbmcGpsInfo.location.hdop = position->horizontalDop;
	}
	if (position->validFields & 0x200000) {
		g_bbmcGpsInfo.locationValidFields |= 4;
		g_bbmcGpsInfo.location.vdop = position->verticalDop;
	}
	if (position->validFields & 0x80000) {
		g_bbmcGpsInfo.locationValidFields |= 8;
		g_bbmcGpsInfo.location.pdop = position->positionDop;
	}
	g_bbmcGpsInfo.location.fixQuality = 1;
	g_bbmcGpsInfo.location.fixType = 1;
	if (position->validFields & 0x10) {
		g_bbmcGpsInfo.locationValidFields |= 0x40;
		g_bbmcGpsInfo.location.latitude = position->latitude;
	}
	if (position->validFields & 0x20) {
		g_bbmcGpsInfo.locationValidFields |= 0x80;
		g_bbmcGpsInfo.location.longitude = position->longitude;
	}
	if (position->validFields & 0x40) {
		g_bbmcGpsInfo.locationValidFields |= 0x1000;
		if (control->versionCapability == 1)
			g_bbmcGpsInfo.location.geoidSeparation = position->altitudeEllipsoid;
		else
			g_bbmcGpsInfo.location.geoidSeparation = position->altitudeEllipsoid * 10.0f - 500.0f;
	}
	if (position->validFields & 0x80) {
		g_bbmcGpsInfo.locationValidFields |= 0x100;
		if (control->versionCapability == 1)
			g_bbmcGpsInfo.location.altitude = position->altitudeSeaLevel;
		else
			g_bbmcGpsInfo.location.altitude = position->altitudeSeaLevel +
					     position->altitudeEllipsoid * 9.0f - 500.0f;
	}
	if (position->validFields & 0x100) {
		g_bbmcGpsInfo.locationValidFields |= 0x200;
		g_bbmcGpsInfo.location.speed = position->horizontalSpeed * 3600.0f / 1000.0f;
	}
	if (position->validFields & 0x400) {
		g_bbmcGpsInfo.locationValidFields |= 0x400;
		g_bbmcGpsInfo.location.course = position->heading;
	}
	sceKernelUnlockFastMutex(&g_bbmcGpsInfo.lock);
	return 0;
}

/* name unknown: a PDS session status: report the kept location to SceGps */
int bbmcNotifyGpsSessionStatus(SceUInt8 *status)
{
	BbmcControl *control = bbmcGetControl();
	int result;

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return 0x80330006;
	}
	bbmcLeaveCS(&control->mutex);
	lockFastMutex(&g_bbmcGpsInfo.lock);
	g_bbmcGpsInfo.location.noClock = *status;
	result = SceGpsForDriver_BE3EB4E9(1, g_bbmcGpsInfo.trackingMode,
					  &g_bbmcGpsInfo.location);
	sceKernelUnlockFastMutex(&g_bbmcGpsInfo.lock);
	return result;
}

/* name unknown: a PDS satellite report: report the satellites in view to SceGps */
int bbmcNotifyGpsSatellites(QmiPdsSatelliteReport *report)
{
	BbmcControl *control = bbmcGetControl();
	QmiPdsSatellite *source;
	GpsSatellite *satellite;
	int result;
	int i;

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return 0x80330006;
	}
	bbmcLeaveCS(&control->mutex);
	lockFastMutex(&g_bbmcGpsInfo.lock);
	memset(&g_bbmcGpsInfo.satellites, 0, sizeof(g_bbmcGpsInfo.satellites));
	memset(g_bbmcGpsInfo.satelliteValidFields, 0,
	       sizeof(g_bbmcGpsInfo.satelliteValidFields));
	g_bbmcGpsInfo.satellites.count = report->count;
	for (i = 0; i < report->count; i++) {
		source = &report->satellites[i];
		satellite = &g_bbmcGpsInfo.satellites.satellite[i];
		if (source->validFields & 2) {
			g_bbmcGpsInfo.satelliteValidFields[i] |= 0x1000;
			satellite->prn = source->prn;
		}
		if (source->validFields & 0x40) {
			g_bbmcGpsInfo.satelliteValidFields[i] |= 0x2000;
			satellite->elevation = (unsigned int)source->elevation;
		}
		if (source->validFields & 0x80) {
			g_bbmcGpsInfo.satelliteValidFields[i] |= 0x4000;
			satellite->azimuth = source->azimuth;
		}
		if (source->validFields & 0x100) {
			g_bbmcGpsInfo.satelliteValidFields[i] |= 0x8000;
			satellite->snr = source->carrierToNoise / 10;
		}
		if (source->validFields & 8) {
			g_bbmcGpsInfo.satelliteValidFields[i] |= 0x10000;
			satellite->used = source->processingStatus == 5;
		}
	}
	result = SceGpsForDriver_8D5688DA(1, &g_bbmcGpsInfo.satellites);
	sceKernelUnlockFastMutex(&g_bbmcGpsInfo.lock);
	return result;
}

int bbmcNotifyTrackingStopped(void)
{
	BbmcControl *control = bbmcGetControl();

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return 0x80330006;
	}
	control->trackingState = 0;
	bbmcLeaveCS(&control->mutex);
	SceGpsForDriver_F2049C02(0, 0x80000);
	return 0;
}

/* name unknown: a PDS NMEA report: keep its first four sentences */
int bbmcNotifyGpsNmea(QmiPdsNmeaReport *report)
{
	BbmcControl *control = bbmcGetControl();
	int i;

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return 0x80330006;
	}
	bbmcLeaveCS(&control->mutex);
	lockFastMutex(&g_bbmcGpsInfo.lock);
	memset(&g_bbmcGpsInfo.nmea, 0, sizeof(g_bbmcGpsInfo.nmea));
	g_bbmcGpsInfo.nmea.operationMode = report->operationMode;
	g_bbmcGpsInfo.nmea.count = report->count;
	if (g_bbmcGpsInfo.nmea.count != 0) {
		for (i = 0; i < 4; i++) {
			if (i < g_bbmcGpsInfo.nmea.count) {
				g_bbmcGpsInfo.nmea.sentences[i].length =
					report->sentences[i].length;
				memcpy(g_bbmcGpsInfo.nmea.sentences[i].text,
				       report->sentences[i].text, 128);
			} else {
				g_bbmcGpsInfo.nmea.sentences[i].length = 0;
				memset(g_bbmcGpsInfo.nmea.sentences[i].text, 0, 255);
			}
		}
	}
	sceKernelUnlockFastMutex(&g_bbmcGpsInfo.lock);
	SceGpsForDriver_F2049C02(0x10000000, 0x11000000);
	return 0;
}

/* name unknown: a PDS Wi-Fi position request */
int bbmcNotifyGpsWifiRequest(QmiPdsWifiRequest *request)
{
	BbmcControl *control = bbmcGetControl();

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return 0x80330006;
	}
	bbmcLeaveCS(&control->mutex);
	lockFastMutex(&g_bbmcGpsInfo.lock);
	g_bbmcGpsInfo.wifiRequest = (QmiPdsWifiRequest){ 0 };
	g_bbmcGpsInfo.wifiRequest.requestType = request->requestType;
	g_bbmcGpsInfo.wifiRequest.intervalMilliseconds = request->intervalMilliseconds;
	sceKernelUnlockFastMutex(&g_bbmcGpsInfo.lock);
	SceGpsForDriver_F2049C02(0x10000000, 0x13000000);
	return 0;
}

/* name unknown: a PDS extended NMEA report: keep its operation mode */
int bbmcNotifyGpsExtendedNmea(QmiPdsNmeaReport *report)
{
	BbmcControl *control = bbmcGetControl();

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return 0x80330006;
	}
	bbmcLeaveCS(&control->mutex);
	lockFastMutex(&g_bbmcGpsInfo.lock);
	g_bbmcGpsInfo.extendedNmeaMode = report->operationMode;
	sceKernelUnlockFastMutex(&g_bbmcGpsInfo.lock);
	SceGpsForDriver_F2049C02(0x10000000, 0x12000000);
	return 0;
}

int bbmcNotifyTrackingStarted(void)
{
	BbmcControl *control = bbmcGetControl();

	if (control == NULL)
		return 0x80330003;
	bbmcEnterCS(&control->mutex);
	if (control->currentState <= 1) {
		bbmcLeaveCS(&control->mutex);
		return 0x80330006;
	}
	bbmcLeaveCS(&control->mutex);
	SceGpsForDriver_F2049C02(0x10000000, 0x14000000);
	return 0;
}
