/*
 * console_info_uc.cpp: the "uc" record (0x810F5DE8-0x810F5E88): one entry of the system
 * software's version, the NP account and the telephony restriction, written when the shell starts
 * and ends, kept in ur0:ci/prev/uc. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <dir/dir.h>
#include "console_info.h"

using namespace paf;

/* the size of the record's entries */
#define CONSOLE_INFO_UC_ENTRY_SIZE	256

/* the record's folder and its one entry */
#define CONSOLE_INFO_UC_DIR		CONSOLE_INFO_PREV_DIR "/uc"
#define CONSOLE_INFO_UC_ENTRY		CONSOLE_INFO_UC_DIR "/uc.dat"

/* (0x81544740) the entry */
static const ConsoleInfoRecord::Field s_ucFields[] = {
	{ "ver",	8,	consoleInfoFormatSystemVersion },
	{ "sid",	64,	consoleInfoFormatAccountId },
	{ "nd",		1,	consoleInfoFormatTelephony },
};

/* (0x810F5DE8) */
ConsoleInfoUc::ConsoleInfoUc()
	: ConsoleInfoRecord(CONSOLE_INFO_TYPE_UC, CONSOLE_INFO_UC_ENTRY_SIZE)
{
}

/* (0x810F5E08, 0x810F5E24) */
ConsoleInfoUc::~ConsoleInfoUc()
{
}

/* (0x810E827C: one function with the "ns" record's) the entry, its header only */
int ConsoleInfoUc::Write(void *data)
{
	int ret = OpenNewEntry();

	if (ret != 0)
		return ret;
	ret = WriteHeader();
	CloseEntry();
	*(int *)data = m_count;
	return ret;
}

/* (0x81409E2C: one "return 0" the build shares) */
int ConsoleInfoUc::Commit()
{
	return 0;
}

/* (0x810F5E44) */
const ConsoleInfoRecord::Field *ConsoleInfoUc::GetFields()
{
	return s_ucFields;
}

/* (0x810F5E4E) the one entry, in the record's folder */
string ConsoleInfoUc::GetEntryPath(int index)
{
	Dir::CreateRecursive(CONSOLE_INFO_UC_DIR);
	return string(CONSOLE_INFO_UC_ENTRY);
}

/* (0x8141F86C: a "return 3" the build shares) */
unsigned int ConsoleInfoUc::GetFieldCount()
{
	return sizeof(s_ucFields) / sizeof(s_ucFields[0]);
}
