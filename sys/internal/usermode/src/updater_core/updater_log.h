/*
 * updater_core's error log (/updater.log in the working directory, sceUpdaterLogStart): every
 * error is two lines' worth, "ERROR : " and the file's ID with Sony's line number, and the
 * value where the site reports one. Each file defines UPDATER_FILE_ID, the number Sony's
 * build gave it. The names are ours.
 */
#ifndef UPDATER_CORE_UPDATER_LOG_H
#define UPDATER_CORE_UPDATER_LOG_H

int UpdaterLogPrintf(const char *format, ...);

#define UPDATER_ERROR(_line) do {						\
	UpdaterLogPrintf("ERROR : ");						\
	UpdaterLogPrintf("0x%08x (%d)\n", UPDATER_FILE_ID, (_line));		\
} while (0)

#define UPDATER_ERROR_FMT(_line, _format, ...) do {				\
	UpdaterLogPrintf("ERROR : ");						\
	UpdaterLogPrintf("0x%08x (%d) : " _format "\n", UPDATER_FILE_ID,	\
			 (_line), __VA_ARGS__);					\
} while (0)

#endif
