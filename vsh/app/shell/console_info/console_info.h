/*
 * console_info.h: the console information the shell records under ur0:ci (ur0:ci/prev,
 * ur0:temp/ci) and sends ("SceConsoleInfoSender", "SceConsoleInfoWriterJob": Sony's word for it):
 * its kinds, each kept as one record object, and the table of the records open. The files' and
 * the other names are ours (PLACEHOLDER: no record names them).
 */
#ifndef _SHELL_CONSOLE_INFO_H
#define _SHELL_CONSOLE_INFO_H

#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <file/file.h>

/* the kinds of content information, each in its own folder ("ns", "ci", "mi", "uc") */
#define CONSOLE_INFO_TYPE_NS		0
#define CONSOLE_INFO_TYPE_CI		1
#define CONSOLE_INFO_TYPE_MI		2
#define CONSOLE_INFO_TYPE_UC		3
#define CONSOLE_INFO_TYPE_COUNT		4

/* the library's errors (PLACEHOLDER names) */
#define CONSOLE_INFO_ERROR_NOT_INITIALIZED	-45
#define CONSOLE_INFO_ERROR_NO_MEMORY		-46
#define CONSOLE_INFO_ERROR_INVALID_ARGUMENT	-47
#define CONSOLE_INFO_ERROR_ALREADY_OPEN		-49
#define CONSOLE_INFO_ERROR_INVALID_HANDLE		-50

/* the folders */
#define CONSOLE_INFO_TEMP_DIR		"ur0:temp/ci"
#define CONSOLE_INFO_PREV_DIR		"ur0:ci/prev"

/*
 * PLACEHOLDER: a kind's record (1052 bytes, constructor 0x810E623E): its kind, the size of
 * its entries, the file open, its folder and its entries' count.
 */
class ConsoleInfoRecord {
public:
	ConsoleInfoRecord(int type, int entrySize);
	virtual ~ConsoleInfoRecord();

	virtual int Write(void *data) = 0;
	virtual int Commit() = 0;
	virtual int Update();
	virtual int Reserved1();
	virtual int Reserved2();

	int ReadEntry(int index, void *buf, int *size);		/* (0x810E646C) */
	int WriteTimestamp();					/* (0x810E6842) */
	int ReadTimestamp(SceUInt64 *tick);			/* (0x810E6998) */

	int m_type;
	int m_entrySize;
	paf::common::SharedPtr<paf::File> m_file;
	char m_reserved[1024];
	int m_count;
	int m_reserved2;
	int m_reserved3;	/* PLACEHOLDER: up to the records' 1056 bytes */
};

/* PLACEHOLDER: a kind's record kept in the console only (no NP account; 0x810E74FC) */
class ConsoleInfoEntries : public ConsoleInfoRecord {
public:
	ConsoleInfoEntries(int type, int entrySize);
	virtual ~ConsoleInfoEntries();

	virtual int Write(void *data);
	virtual int Commit();
};

/* PLACEHOLDER: the "ns" record (0x810E821C) */
class ConsoleInfoNs : public ConsoleInfoRecord {
public:
	ConsoleInfoNs(int entrySize);
	virtual ~ConsoleInfoNs();

	virtual int Write(void *data);
	virtual int Commit();
};

/* PLACEHOLDER: the "mi" record with an NP account (1068 bytes, 0x810E7554) */
class ConsoleInfoMi : public ConsoleInfoRecord {
public:
	ConsoleInfoMi(int entrySize);
	virtual ~ConsoleInfoMi();

	virtual int Write(void *data);
	virtual int Commit();

	int m_list[3];		/* PLACEHOLDER: the object at +0x420 (0x810E76A0) */
};

/* PLACEHOLDER: the "ci" record with an NP account (1080 bytes, 0x810F394C) */
class ConsoleInfoCi : public ConsoleInfoRecord {
public:
	ConsoleInfoCi(int entrySize);
	virtual ~ConsoleInfoCi();

	virtual int Write(void *data);
	virtual int Commit();

	int m_list[5];		/* PLACEHOLDER: the object at +0x420 (0x810F588C) */
	bool m_changed;		/* PLACEHOLDER: +0x434 */
};

/* PLACEHOLDER: an NP account ID is in the registry (/CONFIG/NP account_id; 0x81329E20) */
int shellNpHasAccountId(int *has);

/* the library made ready, its folders made (0x810E5964), and with the sender (0x810E5954) */
int consoleInfoInit(void);
int shellConsoleInfoInit(void);

/* PLACEHOLDER names: the sender's state cleared (0x810F2174), and the sender made: its job
   queue ("SceConsoleInfoSendJobQueue") and its callbacks (0x810F2470) */
int consoleInfoSenderReset(void);
int consoleInfoSenderInit(void);

/* a kind's record made, opened into the table, written, read, closed (0x810E59A8-0x810E5C30) */
ConsoleInfoRecord *consoleInfoCreate(int type, int entrySize, int *error);
int consoleInfoOpen(int type, int entrySize, ConsoleInfoRecord **record);
int consoleInfoWrite(ConsoleInfoRecord *record, void *data);
int consoleInfoReadEntry(ConsoleInfoRecord *record, int index, void *buf, int *size);
int consoleInfoClose(ConsoleInfoRecord *record, bool commit);
int consoleInfoGetTimestamp(int type, SceUInt64 *tick);

#endif /* _SHELL_CONSOLE_INFO_H */
