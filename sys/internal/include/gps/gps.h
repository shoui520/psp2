#pragma once

#include <stdint.h>
#include <stddef.h>
#include <scetypes.h>

/* The position a driver reports, and _sceGpsGetData returns. */
typedef struct GpsLocation {
	SceDateTime time;			/* UTC */
	float hdop, vdop, pdop;			/* dilutions of precision */
	uint8_t fixQuality;			/* NMEA GGA */
	uint8_t fixType;			/* NMEA GSA: 1 none, 2 2D, 3 3D */
	uint8_t noClock;			/* nonzero: time must not set the clock */
	uint8_t reserved;
	float latitude, longitude;		/* degrees, negative south and west */
	float altitude;				/* meters */
	float geoidSeparation;
	float speed;				/* km/h */
	float course, magneticCourse;		/* degrees */
} GpsLocation;

/* A satellite in view. */
typedef struct GpsSatellite {
	uint8_t prn;
	uint8_t elevation;			/* degrees */
	uint16_t azimuth;			/* degrees */
	uint8_t snr;				/* dB */
	uint8_t used;				/* nonzero: in the fix */
	uint8_t reserved[2];
} GpsSatellite;

#define GPS_SATELLITES_MAX	32

typedef struct GpsSatellites {
	uint8_t count;
	uint8_t reserved[3];
	GpsSatellite satellite[GPS_SATELLITES_MAX];
} GpsSatellites;

typedef struct GpsDriver {
	unsigned device, flags;
	int (*start)(void);
	int (*stop)(void);
	int (*restart)(void);
	int (*ioctl)(unsigned command, void *data, unsigned size, unsigned state);
} GpsDriver;

typedef struct GpsXtraData {
	uint8_t sequenceNumber;
	uint16_t reserved;
	uint16_t totalLength;
	uint16_t sequenceLength;
	uint8_t data[1500];
} GpsXtraData;

typedef struct GpsAbsoluteTime {
	uint64_t timestampMilliseconds;
	uint32_t timeUncertaintyMilliseconds;
	uint8_t timeBase;
	uint8_t forceAcceptance;
} GpsAbsoluteTime;

typedef struct GpsWifiAccessPoint {
	uint8_t mac[6];
	uint32_t receivedSignalStrength;
	uint16_t beaconChannel;
	uint8_t flags;
} GpsWifiAccessPoint;

typedef struct GpsWifiPosition {
	uint32_t timeCounterMilliseconds;
	int32_t latitude;
	int32_t longitude;
	uint16_t hepeMeters;
	uint8_t accessPointCount;
	uint8_t errorCode;
	uint8_t recordCount;
	uint32_t reserved[3];
	GpsWifiAccessPoint accessPoints[50];
} GpsWifiPosition;

typedef struct GpsResetMasks {
	uint32_t gpsDataMask;
	uint32_t cellDataMask;
} GpsResetMasks;

_Static_assert(sizeof(GpsXtraData) == 1508, "GpsXtraData");
_Static_assert(offsetof(GpsXtraData, data) == 8, "GpsXtraData.data");
_Static_assert(sizeof(GpsAbsoluteTime) == 16, "GpsAbsoluteTime");
_Static_assert(sizeof(GpsWifiAccessPoint) == 16, "GpsWifiAccessPoint");
_Static_assert(sizeof(GpsWifiPosition) == 832, "GpsWifiPosition");
_Static_assert(offsetof(GpsWifiPosition, accessPoints) == 32,
	"GpsWifiPosition.accessPoints");
_Static_assert(sizeof(GpsResetMasks) == 8, "GpsResetMasks");

_Static_assert(sizeof(GpsLocation) == 0x3c, "GpsLocation");
_Static_assert(sizeof(GpsSatellite) == 8, "GpsSatellite");
_Static_assert(sizeof(GpsSatellites) == 0x104, "GpsSatellites");
_Static_assert(sizeof(GpsDriver) == 24, "GpsDriver");

int SceGpsForDriver_884A5867(const GpsDriver *driver);
int SceGpsForDriver_7F001760(unsigned device);
int SceGpsForDriver_54DBA451(unsigned device);
int SceGpsForDriver_BE3EB4E9(unsigned device, unsigned mode,
	const GpsLocation *location);
int SceGpsForDriver_8D5688DA(unsigned device, const GpsSatellites *satellites);
int SceGpsForDriver_F2049C02(unsigned type, unsigned event);
