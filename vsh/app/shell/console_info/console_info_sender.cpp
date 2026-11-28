/*
 * console_info_sender.cpp: the records sent (0x810F2174-0x810F302C). A job of its own queue sends
 * each kind's record, entry by entry, once the kind's last send is older than the inoperative
 * time: when the console reaches the network by Wi-Fi or a cable, and when a PC's MTP host asks.
 * The "ns" record goes to the network start server, the "ci" and the "mi" records to the
 * environment's update portal. The file's and the names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <rtc/rtc.h>
#include <job/job.h>
#include <thread/thread.h>
#include <file/httpfile.h>
#include <regmgr/regmgr.h>
#include <np_common/np_common.h>
#include "shell.h"
#include "console_info.h"

using namespace paf;

/* PLACEHOLDER: the queue's thread: its priority, its stack and one thread */
#define CONSOLE_INFO_SENDER_PRIORITY	0x1000010E
#define CONSOLE_INFO_SENDER_STACK	0x4000

/* the sender's state: idle or a job queued or running */
#define CONSOLE_INFO_SENDER_IDLE	0
#define CONSOLE_INFO_SENDER_BUSY	1

/* how a send was asked: by the network, or by the MTP host, which sends it itself */
#define CONSOLE_INFO_SENDER_NETWORK	0
#define CONSOLE_INFO_SENDER_MTP		1

/* the time after which a kind is sent again (seconds, by default a day) */
#define CONSOLE_INFO_SENDER_REG		"/CONFIG/SYSTEM/CONSOLE_INFO"
#define CONSOLE_INFO_SENDER_DEFAULT	86400000000ULL

/* PLACEHOLDER: the NetCtl state of the console connected, and its event */
#define CONSOLE_INFO_SENDER_CONNECTED	4
#define CONSOLE_INFO_SENDER_EVENT	4
#define CONSOLE_INFO_SENDER_EVENT_IP	6
#define CONSOLE_INFO_SENDER_EVENT_UP	5

/* PLACEHOLDER: the network's kind kept in ur0:ci/file (flags 1 and 2) */
#define CONSOLE_INFO_SENDER_NET_KIND	1
#define CONSOLE_INFO_SENDER_NET_LAST	2

/* an entry read, and how long the MTP host waits on its request */
#define CONSOLE_INFO_SENDER_ENTRY_SIZE	0x401
#define CONSOLE_INFO_SENDER_MTP_TIMEOUT	10000

/* the servers */
#define CONSOLE_INFO_SENDER_NS_URL	"https://ena.net.playstation.net/netstart/psp2?"
#define CONSOLE_INFO_SENDER_UPDPTL	"https://psp2.updptl."
#define CONSOLE_INFO_SENDER_CI_URL	".community.playstation.net/updptl/psp2/ci?"
#define CONSOLE_INFO_SENDER_MI_URL	".community.playstation.net/updptl/psp2/mi?"

/* (0x815445A8, 0x815445AC) the queue and the sender's state */
static job::JobQueue *s_queue = NULL;
static int s_busy = CONSOLE_INFO_SENDER_IDLE;

/* (0x81559098) the state's lock */
static thread::Mutex s_mutex("SceConsoleInfoSender", 0);

/* (0x81558C90) the entry being sent */
static char s_entry[CONSOLE_INFO_SENDER_ENTRY_SIZE];

static int consoleInfoSenderRun(int mode);

/* (0x810F2174) */
int consoleInfoSenderReset(void)
{
	s_busy = CONSOLE_INFO_SENDER_IDLE;
	return 0;
}

/* the kind's last send older than the inoperative time */
static inline bool consoleInfoSenderIsDue(int type)
{
	rtc::Tick last = 0;
	rtc::Tick now = 0;

	if (consoleInfoGetTimestamp(type, &last) < 0 || rtc::GetCurrentTick(&now) < 0)
		return false;

	int seconds = 0;
	SceUInt64 limit;

	if (sceRegMgrGetKeyInt(CONSOLE_INFO_SENDER_REG, "inoperative_time", &seconds) < 0 ||
	    seconds == 0)
		limit = CONSOLE_INFO_SENDER_DEFAULT;
	else
		limit = 1000000LL * seconds;
	return now - last >= limit;
}

/* (0x810F2182) the job queued, by the MTP host only when a kind is due */
static int consoleInfoSenderStart(int mode)
{
	s_mutex.Lock();
	if (s_busy == CONSOLE_INFO_SENDER_BUSY) {
		s_mutex.Unlock();
		return 0;
	}
	s_busy = CONSOLE_INFO_SENDER_BUSY;
	s_mutex.Unlock();

	if (s_queue == NULL) {
		s_busy = CONSOLE_INFO_SENDER_IDLE;
		return 0;
	}
	if (mode == CONSOLE_INFO_SENDER_MTP && !consoleInfoSenderIsDue(CONSOLE_INFO_TYPE_NS) &&
	    !consoleInfoSenderIsDue(CONSOLE_INFO_TYPE_CI) &&
	    !consoleInfoSenderIsDue(CONSOLE_INFO_TYPE_MI)) {
		s_busy = CONSOLE_INFO_SENDER_IDLE;
		return 0;
	}

	common::SharedPtr<job::JobItem> job(new ConsoleInfoSendDataJob(consoleInfoSenderRun,
								     consoleInfoSenderReset, mode));

	s_queue->Enqueue(job);
	return 1;
}

extern "C" {
int SceShell_77642C92(int *outRawState);
int SceShell_7AD86CE1(int code, SceNetCtlInfo *info);
int SceShell_2E0B6FA6(void (*callback)(int, int, int, int, int, void *), void *arg, int *outId);
}

static void consoleInfoSenderOnNetEvent(int code, int event, int state);
static int consoleInfoSenderOnMtp(void);

/* (0x810F2470) */
int consoleInfoSenderInit(void)
{
	int state = 0;
	int id;
	SceNetCtlInfo info;
	job::JobQueue::Option option;

	option.numThreads = 1;
	option.priority = CONSOLE_INFO_SENDER_PRIORITY;
	option.stackSize = CONSOLE_INFO_SENDER_STACK;
	s_queue = new job::JobQueue("SceConsoleInfoSendJobQueue", &option);

	SceShell_77642C92(&state);
	if (state == CONSOLE_INFO_SENDER_CONNECTED &&
	    SceShell_7AD86CE1(SCE_NET_CTL_INFO_DEVICE, &info) == 0 &&
	    info.device <= SCE_NET_CTL_DEVICE_WIRED)
		consoleInfoSenderStart(CONSOLE_INFO_SENDER_NETWORK);

	int ret = SceShell_2E0B6FA6(reinterpret_cast<void (*)(int, int, int, int, int, void *)>(consoleInfoSenderOnNetEvent), NULL, &id);

	if (ret == 0)
		return shellMtpServerSetConsoleInfoCallback(consoleInfoSenderOnMtp, 0);
	return ret;
}

/* (0x810F251E) the network reached: its kind kept, and the records sent unless by phone */
static void consoleInfoSenderOnNetEvent(int code, int event, int state)
{
	SceUInt8 last;
	SceNetCtlInfo info;

	if (event != CONSOLE_INFO_SENDER_EVENT ||
	    (state != CONSOLE_INFO_SENDER_EVENT_IP && code != CONSOLE_INFO_SENDER_EVENT_UP) ||
	    SceShell_7AD86CE1(SCE_NET_CTL_INFO_DEVICE, &info) < 0)
		return;

	int kind = 0;

	switch (info.device) {
	case SCE_NET_CTL_DEVICE_WIRELESS:
		kind = 1;
		break;
	case SCE_NET_CTL_DEVICE_WIRED:
		kind = 3;
		break;
	case SCE_NET_CTL_DEVICE_PHONE:
		kind = 2;
		break;
	}

	last = 0;
	if (consoleInfoFileReadFlag(CONSOLE_INFO_SENDER_NET_KIND, &last, 0) == 0) {
		consoleInfoFileWriteFlag(CONSOLE_INFO_SENDER_NET_LAST, last, 0);
		consoleInfoFileWriteFlag(CONSOLE_INFO_SENDER_NET_KIND, kind, 0);
	}
	if (info.device == SCE_NET_CTL_DEVICE_WIRELESS || info.device == SCE_NET_CTL_DEVICE_WIRED)
		consoleInfoSenderStart(CONSOLE_INFO_SENDER_NETWORK);
}

/* (0x810F25B0) */
static int consoleInfoSenderOnMtp(void)
{
	return consoleInfoSenderStart(CONSOLE_INFO_SENDER_MTP);
}

/* (0x810F25BC) an entry sent: by HTTP, or as a request to the MTP host */
static int consoleInfoSenderSend(int type, int mode, const char *entry, int size,
				 const char *baseUrl)
{
	if (mode == CONSOLE_INFO_SENDER_NETWORK) {
		HttpFile file;
		HttpFile::OpenArg arg;

		arg.open_flag |= HttpFile::OpenArg::SUPPRESS_3G_ACCESS;

		string url(baseUrl);

		url += entry;
		if (url.length() == 0)
			return -1;
		arg.ParseUrl(url.c_str());

		int ret = file.Open(&arg);

		if (ret < 0)
			return ret;

		SceUInt16 code = 0;

		ret = file.GetResponseCode(code);
		if (ret >= 0 && code != 200 && code != 304)
			ret = -1;

		int closed = file.Close();

		if (ret == 0)
			ret = closed;
		return ret;
	}
	if (mode != CONSOLE_INFO_SENDER_MTP)
		return -1;

	string url(baseUrl);

	url += entry;
	if (url.length() == 0)
		return -1;

	HttpFile::OpenArg arg;

	arg.ParseUrl(url.c_str());

	ShellMtpHttpRequest request;

	request.uriLength = 0;
	request.host = arg.host.c_str();
	request.scheme = arg.scheme.c_str();
	request.uri = arg.uri.c_str();
	request.uriLength = sce_paf_strlen(arg.uri.c_str());
	return shellMtpServerHttpRequest(&request, CONSOLE_INFO_SENDER_MTP_TIMEOUT);
}

/* (0x810F2AF0) the update portal of the NP environment */
static int consoleInfoSenderGetUpdptlUrl(string *url)
{
	char env[16];

	url->clear();
	*url = CONSOLE_INFO_SENDER_UPDPTL;
	sce_paf_memset(env, 0, sizeof(env));

	int ret = sceNpCommonGetNpEnviroment(env);

	if (ret < 0)
		return ret;

	string name(env);

	if (name.length() == 0)
		return -1;
	*url += env;
	return 0;
}

/* (0x810F2BF8) a record's entries sent */
__attribute__((noinline)) static int consoleInfoSenderSendRecord(int type, int mode,
								 ConsoleInfoRecord *record,
								 unsigned int count)
{
	string url;
	int ret;

	switch (type) {
	case CONSOLE_INFO_TYPE_NS:
		url.clear();
		url = CONSOLE_INFO_SENDER_NS_URL;
		ret = 0;
		break;
	case CONSOLE_INFO_TYPE_CI:
		ret = consoleInfoSenderGetUpdptlUrl(&url);
		if (ret == 0)
			url += CONSOLE_INFO_SENDER_CI_URL;
		break;
	case CONSOLE_INFO_TYPE_MI:
		ret = consoleInfoSenderGetUpdptlUrl(&url);
		if (ret == 0)
			url += CONSOLE_INFO_SENDER_MI_URL;
		break;
	default:
		ret = -1;
		break;
	}
	if (ret != 0)
		return ret;

	for (unsigned int i = 0; i < count; i++) {
		int size = 0;

		sce_paf_memset(s_entry, 0, sizeof(s_entry));
		ret = consoleInfoReadEntry(record, i, s_entry, &size);
		if (ret < 0)
			break;
		ret = consoleInfoSenderSend(type, mode, s_entry, size, url.c_str());
		if (ret < 0)
			break;
	}
	return ret;
}

/* (0x810F2DE8) a kind's record sent when it is due, its timestamp renewed when it was sent */
static int consoleInfoSenderSendType(int type, int mode)
{
	bool sent = false;

	if (!consoleInfoSenderIsDue(type))
		return 0;

	ConsoleInfoRecord *record;
	int ret = consoleInfoOpen(type, 1024, &record);

	if (ret < 0)
		return ret;

	unsigned int count = 0;

	/* a record opened to be sent gives its entries' count */
	ret = consoleInfoWrite(record, &count);
	if (ret >= 0) {
		ret = consoleInfoSenderSendRecord(type, mode, record, count);
		if (ret >= 0)
			sent = true;
	}

	int closed = consoleInfoClose(record, sent);

	if (ret >= 0)
		return closed;
	return ret;
}

/* (0x810F2EF2) the job: every kind sent, the MTP host told when it is done */
static int consoleInfoSenderRun(int mode)
{
	int ret;

	consoleInfoSenderSendType(CONSOLE_INFO_TYPE_NS, mode);
	ret = consoleInfoSenderSendType(CONSOLE_INFO_TYPE_CI, mode);
	if (mode != CONSOLE_INFO_SENDER_MTP)
		ret = consoleInfoSenderSendType(CONSOLE_INFO_TYPE_MI, mode);
	if (mode == CONSOLE_INFO_SENDER_MTP) {
		ShellMtpHttpRequest request = { NULL, NULL, NULL, 0 };

		shellMtpServerHttpRequest(&request, 0);
	}
	return ret;
}

/* (0x810F2F4E) the queue, and whether a job is queued or running */
job::JobQueue *consoleInfoSenderGetQueue(bool *busy)
{
	if (busy != NULL)
		*busy = s_busy == CONSOLE_INFO_SENDER_BUSY;
	return s_queue;
}
