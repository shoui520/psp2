/*
 * The shell's MTP host: its server started with the exclusive functions it runs under, libmtp
 * given the host's functions, the application and shell events it follows, and the registry
 * callback that ends MTP over Wi-Fi when Wi-Fi goes off or flight mode goes on. The file's and
 * every name here are ours (no record names them).
 */
#include <stddef.h>
#include <scetypes.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <kernel/threadmgr.h>
#include <regmgr/regmgr.h>
#include "shell.h"
#include <mtp/libmtp/libmtp.h>

using namespace paf;

/* the MTP server's threads */
#define SHELL_MTP_THREAD_PRIORITY		0x100000F7
#define SHELL_MTP_SESSION_THREAD_PRIORITY	0x100000F9
#define SHELL_MTP_EVENT_THREAD_PRIORITY		0x100000FA

#define SHELL_MTP_REG_KEY_COUNT	2

/* the application event (SceShell_83D16CA8's) after SHELL_APP_EVENT_ACTIVATED that MTP
   follows (name ours) */
#define SHELL_APP_EVENT_6	6

/* the shell event the MTP server follows (name ours) */
#define SHELL_EVENT_2		2

/* the exclusive functions the MTP server works under: the mode entered and left, and the
   memory it takes from the exclusive function manager's block */
struct ShellExclusiveFuncMtp {
	int (*begin)(void (*callback)(int event));
	int (*end)(void);
	void *(*malloc)(SceSize size);
	void *(*memalign)(SceSize alignment, SceSize size);
	void (*free)(void *ptr);
	void (*printStats)(void);
};

/* how the MTP server is started */
struct ShellMtpServerParam {
	int threadPriority;
	int sessionThreadPriority;
	int devThreadCpuAffinityMask;
	int eventThreadPriority;
	int xferThreadCpuAffinityMask;
	ShellExclusiveFuncMtp *exclusiveFunc;
};

static bool s_mtpStarted = false;
static SceUID s_mtpRegCallback = -1;
static ShellExclusiveFuncMtp s_exclusiveFuncMtp = {
	shellExclusiveFuncMtpBegin,
	shellExclusiveFuncMtpEnd,
	shellExclusiveFuncMtpMalloc,
	shellExclusiveFuncMtpMemalign,
	shellExclusiveFuncMtpFree,
	shellExclusiveFuncMtpPrintStats,
};
static bool s_mtpMarlinStarted = false;

static void shellMtpOnAppEvent(int code, const void *data, SceSize size, void *arg);
static void shellMtpOnShellEvent(int code, const void *data, SceSize size, void *arg);
static int shellMtpOnRegistryChange(int notifyId, int notifyCount, int notifyArg, void *common);

struct BgdlMtpContent;
struct BgdlMtpRequest;
int shellMtpHostCheckContent(const BgdlMtpContent *content);
int shellMtpHostAddTask(const BgdlMtpRequest *request, SceUInt32 *id);
int shellMtpHostRemoveTask(SceUInt32 id);
int shellMtpHostReloadTask(SceUInt32 id);
int shellMtpHostListTasks(void);

void shellMtpStart(void)
{
	ShellMtpServerParam param;
	SceLibMtpHostFuncs funcs;
	const char *keys[SHELL_MTP_REG_KEY_COUNT];

	param.threadPriority = SHELL_MTP_THREAD_PRIORITY;
	param.sessionThreadPriority = SHELL_MTP_SESSION_THREAD_PRIORITY;
	param.xferThreadCpuAffinityMask = 0;
	param.devThreadCpuAffinityMask = 0;
	param.eventThreadPriority = SHELL_MTP_EVENT_THREAD_PRIORITY;
	param.exclusiveFunc = &s_exclusiveFuncMtp;
	if (shellMtpServerStart(&param) < 0)
		return;

	funcs.func[0] = (int (*)(int))(int (*)(const BgdlMtpContent *))shellMtpHostCheckContent;
	funcs.func[1] = (int (*)(int))(int (*)(const BgdlMtpRequest *, SceUInt32 *))shellMtpHostAddTask;
	funcs.func[2] = (int (*)(int))(int (*)(SceUInt32))shellMtpHostRemoveTask;
	funcs.func[3] = (int (*)(int))(int (*)(SceUInt32))shellMtpHostReloadTask;
	funcs.func[4] = (int (*)(int))(int (*)(void))shellMtpHostListTasks;
	funcs.func[5] = (int (*)(int))shellMtpHostGetTaskFolder;
	SceLibMtp_67DB4C13(&funcs);

	int ret;

	ret = SceShell_83D16CA8(SHELL_APP_EVENT_6, shellMtpOnAppEvent, NULL);
	if (ret < 0)
		sce_paf_printf_dummy();
	ret = SceShell_83D16CA8(SHELL_APP_EVENT_ACTIVATED, shellMtpOnAppEvent, NULL);
	if (ret < 0)
		sce_paf_printf_dummy();
	ret = SceShell_83D16CA8(SHELL_APP_EVENT_ENDED, shellMtpOnAppEvent, NULL);
	if (ret < 0)
		sce_paf_printf_dummy();
	ret = shellAppEventServerAddListener(SHELL_EVENT_2, shellMtpOnShellEvent, NULL);
	if (ret < 0)
		sce_paf_printf_dummy();

	if (s_mtpRegCallback == -1) {
		SceUID cb = sceKernelCreateCallback("SceMtpWiFiRegCB", 0, shellMtpOnRegistryChange,
						    NULL);

		if (cb < 0)
			sce_paf_printf_dummy();
		else
			s_mtpRegCallback = cb;
		if (cb < 0)
			sce_paf_printf_dummy();
	}
	if (s_mtpRegCallback != -1) {
		ret = sceRegMgrRegisterCallback(s_mtpRegCallback);
		if (ret < 0) {
			sce_paf_printf_dummy();
			sceKernelDeleteCallback(s_mtpRegCallback);
			s_mtpRegCallback = -1;
		} else {
			bool addedAll = true;
			keys[0] = "/CONFIG/NET/wifi_flag";
			keys[1] = "/CONFIG/SYSTEM/flight_mode";
			for (unsigned int i = 0; i < SHELL_MTP_REG_KEY_COUNT; i++) {
				int err = sceRegMgrAddRegistryCallback(s_mtpRegCallback, keys[i]);

				if (err < 0) {
					sce_paf_printf_dummy();
					sceKernelDeleteCallback(s_mtpRegCallback);
					s_mtpRegCallback = -1;
					ret = err;
					addedAll = false;
					break;
				}
			}
			if (addedAll) {
				ret = sceRegMgrStartCallback(s_mtpRegCallback);
				if (ret < 0) {
					sceRegMgrUnregisterCallback(s_mtpRegCallback);
					sceKernelDeleteCallback(s_mtpRegCallback);
					s_mtpRegCallback = -1;
				}
			}
		}
		if (ret < 0)
			sce_paf_printf_dummy();
	}
	shellMtpWiFiCreate();
	s_mtpStarted = true;
}

/* the callback the registry changes come to */
void shellMtpRegCallbackCreate(void)
{
	if (s_mtpRegCallback != -1)
		return;

	SceUID cb = sceKernelCreateCallback("SceMtpWiFiRegCB", 0, shellMtpOnRegistryChange, NULL);

	if (cb < 0) {
		sce_paf_printf_dummy();
		return;
	}
	s_mtpRegCallback = cb;
}

/* ends MTP over Wi-Fi when Wi-Fi is off or flight mode is on */
class ShellMtpWiFiTermJob : public job::JobItem {
public:
	ShellMtpWiFiTermJob() : job::JobItem("SceMtpWiFiTermJob", NULL)
	{
	}

	virtual ~ShellMtpWiFiTermJob()
	{
	}

	virtual int Run()
	{
		int wifiFlag = -1;

		if ((sceRegMgrGetKeyInt("/CONFIG/NET", "wifi_flag", &wifiFlag) | wifiFlag) != 0) {
			int flightMode = -1;

			if (sceRegMgrGetKeyInt("/CONFIG/SYSTEM", "flight_mode", &flightMode) != 0 ||
			    flightMode != 1)
				return 0;
		}
		shellMtpWiFiTerminate();
		return 0;
	}

	virtual void Finish(int result)
	{
	}
};

static int shellMtpOnRegistryChange(int notifyId, int notifyCount, int notifyArg, void *common)
{
	ShellMtpWiFiTermJob *item = new ShellMtpWiFiTermJob();
	common::SharedPtr<job::JobItem> job(item);

	job::JobQueue::default_queue->Enqueue(job);
	return 0;
}

/* an application started, came to the front or ended: the MTP server is told */
static void shellMtpOnAppEvent(int code, const void *data, SceSize size, void *arg)
{
	const ShellMtpAppEvent *event = (const ShellMtpAppEvent *)data;

	if (code == SHELL_APP_EVENT_6)
		shellMtpServerNotifyApp6(event->value[0], event->value[1], event->value[2]);
	else if (code == SHELL_APP_EVENT_ACTIVATED)
		shellMtpServerNotifyAppActivated(event->value[0], event->value[1], event->value[2]);
	else if (code == SHELL_APP_EVENT_ENDED)
		shellMtpServerNotifyAppEnded(event->value[0], event->value[1],
					     (const char *)event->value[2]);
	else
		sce_paf_printf_dummy();
}

static void shellMtpOnShellEvent(int code, const void *data, SceSize size, void *arg)
{
	if (code != SHELL_EVENT_2)
		return;
	sce_paf_printf_dummy();
	shellMtpServerNotifyShellEvent(data);
}

/* the Marlin service (SceMtpMlnSvc) started */
void shellMtpMarlinStart(void)
{
	if (shellMtpMarlinServiceStart() >= 0)
		s_mtpMarlinStarted = true;
}
