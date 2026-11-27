/*
 * livearea_reset_job.cpp: livearea_plugin ContentResetJob
 * (0x811A3426-0x811A364C, sti 0x811A35DA).
 */
#include "livearea_internal.h"

class ContentResetJob : public paf::job::JobItem {
public:
	ContentResetJob(LiveAreaPage *page, int resetMask)
		: paf::job::JobItem("ContentResetJob", NULL),
		  m_title(page->GetTitleId()),
		  m_resetMask(resetMask)
	{}

	virtual ~ContentResetJob() {}				/* 0x811A34D6 / 0x811A350A */
	virtual int Run()					/* 0x811A3542 */
	{
		paf::common::SharedPtr<LiTemplate> tmpl;
		liveareaMgrGetCachedTemplate(&tmpl, &m_title, 0);
		return 0;
	}
	virtual void Finish(int result)				/* 0x811A35CA */
	{
		(void)result;
		liveareaMgrNotifyPage(m_title.c_str(), m_resetMask);
	}

	paf::string m_title;					/* +0x1C */
	int m_resetMask;					/* +0x28 */
};

/* 0x811A3426 */
paf::common::SharedPtr<paf::job::JobItem> liveareaEnqueueContentResetJob(LiveAreaPageInner *inner,
									 int resetMask)
{
	paf::job::JobItem *job = new ContentResetJob(inner->m_page, resetMask);
	paf::common::SharedPtr<paf::job::JobItem> jobRef(job);
	paf::job::JobQueue::default_queue->Enqueue(jobRef);
	return jobRef;
}
