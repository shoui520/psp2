/*
 * SceIniFileProcessor: the library's own types. The processor reads and writes INI files; its work
 * goes through an allocator that is either the application's (a pair of functions) or a pool the
 * library makes from a memory block.
 */

#pragma once

#include <scetypes.h>
#include <stddef.h>
#include <kernel/sysmem/memblock.h>
#include <libkernel/libkernel.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>

/* the libkernel mspace calls the tree has no header of yet */
typedef void *SceClibMspace;
extern "C" {
void *sceClibMspaceMalloc(SceClibMspace mspace, SceSize size);
void sceClibMspaceFree(SceClibMspace mspace, void *ptr);
SceClibMspace sceClibMspaceCreate(void *base, SceSize size);
int sceClibMspaceDestroy(SceClibMspace mspace);
}

#define SCE_INI_FILE_PROCESSOR_ERROR_OUT_OF_MEMORY	-2138832895	/* 0x80840001 */
#define SCE_INI_FILE_PROCESSOR_ERROR_NOT_OPEN		-2138832894	/* 0x80840002 */
#define SCE_INI_FILE_PROCESSOR_ERROR_INVALID_ARGUMENT	-2138832890	/* 0x80840006 */

/* the application's allocator: one function to take memory and one to give it back */
struct IniAllocatorOps {
	void *(*alloc)(SceSize size, void *context);
	void (*free)(void *ptr, void *context);
};

/* the library's pool of 4096 byte blocks, which it makes from a memory block when it is first used */
struct IniPool : IniAllocatorOps {
	SceUID memBlock;
	SceClibMspace mspace;
	SceSize size;
};

/* what a processor allocates with: the functions, their context and whether the library owns them */
struct IniAllocatorRef {
	IniAllocatorOps *ops;
	void *context;
	bool owned;
};

/* what the application gives the processor: the size of the pool, or its own allocator */
struct IniConfig {
	SceSize poolSize;
	SceUInt32 reserved;
	IniAllocatorOps *ops;
	void *context;
};

extern int g_poolsCreated;	/* 0x81005010 */
extern int g_poolsDestroyed;	/* 0x81005014 */

extern IniAllocatorRef *iniAllocatorRefInit(IniAllocatorRef *ref);
extern IniAllocatorRef *iniAllocatorRefRelease(IniAllocatorRef *ref);
extern int iniAllocatorRefSet(IniAllocatorRef *ref, const IniConfig *config);
extern int iniAllocatorRefCreatePool(IniAllocatorRef *ref, SceSize size);
extern void *iniAlloc(IniAllocatorRef *ref, SceSize size);
extern void iniFree(IniAllocatorRef *ref, void *ptr);
extern IniAllocatorOps *iniDefaultAllocator(void);

/* the text a scanner reads: its bytes, where to start, whether a byte order mark may open it, and its kind */
struct IniInput {
	const char *data;
	SceSize size;
	SceSize offset;
	bool checkBom;
	int kind;		/* 2: a value without a key, read whole */
};

/* what the scanner found: the kind of line, how it went, and where its parts are */
enum {
	INI_LINE_BLANK = 0,	/* spaces, a tab or an equals sign opens it */
	INI_LINE_PAIR = 1,	/* key = value */
	INI_LINE_EMPTY = 2,	/* nothing before the line ends */
	INI_LINE_COMMENT = 3,	/* ';' */
	INI_LINE_SECTION = 4	/* '[' */
};

struct IniToken {
	int line;		/* INI_LINE_* */
	int error;		/* 1: no end of key, 2: no end of value */
	SceSize offset;		/* where the token's line begins in the text */
	const char *key;
	SceSize keyLength;
	const char *value;
	SceSize valueLength;
	const char *newline;	/* the line end that was read: "\n", "\r" or "\r\n" */
	SceSize newlineLength;
	SceSize position;	/* where the scanner stopped */
	SceSize carry;
};

/* the scanner's steps */
enum {
	INI_STEP_START = 0,
	INI_STEP_BOM = 1,
	INI_STEP_LINE = 2,
	INI_STEP_KEY = 3,
	INI_STEP_VALUE = 4,
	INI_STEP_NEWLINE_BEGIN = 5,
	INI_STEP_END = 6,
	INI_STEP_NEWLINE = 7
};

#define INI_RESULT_END			8650754		/* 0x840002: the text ended */
#define INI_RESULT_BAD_ENCODING		-2138832892	/* 0x80840004: a UTF-16 byte order mark */
#define INI_RESULT_TEXT_TOO_SHORT	-2138832888	/* 0x80840008 */

struct IniScanner {
	const void *vtable;
	const unsigned char *cursor;
	const unsigned char *end;
	IniToken token;
	SceSize bomKind;	/* 1: none, 2: UTF-8, 3 and 4: UTF-16 */
	SceSize step;
};

extern IniInput *iniInputInit(IniInput *input);
extern int iniScannerScan(IniScanner *scanner, const IniInput *input, IniToken *out);
extern IniToken *iniTokenReset(IniToken *token);

/* a string with its allocator: its bytes (the shared empty string when it has none), their number, and the
 * functions and context that gave them */
struct IniString {
	char *data;
	SceSize length;
	IniAllocatorOps *ops;
	void *context;
};

extern char g_iniEmptyString[];		/* 0x81005000 */

extern IniString *iniStringAssign(IniString *string, const IniString *source);
extern IniString *iniStringAssignChars(IniString *string, const char *text, SceSize length);
extern IniString *iniStringDestroy(IniString *string);
extern IniString *iniStringInsert(IniString *string, SceSize position, const void *text, SceSize length);
extern IniString *iniStringAppend(IniString *string, const void *text, SceSize length);
extern IniString *iniStringSubstr(IniString *result, const IniString *source, SceSize position, SceSize length);
extern SceSize iniPathDeviceLength(const char *path);
extern const char *iniPathFileName(const char *path);

#define SCE_INI_FILE_PROCESSOR_ERROR_NO_FILE_NAME	-2138832887	/* 0x80840009 */

class IniBuffer;
/* the buffer a copy goes through (its other methods are not known yet) */
class IniBuffer {
public:
	virtual ~IniBuffer();
	virtual int unused2();
	virtual int unused3();
	virtual int unused4();
	virtual void *data();
	virtual SceSize size();
};

#define INI_RESULT_STREAM_END	8650753		/* 0x840001 */

/* what the processor asks of a file: the base keeps the name, the mode and the last error */
class IniFileBase {
public:
	IniFileBase(IniAllocatorRef *allocator);
	virtual ~IniFileBase();
	virtual int open(const char *path, int mode, int flags) = 0;
	virtual int read(void *buf, SceSize size, SceSSize *readSize) = 0;
	virtual int write(const void *buf, SceSize size) = 0;
	virtual int seek(SceOff offset, int origin) = 0;
	virtual int close() = 0;
	virtual int sync() = 0;
	virtual int stat() = 0;
	virtual int rewind() = 0;
	virtual int replace(IniFileBase *source) = 0;
	virtual SceOff size() = 0;
	virtual SceOff tell() = 0;
	virtual const IniString *pathOf() = 0;
	virtual int unused14() = 0;
	virtual int copyFrom(IniFileBase *source, SceOff offset, SceSize limit, IniBuffer *buffer) = 0;
	virtual const IniString *path();
	virtual int openMode();
	virtual int errorCode();
	virtual int reopen() = 0;
	virtual void unused20();
	virtual void onError(int error);

	IniString m_path;
	int m_mode;
	IniAllocatorRef *m_allocator;
	int m_error;
	int m_flags;
};

/* a file on the system */
class IniFile : public IniFileBase {
public:
	IniFile(IniAllocatorRef *allocator);
	virtual ~IniFile();
	virtual int open(const char *path, int mode, int flags);
	virtual int read(void *buf, SceSize size, SceSSize *readSize);
	virtual int write(const void *buf, SceSize size);
	virtual int seek(SceOff offset, int origin);
	virtual int close();
	virtual int sync();
	virtual int stat();
	virtual int rewind();
	virtual int replace(IniFileBase *source);
	virtual SceOff size();
	virtual SceOff tell();
	virtual const IniString *pathOf();
	virtual int unused14();
	virtual int copyFrom(IniFileBase *source, SceOff offset, SceSize limit, IniBuffer *buffer);
	virtual int reopen();

	SceUID m_fd;
	int m_reserved;
};

/* a file the processor writes to first and then puts in the place of the real one: it has a name of its own
 * (made from the real name and the time) and is removed when it is closed, unless asked to stay */
class IniTempFile : public IniFile {
public:
	IniTempFile(IniAllocatorRef *allocator);
	virtual ~IniTempFile();
	virtual int open(const char *path, int mode, int flags);
	virtual int copyFrom(IniFileBase *source, SceOff offset, SceSize limit, IniBuffer *buffer);

	bool m_autoDelete;
};

extern int iniMakeTempName(IniFileBase *file, const char *path, IniString *name);

/* a temporary file that gathers what is written in a buffer and writes it in blocks */
class IniBufferedFile : public IniTempFile {
public:
	IniBufferedFile(IniAllocatorRef *allocator, SceSize capacity);
	virtual ~IniBufferedFile();
	virtual int write(const void *buf, SceSize size);
	virtual int sync();

	char *m_buffer;
	SceSize m_used;
	SceSize m_capacity;
};
