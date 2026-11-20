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
#include <file/localfile.h>
#include <std/string.h>

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
int consoleInfoGetTelephony(int *restriction);

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
	virtual const Field *GetFields();
	virtual unsigned int GetFieldCount();
};

/* a 64-bit value's bytes reversed (NP's and the kernel's IDs are big-endian) */
static inline SceUInt64 consoleInfoSwap64(SceUInt64 v)
{
	return ((v & 0x00000000000000FFULL) << 56) | ((v & 0x000000000000FF00ULL) << 40) |
	       ((v & 0x0000000000FF0000ULL) << 24) | ((v & 0x00000000FF000000ULL) << 8) |
	       ((v & 0x000000FF00000000ULL) >> 8) | ((v & 0x0000FF0000000000ULL) >> 24) |
	       ((v & 0x00FF000000000000ULL) >> 40) | ((v & 0xFF00000000000000ULL) >> 56);
}

/* the size of a record file's path */
#define CONSOLE_INFO_FILE_PATH_SIZE	64

/*
 * PLACEHOLDER: a file of fixed-size records under ur0:ci (the console information files'
 * library, 0x813770FC-0x813777B0): its file, its name, the size of its records and its state.
 */
class ConsoleInfoFile {
public:
	ConsoleInfoFile(const char *name, SceUInt8 recordSize);	/* (0x813770FC, 0x8137711A) */
	virtual ~ConsoleInfoFile();				/* (0x813771A4, 0x81377214) */
	virtual void GetPath(char *path);			/* (0x813777B0) */

	/* a record written or read: mode 1 opens and keeps the file, 2 closes it after */
	int Write(int index, const void *record, int mode);	/* (0x813774FC) */
	int Read(int index, void *record, int mode);		/* (0x81377634) */

	/* the file's size */
	SceInt64 GetFileSize()
	{
		char path[CONSOLE_INFO_FILE_PATH_SIZE];

		GetPath(path);
		return paf::LocalFile::GetFileSize(path);
	}

	paf::common::SharedPtr<paf::File> m_file;
	const char *m_name;
	SceUInt8 m_recordSize;
	bool m_created;
	int m_opened;
};

/* PLACEHOLDER: a flag of ur0:ci/file read (0x81376CDE) */
int consoleInfoFileReadFlag(int index, SceUInt8 *value, int mode);

/* the "mi" record's process folder */
#define CONSOLE_INFO_MI_PROC_DIR	"ur0:ci/mi/proc"

/* PLACEHOLDER: a process event of the log (64 bytes): what happened to which title, when */
struct ConsoleInfoMiEvent {
	signed char state;		/* CONSOLE_INFO_MI_STATE_* */
	signed char reserved;
	signed char flag;
	signed char kind;
	signed char subkind;
	char titleId[10];
	SceUInt8 accountId[32];
	SceUInt8 padding;
	SceUInt64 tick;			/* the process time */
	SceUInt64 rtcTick;		/* the network time */
};

/* PLACEHOLDER: a title's record in its process file (96 bytes): the event that started it, its
   state, when it changed, and the time it was active and suspended */
struct ConsoleInfoMiRecord {
	ConsoleInfoMiEvent event;
	signed char state;
	SceUInt8 padding[7];
	SceUInt64 time;
	SceUInt64 activeTime;
	SceUInt64 suspendTime;
};

/* PLACEHOLDER: a title's process file, ur0:ci/mi/proc/<title> (136 bytes, 0x810E7D54) */
class ConsoleInfoProcFile : public ConsoleInfoFile {
public:
	ConsoleInfoProcFile(const char *titleId);

	virtual ~ConsoleInfoProcFile();

	virtual void GetPath(char *path);
	virtual int Start(const ConsoleInfoMiEvent *event);
	virtual int Update(const ConsoleInfoMiEvent *event);

	bool Matches(const ConsoleInfoMiEvent *event);
	bool IsIdle();

	/* the time since the record's state last changed */
	SceUInt64 Elapsed(const ConsoleInfoMiEvent *event)
	{
		SceUInt64 elapsed = 0;

		if (m_record.time != 0)
			elapsed = event->tick - m_record.time;
		return elapsed;
	}

	/* the record written and the next one begun */
	void Next()
	{
		ConsoleInfoFile::Write(m_last, &m_record, 1);
		m_last++;
		sce_paf_memset(&m_record, 0, sizeof(m_record));
	}

	ConsoleInfoMiRecord m_record;
	int m_last;
	char m_titleId[10];
};

/* PLACEHOLDER: an application's process file (0x810E804A) */
class ConsoleInfoAppProcFile : public ConsoleInfoProcFile {
public:
	ConsoleInfoAppProcFile(const char *titleId)
		: ConsoleInfoProcFile(titleId)
	{
	}

	virtual int Start(const ConsoleInfoMiEvent *event);
	virtual int Update(const ConsoleInfoMiEvent *event);
};

/* PLACEHOLDER: an application's event (0x810E8204) */
bool consoleInfoIsAppEvent(const ConsoleInfoMiEvent *event);

class ConsoleInfoMi;

/* PLACEHOLDER: the process log written into the "mi" record (12 bytes, 0x810E76A0) */
class ConsoleInfoMiProc {
public:
	ConsoleInfoMiProc(ConsoleInfoMi *owner);
	virtual ~ConsoleInfoMiProc();

	int WriteRecord(const ConsoleInfoMiRecord *record);	/* (0x810E7722) */
	int UpdateTimes(ConsoleInfoMiRecord *record);		/* (0x810E7AB8) */
	int WriteAll();						/* (0x810E7B72) */
	int Write();						/* (0x810E7C6A) */
	int OnEvent(const ConsoleInfoMiEvent *event);		/* (0x810E7CA2) */

	ConsoleInfoMi *m_owner;
	ConsoleInfoProcFile *m_current;
};

/* PLACEHOLDER: the "mi" record with an NP account (1068 bytes, 0x810E7554) */
class ConsoleInfoMi : public ConsoleInfoRecord {
public:
	ConsoleInfoMi(int entrySize);
	virtual ~ConsoleInfoMi();

	virtual int Write(void *data);
	virtual int Commit();
	virtual const Field *GetFields();
	virtual unsigned int GetFieldCount();

	ConsoleInfoMiProc m_proc;
};

/* PLACEHOLDER names: the process event log (0x810F3452), cleared (0x810F321E), each event given
   to a function (0x810F334E), and the boot's events added (0x810F3688) */
ConsoleInfoFile *consoleInfoGetProcLog(void);
int consoleInfoProcLogClear(ConsoleInfoFile *log);
int consoleInfoProcLogForEach(ConsoleInfoFile *log,
			      int (*func)(const ConsoleInfoMiEvent *event, void *arg), void *arg);
void consoleInfoProcLogAddBoot(void);

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
