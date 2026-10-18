/* NMEA acquisition. Internal helper/field names unknown. */
#include "gps_internal.h"

/* Sony's debug build is fortified */
#if FW_DEBUG
#include <sysmem/fortify.h>
#endif

#define GPS_ERROR_SENTENCE	0x80390012	/* malformed */
#define GPS_ERROR_CHECKSUM	0x80390011

/* gpsResetParser */
#define PARSER_ALL		0
#define PARSER_NAVIGATION	1
#define PARSER_SATELLITES	2

#define RECEIVED_NAVIGATION	0x01010101	/* GpsNavigation.received.all: every sentence */
#define RECEIVED_SATELLITES	0x0101		/* GpsSatelliteParse.received.all */

int gpsResetParser(unsigned what)
{
	if (what == PARSER_NAVIGATION)
		memset(&s_gps.navigation, 0, sizeof(s_gps.navigation));
	else if (what == PARSER_SATELLITES)
		memset(&s_gps.satelliteParse, 0, sizeof(s_gps.satelliteParse));
	else if (what == PARSER_ALL) {
		memset(&s_gps.navigation, 0, sizeof(s_gps.navigation));
		memset(&s_gps.satelliteParse, 0, sizeof(s_gps.satelliteParse));
	}
	return 0;
}

int gpsPublishNavigation(void)
{
	GpsLocation *location = &s_gps.location;
	const GpsNavigation *navigation = &s_gps.navigation;

	gpsDataLock();
	location->fixQuality = navigation->fixQuality;
	location->fixType = navigation->fixType;
	location->time = navigation->time;
	location->altitude = navigation->altitude;
	location->speed = navigation->speed;
	location->pdop = navigation->pdop;
	location->course = navigation->course;
	location->geoidSeparation = navigation->geoidSeparation;
	location->magneticCourse = navigation->magneticCourse;
	location->hdop = navigation->hdop;
	location->vdop = navigation->vdop;
	location->latitude = (float)navigation->latitudeDegrees +
			     (float)navigation->latitudeMinutes / 60.0f;
	location->longitude = (float)navigation->longitudeDegrees +
			      (float)navigation->longitudeMinutes / 60.0f;
	location->noClock = 0;
	gpsDataUnlock();
	int ret = gpsResetParser(PARSER_NAVIGATION);
	sceKernelSetEventFlag(s_gps.active->event, 8);
	return ret;
}

int gpsPublishSatellites(void)
{
	const GpsSatelliteParse *parse = &s_gps.satelliteParse;

	gpsDataLock();
	s_gps.satellites.count = parse->count;
	if (s_gps.satellites.count > GPS_SATELLITES_MAX)
		s_gps.satellites.count = 0;
	else {
		for (unsigned i = 0; i < GPS_SATELLITES_MAX; i++) {
			GpsSatellite *out = &s_gps.satellites.satellite[i];
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
	gpsDataUnlock();
	int ret = gpsResetParser(PARSER_SATELLITES);
	sceKernelSetEventFlag(s_gps.active->event, 0x10);
	return ret;
}

static int white(unsigned c)
{
	return c == ' ' || (c >= '\t' && c <= '\r');
}

static unsigned parseInteger(const char *value)
{
	unsigned number = 0;

	while (white((uint8_t)*value))
		value++;
	while ((uint8_t)*value - '0' < 10U)
		number = number * 10 + (uint8_t)*value++ - '0';
	return number;
}

static float parseFloat(const char *value)
{
	float sign = 1.0f, result = 0.0f, divisor = 1.0f;

	while (white((uint8_t)*value))
		value++;
	if (*value == '-') {
		sign = -1.0f;
		value++;
	}
	while ((uint8_t)*value - '0' < 10U)
		result = result * 10.0f + (float)((uint8_t)*value++ - '0');
	if (*value == '.') {
		value++;
		while ((uint8_t)*value - '0' < 10U) {
			divisor *= 10.0f;
			result = result * 10.0f + (float)((uint8_t)*value++ - '0');
		}
	}
	return (sign * result) / divisor;
}

static void clearNumber(void)
{
	for (unsigned i = 0; i < 8; i++)
		s_gps.numberWord[i] = 0;
}

static void copyNumber(const char *field, unsigned size)
{
	memcpy(s_gps.number, field, size);
}

static unsigned integerField(const char *field, unsigned size)
{
	if (size >= 32)
		return 0;
	clearNumber();
	if (!field || !size)
		return 0;
	copyNumber(field, size);
	return parseInteger(s_gps.number);
}

static float floatField(const char *field, unsigned size)
{
	if (size >= 32)
		return 0.0f;
	clearNumber();
	if (field && size)
		copyNumber(field, size);
	return parseFloat(s_gps.number);
}

static const char *copyField(const char *field, unsigned size)
{
	if (size >= 32)
		return NULL;
	clearNumber();
	if (field && size)
		copyNumber(field, size);
	return s_gps.number;
}

static void parseTime(const char *field, unsigned size)
{
	const char *number = copyField(field, size);
	/* Sony assumes a field shorter than the scratch buffer here. */
	char part[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	for (unsigned i = 0; i < 3; i++) {
		part[0] = number[i * 2];
		part[1] = number[i * 2 + 1];
		s_gps.navigation.timePart[3 + i] = parseInteger(part);	/* hour, minute, second */
	}
	for (unsigned i = 0; i < 4; i++)
		part[i] = number[6 + i];
	float microseconds = parseFloat(part) * 1000.0f * 1000.0f;
	s_gps.navigation.time.microsecond = (unsigned)microseconds;
}

static int parseDate(const char *field, unsigned size)
{
	if (size != 6)
		return (int)GPS_ERROR_SENTENCE;
	clearNumber();
	for (unsigned i = 0; i < 6; i++)
		s_gps.number[i] = field[i];
	char part[4] = {0, 0, 0, 0};
	for (unsigned i = 0; i < 3; i++) {
		part[0] = s_gps.number[2 * i];
		part[1] = s_gps.number[2 * i + 1];
		/* DDMMYY: day, month, year */
		s_gps.navigation.timePart[2 - i] = parseInteger(part) + (i == 2 ? 2000 : 0);
	}
	return 0;
}

static void coordinate(const char *field, unsigned size, unsigned degrees)
{
	const char *number = copyField(field, size);
	char part[4] = {0, 0, 0, 0};
	for (unsigned i = 0; i < degrees; i++)
		part[i] = number[i];
	/* latitude is DDMM.MMMM, longitude DDDMM.MMMM */
	*(degrees == 2 ? &s_gps.navigation.latitudeDegrees : &s_gps.navigation.longitudeDegrees) =
		parseInteger(part);
	*(degrees == 2 ? &s_gps.navigation.latitudeMinutes : &s_gps.navigation.longitudeMinutes) =
		(double)parseFloat(number + degrees);
}

static int parseFields(unsigned type, const char *start)
{
	unsigned index = 0, size = 0, used = 0;
	GpsNavigation *navigation = &s_gps.navigation;
	GpsSatelliteParse *parse = &s_gps.satelliteParse;
	GpsSatellite *satellite = NULL;
	for (const char *p = start; *p; p++) {
		char ch = *p;
		if (type == GPS_SENTENCE_RMC) {
			if (index > 11)
				break;
			if (index == 0 && ch == ',') {
				parseTime(p - size, size);
				index = 1;
				size = 0;
			} else if (index == 8 && ch == ',') {
				int ret = parseDate(p - size, size);
				if (ret)
					return ret;
				index++;
				size = 0;
			} else if (index == 11) {
				if (ch == '*')
					index = 12;
			} else if (index == 0 || index == 8)
				size++;
			else if (ch == ',')
				index++;
		} else if (type == GPS_SENTENCE_GGA) {
			if (index > 13)
				break;
			if (index == 0) {
				index = ch == ',';
			} else if (index == 6 || index == 9 || index == 11 || index == 12) {
				if (ch == ',')
					index++;
			} else if (index == 13) {
				if (ch == '*')
					index++;
			} else if (ch == ',') {
				const char *field = p - size;
				switch (index) {
				case 1: coordinate(field, size, 2); break;
				case 2:
					if (size && size < 32 && *copyField(field, size) == 'S')
						navigation->latitudeDegrees = -navigation->latitudeDegrees;
					break;
				case 3: coordinate(field, size, 3); break;
				case 4:
					if (size && size < 32 && *copyField(field, size) == 'W')
						navigation->longitudeDegrees = -navigation->longitudeDegrees;
					break;
				case 5: navigation->fixQuality = integerField(field, size); break;
				case 7: navigation->hdop = floatField(field, size); break;
				case 8: navigation->altitude = floatField(field, size); break;
				case 10: navigation->geoidSeparation = floatField(field, size); break;
				}
				index++;
				size = 0;
			} else
				size++;
		} else if (type == GPS_SENTENCE_GSA) {
			if (index > 16)
				break;
			if (!index)
				index = ch == ',';
			else if (index == 16) {
				if (ch == '*') {
					parse->usedCount = used;
					navigation->vdop = floatField(p - size, size);
					index++;
					size = 0;
				} else
					size++;
			} else if (ch == ',') {
				if (index == 1)
					navigation->fixType = integerField(p - size, size);
				else if (index <= 13) {
					if (size)
						parse->used[used++] = integerField(p - size, size);
				} else if (index == 14)
					navigation->pdop = floatField(p - size, size);
				else if (index == 15)
					navigation->gsaHdop = floatField(p - size, size);
				index++;
				size = 0;
			} else
				size++;
		} else if (type == GPS_SENTENCE_GSV) {
			if (index > 18)
				break;
			if (ch == ',' || (index >= 6 && (index - 6) % 4 == 0 && ch == '*')) {
				if (index == 0)
					parse->messages = integerField(p - size, size);
				else if (index == 1) {
					parse->message = integerField(p - size, size);
					if (parse->message > parse->messages)
						parse->message = 0;
					else if (parse->message == 1) {
						parse->receiving = 1;
						parse->next = 0;
						parse->satellite[0] = (GpsSatellite){ 0 };
					}
				} else if (index == 2) {
					parse->count = integerField(p - size, size);
					if (parse->count > GPS_SATELLITES_MAX)
						parse->count = GPS_SATELLITES_MAX;
				} else if ((index - 3) % 4 == 0) {
					/* each satellite: PRN, elevation, azimuth, SNR */
					if (parse->message) {
						unsigned next = parse->next;
						if (next < GPS_SATELLITES_MAX && next < parse->count) {
							satellite = &parse->satellite[next];
							satellite->prn = size ? integerField(p - size, size) : 0;
							parse->next++;
						} else
							satellite = NULL;
					}
				} else if (satellite) {
					unsigned value = size ? integerField(p - size, size) : 0;
					if ((index - 4) % 4 == 0)
						satellite->elevation = value;
					else if ((index - 5) % 4 == 0)
						satellite->azimuth = value;
					else
						satellite->snr = value;
				}
				index = ch == '*' ? 19 : index + 1;
				size = 0;
			} else
				size++;
		} else if (type == GPS_SENTENCE_VTG) {
			if (index > 8)
				break;
			if (index == 8) {
				if (ch == '*')
					index++;
			} else if (index == 0 || index == 2 || index == 6) {
				if (ch == ',') {
					*(index == 0 ? &navigation->course : index == 2 ? &navigation->magneticCourse :
					  &navigation->speed) = floatField(p - size, size);
					index++;
					size = 0;
				} else
					size++;
			} else if (ch == ',')
				index++;
		}
	}
	if (type == GPS_SENTENCE_RMC || type == GPS_SENTENCE_GGA || type == GPS_SENTENCE_GSA ||
	    type == GPS_SENTENCE_VTG)
		navigation->received.sentence[type == GPS_SENTENCE_VTG ? 3 : type - 1] = 1;
	if (type == GPS_SENTENCE_GSA)
		parse->received.sentence[0] = 1;
	if (type == GPS_SENTENCE_GSV && parse->messages == parse->message) {
		parse->receiving = 0;
		parse->received.sentence[1] = 1;
	}
	return 0;
}

int gpsParseSentence(const char *sentence, unsigned size)
{
	unsigned length = 0;
	while (length < size && sentence[length])
		length++;
	if (length == size)
		return (int)GPS_ERROR_SENTENCE;
	unsigned phase = 0, count = 0, type = 0, checksum = 0;
	const char *fields = NULL;
	int ret = 0;
	for (const char *p = sentence; *p; p++) {
		unsigned ch = (uint8_t)*p;
		if (phase > 4)
			return ret;
		if (!phase) {
			if (ch == '$') {
				phase = 1;
				checksum = 0;
			}
		} else if (phase == 1) {
			if (ch == ',') {
				if (count != 5)
					return (int)GPS_ERROR_SENTENCE;
				clearNumber();
				for (unsigned i = 0; i < 5; i++)
					s_gps.number[i] = p[(int)i - 5];
				static const char *const names[] = {"GPRMC", "GPGGA", "GPGSA", "GPGSV", "GPVTG"};
				for (unsigned i = 0; i < 5; i++)
					if (!strncmp(s_gps.number, names[i], 5)) {
						type = i + 1;
						break;
					}
				fields = p + 1;
				phase = 2;
				count = 0;
			} else
				count++;
			clearNumber();
			s_gps.number[0] = ch;
			checksum ^= ch;
		} else if (phase == 2) {
			if (ch == '*') {
				phase = 3;
				count = 0;
			} else {
				clearNumber();
				s_gps.number[0] = ch;
				checksum ^= ch;
				count++;
			}
		} else if (phase == 3) {
			if (ch == 255)
				return (int)GPS_ERROR_SENTENCE;
			clearNumber();
			s_gps.number[0] = p[0];
			s_gps.number[1] = p[1];
			if ((uint8_t)strtol(s_gps.number, NULL, 16) != checksum)
				ret = (int)GPS_ERROR_CHECKSUM;
			phase = 4;
			p++;
		}
	}
	if (ret)
		return ret;
	if (type && fields) {
		ret = parseFields(type, fields);
		if (ret)
			goto publish;
	}
publish:
	if (s_gps.navigation.received.all == RECEIVED_NAVIGATION)
		ret = gpsPublishNavigation();
	if (s_gps.satelliteParse.received.all == RECEIVED_SATELLITES)
		ret = gpsPublishSatellites();
	return ret;
}
