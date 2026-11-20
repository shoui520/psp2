/*
 * console_info_init.cpp: the console information made ready at boot, its records' library and
 * its sender (0x810E5954). The file's and the names are ours (PLACEHOLDER).
 */
#include "console_info.h"

/* (0x810E5954) */
int shellConsoleInfoInit(void)
{
	consoleInfoInit();
	consoleInfoSenderReset();
	return consoleInfoSenderInit();
}
