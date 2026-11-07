/*
 * PhotoAppLaunchJob: opens the Photos application at its URI on paf's job thread.
 */
#include <stddef.h>
#include <std/string>
#include <common/job_slots.h>
#include <appmgr.h>
#include "photo_app_launch_job.h"

using namespace paf;

/* the launch flags the Photos application is opened with */
#define PHOTO_APP_LAUNCH_FLAGS	0x60000

PhotoAppLaunchJob::PhotoAppLaunchJob(const string &uri)
	: common::SlotJobItem("PhotoAppLaunchJob", NULL), m_uri(uri)
{
}

PhotoAppLaunchJob::~PhotoAppLaunchJob()
{
}

int PhotoAppLaunchJob::Run()
{
	return sceAppMgrLaunchAppByUri(PHOTO_APP_LAUNCH_FLAGS, m_uri.c_str());
}

void PhotoAppLaunchJob::Cancel()
{
	job::JobItem::Cancel();
}

void PhotoAppLaunchJob::Finish(int result)
{
	common::SlotJobItem::Finish(result);
}
