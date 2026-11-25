/*
 * location_xtra.cpp: GPS XTRA ephemeris downloader (daily cron callback, NetCtl retry
 * callback, HTTP download of xtra1/xtra2/xtra3.gpsonextra.net/xtra.bin to
 * ur0:/liblocation/gps/xtra.bin; 0x81189D4C-0x8118A764). The file's and internal names
 * here are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <liblocation.h>
#include <libhttp.h>
#include <libnetctl.h>
#include <kernel/iofilemgr.h>
#include <kernel/threadmgr.h>
#include <kernel/processmgr.h>
#include <std/new>
#include <std/vector>
#include <std/string>
#include "../../../../shell_cron.h"
#include "location_service.h"

using namespace paf;

/* PLACEHOLDER externs for the rest of liblocation */
struct LocationContext;
struct LocationLogger;
extern void *g_locationManager;					/* (0x815496B4) */
extern LocationLogger *g_locationLogger;			/* (0x815496B8) */
void *g_locationXtra;						/* (0x815496BC) */

LocationContext *locationGetContext(void);			/* (0x8118BA84) */
void locationLog(LocationLogger *logger, int level, const char *fmt, ...); /* (0x811895CC) */
int locationTaskSchedule(void *task, int delay);		/* (0x8118A780) */
int locationTaskCancel(void *task);				/* (0x8118A800) */
int locationMgrRegisterTask(void *mgr, void *task);		/* (0x8118AACE) */
int locationMgrUnregisterTask(void *mgr, void *task);		/* (0x8118AB62) */
int locationCtxGetGpsSetting1(LocationContext *ctx, int *out);	/* (0x8118DDA6) */
int locationCtxGetGpsSetting2(LocationContext *ctx, int *out);	/* (0x8118DE0C) */

/* the XTRA download thread ID (0x81544CE0) */
static SceUID s_xtraThreadId = -1;

class LocationXtra;

class LocationXtraPowerTask {
public:
	virtual int OnNotify();					/* (0x81189D4C) */

	int m_queued;						/* +4 */
	int m_active;						/* +8 */
	LocationXtra *m_xtra;					/* +12 */
};

class LocationXtra {
public:
	int Init();						/* (0x81189E10) */
	int CountLocationApps(int *outCount);			/* (0x81189FA4) */
	int DoHttpGet(const char *url, void *buf, SceUInt32 *outSize); /* (0x8118A338) */
	int DownloadAndSave();					/* (0x8118A52C) */

	static int DailyCronCB(SceUID handle, void *arg);	/* (0x8118A0EC) */
	static void NetCtlCB(int eventType, void *arg);		/* (0x8118A248) */
	static int ThreadEntry(SceSize args, void *argp);	/* (0x8118A6BC) */

	SceUID m_cronHandle;					/* +0 */
	int m_netCtlCid;					/* +4 */
	int m_retryNeeded;					/* +8 */
	int m_dayCount;						/* +12 */
	AppInfoQClient m_lsdbClient;				/* +16 */
	void *m_httpBuf;					/* +20 */
	SceUInt32 m_xtraDataSize;				/* +24 */
	int m_downloading;					/* +28 */
	LocationXtraPowerTask m_powerTask;			/* +32..+47 */
};

/* the XTRA downloader singleton (0x81544CDC) */
static LocationXtra *s_xtra;

static const char s_xtraUrls[3][37] = {
	"http://xtra1.gpsonextra.net/xtra.bin",
	"http://xtra2.gpsonextra.net/xtra.bin",
	"http://xtra3.gpsonextra.net/xtra.bin",
};

/* (0x81189D4C) */
int LocationXtraPowerTask::OnNotify()
{
	if (s_xtraThreadId == -1)
		return 0;
	sceKernelPowerTick(SCE_KERNEL_POWER_TICK_DISABLE_AUTO_SUSPEND);
	return 1;
}

/* (0x81189D6E) */
int locationXtraTerm(void)
{
	if (s_xtra != NULL) {
		ShellCron *cron = shellCronGetInstance();

		if (cron != NULL && s_xtra->m_cronHandle != 0) {
			cron->Remove(s_xtra->m_cronHandle);
			s_xtra->m_cronHandle = 0;
		}
		if (s_xtra->m_powerTask.m_queued != 0) {
			locationTaskCancel(&s_xtra->m_powerTask);
			locationMgrUnregisterTask(g_locationManager, &s_xtra->m_powerTask);
		}
		sceNetCtlInetUnregisterCallback(s_xtra->m_netCtlCid);
		s_xtra->m_netCtlCid = -1;
		SceLsdb_DEF5C3F9(&s_xtra->m_lsdbClient);
		if (s_xtra != NULL) {
			if (s_xtra->m_httpBuf != NULL) {
				operator delete(s_xtra->m_httpBuf);
				s_xtra->m_httpBuf = NULL;
			}
			SceLsdb_E2101B03(&s_xtra->m_lsdbClient);
			operator delete(s_xtra);
		}
		s_xtra = NULL;
		g_locationXtra = NULL;
	}
	return 0;
}

/* (0x81189E10) */
int LocationXtra::Init()
{
	SceLsdb_E90B81CD(&m_lsdbClient);

	int ret = sceIoMkdir("ur0:/liblocation/gps/", 06);

	if (ret == 0 || ret == (int)0x80010011)
		ret = 0;
	if (ret == 0) {
		LocationLogger *logger = g_locationLogger;

		if (logger != NULL)
			locationLog(logger, 3, "[XTRA] _setGpsXtraTimer() Called.\n");

		ShellCronOption opt;

		opt.wakeUp = true;
		opt.persist = false;

		ShellCron *cron = shellCronGetInstance();

		if (cron != NULL) {
			int dailyTime = cron->GetDailyTime();
			int cronRet = cron->AddDaily(&m_cronHandle, DailyCronCB, this, dailyTime,
						     SHELL_CRON_EVERY_DAY, &opt);

			if (logger != NULL)
				locationLog(logger, 3,
					    "[XTRA] registerDailyTimerCallback(cbId:0x%x) ret:0x%x\n",
					    m_cronHandle, cronRet);
		}
		ret = sceNetCtlInetRegisterCallback(NetCtlCB, this, &m_netCtlCid);
		locationMgrRegisterTask(g_locationManager, &m_powerTask);
	}
	return ret;
}

/* (0x81189EFC) */
int locationProviderInit(void)
{
	if (s_xtra != NULL)
		return SCE_LOCATION_ERROR_FATAL_ERROR;

	LocationXtra *xtra = (LocationXtra *)operator new(sizeof(LocationXtra), g_locationHeap);

	if (xtra != NULL) {
		SceLsdb_A6D8772C(&xtra->m_lsdbClient);
		new (&xtra->m_powerTask) LocationXtraPowerTask();
		xtra->m_powerTask.m_queued = 0;
		xtra->m_powerTask.m_active = 0;
		xtra->m_powerTask.m_xtra = xtra;
		xtra->m_retryNeeded = 0;
		xtra->m_dayCount = 0;
		xtra->m_cronHandle = 0;
		xtra->m_netCtlCid = -1;
		xtra->m_httpBuf = NULL;
		xtra->m_xtraDataSize = 0;
		xtra->m_downloading = 0;
		s_xtraThreadId = -1;
	}
	s_xtra = xtra;
	if (xtra == NULL)
		return SCE_LOCATION_ERROR_NO_SERVER_MEMORY;

	int ret = xtra->Init();

	if (ret != 0)
		locationXtraTerm();
	else
		g_locationXtra = s_xtra;
	return ret;
}

/* (0x81189FA4) */
int LocationXtra::CountLocationApps(int *outCount)
{
	LsdbQueryCondition cond;
	vector<string> titleIds;

	cond.key = 0xA9492ECAu;
	cond.op = LSDB_QUERY_OP_BITS;
	cond.mask = 3;
	cond.next = NULL;

	LsdbValue val2;

	val2.type = LSDB_VALUE_INTEGER;
	val2.integer = 2;
	cond.values.push_back(val2);

	LsdbValue val3;

	val3.type = LSDB_VALUE_INTEGER;
	val3.integer = 3;
	cond.values.push_back(val3);

	int ret = SceLsdb_76B75713(&m_lsdbClient, &cond, &titleIds, 0);

	if (ret == 0)
		*outCount = (int)titleIds.size();
	return ret;
}

/* (0x8118A0EC) */
int LocationXtra::DailyCronCB(SceUID handle, void *arg)
{
	LocationLogger *logger = g_locationLogger;

	if (logger != NULL)
		locationLog(logger, 3, "[XTRA] cbGpsXtraCallback() Called.\n");

	LocationXtra *xtra = (LocationXtra *)g_locationXtra;
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL && xtra->m_cronHandle == handle) {
		int setting1 = 0;
		int setting2 = 0;
		int appCount = 0;

		locationCtxGetGpsSetting1(ctx, &setting1);
		locationCtxGetGpsSetting2(ctx, &setting2);
		xtra->CountLocationApps(&appCount);
		if ((setting1 == 1 && setting2 == 1) || appCount != 0) {
			if (xtra->m_retryNeeded == 0) {
				xtra->m_dayCount++;
				if (xtra->m_dayCount < 3) {
					if (logger != NULL)
						locationLog(logger, 3,
							    "[XTRA] cbGpsXtraCallback()  not interval days go on!!!\n");
					return 0;
				}
			}
			xtra->m_dayCount = 0;

			int ret = -1;

			if (s_xtraThreadId == -1) {
				SceUID tid = sceKernelCreateThread("SceLocationGetXtra",
								   ThreadEntry, 191, 0x1000,
								   0, 0, NULL);

				ret = tid;
				if (tid >= 0) {
					s_xtraThreadId = tid;
					ret = sceKernelStartThread(tid, sizeof(LocationXtra),
								   s_xtra);
					if (ret != 0) {
						sceKernelDeleteThread(s_xtraThreadId);
						s_xtraThreadId = -1;
					} else {
						locationTaskSchedule(&xtra->m_powerTask, 10000);
					}
				} else {
					xtra->m_retryNeeded = 1;
				}
			}
			if (logger != NULL)
				locationLog(logger, 3, "[XTRA] _xtraGetStart()=0x%08x\n", ret);
		}
	}
	if (logger != NULL)
		locationLog(logger, 3, "[XTRA] cbGpsXtraCallback() end.\n");
	return 0;
}

/* (0x8118A248) */
void LocationXtra::NetCtlCB(int eventType, void *arg)
{
	LocationLogger *logger = g_locationLogger;

	if (logger != NULL)
		locationLog(logger, 3, "[XTRA] cbNetChkCallback() Called.\n");

	LocationXtra *xtra = (LocationXtra *)g_locationXtra;
	LocationContext *ctx = locationGetContext();

	if (ctx != NULL && eventType == 3 && xtra->m_retryNeeded == 1) {
		xtra->m_dayCount = 0;

		int ret = -1;

		if (s_xtraThreadId == -1) {
			SceUID tid = sceKernelCreateThread("SceLocationGetXtra", ThreadEntry,
							   191, 0x1000, 0, 0, NULL);

			ret = tid;
			if (tid >= 0) {
				s_xtraThreadId = tid;
				ret = sceKernelStartThread(tid, sizeof(LocationXtra), s_xtra);
				if (ret != 0) {
					sceKernelDeleteThread(s_xtraThreadId);
					s_xtraThreadId = -1;
				} else {
					locationTaskSchedule(&xtra->m_powerTask, 10000);
				}
			} else {
				xtra->m_retryNeeded = 1;
			}
		}
		if (logger != NULL)
			locationLog(logger, 3, "[XTRA] _xtraGetStart()=0x%08x\n", ret);
	}
	if (logger != NULL)
		locationLog(logger, 3, "[XTRA] cbNetChkCallback() end.\n");
}

/* (0x8118A338) */
int LocationXtra::DoHttpGet(const char *url, void *buf, SceUInt32 *outSize)
{
	LocationLogger *logger = g_locationLogger;
	int tmpl = sceHttpCreateTemplate("liblocation/1.0", 2, 1);

	if (tmpl < 0) {
		if (logger != NULL)
			locationLog(logger, 3, "[XTRA] sceHttpCreateTemplate error:0x%x\n", tmpl);
		return tmpl;
	}

	int conn = sceHttpCreateConnectionWithURL(tmpl, url, 1);

	if (conn < 0) {
		sceHttpDeleteTemplate(tmpl);
		return conn;
	}

	int req = sceHttpCreateRequestWithURL(conn, 0, url, 0);

	if (req < 0) {
		sceHttpDeleteConnection(conn);
		sceHttpDeleteTemplate(tmpl);
		return req;
	}

	if (logger != NULL)
		locationLog(logger, 3, "[XTRA] Send Http GET [%d]\n", req);

	int ret = sceHttpSendRequest(req, NULL, 0);

	if (ret >= 0) {
		int statusCode = 0;

		ret = sceHttpGetStatusCode(req, &statusCode);
		if (ret >= 0) {
			if (logger != NULL)
				locationLog(logger, 3, "[XTRA] Status Code was [%d]\n", statusCode);
			if (statusCode == 200) {
				SceULong64 contentLength = 0;
				int clRet = sceHttpGetResponseContentLength(req, &contentLength);

				if (clRet >= 0) {
					if (logger != NULL)
						locationLog(logger, 3,
							    "[XTRA] Content-Length = %lld\n",
							    contentLength);
				} else if (logger != NULL) {
					locationLog(logger, 3,
						    "[XTRA] sceHttpGetContentLength() returns %x\n",
						    clRet);
				}

				SceUInt32 total = 0;
				SceUID fd = sceIoOpen("ur0:/liblocation/gps/xtra.tmp",
						      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC |
							      0x10000,
						      06);

				if (fd < 0) {
					ret = fd;
				} else {
					int done = 0;

					do {
						int n = sceHttpReadData(req, buf, 0x8000);

						ret = n;
						if (n < 0)
							break;
						if (n != 0) {
							SceSSize w = sceIoWrite(fd, buf, (SceSize)n);

							if (w != n) {
								ret = w;
								break;
							}
							total += (SceUInt32)n;
							ret = 0;
						} else {
							done = 1;
							*outSize = total;
						}
					} while (done != 1);
					sceIoClose(fd);
					if (ret < 0)
						sceIoRemove("ur0:/liblocation/gps/xtra.tmp");
				}
			}
		}
	}
	sceHttpDeleteRequest(req);
	sceHttpDeleteConnection(conn);
	sceHttpDeleteTemplate(tmpl);
	return ret;
}

/* (0x8118A52C) */
int LocationXtra::DownloadAndSave()
{
	LocationLogger *logger = g_locationLogger;

	if (m_downloading != 0)
		return -1;
	m_downloading = 1;

	m_httpBuf = operator new(0x8000, g_locationHeap);

	int ret;

	if (m_httpBuf != NULL) {
		for (int i = 0; i < 3; i++) {
			ret = DoHttpGet(s_xtraUrls[i], m_httpBuf, &m_xtraDataSize);
			if (logger != NULL)
				locationLog(logger, 3,
					    "[XTRA] _doHttpGet()=0x%08x, url:%s, m_xtraDataSize:%d\n",
					    ret, s_xtraUrls[i], m_xtraDataSize);
			if (ret != 0) {
				switch (ret) {
				case -2143215352:
				case -2143215351:
				case -2143215350:
				case -2143215341:
				case -2143215340:
					break;
				default:
					continue;
				}
			} else {
				SceUID srcFd = sceIoOpen("ur0:/liblocation/gps/xtra.tmp",
							 SCE_O_RDONLY | 0x10000, 06);
				int saveRet = 0;

				if (srcFd < 0) {
					saveRet = srcFd;
				} else {
					SceUID dstFd = sceIoOpen("ur0:/liblocation/gps/xtra.bin",
								 SCE_O_WRONLY | SCE_O_CREAT |
									 SCE_O_TRUNC | 0x10000,
								 06);

					if (dstFd < 0) {
						saveRet = dstFd;
					} else {
						SceUInt32 copied = 0;

						while (copied < m_xtraDataSize) {
							SceSSize r = sceIoRead(srcFd, m_httpBuf,
									       0x8000);

							if (r < 0) {
								saveRet = r;
								break;
							}
							SceSSize w = sceIoWrite(dstFd, m_httpBuf,
										(SceSize)r);

							if (w != r) {
								saveRet = w;
								break;
							}
							copied += (SceUInt32)w;
						}
						sceIoClose(dstFd);
					}
					sceIoClose(srcFd);
				}
				ret = saveRet;
				sceIoRemove("ur0:/liblocation/gps/xtra.tmp");
				if (logger != NULL)
					locationLog(logger, 3, "[XTRA] _saveXtraFile()=0x%08x\n",
						    saveRet);
			}
			break;
		}
		operator delete(m_httpBuf);
		m_httpBuf = NULL;
		m_xtraDataSize = 0;
	} else {
		ret = SCE_LOCATION_ERROR_NO_SERVER_MEMORY;
	}
	m_downloading = 0;
	return ret;
}

/* (0x8118A6BC) */
int LocationXtra::ThreadEntry(SceSize args, void *argp)
{
	LocationXtra *xtra = (LocationXtra *)argp;
	int ret = xtra->DownloadAndSave();

	xtra->m_retryNeeded = (ret != 0) ? 1 : 0;
	s_xtraThreadId = -1;
	locationTaskCancel(&xtra->m_powerTask);
	sceKernelExitDeleteThread(0);
	return ret;
}
