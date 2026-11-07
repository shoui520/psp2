/*
 * The job that opens the Photos application at a URI (the job's name, PhotoAppLaunchJob, is
 * Sony's; the class's name is taken from it).
 */
#ifndef PHOTOREVIEW_DIALOG_PHOTO_APP_LAUNCH_JOB_H
#define PHOTOREVIEW_DIALOG_PHOTO_APP_LAUNCH_JOB_H

#include <std/string>
#include <common/job_slots.h>

class PhotoAppLaunchJob : public paf::common::SlotJobItem {
public:
	PhotoAppLaunchJob(const paf::string &uri);
	virtual ~PhotoAppLaunchJob();

	virtual int Run();
	virtual void Cancel();
	virtual void Finish(int result);

	paf::string m_uri;
};

#endif
