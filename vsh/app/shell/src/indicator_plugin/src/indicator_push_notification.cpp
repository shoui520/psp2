/*
 * indicator_push_notification.cpp: a notice pushed to the notice cache as a job on paf's default
 * queue, a notice of the shell's own given the notifications of the title it runs first
 * (0x81182A28-0x81182CD0). The file's and the names here are ours (PLACEHOLDER), but for the
 * job's class, named by the name it gives its job.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <job/job.h>
#include "../../livespace_db/livespace_db_internal.h"
#include "../../livespace_db/lsdb_keys.h"
#include "indicator.h"

using namespace paf;

/* a notice pushed as a job (68 bytes) */
class JobPushNotification : public job::JobItem {
public:
	JobPushNotification(int type, IndicatorPushNotificationParam param);

	virtual ~JobPushNotification();

	virtual int Run();
	virtual void Finish(int result);

	int m_type;
	IndicatorPushNotificationParam m_param;
};

/* (0x81182A28) */
__attribute__((noinline))
JobPushNotification::JobPushNotification(int type, IndicatorPushNotificationParam param)
	: job::JobItem("JobPushNotification", NULL), m_type(type), m_param(param)
{
}

/* (0x81182A92) */
common::SharedPtr<job::JobItem>
indicatorPushNotificationStart(int type, const IndicatorPushNotificationParam &param)
{
	job::JobQueue *queue = job::JobQueue::default_queue;
	common::SharedPtr<job::JobItem> job(new JobPushNotification(type, param));

	queue->Enqueue(job);
	return job;
}

/* (0x81182BCA) */
JobPushNotification::~JobPushNotification()
{
}

/* a notice of the shell's own given the notifications of the title it runs; -1 for any other
   (0x81182C44) */
int JobPushNotification::Run()
{
	int result = -1;

	if (IsCanceled())
		return 0;
	if (m_type == INDICATOR_PUSH_NOTICE && m_param.event.get() != NULL) {
		IndicatorNewEventDb *db = indicatorNewEventDb();

		result = 0;
		/* Sony passes the title it runs where the event's title is read */
		m_param.event->notifyFlags = db->GetNotifyFlags(
			(const LsdbNewEvent *)&m_param.event->execTitleId);
	}
	return result;
}

/* (0x81182C7A) */
void JobPushNotification::Finish(int result)
{
	if (!IsCanceled())
		indicatorNoticeCache()->Add(m_param);
}
