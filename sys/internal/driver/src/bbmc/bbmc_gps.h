/* The GPS provider bbmc registers with SceGps: the modem's position, satellites and NMEA. */
#pragma once

#include <stddef.h>
#include <scetypes.h>
#include <gps/gps.h>
#include <threadmgr/fast_mutex_object.h>
#include "qmi_gps_report.h"

/* name unknown: an NMEA sentence as the SceGps ioctl 0xAB001000 returns it */
typedef struct BbmcGpsNmeaSentence {
	SceUInt8 length;
	char text[255];
} BbmcGpsNmeaSentence;

/* name unknown: the last NMEA report, its first four sentences */
typedef struct BbmcGpsNmea {
	SceUInt16 operationMode;
	SceUInt8 count;
	SceUInt8 reserved;
	BbmcGpsNmeaSentence sentences[4];
} BbmcGpsNmea;

/* name unknown: the provider's state ("SceBbmcGpsInfLock" guards it) */
typedef struct BbmcGpsInfo {
	GpsLocation location;
	GpsSatellites satellites;
	struct ThreadmgrFastMutex lock;
	BbmcGpsNmea nmea;
	QmiPdsWifiRequest wifiRequest;
	SceUInt32 extendedNmeaMode;
	/* bits 1 time, 2 hdop, 4 vdop, 8 pdop, 0x40 latitude, 0x80 longitude, 0x100 altitude,
	 * 0x200 speed, 0x400 course, 0x1000 geoid separation; 0x1000.. per satellite */
	SceUInt32 locationValidFields;
	SceUInt32 satelliteValidFields[GPS_SATELLITES_MAX];
	SceUInt32 reserved;
	SceUInt32 trackingMode;
} BbmcGpsInfo;

_Static_assert(sizeof(BbmcGpsNmea) == 1028, "GPS NMEA copy");
_Static_assert(offsetof(BbmcGpsInfo, lock) == 0x140, "GPS info lock");
_Static_assert(offsetof(BbmcGpsInfo, nmea) == 0x180, "GPS info NMEA");
_Static_assert(offsetof(BbmcGpsInfo, wifiRequest) == 0x584, "GPS info Wi-Fi request");
_Static_assert(offsetof(BbmcGpsInfo, satelliteValidFields) == 0x590, "GPS info satellites");
_Static_assert(sizeof(BbmcGpsInfo) == 0x618, "GPS info size");

/* bbmc_gps.c */
int bbmcInitializeGps(void);
int bbmcFinalizeGps(void);
int bbmcNotifyGpsPosition(QmiPdsPosition *position);
int bbmcNotifyGpsSessionStatus(SceUInt8 *status);
int bbmcNotifyGpsSatellites(QmiPdsSatelliteReport *report);
int bbmcNotifyTrackingStopped(void);
int bbmcNotifyGpsNmea(QmiPdsNmeaReport *report);
int bbmcNotifyGpsWifiRequest(QmiPdsWifiRequest *request);
int bbmcNotifyGpsExtendedNmea(QmiPdsNmeaReport *report);
int bbmcNotifyTrackingStarted(void);
