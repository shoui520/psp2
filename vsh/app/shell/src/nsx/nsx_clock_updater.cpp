/*
 * nsx_clock_updater.cpp: NSX directory setup, clock updater cron job, and push-info scheduler
 * 3.65-CEX: 0x8122ACC0 - 0x8122AFEC, 0x814288D8 - 0x81428970
 */

#include <stddef.h>
#include <scetypes.h>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <job/job.h>
#include <thread/thread.h>
#include <kernel/threadmgr.h>
#include <regmgr/regmgr.h>
#include "../../shell.h"
#include "nsx_internal.h"

extern "C" {
extern int sceKernelPowerTick(SceUInt32 type);
}

extern int liveareaCronEnqueueJob(paf::common::SharedPtr<paf::job::JobItem> &item);
extern void liveareaCronRegister(void (*callback)(void *), void *arg);
extern int npAccountIsSignedIn(void);

namespace {

static const char *const kWhatsNewDir = "ur0:shell/whats_new";
static const char *const kPushInfoDir = "ur0:shell/push_info";
static const int kClockUpdatePriority = 0x10000114;
static const int kClockUpdateDoneState = 2;
static const int kClockUpdatePollMs = 10;

/* 0x814288D8..0x81428970 */
class NsxClockUpdaterJob : public paf::job::JobItem {
public:
	NsxClockUpdaterJob()
		: paf::job::JobItem("SceNsxClockUpdaterJob", NULL),
		  m_requestId(0)
	{
		SceUID pid = sceKernelGetProcessId();
		npUpdateClockStart(&m_requestId, kClockUpdatePriority, pid);
	}

	virtual ~NsxClockUpdaterJob()
	{
	}

	virtual int Run()
	{
		int result = 0;
		if (m_requestId != 0) {
			while (npUpdateClockGetState(m_requestId) != kClockUpdateDoneState) {
				sceKernelPowerTick(1);
				paf::thread::Sleep(kClockUpdatePollMs);
			}
			npUpdateClockGetResult(m_requestId, &result);
			m_requestId = 0;
		}
		return result;
	}

	virtual void Finish(int result)
	{
		(void)result;
	}

private:
	int m_requestId;
};

/* 0x8122AFEC..0x8122B088 */
class NsxPushInfoCheckJob : public paf::job::JobItem {
public:
	NsxPushInfoCheckJob()
		: paf::job::JobItem("SceNsxPushInfoCheckJob", NULL)
	{
	}

	virtual ~NsxPushInfoCheckJob()
	{
	}

	virtual int Run()
	{
		return nsxPushInfoRunCheck();
	}

	virtual void Finish(int result)
	{
		(void)result;
	}
};

/* 0x8122AE54 */
static void nsxPushInfoCronCallback(void *userArg)
{
	(void)userArg;
	if (npAccountIsSignedIn() != 0)
		nsxPushInfoEnqueueJob();
}

/* 0x8122AE7E */
static void nsxClockUpdaterCronCallback(void *userArg)
{
	(void)userArg;
	int setAutomatically = 0;
	if (sceRegMgrGetKeyInt("/CONFIG/DATE", "set_automatically", &setAutomatically) == 0 &&
	    setAutomatically != 0) {
		paf::common::SharedPtr<paf::job::JobItem> jobRef(new NsxClockUpdaterJob());
		liveareaCronEnqueueJob(jobRef);
	}
}

} /* namespace */

/* 0x8122ACC0 */
int shellWhatsNewInit(bool reset)
{
	if (reset) {
		paf::Dir::RemoveRecursive(kWhatsNewDir);
		paf::Dir::RemoveRecursive(kPushInfoDir);
	}
	if (!paf::Dir::IsExist(kWhatsNewDir))
		paf::Dir::CreateRecursive(kWhatsNewDir);
	if (!paf::Dir::IsExist(kPushInfoDir))
		paf::Dir::CreateRecursive(kPushInfoDir);

	nsxClockUpdaterRegisterCron();
	nsxPushInfoRegisterCron();
	return 0;
}

/* 0x8122AD22 */
int nsxPushInfoTriggerCheck(void)
{
	return nsxPushInfoEnqueueJob();
}

/* 0x8122AD2C */
int nsxPushInfoRegisterCron(void)
{
	liveareaCronRegister(nsxPushInfoCronCallback, NULL);
	return 0;
}

/* 0x8122AD40 */
int nsxPushInfoEnqueueJob(void)
{
	paf::common::SharedPtr<paf::job::JobItem> jobRef(new NsxPushInfoCheckJob());
	int ret = liveareaCronEnqueueJob(jobRef);
	return (ret != 0) ? ret : 0;
}

/* 0x8122AE6A */
int nsxClockUpdaterRegisterCron(void)
{
	liveareaCronRegister(nsxClockUpdaterCronCallback, NULL);
	return 0;
}
