/*
 * console_info.cpp: the console information's records open (0x810E5964-0x810E5C7C): one record
 * of each kind at a time, kept in a table; a record is written, read, and closed with its
 * time stamp written. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include "console_info.h"

/* (0x81544474, 0x81544484) */
static ConsoleInfoRecord *s_records[CONSOLE_INFO_TYPE_COUNT];
static bool s_initialized;

/* a record of the table: 0, or why not */
static inline int consoleInfoCheck(ConsoleInfoRecord *record)
{
	int ret;

	if (!s_initialized) {
		ret = CONSOLE_INFO_ERROR_NOT_INITIALIZED;
	} else if (record == NULL) {
		ret = CONSOLE_INFO_ERROR_INVALID_HANDLE;
	} else {
		unsigned int i;

		ret = 0;
		for (i = 0; i < CONSOLE_INFO_TYPE_COUNT; i++) {
			if (s_records[i] == record)
				break;
		}
		if (i >= CONSOLE_INFO_TYPE_COUNT)
			ret = CONSOLE_INFO_ERROR_INVALID_HANDLE;
	}
	return ret;
}

/* (0x810E5964) */
int consoleInfoInit(void)
{
	if (s_initialized)
		return CONSOLE_INFO_ERROR_NOT_INITIALIZED;

	consoleInfoMakeDir(CONSOLE_INFO_TEMP_DIR);

	int ret = consoleInfoMakeDir(CONSOLE_INFO_PREV_DIR);

	if (ret != 0)
		return ret;
	s_initialized = true;
	return 0;
}

/* (0x810E59A8) the kind's record: with an NP account, the console and machine information's
   own kinds */
ConsoleInfoRecord *consoleInfoCreate(int type, int entrySize, int *error)
{
	if (!s_initialized) {
		*error = CONSOLE_INFO_ERROR_NOT_INITIALIZED;
		return NULL;
	}

	int hasAccount = 0;
	int ret = shellNpHasAccountId(&hasAccount);
	ConsoleInfoRecord *record;

	switch (type) {
	case CONSOLE_INFO_TYPE_NS:
		record = new ConsoleInfoNs(entrySize);
		break;
	case CONSOLE_INFO_TYPE_CI:
		if (hasAccount)
			record = new ConsoleInfoCi(entrySize);
		else
			record = new ConsoleInfoEntries(type, entrySize);
		break;
	case CONSOLE_INFO_TYPE_MI:
		if (hasAccount)
			record = new ConsoleInfoMi(entrySize);
		else
			record = new ConsoleInfoEntries(type, entrySize);
		break;
	default:
		ret = CONSOLE_INFO_ERROR_INVALID_ARGUMENT;
		record = NULL;
		break;
	}
	if (ret != 0)
		*error = ret;
	if (record == NULL)
		*error = CONSOLE_INFO_ERROR_NO_MEMORY;
	return record;
}

/* (0x810E5AB0) */
int consoleInfoOpen(int type, int entrySize, ConsoleInfoRecord **record)
{
	if (s_records[type] != NULL)
		return CONSOLE_INFO_ERROR_ALREADY_OPEN;

	int error = 0;
	ConsoleInfoRecord *created = consoleInfoCreate(type, entrySize, &error);

	*record = created;
	s_records[type] = created;
	return error;
}

/* (0x810E5B02) */
int consoleInfoWrite(ConsoleInfoRecord *record, void *data)
{
	if (data == NULL || record == NULL)
		return CONSOLE_INFO_ERROR_INVALID_ARGUMENT;

	int ret = consoleInfoCheck(record);

	if (ret == 0)
		ret = record->Write(data);
	return ret;
}

/* (0x810E5B52) */
int consoleInfoReadEntry(ConsoleInfoRecord *record, int index, void *buf, int *size)
{
	if (buf == NULL || size == NULL)
		return CONSOLE_INFO_ERROR_INVALID_ARGUMENT;

	int ret = consoleInfoCheck(record);

	if (ret == 0)
		ret = record->ReadEntry(index, buf, size);
	return ret;
}

/* (0x810E5BAE) the record committed when asked, its time stamp written, and deleted */
int consoleInfoClose(ConsoleInfoRecord *record, bool commit)
{
	int ret = consoleInfoCheck(record);

	if (ret != 0)
		return ret;
	if (commit) {
		ret = record->Commit();
		if (ret != 0)
			return ret;
	}
	record->WriteTimestamp();

	int type = record->m_type;

	if (s_records[type] != NULL)
		delete s_records[type];
	s_records[type] = NULL;
	return 0;
}

/* (0x810E5C30) when the kind's record was last closed */
int consoleInfoGetTimestamp(int type, SceUInt64 *tick)
{
	int error = 0;
	ConsoleInfoRecord *record = consoleInfoCreate(type, 0, &error);

	if (record != NULL) {
		error = record->ReadTimestamp(tick);
		delete record;
	}
	return error;
}
