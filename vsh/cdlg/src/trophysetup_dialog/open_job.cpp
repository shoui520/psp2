/*
 * The job that opens the application's trophy title and reads its state (Sony's file, named in
 * the 0.99x builds' messages).
 */
#include <stddef.h>
#include <scetypes.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include "np_trophy_setup_dialog_plugin.h"

using namespace paf;

TrophyOpenJob::~TrophyOpenJob()
{
}

void TrophyOpenJob::Cancel()
{
	m_controller->Abort();
	JobItem::Cancel();
}

int TrophyOpenJob::Run()
{
	int ret;

	ret = m_controller->Open();
	if (ret >= 0) {
		ret = m_controller->CheckTitleForRemoval();
		if (ret >= 0)
			return 0;
	}
	m_controller->Close();
	return ret;
}

void TrophyOpenJob::Finish(int result)
{
	m_finished(IsCanceled(), result, m_controller);
}

int StartTrophyOpenJob(job::JobQueue *queue, TrophyJobFinishedFunction finished,
		       SetupController *controller)
{
	common::SharedPtr<job::JobItem> item(new TrophyOpenJob(finished, controller));
	int ret;

	if (item.get() != NULL)
		ret = queue->Enqueue(item);
	else
		ret = TROPHY_SETUP_PLUGIN_ERROR_INTERNAL;
	return ret;
}
