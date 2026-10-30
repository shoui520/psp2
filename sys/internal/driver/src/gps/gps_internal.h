#pragma once
#include "gps.h"
#include <threadmgr/fast_mutex_object.h>

#define GPS_ERROR(code) ((int)(0x80390000U | (code)))

/* Private names/layouts unknown: offsets confirmed against the originals. */
typedef struct GpsContext {
	int apiMutex, dataMutex, event, thread, callback;
	unsigned state, deviceState, mode;
	int heap, systemHandler;
	unsigned queuedCallbacks, callbackPending, setClock, startResult, stopResult, suspended;
} GpsContext;
typedef struct GpsNotice {
	struct GpsNotice *prev, *next;
	unsigned priority, event, result;
} GpsNotice;
typedef struct GpsQueue {
	GpsNotice *head, *tail;
	SceKernelFastMutex mutex;
} GpsQueue;
typedef struct GpsPending {
	unsigned command;
	void *data;
	unsigned size;
} GpsPending;
/* a registered device: what _sceGpsGetDeviceInfo returns */
typedef struct GpsDeviceInfo {
	unsigned device, flags;
} GpsDeviceInfo;

/* gpsLock / gpsUnlock */
#define GPS_LOCK_API	1
#define GPS_LOCK_DATA	2

/* NMEA sentences the parser takes (gpsParseSentence) */
enum GpsSentence {
	GPS_SENTENCE_NONE,
	GPS_SENTENCE_RMC,			/* "GPRMC": date and time */
	GPS_SENTENCE_GGA,			/* "GPGGA": position, altitude */
	GPS_SENTENCE_GSA,			/* "GPGSA": the fix, satellites used */
	GPS_SENTENCE_GSV,			/* "GPGSV": satellites in view */
	GPS_SENTENCE_VTG,			/* "GPVTG": speed and course */
};

/* the position being parsed from RMC, GGA, GSA and VTG */
typedef struct GpsNavigation {
	union {
		SceDateTime time;		/* RMC */
		uint16_t timePart[6];		/* year, month, day, hour, minute, second */
	};
	int latitudeDegrees;			/* GGA, negative south */
	int longitudeDegrees;			/* negative west */
	double latitudeMinutes;
	double longitudeMinutes;
	float altitude;				/* GGA */
	float geoidSeparation;
	uint8_t fixQuality;
	uint8_t padding0[3];
	float hdop;				/* GGA */
	uint8_t fixType;			/* GSA */
	uint8_t reserved0[35];
	float gsaHdop;				/* GSA, not published */
	float vdop;
	float pdop;
	uint32_t reserved1;
	float speed;				/* VTG */
	float course;
	float magneticCourse;
	uint32_t reserved2;
	union {
		uint8_t sentence[4];		/* received: RMC, GGA, GSA, VTG */
		uint32_t all;
	} received;
} GpsNavigation;

/* the satellites being parsed from GSA and GSV */
typedef struct GpsSatelliteParse {
	uint8_t reserved0;
	uint8_t usedCount;			/* GSA */
	uint8_t used[12];			/* GSA: the PRNs in the fix */
	uint8_t reserved1[38];
	uint8_t count;				/* GSV: satellites in view */
	uint8_t receiving;			/* a GSV sequence is in progress */
	uint8_t messages;			/* GSV messages in the sequence */
	uint8_t message;			/* this message's number */
	uint8_t next;				/* the next satellite record */
	uint8_t reserved2[3];
	GpsSatellite satellite[GPS_SATELLITES_MAX];
	union {
		uint8_t sentence[2];		/* received: GSA, the whole GSV sequence */
		uint16_t all;
	} received;
	uint8_t reserved3[2];
} GpsSatelliteParse;

_Static_assert(sizeof(GpsNavigation) == 0x80, "GpsNavigation");
_Static_assert(offsetof(GpsNavigation, gsaHdop) == 92, "GpsNavigation gsaHdop");
_Static_assert(offsetof(GpsNavigation, speed) == 108, "GpsNavigation speed");
_Static_assert(sizeof(GpsSatelliteParse) == 0x140, "GpsSatelliteParse");
_Static_assert(offsetof(GpsSatelliteParse, count) == 0x34, "GpsSatelliteParse count");

/* gps.c: the context, its mutexes, the state and the callback queue */
GpsContext *gpsContext(void);
void gpsLock(unsigned which);
void gpsUnlock(unsigned which);
unsigned gpsState(void);
int gpsSetState(unsigned state);
int gpsCheckState(unsigned api);
int gpsSuspended(void);
int gpsSignal(unsigned command, unsigned mode);
int gpsNotify(unsigned event, unsigned result);
int gpsSetCallback(SceUID callback);
int gpsStart(unsigned mode);
int gpsStop(void);
int gpsClose(void);
int gpsStagePending(unsigned command, const void *data, unsigned size);
int gpsClearPending(void);
int gpsResumeCallback(void);

/* gps_device.c: the registered devices and the data they report */
int startDevice(unsigned mode);
int stopDevice(unsigned mode);
int applyStopResult(unsigned result);
int gpsLocationUpdated(void);
int gpsPublishNavigation(const GpsNavigation *navigation);
int gpsPublishSatellites(const GpsSatelliteParse *parse);
int gpsGetData(GpsLocation *location, GpsSatellites *satellites);
int getDeviceInfo(GpsDeviceInfo *info);
int resetDevices(unsigned device);
int gpsIsDevice(void);
int deviceIoctl(unsigned command, void *data, unsigned size);
int gpsSelectDevice(unsigned device);
unsigned deviceState(void);
int setDeviceState(unsigned state);

/* nmea.c */
int gpsResetParser(unsigned what);
int gpsParseSentence(const char *sentence, unsigned size);

/* gps_api.c: the syscall bodies */
int openCallback(SceUID callback);
int closeCallback(void);
int startAcquisition(unsigned mode);
int stopAcquisition(void);
int requestIoctl(unsigned command, void *data, unsigned size);
int chooseDevice(unsigned device);
int readState(unsigned *state);
int readLocation(GpsLocation *location, GpsSatellites *satellites);
int readInfo(GpsDeviceInfo *info);
int queryDevice(void);
int resumeNotices(void);
int specialStop(const GpsDriver *driver);
int canSetClock(const GpsDriver *driver);
