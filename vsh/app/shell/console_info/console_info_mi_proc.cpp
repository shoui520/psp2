/*
 * console_info_mi_proc.cpp: the process log written into the "mi" record (0x810E76A0-0x810E7D54):
 * the log's events are sorted into one process file per title under ur0:ci/mi/proc, and each
 * title's records are then written as a "d1=" line: the title, the account, when it started and
 * how long it was active and suspended. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <std/stdio.h>
#include <dir/dir.h>
#include <rtc/rtc.h>
#include <datetime/datetime.h>
#include <kernel/processmgr.h>
#include <sceerror.h>
#include "console_info.h"

using namespace paf;

/* the kinds of process a record gives (PLACEHOLDER names) */
#define CONSOLE_INFO_MI_KIND_SUBKIND_1		11
#define CONSOLE_INFO_MI_KIND_SUBKIND_2		10

/* microseconds in a second */
#define CONSOLE_INFO_MI_USEC_PER_SEC		1000000

/* (0x810E76A0) the process files of the last time removed */
ConsoleInfoMiProc::ConsoleInfoMiProc(ConsoleInfoMi *owner)
	: m_owner(owner), m_current(NULL)
{
	Dir::RemoveRecursive(CONSOLE_INFO_MI_PROC_DIR);
	Dir::CreateRecursive(CONSOLE_INFO_MI_PROC_DIR);
}

/* (0x810E76D0, 0x810E76F6) */
ConsoleInfoMiProc::~ConsoleInfoMiProc()
{
	if (m_current != NULL) {
		delete m_current;
		m_current = NULL;
	}
}

/* (0x810E7722) a record's "d1=" line */
int ConsoleInfoMiProc::WriteRecord(const ConsoleInfoMiRecord *record)
{
	datetime::DateTime date(0, 0, 0, 0, 0, 0, 0);

	if (record->event.rtcTick != 0)
		date = datetime::DateTime::ConvertRtcTickToDateTime(
			(rtc::Tick *)&record->event.rtcTick);

	int kind;

	if (record->event.subkind == 1)
		kind = CONSOLE_INFO_MI_KIND_SUBKIND_1;
	else if (record->event.subkind == 2)
		kind = CONSOLE_INFO_MI_KIND_SUBKIND_2;
	else if (record->event.state == 5 || record->event.state == 1)
		kind = record->event.kind;
	else
		kind = 0;

	SceUInt64 account[4];

	sce_paf_memcpy(account, record->event.accountId, sizeof(account));
	account[0] = consoleInfoSwap64(account[0]);
	account[1] = consoleInfoSwap64(account[1]);
	account[2] = consoleInfoSwap64(account[2]);
	account[3] = consoleInfoSwap64(account[3]);

	char *line = m_owner->m_line;

	sce_paf_snprintf(line, CONSOLE_INFO_BUFFER_SIZE,
			 "d1=%s,%1d,%016llx%016llx%016llx%016llx,%04d%02d%02d%02d%02d%02d,%d,%d,%d",
			 record->event.titleId, record->event.flag,
			 account[0], account[1], account[2], account[3],
			 date.m_year, date.m_month, date.m_day, date.m_hour, date.m_minute,
			 date.m_second,
			 (int)(record->activeTime / CONSOLE_INFO_MI_USEC_PER_SEC),
			 (int)(record->suspendTime / CONSOLE_INFO_MI_USEC_PER_SEC), kind);
	return m_owner->AppendLine(sce_paf_strlen(line), 1);
}

/* (0x810E7AB8) the last record's times, up to now */
int ConsoleInfoMiProc::UpdateTimes(ConsoleInfoMiRecord *record)
{
	SceKernelSysClock clock;
	int ret = sceKernelGetProcessTime(&clock);

	if (ret != 0)
		return ret;

	switch (record->state) {
	case 1:
	case 3:
	case 6:
		if (clock.quad > record->time && record->activeTime == 0)
			record->activeTime = clock.quad - record->time;
		break;
	case 8:
		break;
	case 2:
	case 4:
	case 5:
	case 7:
	default:
		if (clock.quad > record->time && record->suspendTime == 0)
			record->suspendTime = clock.quad - record->time;
		break;
	}
	return 0;
}

/* (0x810E7B72) every title's records written */
int ConsoleInfoMiProc::WriteAll()
{
	Dir dir;
	int ret = dir.Open(CONSOLE_INFO_MI_PROC_DIR);

	if (ret != 0)
		return ret;

	for (;;) {
		DirEnt entry;

		if (dir.Read(&entry) != 0) {
			ret = 0;
			break;
		}

		ConsoleInfoProcFile file(entry.name.c_str());

		ret = 0;
		for (int i = 0; i <= file.m_last; i++) {
			ConsoleInfoMiRecord record;

			ret = file.Read(i, &record, 1);
			if (ret != 0)
				break;
			if (record.state != 0) {
				if (i == file.m_last)
					UpdateTimes(&record);
				WriteRecord(&record);
			}
		}
		if (ret != 0)
			break;
	}
	return ret;
}

/* (0x810E7D22) an event of the log given to its title's process file */
static int consoleInfoMiProcOnLogEvent(const ConsoleInfoMiEvent *event, void *arg)
{
	ConsoleInfoMiProc *proc = (ConsoleInfoMiProc *)arg;

	if (proc->OnEvent(event) == 0) {
		if (proc->m_current->IsIdle())
			proc->m_current->Start(event);
		else
			proc->m_current->Update(event);
	}
	return 0;
}

/* (0x810E7C6A) the log sorted into the process files, and their records written */
int ConsoleInfoMiProc::Write()
{
	int ret = consoleInfoProcLogForEach(consoleInfoGetProcLog(), consoleInfoMiProcOnLogEvent,
					    this);

	if (ret != 0)
		return ret;
	if (m_current != NULL)
		delete m_current;
	m_current = NULL;
	ret = WriteAll();
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810E7CA2) the event's title's process file opened */
int ConsoleInfoMiProc::OnEvent(const ConsoleInfoMiEvent *event)
{
	if (m_current != NULL && m_current->Matches(event))
		return 0;
	if (m_current != NULL) {
		delete m_current;
		m_current = NULL;
	}

	ConsoleInfoProcFile *file;

	if (consoleInfoIsAppEvent(event))
		file = new ConsoleInfoAppProcFile(event->titleId);
	else
		file = new ConsoleInfoProcFile(event->titleId);
	m_current = file;
	if (file == NULL)
		return SCE_ERROR_ERRNO_ENOMEM;
	return 0;
}
