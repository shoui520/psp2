/*
 * console_info_mi.cpp: the "mi" record with an NP account (0x810E7554-0x810E76A0): its entries are
 * the titles' process records, written while power_manage_plugin allows it; committing it clears
 * the process log and starts it again with the boot's events. The file's and the names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include "plugins.h"
#include "console_info.h"

using namespace paf;

/* the flag of ur0:ci/file the "fl" field gives */
#define CONSOLE_INFO_MI_FLAG_INDEX	3

static int consoleInfoMiFormatVersion(const ConsoleInfoRecord::Field *field, char *buf, int *len);
static int consoleInfoMiFormatFlag(const ConsoleInfoRecord::Field *field, char *buf, int *len);

/* (0x81544494) the entries' header */
static const ConsoleInfoRecord::Field s_miFields[] = {
	{ "c",	1,	consoleInfoFormatPlatform },
	{ "e",	1,	consoleInfoMiFormatVersion },
	{ "h",	111,	consoleInfoFormatConsole },
	{ "pn",	5,	consoleInfoFormatPage },
	{ "nd",	2,	consoleInfoFormatLineCount },
	{ "fl",	1,	consoleInfoMiFormatFlag },
};

/* (0x810E7554) */
ConsoleInfoMi::ConsoleInfoMi(int entrySize)
	: ConsoleInfoRecord(CONSOLE_INFO_TYPE_MI, entrySize), m_proc(this)
{
}

/* (0x810E757C, 0x810E75A0) */
ConsoleInfoMi::~ConsoleInfoMi()
{
}

/* (0x810E75CA) the processes' records written; the entries made given back */
int ConsoleInfoMi::Write(void *data)
{
	Plugin *power = Plugin::Find("power_manage_plugin");

	if (power != NULL) {
		ShellPluginInterface *iface = (ShellPluginInterface *)power->GetInterface(1);

		if (iface != NULL &&
		    ((int (*)(void))iface->func[POWER_MANAGE_PLUGIN_FUNC_IS_BUSY])() != 0)
			return CONSOLE_INFO_ERROR_NOT_INITIALIZED;
	}

	int ret = NewEntry();

	if (ret == 0) {
		ret = m_proc.Write();
		if (ret == 0)
			FinishAndCloseEntry();
	}
	if (ret != 0)
		return ret;
	*(int *)data = m_count;
	return 0;
}

/* (0x810E7628) */
int ConsoleInfoMi::Commit()
{
	consoleInfoProcLogClear(consoleInfoGetProcLog());
	consoleInfoProcLogAddBoot();
	return 0;
}

/* (0x810E763A) */
const ConsoleInfoRecord::Field *ConsoleInfoMi::GetFields()
{
	return s_miFields;
}

/* (0x81010BE4: a "return 6" the build shares) */
unsigned int ConsoleInfoMi::GetFieldCount()
{
	return sizeof(s_miFields) / sizeof(s_miFields[0]);
}

/* (0x810E7644) */
static int consoleInfoMiFormatVersion(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	buf[0] = '3';
	buf[1] = '\0';
	*len = 1;
	return 0;
}

/* (0x810E7652) */
static int consoleInfoMiFormatFlag(const ConsoleInfoRecord::Field *field, char *buf, int *len)
{
	SceUInt8 flag = 0xFF;

	consoleInfoFileReadFlag(CONSOLE_INFO_MI_FLAG_INDEX, &flag, 0);
	buf[0] = flag != 0 ? '1' : '0';
	buf[1] = '\0';
	*len = 1;
	return 0;
}
