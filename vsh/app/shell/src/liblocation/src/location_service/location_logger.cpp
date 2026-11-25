/*
 * location_logger.cpp: location service logger, ux0:liblocation file writer, debug log
 * state (LocationLogFile), and ur0/ux0 data/history file utilities (0x81188F3E-0x81189D4C).
 * The file's and internal names here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <scetypes.h>
#include <liblocation.h>
#include <rtc.h>
#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <regmgr/regmgr.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/vector>
#include <std/new>
#include <memory/heap_allocator.h>

using namespace paf;

/* PLACEHOLDER externs for the rest of liblocation */
extern memory::HeapAllocator *g_locationHeap;			/* (0x815496B0) */
extern void *g_locationManager;					/* (0x815496B4) */
int locationTaskCancel(void *task);				/* (0x8118A800) */
int locationMgrRegisterTask(void *mgr, void *task);		/* (0x8118AACE) */
int locationMgrUnregisterTask(void *mgr, void *task);		/* (0x8118AB62) */
void *locationLogWriterCtor(void *mem, void *logger);		/* (0x8118AE80) */
int locationLogWriterStart(void *writer);			/* (0x8118B0AC) */
void locationLogWriterStop(void *writer);			/* (0x8118B24A) */
int locationLogWriterWrite(void *writer, const char *fmt, const char *tag,
			   va_list args, void *parser);		/* (0x8118B3D0) */
void locationLogWriterFlush(void);				/* (0x8118B5DC) */
const char *locationStrStr(const char *haystack, const char *needle); /* (0x8118F43C) */

extern "C" int sceIoSyncByFd(SceUID fd, int flag);

struct LocationLogWriterBase {
	virtual ~LocationLogWriterBase();
};

class LocationLogger;

class LocationLogParser {
public:
	virtual ~LocationLogParser()
	{
	}

	virtual void Parse(const char *line, LocationLogger *logger); /* (0x81188F3E) */
};

class LocationLogParserList {
public:
	virtual ~LocationLogParserList()
	{
	}

	virtual void Dispatch(const char *line, LocationLogger *logger)
	{
		for (size_t i = 0; i < m_parsers.size(); i++)
			m_parsers[i]->Parse(line, logger);
	}

	vector<LocationLogParser *> m_parsers;
};

struct LocationLogFile {
	LocationLogFile();					/* (0x81188FE8) */

	int field0;						/* +0 */
	int field4;						/* +4 */
	int field8;						/* +8 */
	int fieldC;						/* +12 */
	int field10;						/* +16 */
	char callbackTag[11];					/* +20..+30 */
	signed char flag1F;					/* +31 */
	int field20;						/* +32 */
	int field24;						/* +36 */
	char buf28[32];						/* +40..+71 */
	char buf48[16];						/* +72..+87 */
	char buf58[0x138];					/* +88..+399 */
	int field190;						/* +400 */
	int field194;						/* +404 */
	int field198;						/* +408 */
	int arr19C[5];						/* +412..+431 */
};

class LocationLogTimeoutTask {
public:
	virtual int OnNotify();					/* (0x81189D00) */

	int m_queued;						/* +4 */
	int m_active;						/* +8 */
	LocationLogger *m_logger;				/* +12 */
	char m_pad[16];						/* +16..+31 */
	char m_reserved20[24];					/* +32..+55 */
	unsigned char m_flag38;					/* +56 */
};

class LocationLogger {
public:
	LocationLogger();					/* (0x81189064) */

	int StopLog();						/* (0x811890B6) */
	int StartLog(SceUInt32 mask);				/* (0x811890FC) */
	void ResetFileLog();					/* (0x81189258) */
	int Init();						/* (0x811892B2) */
	int Term();						/* (0x8118945E) */
	const char *GetTag(int category);			/* (0x811894F2) */
	int VLog(int category, const char *fmt, va_list args);	/* (0x81189548) */
	int Log(int category, const char *fmt, ...);		/* (0x811895CC) */

	int GetField0();					/* (0x8118961E) */
	int GetField4();					/* (0x81189628) */
	int ClearUr0Files();					/* (0x81189632) */
	int SetField8(int val);					/* (0x811896EC) */
	int GetField8();					/* (0x811896F6) */
	int SetFieldC(int val);					/* (0x81189700) */
	int SetFileIndex(int index);				/* (0x8118970C) */
	const char *GetCallbackTag();				/* (0x81189712) */
	int TakeFlag1F();					/* (0x81189726) */
	void SetField10(int val);				/* (0x81189736) */
	int GetField10();					/* (0x8118973E) */
	int ResetDebugPermission();				/* (0x81189748) */
	int GetField20();					/* (0x81189762) */
	int SetField20(int val);				/* (0x8118976C) */
	int SetField24(int val);				/* (0x81189776) */
	int GetField24();					/* (0x81189780) */
	int SetBuf28(const void *src);				/* (0x8118978A) */
	int GetBuf28(void *dst);				/* (0x8118979C) */
	int GetBuf48(void *dst);				/* (0x811897BC) */
	int SetBuf48(const void *src);				/* (0x811897DC) */
	int CountUr0Files(int *outCount);			/* (0x811897EE) */
	int GetBuf58(void *dst);				/* (0x81189884) */
	SceSSize CopyFile(const char *src, const char *dst, int unused,
			  SceOff size);				/* (0x811898C8) */
	int CopyDatFiles(const char *srcDir, const char *dstDir); /* (0x81189A6E) */
	int SyncDatFiles(int fromUx0);				/* (0x81189B68) */
	int SetField190(int val);				/* (0x81189B98) */
	int GetField190();					/* (0x81189BA4) */
	int SetField194(int val);				/* (0x81189BB0) */
	int GetField194();					/* (0x81189BBC) */
	int ExportHistoryFiles();				/* (0x81189BC8) */
	int ImportHistoryFiles();				/* (0x81189BE0) */
	int ClearHistoryFiles();				/* (0x81189BF8) */
	int GetField198AndArr(int *outVal, int *outArr);	/* (0x81189CB2) */
	SceSSize WriteRaw(const void *buf, SceSize size);	/* (0x81189CE0) */
	int SyncRaw();						/* (0x81189CF4) */

	SceUID m_fd;						/* +0 */
	SceUInt32 m_mask;					/* +4 */
	LocationLogWriterBase *m_writer;			/* +8 */
	int m_fileIndex;					/* +12 */
	unsigned char m_ioError;				/* +16 */
	int m_mutexResult;					/* +20 */
	SceKernelLwMutexWork m_mutex;				/* +24..+55 */
	unsigned char m_flag38;					/* +56 */
	int m_field3C;						/* +60 */
	unsigned char m_flag40;					/* +64 */
	unsigned char m_flag41;					/* +65 */
	LocationLogParserList *m_parsers;			/* +68 */
	LocationLogFile *m_fileLog;				/* +72 */
	int m_field4C;						/* +76 */
	LocationLogTimeoutTask m_timeoutTask;			/* +80..+139 */
};

/* (0x81188F3E) */
void LocationLogParser::Parse(const char *line, LocationLogger *logger)
{
	const char *pos = locationStrStr(line, "O] invoking callback:");

	if (pos == NULL)
		return;

	char buf[16];

	memset(buf, 0, 15);
	char *open = sce_paf_strchr(pos, '(');

	if (open == NULL)
		return;

	char *close = sce_paf_strchr(open, ')');

	if (close == NULL)
		return;

	sce_paf_strncpy(buf, open, (size_t)(close - open + 1));
	if (logger->m_fileLog != NULL)
		sce_paf_strncpy(logger->m_fileLog->callbackTag, buf, 11);
}

/* (0x81188FE8) */
LocationLogFile::LocationLogFile()
{
	field0 = 0;
	field4 = 0;
	field8 = 1;
	fieldC = 0;
	field10 = 0;
	field20 = 0;
	field24 = 0;
	field190 = 0;
	field194 = 0;
	sce_paf_memset(buf28, 0, sizeof(buf28));
	field0 = 0;
	field4 = 0;
	sce_paf_strncpy(callbackTag, "(0+0+0)", 11);
	flag1F = 0;
	sce_paf_memset(buf48, 0, sizeof(buf48));
	sce_paf_memset(buf58, 0, sizeof(buf58));
	sce_paf_memset(arr19C, 0, sizeof(arr19C));
	field198 = 0;
	sce_paf_memset(arr19C, 0, sizeof(arr19C));
}

/* (0x81189064) */
LocationLogger::LocationLogger()
{
	m_fd = -1;
	m_mask = 0;
	m_writer = NULL;
	m_fileIndex = 0;
	m_mutexResult = -1;
	m_flag41 = 1;
	m_flag40 = 1;
	m_flag38 = 1;
	m_field3C = 1;
	m_fileLog = NULL;
	m_field4C = -1;
	m_timeoutTask.m_queued = 0;
	m_timeoutTask.m_active = 0;
	m_timeoutTask.m_logger = this;
	sce_paf_memset(m_timeoutTask.m_pad, 0, sizeof(m_timeoutTask.m_pad));
	m_timeoutTask.m_flag38 = 0;
}

/* (0x811890B6) */
int LocationLogger::StopLog()
{
	if (m_writer == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	m_mask = 0;
	locationLogWriterFlush();
	locationLogWriterStop(m_writer);
	if (m_writer != NULL)
		delete m_writer;
	m_writer = NULL;

	int ret = 0;

	if (m_fd >= 0) {
		ret = sceIoClose(m_fd);
		m_fd = -1;
	}
	return ret;
}

/* (0x811890FC) */
int LocationLogger::StartLog(SceUInt32 mask)
{
	SceUInt32 filtered = mask & ~0x10u;

	if (filtered == m_mask)
		return 0;

	if (m_writer != NULL) {
		m_mask = filtered;
		return 0;
	}

	int ret = sceIoMkdir("ux0:liblocation", 0606);

	if (ret != 0 && ret != (int)0x80010011) {
		m_ioError = 1;
	} else {
		SceDateTime time;
		char path[84];

		sceRtcGetCurrentClock(&time, 0);
		sce_paf_snprintf(path, 81, "%s/%s%04d%02d%02d_%02d%02d%02d_%03d.log",
				 "ux0:liblocation", "liblocation_",
				 time.year, time.month, time.day,
				 time.hour, time.minute, time.second, m_fileIndex);
		m_fd = sceIoOpen(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 0606);
		m_ioError = (m_fd < 0) ? 1 : 0;
		ret = m_fd;
	}

	if (ret >= 0) {
		void *mem = operator new(32992, g_locationHeap);
		LocationLogWriterBase *writer =
			(mem != NULL) ? (LocationLogWriterBase *)locationLogWriterCtor(mem, this)
				      : NULL;

		m_writer = writer;
		if (writer != NULL) {
			ret = locationLogWriterStart(writer);
			if (ret == 0)
				m_mask = filtered;
		}
		if (m_writer == NULL || ret != 0) {
			ret = SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
			if (m_fd >= 0) {
				sceIoClose(m_fd);
				m_fd = -1;
			}
		}
	}
	return ret;
}

/* (0x81189258) */
void LocationLogger::ResetFileLog()
{
	if (m_fileLog != NULL) {
		m_fileLog->field0 = 0;
		m_fileLog->field4 = 0;
		sce_paf_strncpy(m_fileLog->callbackTag, "(0+0+0)", 11);
		m_fileLog->flag1F = 0;
		sce_paf_memset(m_fileLog->buf48, 0, sizeof(m_fileLog->buf48));
		sce_paf_memset(m_fileLog->buf58, 0, sizeof(m_fileLog->buf58));
		sce_paf_memset(m_fileLog->arr19C, 0, sizeof(m_fileLog->arr19C));
		m_fileLog->field198 = 0;
		sce_paf_memset(m_fileLog->arr19C, 0, sizeof(m_fileLog->arr19C));
	}
}

/* (0x811892B2) */
int LocationLogger::Init()
{
	m_ioError = 0;
	ResetFileLog();

	LocationLogParserList *list = new (g_locationHeap) LocationLogParserList();

	if (list != NULL) {
		LocationLogParser *parser = new (g_locationHeap) LocationLogParser();

		if (parser != NULL)
			list->m_parsers.push_back(parser);
	}
	m_parsers = list;

	int enableLog = 0;

	sceRegMgrGetKeyInt("/CONFIG/LOCATION", "enable_log", &enableLog);
	if (enableLog != 0)
		StartLog((SceUInt32)enableLog);

	m_mutexResult = sceKernelCreateLwMutex(&m_mutex, "SceLocationGpsData", 2, 0, NULL);
	if (m_mutexResult == 0) {
		if (g_locationManager != NULL)
			return locationMgrRegisterTask(g_locationManager, &m_timeoutTask);
		return 0;
	}

	if (m_parsers != NULL) {
		delete m_parsers;
		m_parsers = NULL;
	}
	if (enableLog != 0)
		StopLog();
	if (m_mutexResult == 0 && g_locationManager != NULL)
		return locationMgrRegisterTask(g_locationManager, &m_timeoutTask);
	return m_mutexResult;
}

/* (0x8118945E) */
int LocationLogger::Term()
{
	if (m_timeoutTask.m_queued != 0 && locationTaskCancel(&m_timeoutTask) == 0 &&
	    g_locationManager != NULL)
		locationMgrUnregisterTask(g_locationManager, &m_timeoutTask);

	if (m_mutexResult == 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}

	int ret = StopLog();

	if (m_parsers != NULL) {
		delete m_parsers;
		m_parsers = NULL;
	}
	if (m_fileLog != NULL) {
		delete m_fileLog;
		m_fileLog = NULL;
	}
	return ret;
}

/* (0x811894F2) */
__attribute__((noinline))
const char *LocationLogger::GetTag(int category)
{
	switch (category) {
	case 1:
		return "[skyhook]";
	case 2:
		return "[driver_if]";
	case 4:
		return "[nmea]";
	case 8:
		return "[location]";
	case 16:
		return "[ext]";
	default:
		return "[etc]";
	}
}

/* (0x81189548) */
int LocationLogger::VLog(int category, const char *fmt, va_list args)
{
	if (m_writer != NULL && (m_mask & (SceUInt32)category) != 0) {
		const char *tag = GetTag(category);
		void *parser = (category == 1) ? (void *)m_parsers : NULL;

		return locationLogWriterWrite(m_writer, fmt, tag, args, parser);
	}
	if (category == 1) {
		char buf[132];

		sce_paf_vsnprintf(buf, 129, fmt, args);
		if (m_parsers != NULL) {
			buf[128] = '\0';
			m_parsers->Dispatch(buf, this);
		}
	}
	return 0;
}

/* (0x811895CC) */
int LocationLogger::Log(int category, const char *fmt, ...)
{
	int ret = 0;
	va_list args;

	va_start(args, fmt);
	if (m_writer != NULL && (m_mask & (SceUInt32)category) != 0)
		ret = VLog(category, fmt, args);
	va_end(args);
	return ret;
}

/* (0x8118961E) */
int LocationLogger::GetField0()
{
	if (m_fileLog != NULL)
		return m_fileLog->field0;
	return 0;
}

/* (0x81189628) */
int LocationLogger::GetField4()
{
	if (m_fileLog != NULL)
		return m_fileLog->field4;
	return 0;
}

/* (0x81189632) */
int LocationLogger::ClearUr0Files()
{
	SceUID dfd = sceIoDopen("ur0:liblocation");

	if (dfd < 0)
		return 0;

	int ret = 0;
	SceIoDirent dir;

	memset(&dir, 0, sizeof(dir));
	while (sceIoDread(dfd, &dir) > 0 && ret >= 0) {
		char path[136];

		path[128] = '\0';
		sce_paf_snprintf(path, 129, "%s/%s", "ur0:liblocation", dir.d_name);
		if ((dir.d_stat.st_mode & 0xF000) != 0x1000) {
			sce_paf_printf_dummy();
			ret = sceIoRemove(path);
		}
		sce_paf_memset(&dir, 0, sizeof(dir));
	}
	sceIoDclose(dfd);
	return ret;
}

/* (0x811896EC) */
int LocationLogger::SetField8(int val)
{
	if (m_fileLog != NULL)
		m_fileLog->field8 = val;
	return 0;
}

/* (0x811896F6) */
int LocationLogger::GetField8()
{
	if (m_fileLog != NULL)
		return m_fileLog->field8;
	return 1;
}

/* (0x81189700) */
int LocationLogger::SetFieldC(int val)
{
	if (m_fileLog != NULL)
		m_fileLog->fieldC = val;
	return 0;
}

/* (0x8118970C) */
int LocationLogger::SetFileIndex(int index)
{
	m_fileIndex = index;
	return 0;
}

/* (0x81189712) */
const char *LocationLogger::GetCallbackTag()
{
	if (m_fileLog != NULL)
		return m_fileLog->callbackTag;
	return "(0+0+0)";
}

/* (0x81189726) */
int LocationLogger::TakeFlag1F()
{
	if (m_fileLog != NULL) {
		int val = m_fileLog->flag1F;

		m_fileLog->flag1F = 0;
		return val;
	}
	return 0;
}

/* (0x81189736) */
void LocationLogger::SetField10(int val)
{
	if (m_fileLog != NULL)
		m_fileLog->field10 = val;
}

/* (0x8118973E) */
int LocationLogger::GetField10()
{
	if (m_fileLog != NULL)
		return m_fileLog->field10;
	return 0;
}

/* (0x81189748) */
int LocationLogger::ResetDebugPermission()
{
	return sceRegMgrSetKeyInt("/CONFIG/LOCATION", "debug_permission_status", 3);
}

/* (0x81189762) */
int LocationLogger::GetField20()
{
	if (m_fileLog != NULL)
		return m_fileLog->field20;
	return 0;
}

/* (0x8118976C) */
int LocationLogger::SetField20(int val)
{
	if (m_fileLog != NULL)
		m_fileLog->field20 = val;
	return 0;
}

/* (0x81189776) */
int LocationLogger::SetField24(int val)
{
	if (m_fileLog != NULL)
		m_fileLog->field24 = val;
	return 0;
}

/* (0x81189780) */
int LocationLogger::GetField24()
{
	if (m_fileLog != NULL)
		return m_fileLog->field24;
	return 0;
}

/* (0x8118978A) */
int LocationLogger::SetBuf28(const void *src)
{
	if (m_fileLog != NULL)
		sce_paf_memcpy(m_fileLog->buf28, src, sizeof(m_fileLog->buf28));
	return 0;
}

/* (0x8118979C) */
int LocationLogger::GetBuf28(void *dst)
{
	if (m_fileLog != NULL)
		sce_paf_memcpy(dst, m_fileLog->buf28, sizeof(m_fileLog->buf28));
	else
		sce_paf_memset(dst, 0, sizeof(m_fileLog->buf28));
	return 0;
}

/* (0x811897BC) */
int LocationLogger::GetBuf48(void *dst)
{
	if (m_fileLog != NULL)
		sce_paf_memcpy(dst, m_fileLog->buf48, sizeof(m_fileLog->buf48));
	else
		sce_paf_memset(dst, 0, sizeof(m_fileLog->buf48));
	return 0;
}

/* (0x811897DC) */
int LocationLogger::SetBuf48(const void *src)
{
	if (m_fileLog != NULL)
		sce_paf_memcpy(m_fileLog->buf48, src, sizeof(m_fileLog->buf48));
	return 0;
}

/* (0x811897EE) */
int LocationLogger::CountUr0Files(int *outCount)
{
	int count = 0;
	SceUID dfd = sceIoDopen("ur0:liblocation");

	if (dfd >= 0) {
		SceIoDirent dir;

		memset(&dir, 0, sizeof(dir));
		while (sceIoDread(dfd, &dir) > 0 && count >= 0) {
			if ((dir.d_stat.st_mode & 0xF000) != 0x1000)
				count++;
			sce_paf_memset(&dir, 0, sizeof(dir));
		}
		sceIoDclose(dfd);
	}
	*outCount = count;
	if (count < 0)
		return count;
	return 0;
}

/* (0x81189884) */
int LocationLogger::GetBuf58(void *dst)
{
	if (m_fileLog != NULL) {
		if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
			sce_paf_memcpy(dst, m_fileLog->buf58, sizeof(m_fileLog->buf58));
			sceKernelUnlockLwMutex(&m_mutex, 1);
		}
	} else {
		sce_paf_memset(dst, 0, sizeof(m_fileLog->buf58));
	}
	return 0;
}

/* (0x811898C8) */
SceSSize LocationLogger::CopyFile(const char *src, const char *dst, int unused, SceOff size)
{
	SceUID srcFd = sceIoOpen(src, SCE_O_RDONLY, 06);

	if (srcFd < 0)
		return srcFd;

	SceSize prefixLen = sce_paf_strlen("ux0:liblocation");
	SceUID dstFd;

	if (sce_paf_strncmp("ux0:liblocation", dst, prefixLen) != 0)
		dstFd = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 06);
	else
		dstFd = sceIoOpen(dst, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 0606);

	SceSSize ret = 0;

	if (dstFd < 0) {
		ret = dstFd;
		sceIoClose(srcFd);
		return ret;
	}

	int copied = 0;
	char buf[4096];

	while (copied < size && ret >= 0) {
		sce_paf_memset(buf, 0, sizeof(buf));
		SceOff remain = size - copied;
		SceSize chunk = (remain > 4096) ? 4096 : (SceSize)remain;

		ret = sceIoRead(srcFd, buf, chunk);
		if (ret < 0)
			break;
		ret = sceIoWrite(dstFd, buf, (SceSize)ret);
		if (ret < 0)
			break;
		copied += ret;
	}
	sceIoClose(srcFd);
	sceIoClose(dstFd);
	return ret;
}

/* (0x81189A6E) */
int LocationLogger::CopyDatFiles(const char *srcDir, const char *dstDir)
{
	int ret = 0;
	SceIoDirent dir;

	memset(&dir, 0, sizeof(dir));
	SceUID dfd = sceIoDopen(srcDir);

	if (dfd >= 0) {
		ret = sceIoMkdir(dstDir, 0606);
		if (ret == 0 || ret == (int)0x80010011) {
			ret = 0;
			while (sceIoDread(dfd, &dir) > 0 && ret >= 0) {
				char srcPath[132];
				char dstPath[132];

				srcPath[128] = '\0';
				dstPath[128] = '\0';
				char *tok = sce_paf_private_strtok(dir.d_name, ".dat");

				if (tok != NULL && tok[4] == '\0') {
					sce_paf_snprintf(srcPath, 129, "%s/%s", srcDir,
							 dir.d_name);
					sce_paf_snprintf(dstPath, 129, "%s/%s", dstDir,
							 dir.d_name);
					ret = CopyFile(srcPath, dstPath, 0, dir.d_stat.st_size);
				}
			}
		}
		sceIoDclose(dfd);
	}
	return ret;
}

/* (0x81189B68) */
int LocationLogger::SyncDatFiles(int fromUx0)
{
	if (fromUx0 != 0)
		return CopyDatFiles("ux0:liblocation", "ur0:liblocation");
	return CopyDatFiles("ur0:liblocation", "ux0:liblocation");
}

/* (0x81189B98) */
int LocationLogger::SetField190(int val)
{
	if (m_fileLog != NULL)
		m_fileLog->field190 = val;
	return 0;
}

/* (0x81189BA4) */
int LocationLogger::GetField190()
{
	if (m_fileLog != NULL)
		return m_fileLog->field190;
	return 0;
}

/* (0x81189BB0) */
int LocationLogger::SetField194(int val)
{
	if (m_fileLog != NULL)
		m_fileLog->field194 = val;
	return 0;
}

/* (0x81189BBC) */
int LocationLogger::GetField194()
{
	if (m_fileLog != NULL)
		return m_fileLog->field194;
	return 0;
}

/* (0x81189BC8) */
int LocationLogger::ExportHistoryFiles()
{
	return CopyDatFiles("ur0:liblocation\\history", "ux0:liblocation\\history");
}

/* (0x81189BE0) */
int LocationLogger::ImportHistoryFiles()
{
	return CopyDatFiles("ux0:liblocation\\history", "ur0:liblocation\\history");
}

/* (0x81189BF8) */
int LocationLogger::ClearHistoryFiles()
{
	SceUID dfd = sceIoDopen("ur0:liblocation\\history");

	if (dfd < 0)
		return 0;

	int ret = 0;
	SceIoDirent dir;

	memset(&dir, 0, sizeof(dir));
	while (sceIoDread(dfd, &dir) > 0 && ret >= 0) {
		char path[136];

		path[128] = '\0';
		sce_paf_snprintf(path, 129, "%s/%s", "ur0:liblocation\\history", dir.d_name);
		if ((dir.d_stat.st_mode & 0xF000) != 0x1000) {
			sce_paf_printf_dummy();
			ret = sceIoRemove(path);
		}
		sce_paf_memset(&dir, 0, sizeof(dir));
	}
	sceIoDclose(dfd);
	return ret;
}

/* (0x81189CB2) */
int LocationLogger::GetField198AndArr(int *outVal, int *outArr)
{
	if (m_fileLog != NULL) {
		*outVal = m_fileLog->field198;
		outArr[0] = m_fileLog->arr19C[0];
		outArr[1] = m_fileLog->arr19C[1];
		outArr[2] = m_fileLog->arr19C[2];
		outArr[3] = m_fileLog->arr19C[3];
		outArr[4] = m_fileLog->arr19C[4];
	}
	return 0;
}

/* (0x81189CE0) */
SceSSize LocationLogger::WriteRaw(const void *buf, SceSize size)
{
	SceSSize ret = sceIoWrite(m_fd, buf, size);

	if (ret < 0)
		m_ioError = 1;
	return ret;
}

/* (0x81189CF4) */
int LocationLogger::SyncRaw()
{
	return sceIoSyncByFd(m_fd, 0);
}

/* (0x81189D00) */
int LocationLogTimeoutTask::OnNotify()
{
	if (m_logger->m_writer != NULL) {
		m_logger->m_mask = 0;
		sceRegMgrSetKeyInt("/CONFIG/LOCATION", "enable_log", 1);
		locationLogWriterStop(m_logger->m_writer);
		if (m_logger->m_writer != NULL)
			delete m_logger->m_writer;
		m_logger->m_writer = NULL;
		if (m_logger->m_fd >= 0) {
			sceIoClose(m_logger->m_fd);
			m_logger->m_fd = -1;
		}
	}
	return 0;
}
