/*
 * The console information files: 1.dat (one-byte records), 4.dat (four-byte records) and
 * r.dat (eight-byte records).
 */

#include <scetypes.h>

#include "console_info_file.h"

static ConsoleInfoFile s_consoleInfoFiles[] = {
	ConsoleInfoFile("1.dat", 1),
	ConsoleInfoFile("4.dat", 4),
	ConsoleInfoFile("r.dat", 8),
};

int consoleInfoWrite1(int index, SceUInt8 value, int mode)
{
	return s_consoleInfoFiles[0].Write(index, &value, mode);
}
