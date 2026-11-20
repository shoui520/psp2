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
#include <job/job.h>
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

/* PLACEHOLDER: the "uc" record (1056 bytes, 0x810F5DE8) */
class ConsoleInfoUc : public ConsoleInfoRecord {
public:
	ConsoleInfoUc();
	virtual ~ConsoleInfoUc();

	virtual int Write(void *data);
	virtual int Commit();
	virtual paf::string GetEntryPath(int index);
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

/* the record files' mode, and how one is opened to be written (SCE_O_*) */
#define CONSOLE_INFO_FILE_MODE		6
#define CONSOLE_INFO_FILE_WRITE		(SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC)

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
	/* the file removed (0x8137776A) */
	int Remove();

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
/* PLACEHOLDER: a flag of ur0:ci/file written (0x81376B04) */
int consoleInfoFileWriteFlag(int index, SceUInt8 value, int mode);
/* PLACEHOLDER: a 32-bit and a 64-bit value of ur0:ci/file read (0x81376CF8, 0x81376D16) */
int consoleInfoFileReadInt(int index, SceUInt32 *value, int mode);
int consoleInfoFileReadInt64(int index, SceUInt64 *value, int mode);
/* PLACEHOLDER: a 32-bit and a 64-bit value of ur0:ci/file written (0x81376B3C, 0x81376B74) */
int consoleInfoFileWriteInt(int index, SceUInt32 value, int mode);
int consoleInfoFileWriteInt64(int index, SceUInt64 value, int mode);
/* PLACEHOLDER: a registry key's text set and not empty (0x810F4B26) */
bool consoleInfoRegKeyIsSet(const char *category, const char *name);

/* the "mi" record's process folder */
#define CONSOLE_INFO_MI_PROC_DIR	"ur0:ci/mi/proc"

/* PLACEHOLDER: a process event of the log (64 bytes): what happened to which title, when */
struct ConsoleInfoMiEvent {
	signed char state;		/* CONSOLE_INFO_MI_STATE_* */
	signed char type;		/* the request's first attribute; 1 for the shell's own */
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

/* the process event log's file, and its count's (ur0:ci/m.log, ur0:ci/m.cnt) */
#define CONSOLE_INFO_PROC_LOG		"m.log"
#define CONSOLE_INFO_PROC_LOG_COUNT	"m.cnt"

/* PLACEHOLDER: the process event log, ur0:ci/m.log (28 bytes, 0x810F31F8): a ring of events, the
   next one written at m_next */
class ConsoleInfoProcLog : public ConsoleInfoFile {
public:
	ConsoleInfoProcLog();

	virtual ~ConsoleInfoProcLog()				/* (0x81414C34, 0x81414C50) */
	{
	}

	int m_next;
};

/* PLACEHOLDER names: a media information request checked (0x810F302C); the process event log
   (0x810F3452), cleared (0x810F321E), an event added (0x810F3248), each event given to a function
   (0x810F334E), the running titles' events added (0x810F3596) and the boot's (0x810F3688) */
struct ShellMediaInfoRequest;
int consoleInfoMediaInfoCheck(const ShellMediaInfoRequest *request);
ConsoleInfoProcLog *consoleInfoGetProcLog(void);
int consoleInfoProcLogClear(ConsoleInfoProcLog *log);
void consoleInfoProcLogAdd(ConsoleInfoProcLog *log, const ConsoleInfoMiEvent *event, int mode);
int consoleInfoProcLogForEach(ConsoleInfoProcLog *log,
			      int (*func)(const ConsoleInfoMiEvent *event, void *arg), void *arg);
void consoleInfoProcLogAddRunning(bool end, const ConsoleInfoMiEvent *base);
void consoleInfoProcLogAddBoot(void);

/* the job writing a media information request's event (96 bytes, 0x810F34B2): when a caller
   waits, the job waits in turn for it to have seen the job run */
class MediaInfoWriterJob : public paf::job::JobItem {
public:
	MediaInfoWriterJob(const ShellMediaInfoRequest *request, bool wait);
	virtual ~MediaInfoWriterJob();
	virtual int Run();
	virtual void Finish(int result);

	int Wait();
	void CountUsage();

	bool m_wait;
	volatile bool m_running;
	volatile bool m_done;
	ConsoleInfoMiEvent m_event;
};

class ConsoleInfoCi;

/*
 * PLACEHOLDER: a values file of the "ci" record (16 bytes, 0x810F3AF0): one value of m_size bytes
 * per field, ur0:temp/ci/ci<size>.dat while the record is made and ur0:ci/prev/ci/ci<size>.dat
 * once it is committed. Store writes the values now, Compare writes the lines of those changed.
 */
class ConsoleInfoCiValueFile {
public:
	ConsoleInfoCiValueFile(int size);
	virtual ~ConsoleInfoCiValueFile();

	virtual int Store() = 0;
	virtual int Compare(ConsoleInfoCi *ci) = 0;

	int Close();
	paf::string GetTempPath();
	int Open(uint32_t flags);
	int Save();
	int Load(ConsoleInfoCi *ci);
	paf::string GetPrevPath();
	int OpenPrev();
	int Write(const void *value);	/* (0x810F4416) */
	int Read(void *value);		/* (0x810F442C) */

	paf::common::SharedPtr<paf::File> m_file;
	int m_size;
};

/* PLACEHOLDER: the "ci" record's four values files: 32-bit, 64-bit and text values and the
   network settings (20 bytes, 0x810F588C) */
class ConsoleInfoCiFiles {
public:
	ConsoleInfoCiFiles();				/* (0x810F588C) */
	virtual ~ConsoleInfoCiFiles();			/* (0x810F5944) */

	int Store();					/* (0x810F59B8) */
	int Compare(ConsoleInfoCi *ci);			/* (0x810F59E0) */
	int Commit();					/* (0x810F5A10) */

	ConsoleInfoCiValueFile *m_files[4];
};

/* PLACEHOLDER: the "ci" record with an NP account (1080 bytes, 0x810F394C): its values files,
   and whether the system software changed since the record was last committed */
class ConsoleInfoCi : public ConsoleInfoRecord {
public:
	ConsoleInfoCi(int entrySize);
	virtual ~ConsoleInfoCi();

	virtual int Write(void *data);
	virtual int Commit();
	virtual const Field *GetFields();
	virtual unsigned int GetFieldCount();

	ConsoleInfoCiFiles m_files;
	bool m_updated;
};

/*
 * PLACEHOLDER names: the "ci" record's fields. Each field has a key (its number in the record's
 * "d<n>=" lines, the size and kind of its value, whether it is written even when unchanged and
 * the line's mode) and a reader that gets its value now; its value is kept in a values file
 * between records, and a line is written when it changed.
 */

/* the key (0x810F444C); its number is 0 where the platform does not report the field */
class ConsoleInfoCiKey {
public:
	ConsoleInfoCiKey(SceUInt16 id, char type, SceUInt16 size, bool force, SceUInt8 lineMode,
			 SceUInt8 platforms);
	~ConsoleInfoCiKey();	/* (0x814098DC: an empty function the build shares) */

	/* the field's "d<n>=" format, the value printed in decimal or hexadecimal (0x810F4490) */
	paf::string GetFormat(const char *decimal, const char *hex) const;

	SceUInt16 m_id;
	SceUInt16 m_size;
	char m_type;		/* CONSOLE_INFO_CI_TYPE_* */
	bool m_force;
	SceUInt8 m_lineMode;
};

/* the kinds of value a key gives: a time is printed as a date */
#define CONSOLE_INFO_CI_TYPE_TIME	'T'

/* the number of the field whose empty value is not written */
#define CONSOLE_INFO_CI_ID_EMPTY_SKIPPED	436

/* a text value: its length and its text */
struct ConsoleInfoCiString {
	SceUInt16 length;
	char text[1020];
};

class ConsoleInfoCiField {
public:
	ConsoleInfoCiField(ConsoleInfoCiKey key)
		: m_key(key)
	{
	}
	virtual ~ConsoleInfoCiField();	/* (0x81414890, 0x81414954) */

	/* the value now, into the values file */
	virtual int Read(ConsoleInfoCiValueFile *file) = 0;
	/* the field's line, when its value changed since the previous values */
	virtual int Write(ConsoleInfoCiValueFile *prev, ConsoleInfoCiValueFile *cur,
			  ConsoleInfoCi *ci) = 0;

	ConsoleInfoCiKey m_key;
};

class ConsoleInfoCiIntField : public ConsoleInfoCiField {
public:
	typedef int (*Getter)(ConsoleInfoCiIntField *field, int *value);

	ConsoleInfoCiIntField(int index, Getter getter, ConsoleInfoCiKey key)	/* (0x814148D0) */
		: ConsoleInfoCiField(key), m_index(index), m_getter(getter)
	{
	}

	virtual int Read(ConsoleInfoCiValueFile *file);			/* (0x810E8440) */
	virtual int Write(ConsoleInfoCiValueFile *prev, ConsoleInfoCiValueFile *cur,
			  ConsoleInfoCi *ci);				/* (0x810E8482) */

	int m_index;
	Getter m_getter;
};

/* a value of ur0:ci/file a field gives: its size, its unit, its index and the most it is
   given as (0: no limit) */
struct ConsoleInfoCiFileValue {
	ConsoleInfoCiFileValue(int kind, int unit, int index, SceUInt32 max, SceUInt8 flags)	/* (0x81414A7C) */
		: kind(kind), unit(unit), index(index), max(max), flags(flags)
	{
	}

	int kind;		/* CONSOLE_INFO_CI_FILE_* */
	int unit;		/* CONSOLE_INFO_CI_UNIT_* */
	int index;
	SceUInt32 max;
	SceUInt8 flags;
};

#define CONSOLE_INFO_CI_FILE_FLAG	0
#define CONSOLE_INFO_CI_FILE_INT	1
#define CONSOLE_INFO_CI_FILE_INT64	2

/* a time kept in microseconds and given in seconds */
#define CONSOLE_INFO_CI_UNIT_USEC	2

class ConsoleInfoCiFileIntField : public ConsoleInfoCiIntField {
public:
	ConsoleInfoCiFileIntField(int index, ConsoleInfoCiKey key, ConsoleInfoCiFileValue value)	/* (0x81414A90) */
		: ConsoleInfoCiIntField(index, Get, key), m_value(value)
	{
	}

	static int Get(ConsoleInfoCiIntField *field, int *value);	/* (0x810E9AFA) */

	ConsoleInfoCiFileValue m_value;
};

class ConsoleInfoCiInt64Field : public ConsoleInfoCiField {
public:
	typedef int (*Getter)(ConsoleInfoCiInt64Field *field, SceInt64 *value);

	ConsoleInfoCiInt64Field(int index, Getter getter, ConsoleInfoCiKey key)	/* (0x81414974) */
		: ConsoleInfoCiField(key), m_index(index), m_getter(getter)
	{
	}

	virtual int Read(ConsoleInfoCiValueFile *file);			/* (0x810E8544) */
	virtual int Write(ConsoleInfoCiValueFile *prev, ConsoleInfoCiValueFile *cur,
			  ConsoleInfoCi *ci);				/* (0x810E858A) */

	int m_index;
	Getter m_getter;
};

class ConsoleInfoCiFileInt64Field : public ConsoleInfoCiInt64Field {
public:
	ConsoleInfoCiFileInt64Field(int index, ConsoleInfoCiKey key, ConsoleInfoCiFileValue value)	/* (0x81414B60) */
		: ConsoleInfoCiInt64Field(index, Get, key), m_value(value)
	{
	}

	static int Get(ConsoleInfoCiInt64Field *field, SceInt64 *value);	/* (0x810E9BA6) */

	ConsoleInfoCiFileValue m_value;
};

class ConsoleInfoCiStringField : public ConsoleInfoCiField {
public:
	typedef int (*Getter)(ConsoleInfoCiStringField *field, ConsoleInfoCiString *value);

	ConsoleInfoCiStringField(int index, Getter getter, ConsoleInfoCiKey key)	/* (0x814149F8) */
		: ConsoleInfoCiField(key), m_index(index), m_getter(getter)
	{
	}

	virtual int Read(ConsoleInfoCiValueFile *file);			/* (0x810E8762) */
	virtual int Write(ConsoleInfoCiValueFile *prev, ConsoleInfoCiValueFile *cur,
			  ConsoleInfoCi *ci);				/* (0x810E87BC) */

	int m_index;
	Getter m_getter;
};

/* a network setting's field: its key, and the registry key giving its value in each network
   setting (a category with the setting's number, and a name) and the value's type (0x814148AC) */
struct ConsoleInfoCiNetField {
	ConsoleInfoCiNetField(ConsoleInfoCiKey key, const char *category, const char *name,
			      const char *type)
		: m_key(key), m_category(category), m_name(name), m_type(type)
	{
	}

	ConsoleInfoCiKey m_key;
	const char *m_category;
	const char *m_name;
	const char *m_type;
};

/* PLACEHOLDER names: the "ci" record's fields by kind, each kind's lookup by index and the
   values taken before the record is written (console_info_ci_fields.cpp) */
ConsoleInfoCiField *consoleInfoCiGetIntField(unsigned int index);
ConsoleInfoCiField *consoleInfoCiGetInt64Field(unsigned int index);
ConsoleInfoCiField *consoleInfoCiGetStringField(unsigned int index);
void consoleInfoCiUpdateCounts(void);
void consoleInfoCiUpdateStrings(void);
void consoleInfoCiUpdateHdmiMonitor(void);

/* PLACEHOLDER names: the Int fields the live area's and the media databases give (0x810F1924,
   0x810F1BDC, 0x810F1E28, 0x810F1E44) */
int consoleInfoCiGetLsdbCount(ConsoleInfoCiIntField *field, int *value);
int consoleInfoCiGetVideoCount(ConsoleInfoCiIntField *field, int *value);
int consoleInfoCiGetMusicCount(ConsoleInfoCiIntField *field, int *value);
int consoleInfoCiGetPhotoCount(ConsoleInfoCiIntField *field, int *value);

/* PLACEHOLDER: the console's activation data (an ini file's MID, DID, AID, OID, SVR and DIG
   entries), read by 0x81361B3E; the struct's and its fields' names are ours */
struct ShellActivationInfo {
	ShellActivationInfo()
		: aid(0), svr(0), hasDigest(false)
	{
	}
	__attribute__((always_inline)) ~ShellActivationInfo()
	{
	}

	paf::string mid;
	paf::string did;
	SceUInt64 aid;
	paf::string oid;
	int svr;
	bool hasDigest;
};

int shellActivationRead(ShellActivationInfo *info, int mode);

/* a device's size and free space (0x8137E308) */
int gameIoGetDeviceSpace(const char *device, SceUInt64 *maxSize, SceUInt64 *freeSize);

/* PLACEHOLDER: an NP account ID is in the registry (/CONFIG/NP account_id; 0x81329E20) */
int shellNpHasAccountId(int *has);

/* the library made ready, its folders made (0x810E5964), and with the sender (0x810E5954) */
int consoleInfoInit(void);
int shellConsoleInfoInit(void);

/* PLACEHOLDER names: the sender's state cleared (0x810F2174), and the sender made: its job
   queue ("SceConsoleInfoSendJobQueue") and its callbacks (0x810F2470) */
int consoleInfoSenderReset(void);
int consoleInfoSenderInit(void);
/* PLACEHOLDER: the job sending the records: its function, the function run when it is done and
   the function's argument (0x810F2FA4-0x810F302C) */
class ConsoleInfoSendDataJob : public paf::job::JobItem {
public:
	ConsoleInfoSendDataJob(int (*run)(int), int (*finish)(void), int arg);
	virtual ~ConsoleInfoSendDataJob();
	virtual int Run();
	virtual void Finish(int result);

	int (*m_run)(int);
	int (*m_finish)(void);
	int m_arg;
};

/* PLACEHOLDER: the sender's queue, and whether it is busy (0x810F2F4E) */
paf::job::JobQueue *consoleInfoSenderGetQueue(bool *busy);

/* a kind's record made, opened into the table, written, read, closed (0x810E59A8-0x810E5C30) */
ConsoleInfoRecord *consoleInfoCreate(int type, int entrySize, int *error);
int consoleInfoOpen(int type, int entrySize, ConsoleInfoRecord **record);
int consoleInfoWrite(ConsoleInfoRecord *record, void *data);
int consoleInfoReadEntry(ConsoleInfoRecord *record, int index, void *buf, int *size);
int consoleInfoClose(ConsoleInfoRecord *record, bool commit);
int consoleInfoGetTimestamp(int type, SceUInt64 *tick);

#endif /* _SHELL_CONSOLE_INFO_H */
