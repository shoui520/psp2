/*
 * location_logger.cpp: location service logger, ux0:liblocation file writer, debug info
 * state (LocationDebugInfo), and ur0/ux0 data/history file utilities (0x81188F3E-0x81189D4C).
 * The file's and internal names here are ours (PLACEHOLDER).
 */
#include "location_internal.h"
#include <string.h>
#include <kernel/iofilemgr.h>
#include <regmgr/regmgr.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/new>

extern "C" int sceIoSyncByFd(SceUID fd, int flag);

/* (0x81188F3E) */
void LocationLogParser::Parse(const char *line, LocationLogger *logger)
{
	const char *pos = locationStrNStr(line, "O] invoking callback:", sce_paf_strlen(line));

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
	if (logger->m_debugInfo != NULL)
		sce_paf_strncpy(logger->m_debugInfo->callbackTag, buf, 11);
}

/* (0x81188FE8) */
LocationDebugInfo::LocationDebugInfo()
{
	gpsSatelliteCount = 0;
	wifiApCount = 0;
	xtraAssistEnabled = 1;
	fakeWifiAs3g = 0;
	intervalOverrideSec = 0;
	xtraState = 0;
	xtraLocalFileOnly = 0;
	forceXtraDownload = 0;
	xtraSourceMode = 0;
	sce_paf_memset(dopBounds, 0, sizeof(dopBounds));
	gpsSatelliteCount = 0;
	wifiApCount = 0;
	sce_paf_strncpy(callbackTag, "(0+0+0)", 11);
	sensorActiveMask = 0;
	sce_paf_memset(dopInfo, 0, sizeof(dopInfo));
	sce_paf_memset(&gpsSnapshot, 0, sizeof(gpsSnapshot));
	sce_paf_memset(cellInfo, 0, sizeof(cellInfo));
	cellRadioIf = 0;
	sce_paf_memset(cellInfo, 0, sizeof(cellInfo));
}

/* (0x81189064) */
LocationLogger::LocationLogger()
{
	m_fd = -1;
	m_mask = 0;
	m_writer = NULL;
	m_fileIndex = 0;
	m_mutexResult = -1;
	m_logTimestampEnabled = 1;
	m_logPrefixEnabled = 1;
	m_logWriteEnabled = 1;
	m_logFlushMode = 1;
	m_debugInfo = NULL;
	m_logSyncFd = -1;
	m_timeoutTask.m_mgr = NULL;
	m_timeoutTask.m_slotIndex = 0;
	m_timeoutTask.m_logger = this;
	sce_paf_memset(&m_timeoutTask.m_startTime, 0, sizeof(m_timeoutTask.m_startTime));
	m_timeoutExpired = 0;
}

/* (0x811890B6) */
int LocationLogger::StopLog()
{
	if (m_writer == NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	m_mask = 0;
	m_writer->FlushAll();
	m_writer->Stop();
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
		LocationLogWriter *writer = new (g_locationHeap) LocationLogWriter(this);

		m_writer = writer;
		if (writer != NULL) {
			ret = writer->Start();
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
void LocationLogger::ResetDebugInfo()
{
	if (m_debugInfo != NULL) {
		m_debugInfo->gpsSatelliteCount = 0;
		m_debugInfo->wifiApCount = 0;
		sce_paf_strncpy(m_debugInfo->callbackTag, "(0+0+0)", 11);
		m_debugInfo->sensorActiveMask = 0;
		sce_paf_memset(m_debugInfo->dopInfo, 0, sizeof(m_debugInfo->dopInfo));
		sce_paf_memset(&m_debugInfo->gpsSnapshot, 0, sizeof(m_debugInfo->gpsSnapshot));
		sce_paf_memset(m_debugInfo->cellInfo, 0, sizeof(m_debugInfo->cellInfo));
		m_debugInfo->cellRadioIf = 0;
		sce_paf_memset(m_debugInfo->cellInfo, 0, sizeof(m_debugInfo->cellInfo));
	}
}

/* (0x811892B2) */
int LocationLogger::Init()
{
	m_ioError = 0;
	ResetDebugInfo();

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
			return g_locationManager->RegisterTask(&m_timeoutTask);
		return 0;
	}

	if (m_parsers != NULL) {
		delete m_parsers;
		m_parsers = NULL;
	}
	if (enableLog != 0)
		StopLog();
	if (m_mutexResult == 0 && g_locationManager != NULL)
		return g_locationManager->RegisterTask(&m_timeoutTask);
	return m_mutexResult;
}

/* (0x8118945E) */
int LocationLogger::Term()
{
	if (m_timeoutTask.m_mgr != NULL && m_timeoutTask.Cancel() == 0 &&
	    g_locationManager != NULL)
		g_locationManager->UnregisterTask(&m_timeoutTask);

	if (m_mutexResult == 0) {
		sceKernelDeleteLwMutex(&m_mutex);
		m_mutexResult = -1;
	}

	int ret = StopLog();

	if (m_parsers != NULL) {
		delete m_parsers;
		m_parsers = NULL;
	}
	if (m_debugInfo != NULL) {
		delete m_debugInfo;
		m_debugInfo = NULL;
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
		LocationLogParserList *parser = (category == 1) ? m_parsers : NULL;

		return m_writer->Write(fmt, tag, args, parser);
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
int LocationLogger::GetGpsSatelliteCount()
{
	if (m_debugInfo != NULL)
		return m_debugInfo->gpsSatelliteCount;
	return 0;
}

/* (0x81189628) */
int LocationLogger::GetWifiApCount()
{
	if (m_debugInfo != NULL)
		return m_debugInfo->wifiApCount;
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
int LocationLogger::SetXtraAssistEnabled(int enabled)
{
	if (m_debugInfo != NULL)
		m_debugInfo->xtraAssistEnabled = enabled;
	return 0;
}

/* (0x811896F6) */
int LocationLogger::GetXtraAssistEnabled()
{
	if (m_debugInfo != NULL)
		return m_debugInfo->xtraAssistEnabled;
	return 1;
}

/* (0x81189700) */
int LocationLogger::SetFakeWifiAs3g(int enabled)
{
	if (m_debugInfo != NULL)
		m_debugInfo->fakeWifiAs3g = enabled;
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
	if (m_debugInfo != NULL)
		return m_debugInfo->callbackTag;
	return "(0+0+0)";
}

/* (0x81189726) */
int LocationLogger::TakeSensorActiveMask()
{
	if (m_debugInfo != NULL) {
		int val = m_debugInfo->sensorActiveMask;

		m_debugInfo->sensorActiveMask = 0;
		return val;
	}
	return 0;
}

/* (0x81189736) */
void LocationLogger::SetIntervalOverrideSec(int sec)
{
	if (m_debugInfo != NULL)
		m_debugInfo->intervalOverrideSec = sec;
}

/* (0x8118973E) */
int LocationLogger::GetIntervalOverrideSec()
{
	if (m_debugInfo != NULL)
		return m_debugInfo->intervalOverrideSec;
	return 0;
}

/* (0x81189748) */
int LocationLogger::ResetDebugPermission()
{
	return sceRegMgrSetKeyInt("/CONFIG/LOCATION", "debug_permission_status", 3);
}

/* (0x81189762) */
int LocationLogger::GetXtraState()
{
	if (m_debugInfo != NULL)
		return m_debugInfo->xtraState;
	return 0;
}

/* (0x8118976C) */
int LocationLogger::SetXtraState(int state)
{
	if (m_debugInfo != NULL)
		m_debugInfo->xtraState = state;
	return 0;
}

/* (0x81189776) */
int LocationLogger::SetXtraLocalFileOnly(int enabled)
{
	if (m_debugInfo != NULL)
		m_debugInfo->xtraLocalFileOnly = enabled;
	return 0;
}

/* (0x81189780) */
int LocationLogger::GetXtraLocalFileOnly()
{
	if (m_debugInfo != NULL)
		return m_debugInfo->xtraLocalFileOnly;
	return 0;
}

/* (0x8118978A) */
int LocationLogger::SetDopBounds(const float *bounds)
{
	if (m_debugInfo != NULL)
		sce_paf_memcpy(m_debugInfo->dopBounds, bounds, sizeof(m_debugInfo->dopBounds));
	return 0;
}

/* (0x8118979C) */
int LocationLogger::GetDopBounds(float *outBounds)
{
	if (m_debugInfo != NULL)
		sce_paf_memcpy(outBounds, m_debugInfo->dopBounds, sizeof(m_debugInfo->dopBounds));
	else
		sce_paf_memset(outBounds, 0, sizeof(m_debugInfo->dopBounds));
	return 0;
}

/* (0x811897BC) */
int LocationLogger::GetDopInfo(float *outDop)
{
	if (m_debugInfo != NULL)
		sce_paf_memcpy(outDop, m_debugInfo->dopInfo, sizeof(m_debugInfo->dopInfo));
	else
		sce_paf_memset(outDop, 0, sizeof(m_debugInfo->dopInfo));
	return 0;
}

/* (0x811897DC) */
int LocationLogger::SetDopInfo(const float *dop)
{
	if (m_debugInfo != NULL)
		sce_paf_memcpy(m_debugInfo->dopInfo, dop, sizeof(m_debugInfo->dopInfo));
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
int LocationLogger::GetGpsSnapshot(void *dst)
{
	if (m_debugInfo != NULL) {
		if (sceKernelLockLwMutex(&m_mutex, 1, NULL) == 0) {
			sce_paf_memcpy(dst, &m_debugInfo->gpsSnapshot,
				       sizeof(m_debugInfo->gpsSnapshot));
			sceKernelUnlockLwMutex(&m_mutex, 1);
		}
	} else {
		sce_paf_memset(dst, 0, sizeof(m_debugInfo->gpsSnapshot));
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
int LocationLogger::SetForceXtraDownload(int enabled)
{
	if (m_debugInfo != NULL)
		m_debugInfo->forceXtraDownload = enabled;
	return 0;
}

/* (0x81189BA4) */
int LocationLogger::GetForceXtraDownload()
{
	if (m_debugInfo != NULL)
		return m_debugInfo->forceXtraDownload;
	return 0;
}

/* (0x81189BB0) */
int LocationLogger::SetXtraSourceMode(int mode)
{
	if (m_debugInfo != NULL)
		m_debugInfo->xtraSourceMode = mode;
	return 0;
}

/* (0x81189BBC) */
int LocationLogger::GetXtraSourceMode()
{
	if (m_debugInfo != NULL)
		return m_debugInfo->xtraSourceMode;
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
int LocationLogger::GetCellInfo(int *outRadioIf, int *outCellInfo)
{
	if (m_debugInfo != NULL) {
		*outRadioIf = m_debugInfo->cellRadioIf;
		outCellInfo[0] = m_debugInfo->cellInfo[0];
		outCellInfo[1] = m_debugInfo->cellInfo[1];
		outCellInfo[2] = m_debugInfo->cellInfo[2];
		outCellInfo[3] = m_debugInfo->cellInfo[3];
		outCellInfo[4] = m_debugInfo->cellInfo[4];
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
		m_logger->m_writer->Stop();
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
