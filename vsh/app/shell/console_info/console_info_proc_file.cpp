/*
 * console_info_proc_file.cpp: a title's process file under ur0:ci/mi/proc (0x810E7D54-0x810E821C):
 * the record open follows the title's events, from its start through suspends and resumes, adding
 * up the time it was active and suspended, and is written and a new one begun when the title ends
 * or starts again. An application's file also keeps its last start's event. The file's and the
 * names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <file/localfile.h>
#include <sceerror.h>
#include "console_info.h"

using namespace paf;

/* (0x81556358) the last application's start */
static ConsoleInfoMiEvent s_lastAppEvent;

/* (0x810E7D54, 0x810E7DF4) the title's last record read back */
ConsoleInfoProcFile::ConsoleInfoProcFile(const char *titleId)
	: ConsoleInfoFile(m_titleId, sizeof(ConsoleInfoMiRecord)), m_last(0)
{
	sce_paf_memset(&m_record, 0, sizeof(m_record));
	sce_paf_memcpy(m_titleId, titleId, sizeof(m_titleId));

	SceInt64 size = GetFileSize();

	if (size > 0) {
		m_last = (int)(size / (SceInt64)sizeof(ConsoleInfoMiRecord)) - 1;
		Read(m_last, &m_record, 1);
	}
}

/* (0x814146CC, 0x814146F8: placed with the code the build shares) the record open written
   back */
ConsoleInfoProcFile::~ConsoleInfoProcFile()
{
	ConsoleInfoFile::Write(m_last, &m_record, 2);
}

/* (0x810E7DFE) */
int ConsoleInfoProcFile::Start(const ConsoleInfoMiEvent *event)
{
	sce_paf_memcpy(&m_record.event, event, sizeof(m_record.event));
	m_record.state = event->state;
	m_record.time = event->tick;
	return 0;
}

/* (0x810E7E22) */
int ConsoleInfoProcFile::Update(const ConsoleInfoMiEvent *event)
{
	signed char state = event->state;

	switch (state) {
	case 4:
		Next();
		Start(event);
		break;
	case 5:
		break;
	case 6:
		if (m_record.state == 4 || m_record.state == 7) {
			SceUInt64 elapsed = Elapsed(event);

			m_record.state = state;
			m_record.suspendTime += elapsed;
			m_record.time = event->tick;
			sce_paf_memcpy(&m_record.event, event, sizeof(m_record.event));
		} else {
			Start(event);
		}
		break;
	case 7: {
		SceUInt64 elapsed = Elapsed(event);

		m_record.state = state;
		m_record.activeTime += elapsed;
		m_record.time = event->tick;
		Next();
		Start(event);
		break;
	}
	case 8:
		if (m_record.state == 6) {
			SceUInt64 elapsed = Elapsed(event);

			m_record.state = state;
			m_record.activeTime += elapsed;
			m_record.time = event->tick;
		} else {
			SceUInt64 elapsed = Elapsed(event);

			m_record.state = state;
			m_record.suspendTime += elapsed;
			m_record.time = event->tick;
		}
		Next();
		break;
	default:
		return SCE_ERROR_ERRNO_EINVAL;
	}
	return 0;
}

/* (0x810E7FFE) */
bool ConsoleInfoProcFile::Matches(const ConsoleInfoMiEvent *event)
{
	return sce_paf_memcmp(m_titleId, event->titleId, sizeof(m_titleId)) == 0;
}

/* (0x810E8016) */
bool ConsoleInfoProcFile::IsIdle()
{
	return m_record.state == 0;
}

/* (0x810E8022) */
void ConsoleInfoProcFile::GetPath(char *path)
{
	sce_paf_memcpy(path, CONSOLE_INFO_MI_PROC_DIR, sizeof(CONSOLE_INFO_MI_PROC_DIR) - 1);
	path[sizeof(CONSOLE_INFO_MI_PROC_DIR) - 1] = '/';
	sce_paf_memcpy(path + sizeof(CONSOLE_INFO_MI_PROC_DIR), m_name, sizeof(m_titleId));
}

/* (0x810E804A) */
int ConsoleInfoAppProcFile::Start(const ConsoleInfoMiEvent *event)
{
	sce_paf_memcpy(&m_record.event, event, sizeof(m_record.event));
	m_record.state = event->state;
	m_record.time = event->tick;
	sce_paf_memcpy(&s_lastAppEvent, event, sizeof(s_lastAppEvent));
	return 0;
}

/* (0x810E8080) the suspended time of a title ended taken from the network time */
int ConsoleInfoAppProcFile::Update(const ConsoleInfoMiEvent *event)
{
	signed char state = event->state;

	switch (state) {
	case 1:
		if (m_record.state != 0)
			Next();
		Start(event);
		return 0;
	case 2: {
		SceUInt64 elapsed = Elapsed(event);

		m_record.state = state;
		m_record.activeTime += elapsed;
		m_record.time = event->tick;
		return 0;
	}
	case 3: {
		SceUInt64 elapsed = Elapsed(event);

		m_record.state = state;
		m_record.activeTime += elapsed;
		m_record.time = event->tick;

		SceUInt64 rtcStart = m_record.event.rtcTick;

		if (rtcStart == 0 || event->rtcTick == 0) {
			m_record.suspendTime = 0;
		} else {
			SceUInt64 start = m_record.event.tick;

			m_record.suspendTime = event->rtcTick - rtcStart - event->tick + start;
			if (event->rtcTick + start < rtcStart + event->tick)
				m_record.suspendTime = 0;
		}
		Next();
		Start(event);
		return 0;
	}
	}
	return SCE_ERROR_ERRNO_EINVAL;
}

/* (0x810E8204) */
bool consoleInfoIsAppEvent(const ConsoleInfoMiEvent *event)
{
	int state = event->state;

	if (state == 1 || state == 2 || state == 3)
		return true;
	return false;
}
