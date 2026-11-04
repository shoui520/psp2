/* Filename and descriptive private names are ours; inferred date component. */
#include "date.h"
#include "memory.h"
#include "format.h"

static const SslDateValue sslDateEpoch;

static const int sslDateMonthDays[2][13] = {
	{0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365},
	{0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366}
};

#define SSL_DATE_IS_LEAP_YEAR(year) \
	((year) % 400 == 0 || ((year) % 4 == 0 && (year) % 100 != 0))

typedef struct SslDatePart {
	int value;
	int width;
	SceInt8 separator;
} SslDatePart;

static const SslDatePart sslDateParsingParts[10] = {
	{0, 2, 0}, {0, 2, 0}, {0, 2, 0}, {0, 2, 0}, {0, 2, 0},
	{0, 2, 0}, {0, 2, 0}, {0, 2, 0}, {0, 2, 0}, {0, 0, 0}
};

_Static_assert(sizeof(SslDatePart) == 12, "SslDatePart");

int SceSslInternal_D3C049B9(SslLibrary *library, int parameter, void **result)
{
	SslDateModuleFactory factory;
	SslDateModuleFactory getMethods;
	const SslDateMethods *methods;
	void *data = NULL;
	int status;

	status = sslGetLibraryMethod(library, SSL_DATE_MODULE_TYPE, 0, 0, parameter,
				     SSL_LIBRARY_GET_METHOD_FACTORY, &factory);
	if (status != 0)
		return status;
	getMethods = factory;
	status = sslGetLibraryData(library, SSL_DATE_MODULE_TYPE, &data);
	if (status != 0)
		return status;
	methods = getMethods(data);
	if (methods == NULL)
		return SSL_CRYPTO_ERROR_UNSUPPORTED_METHOD;
	return methods->CreateContext(library, parameter, result);
}

int SceSslInternal_D4ADF069(void *context)
{
	SslDateContext *object = context;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->methods->DestroyContext(context);
}

int SceSslInternal_5E50CB55(void *context, void **result)
{
	SslDateContext *object = context;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->methods->CreateDate(context, result);
}

int SceSslInternal_6CDB4EE8(void *date)
{
	SslDateObject *object = date;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->DestroyDate(date);
}

int sslRetainDateObject(void *date)
{
	SslDateObject *object = date;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->RetainDate(date);
}

int SceSslInternal_04BAED82(const void *left, const void *right, int *result)
{
	const SslDateObject *object = left;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->CompareDate(left, right, result);
}

int sslAddDateObject(void *result, const void *source, int seconds)
{
	SslDateObject *object = result;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->AddDate(result, source, seconds);
}

int SceSslInternal_D1A82B1A(void *date, int format, void *data,
			  SceUInt32 *length, SceUInt32 capacity)
{
	SslDateObject *object = date;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->EncodeDate(date, format, data, length, capacity);
}

int SceSslInternal_88391E03(void *date, int format, const void *data, SceUInt32 length)
{
	SslDateObject *object = date;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	return object->context->methods->DecodeDate(date, format, data, length);
}

int sslCopyDateObject(void *date, void **result)
{
	SslDateObject *object = date;
	SslDateContext *context;
	SceUInt8 data[8];
	SceUInt32 length = 0;
	int status;

	if (object == NULL || result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	context = object->context;
	status = SceSslInternal_5E50CB55(context, result);
	if (status != 0)
		goto failed;
	status = object->context->methods->EncodeDate(date, SSL_TIME_FORMAT_SECONDS,
						    data, &length, sizeof(data));
	if (status != 0)
		goto failed;
	status = SceSslInternal_88391E03(*result, SSL_TIME_FORMAT_SECONDS, data, length);
	if (status == 0)
		return 0;
failed:
	if (*result != NULL) {
		SceSslInternal_6CDB4EE8(*result);
		*result = NULL;
	}
	return status;
}

int sslNewDateObject(void *context, void **result)
{
	SslDateObject *object;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	object = sslAllocate(sizeof(*object));
	if (object == NULL)
		return 10005;
	object->context = context;
	object->heapObject = 1;
	object->value.secondsHigh = 0;
	object->value.secondsLow = 0;
	object->value.fraction = 0;
	*result = object;
	return 0;
}

int sslFreeDateObject(void *date)
{
	SslDateObject *object = date;

	if (object != NULL && object->heapObject == 1)
		sslFree(object);
	return 0;
}

int sslAddDateValue(void *result, const void *source, int seconds)
{
	SslDateObject *object = result;
	const SslDateObject *input = source;
	const SslDateValue *value = input != NULL ? &input->value : &sslDateEpoch;
	SceUInt32 previous, sum;
	int carry = 0;

	if (object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	previous = value->secondsLow;
	sum = previous + (SceUInt32)seconds;
	object->value.secondsLow = sum;
	if (seconds > 0 && sum < previous)
		carry = 1;
	else if (seconds < 0 && sum > previous)
		carry = -1;
	object->value.secondsHigh = (SceInt32)((SceUInt32)value->secondsHigh + carry);
	return 0;
}

int sslNewDateContext(SslLibrary *library, int parameter, void **result)
{
	SslDateContext *object;
	SslDateModuleFactory factory;
	SslDateModuleFactory getMethods;
	void *data = NULL;
	int status;

	if (result == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	object = sslAllocate(sizeof(*object));
	if (object == NULL)
		return 10005;
	status = sslGetLibraryMethod(library, SSL_DATE_MODULE_TYPE, 0, 0, parameter,
				     SSL_LIBRARY_GET_METHOD_FACTORY, &factory);
	if (status != 0)
		return status;
	getMethods = factory;
	status = sslGetLibraryData(library, SSL_DATE_MODULE_TYPE, &data);
	if (status != 0)
		return status;
	object->methods = getMethods(data);
	*result = object;
	object->heapObject = 1;
	return 0;
}

int sslFreeDateContext(void *context)
{
	SslDateContext *object = context;

	if (object != NULL && object->heapObject == 1)
		sslFree(object);
	return 0;
}

int sslCompareDateValues(const void *left, const void *right, int *result)
{
	const SslDateObject *first = left, *second = right;
	const SslDateValue zero = {0};
	const SslDateValue *a = first != NULL ? &first->value : &zero;
	const SslDateValue *b = second != NULL ? &second->value : &zero;
	int comparison;

	if (a->secondsHigh != b->secondsHigh)
		comparison = a->secondsHigh > b->secondsHigh ? 1 : -1;
	else if (a->secondsLow != b->secondsLow)
		comparison = a->secondsLow > b->secondsLow ? 1 : -1;
	else if (a->fraction != b->fraction)
		comparison = a->fraction > b->fraction ? 1 : -1;
	else
		comparison = 0;
	*result = comparison;
	return 0;
}

int sslEncodeDateValue(void *date, int format, void *data,
			SceUInt32 *length, SceUInt32 capacity)
{
	SslDateObject *object = date;
	SceInt64 seconds, minutes, hours, days, years, year, previousYear;
	SceInt64 centuries, elapsedDays, yearStart, dayIndex;
	SceUInt32 high, low, magnitude, required;
	SceUInt8 *tail;
	const SceUInt8 *bytes;
	int second, minute, hour, day, month, leap, yearNumber;
	int fill, shift, i;

	if (data == NULL || object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (format == 1 || format == 2 || format == 6) {
		seconds = (SceInt64)object->value.secondsHigh * 4294967296LL +
			  object->value.secondsLow;
		second = seconds % 60;
		minutes = seconds / 60;
		minute = minutes % 60;
		hours = minutes / 60;
		hour = hours % 24;
		days = hours / 24;
		years = days * 400 / 146097;
		year = years + 1970;
		previousYear = days >= 0 ? years + 1969 : year;
		centuries = (previousYear / 4) / 25;
		elapsedDays = centuries / 4 + previousYear / 4 + 365 * years - centuries;
		yearStart = elapsedDays - 477;
		if (days < 0 && year >= 0 && !SSL_DATE_IS_LEAP_YEAR(year))
			yearStart = elapsedDays - 476;
		dayIndex = days - yearStart;
		if (dayIndex < 0) {
			year = years + 1969;
			dayIndex += SSL_DATE_IS_LEAP_YEAR(year) ? 366 : 365;
		}
		day = (SceInt32)dayIndex;
		leap = 0;
		if (!SSL_DATE_IS_LEAP_YEAR(year)) {
			if (day >= 365) {
				year++;
				day -= 365;
				if (SSL_DATE_IS_LEAP_YEAR(year))
					leap = 1;
			}
		} else if (day < 366) {
			leap = 1;
		} else {
			year++;
			day -= 366;
		}
		for (month = 1; month < 13; month++) {
			if (day < sslDateMonthDays[leap][month])
				break;
		}
		day = month < 13 ? day - sslDateMonthDays[leap][month - 1] : -1;
		yearNumber = (SceInt32)year;
		if (format == 1) {
			if (year < 0 || yearNumber > 2049 || yearNumber < 1950)
				return 10019;
			*length = 14;
			if (capacity < 14)
				return 10016;
			sslFormatString(data, capacity, "%02d%02d%02d%02d%02d%02dZ",
					yearNumber % 100, month, day + 1, hour, minute, second);
		} else if (format == 2) {
			if (year < 0 || yearNumber > 9999)
				return 10019;
			required = sslStringLength("YYYYMMDDHHMMSSZ") + 1;
			*length = required;
			if (capacity < required)
				return 10016;
			sslFormatString(data, capacity, "%04d%02d%02d%02d%02d%02dZ",
					yearNumber, month, day + 1, hour, minute, second);
		} else {
			if (year < 0 || yearNumber > 9999)
				return 10019;
			*length = 20;
			if (capacity < 20)
				return 10016;
			sslFormatString(data, capacity, "%04d:%02d:%02d:%02d:%02d:%02d",
					yearNumber, month, day + 1, hour, minute, second);
		}
		return 0;
	}
	if (format != SSL_TIME_FORMAT_SECONDS)
		return 10020;

	high = (SceUInt32)object->value.secondsHigh;
	fill = object->value.secondsHigh < 0 ? -1 : 0;
	magnitude = fill != 0 ? 0U - high : high;
	required = 8;
	for (shift = 24; shift >= 0; shift -= 8) {
		if (((magnitude >> shift) & 255) != 0)
			break;
		required--;
	}
	if (required == 4) {
		low = object->value.secondsLow;
		magnitude = fill != 0 ? 0U - low : low;
		for (shift = 24; shift >= 0; shift -= 8) {
			if (((magnitude >> shift) & 255) != 0)
				break;
			required--;
		}
	}
	if (required > capacity) {
		*length = required;
		return 10016;
	}
	sslFillMemory(data, fill, capacity - required);
	*length = capacity;
	tail = (SceUInt8 *)((SceUIntPtr)data + capacity - 1);
	bytes = (const SceUInt8 *)&object->value.secondsLow;
	i = 0;
	do {
		*tail-- = bytes[i];
		if ((SceUInt32)i == required)
			break;
		i++;
	} while (i < 4);
	if ((SceUInt32)i != required) {
		bytes = (const SceUInt8 *)&object->value.secondsHigh;
		i = 0;
		do {
			*tail-- = bytes[i];
			if ((SceUInt32)(i + 4) == required)
				break;
			i++;
		} while (i < 4);
	}
	return 0;
}

int sslDecodeDateValue(void *date, int format, const void *data, SceUInt32 length)
{
	SslDateObject *object = date;
	const SceInt8 *input = data;
	SslDatePart parts[10], *part;
	SceUInt32 position = 0, start, nextPosition, fractionPosition;
	SceUIntPtr cursor;
	SceUInt8 *bytes;
	SceInt64 year, previousYear, centuries, elapsedDays, yearStart, seconds;
	int count = 0, digits, fractionDigits, tolerant, character;
	int month, day, hour, minute, second, terminal, zoneSign = 0;
	int zoneHour = 0, zoneMinute = 0, fraction = 0;
	int zoneField, leap, beforeMonth, offset, i;
	SceInt32 limit = (SceInt32)length;

	if (data == NULL || object == NULL)
		return SSL_CRYPTO_ERROR_NULL_ARGUMENT;
	if (format == SSL_TIME_FORMAT_SECONDS) {
		if (length > 8)
			return 10022;
		cursor = (SceUIntPtr)data + length - 1;
		object->value.secondsLow = 0;
		object->value.fraction = 0;
		object->value.secondsHigh = 0;
		bytes = (SceUInt8 *)&object->value.secondsLow;
		for (i = 0; i < 4; i++) {
			bytes[i] = *(const SceUInt8 *)cursor--;
			if (cursor < (SceUIntPtr)data)
				break;
		}
		if (cursor >= (SceUIntPtr)data) {
			bytes = (SceUInt8 *)&object->value.secondsHigh;
			for (i = 0; i < 4; i++) {
				bytes[i] = *(const SceUInt8 *)cursor--;
				if (cursor < (SceUIntPtr)data)
					break;
			}
		}
		return 0;
	}
	if (format != 1 && format != 2 && format != 6)
		return 10020;
	sslCopyMemory(parts, sslDateParsingParts, sizeof(parts));
	if (format == 2) {
		parts[0].width = 4;
		parts[6].separator = '.';
	} else if (format == 6) {
		parts[0].width = 4;
		for (i = 1; i < 6; i++)
			parts[i].separator = ':';
	}
	while (count < 10) {
		part = &parts[count];
		start = position;
		if ((SceInt32)position >= limit)
			break;
		character = input[position++];
		if (character == 0)
			break;
		if (character >= '0' && character <= '9') {
			part->value = character - '0';
			digits = 1;
		} else {
			tolerant = part->separator == '.';
			if (tolerant && count == 6 && (character == '.' || character == ',')) {
				part->separator = character;
				nextPosition = start + 2;
				fractionPosition = nextPosition;
				fractionDigits = 0;
				for (;;) {
					character = input[position];
					position = nextPosition;
					start = position;
					if (character < '0')
						break;
					character = input[fractionPosition];
					position++;
					if (character > '9' || (SceInt32)position > limit ||
					    fractionDigits >= 3)
						break;
					part->value = 10 * part->value + character - '0';
					fractionDigits++;
					fractionPosition += 2;
					nextPosition = start + 2;
				}
				if (character >= '0' && character <= '9' &&
				    (SceInt32)position <= limit && fractionDigits == 3) {
					fractionPosition = position;
					do {
						start = position++;
						if (input[fractionPosition] < '0')
							break;
						character = input[fractionPosition + 1];
						fractionPosition += 2;
						position = start + 2;
						if (character > '9')
							break;
					} while ((SceInt32)position <= limit);
				}
				for (; fractionDigits < 3; fractionDigits++)
					part->value *= 10;
				position--;
				digits = part->width;
			} else {
				if (part->separator != 0 && part->separator != character && !tolerant)
					break;
				part->separator = character;
				part->value = 0;
				digits = 0;
			}
		}
		while (digits < part->width) {
			if ((SceInt32)position >= limit)
				goto parsed;
			character = input[position++];
			if (character < '0' || character > '9')
				goto parsed;
			part->value = 10 * part->value + character - '0';
			digits++;
		}
		count++;
	}
parsed:
	year = parts[0].value;
	month = parts[1].value;
	day = parts[2].value;
	hour = parts[3].value;
	minute = parts[4].value;
	second = parts[5].value;
	terminal = parts[6].separator;
	if (format == 1) {
		if (count < 5)
			return 10022;
		if (year < 50)
			year += 100;
		if (count == 5) {
			second = 0;
			terminal = parts[5].separator;
		} else if (count == 7) {
			zoneSign = parts[5].separator;
			zoneHour = parts[5].value;
			zoneMinute = parts[6].value;
		} else if (count == 8) {
			zoneSign = parts[6].separator;
			zoneHour = parts[6].value;
			zoneMinute = parts[7].value;
		}
		year += 1900;
	} else if (format == 6) {
		if (count != 6)
			return 10020;
		terminal = 'Z';
	} else {
		if (count < 6)
			return 10020;
		if (count > 6) {
			zoneField = 6;
			character = parts[6].separator;
			if (character == '.' || character == ',') {
				zoneField = 7;
				character = parts[7].separator;
				fraction = parts[6].value;
			}
			if (sslToUpper(character) != 'Z' && zoneField < count) {
				zoneSign = parts[zoneField].separator;
				if (zoneSign != '+' && zoneSign != '-')
					return 10020;
				zoneHour = parts[zoneField].value;
				zoneMinute = parts[zoneField + 1].value;
			} else {
				terminal = 'Z';
			}
		} else {
			terminal = 'Z';
		}
	}
	leap = SSL_DATE_IS_LEAP_YEAR(year);
	if (month > 12)
		return 10022;
	beforeMonth = month == 0 ? (leap ? sslDateMonthDays[0][12] : 0) :
		      sslDateMonthDays[leap][month - 1];
	if (beforeMonth + day > sslDateMonthDays[leap][month] || hour > 23 ||
	    minute > 59 || second > 60 || zoneHour > 12 || zoneMinute > 59 ||
	    (zoneSign != '+' && zoneSign != '-' && sslToUpper(terminal) != 'Z'))
		return 10022;
	previousYear = year >= 1970 ? year - 1 : year;
	centuries = (previousYear / 4) / 25;
	elapsedDays = previousYear / 4 + 365 * (year - 1970) - centuries + centuries / 4;
	yearStart = elapsedDays - 477;
	if (!leap && year >= 0 && yearStart < 0)
		yearStart = elapsedDays - 476;
	if (second == 60)
		second = 59;
	seconds = 60 * (60 * (24 * (yearStart + beforeMonth + day - 1) + hour) + minute) +
		  second;
	object->value.fraction = 0;
	object->value.secondsHigh = (SceInt32)(seconds >> 32);
	object->value.secondsLow = (SceUInt32)seconds;
	if (format == 2)
		object->value.fraction = (SceUInt32)fraction * 100 * 100 * 100;
	offset = 60 * (zoneMinute + 60 * zoneHour);
	if (zoneSign == '+')
		offset = -offset;
	sslAddDateValue(object, object, offset);
	return 0;
}

int sslRetainDateValue(void *date)
{
	(void)date;
	return 0;
}

static const SslDateMethods sslDateMethods = {
	.CreateContext = sslNewDateContext,
	.DestroyContext = sslFreeDateContext,
	.CreateDate = sslNewDateObject,
	.DestroyDate = sslFreeDateObject,
	.RetainDate = sslRetainDateValue,
	.CompareDate = sslCompareDateValues,
	.AddDate = sslAddDateValue,
	.EncodeDate = sslEncodeDateValue,
	.DecodeDate = sslDecodeDateValue
};

const SslDateMethods *sslGetDateMethods(void *data)
{
	(void)data;
	return &sslDateMethods;
}

const SslDateValue *sslGetDateEpoch(void)
{
	return &sslDateEpoch;
}
