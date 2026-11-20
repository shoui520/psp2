/*
 * console_info_media_info_writer_job.cpp: the SceMediaInfoWriterJob (0x810F34B2-0x810F3910): the
 * request's event stamped with the network and the process time and added to the process event
 * log, with the running titles' events when the shell starts and ends, the "uc" record written
 * when it starts and ends, and the usage counted of a title suspended. The file's and the names are
 * ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <rtc.h>
#include <kernel/processmgr.h>
#include <std/string.h>
#include <std/vector>
#include <thread/thread.h>
#include "shell.h"
#include "plugins.h"
#include "console_info.h"
#include "../src/applauncher_plugin/src/applauncher.h"
#include "../src/livespace_db/livespace_db_internal.h"

using namespace paf;

/* the shell's own title ID, its events' */
#define MEDIA_INFO_SHELL_TITLE		"NPXS19999"

/* how long a waiting side sleeps between looks (milliseconds) */
#define MEDIA_INFO_WAIT_SLEEP		50

/* PLACEHOLDER: the account's ID of the process (32 bytes, and four the call fills after it) */
extern "C" int SceLibKernel_F7027E6A(void *accountId);

/* PLACEHOLDER: a title topmenu lists as running: its state (1 running, 2 suspended) and what its
   events carry (24 bytes) */
struct MediaInfoRunningTitle {
	char titleId[12];
	int state;
	int type;
	int flag;
};

/* the request's event: the shell's own events carry its title ID */
static inline void mediaInfoMakeEvent(ConsoleInfoMiEvent *event,
				      const ShellMediaInfoRequest *request)
{
	sce_paf_memset(event, 0, sizeof(*event));
	event->state = request->type;
	event->flag = request->attr[1];
	event->kind = request->attr[2];
	event->subkind = request->attr[3];
	if (request->type == 1 || request->type == 2 || request->type == 3) {
		event->type = 1;
		sce_paf_memcpy(event->titleId, MEDIA_INFO_SHELL_TITLE, sizeof(event->titleId));
	} else {
		event->type = request->attr[0];
		sce_paf_memcpy(event->titleId, request->titleId, sizeof(event->titleId));
	}
}

/* (0x810F34B2) */
MediaInfoWriterJob::MediaInfoWriterJob(const ShellMediaInfoRequest *request, bool wait)
	: JobItem("SceMediaInfoWriterJob", NULL)
{
	m_wait = wait;
	m_running = true;
	m_done = false;
	mediaInfoMakeEvent(&m_event, request);
}

/* (0x810F3540, 0x810F355C) */
MediaInfoWriterJob::~MediaInfoWriterJob()
{
}

/* (0x810F357C) the caller's side: the job run, the job told it was seen */
int MediaInfoWriterJob::Wait()
{
	while (m_running)
		thread::Sleep(MEDIA_INFO_WAIT_SLEEP);
	m_done = true;
	return 1;
}

/* (0x810F3596) an event of every title running added: suspended (or ended, at the shell's end)
   for those running, resumed (or ended) for those suspended */
void consoleInfoProcLogAddRunning(bool end, const ConsoleInfoMiEvent *base)
{
	ShellPluginInterface *topmenu =
		(ShellPluginInterface *)applauncherPluginGetInterface("topmenu_plugin", 1);
	vector<MediaInfoRunningTitle> titles;

	if (((int (*)(vector<MediaInfoRunningTitle> *))
		     topmenu->func[TOPMENU_PLUGIN_FUNC_GET_RUNNING])(&titles) == 0)
		return;

	ConsoleInfoMiEvent event;

	sce_paf_memcpy(&event, base, sizeof(event));
	for (vector<MediaInfoRunningTitle>::iterator it = titles.begin(); it != titles.end();
	     ++it) {
		sce_paf_memcpy(event.titleId, it->titleId, sizeof(event.titleId));
		if (it->state == 1)
			event.state = end ? 7 : 6;
		else if (it->state == 2)
			event.state = end ? 8 : 4;
		else
			continue;
		event.type = it->type;
		event.flag = it->flag;
		consoleInfoProcLogAdd(consoleInfoGetProcLog(), &event, 1);
	}
	consoleInfoGetProcLog()->Read(0, &event, 2);
}

/* (0x810F3688) the boot's event, then the running titles' */
void consoleInfoProcLogAddBoot(void)
{
	SceRtcTick rtc2;
	SceKernelSysClock clock2;
	SceRtcTick rtc;
	SceKernelSysClock clock;
	ShellMediaInfoRequest request;
	ConsoleInfoMiEvent event;

	request.type = 3;
	request.attr[0] = 0;
	request.attr[1] = 0;
	request.attr[2] = 0;
	request.attr[3] = 0;
	mediaInfoMakeEvent(&event, &request);
	if (sceRtcGetCurrentNetworkTick(&rtc) == 0)
		event.rtcTick = rtc.tick;
	if (sceKernelGetProcessTime(&clock) == 0)
		event.tick = clock.quad;
	consoleInfoProcLogAdd(consoleInfoGetProcLog(), &event, 0);

	sce_paf_memset(&event, 0, sizeof(event));
	if (sceRtcGetCurrentNetworkTick(&rtc2) == 0)
		event.rtcTick = rtc2.tick;
	if (sceKernelGetProcessTime(&clock2) == 0)
		event.tick = clock2.quad;
	consoleInfoProcLogAddRunning(false, &event);
}

/* (0x810F377A) a title suspended counted, by its kind */
__attribute__((noinline)) void MediaInfoWriterJob::CountUsage()
{
	int id;

	switch (m_event.type) {
	case 2:
		id = 0;
		break;
	case 3:
		id = 1;
		break;
	case 4:
		id = 2;
		break;
	case 5:
		id = 3;
		break;
	case 6:
		id = 4;
		break;
	case 7:
		id = 5;
		break;
	case 8:
		id = 6;
		break;
	default:
		return;
	}
	shellUsageCounterAdd(id, 1, 0);
}

/* (0x810F37B6) */
int MediaInfoWriterJob::Run()
{
	SceRtcTick rtc;
	SceKernelSysClock clock;

	if (sceRtcGetCurrentNetworkTick(&rtc) == 0)
		m_event.rtcTick = rtc.tick;
	if (sceKernelGetProcessTime(&clock) == 0)
		m_event.tick = clock.quad;
	if (m_event.flag == 1) {
		SceUInt8 accountId[36];

		SceLibKernel_F7027E6A(accountId);
		sce_paf_memcpy(m_event.accountId, accountId, sizeof(m_event.accountId));
	}
	switch (m_event.state) {
	case 1:
	case 3: {
		ConsoleInfoUc uc;
		int data = 0;

		if (uc.Write(&data) != 0)
			break;
		break;
	}
	case 2:
		consoleInfoProcLogAddRunning(true, &m_event);
		break;
	case 5:
		CountUsage();
		break;
	}
	consoleInfoProcLogAdd(consoleInfoGetProcLog(), &m_event, 0);
	m_running = false;
	if (m_wait) {
		while (!m_done)
			thread::Sleep(MEDIA_INFO_WAIT_SLEEP);
	}
	return 0;
}

/* (0x814098DC, one function for the build's empty ones) */
void MediaInfoWriterJob::Finish(int result)
{
}
