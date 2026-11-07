/*
 * The job that removes the installed title when the user said so and sets the title up
 * (Sony's file, named in the 0.99x builds' messages).
 */
#include <stddef.h>
#include <scetypes.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include "np_trophy_setup_dialog_plugin.h"

using namespace paf;

TrophySetupJob::~TrophySetupJob()
{
}

void TrophySetupJob::Cancel()
{
	m_controller->Abort();
	JobItem::Cancel();
}

int TrophySetupJob::Run()
{
	int ret;

	ret = m_controller->RemoveTitle();
	if (ret >= 0) {
		ret = m_controller->Setup();
		if (ret >= 0)
			ret = 0;
	}
	m_controller->Close();
	return ret;
}

void TrophySetupJob::Finish(int result)
{
	m_finished(IsCanceled(), result, m_controller);
}

int StartTrophySetupJob(job::JobQueue *queue, TrophyJobFinishedFunction finished,
			SetupController *controller)
{
	common::SharedPtr<job::JobItem> item(new TrophySetupJob(finished, controller));
	int ret;

	if (item.get() != NULL) {
		ret = queue->Enqueue(item);
		if (ret >= 0)
			return 0;
	} else {
		ret = TROPHY_SETUP_PLUGIN_ERROR_INTERNAL;
	}
	return ret;
}
