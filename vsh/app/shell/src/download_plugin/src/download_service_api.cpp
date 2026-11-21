/*
 * download_service_api.cpp: the download service's calls (0x81138F80-0x8113A18E; the file's
 * name is ours). Each checks that the bgdl library's manager is there and its arguments, and
 * passes the call on to the library (esecron) or to download_plugin; the service's IPMI
 * dispatcher (0x8113BCFE) runs one per method, and the shell's own plugins call them directly.
 * The first argument is the IPMI session (0 from the shell's own callers), never read.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/string.h>
#include <std/string>
#include <dir/dir.h>
#include <plugin.h>

#include "../../esecron/include/esebase.hpp"
#include "../../esecron/include/esetask.hpp"
#include "../../esecron/include/eseparam.hpp"
#include "../../esecron/include/eseutil.hpp"
#include "../../esecron/include/esehttp.hpp"
#include "src/livespace_db/lsdb_keys.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "download_plugin.h"
#include "download_service.h"

/* the param file's keys read here */
#define BGDL_KEY_RUN_STATUS		101
#define BGDL_KEY_RESULT			104
#define BGDL_KEY_NAME			105
#define BGDL_KEY_KEY113			113
#define BGDL_KEY_URL			202
#define BGDL_KEY_KEY204			204
#define BGDL_KEY_TOTAL			206
#define BGDL_KEY_CURRENT		208
#define BGDL_KEY_TITLE_ID		220
#define BGDL_KEY_FILE_NAME		5000	/* plus 32 per file */
#define BGDL_KEY_FILE_DIGEST		5001
#define BGDL_KEY_FILE_SIZE		5002

/* the room a download's licence takes on ux0: when it has none yet: two clusters */
#define BGDL_LICENSE_PATH		"ux0:/MLN_ROOT/LICENSE/"
#define BGDL_DEFAULT_SIZE		0xC000

/* (0x81138F80) a download task added; *id its id */
int Bgdl_81138F80(IPMI::Session *session, int kind, BgdlTaskDetail *detail, SceUInt32 *id)
{
	SceUInt32 taskId;
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (detail == NULL)
		return BGDL_ERROR_NULL_TITLE;
	taskId = (SceUInt32)-1;
	/* the detail is the request's layout (PLACEHOLDER: one struct once both are named) */
	ret = bgdlAddDownloadTask(kind, (const BgdlDownloadRequest *)detail, &taskId);
	if (ret == 0) {
		if (id != NULL)
			*id = taskId;
		return 0;
	}
	return ret;
}

/* (0x81138FE4) the same, its id not given back */
int Bgdl_81138FE4(IPMI::Session *session, int kind, const BgdlTaskDetail *detail)
{
	SceUInt32 taskId;
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (detail == NULL)
		return BGDL_ERROR_NULL_TITLE;
	taskId = (SceUInt32)-1;
	ret = bgdlAddDownloadTask(kind, (const BgdlDownloadRequest *)detail, &taskId);
	if (ret == 0)
		return 0;
	return ret;
}

/* (0x8113903E) a task removed */
int Bgdl_8113903E(IPMI::Session *session, SceUInt32 id)
{
	BgdlManager *manager = bgdlManagerGet();
	int ret;

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = manager->CheckTask(id);
	if (ret == 0) {
		ret = bgdlRemoveTask(id);
		if (ret == 0)
			return 0;
	}
	return ret;
}

/* (0x8113906E) a task cancelled and removed, what it had done given back first */
int Bgdl_8113906E(IPMI::Session *session, SceUInt32 id, BgdlTaskState *state)
{
	BgdlManager *manager = bgdlManagerGet();
	int ret;

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = manager->CheckTask(id);
	if (ret != 0)
		return ret;
	ret = bgdlCancelTask(id);
	if (ret != 0 && ret != (int)BGDL_ERROR_NOT_QUEUED)
		return ret;
	if (state != NULL) {
		sce_paf_memset(state, 0, sizeof(*state));

		EseParam param(id);

		if (param.open() != 0)
			return DOWNLOAD_SVC_ERROR_NO_PARAM;
		param.read(BGDL_KEY_CURRENT, &state->value208, sizeof(state->value208));
		param.read(BGDL_KEY_TOTAL, &state->value206, sizeof(state->value206));

		paf::string value;

		param.read(BGDL_KEY_KEY204, &value);
		if (value.length() <= sizeof(state->value204) - 1)
			sce_paf_strncpy(state->value204, value.c_str(), sizeof(state->value204));
	}
	return Bgdl_8113903E(session, id);
}

/* (0x81139190) a task started again */
int Bgdl_81139190(IPMI::Session *session, SceUInt32 id)
{
	BgdlManager *manager = bgdlManagerGet();
	int ret;

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = manager->CheckTask(id);
	if (ret == 0) {
		ret = bgdlReloadTask(id);
		if (ret == 0)
			return 0;
	}
	return ret;
}

/* (0x811391C0) a task cancelled */
int Bgdl_811391C0(IPMI::Session *session, SceUInt32 id)
{
	BgdlManager *manager = bgdlManagerGet();
	int ret;

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = manager->CheckTask(id);
	if (ret == 0) {
		ret = bgdlCancelTask(id);
		if (ret == 0)
			return 0;
	}
	return ret;
}

/* (0x811391F0) the tasks removed, only the done ones when doneOnly is set */
int Bgdl_811391F0(IPMI::Session *session, bool doneOnly)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = bgdlRemoveTasks(doneOnly);
	if (ret == 0)
		return 0;
	return ret;
}

/* (0x81139216) the tasks started again */
int Bgdl_81139216(IPMI::Session *session)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = bgdlReloadTasks();
	if (ret == 0)
		return 0;
	return ret;
}

/* (0x81139238) the waiting tasks given up */
int Bgdl_81139238(IPMI::Session *session)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = bgdlCancelWaitingTasks();
	if (ret == 0)
		return 0;
	return ret;
}

/* (0x8113925A) a task's progress */
int Bgdl_8113925A(IPMI::Session *session, SceUInt32 id, BgdlTaskProgress *progress)
{
	BgdlManager *manager = bgdlManagerGet();
	int ret;

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = manager->CheckTask(id);
	if (ret == 0) {
		BgdlTaskStatus status;

		ret = bgdlGetTaskStatus(id, &status);
		if (ret == 0) {
			if (progress != NULL) {
				progress->id = status.id;
				progress->total = status.total;
				progress->current = status.current;
				progress->filesTotal = status.filesTotal;
				progress->filesCurrent = status.filesCurrent;
				progress->fileCount = status.fileCount;
				progress->fileNumber = status.fileNumber;
				progress->remaining = status.remaining;
				progress->filesRemaining = status.filesRemaining;
			}
			return 0;
		}
	}
	return ret;
}

/* (0x81139318) the tasks listed, at most max of them */
int Bgdl_81139318(IPMI::Session *session, int max, int unused, int *count, BgdlTaskId *ids)
{
	BgdlManager *manager = bgdlManagerGet();
	int n;
	BgdlTaskEntry list[BGDL_QUEUE_MAX];
	int ret;
	int i;

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	n = max;
	ret = manager->GetTaskList(list, &n, -1);
	if (ret == 0) {
		for (i = 0; i < n; i++) {
			ids[i].id = list[i].id;
			ids[i].param = list[i].param;
		}
		if (count != NULL)
			*count = n;
	}
	return ret;
}

/* (0x81139392) the tasks of a device (key 111) listed */
int Bgdl_81139392(IPMI::Session *session, int kind, int max, int *count, BgdlTaskId *ids)
{
	BgdlManager *manager = bgdlManagerGet();
	int n;
	BgdlTaskEntry list[BGDL_QUEUE_MAX];
	int found;
	int ret;
	SceUInt32 i;

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	n = max;
	ret = manager->GetTaskList(list, &n, -1);
	if (ret == 0) {
		found = 0;
		for (i = 0; i < (SceUInt32)n; i++) {
			if (list[i].param.key111 == kind) {
				found++;
				ids->id = list[i].id;
				ids->param = list[i].param;
				ids++;
			}
		}
		if (count != NULL)
			*count = found;
	}
	return ret;
}

/* (0x8113942C) a task's url */
int Bgdl_8113942C(IPMI::Session *session, int type, SceUInt32 id, BgdlTaskDetail *detail)
{
	BgdlManager *manager = bgdlManagerGet();
	int ret;

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = manager->CheckTask(id);
	if (ret == 0) {
		if (detail == NULL || id == (SceUInt32)-1)
			return DOWNLOAD_SVC_ERROR_INVALID_ARGUMENT;
		sce_paf_memset(detail, 0, sizeof(*detail));

		EseParam param(id);

		if (param.open() != 0)
			return DOWNLOAD_SVC_ERROR_NO_PARAM;
		param.read(BGDL_KEY_URL, detail->path, sizeof(detail->path));
		return 0;
	}
	return ret;
}

/* (0x811394CC) a content asked of the server */
int Bgdl_811394CC(IPMI::Session *session, const BgdlServerQuery *query, const BgdlServerIds *ids,
		  BgdlContentInfo *info)
{
	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (query == NULL || query->url[0] == '\0')
		return DOWNLOAD_SVC_ERROR_INVALID_ARGUMENT;
	sce_paf_memset(info, 0, sizeof(*info));

	bool cancel = false;

	return bgdlServerQuery(query, ids, info, &cancel);
}

/* (0x81139536) the patch list's path, ux0:'s or ur0:'s */
int Bgdl_81139536(IPMI::Session *session, const SceUInt32 *flags, char *path)
{
	paf::string file("");
	int ret;

	if (flags == NULL)
		return DOWNLOAD_SVC_ERROR_INVALID_ARGUMENT;
	ret = bgdlPatchFile(&file, (*flags & DOWNLOAD_SVC_FLAG_UR0) ? 2 : 0);
	if (ret != 0 || file.length() == 0)
		return ret;
	if (file.length() > 255)
		return DOWNLOAD_SVC_ERROR_PATH_LONG;
	sce_paf_memset(path, 0, 256);
	sce_paf_strncpy(path, file.c_str(), 256);
	return 0;
}

/* (0x8113961C) a package download added, download_plugin told when it is to be installed */
int Bgdl_8113961C(IPMI::Session *session, const BgdlPackageRequest *request, BgdlPackageResult *result)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (request == NULL || result == NULL)
		return BGDL_ERROR_NULL_TITLE;
	ret = bgdlAddPackageTask(request, result);
	if (ret == 1) {
		ret = 0;

		paf::Plugin *plugin = paf::Plugin::Find("download_plugin");

		if (plugin != NULL) {
			const DownloadPluginInterface *iface =
				(const DownloadPluginInterface *)plugin->GetInterface(1);

			if (iface != NULL)
				ret = iface->Bgdl_811239DC((void *)request, &result->id);
		}
	}
	return ret;
}

/* (0x81139680) indicator_plugin told of a task */
int Bgdl_81139680(IPMI::Session *session, SceUInt32 id)
{
	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;

	paf::Plugin *plugin = paf::Plugin::Find("indicator_plugin");

	if (plugin != NULL) {
		const IndicatorPluginInterface *iface =
			(const IndicatorPluginInterface *)plugin->GetInterface(1);

		if (iface != NULL) {
			IndicatorTaskEvent event;

			sce_paf_memset(&event, 0, sizeof(event));
			event.taskId = id;
			iface->TaskEvent(&event);
		}
	}
	return 0;
}

/* (0x811396EE) a content the server described added; *id its id */
int Bgdl_811396EE(IPMI::Session *session, const BgdlContentRequest *request,
		  const BgdlContentAccount *account, const BgdlContentInfo *info, SceUInt32 *id)
{
	SceUInt32 taskId;
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (request == NULL || account == NULL || info == NULL)
		return BGDL_ERROR_NULL_TITLE;
	taskId = (SceUInt32)-1;
	ret = bgdlAddContentTask(request, account, info, &taskId);
	if (ret == 0) {
		if (id != NULL)
			*id = taskId;
		return 0;
	}
	return ret;
}

/*
 * (0x81139766) a download the server describes added; *id its id. 1 from the library: the
 * content is installed at once, by download_plugin. A pre-order's event is logged.
 */
int Bgdl_81139766(IPMI::Session *session, const BgdlDownloadRequest *request, BgdlContentInfo *info,
		  SceUInt32 *id)
{
	SceUInt32 taskId;
	SceUInt32 newId;
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (request == NULL || info == NULL)
		return BGDL_ERROR_NULL_TITLE;
	taskId = (SceUInt32)-1;
	ret = bgdlAddServerTask(request, info, &taskId);
	newId = taskId;
	if ((SceUInt32)ret > 1)
		return ret;
	if (id != NULL)
		*id = taskId;
	if (ret == 1) {
		ret = 0;

		paf::Plugin *plugin = paf::Plugin::Find("download_plugin");

		if (plugin != NULL) {
			const DownloadPluginInterface *iface =
				(const DownloadPluginInterface *)plugin->GetInterface(1);

			/* the caller's install arguments are the request's last fields; the
			   second argument is not read */
			if (iface != NULL)
				ret = iface->Bgdl_811239D4((const FgdlCaller *)request, (int)info,
							   newId);
		}
		if (ret == 1) {
			BgdlDispatcherState state;

			DownloadService::Bgdl_8113A434()->Bgdl_8113A47C(&state, false);
		}
	}
	if (request->contentId[0] != '\0') {
		LsdbNewEventKey key;

		key.titleId = "LOGSTATUS0";
		key.itemId = request->contentId;
		key.itemId += ":PREORDER";
		Bgdl_81123C90(0, &key, 0);
	}
	return ret;
}

/* (0x81139906) a package task's progress, from its param file */
int Bgdl_81139906(IPMI::Session *session, SceUInt32 id, BgdlPackageStatus *status)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (status == NULL)
		return BGDL_ERROR_NULL_TITLE;
	sce_paf_memset(status, 0, sizeof(*status));

	EseParam param(id);

	ret = param.open();
	if (ret == 0) {
		int runStatus = 1;
		int result = -1;

		param.read(BGDL_KEY_RUN_STATUS, &runStatus, sizeof(runStatus));
		param.read(BGDL_KEY_RESULT, &result, sizeof(result));

		bool key113 = false;

		param.read(BGDL_KEY_KEY113, &key113, sizeof(key113));
		if (runStatus == 2) {
			status->state = 3;
		} else if (runStatus == 1) {
			status->state = 2;
		} else {
			EseTask *task = bgdlManagerGet()->FindTask(id);

			if (task != NULL && task->m_ParamCache.runStatus == 0 &&
			    task->m_ParamCache.key113 && task->result == 0)
				status->state = 4;
			else
				status->state = result == 0 || result == (int)BGDL_ERROR_INTERRUPTED ||
						result == (int)BGDL_ERROR_CANCELLED;
		}
		param.read(500, &status->key500, sizeof(status->key500));
		param.read(501, &status->key501, sizeof(status->key501));
		param.read(503, &status->key503, sizeof(status->key503));
		param.read(504, &status->key504, sizeof(status->key504));

		paf::string titleId;

		param.read(BGDL_KEY_TITLE_ID, &titleId);
		if (titleId.length() != 0)
			sce_paf_snprintf(status->titleId, sizeof(status->titleId) - 1, "%s",
					 titleId.c_str());

		paf::string name;

		param.read(BGDL_KEY_NAME, &name);
		if (name.length() != 0)
			sce_paf_snprintf(status->name, sizeof(status->name), "%s", name.c_str());
	}
	return ret;
}

/* (0x81139AE8) a package task's file, from its param file */
int Bgdl_81139AE8(IPMI::Session *session, SceUInt32 id, int index, BgdlPackageEntry *entry)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (entry == NULL)
		return BGDL_ERROR_NULL_TITLE;
	sce_paf_memset(entry, 0, sizeof(*entry));

	EseParam param(id);

	ret = param.open();
	if (ret == 0) {
		paf::string name;
		int key = 32 * index;

		ret = param.read(key + BGDL_KEY_FILE_NAME, &name);
		if (name.length() != 0)
			sce_paf_snprintf(entry->name, 255, "%s", name.c_str());
		param.read(key + BGDL_KEY_FILE_SIZE, &entry->size, sizeof(entry->size));
		param.read(key + BGDL_KEY_FILE_DIGEST, entry->digest, sizeof(entry->digest));
	}
	return ret;
}

/* (0x81139BD6) a list's package files added to a package task */
int Bgdl_81139BD6(IPMI::Session *session, SceUInt32 id, const BgdlPackageRequest *request)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (request == NULL)
		return BGDL_ERROR_NULL_TITLE;
	ret = bgdlAddPackageFiles(id, request);
	if (ret == 0)
		return 0;
	return ret;
}

/* (0x81139C0C) a task of the content is there */
int Bgdl_81139C0C(IPMI::Session *session, const BgdlContentQuery *query)
{
	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (query != NULL)
		return bgdlHasContentTask(query);
	return BGDL_ERROR_NULL_TITLE;
}

/* (0x81139C36) the content's tasks counted */
int Bgdl_81139C36(IPMI::Session *session, const BgdlContentQuery *query)
{
	BgdlManager *manager = bgdlManagerGet();

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (query != NULL)
		return manager->CountContentTasks(query);
	return BGDL_ERROR_NULL_TITLE;
}

/* (0x81139C60) the foreground install of task id: its state, its result once it is done */
int Bgdl_81139C60(IPMI::Session *session, SceUInt32 id, BgdlDispatcherState *state, int *result)
{
	if (state == NULL)
		return BGDL_ERROR_NULL_TITLE;
	DownloadService::Bgdl_8113A434()->Bgdl_8113A47C(state, true);
	if (state->state == 3 && state->taskId == id) {
		DownloadService::Bgdl_8113A434()->Bgdl_8113A4DA(result);
		return 0;
	}
	DownloadService::Bgdl_8113A434()->Bgdl_8113A47C(state, false);
	if (state->state != 0 && state->taskId == id)
		return 0;
	return BGDL_ERROR_NO_TASK;
}

/* (0x81139CBE) an installed task made ready to install again */
int Bgdl_81139CBE(IPMI::Session *session, SceUInt32 id)
{
	if (bgdlManagerGet() != NULL)
		return bgdlResetInstalledTask(id);
	return BGDL_ERROR_NOT_STARTED;
}

/* (0x81139CDC) ux0:'s space, and whether it has the room for a download of size bytes */
int Bgdl_81139CDC(IPMI::Session *session, SceUInt32 size, BgdlSpaceInfo *info)
{
	int ret;

	if (Bgdl_81112284("ux0:") != true)
		return BGDL_ERROR_NO_UX0;
	if (info != NULL)
		sce_paf_memset(info, 0, sizeof(*info));

	BgdlStorageInfo storage;

	ret = Bgdl_8111234C("ux0:", &storage);
	if (ret == 0) {
		paf::string license(BGDL_LICENSE_PATH);
		SceUInt64 extra = 0;
		SceUInt64 needed;

		if (paf::Dir::IsExist(license.c_str()) != true)
			extra = 2 * storage.clusterSize;
		if (size != 0)
			needed = Bgdl_81112310(size, storage.clusterSize);
		else
			needed = Bgdl_81112310(BGDL_DEFAULT_SIZE, storage.clusterSize);
		needed += extra;
		if (info != NULL) {
			info->max = storage.max;
			info->used = storage.max - storage.free;
			info->free = storage.free;
			info->needed = needed;
		}
		if (storage.free < needed)
			return BGDL_ERROR_UX0_FULL;
		return 0;
	}
	return ret;
}

/* (0x81139E2A) the same, the space not given back */
int Bgdl_81139E2A(IPMI::Session *session, SceUInt32 size)
{
	BgdlSpaceInfo info;

	return Bgdl_81139CDC(session, size, &info);
}

/* (0x81139E52) the title's storage is there; *grw0 when it is grw0: */
int Bgdl_81139E52(IPMI::Session *session, const BgdlStorageQuery *query, bool *grw0)
{
	BgdlManager *manager;

	if (query == NULL || grw0 == NULL)
		return BGDL_ERROR_NULL_TITLE;
	if (query->titleId[0] == '\0')
		return BGDL_ERROR_NULL_TITLE;
	manager = bgdlManagerGet();
	if (manager != NULL)
		return manager->CheckStorage(query->titleId, grw0);
	return BGDL_ERROR_NOT_STARTED;
}

/* (0x81139E92) a title's install queued by download_plugin */
int Bgdl_81139E92(IPMI::Session *session, const FgdlTitleRequest *request, int arg)
{
	int ret;

	if (request == NULL || arg == 0)
		return BGDL_ERROR_NULL_TITLE;
	ret = 0;

	paf::Plugin *plugin = paf::Plugin::Find("download_plugin");

	if (plugin != NULL) {
		const DownloadPluginInterface *iface =
			(const DownloadPluginInterface *)plugin->GetInterface(1);

		if (iface != NULL) {
			ret = iface->Bgdl_811239E4(request, arg);
			if (ret == 1) {
				BgdlDispatcherState state;

				DownloadService::Bgdl_8113A434()->Bgdl_8113A47C(&state, false);
			}
		}
	}
	return ret;
}

/* (0x81139EFE) the download event of a livespace_db row logged, by download_plugin */
int Bgdl_81139EFE(IPMI::Session *session, int unused, SceInt64 rowid)
{
	int ret = 0;

	if (rowid == 0)
		return BGDL_ERROR_NULL_TITLE;

	paf::Plugin *plugin = paf::Plugin::Find("download_plugin");

	if (plugin != NULL) {
		const DownloadPluginInterface *iface =
			(const DownloadPluginInterface *)plugin->GetInterface(1);

		if (iface != NULL)
			return iface->EventLogDownload(rowid);
	}
	return ret;
}

/* (0x81139F46) a method kept for its argument check */
int Bgdl_81139F46(IPMI::Session *session, const void *arg)
{
	if (arg != NULL)
		return 0;
	return BGDL_ERROR_NULL_TITLE;
}

/* (0x81139F58) the same, after the manager's */
int Bgdl_81139F58(IPMI::Session *session, int unused, const void *arg)
{
	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (arg != NULL)
		return 0;
	return BGDL_ERROR_NULL_TITLE;
}

/* (0x81139F7E) the PSM runtime's download task made; *needed the space it lacks */
int Bgdl_81139F7E(IPMI::Session *session, int arg, SceUInt64 *needed)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (arg == 0)
		return BGDL_ERROR_NULL_TITLE;

	paf::string iconPath;

	iconPath.clear();
	ret = bgdlAddPsmRuntimeTask(&iconPath, arg, needed);
	return ret;
}

/* (0x81139FFE) a title's patch registered */
int Bgdl_81139FFE(IPMI::Session *session, const BgdlPatchRequest *request)
{
	int ret;

	if (bgdlManagerGet() == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (request == NULL)
		return BGDL_ERROR_NULL_TITLE;
	if (request->titleId[0] == '\0' || request->contentId[0] == '\0')
		return BGDL_ERROR_NULL_TITLE;

	BgdlTaskPackage package;

	package.key220 = request->titleId;
	package.key217 = request->contentId;
	ret = bgdlRegistPatchByTitleID(&package, NULL);
	return ret;
}

/* (0x8113A0E2) an MTP copy added; *id its id */
int Bgdl_8113A0E2(IPMI::Session *session, const BgdlMtpRequest *request, SceUInt32 *id)
{
	BgdlManager *manager = bgdlManagerGet();

	if (manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (request != NULL)
		return manager->AddMtpTask(request, id);
	return BGDL_ERROR_NULL_TITLE;
}

/* (0x8113A110) */
int Bgdl_8113A110(IPMI::Session *session, int arg)
{
	return Bgdl_8113BCDE(session, arg, 0);
}
