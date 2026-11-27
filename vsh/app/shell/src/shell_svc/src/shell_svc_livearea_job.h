/*
 * SceShell shell_svc: shared LiveAreaUtil async job base class
 */
#ifndef SHELL_SVC_LIVEAREA_JOB_H
#define SHELL_SVC_LIVEAREA_JOB_H

#include <scetypes.h>
#include <std/string>
#include <ipmi/session.h>
#include <job/job.h>

class LiveAreaUtilJobBase : public paf::job::JobItem {
public:
	LiveAreaUtilJobBase(IPMI::Session *session, SceUInt32 reqId,
		const char *path, const char *titleId, SceUInt8 flag);
	virtual ~LiveAreaUtilJobBase();
	virtual int Run() { return 0; }
	virtual void Finish(int) {}

	IPMI::Session *m_session;				/* +28 */
	SceUInt32 m_reqId;					/* +32 */
	paf::string m_path;					/* +36 */
	paf::string m_titleId;					/* +48 */
	SceUInt8 m_flag;					/* +60 */
};

#endif /* SHELL_SVC_LIVEAREA_JOB_H */
