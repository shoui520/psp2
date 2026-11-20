/*
 * bgdl_util.cpp: the background download library's helpers (0x810FD14C on). No record names
 * the file or its functions: the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <timer/timer.h>

#include "bgdl_util.h"
#include "bgdl_task.h"

using namespace paf;

/* (0x810FD14C) */
void BgdlProgress::Start(SceUInt64 total, SceUInt64 base, SceUInt64 reserved, SceUInt64 interval)
{
	m_done = 0;
	m_startTime = 0;
	m_lastTime = 0;
	m_reportTime = 0;
	m_reserved0 = 0;
	m_reserved1 = 0;
	m_total = total;
	m_base = base;
	m_current = 0;
	m_interval = interval;
	m_startTime = get_time_internal();
	m_reportTime = m_startTime;
}

/* (0x810FD19A) 1 when an interval has passed since the last report */
int BgdlProgress::Update(SceUInt64 current)
{
	int ret = 0;
	SceUInt64 now;

	now = get_time_internal();
	m_lastTime = now;
	m_current = current;
	m_done = current - m_base;
	if (now - m_reportTime > m_interval) {
		ret = 1;
		m_reportTime = now;
	}
	return ret;
}

/* (0x810FD1F2) */
SceUInt64 BgdlProgress::GetSpeed(void)
{
	SceUInt64 speed = 0;
	SceUInt64 elapsed;

	if (m_total == (SceUInt64)-1 || m_total == 0)
		return speed;
	if (m_done == (SceUInt64)-1 || m_done == 0)
		return speed;
	elapsed = m_lastTime - m_startTime;
	if (elapsed == 0)
		return speed;
	speed = (SceUInt64)((double)m_done / (double)elapsed * 1000000.0);
	return speed;
}

/* (0x810FD2F0) */
SceUInt64 BgdlProgress::GetRemaining(void)
{
	SceUInt64 speed;

	speed = GetSpeed();
	if (speed > 0)
		return (m_total - m_current) / speed;
	return 0;
}

/* (0x810FD328) */
int bgdlStringToInt(const string &str)
{
	int value;

	sce_paf_memset(&value, 0, sizeof(value));
	if (str.length() != 0)
		return sce_paf_atoi(str.c_str());
	return value;
}

/* (0x810FD366) */
int bgdlUIntToString(const SceUInt32 *value, string *str)
{
	char buf[16] = "";
	SceUInt32 v;

	sce_paf_memset(&v, 0, sizeof(v));
	v = *value;
	sce_paf_snprintf(buf, sizeof(buf) - 1, "%u", v);
	str->assign(buf, sce_paf_strlen(buf));
	return 0;
}

/* (0x810FD412) */
int bgdlGetInt(const int *value)
{
	int v;

	sce_paf_memset(&v, 0, sizeof(v));
	v = *value;
	return v;
}

/* (0x810FD442) */
int bgdlParseInt(const string &str)
{
	if (str.length() != 0)
		return sce_paf_atoi(str.c_str());
	return 0;
}

/* (0x810FD454) */
int bgdlFormatUInt(SceUInt32 value, string *str)
{
	char buf[16] = "";

	sce_paf_snprintf(buf, sizeof(buf) - 1, "%u", value);
	str->assign(buf, sce_paf_strlen(buf));
	return 0;
}

/* the first of [first, last) whose id is not below the task's (Dinkumware's lower_bound) */
static inline BgdlTask **bgdlLowerBound(BgdlTask **first, BgdlTask **last, BgdlTask *const &task)
{
	int count = last - first;

	while (count > 0) {
		int half = count / 2;
		BgdlTask **mid = first + half;

		if ((*mid)->id < task->id) {
			first = ++mid;
			count -= half + 1;
		} else {
			count = half;
		}
	}
	return first;
}

/* (0x810FD4EA) */
int BgdlTaskList::Insert(BgdlTask *task)
{
	BgdlArray<BgdlTask *> list;
	BgdlTask **first;
	BgdlTask **last;
	BgdlTask **dst;
	BgdlTask **it;
	int count;
	int half;
	int ret;

	ret = list.Reserve((char *)m_end - (char *)m_begin + sizeof(BgdlTask *));
	if (ret != 0)
		return ret;
	first = m_begin;
	last = m_end;
	it = first;
	count = ((char *)last - (char *)first) / (int)sizeof(BgdlTask *);
	while (count > 0) {
		half = count / 2;
		if (it[half]->id < task->id) {
			it += half + 1;
			count = count - half - 1;
		} else {
			count = half;
		}
	}
	if (it != last && !(task->id < (*it)->id))
		return BGDL_ERROR_ALREADY_EXISTS;
	dst = list.m_begin;
	while (first != it)
		*dst++ = *first++;
	*dst++ = task;
	while (it != last)
		*dst++ = *it++;
	Swap(list);
	return 0;
}

/* (0x810FD5E6) */
void BgdlTaskList::Erase(BgdlTask **pos)
{
	BgdlTask **it;

	if (pos >= m_end)
		return;
	for (it = pos + 1; it != m_end; it++)
		it[-1] = *it;
	Reserve((char *)m_end - (char *)m_begin - sizeof(BgdlTask *));
}

/* (0x810FD64E) */
BgdlTask **BgdlTaskList::Find(BgdlTask *task)
{
	BgdlTask **it;

	it = bgdlLowerBound(m_begin, m_end, task);
	if (it != m_end && task->id < (*it)->id)
		return m_end;
	return it;
}

/* (0x810FD6BA) */
void BgdlTaskList::Clear(void)
{
	BgdlTask **it;

	for (it = m_begin; it != m_end; it++) {
		if (*it != NULL) {
			delete *it;
			*it = NULL;
		}
	}
	Reserve(0);
}
