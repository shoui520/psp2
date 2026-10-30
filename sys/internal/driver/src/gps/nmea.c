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

/* the sentences gpsParseSentence takes */
static const struct {
	unsigned type;
	const char *name;
} s_sentences[] = {
	{ GPS_SENTENCE_RMC, "GPRMC" },
	{ GPS_SENTENCE_GGA, "GPGGA" },
	{ GPS_SENTENCE_GSA, "GPGSA" },
	{ GPS_SENTENCE_GSV, "GPGSV" },
	{ GPS_SENTENCE_VTG, "GPVTG" },
};

static GpsNavigation s_navigation;
static GpsSatelliteParse s_satelliteParse;
static union {
	char number[0x20];			/* a field being converted */
	unsigned numberWord[8];
} s_field;

int gpsResetParser(unsigned what)
{
	if (what == PARSER_NAVIGATION)
		memset(&s_navigation, 0, sizeof(s_navigation));
	else if (what == PARSER_SATELLITES)
		memset(&s_satelliteParse, 0, sizeof(s_satelliteParse));
	else if (what == PARSER_ALL) {
		memset(&s_navigation, 0, sizeof(s_navigation));
		memset(&s_satelliteParse, 0, sizeof(s_satelliteParse));
	}
	return 0;
}

/* Sony inlines these where noted and keeps a copy for the other callers */
#define INLINE static inline __attribute__((always_inline))
#define OUTLINE static __attribute__((noinline))

INLINE int white(unsigned c)
{
	return c == ' ' || (c >= '\t' && c <= '\r');
}

INLINE unsigned integerValue(const char *value)
{
	unsigned number = 0;

	while (white((uint8_t)*value))
		value++;
	while ((uint8_t)*value - '0' < 10U)
		number = number * 10 + (uint8_t)*value++ - '0';
	return number;
}

INLINE float floatValue(const char *value)
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

OUTLINE float parseFloat(const char *value)
{
	return floatValue(value);
}

OUTLINE unsigned parseInteger(const char *value)
{
	return integerValue(value);
}

INLINE void clearNumber(void)
{
	for (unsigned i = 0; i < 8; i++)
		s_field.numberWord[i] = 0;
}

INLINE void copyNumber(const char *field, unsigned size)
{
	memcpy(s_field.number, field, size);
}

OUTLINE unsigned integerField(const char *field, unsigned size)
{
	if (size >= 32)
		return 0;
	clearNumber();
	if (!field || !size)
		return 0;
	copyNumber(field, size);
	return integerValue(s_field.number);
}

OUTLINE float floatField(const char *field, unsigned size)
{
	if (size >= 32)
		return 0.0f;
	clearNumber();
	if (field && size)
		copyNumber(field, size);
	return floatValue(s_field.number);
}

INLINE const char *copyField(const char *field, unsigned size)
{
	if (size >= 32)
		return NULL;
	clearNumber();
	if (field && size)
		copyNumber(field, size);
	return s_field.number;
}

INLINE void parseTime(const char *field, unsigned size)
{
	const char *number = copyField(field, size);
	/* Sony assumes a field shorter than the scratch buffer here. */
	char part[8] = {0, 0, 0, 0, 0, 0, 0, 0};
	for (unsigned i = 0; i < 3; i++) {
		part[0] = number[i * 2];
		part[1] = number[i * 2 + 1];
		s_navigation.timePart[3 + i] = parseInteger(part);	/* hour, minute, second */
	}
	for (unsigned i = 0; i < 4; i++)
		part[i] = number[6 + i];
	float microseconds = parseFloat(part) * 1000.0f * 1000.0f;
	s_navigation.time.microsecond = (unsigned)microseconds;
}

INLINE int parseDate(const char *field, unsigned size)
{
	if (size != 6)
		return (int)GPS_ERROR_SENTENCE;
	clearNumber();
	for (unsigned i = 0; i < 6; i++)
		s_field.number[i] = field[i];
	char part[4] = {0, 0, 0, 0};
	for (unsigned i = 0; i < 3; i++) {
		part[0] = s_field.number[2 * i];
		part[1] = s_field.number[2 * i + 1];
		/* DDMMYY: day, month, year */
		s_navigation.timePart[2 - i] = parseInteger(part) + (i == 2 ? 2000 : 0);
	}
	return 0;
}

INLINE void coordinate(const char *field, unsigned size, unsigned degrees)
{
	const char *number = copyField(field, size);
	char part[4] = {0, 0, 0, 0};
	for (unsigned i = 0; i < degrees; i++)
		part[i] = number[i];
	/* latitude is DDMM.MMMM, longitude DDDMM.MMMM */
	*(degrees == 2 ? &s_navigation.latitudeDegrees : &s_navigation.longitudeDegrees) =
		parseInteger(part);
	*(degrees == 2 ? &s_navigation.latitudeMinutes : &s_navigation.longitudeMinutes) =
		(double)parseFloat(number + degrees);
}

INLINE int parseFields(unsigned type, const char *start)
{
	unsigned index = 0, size = 0, used = 0;
	GpsNavigation *navigation = &s_navigation;
	GpsSatelliteParse *parse = &s_satelliteParse;
	GpsSatellite *satellite = NULL;
	const char *number;

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
					/* Sony reads the hemisphere even past a field too long to copy */
					number = copyField(field, size);
					if ((size > 31 || (field && size)) && *number == 'S')
						navigation->latitudeDegrees = -navigation->latitudeDegrees;
					break;
				case 3: coordinate(field, size, 3); break;
				case 4:
					number = copyField(field, size);
					if ((size > 31 || (field && size)) && *number == 'W')
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
	unsigned length;

	/* Sony reads up to the byte after the buffer */
	for (length = 0; sentence[length]; length++)
		if (length == size)
			return (int)GPS_ERROR_SENTENCE;
	if (length >= size)
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
					s_field.number[i] = p[(int)i - 5];
				for (unsigned i = 0; i < sizeof(s_sentences) / sizeof(s_sentences[0]); i++)
					if (!strncmp(s_field.number, s_sentences[i].name, 5)) {
						type = s_sentences[i].type;
						break;
					}
				fields = p + 1;
				phase = 2;
				count = 0;
			} else
				count++;
			clearNumber();
			s_field.number[0] = ch;
			checksum ^= ch;
		} else if (phase == 2) {
			if (ch == '*') {
				phase = 3;
				count = 0;
			} else {
				clearNumber();
				s_field.number[0] = ch;
				checksum ^= ch;
				count++;
			}
		} else if (phase == 3) {
			if (ch == 255)
				return (int)GPS_ERROR_SENTENCE;
			clearNumber();
			s_field.number[0] = p[0];
			s_field.number[1] = p[1];
			if (strtol(s_field.number, NULL, 16) != (long)checksum)
				ret = (int)GPS_ERROR_CHECKSUM;
			phase = 4;
			p++;
		} else
			p++;		/* Sony skips the rest of the sentence two characters at a time */
	}
	if (ret)
		return ret;
	if (type && fields)
		ret = parseFields(type, fields);
	if (s_navigation.received.all == RECEIVED_NAVIGATION)
		ret = gpsPublishNavigation(&s_navigation);
	if (s_satelliteParse.received.all == RECEIVED_SATELLITES)
		ret = gpsPublishSatellites(&s_satelliteParse);
	return ret;
}
