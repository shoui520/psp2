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

/* PLACEHOLDER: the file errors the records pass on (paf's) */
#define CONSOLE_INFO_ERROR_INVALID_INDEX	-44
#define CONSOLE_INFO_ERROR_NOT_FOUND		-2135994334	/* 0x80AF5022 */

/* the size of an entry's header line and of the record's own buffer */
#define CONSOLE_INFO_BUFFER_SIZE	1024

/*
 * PLACEHOLDER: a kind's record (1056 bytes, constructor 0x810E623E, vtable 0x8152FE90): its kind,
 * the size of its entries, the entry file open, the line being written, the entries' count, the
 * room left in the entry and the lines written to it. An entry is a header of "name=value"
 * fields joined by '&', then the lines, and its "nd=" count of lines.
 */
class ConsoleInfoRecord {
public:
	/* a header field: its name, the longest value, and the function that writes the value */
	struct Field {
		const char *name;
		int maxLen;
		int (*format)(const Field *field, char *buf, int *len);
	};

	ConsoleInfoRecord(int type, int entrySize);
	virtual ~ConsoleInfoRecord();

	virtual int Write(void *data) = 0;
	virtual int Commit() = 0;
	virtual paf::string GetEntryPath(int index);		/* (0x810E73EC) */
	virtual const Field *GetFields();
	virtual unsigned int GetFieldCount();

	int GetPrevDir(paf::string *path);			/* (0x810E5DA0) */
	int GetTempDir(paf::string *path);			/* (0x810E5EDE) */
	int MakeDirs();						/* (0x810E6148) */
	int OpenEntryFile(unsigned int index, paf::common::SharedPtr<paf::File> *file,
			  uint32_t flags);			/* (0x810E62F0) */
	int ReadEntry(int index, void *buf, int *size);		/* (0x810E646C) */
	int OpenTimestampFile(paf::common::SharedPtr<paf::File> *file,
			      uint32_t flags);			/* (0x810E660C) */
	int WriteTimestamp();					/* (0x810E6842) */
	int ReadTimestamp(SceUInt64 *tick);			/* (0x810E6998) */
	int RemoveTempDir();					/* (0x810E6B48) */
	int OpenNewEntry();					/* (0x810E6B62) */
	int CloseEntry();					/* (0x810E6BA0) */
	int WriteHeader();					/* (0x810E6C0C) */
	int NewEntry();						/* (0x810E6D7A) */
	int FinishEntry();					/* (0x810E6DDC) */
	int FinishAndCloseEntry();				/* (0x810E6E90) */
	int AppendLine(SceSize len, SceUInt8 count);		/* (0x810E6EAC) */

	int m_type;
	int m_entrySize;
	paf::common::SharedPtr<paf::File> m_file;
	char m_line[CONSOLE_INFO_BUFFER_SIZE];	/* the line AppendLine writes */
	int m_count;				/* the entries made */
	int m_remaining;			/* the room left in the entry */
	int m_written;				/* the lines written to it */
};

/* PLACEHOLDER names: the NP account's IDs read (0x810E5C7C), a folder made (0x810E5D38), a
   kind's folder name (0x810E5D58), the system software's version (0x810E6F48) */
int consoleInfoLoadAccountId(void);
int consoleInfoMakeDir(const char *path);
const char *consoleInfoTypeName(int type);
int consoleInfoGetSystemVersion(int *version);

/* PLACEHOLDER names: the header fields' values (0x810E6F80-0x810E73C8) */
int consoleInfoFormatSystemVersion(const ConsoleInfoRecord::Field *field, char *buf, int *len);
int consoleInfoFormatPsCode(const ConsoleInfoRecord::Field *field, char *buf, int *len);
int consoleInfoFormatOpenPsId(const ConsoleInfoRecord::Field *field, char *buf, int *len);
int consoleInfoFormatPlatform(const ConsoleInfoRecord::Field *field, char *buf, int *len);
int consoleInfoFormatAccountId(const ConsoleInfoRecord::Field *field, char *buf, int *len);
int consoleInfoFormatTelephony(const ConsoleInfoRecord::Field *field, char *buf, int *len);
int consoleInfoFormatConsole(const ConsoleInfoRecord::Field *field, char *buf, int *len);
int consoleInfoFormatPage(const ConsoleInfoRecord::Field *field, char *buf, int *len);
int consoleInfoFormatLineCount(const ConsoleInfoRecord::Field *field, char *buf, int *len);

/* PLACEHOLDER names: the console's platform, 2 on a PS TV (0x810F3910), and the telephony
   plugin's state (0x810E83C8) */
int consoleInfoGetPlatform(void);
int consoleInfoGetTelephony(int *active);

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
