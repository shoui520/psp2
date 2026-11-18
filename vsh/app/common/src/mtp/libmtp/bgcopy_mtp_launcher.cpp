/*
 * BgCopyMtpLauncher: the thread that starts NPXS10100, the background copy service, when it
 * does not run, and waits up to fifteen seconds for its process.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <kernel/threadmgr.h>
#include <std/string>
#include <std/vector>
#include <thread/thread.h>
#include "mtp_internal.h"
extern "C" {
#include <appmgr/appmgr.h>
}

PAF_VECTOR_INLINE_DTOR(string)

#define MTP_TAG_BGCP_LAUNCHER		"BGCP Launcher:"

#define BGCP_LAUNCHER_THREAD_PRIORITY	0x10000100
#define BGCP_LAUNCHER_THREAD_STACK	0x2000

/* the service's process is waited for this many seconds */
#define BGCP_LAUNCHER_WAIT_COUNT	15

BgCopyMtpLauncher::BgCopyMtpLauncher() : m_state(0), m_thread(0), m_fileIndex(0)
{
	m_serviceIds[0] = -1;
	m_serviceIds[1] = -1;
	m_serviceIds[2] = -1;
	m_sourceName.clear();
	m_sourceData.clear();
	m_sourceFlags = 0;
	m_targetName.clear();
	m_targetInfo[0] = 0;
	m_targetInfo[1] = 0;
	m_targetInfo[2] = 0;
	m_targetInfo[3] = 0;
	m_targetSize = 0;
	m_targetFlags = 0;
	m_targetData.clear();
	m_targetKind = 0;
	m_titleFlags = 0;
	m_titleSize = 0;
	m_titleName.clear();
	m_titleKind = 0;
	m_dirFlags = 0;
	m_dirKind = 0;
	m_dirSize = 0;
	m_dirName.clear();
	m_thread = sceKernelCreateThread("SceMtpBgCopyLauncher", execute,
					 BGCP_LAUNCHER_THREAD_PRIORITY, BGCP_LAUNCHER_THREAD_STACK,
					 0, 0, NULL);
	if (m_thread < 0)
		MTP_ERR(MTP_TAG_BGCP_LAUNCHER, 38, "Create BgCopyMtpLauncher failed: 0x%x\n",
			m_thread);
}

BgCopyMtpLauncher::~BgCopyMtpLauncher()
{
	int ret;

	if (m_thread >= 0) {
		ret = sceKernelWaitThreadEnd(m_thread, NULL, NULL);
		if (ret != 0)
			MTP_ERR(MTP_TAG_BGCP_LAUNCHER, 50, "Wait BgCopyMtpLauncher failed: 0x%x\n",
				ret);
		ret = sceKernelDeleteThread(m_thread);
		if (ret != 0)
			MTP_ERR(MTP_TAG_BGCP_LAUNCHER, 55,
				"Delete BgCopyMtpLauncher failed: 0x%x\n", ret);
	}
}

int BgCopyMtpLauncher::start()
{
	BgCopyMtpLauncher *self;
	int ret;

	if (m_thread < 0)
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	self = this;
	ret = sceKernelStartThread(m_thread, sizeof(self), &self);
	if (ret != 0)
		MTP_ERR(MTP_TAG_BGCP_LAUNCHER, 109, "Start BgCopyMtpLauncher failed: 0x%x\n", ret);
	return ret;
}

/* a host's operation is running (or the contexts cannot say) */
bool BgCopyMtpLauncher::isMtpTaskInProgress()
{
	int idle;
	int ret;

	idle = 0;
	ret = mtpContextIsIdle(&idle);
	if (ret != 0) {
		MTP_ERR(MTP_TAG_BGCP_LAUNCHER, 174,
			"BgCopyMtpLauncher::isMtpTaskInProgress() failed: 0x%x\n", ret);
		return true;
	}
	return idle == 0;
}

int BgCopyMtpLauncher::execute(SceSize args, void *argp)
{
	SceAppMgrAppStatus status;
	int count;
	int ret;

	ret = mtpBgCopyGetServiceStatus(&status);
	if (ret != 0 || status.processId == -1) {
		ret = mtpBgCopyLaunchService(-1, -1);
		if (ret != 0) {
			MTP_ERR(MTP_TAG_BGCP_LAUNCHER, 206,
				"Error launching BgCopyService ret [%#x]\n", ret);
			return ret;
		}
		count = 0;
		ret = mtpBgCopyGetServiceStatus(&status);
		while (status.processId <= 0 && count < BGCP_LAUNCHER_WAIT_COUNT) {
			paf::thread::Sleep(1000);
			count++;
			ret = mtpBgCopyGetServiceStatus(&status);
		}
	}
	return ret;
}
