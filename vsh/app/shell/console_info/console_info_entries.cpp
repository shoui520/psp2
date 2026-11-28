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
