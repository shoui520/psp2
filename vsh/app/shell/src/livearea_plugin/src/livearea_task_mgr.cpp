/*
 * SceShell livearea_plugin: LiveArea task manager facade
 * 3.65-CEX: 0x811B204C - 0x811B216C (sti 0x811B20FC)
 */
#include "livearea_internal.h"

class LiveAreaTaskMgr {
public:
	LiveAreaTaskMgr();
	~LiveAreaTaskMgr();

	int RequestContentUpdate(SceUInt64 *outId, const char *titleId, const SceUInt64 *timestamp,
				 bool requireNetwork, SceUInt8 checkDevDir, SceUInt8 postNotice,
				 void *callback, void *userdata, const char *localPkg);
	int RequestPatchCheck(SceUInt64 *outId, const char *titleId, bool requireNetwork,
			      int appType, void *callback, void *userdata);
	int RequestActivityUpdate(SceUInt64 *outId, const char *titleId, int forceFlag,
				  bool requireNetwork, void *callback, void *userdata);
	int RequestClean(SceUInt64 *outId, const char *titleId, bool requireNetwork,
			 void *callback, void *userdata);

private:
	LiveAreaUpdatorImpl *m_impl;
};

/* 0x811B204C */
LiveAreaTaskMgr::LiveAreaTaskMgr()
{
	void *mem = operator new(56);
	m_impl = mem ? liveareaUpdatorImplCtor(mem) : NULL;
}

/* 0x811B2068 */
LiveAreaTaskMgr::~LiveAreaTaskMgr()
{
	if (m_impl != NULL) {
		liveareaUpdatorImplDtor(m_impl);
		operator delete(m_impl);
	}
}

/* 0x811B207C */
int LiveAreaTaskMgr::RequestContentUpdate(SceUInt64 *outId, const char *titleId,
					  const SceUInt64 *timestamp, bool requireNetwork,
					  SceUInt8 checkDevDir, SceUInt8 postNotice,
					  void *callback, void *userdata, const char *localPkg)
{
	return liveareaUpdatorImplRequestContentUpdate(m_impl, outId, titleId, timestamp,
						       requireNetwork, checkDevDir, postNotice,
						       callback, userdata, localPkg);
}

/* 0x811B20AC */
int LiveAreaTaskMgr::RequestPatchCheck(SceUInt64 *outId, const char *titleId,
				       bool requireNetwork, int appType,
				       void *callback, void *userdata)
{
	return liveareaUpdatorImplRequestPatchCheck(m_impl, outId, titleId, requireNetwork,
						    appType, callback, userdata);
}

/* 0x811B20C8 */
int LiveAreaTaskMgr::RequestActivityUpdate(SceUInt64 *outId, const char *titleId,
					   int forceFlag, bool requireNetwork,
					   void *callback, void *userdata)
{
	return liveareaUpdatorImplRequestActivityUpdate(m_impl, outId, titleId, forceFlag,
							requireNetwork, callback, userdata);
}

/* 0x811B20E4 */
int LiveAreaTaskMgr::RequestClean(SceUInt64 *outId, const char *titleId, bool requireNetwork,
				  void *callback, void *userdata)
{
	return liveareaUpdatorImplRequestClean(m_impl, outId, titleId, requireNetwork,
					       callback, userdata);
}

LiveAreaTaskMgr *liveareaTaskMgrCtor(void *mem)
{
	return new (mem) LiveAreaTaskMgr();
}

LiveAreaTaskMgr *liveareaTaskMgrDtor(LiveAreaTaskMgr *taskMgr)
{
	taskMgr->~LiveAreaTaskMgr();
	return taskMgr;
}

int liveareaTaskMgrRequestContentUpdate(LiveAreaTaskMgr *taskMgr, SceUInt64 *outId,
					const char *titleId, const SceUInt64 *timestamp,
					bool requireNetwork, SceUInt8 checkDevDir,
					SceUInt8 postNotice, void *callback,
					void *userdata, const char *localPkg)
{
	return taskMgr->RequestContentUpdate(outId, titleId, timestamp, requireNetwork,
					     checkDevDir, postNotice, callback, userdata, localPkg);
}

int liveareaTaskMgrRequestPatchCheck(LiveAreaTaskMgr *taskMgr, SceUInt64 *outId,
				     const char *titleId, bool requireNetwork,
				     int appType, void *callback, void *userdata)
{
	return taskMgr->RequestPatchCheck(outId, titleId, requireNetwork, appType,
					  callback, userdata);
}

int liveareaTaskMgrRequestActivityUpdate(LiveAreaTaskMgr *taskMgr, SceUInt64 *outId,
					 const char *titleId, int forceFlag,
					 bool requireNetwork, void *callback, void *userdata)
{
	return taskMgr->RequestActivityUpdate(outId, titleId, forceFlag, requireNetwork,
					      callback, userdata);
}

int liveareaTaskMgrRequestClean(LiveAreaTaskMgr *taskMgr, SceUInt64 *outId,
				const char *titleId, bool requireNetwork,
				void *callback, void *userdata)
{
	return taskMgr->RequestClean(outId, titleId, requireNetwork, callback, userdata);
}
