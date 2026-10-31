/*
 * The rest of HttpFile's readers: the dates of its responses (RFC 1123, RFC 850
 * and asctime forms, any time zone), its connection pool and reader
 * configuration, and the reader that asks for byte ranges
 * (HttpFileRangeReader) while the server answers them.
 */
#include <kernel/iofilemgr.h>
#include <rtc.h>
#include <file/httpfile.h>
#include <rtc/rtc.h>
#include <std/stdlib.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/ctype.h>
#include "httpfile_reader.h"

namespace paf {

static const char httpDateMonths[] =
	"JanuaryFebruaryMarchAprilMayJuneJulyAugustSeptemberOctoberNovemberDecember";
static const uint8_t httpDateMonthLengths[] = {
	7, 8, 5, 5, 3, 4, 4, 6, 9, 7, 8, 8
};
static const char httpDateWeekdays[] =
	"SundayMondayTuesdayWednesdayThursdayFridaySaturday";
static const uint8_t httpDateWeekdayLengths[] = { 6, 6, 7, 9, 8, 6, 8 };

/* A time zone a date may name, and its offset from UTC. */
class HttpDateTimeZone {
public:
	const char *name;
	uint8_t length;
	int16_t minutes;
};

#define HTTP_TIME_ZONE(name, minutes) { name, sizeof(name) - 1, minutes }

static const HttpDateTimeZone httpDateTimeZones[] = {
	HTTP_TIME_ZONE("GMT", 0),
	HTTP_TIME_ZONE("EST", -300),
	HTTP_TIME_ZONE("EDT", -240),
	HTTP_TIME_ZONE("CST", -360),
	HTTP_TIME_ZONE("CDT", -300),
	HTTP_TIME_ZONE("MST", -420),
	HTTP_TIME_ZONE("MDT", -360),
	HTTP_TIME_ZONE("PST", -480),
	HTTP_TIME_ZONE("PDT", -420),
	HTTP_TIME_ZONE("NZDT", 780),
	HTTP_TIME_ZONE("NZST", 720),
	HTTP_TIME_ZONE("IDLE", 720),
	HTTP_TIME_ZONE("NZT", 720),
	HTTP_TIME_ZONE("AESST", 660),
	HTTP_TIME_ZONE("ACSST", 630),
	HTTP_TIME_ZONE("CADT", 630),
	HTTP_TIME_ZONE("SADT", 630),
	HTTP_TIME_ZONE("AEST", 600),
	HTTP_TIME_ZONE("EAST", 600),
	HTTP_TIME_ZONE("GST", 600),
	HTTP_TIME_ZONE("LIGT", 600),
	HTTP_TIME_ZONE("ACST", 570),
	HTTP_TIME_ZONE("SAST", 570),
	HTTP_TIME_ZONE("CAST", 570),
	HTTP_TIME_ZONE("AWSST", 540),
	HTTP_TIME_ZONE("JST", 540),
	HTTP_TIME_ZONE("KST", 540),
	HTTP_TIME_ZONE("WDT", 540),
	HTTP_TIME_ZONE("MT", 510),
	HTTP_TIME_ZONE("AWST", 480),
	HTTP_TIME_ZONE("CCT", 480),
	HTTP_TIME_ZONE("WADT", 480),
	HTTP_TIME_ZONE("WST", 480),
	HTTP_TIME_ZONE("JT", 450),
	HTTP_TIME_ZONE("WAST", 420),
	HTTP_TIME_ZONE("IT", 210),
	HTTP_TIME_ZONE("BT", 180),
	HTTP_TIME_ZONE("EETDST", 180),
	HTTP_TIME_ZONE("EET", 120),
	HTTP_TIME_ZONE("CETDST", 120),
	HTTP_TIME_ZONE("FWT", 120),
	HTTP_TIME_ZONE("IST", 120),
	HTTP_TIME_ZONE("MEST", 120),
	HTTP_TIME_ZONE("METDST", 120),
	HTTP_TIME_ZONE("SST", 120),
	HTTP_TIME_ZONE("BST", 60),
	HTTP_TIME_ZONE("CET", 60),
	HTTP_TIME_ZONE("DNT", 60),
	HTTP_TIME_ZONE("FST", 60),
	HTTP_TIME_ZONE("MET", 60),
	HTTP_TIME_ZONE("MEWT", 60),
	HTTP_TIME_ZONE("MEZ", 60),
	HTTP_TIME_ZONE("NOR", 60),
	HTTP_TIME_ZONE("SET", 60),
	HTTP_TIME_ZONE("SWT", 60),
	HTTP_TIME_ZONE("WETDST", 60),
	HTTP_TIME_ZONE("WET", 0),
	HTTP_TIME_ZONE("WAT", -60),
	HTTP_TIME_ZONE("NDT", -90),
	HTTP_TIME_ZONE("ADT", -180),
	HTTP_TIME_ZONE("NFT", -150),
	HTTP_TIME_ZONE("NST", -150),
	HTTP_TIME_ZONE("AST", -240),
	HTTP_TIME_ZONE("YDT", -480),
	HTTP_TIME_ZONE("HDT", -540),
	HTTP_TIME_ZONE("YST", -540),
	HTTP_TIME_ZONE("AHST", -600),
	HTTP_TIME_ZONE("CAT", -600),
	HTTP_TIME_ZONE("NT", -660),
	HTTP_TIME_ZONE("IDLW", -720),
};

#undef HTTP_TIME_ZONE


/* Compares the letters of two names, ignoring case: <0, 0 or >0. */
static inline __attribute__((always_inline))
int HttpDateCompare(const char *left, const char *right, int length)
{
	for (; length > 0; --length) {
		int l = *left++ & 0xdf;
		int r = *right++ & 0xdf;

		if (l != r)
			return l < r ? -1 : 1;
	}
	return 0;
}

static const char *HttpFileParseDateMonth(const char *text, int32_t *month)
{
	const char *name = httpDateMonths;

	for (int i = 0; i < 12; ++i) {
		int length = httpDateMonthLengths[i];

		if (HttpDateCompare(name, text, length) == 0) {
			*month = i + 1;
			return text + length;
		}
		if (HttpDateCompare(name, text, 3) == 0) {
			*month = i + 1;
			return text + 3;
		}
		name += length;
	}
	return NULL;
}

static inline __attribute__((always_inline))
bool HttpDateIsDigit(int ch)
{
	return ch >= '0' && ch <= '9';
}

/* One or two digits. */
static inline __attribute__((always_inline))
const char *HttpDateReadNumber(const char *text, int32_t *number)
{
	int first = (signed char)text[0];
	int second = (signed char)text[1];

	if (!HttpDateIsDigit(first)) {
		*number = -1;
		return NULL;
	}
	if (HttpDateIsDigit(second)) {
		*number = 10 * first + second - 11 * '0';
		return text + 2;
	}
	*number = first - '0';
	return text + 1;
}

/* Four digits, or -1. */
static inline __attribute__((always_inline))
int32_t HttpDateReadYear(const char *text)
{
	int a = (signed char)text[0];
	int b = (signed char)text[1];
	int c = (signed char)text[2];
	int d = (signed char)text[3];

	if (!HttpDateIsDigit(a) || !HttpDateIsDigit(b) || !HttpDateIsDigit(c) || !HttpDateIsDigit(d))
		return -1;
	return 1000 * a + 100 * b + 10 * c + d - 1111 * '0';
}

static int32_t HttpFileParseDateTick(rtc::Tick *tick, const char *text)
{
	rtc::DateTime date;
	const char *name = httpDateWeekdays;
	const char *next = NULL;
	int32_t number;
	int32_t offset = 0;
	int i;

	while (*text == ' ' || *text == '\t')
		++text;
	for (i = 0; i < 7; ++i) {
		int length = httpDateWeekdayLengths[i];

		if (HttpDateCompare(name, text, length) == 0) {
			next = text + length;
			break;
		}
		if (HttpDateCompare(name, text, 3) == 0) {
			next = text + 3;
			break;
		}
		name += length;
	}
	if (next == NULL)
		return SCE_RTC_ERROR_BAD_PARSE;
	if (*next == ',')
		++next;
	while (*next == ' ' || *next == '\t')
		++next;
	text = next;
	next = HttpFileParseDateMonth(text, &number);
	if (next != NULL) {
		/* asctime: "Sun Nov  6 08:49:37 1994" */
		date.month = number;
		if (*next != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		if (next[1] == ' ') {
			number = (signed char)next[2] - '0';
			if (number > 9)
				return SCE_RTC_ERROR_BAD_PARSE;
			next += 3;
		} else {
			next = HttpDateReadNumber(next + 1, &number);
			if (next == NULL)
				return SCE_RTC_ERROR_BAD_PARSE;
		}
		date.day = number;
		if (*next != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (next == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.hour = number;
		if (*next != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (next == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.minute = number;
		if (*next != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (next == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.second = number;
		if (*next != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		number = HttpDateReadYear(next + 1);
		if (number < 0)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.year = number;
	} else {
		/* RFC 1123 "Sun, 06 Nov 1994 08:49:37 GMT", RFC 850 "Sunday, 06-Nov-94 ..." */
		next = HttpDateReadNumber(text, &number);
		if (next == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.day = number;
		if (*next != ' ' && *next != '-')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpFileParseDateMonth(next + 1, &number);
		if (next == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.month = number;
		if (*next != ' ' && *next != '-')
			return SCE_RTC_ERROR_BAD_PARSE;
		++next;
		number = HttpDateReadYear(next);
		if (number < 0) {
			int a = (signed char)next[0];
			int b = (signed char)next[1];

			if (!HttpDateIsDigit(a) || !HttpDateIsDigit(b))
				return SCE_RTC_ERROR_BAD_PARSE;
			number = 10 * a + b - 11 * '0';
			number += number < 50 ? 2000 : 1900;
			next += 2;
		} else {
			next += 4;
		}
		date.year = number;
		if (*next != ' ')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (next == NULL || number > 25)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.hour = number;
		if (*next != ':')
			return SCE_RTC_ERROR_BAD_PARSE;
		next = HttpDateReadNumber(next + 1, &number);
		if (next == NULL)
			return SCE_RTC_ERROR_BAD_PARSE;
		date.minute = number;
		if (*next == ' ') {
			date.second = 0;
			++next;
		} else {
			if (*next != ':')
				return SCE_RTC_ERROR_BAD_PARSE;
			next = HttpDateReadNumber(next + 1, &number);
			if (next == NULL)
				return SCE_RTC_ERROR_BAD_PARSE;
			date.second = number;
		}
		if (*next == ' ') {
			int ch = (signed char)next[1];

			if (ch == '+' || ch == '-') {
				number = HttpDateReadYear(next + 2);
				offset = number % 60 + 60 * (number / 100);
				if (ch == '-')
					offset = -offset;
			} else if (ch != 'U' && next[2] != 'T') {
				for (i = 0; i < 70; ++i) {
					const HttpDateTimeZone *zone = &httpDateTimeZones[i];

					if (HttpDateCompare(zone->name, next + 1, zone->length) == 0) {
						offset = zone->minutes;
						break;
					}
				}
				if (i == 70) {
					/* A military zone letter. */
					ch = (signed char)(next[1] & 0xdf);
					if ((ch >= 'A' && ch <= 'I') || (ch >= 'K' && ch <= 'M'))
						offset = 60 * (ch - 'A');
					else if (ch >= 'N' && ch <= 'Y')
						offset = 60 * ('N' - ch);
					else if (ch != 'Z')
						return SCE_RTC_ERROR_BAD_PARSE;
				}
			}
		}
	}
	date.microsecond = 0;
	rtc::ConvertDateTimeToTick(&date, tick);
	rtc::TickAddMinutes(tick, tick, -offset);
	return 0;
}

datetime::DateTime HttpFileParseDate(const char *text)
{
	rtc::Tick ignored;
	rtc::Tick tick;
	rtc::DateTime date;

	HttpFileParseDateTick(&ignored, text);
	HttpFileParseDateTick(&tick, text);
	rtc::ConvertTickToDateTime(&tick, &date, NULL);
	return datetime::DateTime(date.year, date.month, date.day, date.hour, date.minute,
				  date.second, date.microsecond);
}

HttpFileReaderContext::HttpFileReaderContext(size_t capacity)
	: m_capacity(capacity), m_mutex("ScePafHttpConnectionPool", 0)
{
}

HttpFileReaderConfig::HttpFileReaderConfig(const HttpFileReaderParam &param)
	: scheme(param.scheme.c_str()), host(param.host.c_str()), port(param.port),
	  method(param.method.c_str()), version(param.version.c_str()), uri(param.uri.c_str()),
	  user(param.user.c_str()), passwd(param.passwd.c_str()), header(param.header),
	  msg_body(param.msg_body), open_flag(param.open_flag), https_flag(param.https_flag),
	  timeout(param.timeout), accept_encoding_gzip(param.accept_encoding_gzip),
	  context(param.context), template_option(param.template_option),
	  response_header_max_size(param.response_header_max_size)
{
}

common::SharedPtr<HttpFileReaderConfig> HttpFileReaderConfig::Create(
	const HttpFileReaderParam &param)
{
	return common::SharedPtr<HttpFileReaderConfig>(new HttpFileReaderConfig(param));
}

common::SharedPtr<HttpFileReaderConfig> HttpFileReaderConfig::Clone(
	common::SharedPtr<HttpFileReaderConfig> src)
{
	HttpFileReaderConfig *config = new HttpFileReaderConfig();

	if (config != NULL)
		*config = *src;
	return common::SharedPtr<HttpFileReaderConfig>(config);
}

bool HttpFileReaderConfig::IsHttpsCheckEnabled() const
{
	return !(open_flag & HttpFile::OpenArg::DISABLE_HTTPS_CHECK);
}

bool HttpFileReaderConfig::IsProxyIgnored() const
{
	return (open_flag & HttpFile::OpenArg::IGNORE_PROXY_SETTING) != 0;
}

HttpFileRangeReader::HttpFileRangeReader(common::SharedPtr<HttpFileReaderConfig> config)
	: HttpFileReader(config), m_range_enabled(true), m_reader(NULL), m_position(0),
	  m_range_begin(-1), m_range_end(-1), m_content_length(SCE_PAF_ERROR_FILE_UNKNOWN_SIZE)
{
}

HttpFileRangeReader::~HttpFileRangeReader()
{
	if (m_reader != NULL)
		delete m_reader;
	m_reader = NULL;
}

int32_t HttpFileRangeReader::Open()
{
	common::SharedPtr<HttpFileReaderConfig> config;
	int32_t ret;

	m_position = 0;
	m_range_begin = -1;
	m_range_end = -1;
	m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	if (m_range_enabled) {
		/* Ask first whether the server answers ranges at all. */
		config = HttpFileReaderConfig::Clone(m_config);
		ScePafMisc_E21E796D(&config->header, "Range");
		config->method = "HEAD";
		m_reader = new HttpFileHttpReader(config);
		if (m_reader == NULL)
			return SCE_PAF_ERROR_FILE_NO_MEMORY;
		ret = m_reader->Open();
		m_result[0] = m_reader->m_result[0];
		m_result[1] = m_reader->m_result[1];
		m_result[2] = m_reader->m_result[2];
		if (ret != 0) {
			delete m_reader;
			m_reader = NULL;
			return ret;
		}
		m_content_length = m_reader->GetFileSize();

		HttpResponse *response = m_reader->GetResponse();

		if (response->code == 200 && response->major == 1 && response->minor == 1)
			return 0;
		m_range_enabled = false;
		m_reader->Close();
		delete m_reader;
		m_reader = NULL;
	}
	config = HttpFileReaderConfig::Clone(m_config);
	ScePafMisc_E21E796D(&config->header, "Range");
	m_reader = new HttpFileHttpReader(config);
	if (m_reader == NULL)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	ret = m_reader->Open();
	m_result[0] = m_reader->m_result[0];
	m_result[1] = m_reader->m_result[1];
	m_result[2] = m_reader->m_result[2];
	if (ret != 0) {
		delete m_reader;
		m_reader = NULL;
	}
	return ret;
}

int32_t HttpFileRangeReader::Close()
{
	int32_t ret = 0;

	if (m_reader != NULL) {
		ret = m_reader->Close();
		delete m_reader;
		m_reader = NULL;
	}
	m_position = 0;
	m_range_begin = -1;
	m_range_end = -1;
	m_content_length = SCE_PAF_ERROR_FILE_UNKNOWN_SIZE;
	return ret;
}

/* "bytes <begin>-<end>/<length>": the range a 206 response holds. */
static inline __attribute__((always_inline))
int32_t HttpFileRangeReaderParseContentRange(HttpFileRangeReader *reader, const char *value)
{
	if (value == NULL)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

	int32_t len = (int32_t)sce_paf_strlen(value);
	int32_t pos = 0;
	uint64_t end = 0;

	reader->m_range_end = -1;
	reader->m_range_begin = -1;
	while (pos < len && (sce_paf_look_ctype_table((signed char)value[pos]) & 8))
		pos++;

	size_t prefix_len = sce_paf_strlen("bytes ");

	if ((int32_t)((uint32_t)pos + prefix_len) > len ||
	    sce_paf_memcmp(value + pos, "bytes ", prefix_len) != 0)
		return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
	pos = (int32_t)((uint32_t)pos + prefix_len);
	reader->m_range_begin = 0;
	while (pos < len) {
		if ((signed char)value[pos] == '-') {
			pos++;
			break;
		}
		if (!(sce_paf_look_ctype_table((signed char)value[pos]) & 4))
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

		uint64_t begin = (uint64_t)reader->m_range_begin * 10;

		reader->m_range_begin = begin;
		pos++;
		reader->m_range_begin = begin + (int64_t)((signed char)value[pos - 1] - '0');
	}
	reader->m_range_end = 0;
	while (pos < len) {
		if ((signed char)value[pos] == '/') {
			reader->m_range_end = end + 1;
			return 0;
		}
		if (!(sce_paf_look_ctype_table((signed char)value[pos]) & 4))
			return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;

		uint64_t digits = (uint64_t)reader->m_range_end * 10;

		reader->m_range_end = digits;
		pos++;
		end = digits + (int64_t)((signed char)value[pos - 1] - '0');
		reader->m_range_end = end;
	}
	return SCE_PAF_ERROR_FILE_INVALID_ARGUMENT;
}

int32_t HttpFileRangeReader::Read(void *buf, size_t nbyte)
{
	char *dst = (char *)buf;
	int32_t total = 0;
	int32_t ret;

	if (nbyte == 0)
		return 0;

	int64_t position = m_position;

	if (position == m_content_length)
		return 0;
	if (m_range_enabled) {
		if (m_reader != NULL && m_range_begin <= position && position < m_range_end) {
			/* Within the range already asked for. */
			int32_t available = (uint64_t)m_range_end - (uint64_t)position;
			int32_t remaining = (int32_t)nbyte;

			if (available < remaining)
				remaining = available;
			if ((uint32_t)remaining > nbyte)
				return SCE_PAF_ERROR_FILE_INVALID_OFFSET;
			while (remaining > 0) {
				ret = m_reader->Read(dst, (uint32_t)remaining);
				m_result[0] = m_reader->m_result[0];
				m_result[1] = m_reader->m_result[1];
				m_result[2] = m_reader->m_result[2];
				if (ret < 0)
					return ret;
				if (ret == 0)
					return total;
				m_position = (uint64_t)m_position + (uint64_t)(int64_t)ret;
				dst += ret;
				total += ret;
				remaining = (uint32_t)remaining - (uint32_t)ret;
			}
			return total;
		}
		if (m_reader != NULL) {
			m_reader->Close();
			delete m_reader;
			m_reader = NULL;
		}
		{
			common::SharedPtr<HttpFileReaderConfig> config =
				HttpFileReaderConfig::Clone(m_config);
			char range[64] = { 0 };

			if (FormatRange(range, sizeof(range), nbyte))
				ScePafMisc_1156F852(&config->header, "Range", range, true);
			m_reader = new HttpFileHttpReader(config);
			if (m_reader == NULL)
				return SCE_PAF_ERROR_FILE_NO_MEMORY;
			ret = m_reader->Open();
			m_result[0] = m_reader->m_result[0];
			m_result[1] = m_reader->m_result[1];
			m_result[2] = m_reader->m_result[2];
			if (ret != 0)
				goto error;

			switch (m_reader->GetResponse()->code) {
			case 206: {
				ret = HttpFileRangeReaderParseContentRange(
					this, ScePafMisc_82171187(m_reader->GetResponseHeaders(),
								  "Content-Range", 0));
				if (ret != 0) {
					ret = SCE_PAF_ERROR_FILE_INVALID_CONTENT_RANGE;
					goto error;
				}
				position = m_position;

				int64_t begin = m_range_begin;

				if (begin > position || position >= m_range_end) {
					ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
					goto error;
				}

				int64_t end = m_range_end;

				if (begin < position) {
					/* The server started the range before where we are. */
					int64_t skipped = Seek((uint64_t)position - (uint64_t)begin,
							       SCE_SEEK_CUR);

					position = m_position;
					end = m_range_end;
					if (skipped < 0)
						goto error;
				}

				int64_t available = (uint64_t)end - (uint64_t)position;
				size_t count = nbyte;

				if ((int64_t)(uint64_t)nbyte > available)
					count = (uint32_t)available;
				if (count > nbyte)
					return SCE_PAF_ERROR_FILE_INVALID_OFFSET;
				ret = m_reader->Read(dst, count);
				m_result[0] = m_reader->m_result[0];
				m_result[1] = m_reader->m_result[1];
				m_result[2] = m_reader->m_result[2];
				if (ret < 0)
					goto error;
				m_position = (uint64_t)m_position + (uint64_t)(int64_t)ret;
				return ret;
			}
			case 200: {
				/* No ranges: read the whole body from here on. */
				position = m_position;
				m_range_enabled = false;
				m_position = 0;

				int64_t skipped = Seek(position, SCE_SEEK_SET);

				if (skipped < 0) {
					ret = (int32_t)skipped;
					goto error;
				}
				break;
			}
			case 416:
				return 0;
			default:
				ret = SCE_PAF_ERROR_FILE_INVALID_OFFSET;
				goto error;
			}
		}
	}
	if (m_reader == NULL)
		return SCE_PAF_ERROR_FILE_NO_MEMORY;
	ret = m_reader->Read(dst, nbyte);
	m_result[0] = m_reader->m_result[0];
	m_result[1] = m_reader->m_result[1];
	m_result[2] = m_reader->m_result[2];
	return ret;

error:
	if (m_reader != NULL) {
		m_reader->Close();
		delete m_reader;
		m_reader = NULL;
	}
	return ret;
}

int64_t HttpFileRangeReader::Seek(int64_t offset, int32_t whence)
{
	int64_t position;

	if (!m_range_enabled) {
		if (m_reader == NULL)
			return SCE_PAF_ERROR_FILE_NO_MEMORY;

		int32_t ret = (int32_t)m_reader->Seek(offset, whence);

		m_result[0] = m_reader->m_result[0];
		m_result[1] = m_reader->m_result[1];
		m_result[2] = m_reader->m_result[2];
		return ret;
	}
	switch (whence) {
	case SCE_SEEK_SET:
		position = offset;
		break;
	case SCE_SEEK_CUR:
		position = (uint64_t)m_position + (uint64_t)offset;
		break;
	default:
		return SCE_PAF_ERROR_FILE_NOT_SUPPORTED;
	}
	if (m_reader != NULL) {
		if (m_range_begin < 0 || m_range_end <= m_range_begin || m_position > position ||
		    position >= m_range_end) {
			m_reader->Close();
			delete m_reader;
			m_reader = NULL;
		} else {
			int32_t ret = (int32_t)m_reader->Seek((uint64_t)position - (uint64_t)m_position,
							      SCE_SEEK_CUR);

			m_result[0] = m_reader->m_result[0];
			m_result[1] = m_reader->m_result[1];
			m_result[2] = m_reader->m_result[2];
			if (ret >= 0)
				m_position = ret;
			return position;
		}
	}
	m_position = position;
	m_range_begin = -1;
	m_range_end = -1;
	return position;
}

int32_t HttpFileRangeReader::Abort()
{
	if (m_reader != NULL)
		return m_reader->Abort();
	return 0;
}

int64_t HttpFileRangeReader::GetFileSize()
{
	if (m_range_enabled)
		return m_content_length;
	if (m_reader != NULL)
		return m_reader->GetFileSize();
	return -1;
}

bool HttpFileRangeReader::FormatRange(char *dst, size_t max, size_t nbyte)
{
	uint32_t flags = 0;

	if (dst == NULL || max < 64)
		return false;
	if (m_config.get() != NULL)
		flags = m_config->open_flag;
	if (flags & HttpFile::OpenArg::ENABLE_RANGE_GET) {
		int64_t position = m_position;
		int64_t end = (uint64_t)position + (uint64_t)nbyte - 1;

		sce_paf_snprintf(dst, max, "bytes=%lld-%lld", position, end);
		return true;
	}
	if (!(flags & HttpFile::OpenArg::ENABLE_HALF_RANGE_GET))
		return false;
	sce_paf_snprintf(dst, max, "bytes=%lld-", m_position);
	return true;
}

}
