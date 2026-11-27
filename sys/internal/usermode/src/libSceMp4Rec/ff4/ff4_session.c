/*
 * Sony FF4 MP4 library: session management, MP4 epoch date conversion,
 * 128-bit arithmetic, and I/O + memory callback wrappers.
 */
#include "ff4_internal.h"

static const uint8_t s_daysInMonth[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int32_t is_leap_year(uint64_t year)
{
	if ((year % 400u) == 0u)
		return 1;
	if ((year % 100u) == 0u)
		return 0;
	return ((year & 3u) == 0u) ? 1 : 0;
}

int32_t Ff4CreateSession(const Ff4Callbacks *cb, Ff4Session *outSession)
{
	Ff4SessionImpl *sess;

	if (cb == NULL || outSession == NULL)
		return FF4_ERR_INVALID_ARG;

	sess = (Ff4SessionImpl *)cb->memAlloc(cb->allocUser, sizeof(Ff4SessionImpl));
	if (sess == NULL)
		return FF4_ERR_NO_MEM;

	cb->memSet(cb->allocUser, sess, 0, sizeof(Ff4SessionImpl));
	cb->memCpy(cb->allocUser, &sess->cb, sizeof(Ff4Callbacks), cb, sizeof(Ff4Callbacks));
	*outSession = sess;
	return FF4_OK;
}

int32_t Ff4DestroySession(Ff4Session session)
{
	if (session != NULL)
		ff4_free((Ff4SessionImpl *)session, session);
	return FF4_OK;
}

int32_t ff4_convert_mp4time_to_date(Ff4SessionImpl *sess, uint64_t mp4Time, Ff4DateTime *outDate)
{
	Ff4DateTime dt;
	uint8_t dim[12];
	uint64_t sec;
	uint64_t min;
	uint64_t hour;
	uint64_t days;
	uint64_t cycles400;
	uint64_t remDays;
	uint32_t yearOff;
	uint32_t month;
	uint64_t fullYear;
	int32_t leap;

	if (sess == NULL || outDate == NULL)
		return FF4_ERR_INVALID_ARG;

	for (uint32_t i = 0; i < 12u; i++)
		dim[i] = s_daysInMonth[i];

	sec = mp4Time % 60u;
	mp4Time /= 60u;
	min = mp4Time % 60u;
	mp4Time /= 60u;
	hour = mp4Time % 24u;
	days = mp4Time / 24u;

	cycles400 = days / 146097u;
	remDays = days % 146097u;

	yearOff = 0;
	leap = 0;
	while (yearOff < 400u) {
		uint32_t y = yearOff + 1904u;
		uint32_t yDays;
		leap = is_leap_year(y);
		yDays = leap ? 366u : 365u;
		if (remDays < yDays)
			break;
		remDays -= yDays;
		yearOff++;
	}

	fullYear = cycles400 * 400u + yearOff + 1904u;
	if ((fullYear >> 16) != 0u)
		return FF4_ERR_OVERFLOW;

	month = 0;
	while (month < 12u) {
		uint64_t mDays = dim[month];
		if (month == 1u && leap != 0)
			mDays++;
		if (remDays < mDays)
			break;
		remDays -= mDays;
		month++;
	}

	ff4_memset(sess, &dt, 0, sizeof(dt));
	dt.year = (uint16_t)fullYear;
	dt.month = (uint16_t)(month + 1u);
	dt.day = (uint16_t)(remDays + 1u);
	dt.hour = (uint16_t)hour;
	dt.minute = (uint16_t)min;
	dt.second = (uint16_t)sec;
	ff4_memcpy(sess, outDate, sizeof(dt), &dt, sizeof(dt));
	return FF4_OK;
}

int32_t Ff4ConvertDateToMp4Time(Ff4Session session, const uint16_t *dateFields, uint64_t *outMp4Time)
{
	Ff4SessionImpl *sess = (Ff4SessionImpl *)session;
	Ff4DateTime dt;
	uint8_t dim[12];
	uint64_t days;
	uint64_t remYears;
	uint64_t i;
	uint64_t maxDay;
	int32_t leap;

	if (sess == NULL || dateFields == NULL || outMp4Time == NULL)
		return FF4_ERR_INVALID_ARG;

	for (uint32_t j = 0; j < 12u; j++)
		dim[j] = s_daysInMonth[j];

	ff4_memcpy(sess, &dt, sizeof(dt), dateFields, sizeof(dt));

	if (dt.year < 1904u)
		return FF4_ERR_INVALID_ARG;
	if (dt.month < 1u || dt.month > 12u)
		return FF4_ERR_INVALID_ARG;
	if (dt.day < 1u || dt.day > 31u)
		return FF4_ERR_INVALID_ARG;
	if (dt.hour > 23u || dt.minute > 59u || dt.second > 59u)
		return FF4_ERR_INVALID_ARG;

	{
		uint32_t yOff = (uint32_t)(dt.year - 1904u);
		days = (uint64_t)(yOff / 400u) * 146097u;
		remYears = yOff % 400u;
	}

	for (i = 0; i < remYears; i++) {
		days += is_leap_year(i + 1904u) ? 366u : 365u;
	}

	leap = is_leap_year(dt.year);
	maxDay = dim[dt.month - 1u];
	if (dt.month == 2u && leap != 0)
		maxDay++;
	if ((uint64_t)dt.day > maxDay)
		return FF4_ERR_INVALID_ARG;

	for (i = 0; i < 12u && (i + 1u) < (uint64_t)dt.month; i++) {
		uint64_t mDays = dim[i];
		if (i == 1u && leap != 0)
			mDays++;
		days += mDays;
	}

	days += (uint64_t)(dt.day - 1u);
	*outMp4Time = days * 86400u + (uint64_t)dt.hour * 3600u +
		      (uint64_t)dt.minute * 60u + (uint64_t)dt.second;
	return FF4_OK;
}

int32_t ff4_rescale_u64(uint64_t value, uint32_t denom, uint32_t numer, uint64_t *outVal)
{
	uint64_t prodHi = 0;
	uint64_t prodLo = 0;
	uint64_t quotHi = 0;
	uint64_t quotLo = 0;
	uint64_t rem = 0;
	int32_t ret;

	if (denom == 0u || outVal == NULL)
		return FF4_ERR_INVALID_ARG;

	ret = ff4_u64_mul128(value, (uint64_t)numer, &prodHi, &prodLo);
	if (ret != FF4_OK)
		return ret;

	ret = ff4_u128_div64(prodHi, prodLo, (uint64_t)denom, &quotHi, &quotLo, &rem);
	if (ret != FF4_OK)
		return ret;

	if (quotHi != 0u)
		return FF4_ERR_OVERFLOW;

	*outVal = quotLo;
	return FF4_OK;
}

int32_t ff4_get_mem_stats(Ff4SessionImpl *sess, Ff4MemStats *outStats)
{
	if (sess == NULL || outStats == NULL)
		return FF4_ERR_INVALID_ARG;

	if (sess->pool1Limit < sess->pool1Used || sess->pool2Limit < sess->pool2Used)
		return FF4_ERR_INTERNAL;

	ff4_memset(sess, outStats, 0, sizeof(Ff4MemStats));
	outStats->pool1Limit = sess->pool1Limit;
	outStats->pool1Used = sess->pool1Used;
	outStats->pool1Remain = sess->pool1Limit - sess->pool1Used;
	outStats->pool2Limit = sess->pool2Limit;
	outStats->pool2Used = sess->pool2Used;
	outStats->pool2Remain = sess->pool2Limit - sess->pool2Used;
	return FF4_OK;
}

int32_t ff4_u128_add(int32_t *outOverflow, uint64_t *hi, uint64_t *lo, uint64_t addHi, uint64_t addLo)
{
	uint64_t newLo;
	uint64_t carry;
	uint64_t tmpHi;
	uint64_t newHi;

	if (outOverflow == NULL || hi == NULL || lo == NULL)
		return FF4_ERR_INVALID_ARG;

	newLo = *lo + addLo;
	carry = (newLo < addLo) ? 1u : 0u;
	tmpHi = *hi + addHi;
	newHi = tmpHi + carry;
	*outOverflow = (tmpHi < addHi || newHi < carry) ? 1 : 0;
	*hi = newHi;
	*lo = newLo;
	return FF4_OK;
}

int32_t ff4_u128_sub(int32_t *outBorrow, uint64_t *hi, uint64_t *lo, uint64_t subHi, uint64_t subLo)
{
	uint64_t curLo;
	uint64_t curHi;
	uint64_t borrow;
	uint64_t tmpHi;

	if (outBorrow == NULL || hi == NULL || lo == NULL)
		return FF4_ERR_INVALID_ARG;

	curLo = *lo;
	curHi = *hi;
	borrow = (curLo < subLo) ? 1u : 0u;
	tmpHi = curHi - subHi;
	*outBorrow = (curHi < subHi || tmpHi < borrow) ? 1 : 0;
	*hi = tmpHi - borrow;
	*lo = curLo - subLo;
	return FF4_OK;
}

int32_t ff4_u64_mul128(uint64_t a, uint64_t b, uint64_t *outHi, uint64_t *outLo)
{
	uint32_t aLo = (uint32_t)(a & 0xFFFFFFFFu);
	uint32_t aHi = (uint32_t)(a >> 32);
	uint32_t bLo = (uint32_t)(b & 0xFFFFFFFFu);
	uint32_t bHi = (uint32_t)(b >> 32);
	uint64_t p0;
	uint64_t p1;
	uint64_t p2;
	uint64_t mid;
	uint64_t hi = 0;

	if (outHi == NULL || outLo == NULL)
		return FF4_ERR_INVALID_ARG;

	p0 = (uint64_t)aLo * (uint64_t)bLo;
	if ((aHi | bHi) != 0u) {
		p1 = (uint64_t)aHi * (uint64_t)bLo;
		p2 = (uint64_t)aLo * (uint64_t)bHi;
		mid = (p1 & 0xFFFFFFFFu) + (p2 & 0xFFFFFFFFu) + (p0 >> 32);
		hi = (uint64_t)aHi * (uint64_t)bHi + (p1 >> 32) + (p2 >> 32) + (mid >> 32);
		p0 = (mid << 32) | (p0 & 0xFFFFFFFFu);
	}
	*outHi = hi;
	*outLo = p0;
	return FF4_OK;
}

int32_t ff4_u64_shl128(uint64_t val, uint32_t shift, uint64_t *outHi, uint64_t *outLo)
{
	uint64_t hi = 0;
	uint64_t lo = 0;

	if (outHi == NULL || outLo == NULL || shift > 127u)
		return FF4_ERR_INVALID_ARG;

	if (shift == 0u) {
		lo = val;
	} else if (shift >= 64u) {
		hi = val << (shift - 64u);
	} else {
		hi = val >> (64u - shift);
		lo = val << shift;
	}
	*outHi = hi;
	*outLo = lo;
	return FF4_OK;
}

int32_t ff4_u128_div64(uint64_t numHi, uint64_t numLo, uint64_t denom,
		       uint64_t *outQuotHi, uint64_t *outQuotLo, uint64_t *outRem)
{
	uint64_t qHi = 0;
	uint64_t qLo = 0;
	uint64_t rHi = numHi;
	uint64_t rLo = numLo;
	uint64_t shHi = 0;
	uint64_t shLo = 0;
	uint64_t bitHi = 0;
	uint64_t bitLo = 0;
	int32_t shift = 0;
	int32_t ret;

	if (outQuotHi == NULL || outQuotLo == NULL || outRem == NULL || denom == 0u)
		return FF4_ERR_INVALID_ARG;

	if (numHi == 0u) {
		*outQuotHi = 0;
		*outQuotLo = numLo / denom;
		*outRem = numLo % denom;
		return FF4_OK;
	}

	while (shift < 128) {
		ret = ff4_u64_shl128(denom, (uint32_t)shift, &shHi, &shLo);
		if (ret != FF4_OK)
			return ret;
		if ((shHi & 0x8000000000000000ULL) != 0u)
			break;
		if (rHi < shHi || (rHi == shHi && rLo <= shLo))
			break;
		shift++;
	}

	while (shift >= 0) {
		ret = ff4_u64_shl128(denom, (uint32_t)shift, &shHi, &shLo);
		if (ret != FF4_OK)
			return ret;

		if (rHi > shHi || (rHi == shHi && rLo >= shLo)) {
			uint64_t borrow = (rLo < shLo) ? 1u : 0u;
			rLo -= shLo;
			rHi = rHi - shHi - borrow;

			ret = ff4_u64_shl128(1u, (uint32_t)shift, &bitHi, &bitLo);
			if (ret != FF4_OK)
				return ret;

			{
				uint64_t newLo = qLo + bitLo;
				uint64_t carry = (newLo < bitLo) ? 1u : 0u;
				qLo = newLo;
				qHi += bitHi + carry;
			}
		}

		if (rHi == 0u && rLo < denom)
			break;
		shift--;
	}

	*outQuotHi = qHi;
	*outQuotLo = qLo;
	*outRem = rLo;
	return FF4_OK;
}

int32_t ff4_io_read(Ff4SessionImpl *sess, void *fileCtx, void *buf, uint64_t size, uint64_t *outRead)
{
	if (sess->cb.ioRead(fileCtx, buf, size, outRead) != 0)
		return FF4_ERR_IO;
	return FF4_OK;
}

int32_t ff4_io_read_be16(Ff4SessionImpl *sess, void *fileCtx, uint16_t *outVal)
{
	uint16_t raw = 0;
	uint64_t n = 0;

	if (sess->cb.ioRead(fileCtx, &raw, 2u, &n) != 0 || n != 2u)
		return FF4_ERR_IO;
	*outVal = ff4_bswap16(raw);
	return FF4_OK;
}

int32_t ff4_io_read_be32(Ff4SessionImpl *sess, void *fileCtx, uint32_t *outVal)
{
	uint32_t raw = 0;
	uint64_t n = 0;

	if (sess->cb.ioRead(fileCtx, &raw, 4u, &n) != 0 || n != 4u)
		return FF4_ERR_IO;
	*outVal = ff4_bswap32(raw);
	return FF4_OK;
}

int32_t ff4_io_read_be64(Ff4SessionImpl *sess, void *fileCtx, uint64_t *outVal)
{
	uint32_t raw[2] = { 0, 0 };
	uint64_t n = 0;

	if (sess->cb.ioRead(fileCtx, raw, 8u, &n) != 0 || n != 8u)
		return FF4_ERR_IO;
	*outVal = ((uint64_t)ff4_bswap32(raw[0]) << 32) | (uint64_t)ff4_bswap32(raw[1]);
	return FF4_OK;
}

int32_t ff4_io_write(Ff4SessionImpl *sess, void *fileCtx, const void *buf, uint64_t size, uint64_t *outWritten)
{
	if (sess->cb.ioWrite(fileCtx, buf, size, outWritten) != 0)
		return FF4_ERR_IO;
	return FF4_OK;
}

int32_t ff4_io_seek(Ff4SessionImpl *sess, void *fileCtx, int64_t offset, int32_t whence)
{
	if (sess->cb.ioSeek(fileCtx, offset, whence) != 0)
		return FF4_ERR_IO;
	return FF4_OK;
}

int32_t ff4_io_tell(Ff4SessionImpl *sess, void *fileCtx, uint64_t *outPos)
{
	if (sess->cb.ioTell(fileCtx, outPos) != 0)
		return FF4_ERR_IO;
	return FF4_OK;
}

int32_t ff4_io_getsize(Ff4SessionImpl *sess, void *fileCtx, uint64_t *outSize)
{
	if (sess->cb.ioGetSize(fileCtx, outSize) != 0)
		return FF4_ERR_IO;
	return FF4_OK;
}

void *ff4_malloc(Ff4SessionImpl *sess, uint64_t size)
{
	return sess->cb.memAlloc(sess->cb.allocUser, size);
}

void *ff4_memset(Ff4SessionImpl *sess, void *dst, int32_t val, uint64_t size)
{
	return sess->cb.memSet(sess->cb.allocUser, dst, val, size);
}

void *ff4_calloc(Ff4SessionImpl *sess, uint64_t size)
{
	void *ptr = sess->cb.memAlloc(sess->cb.allocUser, size);
	if (ptr != NULL)
		sess->cb.memSet(sess->cb.allocUser, ptr, 0, size);
	return ptr;
}

void ff4_free(Ff4SessionImpl *sess, void *ptr)
{
	sess->cb.memFree(sess->cb.allocUser, ptr);
}

uint64_t ff4_strnlen(const char *s, uint64_t maxLen)
{
	uint64_t i = 0;

	while (i != maxLen && s[i] != '\0')
		i++;
	return i;
}

int32_t ff4_strncmp(Ff4SessionImpl *sess, const char *s1, const char *s2, uint64_t n)
{
	return sess->cb.strCmp(sess->cb.allocUser, s1, s2, n);
}

char *ff4_strncpy(Ff4SessionImpl *sess, char *dst, uint64_t dstSize, const char *src, uint64_t n)
{
	if (n == 0u)
		return dst;
	dst[n - 1u] = '\0';
	return sess->cb.strCpy(sess->cb.allocUser, dst, dstSize, src, n);
}

int32_t ff4_memcmp(Ff4SessionImpl *sess, const void *p1, const void *p2, uint64_t n)
{
	return sess->cb.memCmp(sess->cb.allocUser, p1, p2, n);
}

void *ff4_memcpy(Ff4SessionImpl *sess, void *dst, uint64_t dstSize, const void *src, uint64_t n)
{
	return sess->cb.memCpy(sess->cb.allocUser, dst, dstSize, src, n);
}

void *ff4_memmove(Ff4SessionImpl *sess, void *dst, uint64_t dstSize, const void *src, uint64_t n)
{
	return sess->cb.memMove(sess->cb.allocUser, dst, dstSize, src, n);
}
