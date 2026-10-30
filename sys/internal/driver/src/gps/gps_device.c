/* SceGps: the registered devices, the data they report and the driver API.
 * Internal names unknown: descriptive names follow the observed behavior.
 */
#include "gps_internal.h"

#define STATE_ERROR(code)	((code) ? GPS_ERROR(code) : 0)
#define E(code)			STATE_ERROR(code)

static GpsLocation s_location;
static GpsDriver s_drivers[4];
static GpsSatellites s_satellites;
static unsigned s_selected;
static GpsDeviceInfo s_info[4];

/* the error each driver call returns in each state */
static const int s_registerErrors[9] = { E(1), E(0), E(0), E(3), E(11), E(3), E(3), E(3), E(11) };
static const int s_startErrors[9] = { E(1), E(2), E(4), E(6), E(0), E(7), E(7), E(7), E(11) };
static const int s_stopErrors[9] = { E(1), E(2), E(4), E(6), E(11), E(7), E(7), E(7), E(0) };
static const int s_resultErrors[9] = { E(1), E(2), E(4), E(6), E(11), E(0), E(0), E(0), E(0) };
static const int s_unregisterErrors[9] = { E(1), E(2), E(0), E(5), E(11), E(7), E(7), E(7), E(11) };
static const int s_publishErrors[9] = { E(1), E(2), E(4), E(6), E(11), E(0), E(0), E(0), E(11) };

/* Sony inlines these within this file and keeps a copy for the others */
#define INLINE static inline __attribute__((always_inline))

INLINE unsigned getDeviceState(void)
{
	unsigned state = 0;

	if (gpsContext()) {
		gpsLock(GPS_LOCK_DATA);
		state = gpsContext()->deviceState;
		gpsUnlock(GPS_LOCK_DATA);
	}
	return state;
}

INLINE int putDeviceState(unsigned state)
{
	if (!gpsContext())
		return GPS_ERROR(1);
	gpsLock(GPS_LOCK_DATA);
	gpsContext()->deviceState = state;
	gpsUnlock(GPS_LOCK_DATA);
	return 0;
}

INLINE void clearData(void)
{
	gpsLock(GPS_LOCK_DATA);
	memset(&s_location, 0, sizeof(s_location));
	memset(&s_satellites, 0, sizeof(s_satellites));
	gpsUnlock(GPS_LOCK_DATA);
}

int SceGpsForDriver_884A5867(const GpsDriver *driver)
{
	GpsDriver *copy;
	unsigned index;
	int ret;

	if (!driver || !driver->start || !driver->stop || driver->device - 1U >= 4)
		return GPS_ERROR(32);
	index = driver->device - 1;
	if (s_info[index].device)
		return GPS_ERROR(3);
	copy = &s_drivers[index];
	copy->device = copy->flags = 0;
	copy->start = copy->stop = copy->restart = NULL;
	copy->ioctl = NULL;
	*copy = *driver;
	s_info[index].device = copy->device;
	s_info[index].flags = copy->flags;
	if (!s_selected)
		s_selected = driver->device;
	ret = s_registerErrors[gpsState()];
	if (!ret)
		gpsSetState(2);
	return ret;
}

int SceGpsForDriver_BE3EB4E9(unsigned device, unsigned mode, const GpsLocation *location)
{
	SceDateTime date;
	SceRtcTick tick;
	unsigned state;
	int ret, allowed;

	if (!location || s_selected != device || (mode & 15) > 1)
		return GPS_ERROR(32);
	ret = s_publishErrors[gpsState()];
	if (ret)
		return ret;
	gpsLock(GPS_LOCK_DATA);
	s_location = *location;
	date = s_location.time;
	state = gpsState();
	if (device - 1U < 4 && gpsContext()->setClock == 1) {
		allowed = device == 2 || (device == 1 && !canSetClock(&s_drivers[0]));
		if (allowed && state - 6U < 2 && !s_location.noClock &&
		    sceRtcGetTick(&date, &tick) >= 0 && SceRtcForDriver_A7236656(&tick) >= 0)
			gpsContext()->setClock = 0;
	}
	gpsUnlock(GPS_LOCK_DATA);
	sceKernelSetEventFlag(gpsContext()->event, 8);
	return 0;
}

int SceGpsForDriver_8D5688DA(unsigned device, const GpsSatellites *satellites)
{
	int ret;

	if (!satellites || device != s_selected)
		return GPS_ERROR(32);
	ret = s_publishErrors[gpsState()];
	if (ret)
		return ret;
	gpsLock(GPS_LOCK_DATA);
	memcpy(&s_satellites, satellites, sizeof(s_satellites));
	gpsUnlock(GPS_LOCK_DATA);
	sceKernelSetEventFlag(gpsContext()->event, 0x10);
	return 0;
}

int SceGpsForDriver_092AD9BF(unsigned device, const char *sentence, unsigned size)
{
	int ret;

	if (!sentence || device != s_selected)
		return GPS_ERROR(32);
	ret = s_publishErrors[gpsState()];
	return ret ? ret : gpsParseSentence(sentence, size);
}

int SceGpsForDriver_F2049C02(unsigned type, unsigned event)
{
	unsigned result = event & 255;
	unsigned flag;
	int ret;

	event &= 0xffff0000;
	switch (type) {
	case 0:
		switch (event) {
		case 0x10000:
			flag = 2;
			gpsLock(GPS_LOCK_DATA);
			gpsContext()->startResult = result;
			gpsUnlock(GPS_LOCK_DATA);
			break;
		case 0x80000:
			flag = 0x40;
			gpsLock(GPS_LOCK_DATA);
			gpsContext()->stopResult = result;
			gpsUnlock(GPS_LOCK_DATA);
			break;
		case 0x60000:
			ret = gpsClearPending();
			if (ret)
				return ret;
			return gpsNotify(event, result);
		default:
			return 0;
		}
		sceKernelSetEventFlag(gpsContext()->event, flag);
		return 0;
	case 0x10000000:
	case 0x20000000:
	case 0x30000000:
	case 0x40000000:
		return gpsNotify(event, result);
	default:
		return 0;
	}
}

int SceGpsForDriver_54DBA451(unsigned device)
{
	int ret = device - 1U < 4 ? 0 : GPS_ERROR(32);

	/* Sony still indexes the table for invalid device numbers. */
	if (!s_drivers[device - 1].device)
		ret = GPS_ERROR(2);
	return ret;
}

int startDevice(unsigned mode)
{
	GpsDriver *driver;
	int ret, ready;

	if (s_selected - 1U >= 4)
		return GPS_ERROR(32);
	driver = &s_drivers[s_selected - 1];
	gpsLock(GPS_LOCK_DATA);
	ret = s_startErrors[gpsState()];
	if (!(mode & 15)) {
		if (!ret) {
			clearData();
			ret = driver->start ? driver->start() : GPS_ERROR(32);
			if (!ret)
				goto started;
		}
	} else if ((mode & 15) == 1) {
		ready = getDeviceState() == 2;
		if (!ret)
			ready = 1;
		if (ready) {
			ret = driver->restart ? driver->restart() : GPS_ERROR(34);
			if (!ret)
				goto started;
		}
	} else
		ret = GPS_ERROR(32);
	putDeviceState(0);
	gpsSetState(3);
	gpsUnlock(GPS_LOCK_DATA);
	return ret;
started:
	if (!(driver->flags & 8)) {
		putDeviceState(1);
		gpsSetState(5);
	}
	gpsUnlock(GPS_LOCK_DATA);
	return 0;
}

int stopDevice(unsigned mode)
{
	GpsDriver *driver;
	int ret;

	if (s_selected - 1U >= 4)
		return GPS_ERROR(32);
	driver = &s_drivers[s_selected - 1];
	gpsLock(GPS_LOCK_DATA);
	ret = s_stopErrors[gpsState()];
	if (ret)
		goto failure;
	if (!(mode & 15)) {
		if (!driver->stop)
			goto invalid;
		ret = driver->stop();
	} else if ((mode & 15) == 1) {
		if (getDeviceState() == 1) {
			if (driver->device != 1) {
				clearData();
				goto stopped;
			}
			ret = specialStop(driver);
		} else {
			if (driver->flags & 0x10) {
				gpsLock(GPS_LOCK_DATA);
				gpsContext()->stopResult = 0;
				gpsUnlock(GPS_LOCK_DATA);
				sceKernelSetEventFlag(gpsContext()->event, 0x40);
			}
			clearData();
			goto stopped;
		}
	} else
		goto invalid;
	clearData();
	if (ret)
		goto failure;
stopped:
	if (driver->flags & 0x10) {
		gpsUnlock(GPS_LOCK_DATA);
		return 0;
	}
	putDeviceState(0);
	gpsSetState(3);
	gpsUnlock(GPS_LOCK_DATA);
	return 0;
invalid:
	ret = GPS_ERROR(32);
	clearData();
failure:
	putDeviceState(1);
	gpsSetState(5);
	gpsUnlock(GPS_LOCK_DATA);
	return ret;
}

int applyStopResult(unsigned result)
{
	int ret;

	gpsLock(GPS_LOCK_DATA);
	ret = s_resultErrors[gpsState()];
	if (!ret) {
		if (gpsState() == 8) {
			putDeviceState(result ? 1 : 0);
			gpsSetState(result ? 5 : 3);
		} else
			putDeviceState(2);
	}
	gpsUnlock(GPS_LOCK_DATA);
	return ret;
}

/* the worker saw a new location: the state says so */
int gpsLocationUpdated(void)
{
	gpsLock(GPS_LOCK_DATA);
	if (!s_publishErrors[gpsState()])
		gpsSetState(6);
	gpsUnlock(GPS_LOCK_DATA);
	return 0;
}

/* a complete set of navigation sentences: publish it as the location */
int gpsPublishNavigation(const GpsNavigation *navigation)
{
	int ret;

	if (!navigation)
		return 0;
	gpsLock(GPS_LOCK_DATA);
	s_location.fixQuality = navigation->fixQuality;
	s_location.fixType = navigation->fixType;
	s_location.time = navigation->time;
	s_location.altitude = navigation->altitude;
	s_location.speed = navigation->speed;
	s_location.pdop = navigation->pdop;
	s_location.course = navigation->course;
	s_location.geoidSeparation = navigation->geoidSeparation;
	s_location.magneticCourse = navigation->magneticCourse;
	s_location.hdop = navigation->hdop;
	s_location.vdop = navigation->vdop;
	s_location.latitude = (float)navigation->latitudeDegrees +
			      (float)navigation->latitudeMinutes / 60.0f;
	s_location.longitude = (float)navigation->longitudeDegrees +
			       (float)navigation->longitudeMinutes / 60.0f;
	s_location.noClock = 0;
	gpsUnlock(GPS_LOCK_DATA);
	ret = gpsResetParser(1);
	sceKernelSetEventFlag(gpsContext()->event, 8);
	return ret;
}

/* a complete GSV sequence: publish the satellites in view */
int gpsPublishSatellites(const GpsSatelliteParse *parse)
{
	int ret;

	if (!parse)
		return 0;
	gpsLock(GPS_LOCK_DATA);
	if (parse->count > GPS_SATELLITES_MAX)
		s_satellites.count = 0;
	else {
		s_satellites.count = parse->count;
		for (unsigned i = 0; i < GPS_SATELLITES_MAX; i++) {
			GpsSatellite *out = &s_satellites.satellite[i];
			const GpsSatellite *in = &parse->satellite[i];
			if (i < parse->count) {
				out->prn = in->prn;
				out->elevation = in->elevation;
				out->azimuth = in->azimuth;
				out->snr = in->snr;
				out->used = 0;
				for (unsigned j = 0; j < parse->usedCount; j++)
					if (parse->used[j] == in->prn) {
						out->used = 1;
						break;
					}
			} else
				for (unsigned j = 0; j < sizeof(*out); j++)
					((uint8_t *)out)[j] = 0;
		}
	}
	gpsUnlock(GPS_LOCK_DATA);
	ret = gpsResetParser(2);
	sceKernelSetEventFlag(gpsContext()->event, 0x10);
	return ret;
}

int gpsGetData(GpsLocation *location, GpsSatellites *satellites)
{
	int ret;

	if (location) {
		gpsLock(GPS_LOCK_DATA);
		location->time.year = s_location.time.year;
		location->time.month = s_location.time.month;
		location->time.day = s_location.time.day;
		location->time.hour = s_location.time.hour;
		location->time.minute = s_location.time.minute;
		location->time.second = s_location.time.second;
		location->time.microsecond = s_location.time.microsecond;
		location->hdop = s_location.hdop;
		location->vdop = s_location.vdop;
		location->pdop = s_location.pdop;
		location->fixQuality = s_location.fixQuality;
		location->fixType = s_location.fixType;
		location->noClock = s_location.noClock;
		location->latitude = s_location.latitude;
		location->longitude = s_location.longitude;
		location->altitude = s_location.altitude;
		location->geoidSeparation = s_location.geoidSeparation;
		location->speed = s_location.speed;
		location->course = s_location.course;
		location->magneticCourse = s_location.magneticCourse;
		gpsUnlock(GPS_LOCK_DATA);
	}
	ret = 0;
	if (satellites) {
		if (s_satellites.count > GPS_SATELLITES_MAX)
			ret = GPS_ERROR(32);
		else {
			gpsLock(GPS_LOCK_DATA);
			satellites->count = s_satellites.count;
			for (unsigned i = 0; i < GPS_SATELLITES_MAX; i++) {
				GpsSatellite *out = &satellites->satellite[i];
				const GpsSatellite *in = &s_satellites.satellite[i];
				if (i < s_satellites.count) {
					out->prn = in->prn;
					out->elevation = in->elevation;
					out->azimuth = in->azimuth;
					out->snr = in->snr;
					out->used = in->used;
				} else {
					out->prn = 0;
					out->elevation = 0;
					out->azimuth = 0;
					out->snr = 0;
					out->used = 0;
				}
			}
			gpsUnlock(GPS_LOCK_DATA);
		}
	}
	/* a restart that is waiting for the data to be read */
	if (gpsContext()->mode == 1 && getDeviceState() == 2) {
		gpsLock(GPS_LOCK_API);
		ret = gpsSignal(1, 1);
		gpsUnlock(GPS_LOCK_API);
	}
	if (gpsState() == 6)
		gpsSetState(7);
	return ret;
}

int getDeviceInfo(GpsDeviceInfo *info)
{
	if (s_selected - 1U >= 4 || !info)
		return GPS_ERROR(32);
	*info = s_info[s_selected - 1];
	return 0;
}

int resetDevices(unsigned device)
{
	gpsLock(GPS_LOCK_DATA);
	if (!device) {
		memset(s_drivers, 0, sizeof(s_drivers));
		s_selected = 0;
		for (unsigned i = 0; i < 4; i++)
			s_info[i].device = s_info[i].flags = 0;
	} else if (device < 5) {
		GpsDriver *driver = &s_drivers[device - 1];
		driver->device = driver->flags = 0;
		driver->start = driver->stop = driver->restart = NULL;
		driver->ioctl = NULL;
		s_info[device - 1].device = s_info[device - 1].flags = 0;
		s_selected = 0;
		for (unsigned i = 0; i < 4; i++)
			if (s_drivers[i].device) {
				s_selected = i + 1;
				break;
			}
	}
	memset(&s_location, 0, sizeof(s_location));
	memset(&s_satellites, 0, sizeof(s_satellites));
	gpsUnlock(GPS_LOCK_DATA);
	return gpsSetState(s_selected ? 2 : 1);
}

int SceGpsForDriver_7F001760(unsigned device)
{
	int ret;

	if (device - 1U >= 4)
		return GPS_ERROR(32);
	if (!s_drivers[device - 1].device)
		return GPS_ERROR(2);
	ret = s_unregisterErrors[gpsState()];
	if (ret == GPS_ERROR(1))
		return ret;
	if (ret)
		sceKernelSetEventFlag(gpsContext()->event, 0x20000000);
	resetDevices(device);
	return 0;
}

int gpsIsDevice(void)
{
	unsigned state = gpsState();

	return s_selected - 1U < 4 && state > 1 && s_drivers[s_selected - 1].flags != 0;
}

int deviceIoctl(unsigned command, void *data, unsigned size)
{
	GpsDriver *driver;

	if (s_selected - 1U >= 4)
		return GPS_ERROR(32);
	driver = &s_drivers[s_selected - 1];
	if (!driver->ioctl)
		return GPS_ERROR(34);
	return driver->ioctl(command, data, size, gpsState());
}

int gpsSelectDevice(unsigned device)
{
	if (device - 1U >= 4)
		return GPS_ERROR(32);
	if (!s_drivers[device - 1].device)
		return GPS_ERROR(2);
	s_selected = device;
	return 0;
}

unsigned deviceState(void)
{
	return getDeviceState();
}

int setDeviceState(unsigned state)
{
	return putDeviceState(state);
}
