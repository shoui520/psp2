/*
 * qualcomm_adapter.cpp: QualCommAdapter (A-GPS time injection, XTRA ephemeris injection, and
 * "SceLocationAGps" / "SceLocationXtra" worker threads; 0x811976D0-0x81198C94, sti 0x81198C24).
 * Sony's class name ("QualCommAdapter::OnGpsNotification()") comes from its log string.
 */
#include "location_service.h"
#include <libhttp.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/new>

extern "C" {
int sceRtcGetCurrentRetainedNetworkTick(SceRtcTick *tick);
int sceRtcGetCurrentGpsTick(SceRtcTick *tick);
}

extern void *g_locationLogger;				/* 0x815496B8 */
extern void *g_locationXtra;				/* 0x815496BC */
extern void locationLoggerPrintf(void *logger, int level, const char *fmt, ...);
extern int locationLoggerGetField8(void *logger);	/* 0x811896F6 */
extern int locationLoggerSetXtraState(void *logger, int state); /* 0x8118976C */
extern int locationLoggerGetField24(void *logger);	/* 0x81189780 */
extern int locationLoggerGetField190(void *logger);	/* 0x81189BA4 */
extern int locationLoggerGetField194(void *logger);	/* 0x81189BBC */

extern int gpsDrvOpen(SceUID cbId);			/* 0x8144FDA4 */
extern int gpsDrvClose(void);				/* 0x8144FDAC */
extern int gpsDrvStart(int mode);			/* 0x8144FDB4 */
extern int gpsDrvStop(void);				/* 0x8144FDBC */
extern int gpsDrvIoctl(int cmd, void *arg, SceSize size); /* 0x8144FDC4 */
extern int gpsDrvAckEvent(void);			/* 0x8144FDE0 */
extern int gpsDrvGetStatus(int *outStatus);		/* 0x8144FDE8 */

struct GpsXtraSetArg {
	unsigned char partIndex;			/* +0 (+16) */
	unsigned char pad1[3];
	unsigned short totalSize;			/* +4 (+20) */
	unsigned short partSize;			/* +6 (+22) */
	unsigned char data[1500];			/* +8 (+24..+1523) */
};

struct GpsXtraInfo {
	unsigned short maxSize;				/* +0 (+1524) */
	unsigned char numUrls;				/* +2 (+1526) */
	unsigned char pad3;
	unsigned char urls[4][256];			/* +4 (+1528..+2551) */
};

class QualCommAdapter {
public:
	QualCommAdapter();
	virtual void OnStateChanged(int state);		/* 0x81198BA6 (+0) */
	virtual int OnGpsNotification(int notifyId, int notifyCount, int notifyArg); /* 0x81198BA8 (+4) */
	virtual ~QualCommAdapter();			/* 0x811976D0 / 0x81197702 (+8/+12) */

	int StartAGpsThread();				/* 0x8119773A */
	int StartXtraThread(char fromUx0Only);		/* 0x811979C8 */
	int StartXtraDownload();			/* 0x81197AB6 */
	SceInt64 DateTimeToMsec(SceDateTime dt);	/* 0x81197B20 */
	int InjectTime();				/* 0x81197D00 */
	int SendNextXtraPart(int mode);			/* 0x81197E62 */
	int DoFileGet(const char *path, void *buf, int *outSize); /* 0x81197FA8 */
	int DoHttpGet(const unsigned char *urlEntry, void *buf, int *outSize); /* 0x811980F6 */
	int DownloadAndInjectXtra();			/* 0x811982DA */
	int AgpsCallback(int notifyId, int notifyCount, int notifyArg); /* 0x81198B1C */

	static int AGpsThreadEntry(SceSize args, void *argp); /* 0x81198538 */
	static int XtraGpsCallback(int notifyId, int notifyCount, int notifyArg, void *common); /* 0x8119863A */
	static int XtraResetGps(void);			/* 0x81198772 */
	static int XtraThreadEntry(SceSize args, void *argp); /* 0x811988D0 */

	void *m_xtraBuf;				/* +4 */
	int m_xtraDataSize;				/* +8 */
	int m_partIndex;				/* +12 */
	GpsXtraSetArg m_xtraSetArg;			/* +16..+1523 */
	GpsXtraInfo m_xtraInfo;				/* +1524..+2551 */
	unsigned int m_timeInfo;			/* +2552 */
	unsigned char m_injectState;			/* +2556 */
	int m_xtraStatus;				/* +2560 */
	SceUID m_agpsEvf;				/* +2564 */
	SceUID m_agpsThread;				/* +2568 */
	unsigned char m_agpsRunning;			/* +2572 */
	int m_xtraOffset;				/* +2576 */
	unsigned char m_enabled;			/* +2580 */
	SceUID m_xtraEvf;				/* +2584 */
	SceUID m_xtraThread;				/* +2588 */
	unsigned char m_xtraRunning;			/* +2592 */
	SceUID m_xtraCbId;				/* +2596 */
};

static QualCommAdapter *s_qualcomm;			/* 0x81544CEC */
static int s_xtraResetPending;				/* 0x81544D58 */

static const int s_daysNormal[13] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};
static const int s_daysLeap[13] = {
	0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

QualCommAdapter::QualCommAdapter()
	: m_xtraBuf(NULL)
	, m_xtraDataSize(0)
	, m_partIndex(-1)
	, m_injectState(0)
	, m_xtraStatus(0)
	, m_agpsEvf(-1)
	, m_agpsThread(-1)
	, m_agpsRunning(0)
	, m_xtraOffset(0)
	, m_enabled(1)
	, m_xtraEvf(-1)
	, m_xtraThread(-1)
	, m_xtraRunning(0)
	, m_xtraCbId(-1)
{
}

/* 0x811976D0 / 0x81197702 */
QualCommAdapter::~QualCommAdapter()
{
	m_partIndex = -1;
	if (m_xtraBuf != NULL) {
		operator delete(m_xtraBuf);
		m_xtraBuf = NULL;
	}
}

/* 0x8119773A */
int QualCommAdapter::StartAGpsThread()
{
	m_agpsEvf = sceKernelCreateEventFlag("SceLocationAGpsEvent", 0x1000, 0, NULL);
	if (m_agpsEvf < 0)
		return m_agpsEvf;

	SceUID thid = sceKernelCreateThread("SceLocationAGps", AGpsThreadEntry, 191, 0x1000, 0, 0, NULL);
	if (thid < 0) {
		sceKernelDeleteEventFlag(m_agpsEvf);
		m_agpsEvf = -1;
		return thid;
	}

	m_agpsThread = thid;
	m_agpsRunning = 1;
	int ret = sceKernelStartThread(thid, 0, NULL);
	if (ret < 0) {
		m_agpsRunning = 0;
		sceKernelDeleteThread(m_agpsThread);
		sceKernelDeleteEventFlag(m_agpsEvf);
		m_agpsEvf = -1;
		m_agpsThread = -1;
		return ret;
	}
	return 0;
}

/* 0x811977E0 */
int gpsAssistInit(void)
{
	QualCommAdapter *obj = new (g_locationHeap) QualCommAdapter();
	s_qualcomm = obj;
	if (obj != NULL)
		return obj->StartAGpsThread();
	return SCE_LOCATION_ERROR_NO_MEMORY;
}

/* 0x8119785E */
void gpsAssistTerm(void)
{
	if (s_qualcomm != NULL) {
		if (s_qualcomm->m_xtraRunning != 0) {
			s_qualcomm->m_xtraRunning = 0;
			sceKernelSetEventFlag(s_qualcomm->m_xtraEvf, 0x80000000U);
			sceKernelWaitThreadEnd(s_qualcomm->m_xtraThread, NULL, NULL);
			sceKernelDeleteThread(s_qualcomm->m_xtraThread);
			sceKernelDeleteEventFlag(s_qualcomm->m_xtraEvf);
			s_qualcomm->m_xtraEvf = -1;
			s_qualcomm->m_xtraThread = -1;
			if (s_qualcomm->m_xtraCbId >= 0) {
				sceKernelDeleteCallback(s_qualcomm->m_xtraCbId);
				s_qualcomm->m_xtraCbId = -1;
			}
		}
		if (s_qualcomm->m_xtraStatus == 2) {
			if (g_locationLogger != NULL)
				locationLoggerSetXtraState(g_locationLogger, 4);
			s_qualcomm->m_xtraStatus = 4;
		}
		if (s_qualcomm->m_agpsRunning != 0) {
			s_qualcomm->m_agpsRunning = 0;
			SceUInt32 numWait = 0;
			sceKernelCancelEventFlag(s_qualcomm->m_agpsEvf, 1, &numWait);
			sceKernelWaitThreadEnd(s_qualcomm->m_agpsThread, NULL, NULL);
			sceKernelDeleteThread(s_qualcomm->m_agpsThread);
			sceKernelDeleteEventFlag(s_qualcomm->m_agpsEvf);
			s_qualcomm->m_agpsEvf = -1;
			s_qualcomm->m_agpsThread = -1;
		}
		if (s_qualcomm->m_xtraStatus == 2) {
			if (g_locationLogger != NULL)
				locationLoggerSetXtraState(g_locationLogger, 4);
			s_qualcomm->m_xtraStatus = 4;
		}
		delete s_qualcomm;
		s_qualcomm = NULL;
	}
}

/* 0x8119798C */
void gpsAssistAck(void)
{
	if (s_qualcomm != NULL && s_qualcomm->m_xtraStatus == 2) {
		if (g_locationLogger != NULL)
			locationLoggerSetXtraState(g_locationLogger, 4);
		s_qualcomm->m_xtraStatus = 4;
	}
}

/* 0x811979BC */
QualCommAdapter *gpsAssistGetInstance(void)
{
	return s_qualcomm;
}

/* 0x811979C8 */
int QualCommAdapter::StartXtraThread(char fromUx0Only)
{
	int ret = 0;
	if (fromUx0Only == 1) {
		SceIoStat stat;
		ret = sceIoGetstat("ux0:liblocation/xtra.bin", &stat);
		if (ret != 0)
			return ret;
	}

	m_xtraEvf = sceKernelCreateEventFlag("SceLocationXtraEvent", 0x1000, 0, NULL);
	if (m_xtraEvf < 0)
		return m_xtraEvf;

	SceUID thid = sceKernelCreateThread("SceLocationXtra", XtraThreadEntry, 0x10000100, 0x1000,
					    0, 0, NULL);
	if (thid < 0) {
		sceKernelDeleteEventFlag(m_xtraEvf);
		m_xtraEvf = -1;
		return thid;
	}

	m_xtraThread = thid;
	m_xtraRunning = 1;
	ret = sceKernelStartThread(thid, 0, NULL);
	if (ret < 0) {
		m_xtraRunning = 0;
		sceKernelDeleteThread(m_xtraThread);
		sceKernelDeleteEventFlag(m_xtraEvf);
		m_xtraEvf = -1;
		m_xtraThread = -1;
		return ret;
	}
	if (ret == 0)
		sceKernelSetEventFlag(m_xtraEvf, 0x100000U);
	return ret;
}

/* 0x81197AB6 */
int QualCommAdapter::StartXtraDownload()
{
	m_enabled = 1;
	if (g_locationLogger != NULL) {
		if (locationLoggerGetField24(g_locationLogger) != 0)
			return s_qualcomm->StartXtraThread(1);
		if (locationLoggerGetField8(g_locationLogger) == 0 ||
		    locationLoggerGetField190(g_locationLogger) == 1)
			return s_qualcomm->StartXtraThread(0);
	}
	return 0;
}

/* 0x81197B20 */
SceInt64 QualCommAdapter::DateTimeToMsec(SceDateTime dt)
{
	int daysNormal[13];
	int daysLeap[13];
	sce_paf_memcpy(daysNormal, s_daysNormal, sizeof(daysNormal));
	sce_paf_memcpy(daysLeap, s_daysLeap, sizeof(daysLeap));

	if (dt.year < 1970)
		return -1;
	if (dt.month == 0 || dt.month > 12)
		return -1;

	const int *tbl = ((dt.year % 4 != 0) || (dt.year % 100 == 0 && dt.year % 400 != 0))
				 ? daysNormal
				 : daysLeap;
	if (dt.day == 0 || dt.day > tbl[dt.month])
		return -1;
	if (dt.hour > 23 || dt.minute > 59 || dt.second > 59)
		return -1;

	int leapDays = 0;
	for (unsigned int y = 1970; y < dt.year; y++) {
		if ((y & 3) == 0 && (y % 100 != 0 || y % 400 == 0))
			leapDays++;
	}

	int totalDays = leapDays + 365 * dt.year - 719050;
	for (int m = 0; m < dt.month; m++)
		totalDays += tbl[m];
	totalDays += dt.day - 1;

	SceInt64 secs = dt.second + 60LL * (dt.minute + 60LL * (dt.hour + 24LL * totalDays));
	return secs * 1000LL + (dt.microsecond / 1000U);
}

/* 0x81197D00 */
int QualCommAdapter::InjectTime()
{
	sce_paf_memset(&m_timeInfo, 0, sizeof(m_timeInfo));
	gpsDrvIoctl((int)0xAB001001, &m_timeInfo, sizeof(m_timeInfo));

	SceRtcTick tick;
	SceDateTime dt;
	SceInt64 msec;
	int ret = sceRtcGetCurrentRetainedNetworkTick(&tick);
	if (ret == 0) {
		ret = sceRtcSetTick(&dt, &tick);
		msec = (ret == 0) ? DateTimeToMsec(dt) : ret;
	} else {
		ret = sceRtcGetCurrentGpsTick(&tick);
		if (ret == 0) {
			ret = sceRtcSetTick(&dt, &tick);
			msec = (ret == 0) ? DateTimeToMsec(dt) : ret;
		} else {
			msec = ret;
		}
	}
	if (msec < 0)
		return (int)msec;

	struct {
		SceInt64 timeMsec;
		int timeUncMsec;
		unsigned char timeBase;
		unsigned char fForce;
		unsigned char pad[2];
	} argSet;
	sce_paf_memset(&argSet, 0, sizeof(argSet));
	argSet.timeMsec = msec;
	argSet.timeUncMsec = 0;
	argSet.timeBase = 1;
	argSet.fForce = 0;

	ret = gpsDrvIoctl(0x5B001001, &argSet, sizeof(argSet));
	if (g_locationLogger != NULL) {
		locationLoggerPrintf(g_locationLogger, 2, "[COM] sceGpsIoctl(TimeSet)=0x%08x\n", ret);
		locationLoggerPrintf(
			g_locationLogger, 2,
			"[COM] argSet.time_msec=%lld, argSet.timeUnc_msec=%d, argSet.timeBase=%d, argSet.fForce=%d\n",
			argSet.timeMsec, argSet.timeUncMsec, argSet.timeBase, argSet.fForce);
	}
	return ret;
}

/* 0x81197E62 */
int QualCommAdapter::SendNextXtraPart(int mode)
{
	if ((unsigned int)m_xtraOffset >= (unsigned int)(m_xtraDataSize - 1) || mode == 2) {
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2, "[COM] ### XTRA Set End ###\n");
		operator delete(m_xtraBuf);
		m_injectState = 0;
		m_xtraBuf = NULL;
		m_xtraDataSize = 0;
		m_xtraOffset = 0;
		m_partIndex = -1;
		int st = (mode == 2) ? 4 : 3;
		if (g_locationLogger != NULL)
			locationLoggerSetXtraState(g_locationLogger, st);
		m_xtraStatus = st;
		if (g_locationLogger != NULL && locationLoggerGetField24(g_locationLogger) == 1)
			sceKernelSetEventFlag(m_xtraEvf, 0x800000U);
		return 0;
	}

	m_xtraSetArg.partIndex = (unsigned char)m_partIndex;
	m_xtraSetArg.totalSize = (unsigned short)m_xtraDataSize;
	const unsigned char *src = (const unsigned char *)m_xtraBuf + m_xtraOffset;
	if ((unsigned int)(m_xtraOffset + 1500) <= (unsigned int)m_xtraDataSize) {
		m_xtraSetArg.partSize = 1500;
		sce_paf_memcpy(m_xtraSetArg.data, src, 1500);
	} else {
		unsigned short rem = (unsigned short)(m_xtraDataSize - m_xtraOffset);
		m_xtraSetArg.partSize = rem;
		sce_paf_memcpy(m_xtraSetArg.data, src, rem);
	}

	int ret = gpsDrvIoctl(0x5B001000, &m_xtraSetArg, sizeof(m_xtraSetArg));
	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 2, "[COM] sceGpsIoctl(SET_XTRA) = 0x%08x\n",
				     ret);
	if (ret != 0) {
		operator delete(m_xtraBuf);
		m_injectState = 0;
		m_xtraBuf = NULL;
		m_xtraDataSize = 0;
		m_xtraOffset = 0;
		m_partIndex = -1;
		if (g_locationLogger != NULL)
			locationLoggerSetXtraState(g_locationLogger, 4);
		m_xtraStatus = 4;
	} else {
		m_xtraOffset += m_xtraSetArg.partSize;
		m_partIndex++;
	}
	return ret;
}

/* 0x81197FA8 */
int QualCommAdapter::DoFileGet(const char *path, void *buf, int *outSize)
{
	SceIoStat stat;
	int ret = sceIoGetstat(path, &stat);
	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 2, "[COM] XTRA FILE result:0x%x size:%d\n",
				     ret, (int)stat.st_size);
	if (ret == 0 && stat.st_size > 0) {
		SceUID fd = sceIoOpen(path, SCE_O_RDONLY, 0404);
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] XTRA file Open(fd):0x%x path:%s\n", fd, path);
		if (fd < 0)
			return fd;
		if (m_xtraInfo.maxSize < stat.st_size) {
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 2,
						     "[COM] XTRA file size large\n");
			return -1;
		}
		SceSSize rd = sceIoRead(fd, buf, (SceSize)stat.st_size);
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2, "[COM] XTRA file read size:%d\n",
					     (int)rd);
		ret = (int)rd;
		if (ret >= 0 && ret == stat.st_size) {
			*outSize = ret;
			ret = 0;
		}
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2, "[COM] sceIoClose(fd)\n");
		sceIoClose(fd);
	}
	return ret;
}

/* 0x811980F6 */
int QualCommAdapter::DoHttpGet(const unsigned char *urlEntry, void *buf, int *outSize)
{
	int tmplId = sceHttpCreateTemplate("test/1.00", SCE_HTTP_VERSION_1_1, 1);
	if (tmplId < 0) {
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] sceHttpCreateTemplate error:0x%x\n", tmplId);
		return tmplId;
	}

	const char *url = (const char *)(urlEntry + 1);
	int connId = sceHttpCreateConnectionWithURL(tmplId, url, 1);
	if (connId < 0) {
		sceHttpDeleteTemplate(tmplId);
		return connId;
	}

	int reqId = sceHttpCreateRequestWithURL(connId, SCE_HTTP_METHOD_GET, url, 0);
	if (reqId < 0) {
		sceHttpDeleteConnection(connId);
		sceHttpDeleteTemplate(tmplId);
		return reqId;
	}

	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 2, "[COM] Send Http GET [%d]\n", reqId);

	int ret = sceHttpSendRequest(reqId, NULL, 0);
	if (ret >= 0) {
		int statusCode = 0;
		ret = sceHttpGetStatusCode(reqId, &statusCode);
		if (ret >= 0) {
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 2,
						     "[COM] Status Code was [%d]\n", statusCode);
			if (statusCode == 200) {
				SceUInt64 contentLen = 0;
				ret = sceHttpGetResponseContentLength(reqId, &contentLen);
				if (ret >= 0) {
					if (g_locationLogger != NULL)
						locationLoggerPrintf(g_locationLogger, 2,
								     "[COM] Content-Length = %lld\n",
								     contentLen);
					if (contentLen > m_xtraInfo.maxSize)
						ret = SCE_LOCATION_ERROR_FATAL_ERROR;
				} else if (g_locationLogger != NULL) {
					locationLoggerPrintf(
						g_locationLogger, 2,
						"[COM] sceHttpGetContentLength() returns %x\n",
						ret);
				}

				if (ret >= 0) {
					int done = 0;
					do {
						ret = sceHttpReadData(reqId, buf, m_xtraInfo.maxSize);
						if (ret < 0)
							break;
						if (ret != 0)
							*outSize = ret;
						else
							done = 1;
					} while (!done);
				}
			}
		}
	}

	sceHttpDeleteRequest(reqId);
	sceHttpDeleteConnection(connId);
	sceHttpDeleteTemplate(tmplId);
	return ret;
}

/* 0x811982DA */
int QualCommAdapter::DownloadAndInjectXtra()
{
	int xtraMode = 0;
	if (g_locationLogger != NULL) {
		xtraMode = locationLoggerGetField194(g_locationLogger);
		locationLoggerSetXtraState(g_locationLogger, 2);
	}
	m_xtraStatus = 2;

	if (m_xtraBuf != NULL) {
		operator delete(m_xtraBuf);
		m_xtraBuf = NULL;
	}

	int ret = gpsDrvIoctl((int)0xAB001000, &m_xtraInfo, sizeof(m_xtraInfo));
	if (ret == 0) {
		m_xtraBuf = g_locationHeap->sce_paf_malloc(m_xtraInfo.maxSize);
		if (m_xtraBuf == NULL) {
			ret = SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
		} else if (g_locationLogger != NULL && locationLoggerGetField24(g_locationLogger) == 1) {
			ret = DoFileGet("ux0:liblocation/xtra.bin", m_xtraBuf, &m_xtraDataSize);
		} else if (xtraMode == 1) {
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 2,
						     "[COM] XTRA file from=None\n");
			ret = -1;
		} else if (xtraMode == 3) {
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 2,
						     "[COM] XTRA file from=File\n");
			ret = DoFileGet("ux0:liblocation/xtra.bin", m_xtraBuf, &m_xtraDataSize);
		} else {
			for (int i = 0; i < m_xtraInfo.numUrls; i++) {
				ret = DoHttpGet(m_xtraInfo.urls[i], m_xtraBuf, &m_xtraDataSize);
				if (g_locationLogger != NULL)
					locationLoggerPrintf(
						g_locationLogger, 2,
						"[COM] _doHttpGet()=0x%08x, url:%s, m_xtraDataSize:%d\n",
						ret, (const char *)(m_xtraInfo.urls[i] + 1),
						m_xtraDataSize);
				if (ret == 0) {
					SceUID fd = sceIoOpen("ur0:/liblocation/gps/xtra.bin",
							      0x10602, 6);
					ret = fd;
					if (fd >= 0) {
						SceSSize wr = sceIoWrite(fd, m_xtraBuf, m_xtraDataSize);
						ret = (wr == m_xtraDataSize) ? 0 : (int)wr;
						sceIoClose(fd);
					}
					if (g_locationLogger != NULL)
						locationLoggerPrintf(
							g_locationLogger, 2,
							"[COM] _saveXtraFile()=0x%08x, m_xtraDataSize:%d\n",
							ret, m_xtraDataSize);
					break;
				}
				if (ret == (int)0x80412088 || ret == (int)0x80412089 ||
				    ret == (int)0x8041208A || ret == (int)0x80412093 ||
				    ret == (int)0x80412094)
					break;
			}

			if (g_locationXtra != NULL) {
				if (ret != 0) {
					*(int *)((char *)g_locationXtra + 8) = 1;
					ret = DoFileGet("ur0:/liblocation/gps/xtra.bin", m_xtraBuf,
							&m_xtraDataSize);
					if (g_locationLogger != NULL)
						locationLoggerPrintf(
							g_locationLogger, 2,
							"[COM] _doFileGet(%s)=0x%08x, m_xtraDataSize:%d\n",
							"ur0:/liblocation/gps/xtra.bin", ret,
							m_xtraDataSize);
				} else {
					*(int *)((char *)g_locationXtra + 8) = 0;
				}
				*(int *)((char *)g_locationXtra + 12) = 0;
			}
		}

		if (ret == 0) {
			m_xtraOffset = 0;
			m_partIndex = 0;
			return SendNextXtraPart(0);
		}
	}

	if (g_locationLogger != NULL)
		locationLoggerSetXtraState(g_locationLogger, 4);
	m_xtraStatus = 4;
	if (m_xtraBuf != NULL) {
		operator delete(m_xtraBuf);
		m_xtraBuf = NULL;
	}
	return ret;
}

/* 0x81198538 */
int QualCommAdapter::AGpsThreadEntry(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	int ret;
	for (;;) {
		unsigned int outBits = 0;
		ret = sceKernelWaitEventFlagCB(s_qualcomm->m_agpsEvf, 0x3FU, 5, &outBits, NULL);
		if (g_locationLogger != NULL)
			locationLoggerPrintf(
				g_locationLogger, 2,
				"[COM] qualCommCallback() sceKernelWaitEventFlagCB : 0x%08x\n",
				outBits);
		if (ret != 0 || (outBits & 1) != 0)
			break;
		if ((outBits & 4) != 0 && s_qualcomm->m_injectState == 2)
			s_qualcomm->SendNextXtraPart(4);
		if ((outBits & 2) != 0 && s_qualcomm->m_injectState == 2)
			s_qualcomm->SendNextXtraPart(2);
		if ((outBits & 8) != 0) {
			if (s_qualcomm->DownloadAndInjectXtra() == 0)
				s_qualcomm->m_injectState = 2;
		}
		if ((outBits & 0x10) != 0) {
			s_qualcomm->m_injectState = 1;
			s_qualcomm->InjectTime();
		}
	}
	return ret;
}

/* 0x8119863A */
int QualCommAdapter::XtraGpsCallback(int notifyId, int notifyCount, int notifyArg, void *common)
{
	(void)notifyId;
	(void)notifyCount;
	(void)common;
	int state = 9;
	unsigned int hi = (unsigned int)notifyArg & 0xFFFF0000U;
	gpsDrvGetStatus(&state);
	switch (hi) {
	case 0x12000000U:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] Gps Notify Time Inject. GpsState(%d)\n", state);
		sceKernelSetEventFlag(s_qualcomm->m_xtraEvf, 0x40000U);
		break;
	case 0x11000000U:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] Gps Notify XTRA Inject. GpsState(%d)\n", state);
		sceKernelSetEventFlag(s_qualcomm->m_xtraEvf, 0x10000U);
		break;
	case 0x00060000U:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] Gps Notify IoCtl. GpsState(%d)\n", state);
		sceKernelSetEventFlag(s_qualcomm->m_xtraEvf, 0x20000U);
		break;
	case 0x00020000U:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] Gps Notify Stop. GpsState(%d)\n", state);
		sceKernelSetEventFlag(s_qualcomm->m_xtraEvf, 0x200000U);
		break;
	case 0x00010000U:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] Gps Notify Start. GpsState(%d)\n", state);
		break;
	default:
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] Notify %#08x. GpsState(%d)\n", hi, state);
		break;
	}
	gpsDrvAckEvent();
	return 0;
}

/* 0x81198772 */
int QualCommAdapter::XtraResetGps(void)
{
	int state = 0;
	gpsDrvGetStatus(&state);
	if (state == 3) {
		int resetArg[2] = { 0x1FFF, 1023 };
		s_qualcomm->m_injectState = 0;
		int ret = gpsDrvIoctl(0x5B001003, resetArg, sizeof(resetArg));
		if (g_locationLogger != NULL)
			locationLoggerSetXtraState(g_locationLogger, 1);
		s_qualcomm->m_xtraStatus = 1;
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] sceGpsIoctl(RESET)=0x%08x\n", ret);
		return 0;
	}

	s_qualcomm->m_xtraCbId = sceKernelCreateCallback("SceLocationXtraCB", 0, XtraGpsCallback, NULL);
	if (s_qualcomm->m_xtraCbId < 0)
		return s_qualcomm->m_xtraCbId;

	int ret = gpsDrvOpen(s_qualcomm->m_xtraCbId);
	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 2, "[COM] sceGpsOpen()=0x%08x\n", ret);
	if (ret == 0) {
		int resetArg[2] = { 0x1FFF, 1023 };
		s_qualcomm->m_injectState = 0;
		int iret = gpsDrvIoctl(0x5B001003, resetArg, sizeof(resetArg));
		if (g_locationLogger != NULL)
			locationLoggerSetXtraState(g_locationLogger, 1);
		s_qualcomm->m_xtraStatus = 1;
		if (g_locationLogger != NULL)
			locationLoggerPrintf(g_locationLogger, 2,
					     "[COM] sceGpsIoctl(RESET)=0x%08x\n", iret);
	}
	return ret;
}

/* 0x811988D0 */
int QualCommAdapter::XtraThreadEntry(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	for (;;) {
		unsigned int outBits = 0;
		int ret = sceKernelWaitEventFlagCB(s_qualcomm->m_xtraEvf, 0x8FFF1000U, 5, &outBits,
						   NULL);
		if (ret != (int)0x80028005 && ret < 0)
			break;

		if ((outBits & 0x100000U) != 0) {
			XtraResetGps();
			s_xtraResetPending = 1;
		}
		if ((outBits & 0x200000U) != 0) {
			int cret = gpsDrvClose();
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 2,
						     "[COM] sceGpsClose()=0x%08x\n", cret);
			sceKernelDeleteCallback(s_qualcomm->m_xtraCbId);
			s_qualcomm->m_xtraCbId = -1;
			return 0;
		}
		if ((outBits & 0x800000U) != 0) {
			int sret = gpsDrvStop();
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 2,
						     "[COM] sceGpsStop()=0x%08x\n", sret);
		}
		if ((outBits & 0x20000U) != 0) {
			if (s_xtraResetPending != 0) {
				s_xtraResetPending = 0;
				if (g_locationLogger == NULL ||
				    locationLoggerGetField24(g_locationLogger) != 1) {
					int cret = gpsDrvClose();
					if (g_locationLogger != NULL)
						locationLoggerPrintf(g_locationLogger, 2,
								     "[COM] sceGpsClose()=0x%08x\n",
								     cret);
					sceKernelDeleteCallback(s_qualcomm->m_xtraCbId);
					s_qualcomm->m_xtraCbId = -1;
					return 0;
				}
				int sret = gpsDrvStart(16);
				if (g_locationLogger != NULL)
					locationLoggerPrintf(g_locationLogger, 2,
							     "[COM] sceGpsStart()=0x%08x\n", sret);
			} else {
				sceKernelSetEventFlag(s_qualcomm->m_agpsEvf, 4U);
			}
		}
		if ((outBits & 0x10000U) != 0)
			sceKernelSetEventFlag(s_qualcomm->m_agpsEvf, 8U);
		if ((outBits & 0x40000U) != 0)
			sceKernelSetEventFlag(s_qualcomm->m_agpsEvf, 0x10U);
		if ((int)outBits < 0) {
			int state = 0;
			gpsDrvGetStatus(&state);
			if (g_locationLogger != NULL)
				locationLoggerPrintf(g_locationLogger, 2,
						     "[COM] Xtra TERMINATE : state(%d)\n", state);
			if (state == 5 || state == 6 || state == 7) {
				int sret = gpsDrvStop();
				if (g_locationLogger != NULL)
					locationLoggerPrintf(g_locationLogger, 2,
							     "[COM] sceGpsStop()=0x%08x\n", sret);
			} else if (state == 3) {
				int cret = gpsDrvClose();
				if (g_locationLogger != NULL)
					locationLoggerPrintf(g_locationLogger, 2,
							     "[COM] sceGpsClose()=0x%08x\n", cret);
				sceKernelDeleteCallback(s_qualcomm->m_xtraCbId);
				s_qualcomm->m_xtraCbId = -1;
				return 0;
			} else if (state == 2) {
				return 0;
			}
		}
	}
	return 0;
}

/* 0x81198B1C */
int QualCommAdapter::AgpsCallback(int notifyId, int notifyCount, int notifyArg)
{
	(void)notifyId;
	(void)notifyCount;
	unsigned int hi = (unsigned int)notifyArg & 0xFFFF0000U;
	int lo = notifyArg & 0xFF;
	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 2, "[COM] _agpsCallback() arg : 0x%08x\n",
				     notifyArg);
	if (hi == 0x13000000U)
		return 0;

	unsigned int evBit;
	if (hi == 0x12000000U) {
		evBit = 16;
	} else if (hi == 0x11000000U) {
		if (m_injectState == 2)
			return 0;
		evBit = 8;
	} else if (hi == 0x00060000U && m_injectState == 2) {
		evBit = (lo == 0) ? 4 : 2;
	} else {
		return 0;
	}

	if (g_locationLogger != NULL)
		locationLoggerPrintf(g_locationLogger, 2,
				     "[COM] _agpsCallback() eventSet : 0x%08x\n", evBit);
	return sceKernelSetEventFlag(m_agpsEvf, evBit);
}

/* 0x81198BA6 */
void QualCommAdapter::OnStateChanged(int state)
{
	(void)state;
}

/* 0x81198BA8 */
int QualCommAdapter::OnGpsNotification(int notifyId, int notifyCount, int notifyArg)
{
	unsigned int hi = (unsigned int)notifyArg & 0xFFFF0000U;
	if (hi == 0x11000000U || hi == 0x12000000U || hi == 0x13000000U || hi == 0x00060000U) {
		if (g_locationLogger != NULL) {
			if (locationLoggerGetField8(g_locationLogger) == 1) {
				AgpsCallback(notifyId, notifyCount, notifyArg);
				locationLoggerPrintf(
					g_locationLogger, 2,
					"[COM] QualCommAdapter::OnGpsNotification() NoSendtoXps notify=0x%08x\n",
					hi);
			}
		} else {
			return AgpsCallback(notifyId, notifyCount, notifyArg);
		}
	}
	return 0;
}
