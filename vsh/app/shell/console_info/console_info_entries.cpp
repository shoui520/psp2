/*
 * console_info_entries.cpp: the record of a kind kept without an NP account (0x810E74FC-0x810E7554):
 * nothing is written and nothing is committed. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include "console_info.h"

/* (0x810E74FC) */
ConsoleInfoEntries::ConsoleInfoEntries(int type, int entrySize)
	: ConsoleInfoRecord(type, entrySize)
{
}

/* (0x810E7514, 0x810E7530) */
ConsoleInfoEntries::~ConsoleInfoEntries()
{
}

/* (0x8141038C: a body the build shares) no entry written */
int ConsoleInfoEntries::Write(void *data)
{
	*(int *)data = 0;
	return 0;
}

/* (0x81409E2C: one "return 0" the build shares) */
int ConsoleInfoEntries::Commit()
{
	return 0;
}
