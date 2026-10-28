#include "datetime.h"

DateTime *DateTimeConstruct(DateTime *self, uint16_t year, uint16_t month,
			    uint16_t day, uint16_t hour, uint16_t minute,
			    uint16_t second, uint32_t microsecond)
{
	self->m_year = year;
	self->m_month = month;
	self->m_day = day;
	self->m_hour = hour;
	self->m_minute = minute;
	self->m_second = second;
	self->m_parse_failed = 0;
	self->m_tz_offset = 0;
	self->m_microsecond = microsecond;
	return self;
}

DateTime *DateTimeDestroy(DateTime *self)
{
	return self;
}

DateTime *DateTimeConstructBase(DateTime *self, uint16_t year, uint16_t month,
			    uint16_t day, uint16_t hour, uint16_t minute,
			    uint16_t second, uint32_t microsecond)
{
	self->m_year = year;
	self->m_month = month;
	self->m_day = day;
	self->m_hour = hour;
	self->m_minute = minute;
	self->m_second = second;
	self->m_parse_failed = 0;
	self->m_tz_offset = 0;
	self->m_microsecond = microsecond;
	return self;
}

DateTime *DateTimeDestroyBase(DateTime *self)
{
	return self;
}
