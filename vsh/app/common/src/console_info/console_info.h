/*
 * The console information files (ur0:ci/file/): fixed-size records the system software
 * writes about how the console is used (SceConsoleInfo). The shell and several applications
 * link this code; each file is a ConsoleInfoFile, its records all one size.
 */

#ifndef _CONSOLE_INFO_H
#define _CONSOLE_INFO_H

#include <scetypes.h>

/* what ConsoleInfoFile::Write does with the file around the record it writes */
#define CONSOLE_INFO_WRITE_ONCE		0	/* opens it, writes, closes it */
#define CONSOLE_INFO_WRITE_KEEP_OPEN	1	/* opens it if not yet open, keeps it open */
#define CONSOLE_INFO_WRITE_LAST		2	/* writes, then closes it */

#ifdef __cplusplus
extern "C" {
#endif

/* writes a byte into record index of 1.dat */
int consoleInfoWrite1(int index, SceUInt8 value, int mode);

#ifdef __cplusplus
}
#endif

#endif /* _CONSOLE_INFO_H */
