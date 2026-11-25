/*
 * livearea_reset_job.cpp: livearea_plugin ContentResetJob
 * (0x811A3426-0x811A364C, sti 0x811A35DA). The file and class names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <job/job.h>
#include <common/shared_ptr.h>
#include "../../livespace_db/lsdb_keys.h"

extern paf::job::JobQueue *g_activityJobQueue;			/* 0x81488474 + 146*4 */
extern const char *liveareaPageGetTitleId(void *page);		/* 0x8119B990 */
extern int liveareaMgrLsdbCall(void *outSp, paf::string *title, unsigned char flag); /* 0x8119A99C */
extern void liveareaMgrNotifyPage(const char *titleId, int arg); /* 0x8119AC9C */

class ContentResetJob : public paf::job::JobItem {
public:
	ContentResetJob(void *page, int arg)
		: paf::job::JobItem("ContentResetJob", NULL),
		  m_title(liveareaPageGetTitleId(page)),
		  m_arg(arg)
	{}

	virtual ~ContentResetJob() {}				/* 0x811A34D6 / 0x811A350A */
	virtual int Run()					/* 0x811A3542 */
	{
		paf::common::SharedPtr<int> sp;
		liveareaMgrLsdbCall(&sp, &m_title, 0);
		return 0;
	}
	virtual void Finish(int result)				/* 0x811A35CA */
	{
		(void)result;
		liveareaMgrNotifyPage(m_title.c_str(), m_arg);
	}

	paf::string m_title;					/* +0x1C */
	int m_arg;						/* +0x28 */
};

/* 0x811A3426 */
paf::common::SharedPtr<paf::job::JobItem> liveareaSubA3426(void **inner, int arg)
{
	paf::job::JobItem *job = new ContentResetJob(inner[0], arg);
	paf::common::SharedPtr<paf::job::JobItem> sp(job);
	g_activityJobQueue->Enqueue(sp);
	return sp;
}
