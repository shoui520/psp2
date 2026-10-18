#pragma once
#include "gps.h"
#include <threadmgr/fast_mutex_object.h>

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

typedef struct GpsGlobals {
	GpsQueue queue;
	GpsContext context;
	GpsPending *pending;
	GpsQueue *activeQueue;
	GpsContext *active;
	GpsPending request;
	GpsLocation location;
	GpsDriver driver[4];
	GpsSatellites satellites;
	unsigned selected, info[4][2], reserved;
	GpsNavigation navigation;
	GpsSatelliteParse satelliteParse;
	union {
		char number[0x20];			/* a field being converted */
		unsigned numberWord[8];
	};
} GpsGlobals;
extern GpsGlobals s_gps;
void gpsLock(unsigned which);
void gpsUnlock(unsigned which);

/*
 * The data mutex (2) is locked inline in Sony's code; after the active device has been
 * dereferenced, the compiler drops the NULL check of the inlined unlock. The API mutex
 * (1) goes through the out-of-line gpsLock / gpsUnlock, which keep their checks.
 */
static inline __attribute__((always_inline)) void gpsDataLock(void)
{
	if (s_gps.active)
		sceKernelLockMutex(s_gps.active->dataMutex, 1, NULL);
}

static inline __attribute__((always_inline)) void gpsDataUnlock(void)
{
	if (s_gps.active)
		sceKernelUnlockMutex(s_gps.active->dataMutex, 1);
}
unsigned gpsState(void);
int gpsSetState(unsigned state);
int gpsResetParser(unsigned what);
int gpsPublishNavigation(void);
int gpsPublishSatellites(void);
int gpsParseSentence(const char *sentence, unsigned size);
