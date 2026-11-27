/*
 * gps_emulator.cpp: SceLocationGpsEmulator — NMEA-0183 file playback driver
 * (ux0:/data/gpsdata/gpsdata.nma) supporting $GPRMC, $GPGGA, $GPGSA, $GPGSV,
 * and $GPVTG sentences (0x81373368..0x81376B03).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <rtc.h>
#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <gps/gps.h>

#include "location_internal.h"

#define GPS_EMU_DEFAULT_NMEA_PATH		"ux0:/data/gpsdata/gpsdata.nma"
#define GPS_EMU_READ_BUFFER_SIZE		4096
#define GPS_EMU_READ_REFILL_THRESHOLD		3996
#define GPS_EMU_TOKEN_BUF_SIZE			32
#define GPS_EMU_PATH_MAX_LEN			260
#define GPS_EMU_DEFAULT_PERIOD_USEC		1000000ULL
#define GPS_EMU_TIMER_OVERHEAD_USEC		180ULL
#define GPS_EMU_INITIAL_DELAY_USEC		500000ULL
#define GPS_EMU_KNOTS_TO_KMH			1.852

#define GPS_EMU_ERR_UNINIT			(-2143748095)	/* 0x80390001 */
#define GPS_EMU_ERR_NOT_READY			(-2143748094)	/* 0x80390002 */
#define GPS_EMU_ERR_NOT_OPEN			(-2143748092)	/* 0x80390004 */
#define GPS_EMU_ERR_ALREADY_OPEN		(-2143748091)	/* 0x80390005 */
#define GPS_EMU_ERR_NOT_STARTED			(-2143748090)	/* 0x80390006 */
#define GPS_EMU_ERR_ALREADY_STARTED		(-2143748089)	/* 0x80390007 */
#define GPS_EMU_ERR_BUSY			(-2143748085)	/* 0x8039000B */
#define GPS_EMU_ERR_CHECKSUM			(-2143748079)	/* 0x80390011 */
#define GPS_EMU_ERR_SYNTAX			(-2143748078)	/* 0x80390012 */
#define GPS_EMU_ERR_INVALID_ARG			(-2143748064)	/* 0x80390020 */

#define GPS_EMU_LOC_ERR_NO_MEMORY		(-2146430331)	/* 0x80101285 */
#define GPS_EMU_LOC_ERR_NO_FILE			(-2146430240)	/* 0x801012E0 */

#define GPS_EMU_EVF_START			0x00100000u
#define GPS_EMU_EVF_STOP			0x00200000u
#define GPS_EMU_EVF_RESUME			0x00400000u
#define GPS_EMU_EVF_DATA_READY			0x01000000u
#define GPS_EMU_EVF_SAT_READY			0x02000000u
#define GPS_EMU_EVF_EXIT			0x80000000u
#define GPS_EMU_EVF_MAIN_MASK \
	(GPS_EMU_EVF_START | GPS_EMU_EVF_STOP | GPS_EMU_EVF_DATA_READY | \
	 GPS_EMU_EVF_SAT_READY | GPS_EMU_EVF_EXIT)

#define GPS_EMU_CB_NOTIFY_START			0x00010000
#define GPS_EMU_CB_NOTIFY_STOP			0x00020000
#define GPS_EMU_CB_NOTIFY_DATA			0x00030000
#define GPS_EMU_CB_NOTIFY_SAT			0x00040000

#define GPS_EMU_POWER_SUSPEND_BIT		0x00010000
#define GPS_EMU_POWER_RESUME_BIT		0x00040000

enum GpsEmuState {
	GPS_EMU_STATE_UNINIT		= 0,
	GPS_EMU_STATE_INITIALIZING	= 1,
	GPS_EMU_STATE_IDLE		= 2,
	GPS_EMU_STATE_OPENED		= 3,
	GPS_EMU_STATE_STARTING		= 4,
	GPS_EMU_STATE_RUNNING		= 5,
	GPS_EMU_STATE_DATA_READY	= 6,
	GPS_EMU_STATE_DATA_CONSUMED	= 7,
	GPS_EMU_STATE_STOPPING		= 8,
	GPS_EMU_STATE_COUNT		= 9
};

enum GpsEmuOperation {
	GPS_EMU_OP_OPEN			= 0,
	GPS_EMU_OP_START		= 1,
	GPS_EMU_OP_STOP			= 2,
	GPS_EMU_OP_CLOSE		= 3,
	GPS_EMU_OP_GET_DATA		= 4,
	GPS_EMU_OP_GET_DEVICE_INFO	= 5,
	GPS_EMU_OP_COUNT		= 6
};

enum GpsEmuMutexKind {
	GPS_EMU_MUTEX_API		= 1,
	GPS_EMU_MUTEX_DATA		= 2
};

enum GpsNmeaSentenceId {
	GPS_NMEA_NONE			= 0,
	GPS_NMEA_GPRMC			= 1,
	GPS_NMEA_GPGGA			= 2,
	GPS_NMEA_GPGSA			= 3,
	GPS_NMEA_GPGSV			= 4,
	GPS_NMEA_GPVTG			= 5
};

struct GpsEmuDeviceInfo {
	SceUInt32 deviceKind;
	SceUInt32 version;
};

struct GpsNmeaSentenceEntry {
	int sentenceId;
	const char *talkerTag;
};

struct GpsEmuGgaTime {
	SceUInt16 hour;
	SceUInt16 minute;
	SceUInt16 second;
	SceUInt16 reserved;
	SceUInt32 microsecond;
};

struct GpsEmuNmeaRecord {
	float rmcSpeedKnots;
	float rmcCourseDeg;
	SceDateTime rmcDateTime;
	GpsEmuGgaTime ggaTime;
	SceInt32 latDegrees;
	double latMinutes;
	SceInt32 lonDegrees;
	double lonMinutes;
	float altitude;
	float geoidSeparation;
	SceUInt8 fixQuality;
	float ggaHdop;
	SceUInt8 fixMode;
	SceUInt8 activeSatCount;
	SceUInt8 activeSatPrns[GPS_SATELLITES_MAX];
	float gsaHdop;
	float gsaVdop;
	float gsaPdop;
	SceUInt8 totalSatsInView;
	SceUInt8 gsvReceiving;
	SceUInt8 gsvTotalMessages;
	SceUInt8 gsvCurrentMessage;
	SceUInt8 gsvCollectedCount;
	GpsSatellite gsvSatellites[GPS_SATELLITES_MAX];
	float vtgSpeedKmh;
	float vtgTrueTrack;
	float vtgMagneticTrack;
	SceUInt8 hasRmc;
	SceUInt8 hasGga;
	SceUInt8 hasGsa;
	SceUInt8 hasGsv;
	SceUInt8 hasVtg;
	SceUInt8 parseFailed;
};

struct GpsEmuContext {
	int state;
	SceUID suspendEventFlag;
	SceUID powerCallbackId;
	SceUID mainEventFlag;
	SceUID mainThreadId;
	SceUID apiMutexId;
	SceUID dataMutexId;
	SceUID parseThreadId;
	SceUID notifyTimerId;
	SceUID clientCallbackId;
	int startMode;
	SceUID nmeaFd;
	SceOff nmeaFileSize;
	char *readBuffer;
};

extern "C" {
void *ScePafStdc_3CF19248(SceSize size, void *heap);
int sce_paf_atoi(const char *str);
double sce_paf_atof(const char *str);
long sce_paf_strtol(const char *str, char **endPtr, int base);
int sceIoClose(SceUID fd);
SceSSize sceIoRead(SceUID fd, void *buf, SceSize size);
int sceKernelCreateCallback(const char *name, SceUInt32 attr,
				   SceKernelCallbackFunction func, void *arg);
int sceKernelDeleteCallback(SceUID cbId);
int sceKernelNotifyCallback(SceUID cbId, int arg);
int sceKernelDeleteEventFlag(SceUID evfId);
int sceKernelSetEventFlag(SceUID evfId, SceUInt32 bits);
int sceKernelDeleteMutex(SceUID mutexId);
int sceKernelUnlockMutex(SceUID mutexId, int count);
int sceKernelDeleteThread(SceUID threadId);
int sceKernelStartTimer(SceUID timerId);
int sceKernelStopTimer(SceUID timerId);
int sceKernelDeleteTimer(SceUID timerId);
int sceKernelWaitEvent(SceUID eventId, SceUInt32 bitPattern, SceUInt32 *outBits,
		       SceUInt64 *outUserData, SceUInt32 *timeout);
int scePowerRegisterCallback(SceUID cbId);
int scePowerUnregisterCallback(SceUID cbId);
}

/* (0x8151FF2C) Operation x State transition error matrix */
static const int s_gpsEmuStateMatrix[GPS_EMU_OP_COUNT][GPS_EMU_STATE_COUNT] = {
	{ GPS_EMU_ERR_UNINIT, GPS_EMU_ERR_NOT_READY, 0, GPS_EMU_ERR_ALREADY_OPEN,
	  GPS_EMU_ERR_BUSY, GPS_EMU_ERR_ALREADY_OPEN, GPS_EMU_ERR_ALREADY_OPEN,
	  GPS_EMU_ERR_ALREADY_OPEN, GPS_EMU_ERR_BUSY },
	{ GPS_EMU_ERR_UNINIT, GPS_EMU_ERR_NOT_READY, GPS_EMU_ERR_NOT_OPEN, 0,
	  GPS_EMU_ERR_BUSY, GPS_EMU_ERR_ALREADY_STARTED, GPS_EMU_ERR_ALREADY_STARTED,
	  GPS_EMU_ERR_ALREADY_STARTED, GPS_EMU_ERR_BUSY },
	{ GPS_EMU_ERR_UNINIT, GPS_EMU_ERR_NOT_READY, GPS_EMU_ERR_NOT_OPEN,
	  GPS_EMU_ERR_NOT_STARTED, GPS_EMU_ERR_BUSY, 0, 0, 0, GPS_EMU_ERR_BUSY },
	{ GPS_EMU_ERR_UNINIT, 0, 0, 0, GPS_EMU_ERR_BUSY, GPS_EMU_ERR_ALREADY_STARTED,
	  GPS_EMU_ERR_ALREADY_STARTED, GPS_EMU_ERR_ALREADY_STARTED, GPS_EMU_ERR_BUSY },
	{ GPS_EMU_ERR_UNINIT, GPS_EMU_ERR_NOT_READY, GPS_EMU_ERR_NOT_OPEN,
	  GPS_EMU_ERR_NOT_STARTED, GPS_EMU_ERR_BUSY, 0, 0, 0, GPS_EMU_ERR_BUSY },
	{ GPS_EMU_ERR_UNINIT, GPS_EMU_ERR_NOT_READY, 0, 0, GPS_EMU_ERR_BUSY,
	  0, 0, 0, GPS_EMU_ERR_BUSY }
};

/* (0x8152011C) Supported NMEA-0183 sentence headers */
static const GpsNmeaSentenceEntry s_nmeaSentenceTable[5] = {
	{ GPS_NMEA_GPRMC, "GPRMC" },
	{ GPS_NMEA_GPGGA, "GPGGA" },
	{ GPS_NMEA_GPGSA, "GPGSA" },
	{ GPS_NMEA_GPGSV, "GPGSV" },
	{ GPS_NMEA_GPVTG, "GPVTG" }
};

/* Emulated GPS state and parsed NMEA buffers (0x815489A8..0x815C7537) */
static GpsEmuContext *s_gpsEmuCtx = NULL;
static SceUInt8 s_gpsEmuUseCurrentClock = 0;
static SceOff s_gpsEmuFileOffset = 0;
static SceUInt8 s_gpsEmuSuspended = 0;
static SceUInt8 s_gpsEmuSingleEpochMode = 0;
static SceUInt8 s_gpsEmuResumedFromSuspend = 0;
static SceUInt8 s_gpsEmuNeedsBufferFill = 0;
static int s_gpsEmuBufferCursor = 0;
static SceUInt8 s_gpsEmuSmallFileLoaded = 0;
static SceUInt8 s_gpsEmuParsedValidSentence = 0;

static GpsLocation s_gpsEmuLocation;
static GpsSatellites s_gpsEmuSatellites;
static SceDateTime s_gpsEmuLastPublishedTime;
static GpsEmuNmeaRecord s_gpsEmuCurrentRecord;
static GpsEmuNmeaRecord s_gpsEmuNextRecord;
static SceKernelSysClock s_gpsEmuTimerInterval;
static GpsEmuContext s_gpsEmuStorage;
static char s_gpsEmuTokenBuf[GPS_EMU_TOKEN_BUF_SIZE];

/* Forward declarations */
static int gpsEmuStartInternal(const char *nmeaPath);
static int gpsEmuStopInternal(void);
static int gpsEmuGetStateLocked(void);
static int gpsEmuDoOpen(SceUID callbackId);
static int gpsEmuDoStart(int mode);
static int gpsEmuDoStop(void);
static int gpsEmuDoClose(void);
static int gpsEmuDoGetData(GpsLocation *outLocation, GpsSatellites *outSatellites);
static int gpsEmuCheckState(int op);
static int gpsEmuParseNmeaLine(const unsigned char *line, unsigned int *outConsumed,
			       GpsEmuNmeaRecord *record);

/* (0x813735F0) Unlocks either the API mutex or the data mutex */
static int gpsEmuUnlockMutex(int mutexKind)
{
	if (s_gpsEmuCtx != NULL) {
		if (mutexKind == GPS_EMU_MUTEX_API)
			return sceKernelUnlockMutex(s_gpsEmuCtx->apiMutexId, 1);
		if (mutexKind == GPS_EMU_MUTEX_DATA)
			return sceKernelUnlockMutex(s_gpsEmuCtx->dataMutexId, 1);
	}
	return mutexKind;
}

/* (0x8137361A) Locks either the API mutex or the data mutex */
static bool gpsEmuLockMutex(int mutexKind)
{
	if (s_gpsEmuCtx != NULL) {
		if (mutexKind == GPS_EMU_MUTEX_API)
			return sceKernelLockMutex(s_gpsEmuCtx->apiMutexId, 1, NULL) == 0;
		if (mutexKind == GPS_EMU_MUTEX_DATA)
			return sceKernelLockMutex(s_gpsEmuCtx->dataMutexId, 1, NULL) == 0;
	}
	return false;
}

static void gpsEmuSetStateLocked(int nextState)
{
	if (s_gpsEmuCtx == NULL)
		return;
	bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_DATA);
	s_gpsEmuCtx->state = nextState;
	if (locked)
		gpsEmuUnlockMutex(GPS_EMU_MUTEX_DATA);
}

/* (0x81373ECC) Returns the current emulator state under the data mutex */
static int gpsEmuGetStateLocked(void)
{
	if (s_gpsEmuCtx == NULL)
		return GPS_EMU_STATE_UNINIT;
	bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_DATA);
	int curState = s_gpsEmuCtx->state;
	if (locked)
		gpsEmuUnlockMutex(GPS_EMU_MUTEX_DATA);
	return curState;
}

/* (0x81374844) Validates an operation against the current emulator state */
static int gpsEmuCheckState(int op)
{
	int curState = gpsEmuGetStateLocked();
	return s_gpsEmuStateMatrix[op][curState];
}

/* (0x8137489C) Release-build debug trace stub */
static void gpsEmuTraceLocation(const GpsLocation *location, const GpsSatellites *satellites)
{
	(void)location;
	(void)satellites;
}

/* (0x81373F10) Copies hour/minute/second/microsecond into a SceDateTime */
static void gpsEmuCopyTimeToDateTime(SceDateTime *dst, const GpsEmuGgaTime *src)
{
	dst->hour = src->hour;
	dst->minute = src->minute;
	dst->second = src->second;
	dst->microsecond = src->microsecond;
}

static char *gpsEmuCopyToken(const unsigned char *tokenStart, SceSize tokenLen)
{
	if (tokenLen >= GPS_EMU_TOKEN_BUF_SIZE)
		return NULL;
	sce_paf_memset(s_gpsEmuTokenBuf, 0, sizeof(s_gpsEmuTokenBuf));
	sce_paf_memcpy(s_gpsEmuTokenBuf, tokenStart, tokenLen);
	return s_gpsEmuTokenBuf;
}

/* (0x81373368) Public entry: initialize GPS emulator from NMEA file */
int gpsEmuInit(const char *path)
{
	return gpsEmuStartInternal(path);
}

/* (0x81373370) Public entry: terminate GPS emulator */
int gpsEmuTerm(void)
{
	return gpsEmuStopInternal();
}

/* (0x81373378) Public entry: open emulated GPS channel with client callback */
int gpsEmuOpen(SceUID callbackId)
{
	int ret = gpsEmuCheckState(GPS_EMU_OP_OPEN);
	if (ret == 0) {
		bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_API);
		ret = gpsEmuDoOpen(callbackId);
		if (locked)
			gpsEmuUnlockMutex(GPS_EMU_MUTEX_API);
	}
	return ret;
}

/* (0x813733A4) Public entry: start emulated GPS playback */
int gpsEmuStart(int mode)
{
	int ret = gpsEmuCheckState(GPS_EMU_OP_START);
	if (ret == 0) {
		bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_API);
		ret = gpsEmuDoStart(mode);
		if (locked)
			gpsEmuUnlockMutex(GPS_EMU_MUTEX_API);
	}
	return ret;
}

/* (0x813733D0) Public entry: stop emulated GPS playback */
int gpsEmuStop(void)
{
	int ret = gpsEmuCheckState(GPS_EMU_OP_STOP);
	if (ret == 0) {
		bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_API);
		ret = gpsEmuDoStop();
		if (locked)
			gpsEmuUnlockMutex(GPS_EMU_MUTEX_API);
	}
	return ret;
}

/* (0x813733F8) Public entry: close emulated GPS channel */
int gpsEmuClose(void)
{
	int ret = gpsEmuCheckState(GPS_EMU_OP_CLOSE);
	if (ret == 0) {
		bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_API);
		ret = gpsEmuDoClose();
		if (locked)
			gpsEmuUnlockMutex(GPS_EMU_MUTEX_API);
	}
	return ret;
}

/* (0x81373420) Public entry: query current emulator state */
int gpsEmuGetSatellites(int *outStatus)
{
	if (outStatus == NULL)
		return GPS_EMU_ERR_INVALID_ARG;
	bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_API);
	*outStatus = gpsEmuGetStateLocked();
	if (locked)
		gpsEmuUnlockMutex(GPS_EMU_MUTEX_API);
	return 0;
}

/* (0x8137344E) Public entry: copy latest emulated position and satellite data */
int gpsEmuGetData(GpsLocation *location, GpsSatellites *satellites)
{
	if (location == NULL || satellites == NULL)
		return GPS_EMU_ERR_INVALID_ARG;

	int ret = gpsEmuCheckState(GPS_EMU_OP_GET_DATA);
	if (ret == 0) {
		bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_API);
		ret = gpsEmuDoGetData(location, satellites);
		if (locked)
			gpsEmuUnlockMutex(GPS_EMU_MUTEX_API);
		if (ret == 0)
			gpsEmuTraceLocation(location, satellites);
	}
	return ret;
}

/* (0x813735CC) Public entry: query emulated device descriptor */
int gpsEmuSetParam(GpsEmuDeviceInfo *outInfo)
{
	if (outInfo == NULL)
		return GPS_EMU_ERR_INVALID_ARG;
	int ret = gpsEmuCheckState(GPS_EMU_OP_GET_DEVICE_INFO);
	if (ret == 0) {
		outInfo->deviceKind = 4;
		outInfo->version = 1;
	}
	return ret;
}

/* (0x81374622) Internal open handler */
static int gpsEmuDoOpen(SceUID callbackId)
{
	if (s_gpsEmuCtx == NULL)
		return GPS_EMU_ERR_UNINIT;
	s_gpsEmuCtx->clientCallbackId = callbackId;
	gpsEmuSetStateLocked(GPS_EMU_STATE_OPENED);
	return 0;
}

/* (0x81374670) Internal start handler */
static int gpsEmuDoStart(int mode)
{
	if (s_gpsEmuCtx == NULL)
		return GPS_EMU_ERR_UNINIT;
	s_gpsEmuCtx->startMode = mode;
	gpsEmuSetStateLocked(GPS_EMU_STATE_STARTING);
	sceKernelSetEventFlag(s_gpsEmuCtx->mainEventFlag, GPS_EMU_EVF_START);
	return 0;
}

/* (0x813746D4) Internal stop handler */
static int gpsEmuDoStop(void)
{
	if (s_gpsEmuCtx == NULL)
		return GPS_EMU_ERR_UNINIT;
	gpsEmuSetStateLocked(GPS_EMU_STATE_STOPPING);
	sceKernelSetEventFlag(s_gpsEmuCtx->mainEventFlag, GPS_EMU_EVF_STOP);
	return 0;
}

/* (0x81374736) Internal close handler */
static int gpsEmuDoClose(void)
{
	if (s_gpsEmuCtx == NULL)
		return GPS_EMU_ERR_UNINIT;
	s_gpsEmuCtx->clientCallbackId = -1;
	gpsEmuSetStateLocked(GPS_EMU_STATE_IDLE);
	return 0;
}

/* (0x81374788) Internal get-data handler */
static int gpsEmuDoGetData(GpsLocation *outLocation, GpsSatellites *outSatellites)
{
	if (outLocation == NULL || outSatellites == NULL)
		return GPS_EMU_ERR_INVALID_ARG;
	if (s_gpsEmuCtx == NULL)
		return GPS_EMU_ERR_UNINIT;

	bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_DATA);
	sce_paf_memcpy(outLocation, &s_gpsEmuLocation, sizeof(s_gpsEmuLocation));
	sce_paf_memcpy(outSatellites, &s_gpsEmuSatellites, sizeof(s_gpsEmuSatellites));
	if (locked)
		gpsEmuUnlockMutex(GPS_EMU_MUTEX_DATA);

	gpsEmuSetStateLocked(GPS_EMU_STATE_DATA_CONSUMED);
	return 0;
}

/* (0x813748CC) Power callback pausing/resuming NMEA playback across suspend */
static int gpsEmuPowerCallback(int notifyId, int notifyCount, int powerFlags, void *userArg)
{
	(void)notifyId;
	(void)notifyCount;
	(void)userArg;

	if ((powerFlags & GPS_EMU_POWER_SUSPEND_BIT) != 0) {
		s_gpsEmuSuspended = 1;
	} else if ((powerFlags & GPS_EMU_POWER_RESUME_BIT) != 0 && s_gpsEmuSuspended == 1) {
		s_gpsEmuSuspended = 0;
		if (s_gpsEmuCtx != NULL && s_gpsEmuCtx->suspendEventFlag >= 0)
			sceKernelSetEventFlag(s_gpsEmuCtx->suspendEventFlag, GPS_EMU_EVF_RESUME);
	}
	return 0;
}

static bool gpsEmuIsActiveState(void)
{
	int curState = gpsEmuGetStateLocked();
	return curState == GPS_EMU_STATE_RUNNING ||
	       curState == GPS_EMU_STATE_DATA_CONSUMED ||
	       curState == GPS_EMU_STATE_DATA_READY;
}

/* (0x81374916) SceLocationGpsEmulatorMain thread: dispatches client callbacks */
static int gpsEmuMainThread(SceSize args, void *argp)
{
	(void)args;
	if (argp == NULL)
		return GPS_EMU_ERR_INVALID_ARG;

	GpsEmuContext *ctx = *static_cast<GpsEmuContext **>(argp);
	for (;;) {
		SceUInt32 outBits = 0;
		int ret = sceKernelWaitEventFlag(ctx->mainEventFlag, GPS_EMU_EVF_MAIN_MASK,
						 5u, &outBits, NULL);
		if (ret < 0)
			return ret;

		if ((outBits & GPS_EMU_EVF_EXIT) != 0) {
			gpsEmuSetStateLocked(GPS_EMU_STATE_UNINIT);
			return 0;
		}

		if ((outBits & GPS_EMU_EVF_START) != 0) {
			if (ctx->clientCallbackId > 0) {
				sceKernelNotifyCallback(ctx->clientCallbackId, GPS_EMU_CB_NOTIFY_START);
				gpsEmuSetStateLocked(GPS_EMU_STATE_RUNNING);
			}
		} else if ((outBits & GPS_EMU_EVF_STOP) != 0) {
			if (ctx->clientCallbackId > 0) {
				gpsEmuSetStateLocked(GPS_EMU_STATE_OPENED);
				sceKernelNotifyCallback(ctx->clientCallbackId, GPS_EMU_CB_NOTIFY_STOP);
			}
		} else if ((outBits & GPS_EMU_EVF_DATA_READY) != 0) {
			if (ctx->clientCallbackId > 0 && gpsEmuIsActiveState()) {
				gpsEmuSetStateLocked(GPS_EMU_STATE_DATA_READY);
				sceKernelNotifyCallback(ctx->clientCallbackId, GPS_EMU_CB_NOTIFY_DATA);
			}
		} else if ((outBits & GPS_EMU_EVF_SAT_READY) != 0) {
			if (ctx->clientCallbackId > 0 && gpsEmuIsActiveState())
				sceKernelNotifyCallback(ctx->clientCallbackId, GPS_EMU_CB_NOTIFY_SAT);
		}
	}
}

/* (0x81373F54) Reads sentences from the NMEA file until one complete epoch is assembled */
static int gpsEmuReadNextNmeaEpoch(GpsEmuNmeaRecord *record, SceUInt8 *outHasMoreEpochs)
{
	SceOff startOffset = s_gpsEmuFileOffset;
	*outHasMoreEpochs = 1;

	int status = 0;
	for (;;) {
		if (s_gpsEmuNeedsBufferFill != 0) {
			SceSize readSize = GPS_EMU_READ_BUFFER_SIZE;
			s_gpsEmuNeedsBufferFill = 0;
			bool skipRead = false;
			if (s_gpsEmuCtx->nmeaFileSize <= GPS_EMU_READ_BUFFER_SIZE) {
				if (s_gpsEmuSmallFileLoaded != 0) {
					skipRead = true;
				} else {
					s_gpsEmuSmallFileLoaded = 1;
					readSize = static_cast<SceSize>(s_gpsEmuCtx->nmeaFileSize);
				}
			}
			if (!skipRead) {
				sce_paf_memset(s_gpsEmuCtx->readBuffer, 0, GPS_EMU_READ_BUFFER_SIZE);
				SceSSize readBytes = sceIoRead(
					s_gpsEmuCtx->nmeaFd, s_gpsEmuCtx->readBuffer, readSize);
				if (readBytes < 0) {
					status = static_cast<int>(readBytes);
					break;
				}
			}
		}

		unsigned int consumed = 0;
		const unsigned char *cursor =
			reinterpret_cast<const unsigned char *>(s_gpsEmuCtx->readBuffer) +
			s_gpsEmuBufferCursor;
		status = gpsEmuParseNmeaLine(cursor, &consumed, record);
		s_gpsEmuBufferCursor += static_cast<int>(consumed);

		if (status == 0) {
			s_gpsEmuParsedValidSentence = 1;
			if (s_gpsEmuFileOffset + s_gpsEmuBufferCursor >= s_gpsEmuCtx->nmeaFileSize) {
				s_gpsEmuBufferCursor = 0;
				s_gpsEmuFileOffset = 0;
			}
		} else if (status == 1) {
			bool hasGga = (record->hasGga == 1);
			if (hasGga) {
				s_gpsEmuParsedValidSentence = 1;
			} else {
				sce_paf_memset(record, 0, sizeof(*record));
			}

			if (s_gpsEmuFileOffset + s_gpsEmuBufferCursor >= s_gpsEmuCtx->nmeaFileSize) {
				if (hasGga)
					status = 0;
				else
					sce_paf_memset(record, 0, sizeof(*record));

				s_gpsEmuFileOffset = 0;
				s_gpsEmuBufferCursor = 0;
				*outHasMoreEpochs = 0;
				if (s_gpsEmuParsedValidSentence == 0) {
					status = GPS_EMU_ERR_SYNTAX;
					break;
				}
			}
		} else {
			if (s_gpsEmuFileOffset + s_gpsEmuBufferCursor >= s_gpsEmuCtx->nmeaFileSize &&
			    s_gpsEmuParsedValidSentence == 0) {
				sce_paf_memset(record, 0, sizeof(*record));
				s_gpsEmuBufferCursor = 0;
				s_gpsEmuFileOffset = 0;
				break;
			}
			status = 1;
		}

		if (status >= 0) {
			if (s_gpsEmuBufferCursor > GPS_EMU_READ_REFILL_THRESHOLD ||
			    s_gpsEmuBufferCursor == 0) {
				SceOff seekPos = sceIoLseek(
					s_gpsEmuCtx->nmeaFd,
					s_gpsEmuFileOffset + s_gpsEmuBufferCursor,
					SCE_SEEK_SET);
				s_gpsEmuFileOffset = seekPos;
				if (seekPos < 0) {
					s_gpsEmuFileOffset = 0;
					status = static_cast<int>(seekPos);
					break;
				}
				s_gpsEmuBufferCursor = 0;
				s_gpsEmuNeedsBufferFill = 1;
			}
			if (status == 0)
				break;
		}
	}

	if (startOffset == 0)
		*outHasMoreEpochs = 0;
	return status;
}

/* (0x813742BA) Converts a parsed NMEA record into GpsLocation and GpsSatellites */
static int gpsEmuPublishEpoch(const GpsEmuNmeaRecord *record, SceUInt64 elapsedUsec)
{
	if (record == NULL)
		return 0;

	bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_DATA);
	sce_paf_memset(&s_gpsEmuLocation, 0, sizeof(s_gpsEmuLocation));
	sce_paf_memset(&s_gpsEmuSatellites, 0, sizeof(s_gpsEmuSatellites));

	if (s_gpsEmuUseCurrentClock == 1 || s_gpsEmuResumedFromSuspend == 1) {
		s_gpsEmuUseCurrentClock = 0;
		s_gpsEmuResumedFromSuspend = 0;
		sceRtcGetCurrentClock(&s_gpsEmuLocation.time, 0);
	} else {
		SceRtcTick tick;
		if (sceRtcGetTick(&s_gpsEmuLastPublishedTime, &tick) == 0) {
			tick.tick += elapsedUsec;
			sceRtcSetTick(&s_gpsEmuLocation.time, &tick);
		}
	}
	s_gpsEmuLastPublishedTime = s_gpsEmuLocation.time;

	s_gpsEmuLocation.hdop = record->ggaHdop;
	s_gpsEmuLocation.vdop = record->gsaVdop;
	s_gpsEmuLocation.pdop = record->gsaPdop;
	s_gpsEmuLocation.fixQuality = (record->fixQuality != 0) ? record->fixQuality : 1;

	SceUInt8 fixType = 0;
	if (record->hasGsa != 0 && record->fixMode != 0)
		fixType = static_cast<SceUInt8>(record->fixMode - 1);
	s_gpsEmuLocation.fixType = fixType;
	s_gpsEmuLocation.noClock = 0;

	s_gpsEmuLocation.latitude =
		static_cast<float>(record->latDegrees) + static_cast<float>(record->latMinutes / 60.0);
	s_gpsEmuLocation.longitude =
		static_cast<float>(record->lonDegrees) + static_cast<float>(record->lonMinutes / 60.0);
	s_gpsEmuLocation.altitude = record->altitude;

	if (record->hasVtg == 1) {
		s_gpsEmuLocation.speed = record->vtgSpeedKmh;
		s_gpsEmuLocation.course = record->vtgTrueTrack;
		s_gpsEmuLocation.magneticCourse = record->vtgMagneticTrack;
	} else if (record->hasRmc == 1) {
		s_gpsEmuLocation.speed = static_cast<float>(record->rmcSpeedKnots * GPS_EMU_KNOTS_TO_KMH);
		s_gpsEmuLocation.course = record->rmcCourseDeg;
	}

	if (record->totalSatsInView > GPS_SATELLITES_MAX) {
		s_gpsEmuSatellites.count = 0;
	} else if (record->hasGsv == 1) {
		s_gpsEmuSatellites.count = record->totalSatsInView;
		for (int satIndex = 0; satIndex < GPS_SATELLITES_MAX; ++satIndex) {
			if (satIndex >= record->totalSatsInView) {
				sce_paf_memset(&s_gpsEmuSatellites.satellite[satIndex], 0, sizeof(GpsSatellite));
			} else {
				const GpsSatellite &srcSat = record->gsvSatellites[satIndex];
				GpsSatellite &dstSat = s_gpsEmuSatellites.satellite[satIndex];
				dstSat.prn = srcSat.prn;
				dstSat.elevation = srcSat.elevation;
				dstSat.azimuth = srcSat.azimuth;
				dstSat.snr = srcSat.snr;
				dstSat.used = 0;
				for (int activeIndex = 0; activeIndex < record->activeSatCount; ++activeIndex) {
					if (record->activeSatPrns[activeIndex] == srcSat.prn) {
						dstSat.used = 1;
						break;
					}
				}
			}
		}
	} else {
		/* Synthetic default constellation when the NMEA file contains no $GPGSV */
		s_gpsEmuSatellites.count = 3;
		s_gpsEmuSatellites.satellite[0].prn = 24;
		s_gpsEmuSatellites.satellite[0].elevation = 62;
		s_gpsEmuSatellites.satellite[0].azimuth = 315;
		s_gpsEmuSatellites.satellite[0].snr = 29;
		s_gpsEmuSatellites.satellite[0].used = 1;

		s_gpsEmuSatellites.satellite[1].prn = 20;
		s_gpsEmuSatellites.satellite[1].elevation = 61;
		s_gpsEmuSatellites.satellite[1].azimuth = 46;
		s_gpsEmuSatellites.satellite[1].snr = 29;
		s_gpsEmuSatellites.satellite[1].used = 1;

		s_gpsEmuSatellites.satellite[2].prn = 17;
		s_gpsEmuSatellites.satellite[2].elevation = 60;
		s_gpsEmuSatellites.satellite[2].azimuth = 323;
		s_gpsEmuSatellites.satellite[2].snr = 33;
		s_gpsEmuSatellites.satellite[2].used = 1;

		for (int satIndex = 3; satIndex < GPS_SATELLITES_MAX; ++satIndex)
			sce_paf_memset(&s_gpsEmuSatellites.satellite[satIndex], 0, sizeof(GpsSatellite));
	}

	if (locked)
		gpsEmuUnlockMutex(GPS_EMU_MUTEX_DATA);
	return 0;
}

static bool gpsEmuComputeEpochDelta(const GpsEmuNmeaRecord *prevRec,
				    const GpsEmuNmeaRecord *nextRec,
				    SceUInt64 *outDeltaUsec)
{
	SceDateTime firstDate;
	SceDateTime secondDate;

	if (prevRec->hasRmc == 1) {
		if (nextRec->hasRmc == 1) {
			firstDate = prevRec->rmcDateTime;
			secondDate = nextRec->rmcDateTime;
		} else {
			firstDate = prevRec->rmcDateTime;
			secondDate = prevRec->rmcDateTime;
			gpsEmuCopyTimeToDateTime(&secondDate, &nextRec->ggaTime);
		}
	} else if (prevRec->hasGga == 1) {
		if (nextRec->hasRmc == 1) {
			firstDate = nextRec->rmcDateTime;
			gpsEmuCopyTimeToDateTime(&firstDate, &prevRec->ggaTime);
			secondDate = nextRec->rmcDateTime;
		} else {
			sceRtcGetCurrentClock(&firstDate, 0);
			secondDate = firstDate;
			gpsEmuCopyTimeToDateTime(&firstDate, &prevRec->ggaTime);
			gpsEmuCopyTimeToDateTime(&secondDate, &nextRec->ggaTime);
		}
	} else {
		return false;
	}

	SceRtcTick firstTick;
	SceRtcTick secondTick;
	int firstRet = sceRtcGetTick(&firstDate, &firstTick);
	int secondRet = sceRtcGetTick(&secondDate, &secondTick);
	if ((firstRet | secondRet) != 0)
		return true;
	if (firstTick.tick > secondTick.tick)
		return false;
	*outDeltaUsec = secondTick.tick - firstTick.tick;
	return true;
}

/* (0x81374C10) SceLocationGpsEmulatorParse thread: paces NMEA epochs on a kernel timer */
static int gpsEmuParseThread(SceSize args, void *argp)
{
	(void)args;
	if (argp == NULL)
		return GPS_EMU_ERR_INVALID_ARG;

	GpsEmuContext *ctx = *static_cast<GpsEmuContext **>(argp);
	s_gpsEmuUseCurrentClock = 1;
	sce_paf_memset(&s_gpsEmuCurrentRecord, 0, sizeof(s_gpsEmuCurrentRecord));
	sce_paf_memset(&s_gpsEmuNextRecord, 0, sizeof(s_gpsEmuNextRecord));
	s_gpsEmuNeedsBufferFill = 1;
	s_gpsEmuParsedValidSentence = 0;

	SceUInt64 deltaUsec = 0;
	SceUInt8 hasMoreEpochs = 0;
	bool deltaValid = false;
	do {
		int curRet = gpsEmuReadNextNmeaEpoch(&s_gpsEmuCurrentRecord, &hasMoreEpochs);
		if (curRet != 0) {
			gpsEmuSetStateLocked(GPS_EMU_STATE_UNINIT);
			return curRet;
		}
		int nextRet = gpsEmuReadNextNmeaEpoch(&s_gpsEmuNextRecord, &hasMoreEpochs);
		if (nextRet != 0) {
			gpsEmuSetStateLocked(GPS_EMU_STATE_UNINIT);
			return nextRet;
		}
		deltaValid = gpsEmuComputeEpochDelta(&s_gpsEmuCurrentRecord, &s_gpsEmuNextRecord, &deltaUsec);
	} while (!deltaValid);

	if (deltaUsec == 0 && hasMoreEpochs == 0) {
		s_gpsEmuSingleEpochMode = 1;
		deltaUsec = GPS_EMU_DEFAULT_PERIOD_USEC;
	}

	s_gpsEmuTimerInterval.quad = deltaUsec;
	SceUInt64 publishedStepUsec = deltaUsec;
	SceKernelSysClock initialDelay;
	initialDelay.quad = GPS_EMU_INITIAL_DELAY_USEC;
	int status = sceKernelSetTimerEvent(ctx->notifyTimerId, 1, &initialDelay, 1);
	gpsEmuSetStateLocked(GPS_EMU_STATE_IDLE);

	for (;;) {
		if (gpsEmuGetStateLocked() == GPS_EMU_STATE_UNINIT)
			return status;

		SceUInt32 outBits = 0;
		if (s_gpsEmuSuspended == 1) {
			status = sceKernelWaitEventFlag(ctx->suspendEventFlag, GPS_EMU_EVF_RESUME,
							4u, &outBits, NULL);
			s_gpsEmuSuspended = 0;
			s_gpsEmuResumedFromSuspend = 1;
		} else {
			status = sceKernelWaitEvent(ctx->notifyTimerId, 0x8000u, &outBits, NULL, NULL);
		}
		if (status != 0)
			return status;

		if (outBits != 0x8000u)
			continue;

		sceKernelSetTimerEvent(ctx->notifyTimerId, 1, &s_gpsEmuTimerInterval, 1);
		gpsEmuPublishEpoch(&s_gpsEmuCurrentRecord, publishedStepUsec);
		sceKernelSetEventFlag(ctx->mainEventFlag, GPS_EMU_EVF_SAT_READY);
		sceKernelSetEventFlag(ctx->mainEventFlag, GPS_EMU_EVF_DATA_READY);

		bool locked = gpsEmuLockMutex(GPS_EMU_MUTEX_DATA);
		if (s_gpsEmuSingleEpochMode == 0) {
			s_gpsEmuCurrentRecord = s_gpsEmuNextRecord;
			sce_paf_memset(&s_gpsEmuNextRecord, 0, sizeof(s_gpsEmuNextRecord));
		}
		publishedStepUsec = deltaUsec;
		if (locked)
			gpsEmuUnlockMutex(GPS_EMU_MUTEX_DATA);

		bool wrappedOrFailed = false;
		for (;;) {
			int readRet = 0;
			if (s_gpsEmuSingleEpochMode == 0)
				readRet = gpsEmuReadNextNmeaEpoch(&s_gpsEmuNextRecord, &hasMoreEpochs);
			status = readRet;
			if (readRet != 0) {
				sceKernelStopTimer(s_gpsEmuCtx->notifyTimerId);
				sceKernelDeleteTimer(s_gpsEmuCtx->notifyTimerId);
				wrappedOrFailed = true;
				break;
			}
			if (hasMoreEpochs == 0) {
				deltaUsec = GPS_EMU_DEFAULT_PERIOD_USEC;
				s_gpsEmuTimerInterval.quad = GPS_EMU_DEFAULT_PERIOD_USEC - GPS_EMU_TIMER_OVERHEAD_USEC;
				wrappedOrFailed = true;
				break;
			}
			if (gpsEmuComputeEpochDelta(&s_gpsEmuCurrentRecord, &s_gpsEmuNextRecord, &deltaUsec))
				break;
			sce_paf_memset(&s_gpsEmuNextRecord, 0, sizeof(s_gpsEmuNextRecord));
		}

		if (!wrappedOrFailed && deltaUsec != 0)
			s_gpsEmuTimerInterval.quad = deltaUsec - GPS_EMU_TIMER_OVERHEAD_USEC;
	}
}

static void gpsEmuCleanupInitResources(bool stopTimer, bool stopParseThread, bool stopMainThread,
				       bool delParseThread, bool delMainThread,
				       bool delDataMutex, bool delApiMutex,
				       bool delMainEvf, bool unregPowerCb, bool delSuspendEvf)
{
	if (stopTimer)
		sceKernelDeleteTimer(s_gpsEmuStorage.notifyTimerId);
	if (stopParseThread)
		sceKernelWaitThreadEnd(s_gpsEmuStorage.parseThreadId, NULL, NULL);
	if (stopMainThread) {
		sceKernelSetEventFlag(s_gpsEmuStorage.mainEventFlag, GPS_EMU_EVF_EXIT);
		sceKernelWaitThreadEnd(s_gpsEmuStorage.mainThreadId, NULL, NULL);
	}
	if (delParseThread)
		sceKernelDeleteThread(s_gpsEmuStorage.parseThreadId);
	if (delMainThread)
		sceKernelDeleteThread(s_gpsEmuStorage.mainThreadId);
	if (delDataMutex)
		sceKernelDeleteMutex(s_gpsEmuStorage.dataMutexId);
	if (delApiMutex)
		sceKernelDeleteMutex(s_gpsEmuStorage.apiMutexId);
	if (delMainEvf)
		sceKernelDeleteEventFlag(s_gpsEmuStorage.mainEventFlag);
	if (delSuspendEvf)
		sceKernelDeleteEventFlag(s_gpsEmuStorage.suspendEventFlag);
	if (unregPowerCb) {
		scePowerUnregisterCallback(s_gpsEmuStorage.powerCallbackId);
		sceKernelDeleteCallback(s_gpsEmuStorage.powerCallbackId);
	}
	sceIoClose(s_gpsEmuStorage.nmeaFd);
	if (s_gpsEmuStorage.readBuffer != NULL) {
		operator delete(s_gpsEmuStorage.readBuffer);
		s_gpsEmuStorage.readBuffer = NULL;
	}
	s_gpsEmuCtx = NULL;
	s_gpsEmuStorage.suspendEventFlag = -1;
	s_gpsEmuStorage.powerCallbackId = -1;
	s_gpsEmuStorage.mainEventFlag = -1;
	s_gpsEmuStorage.mainThreadId = -1;
	s_gpsEmuStorage.apiMutexId = -1;
	s_gpsEmuStorage.dataMutexId = -1;
	s_gpsEmuStorage.parseThreadId = -1;
	s_gpsEmuStorage.notifyTimerId = -1;
}

/* (0x81373658) Opens the NMEA file and creates emulator threads, mutexes, events, and timer */
static int gpsEmuStartInternal(const char *nmeaPath)
{
	SceIoStat stat;
	char pathBuf[GPS_EMU_PATH_MAX_LEN];
	int statRet;

	if (nmeaPath[0] != '\0') {
		sce_paf_memset(pathBuf, 0, sizeof(pathBuf));
		sce_paf_strncpy(pathBuf, nmeaPath, sizeof(pathBuf) - 1);
		statRet = sceIoGetstat(pathBuf, &stat);
	} else {
		statRet = sceIoGetstat(GPS_EMU_DEFAULT_NMEA_PATH, &stat);
	}
	if (statRet != 0 || stat.st_size <= 0)
		return GPS_EMU_LOC_ERR_NO_FILE;

	const char *openPath = (nmeaPath[0] != '\0') ? pathBuf : GPS_EMU_DEFAULT_NMEA_PATH;
	SceUID fd = sceIoOpen(openPath, 0x10001, 0x104);
	if (fd < 0)
		return fd;

	s_gpsEmuStorage.nmeaFd = fd;
	s_gpsEmuStorage.nmeaFileSize = stat.st_size;
	s_gpsEmuNeedsBufferFill = 0;
	s_gpsEmuBufferCursor = 0;
	s_gpsEmuSmallFileLoaded = 0;

	char *readBuf = static_cast<char *>(ScePafStdc_3CF19248(GPS_EMU_READ_BUFFER_SIZE, g_locationHeap));
	s_gpsEmuStorage.readBuffer = readBuf;
	if (readBuf == NULL) {
		sceIoClose(fd);
		return GPS_EMU_LOC_ERR_NO_MEMORY;
	}

	sce_paf_memset(readBuf, 0, GPS_EMU_READ_BUFFER_SIZE);
	sce_paf_memset(&s_gpsEmuLocation, 0, sizeof(s_gpsEmuLocation));
	sce_paf_memset(&s_gpsEmuSatellites, 0, sizeof(s_gpsEmuSatellites));
	sce_paf_memset(&s_gpsEmuLastPublishedTime, 0, sizeof(s_gpsEmuLastPublishedTime));
	sce_paf_memset(&s_gpsEmuCurrentRecord, 0, sizeof(s_gpsEmuCurrentRecord));
	sce_paf_memset(&s_gpsEmuNextRecord, 0, sizeof(s_gpsEmuNextRecord));
	s_gpsEmuStorage.clientCallbackId = 0;
	s_gpsEmuStorage.state = GPS_EMU_STATE_UNINIT;

	SceUID suspendEvf = sceKernelCreateEventFlag("SceLocationGpsEmuSuspend", 0, 0, NULL);
	s_gpsEmuStorage.suspendEventFlag = suspendEvf;
	if (suspendEvf < 0) {
		gpsEmuCleanupInitResources(false, false, false, false, false, false, false, false, false, false);
		return suspendEvf;
	}

	SceUID powerCb = sceKernelCreateCallback(
		"SceLocationGpsEmuSuspendCallback", 0, gpsEmuPowerCallback, NULL);
	s_gpsEmuStorage.powerCallbackId = powerCb;
	if (powerCb < 0) {
		gpsEmuCleanupInitResources(false, false, false, false, false, false, false, false, false, true);
		return powerCb;
	}
	scePowerRegisterCallback(powerCb);

	SceUID mainEvf = sceKernelCreateEventFlag("SceLocationGpsEmulator", 0, 0, NULL);
	s_gpsEmuStorage.mainEventFlag = mainEvf;
	if (mainEvf < 0) {
		gpsEmuCleanupInitResources(false, false, false, false, false, false, false, false, true, true);
		return mainEvf;
	}

	SceUID apiMutex = sceKernelCreateMutex("SceLocationGpsEmulatorApi", 2u, 0, NULL);
	s_gpsEmuStorage.apiMutexId = apiMutex;
	if (apiMutex < 0) {
		gpsEmuCleanupInitResources(false, false, false, false, false, false, false, true, true, true);
		return apiMutex;
	}

	SceUID dataMutex = sceKernelCreateMutex("SceLocationGpsEmulatorData", 2u, 0, NULL);
	s_gpsEmuStorage.dataMutexId = dataMutex;
	if (dataMutex < 0) {
		gpsEmuCleanupInitResources(false, false, false, false, false, false, true, true, true, true);
		return dataMutex;
	}

	SceUID mainThid = sceKernelCreateThread(
		"SceLocationGpsEmulatorMain", gpsEmuMainThread, 106, 0x800u, 0, 0, NULL);
	s_gpsEmuStorage.mainThreadId = mainThid;
	if (mainThid < 0) {
		gpsEmuCleanupInitResources(false, false, false, false, false, true, true, true, true, true);
		return mainThid;
	}

	SceUID parseThid = sceKernelCreateThread(
		"SceLocationGpsEmulatorParse", gpsEmuParseThread, 106, 0x800u, 0, 0, NULL);
	s_gpsEmuStorage.parseThreadId = parseThid;
	if (parseThid < 0) {
		gpsEmuCleanupInitResources(false, false, false, false, true, true, true, true, true, true);
		return parseThid;
	}

	s_gpsEmuCtx = &s_gpsEmuStorage;
	int startMainRet = sceKernelStartThread(mainThid, sizeof(s_gpsEmuCtx), &s_gpsEmuCtx);
	if (startMainRet != 0) {
		gpsEmuCleanupInitResources(false, false, false, true, true, true, true, true, true, true);
		return startMainRet;
	}

	SceUID timerId = sceKernelCreateTimer("SceLocationGpsEmulatorNotifyData", 0, NULL);
	s_gpsEmuCtx->notifyTimerId = timerId;
	if (timerId < 0) {
		gpsEmuCleanupInitResources(false, false, true, true, true, true, true, true, true, true);
		return timerId;
	}

	int startParseRet = sceKernelStartThread(parseThid, sizeof(s_gpsEmuCtx), &s_gpsEmuCtx);
	if (startParseRet != 0) {
		gpsEmuCleanupInitResources(true, false, true, true, true, true, true, true, true, true);
		return startParseRet;
	}

	int startTimerRet = sceKernelStartTimer(timerId);
	if (startTimerRet < 0) {
		gpsEmuSetStateLocked(GPS_EMU_STATE_UNINIT);
		gpsEmuCleanupInitResources(true, true, true, true, true, true, true, true, true, true);
		return startTimerRet;
	}

	return 0;
}

/* (0x81373D5A) Shuts down emulator threads, timer, and file resources */
static int gpsEmuStopInternal(void)
{
	int ret = 0;
	if (s_gpsEmuCtx != NULL) {
		gpsEmuSetStateLocked(GPS_EMU_STATE_UNINIT);
		sceKernelStopTimer(s_gpsEmuCtx->notifyTimerId);
		sceKernelDeleteTimer(s_gpsEmuCtx->notifyTimerId);
		sceIoClose(s_gpsEmuCtx->nmeaFd);
		sceKernelWaitThreadEnd(s_gpsEmuCtx->parseThreadId, NULL, NULL);
		sceKernelSetEventFlag(s_gpsEmuCtx->mainEventFlag, GPS_EMU_EVF_EXIT);
		sceKernelWaitThreadEnd(s_gpsEmuCtx->mainThreadId, NULL, NULL);
		sceKernelDeleteThread(s_gpsEmuCtx->parseThreadId);
		sceKernelDeleteThread(s_gpsEmuCtx->mainThreadId);
		sceKernelDeleteMutex(s_gpsEmuCtx->dataMutexId);
		sceKernelDeleteMutex(s_gpsEmuCtx->apiMutexId);
		sceKernelDeleteEventFlag(s_gpsEmuCtx->mainEventFlag);
		scePowerUnregisterCallback(s_gpsEmuStorage.powerCallbackId);
		sceKernelDeleteCallback(s_gpsEmuStorage.powerCallbackId);
		ret = sceKernelDeleteEventFlag(s_gpsEmuCtx->suspendEventFlag);

		s_gpsEmuCtx = NULL;
		s_gpsEmuStorage.clientCallbackId = 0;
		s_gpsEmuStorage.suspendEventFlag = -1;
		s_gpsEmuStorage.powerCallbackId = -1;
		s_gpsEmuStorage.apiMutexId = -1;
		s_gpsEmuStorage.notifyTimerId = -1;
		s_gpsEmuStorage.dataMutexId = -1;
		s_gpsEmuStorage.parseThreadId = -1;
		s_gpsEmuStorage.mainEventFlag = -1;
		s_gpsEmuStorage.mainThreadId = -1;
		s_gpsEmuSingleEpochMode = 0;
		s_gpsEmuResumedFromSuspend = 0;
		s_gpsEmuFileOffset = 0;
	}

	s_gpsEmuNeedsBufferFill = 0;
	s_gpsEmuBufferCursor = 0;
	s_gpsEmuSmallFileLoaded = 0;
	if (s_gpsEmuStorage.readBuffer != NULL) {
		operator delete(s_gpsEmuStorage.readBuffer);
		s_gpsEmuStorage.readBuffer = NULL;
	}
	return ret;
}

/* (0x81375588) Parses $GPRMC payload fields */
static int gpsEmuParseGprmc(const unsigned char *payload, GpsEmuNmeaRecord *record)
{
	SceSize tokenLen = 0;
	const unsigned char *cursor = payload;
	int colIndex = 0;

	while (*cursor != '\n') {
		unsigned char ch = *cursor;
		switch (colIndex) {
		case 0: /* UTC hhmmss.sss */
			if (ch == ',') {
				if (tokenLen > 10)
					return GPS_EMU_ERR_SYNTAX;
				char part[8];
				sce_paf_memset(part, 0, 5);
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				sce_paf_memcpy(part, tok, 2);
				record->rmcDateTime.hour = static_cast<SceUInt16>(sce_paf_atoi(part));
				sce_paf_memcpy(part, tok + 2, 2);
				record->rmcDateTime.minute = static_cast<SceUInt16>(sce_paf_atoi(part));
				sce_paf_memcpy(part, tok + 4, 2);
				record->rmcDateTime.second = static_cast<SceUInt16>(sce_paf_atoi(part));
				sce_paf_memcpy(part, tok + 6, 4);
				record->rmcDateTime.microsecond =
					static_cast<SceUInt32>(static_cast<int>(sce_paf_atof(part) * 1000.0 * 1000.0));
				colIndex = 1;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 9:
		case 10:
			if (ch == ',')
				++colIndex;
			++cursor;
			break;

		case 6: /* Speed over ground in knots */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float speed = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->rmcSpeedKnots = speed;
				if (speed < 0.0f || speed > 999.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 7;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 7: /* Course over ground in degrees */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float course = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->rmcCourseDeg = course;
				if (course < 0.0f || course > 359.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 8;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 8: /* Date ddmmyy */
			if (ch == ',') {
				if (tokenLen != 6)
					return GPS_EMU_ERR_SYNTAX;
				char part[4];
				sce_paf_memset(part, 0, sizeof(part));
				char *tok = gpsEmuCopyToken(cursor - 6, 6);
				sce_paf_memcpy(part, tok, 2);
				record->rmcDateTime.day = static_cast<SceUInt16>(sce_paf_atoi(part));
				sce_paf_memcpy(part, tok + 2, 2);
				record->rmcDateTime.month = static_cast<SceUInt16>(sce_paf_atoi(part));
				sce_paf_memcpy(part, tok + 4, 2);
				record->rmcDateTime.year = static_cast<SceUInt16>(sce_paf_atoi(part) + 2000);
				colIndex = 9;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 11:
			if (ch == '*')
				colIndex = 12;
			++cursor;
			break;

		default:
			return 0;
		}
	}
	return 0;
}

/* (0x813758B4) Parses $GPGGA payload fields */
static int gpsEmuParseGpgga(const unsigned char *payload, GpsEmuNmeaRecord *record)
{
	SceSize tokenLen = 0;
	SceSize degLen = 0;
	const unsigned char *cursor = payload;
	int colIndex = 0;

	while (*cursor != '\n') {
		unsigned char ch = *cursor;
		switch (colIndex) {
		case 0: /* UTC hhmmss.sss */
			if (ch == ',') {
				if (tokenLen > 10)
					return GPS_EMU_ERR_SYNTAX;
				char part[8];
				sce_paf_memset(part, 0, 5);
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				sce_paf_memcpy(part, tok, 2);
				record->ggaTime.hour = static_cast<SceUInt16>(sce_paf_atoi(part));
				sce_paf_memcpy(part, tok + 2, 2);
				record->ggaTime.minute = static_cast<SceUInt16>(sce_paf_atoi(part));
				sce_paf_memcpy(part, tok + 4, 2);
				record->ggaTime.second = static_cast<SceUInt16>(sce_paf_atoi(part));
				sce_paf_memcpy(part, tok + 6, 4);
				record->ggaTime.microsecond =
					static_cast<SceUInt32>(static_cast<int>(sce_paf_atof(part) * 1000.0 * 1000.0));
				colIndex = 1;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 1: /* Latitude ddmm.mmmm */
			if (ch == ',') {
				char degStr[4];
				sce_paf_memset(degStr, 0, sizeof(degStr));
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				sce_paf_memcpy(degStr, tok, degLen);
				record->latDegrees = sce_paf_atoi(degStr);
				record->latMinutes = sce_paf_atof(tok + degLen);
				colIndex = 2;
				tokenLen = 0;
			} else {
				if (ch == '.') {
					degLen = tokenLen - 2;
					if (static_cast<int>(degLen) > 3)
						return GPS_EMU_ERR_SYNTAX;
				}
				++tokenLen;
			}
			++cursor;
			break;

		case 2: /* N/S indicator */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				if (tok != NULL && tok[0] == 'S') {
					record->latDegrees = -record->latDegrees;
					record->latMinutes = -record->latMinutes;
				}
				colIndex = 3;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 3: /* Longitude dddmm.mmmm */
			if (ch == ',') {
				char degStr[4];
				sce_paf_memset(degStr, 0, sizeof(degStr));
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				sce_paf_memcpy(degStr, tok, degLen);
				record->lonDegrees = sce_paf_atoi(degStr);
				record->lonMinutes = sce_paf_atof(tok + degLen);
				colIndex = 4;
				tokenLen = 0;
			} else {
				if (ch == '.') {
					degLen = tokenLen - 2;
					if (static_cast<int>(degLen) > 4)
						return GPS_EMU_ERR_SYNTAX;
				}
				++tokenLen;
			}
			++cursor;
			break;

		case 4: /* E/W indicator */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				if (tok != NULL && tok[0] == 'W') {
					record->lonDegrees = -record->lonDegrees;
					record->lonMinutes = -record->lonMinutes;
				}
				colIndex = 5;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 5: /* Fix quality (0..8) */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				SceUInt8 quality = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
				record->fixQuality = quality;
				if (quality > 8u)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 6;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 6:
		case 9:
		case 11:
		case 12:
			if (ch == ',')
				++colIndex;
			++cursor;
			break;

		case 7: /* HDOP */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float hdop = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->ggaHdop = hdop;
				if (hdop < 0.0f || hdop > 99.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 8;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 8: /* Altitude */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float alt = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->altitude = alt;
				if (alt < -9999.9f || alt > 17999.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 9;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 10: /* Geoid separation */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float geoid = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->geoidSeparation = geoid;
				if (geoid < -999.9f || geoid > 9999.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 11;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 13:
			if (ch == '*')
				colIndex = 14;
			++cursor;
			break;

		default:
			return 0;
		}
	}
	return 0;
}

/* (0x81375DF8) Parses $GPGSA payload fields */
static int gpsEmuParseGpgsa(const unsigned char *payload, GpsEmuNmeaRecord *record)
{
	SceSize tokenLen = 0;
	const unsigned char *cursor = payload;
	SceUInt8 activeCount = 0;
	int colIndex = 0;

	while (*cursor != '\n') {
		unsigned char ch = *cursor;
		switch (colIndex) {
		case 0:
			if (ch == ',')
				colIndex = 1;
			++cursor;
			break;

		case 1: /* Fix mode (1..3) */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				SceUInt8 mode = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
				record->fixMode = mode;
				if (mode == 0 || mode > 3u)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 2;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
		case 10:
		case 11:
		case 12:
		case 13:
			if (ch == ',') {
				if (tokenLen > 0) {
					char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
					SceUInt8 prn = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
					record->activeSatPrns[activeCount] = prn;
					if (prn == 0 || prn > GPS_SATELLITES_MAX)
						return GPS_EMU_ERR_SYNTAX;
					++activeCount;
				}
				++colIndex;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 14: /* PDOP */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float pdop = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->gsaPdop = pdop;
				if (pdop < 0.0f || pdop > 99.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 15;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 15: /* HDOP */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float hdop = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->gsaHdop = hdop;
				if (hdop < 0.0f || hdop > 99.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 16;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 16: /* VDOP followed by '*' */
			if (ch == '*') {
				record->activeSatCount = activeCount;
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float vdop = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->gsaVdop = vdop;
				if (vdop < 0.0f || vdop > 99.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 17;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		default:
			return 0;
		}
	}
	return 0;
}

/* (0x8137610E) Parses $GPGSV payload fields */
static int gpsEmuParseGpgsv(const unsigned char *payload, GpsEmuNmeaRecord *record)
{
	GpsSatellite *curSat = NULL;
	const unsigned char *cursor = payload;
	SceSize tokenLen = 0;
	int colIndex = 0;

	while (*cursor != '\n') {
		unsigned char ch = *cursor;
		switch (colIndex) {
		case 0: /* Total number of GSV messages (1..9) */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				SceUInt8 totalMsgs = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
				record->gsvTotalMessages = totalMsgs;
				if (totalMsgs == 0 || totalMsgs > 9u)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 1;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 1: /* Current message index (1..totalMsgs) */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				SceUInt8 msgNum = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
				record->gsvCurrentMessage = msgNum;
				if (msgNum == 0 || msgNum > 9u)
					return GPS_EMU_ERR_SYNTAX;
				if (msgNum <= record->gsvTotalMessages) {
					if (msgNum == 1) {
						record->gsvCollectedCount = 0;
						sce_paf_memset(&record->gsvSatellites[0], 0, sizeof(GpsSatellite));
						record->gsvReceiving = 1;
					}
				} else {
					record->gsvCurrentMessage = 0;
				}
				colIndex = 2;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 2: /* Total satellites in view */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				SceUInt8 totalSats = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
				record->totalSatsInView = (totalSats > GPS_SATELLITES_MAX) ? GPS_SATELLITES_MAX : totalSats;
				colIndex = 3;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 3:
		case 7:
		case 11:
		case 15: /* Satellite PRN */
			if (ch == ',') {
				if (record->gsvCurrentMessage != 0) {
					unsigned int satPos = record->gsvCollectedCount;
					if (satPos >= GPS_SATELLITES_MAX || satPos >= record->totalSatsInView) {
						curSat = NULL;
					} else {
						curSat = &record->gsvSatellites[satPos];
						if (tokenLen != 0) {
							char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
							SceUInt8 prn = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
							curSat->prn = prn;
							if (prn == 0 || prn > GPS_SATELLITES_MAX)
								return GPS_EMU_ERR_SYNTAX;
						} else {
							curSat->prn = 0;
						}
						++record->gsvCollectedCount;
					}
				}
				++colIndex;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 4:
		case 8:
		case 12:
		case 16: /* Elevation (0..90 deg) */
			if (ch == ',') {
				if (curSat != NULL) {
					if (tokenLen != 0) {
						char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
						SceUInt8 elev = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
						curSat->elevation = elev;
						if (elev > 90u)
							return GPS_EMU_ERR_SYNTAX;
					} else {
						curSat->elevation = 0;
					}
				}
				++colIndex;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 5:
		case 9:
		case 13:
		case 17: /* Azimuth (0..359 deg) */
			if (ch == ',') {
				if (curSat != NULL) {
					if (tokenLen != 0) {
						char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
						SceUInt16 azim = static_cast<SceUInt16>(tok != NULL ? sce_paf_atoi(tok) : 9999);
						curSat->azimuth = azim;
						if (azim > 359u)
							return GPS_EMU_ERR_SYNTAX;
					} else {
						curSat->azimuth = 0;
					}
				}
				++colIndex;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 6:
		case 10:
		case 14:
		case 18: /* SNR (0..99 dB) */
			if (ch == ',' || ch == '*') {
				if (curSat != NULL) {
					if (tokenLen != 0) {
						char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
						SceUInt8 snr = static_cast<SceUInt8>(tok != NULL ? sce_paf_atoi(tok) : 15);
						curSat->snr = snr;
						if (snr > 99u)
							return GPS_EMU_ERR_SYNTAX;
					} else {
						curSat->snr = 0;
					}
				}
				colIndex = (ch == '*') ? 19 : (colIndex + 1);
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		default:
			return 0;
		}
	}
	return 0;
}

/* (0x813764F4) Parses $GPVTG payload fields */
static int gpsEmuParseGpvtg(const unsigned char *payload, GpsEmuNmeaRecord *record)
{
	SceSize tokenLen = 0;
	const unsigned char *cursor = payload;
	int colIndex = 0;

	while (*cursor != '\n') {
		unsigned char ch = *cursor;
		switch (colIndex) {
		case 0: /* True track made good (degrees) */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float trueTrack = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->vtgTrueTrack = trueTrack;
				if (trueTrack < 0.0f || trueTrack > 359.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 1;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 1:
		case 3:
		case 4:
		case 5:
		case 7:
			if (ch == ',')
				++colIndex;
			++cursor;
			break;

		case 2: /* Magnetic track made good (degrees) */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float magTrack = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->vtgMagneticTrack = magTrack;
				if (magTrack < 0.0f || magTrack > 359.9f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 3;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 6: /* Ground speed (km/h) */
			if (ch == ',') {
				char *tok = gpsEmuCopyToken(cursor - tokenLen, tokenLen);
				float speedKmh = static_cast<float>(tok != NULL ? sce_paf_atof(tok) : 99999.8984375);
				record->vtgSpeedKmh = speedKmh;
				if (speedKmh < 0.0f || speedKmh > 1800.0f)
					return GPS_EMU_ERR_SYNTAX;
				colIndex = 7;
				tokenLen = 0;
			} else {
				++tokenLen;
			}
			++cursor;
			break;

		case 8:
			if (ch == '*')
				colIndex = 9;
			++cursor;
			break;

		default:
			return 0;
		}
	}
	return 0;
}

/* (0x81376700) Validates NMEA-0183 framing & XOR checksum, then dispatches sentence parser */
static int gpsEmuParseNmeaLine(const unsigned char *line, unsigned int *outConsumed,
			       GpsEmuNmeaRecord *record)
{
	unsigned int tokenLen = 0;
	unsigned int checksum = 0;
	unsigned int consumed = 0;
	int parseErr = 0;
	int sentenceId = GPS_NMEA_NONE;
	int stage = 0;
	const unsigned char *payload = NULL;
	const unsigned char *cursor = line;

	while (*cursor != '\n' && *cursor != '\0') {
		unsigned char ch = *cursor;
		switch (stage) {
		case 0: /* Wait for '$' sentence start */
			if (ch == '$') {
				checksum = 0;
				stage = 1;
			}
			break;

		case 1: /* Sentence tag up to first ',' */
			if (ch == ',') {
				if (tokenLen == 5) {
					char *tag = gpsEmuCopyToken(cursor - 5, 5);
					for (unsigned int tableIdx = 0; tableIdx < 5; ++tableIdx) {
						if (sce_paf_strncmp(tag, s_nmeaSentenceTable[tableIdx].talkerTag, 5) == 0) {
							sentenceId = s_nmeaSentenceTable[tableIdx].sentenceId;
							break;
						}
					}
					tokenLen = 0;
					payload = cursor + 1;
					stage = 2;
				} else {
					record->parseFailed = 1;
					parseErr = GPS_EMU_ERR_SYNTAX;
				}
			} else {
				++tokenLen;
			}
			checksum = (checksum ^ ch) & 0xFFu;
			break;

		case 2: /* Body up to '*' checksum delimiter */
			if (ch == '*') {
				tokenLen = 0;
				stage = 3;
			} else {
				checksum = (checksum ^ ch) & 0xFFu;
				++tokenLen;
			}
			break;

		case 3: /* Two hex checksum digits */
			if (cursor[1] == '\r') {
				record->parseFailed = 1;
				parseErr = GPS_EMU_ERR_SYNTAX;
			} else {
				char *hexTok = gpsEmuCopyToken(cursor, 2);
				++consumed;
				++cursor;
				stage = 4;
				unsigned int expected = static_cast<unsigned int>(sce_paf_strtol(hexTok, NULL, 16)) & 0xFFu;
				if (expected != checksum) {
					record->parseFailed = 1;
					parseErr = GPS_EMU_ERR_CHECKSUM;
				}
			}
			break;

		case 4: /* Verify CRLF terminator */
			if (ch != '\r' || cursor[1] != '\n') {
				record->parseFailed = 1;
				parseErr = GPS_EMU_ERR_SYNTAX;
			}
			break;
		}

		if (*cursor == '\0')
			break;
		++cursor;
		++consumed;
	}

	++consumed;

	int result = parseErr;
	if (result == 0) {
		switch (sentenceId) {
		case GPS_NMEA_GPRMC:
			if (record->hasRmc == 1) {
				result = 0;
				consumed = 0;
			} else {
				if (gpsEmuParseGprmc(payload, record) != 0 ||
				    sceRtcCheckValid(&record->rmcDateTime) != 0) {
					record->rmcSpeedKnots = 0.0f;
					record->rmcCourseDeg = 0.0f;
					sce_paf_memset(&record->rmcDateTime, 0, sizeof(record->rmcDateTime));
				} else {
					record->hasRmc = 1;
				}
				result = 1;
			}
			break;

		case GPS_NMEA_GPGGA:
			if (record->hasGga == 1) {
				result = 0;
				consumed = 0;
			} else {
				if (gpsEmuParseGpgga(payload, record) != 0) {
					sce_paf_memset(&record->ggaTime, 0, 64);
				} else {
					SceDateTime checkDate;
					sceRtcGetCurrentClock(&checkDate, 0);
					gpsEmuCopyTimeToDateTime(&checkDate, &record->ggaTime);
					if (sceRtcCheckValid(&checkDate) != 0)
						sce_paf_memset(&record->ggaTime, 0, 64);
					else
						record->hasGga = 1;
				}
				result = 1;
			}
			break;

		case GPS_NMEA_GPGSA:
			if (record->hasGsa == 1) {
				result = 0;
				consumed = 0;
			} else {
				if (gpsEmuParseGpgsa(payload, record) != 0)
					sce_paf_memset(&record->fixMode, 0, 52);
				else
					record->hasGsa = 1;
				result = 1;
			}
			break;

		case GPS_NMEA_GPGSV:
			if (record->hasGsv == 1) {
				result = 0;
				consumed = 0;
			} else {
				if (gpsEmuParseGpgsv(payload, record) != 0) {
					sce_paf_memset(&record->totalSatsInView, 0, 264);
				} else if (record->gsvTotalMessages == record->gsvCurrentMessage) {
					record->gsvReceiving = 0;
					record->hasGsv = 1;
				}
				result = 1;
			}
			break;

		case GPS_NMEA_GPVTG:
			if (record->hasVtg == 1) {
				result = 0;
				consumed = 0;
			} else {
				if (gpsEmuParseGpvtg(payload, record) != 0) {
					record->vtgSpeedKmh = 0.0f;
					record->vtgTrueTrack = 0.0f;
					record->vtgMagneticTrack = 0.0f;
				} else {
					record->hasVtg = 1;
				}
				result = 1;
			}
			break;

		default:
			result = 1;
			break;
		}

		if (record->hasRmc == 1 && record->hasGga == 1 &&
		    record->hasGsa == 1 && record->hasGsv == 1 &&
		    record->hasVtg == 1) {
			result = 0;
		}
	}

	outConsumed[0] = consumed;
	outConsumed[1] = 0;
	return result;
}
