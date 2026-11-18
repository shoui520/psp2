/*
 * BgCopySrvMgr (Sony's bgcopy_service_manager.cpp): the imports the background copy service
 * runs, by the service's ID for each, one of them current; and the service itself, NPXS10100,
 * launched and asked for its status.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/string>
#include <std/string.h>
#include <std/map>
#include "mtp_internal.h"
extern "C" {
#include <appmgr.h>
#include <appmgr/appmgr.h>
}

#define BGCOPY_SERVICE_MANAGER_SOURCE_FILE \
	"W:\\Build\\J01509658\\vsh\\app\\common\\src\\mtp\\libmtp\\bgcopy_service_manager.cpp"

/* the background copy service */
#define BGCOPY_SERVICE_NAME		"NPXS10100"

typedef paf::map<SceUInt32, BgCopyImportContext *>::iterator BgCopyContextIterator;

SceUInt32 BgCopySrvMgr::s_currentId;

__attribute__((noinline)) int BgCopySrvMgr::findContext(SceUInt32 id,
						      BgCopyImportContext **context)
{
	BgCopyContextIterator it = m_contexts.find(id);

	if (it == m_contexts.end())
		return SCE_MTP_ERROR_DEVICE;
	*context = it->second;
	return 0;
}

BgCopySrvMgr::BgCopySrvMgr()
{
	m_contexts.clear();
}

int BgCopySrvMgr::addContext(const BgCopyImportContext &context)
{
	BgCopyImportContext *copy = new BgCopyImportContext(context);

	m_contexts.insert(paf::pair<SceUInt32, BgCopyImportContext *>(copy->id, copy));
	return 0;
}

int BgCopySrvMgr::removeContext(SceUInt32 id)
{
	BgCopyImportContext *context = NULL;

	if (findContext(id, &context) == 0 && context != NULL) {
		delete context;
		m_contexts.erase(id);
	}
	return 0;
}

void BgCopySrvMgr::setCurrentId(SceUInt32 id)
{
	s_currentId = id;
}

SceUInt32 BgCopySrvMgr::getCurrentId()
{
	return s_currentId;
}

int BgCopySrvMgr::setPauseSource(void *data)
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return ret != 0 ? ret : SCE_MTP_ERROR_DEVICE;
	context->pauseSource = data;
	return ret;
}

int BgCopySrvMgr::setPauseTarget(void *data)
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return ret != 0 ? ret : SCE_MTP_ERROR_DEVICE;
	context->pauseTarget = data;
	return ret;
}

int BgCopySrvMgr::setPath(paf::string path)
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return ret != 0 ? ret : SCE_MTP_ERROR_DEVICE;
	context->path = paf::string(path);
	return ret;
}

int BgCopySrvMgr::setStatus(const MtpBgCopyTaskStatus *status)
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return ret != 0 ? ret : SCE_MTP_ERROR_DEVICE;
	context->status = *status;
	context->status.name = status->name;
	return ret;
}

int BgCopySrvMgr::setRestart(bool restart)
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return ret != 0 ? ret : SCE_MTP_ERROR_DEVICE;
	context->restart = restart;
	return ret;
}

/* the error code in its place when there is no current import */
void *BgCopySrvMgr::getPauseSource()
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return (void *)(ret != 0 ? ret : SCE_MTP_ERROR_DEVICE);
	return context->pauseSource;
}

void *BgCopySrvMgr::getPauseTarget()
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return (void *)(ret != 0 ? ret : SCE_MTP_ERROR_DEVICE);
	return context->pauseTarget;
}

int BgCopySrvMgr::getPath(paf::string *path)
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return ret != 0 ? ret : SCE_MTP_ERROR_DEVICE;
	*path = context->path;
	return ret;
}

int BgCopySrvMgr::getType()
{
	BgCopyImportContext *context = NULL;

	if (findContext(s_currentId, &context) != 0 || context == NULL)
		return 0;
	return context->type;
}

int BgCopySrvMgr::getStatus(MtpBgCopyTaskStatus *status)
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return ret != 0 ? ret : SCE_MTP_ERROR_DEVICE;
	*status = context->status;
	return ret;
}

int BgCopySrvMgr::clearStatus()
{
	BgCopyImportContext *context = NULL;
	int ret;

	ret = findContext(s_currentId, &context);
	if (ret != 0 || context == NULL)
		return ret != 0 ? ret : SCE_MTP_ERROR_DEVICE;
	context->status.name = "";
	context->status.progress = 0;
	context->status.index = 0;
	context->status.total = 0;
	context->status.remaining = 0;
	context->status.result = 0;
	return ret;
}

/* func is called for every import not to be started again; those it accepts are removed */
int BgCopySrvMgr::forEachRestart(int (*func)(SceUInt32 id))
{
	BgCopyContextIterator it = m_contexts.begin();

	if (func == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	while (it != m_contexts.end()) {
		if (it->second->restart != 0) {
			++it;
		} else if (func(it->first) >= 0) {
			m_contexts.erase(it);
			it = m_contexts.begin();
		} else {
			++it;
		}
	}
	return 0;
}

BgCopySrvMgr::~BgCopySrvMgr()
{
	for (BgCopyContextIterator it = m_contexts.begin(); it != m_contexts.end(); ++it) {
		if (it->second != NULL)
			delete it->second;
	}
	m_contexts.clear();
}

int mtpBgCopyLaunchService(int arg1, int arg2)
{
	int ret;

	sceClibPrintf("TRACE : %s %d %s\n", BGCOPY_SERVICE_MANAGER_SOURCE_FILE, 26, __FUNCTION__);
	ret = sceAppMgrLaunchAppByName(0, BGCOPY_SERVICE_NAME, NULL);
	if (ret != 0)
		sceClibPrintf("[BgCopySrvMgr:Error] sceAppMgrLaunchAppByName() = 0x%08x[%d]\n", ret,
			      ret);
	return ret;
}

int mtpBgCopyGetServiceStatus(SceAppMgrAppStatus *status)
{
	int ret;

	sce_paf_memset(status, 0, sizeof(*status));
	ret = sceAppMgrGetStatusByName(BGCOPY_SERVICE_NAME, status);
	if (ret != 0)
		sceClibPrintf("[BgCopySrvMgr:Error] Error getting BgCopyService Status by Name: %#x\n",
			      ret);
	return ret;
}
