/*
 * esetask.hpp: the background download library's task (Sony's EseTask, "PAF_NEW EseTask" in
 * the 0.990 prototype): what the manager keeps of each download (304
 * bytes, made in 0x811000D4, filled from the task's param file in 0x810FF6F4), kept in a
 * BgdlTaskList. The param file's keys are Sony's numbers (100-116, 206, 208, 217, 220, 500,
 * 501, 505, 5002 on); no record names the other types or the fields: the names are ours
 * (PLACEHOLDER), and a field known only by the key it is read from is named by the key.
 */
#ifndef ESECRON_ESETASK_HPP
#define ESECRON_ESETASK_HPP
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <std/map>
#include <thread/thread.h>
#include <job/job.h>
#include <file/file.h>
#include <common/shared_ptr.h>

#include "esebase.hpp"
#include "shell.h"

/* a task's state as the manager shows it (the manager copies 60 bytes of it) */
struct BgdlTaskStatus {
	BgdlTaskStatus() : id(0), total(0), current(0), filesTotal(0), filesCurrent(0), filesBase(0),
		fileCount(1), fileNumber(1), remaining(0), filesRemaining(0)
	{
	}

	SceUInt32 id;
	int reserved;			/* PLACEHOLDER: padding before the 64-bit fields */
	SceUInt64 total;		/* key 206 */
	SceUInt64 current;		/* key 208 */
	SceUInt64 filesTotal;		/* key 505 */
	SceUInt64 filesCurrent;
	SceUInt64 filesBase;		/* the sizes of the files already done (keys 5002 on) */
	SceUInt16 fileCount;		/* key 501, plus 1 */
	SceUInt16 fileNumber;		/* key 500 */
	int remaining;			/* seconds left (BgdlProgress::GetRemaining) */
	int filesRemaining;
};

/* libmtp's background copy, as the task keeps it (PLACEHOLDER names) */
/* a copy's progress as libmtp reports it (libmtp's MtpBgCopyProgress, less the id) */
struct BgdlTaskCopy {
	BgdlTaskCopy() : progress(0), number(0), total(1), remaining(0), reserved(0)
	{
		name = "";
	}

	paf::string name;
	SceUInt32 progress;		/* in percent */
	int number;			/* the object being copied, from 1 */
	SceUInt32 total;
	int remaining;			/* seconds left */
	int reserved;
};

/* a task's copy as GetTaskCopy gives it (PLACEHOLDER names) */
struct BgdlTaskCopyStatus {
	int result;			/* the task's copyResult */
	BgdlTaskCopy copy;
};

/*
 * libmtp's background copy (SceLibMtp_BF311CC8), as the shell gives it: libmtp's own
 * mtp_internal.h has the strings at +0x14 and +0x2C as reserved words
 */
struct MtpBgCopyProgress {
	SceUInt32 id;
	paf::string name;
	SceUInt32 progress;
	int number;
	SceUInt32 total;
	int remaining;
	int reserved;
};

class MtpBgCopyHost {
public:
	virtual ~MtpBgCopyHost() {}
	virtual int isRequested() = 0;
};

struct MtpBgCopyRunParam {
	SceUInt32 id;
	void *arg;
	void (*notify)(void *arg, const MtpBgCopyProgress *progress);
	int (*accept)(void);
	MtpBgCopyHost *host;
	paf::string file;		/* the task's file (key 203) */
	paf::string path;		/* the task's folder */
	paf::string paramFolder;	/* the folder its param file names (key 229) */
};

/* a task's flags from its param file (Sony's m_ParamCache, with its "type"; the rest ours) */
struct EseParamCache {
	SceUInt32 type : 4;		/* key 100 */
	SceUInt32 key107 : 4;
	SceUInt32 runStatus : 2;	/* key 101 (setRunStatus) */
	SceUInt32 key102 : 1;
	SceUInt32 ready : 1;		/* key 104 and key 113 both 0 */
	SceUInt32 key108 : 2;
	SceUInt32 state : 3;		/* key 109 (setState) */
	SceUInt32 key112 : 1;
	SceUInt32 key113 : 1;
	SceUInt32 key110 : 1;
	SceUInt32 key111 : 3;
	SceUInt32 kind : 3;		/* key 114 (BgdlTaskInfo::kind) */
	SceUInt32 key115 : 1;
	SceUInt32 key116 : 1;
	SceUInt32 reserved : 4;		/* PLACEHOLDER: kept as they are */
};

/* what the manager's listeners are told */
struct BgdlEvent {
	BgdlEvent()
	{
	}

	/* (0x81415638) */
	__attribute__((noinline)) BgdlEvent(int type, SceUInt32 id, int result, int reserved)
		: type(type), id(id), result(result), reserved(reserved), value(0)
	{
	}

	int type;			/* 0 added, 3 state, 4 run status (PLACEHOLDER: others) */
	SceUInt32 id;
	EseParamCache param;
	int result;
	int reserved;			/* PLACEHOLDER: always 0 so far */
	SceUInt64 value;
};

/* what a task says of its package (PLACEHOLDER names), copied under "SceBgdlRMutexPackage" */
struct BgdlTaskPackage {
	BgdlTaskPackage() : reserved2(0), reserved3(0)
	{
		key220.clear();
	}

	BgdlTaskPackage &operator=(const BgdlTaskPackage &src)
	{
		key220 = !src.key220.empty() ? src.key220.c_str() : NULL;
		key217 = !src.key217.empty() ? src.key217.c_str() : NULL;
		reserved2 = src.reserved2;
		reserved3 = src.reserved3;
		return *this;
	}

	paf::string key220;		/* the title id (13 characters kept) */
	paf::string key217;		/* the content id */
	int reserved2;			/* PLACEHOLDER: the server's category, not read yet */
	int reserved3;			/* and its attributes */
};

class EseTask {
public:
	EseTask(SceUInt32 taskId)
		: id(taskId), result(BGDL_ERROR_NOT_LOADED), value(0), transfer(), files(),
		  m_removing(false), copyResult(0)
	{
	}

	int setRunStatus(int status);					/* (0x810FD94C) */
	int setState(int state, int *current, bool force);		/* (0x810FDA08) */
	int GetFolder(BgdlTaskPath *path, bool fromParam);		/* (0x810FDAEC) */
	int getFilePath(BgdlTaskPath *path, bool fromParam);		/* (0x810FDD12) */
	int copyMtp(int *runStatus);					/* (0x810FE06C) PLACEHOLDER name */
	int invoke(void);						/* (0x810FE448) */
	int Load(void);			/* (0x810FF6F4) PLACEHOLDER name: the task read from its param file */
	int setResult(int status, int taskResult);	/* (0x81103722) PLACEHOLDER name */
	int execute(int *runStatus);					/* (0x8111FEBC) esehttp.cpp */
	/* PLACEHOLDER names: Sony's calls these out of line (0x814154EC on) */
	__attribute__((noinline)) void setKey107(int value)
	{
		m_ParamCache.key107 = value;
	}

	__attribute__((noinline)) void setKey102(bool value)
	{
		m_ParamCache.key102 = value;
	}

	__attribute__((noinline)) void setKey113(bool value)
	{
		m_ParamCache.key113 = value;
	}

	__attribute__((noinline)) void setKey110(bool value)
	{
		m_ParamCache.key110 = value;
	}

	/* (0x8141550C) the transfer started over, shown when report is set */
	__attribute__((noinline)) void setProgress(bool report, SceUInt64 total, SceUInt64 current);
	/* (0x81415ECC) the transfer at current, the time left worked out again, shown when report
	   is set (PLACEHOLDER name; esehttp.hpp) */
	__attribute__((noinline)) void updateProgress(bool report, SceUInt64 current);
	/* updateProgress's body, which Sony's compiler also places in its callers (PLACEHOLDER
	   name; esehttp.hpp) */
	void doUpdateProgress(bool report, SceUInt64 current);
	/* (0x8141559C) the package copied in under "SceBgdlRMutexPackage" */
	__attribute__((noinline)) void setPackage(const struct BgdlTaskPackage *src);
	/* setPackage's body, which Sony's compiler also places in its callers (PLACEHOLDER name) */
	void doSetPackage(const struct BgdlTaskPackage *src);

	SceUInt32 id;			/* the task's number, its folder's name */
	int result;			/* key 104; 0x80103A1A until it is read */
	SceUInt64 value;		/* PLACEHOLDER: passed in the task's events */
	BgdlProgress transfer;		/* keys 206 and 208 */
	BgdlProgress files;		/* keys 505 and 5002 on */
	bool m_removing;		/* PLACEHOLDER name: RemoveTask is removing it */
	EseParamCache m_ParamCache;
	BgdlTaskPackage package;	/* guarded by "SceBgdlRMutexPackage" */
	BgdlTaskStatus status;
	int copyResult;			/* from libmtp's progress */
	BgdlTaskCopy copy;
};

/* a function told of the manager's events */
struct BgdlListener {
	void (*func)(BgdlEvent *event, void *arg);
	void *arg;
};

#define BGDL_LISTENER_MAX	16

/*
 * The manager: one, made by bgdlManagerCreate (0x811041EC; constructor 0x8110401E), its
 * thread "SceBgdlManagerThread" running the tasks (0.990's startMng). The mutexes' names are
 * Sony's ("SceBgdlMutexEseTaskList", ...); the class's and the fields' names are ours
 * (PLACEHOLDER).
 */
#define BGDL_QUEUE_MAX		32

/* a task as the manager last listed it (PLACEHOLDER names) */
struct BgdlTaskSummary {
	bool ready;			/* done: no error, not to run again, key 113 set */
	EseParamCache param;
	char titleId[13];
	char padding0;
	char contentId[51];
	char padding1[3];
};

/* the manager's list of its tasks, cleared before anything else is made (PLACEHOLDER name) */
struct BgdlTaskSummaryList {
	BgdlTaskSummaryList()
	{
		sce_paf_memset(entries, 0, sizeof(entries));
	}

	BgdlTaskSummary entries[BGDL_QUEUE_MAX];
};

/* an entry of appmgr's background download queue (vshAppMgrBgdlSetQueueStatus) */
struct BgdlQueueItem {
	SceUInt32 type;			/* 1, 3: an application; 2: additional content; 4 */
	SceUInt32 state;		/* 1: ready */
	char contentId[48];
};

/*
 * A package download's request: a list file of its package files, the title it is for
 * (AddPackageTask, 0x81100E8C). PLACEHOLDER names; the offsets are the original's.
 */
#define BGDL_REQUEST_FLAG_KEY116	0x08
#define BGDL_REQUEST_FLAG_KEY115	0x10

struct BgdlPackageRequest {
	SceUInt32 type;			/* key 111: 0, 1: ux0:; 2: ur0:; 3 */
	SceUInt32 flags;		/* BGDL_REQUEST_FLAG_* */
	int kind;			/* 1: an application (key 114 set) */
	SceUInt32 fileCount;		/* key 500: the files the list must hold */
	SceUInt16 key503;
	SceUInt16 key504;
	char titleId[14];		/* key 220 */
	char contentId[52];		/* key 217 */
	char name[257];			/* key 105 */
	char iconPath[256];		/* key 106: the icon, copied into the task's folder */
	char listPath[256];		/* the list file, removed when done */
	char key235[41];
};

/* a download task's request (PLACEHOLDER names; the keys are the param file's) */
struct BgdlDownloadRequest {
	SceUInt32 type;			/* key 111 */
	SceUInt32 flags;		/* BGDL_REQUEST_FLAG_*; 0x02: not started at once */
	int installMode;		/* PLACEHOLDER: from 0 up, grw0: needs QAF's fake AC install */
	char reserved1[20];
	SceUInt64 key208;
	SceUInt64 key206;
	char reserved2[40];
	char gameTitleId[24];		/* PLACEHOLDER: the game the content is for */
	char url[2048];			/* key 202 */
	char iconPath[256];		/* key 106: the icon, copied into the task's folder */
	char name[257];			/* key 105 */
	char key203[193];		/* ur0:'s tasks only */
	char key229[256];
	char titleId[16];
	char contentId[52];		/* key 217 */
	char key222[52];
	char key230[128];		/* a file copied into the task's folder */
	char key221[1];			/* PLACEHOLDER: its length is not known yet */
	char reserved3[63];
	char key234[62];
	SceUInt32 version;		/* PLACEHOLDER: from 0x01800000 to 0x01FFFFFF with platform 2 */
	int platform;			/* PLACEHOLDER: the content's package keeps its id when 2 */
	char reserved5[16];
};

/* the request's flags (PLACEHOLDER names) */
#define BGDL_REQUEST_FLAG_SERVER4	0x04	/* passed to the server request */
#define BGDL_REQUEST_FLAG_KEY239	0x20	/* key 239 set, and passed to the server request */
#define BGDL_REQUEST_FLAG_STOP_ALL	0x40	/* the waiting tasks given up first */

#define BGDL_REQUEST_FLAG_NO_START	0x02

/* a download of a content the server described, as AddContentTask takes it (PLACEHOLDER names) */
struct BgdlContentRequest {
	SceUInt32 device;		/* 0: ux0:, 2: ur0: */
	SceUInt32 flags;		/* BGDL_REQUEST_FLAG_* */
	char reserved[20];
	char url[2048];			/* key 202 */
	char iconPath[256];		/* key 106: the icon, copied into the task's folder */
	char name[257];			/* key 105 */
};

/* the account a content download signs in with (PLACEHOLDER names) */
struct BgdlContentAccount {
	char reserved;
	char user[32];			/* key 200, stored scrambled */
	char password[32];		/* key 201, stored scrambled */
	char key209[64];		/* PLACEHOLDER: its length is not known yet */
};

/* what AddPackageTask gives back (PLACEHOLDER names) */
struct BgdlPackageResult {
	SceUInt32 id;			/* the task made */
	int reserved;
	SceUInt64 needed;		/* the space the files need, when the device is too full */
};

/* an entry of a package list file (PLACEHOLDER names) */
struct BgdlPackageEntry {
	SceUInt64 reserved;
	SceUInt64 size;			/* key 5002 + 32 * n */
	char digest[20];		/* key 5001 + 32 * n */
	char name[260];			/* key 5000 + 32 * n */
};

/* a device's space (Bgdl_8111234C; constructor 0x8141523C) PLACEHOLDER names */
struct BgdlStorageInfo {
	BgdlStorageInfo() : max(0), free(0), clusterSize(0), reserved2(0)
	{
	}

	SceUInt64 max;			/* the device's size */
	SceUInt64 free;
	SceUInt32 clusterSize;
	int reserved2;
};

/* a file closed when it goes (constructor 0x814152A0, destructor 0x814152C8) PLACEHOLDER */
class BgdlFileCloser {
public:
	BgdlFileCloser() : m_result(0)
	{
	}

	BgdlFileCloser(paf::common::SharedPtr<paf::File> file) : m_file(file), m_result(0)
	{
	}

	/* (0x81415DAC) the file to close */
	__attribute__((noinline)) void Set(paf::common::SharedPtr<paf::File> file)
	{
		m_file = file;
	}

	~BgdlFileCloser()
	{
		if (m_file.get() != NULL)
			m_result = m_file->Close();
	}

	paf::common::SharedPtr<paf::File> m_file;
	int m_result;
};

/* the id of a task's work folder, for a request's type (0x81415250) PLACEHOLDER name */
inline SceUInt32 bgdlWorkId(SceUInt32 type, bool flag)
{
	if (type <= 1)
		return (SceUInt32)-1;
	if (type == 2)
		return (SceUInt32)-2;
	if (type == 3) {
		if (flag == true)
			return (SceUInt32)-1;
		return (SceUInt32)-2;
	}
	return (SceUInt32)-1;
}

bool bgdlKindIsGame(int kind);		/* (0x81415650) esetask.cpp */

class BgdlManagerThread;

/* a task as GetTaskList lists it (PLACEHOLDER names) */
struct BgdlTaskEntry {
	SceUInt32 id;
	EseParamCache param;
	int result;
};
struct BgdlServerQuery;
struct LsdbValue;		/* livespace_db_internal.h */
struct BgdlServerIds;
struct BgdlContentInfo;

/* what an MTP copy copies (PLACEHOLDER names) */
struct BgdlMtpContent {
	SceUInt32 options;		/* BGDL_MTP_OPTION_* */
	int kind;			/* 0 to 3: key 107 1, 2, 3 or 6 */
	SceUInt64 size;			/* mtp_contents.bin's */
};
#define BGDL_MTP_OPTION_CONTENTS	0x01	/* mtp_contents.bin made (key 112) */

/* an MTP copy asked for (PLACEHOLDER names) */
struct BgdlMtpRequest {
	SceUInt32 type;			/* key 111: 0, 1: ux0:; 2: ur0: */
	SceUInt32 flags;		/* BGDL_REQUEST_FLAG_* */
	BgdlMtpContent content;
	char name[257];			/* key 105 */
	char iconPath[256];		/* key 106: the icon, copied into the task's folder */
};

/* a content a task is looked for by (PLACEHOLDER names) */
struct BgdlContentQuery {
	int kind;			/* key 111; 7 asks for any (0x8110F50C) */
	char contentId[60];
};

class BgdlManager {
public:
	BgdlManager();							/* (0x8110401E) */
	~BgdlManager();							/* (0x81104374) */
	int Stop(void);			/* (0x811043F4) PLACEHOLDER name: the thread ended */
	/* (0x811047E4) PLACEHOLDER name: a content asked of the server */
	int Bgdl_811047E4(const BgdlServerQuery *query, const BgdlServerIds *ids, BgdlContentInfo *info,
			  const bool *cancel);
	int prepare_device(const char *device, bool clean, int quota);	/* (0x81103834) probable Sony name */
	void Bgdl_811033C4(void);
	void Bgdl_81103224(bool clean);
	__attribute__((noinline)) void UpdateFlags(SceUInt32 value, SceUInt32 mask);	/* (0x810FD758) */
	bool IsCancelled(void);						/* (0x810FD8EC) */
	void NotifyListeners(BgdlEvent *event);				/* (0x810FD90C) */
	__attribute__((noinline)) EseTask *FindTask(SceUInt32 id);	/* (0x810FDC1C) */
	/* (0x810FE038) PLACEHOLDER name: false when the running task is already cancelled */
	bool SetCancelHandler(void (*handler)(void *arg), void *arg);
	void UpdateQueueStatus(void);					/* (0x810FE5A4) PLACEHOLDER name */
	int deleteTask(SceUInt32 id, const EseParamCache *param);	/* (0x810FE8F0) probable */
	int finishTask(EseTask *task, int result);			/* (0x810FED12) PLACEHOLDER name */
	bool IsSuspended(void);						/* (0x810FF102) PLACEHOLDER name */
	bool IsHeld(void);						/* (0x811104A0) PLACEHOLDER name */
	bool NextTask(EseTask **next);					/* (0x810FF108) PLACEHOLDER name */
	int RemoveTask(SceUInt32 id);					/* (0x810FF37C) PLACEHOLDER name */
	void SetStatus(const BgdlTaskStatus *status);			/* (0x810FF6CC) PLACEHOLDER name */
	int ReloadTask(SceUInt32 id);					/* (0x810FFF22) PLACEHOLDER name */
	/* (0x811000D4) PLACEHOLDER name: a task made from its first keys; *id its new id */
	int AddTask(SceUInt32 *id, int type, SceUInt32 key111, const char *key105, bool key115, bool key116);
	/* (0x8110088A) PLACEHOLDER name: the title's storage is there; *grw0 when it is grw0: */
	int CheckStorage(const char *titleId, bool *grw0);
	/* (0x81100E8C) PLACEHOLDER name: a package download made a task */
	int AddPackageTask(const BgdlPackageRequest *request, BgdlPackageResult *result);
	/* (0x811049D4) PLACEHOLDER name: a download made a task; *id its id */
	int AddDownloadTask(SceUInt32 device, const BgdlDownloadRequest *request, SceUInt32 *id);
	/* (0x8110532E) PLACEHOLDER name: a content the server described made a task; *id its id */
	int AddContentTask(const BgdlContentRequest *request, const BgdlContentAccount *account,
			   const BgdlContentInfo *info, SceUInt32 *id);
	int Bgdl_81106390(SceUInt32 *id);	/* (0x81106390) PLACEHOLDER name */
	/* (0x8110671E) PLACEHOLDER name: the PSM runtime's download task made, or its patch looked
	 * for when it is installed; *iconPath its icon */
	int AddPsmRuntimeTask(paf::string *iconPath, int reserved0, int reserved1, SceUInt64 *needed);
	/* (0x81107BDE) PLACEHOLDER name: a download the server describes made a task; *id its id */
	int Bgdl_81107BDE(const BgdlDownloadRequest *request, BgdlContentInfo *info, SceUInt32 *id);
	/* (0x81109CAC) PLACEHOLDER name: a list's package files added to the package task id */
	int AddPackageFiles(SceUInt32 id, const BgdlPackageRequest *request);
	int GetTaskStatus(SceUInt32 id, BgdlTaskStatus *status);	/* (0x8110AA8E) PLACEHOLDER name */
	int GetTaskCopy(SceUInt32 id, BgdlTaskCopyStatus *copy);	/* (0x8110AB42) PLACEHOLDER name */
	int GetTaskParam(SceUInt32 id, EseParamCache *param);		/* (0x8110AC20) PLACEHOLDER name */
	int CancelTask(SceUInt32 id);					/* (0x8110ADB0) PLACEHOLDER name */
	int ReloadTasks(void);						/* (0x8110AEE6) PLACEHOLDER name */
	int RemoveTasks(bool doneOnly);					/* (0x8110B068) PLACEHOLDER name */
	/* (0x8110B202) PLACEHOLDER name: the title's waiting tasks of a kind */
	int FindTitleTasks(paf::vector<BgdlTaskEntry> *list, const paf::string *titleId, int kind);
	/* (0x8110B8BC) PLACEHOLDER name: the listed tasks of a title counted */
	int CountTitleSummaries(const paf::common::SharedPtr<paf::map<unsigned int, LsdbValue> > *record,
				const char *titleId, int kind, SceUInt32 flags,
				paf::vector<BgdlTaskEntry> *list, int *count);
	/* (0x8110BB9C) PLACEHOLDER name: the listed tasks of a record's title counted */
	int CountRecordTasks(const paf::common::SharedPtr<paf::map<unsigned int, LsdbValue> > *record,
			     SceUInt32 flags, paf::vector<BgdlTaskEntry> *list, int *count);
	/* (0x8110C0C0) PLACEHOLDER name: the tasks of a type (-1: all) listed */
	int ListTasks(paf::vector<BgdlTaskEntry> *list, int type);
	int RemoveGameDataTasks(const char *titleId);	/* (0x8110C1EC) PLACEHOLDER name */
	void FinishInstall(SceUInt32 id, EseTask *task, int *result, class BgdlTaskMark *mark);	/* (0x8110D34E) */
	int InstallTask(struct BgdlTaskInfo *info);	/* (0x8110D722) PLACEHOLDER name */
	/* (0x8110D7E2) PLACEHOLDER name: the title's waiting tasks described */
	int GetTitleTaskInfos(const char *titleId, paf::vector<struct BgdlTaskInfo> *infos, SceUInt32 flags,
			      int type);
	/* (0x8110E19C) PLACEHOLDER name: the title's waiting tasks installed */
	int InstallTitleTasks(const char *titleId, int type, struct BgdlTaskInfo *failed, bool *patched);
	/* (0x8110E99A) PLACEHOLDER name: the waiting content tasks described */
	int GetContentTaskInfos(paf::vector<struct BgdlTaskInfo> *infos);
	/* (0x8110E4B8) PLACEHOLDER name: the same for the kinds flags asks for */
	int InstallTitleTasks(const char *titleId, int type, SceUInt32 flags, bool dialog,
			      struct BgdlTaskInfo *failed, bool *patched);
	int GetTaskInfo(SceUInt32 id, struct BgdlTaskInfo *info);	/* (0x8110EDBA) PLACEHOLDER name */
	int ResetInstalledTask(SceUInt32 id);	/* (0x8110F064) PLACEHOLDER name */
	void SetHold(bool hold);		/* (0x8110F1D8) PLACEHOLDER name */
	/* (0x8110F238) PLACEHOLDER name: the PSM runtime's waiting patch's version */
	bool GetPsmRuntimeVersion(SceUInt16 *version);
	int HasContentTask(const BgdlContentQuery *query);	/* (0x8110F3FA) PLACEHOLDER name */
	int CountContentTasks(const BgdlContentQuery *query);	/* (0x8110F50C) PLACEHOLDER name */
	int GetPatchVersion(const char *titleId, SceUInt8 *version);	/* (0x8110F94A) PLACEHOLDER name */
	int CheckMtpContent(const BgdlMtpContent *content);	/* (0x8110FB76) PLACEHOLDER name */
	int AddMtpTask(const BgdlMtpRequest *request, SceUInt32 *id);	/* (0x8110FC62) PLACEHOLDER name */
	/* (0x811103EA, 0x81110448) PLACEHOLDER names */
	int AddListener(void (*func)(BgdlEvent *event, void *arg), void *arg);
	int RemoveListener(void (*func)(BgdlEvent *event, void *arg), void *arg);
	int CheckTask(SceUInt32 id);		/* (0x811104A8) PLACEHOLDER name */
	int RemoveTitleTasks(const char *titleId);	/* (0x8110C8B4) PLACEHOLDER name */
	int RemoveInstalledTitleTasks(const char *titleId);	/* (0x8110CB90) PLACEHOLDER name */
	int CancelWaitingTask(SceUInt32 id);	/* (0x811078DC) PLACEHOLDER name */
	/* (0x81107986) PLACEHOLDER name: the tasks of a type (-1: all), *count of them */
	int GetTaskList(BgdlTaskEntry *list, int *count, int type);
	int CancelWaitingTasks(void);		/* (0x81107A9E) PLACEHOLDER name */
	/* (0x81107512) PLACEHOLDER name: the storage a game's content goes to; *grw0 when grw0: */
	int CheckContentStorage(const BgdlDownloadRequest *request, bool *grw0);

	BgdlManagerThread *m_thread;	/* "SceBgdlManagerThread", made by startMng */
	BgdlTaskList m_tasks;
	BgdlTaskSummaryList m_summaries;
	paf::thread::Mutex m_taskListMutex;	/* "SceBgdlMutexEseTaskList" */
	paf::thread::Cond m_taskListCond;	/* "SceBgdlCondEseTaskList" */
	paf::thread::RMutex m_cancelMutex;	/* "SceBgdlMutexEseCancel" */
	paf::thread::Mutex m_listenersMutex;	/* "SceBgdlMutexEseListeners" */
	paf::thread::RMutex m_syncHandlerMutex;	/* "SceBgdlRMutexEseSyncHandler" */
	paf::thread::RMutex m_progressMutex;	/* "SceBgdlRMutexProgerss" */
	int m_error;			/* 0x80103A15 until it is started */
	bool m_stopping;		/* PLACEHOLDER name: the thread is to end */
	bool m_removeRunning;		/* PLACEHOLDER name: the running task is to be removed */
	bool m_cancelled;		/* the running task was told to stop */
	void (*m_cancelHandler)(void *arg);	/* stops the running task */
	void *m_cancelHandlerArg;
	EseTask *m_runningTask;		/* PLACEHOLDER name: the task running, or NULL */
	SceUInt32 m_lastId;		/* PLACEHOLDER name: the last task run (one less when cancelled) */
	SceUInt32 m_nextId;		/* ux0:'s next task id */
	SceUInt32 m_nextInternalId;	/* ur0:'s, from 0x80000000 */
	SceUInt32 m_flags;		/* what the tasks may do (UpdateFlags) */
	bool m_ux0Ready;		/* ux0: is there */
	bool m_hold;			/* PLACEHOLDER name: set by 0x8110F1D8 and 0x8110F1DE; a download stops while it is set */
	bool m_resumed;			/* PLACEHOLDER name: the system resumed (SHELL_EVENT_POWER) */
	bool m_suspended;		/* PLACEHOLDER name: no task is started while it is set */
	BgdlTaskStatus m_status;	/* the running task's (60 bytes copied) */
	int m_copyResult;
	BgdlTaskCopy m_copy;
	bool m_hasPending;		/* PLACEHOLDER name: a task of kind 5 or key 111 = 2 is listed */
	BgdlListener m_listeners[BGDL_LISTENER_MAX];
	int m_listenerCount;
	char reserved9[0x40];		/* PLACEHOLDER: cleared by the constructor, not read yet */
	char reserved10[0x40];
	SceUID m_drmHeap;		/* "SceNpDrmInstallHeap", or -1 */
	BgdlQueueItem m_queue[BGDL_QUEUE_MAX];
	char reserved13[0x400];		/* PLACEHOLDER: not even cleared so far */
};

/* UpdateFlags run by the job queue (0.990's name; made in 0x81104574) */
class Job_EseSetPermission : public paf::job::JobItem {
public:
	Job_EseSetPermission(SceUInt32 value, SceUInt32 mask)
		: paf::job::JobItem("Job_EseSetPermission", NULL), m_value(value), m_mask(mask)
	{
	}

	virtual ~Job_EseSetPermission()
	{
	}

	virtual int Run();					/* (0x810FD8A4) */

	virtual void Finish(int result)
	{
	}

	SceUInt32 m_value;
	SceUInt32 m_mask;
};

/* the next file's (0x8113AF58): a content's auto download (PLACEHOLDER names) */
class BgdlAutoDownload {
public:
	BgdlAutoDownload(const char *contentId);
	~BgdlAutoDownload();
	int Bgdl_8113B164(void);

	char reserved[104];		/* PLACEHOLDER: the next file's */
};

/* a content's auto download turned off ("Job_EseUnsetAutoDownload") */
class Job_EseUnsetAutoDownload : public paf::job::JobItem {
public:
	Job_EseUnsetAutoDownload(const char *contentId)
		: paf::job::JobItem("Job_EseUnsetAutoDownload", NULL), m_contentId(contentId)
	{
	}

	virtual ~Job_EseUnsetAutoDownload()
	{
	}

	virtual int Run()
	{
		BgdlAutoDownload download(m_contentId.c_str());

		return download.Bgdl_8113B164();
	}

	virtual void Finish(int result)
	{
	}

	paf::string m_contentId;
};

/* (0x81114524, eseutil.cpp) 1: the task is a game's, of key 107 6 and kind 2 or 3, and the game
 * being run is its title */
bool Bgdl_81114524(SceUInt32 id);

/* a finished game download's event ("Job_EseGDEvent"); the shell is told when it is not handled */
class Job_EseGDEvent : public paf::job::JobItem {
public:
	Job_EseGDEvent(SceUInt32 id)
		: paf::job::JobItem("Job_EseGDEvent", NULL), m_id(id), m_handled(false)
	{
	}

	virtual ~Job_EseGDEvent()
	{
	}

	virtual int Run()
	{
		m_handled = Bgdl_81114524(m_id);
		return 0;
	}

	virtual void Finish(int result)
	{
		if (m_handled == false)
			shellEventServerSend(1, NULL, 0);
	}

	SceUInt32 m_id;
	bool m_handled;
};

/* the manager's thread, "SceBgdlManagerThread" (PLACEHOLDER class name) */
class BgdlManagerThread : public paf::thread::Thread {
public:
	BgdlManagerThread()
		: paf::thread::Thread(248, 0x5000, "SceBgdlManagerThread", NULL), m_manager(NULL)
	{
	}

	virtual void EntryFunction();					/* (0x810FF214) */

	BgdlManager *m_manager;
};

/* a patch registered by title id ("Job_EseRegistPatchByTitleID") */
class Job_EseRegistPatchByTitleID : public paf::job::JobItem {
public:
	Job_EseRegistPatchByTitleID(const BgdlTaskPackage &package, const bool *flag);	/* (0x810FF2AE) */

	virtual ~Job_EseRegistPatchByTitleID()
	{
	}

	virtual int Run();						/* (0x8110313E) */

	virtual void Finish(int result)
	{
	}

	BgdlTaskPackage m_package;
	bool m_flag;			/* PLACEHOLDER name */
};

BgdlManager *bgdlManagerGet(void);				/* (0x810FD8E0) */
paf::string bgdlTaskFolder(SceUInt32 id, bool fromParam);	/* (0x810FDC72) */
void bgdlCountFinished(const EseTask *task);			/* (0x810FEA80) PLACEHOLDER name */
void bgdlUnsetAutoDownload(const EseTask *task);		/* (0x810FEB22) PLACEHOLDER name */
void bgdlManagerCreate(bool clean, const int *quota);		/* (0x811041EC) */
void startMng(void);						/* (0x811042C0) 0.990's name */
bool bgdlRemoveFolders(void);					/* (0x81104346) PLACEHOLDER name */
void bgdlManagerDestroy(void);					/* (0x81104474) PLACEHOLDER name */
void bgdlManagerWakeup(SceUInt64 *request);			/* (0x811044BC) PLACEHOLDER name */
void bgdlSetPermission(SceUInt32 value, SceUInt32 mask);		/* (0x81104574) PLACEHOLDER name */
int bgdlServerQuery(const BgdlServerQuery *query, const BgdlServerIds *ids, BgdlContentInfo *info,
		    const bool *cancel);					/* (0x811049A4) PLACEHOLDER name */
int bgdlAddDownloadTask(SceUInt32 device, const BgdlDownloadRequest *request,
			SceUInt32 *id);						/* (0x81105306) PLACEHOLDER name */
int bgdlAddContentTask(const BgdlContentRequest *request, const BgdlContentAccount *account,
		       const BgdlContentInfo *info, SceUInt32 *id);	/* (0x81106316) PLACEHOLDER name */
int bgdlCheckQueue(void);					/* (0x81106346) PLACEHOLDER name */
int bgdlManagerNotify(int value, int mask);			/* (0x810FD862) */
int bgdlGetTaskParam(SceUInt32 id, EseParamCache *param);	/* (0x8110AD48) PLACEHOLDER name */
int bgdlRemoveTask(SceUInt32 id);				/* (0x8110AD6C) PLACEHOLDER name */
int bgdlReloadTask(SceUInt32 id);				/* (0x8110AD8E) PLACEHOLDER name */
int bgdlCountTitleTasks(const char *titleId, SceUInt32 *count);	/* (0x8110B7E2) PLACEHOLDER name */
int bgdlGetTaskTitle(SceUInt32 id, paf::string *titleId);	/* (0x8110E716) PLACEHOLDER name */
/* (0x8110BD6E, 0x8110BDC0, 0x8110BEC2) PLACEHOLDER names */
bool bgdlRecordHasTasks(const paf::common::SharedPtr<paf::map<unsigned int, LsdbValue> > *record,
			SceUInt32 flags);
bool bgdlTitleHasTasks(const char *titleId, SceUInt32 flags);
bool bgdlTitleHasTasksOfKind(const char *titleId, int type, SceUInt32 flags);
int bgdlCountAppTasks(const char *titleId, SceUInt32 *count);	/* (0x8110BFE4) PLACEHOLDER name */
int bgdlDeletePrepare(const char *titleId);			/* (0x8110CD86) PLACEHOLDER name */
/* (0x8110E400 to 0x8110E48C, 0x8110E6DE) PLACEHOLDER names */
int bgdlInstallTitleTasks(const char *titleId, struct BgdlTaskInfo *failed, bool *patched);
int bgdlInstallTitleTasksType1(const char *titleId, struct BgdlTaskInfo *failed);
int bgdlInstallTitleTasksType2(const char *titleId, struct BgdlTaskInfo *failed);
int bgdlInstallTitleTasksType3(const char *titleId, int type, SceUInt32 flags,
			       struct BgdlTaskInfo *failed, bool *patched);
int bgdlInstallTitleTasksOfKinds(const char *titleId, int type, SceUInt32 flags, bool dialog,
				 struct BgdlTaskInfo *failed, bool *patched);
int bgdlCountTasks(SceUInt32 *count, paf::vector<BgdlTaskEntry> *list);	/* (0x8110E884) */
int bgdlGetContentTaskInfos(paf::vector<struct BgdlTaskInfo> *infos);	/* (0x8110ED40) */
int bgdlInstallTask(struct BgdlTaskInfo *info);		/* (0x8110ED62) PLACEHOLDER name */
int bgdlGetTitleTaskInfos(const char *titleId, paf::vector<struct BgdlTaskInfo> *infos,
			  SceUInt32 flags, int type);			/* (0x8110ED8A) */
int bgdlGetTaskInfo(SceUInt32 id, struct BgdlTaskInfo *info);	/* (0x8110F018) PLACEHOLDER name */
int bgdlInstallTask2(struct BgdlTaskInfo *info);		/* (0x8110F03C) PLACEHOLDER name */
void bgdlHold(void);						/* (0x8110F1DE) PLACEHOLDER name */
void bgdlSuspend(bool reserved);				/* (0x8110F1F4) PLACEHOLDER name */
/* (0x8110F208) PLACEHOLDER name */
int bgdlAddPsmRuntimeTask(paf::string *iconPath, int arg1, SceUInt64 *needed);
bool bgdlGetPsmRuntimeVersion(SceUInt16 *version);		/* (0x8110F3A2) PLACEHOLDER name */
bool bgdlHasPending(void);					/* (0x8110F718) PLACEHOLDER name */
int bgdlGetPatchVersion(const char *titleId, SceUInt8 *version);	/* (0x8110FB04) PLACEHOLDER name */
int bgdlListMtpTasks(void);					/* (0x8110FB28) PLACEHOLDER name */
int bgdlCheckMtpContent(const BgdlMtpContent *content, int reserved);	/* (0x8110FC3E) */
int bgdlAddMtpTask(const BgdlMtpRequest *request, SceUInt32 *id);	/* (0x811103CA) */

/* (0x81563150) "SceBgdlRMutexPackage", guarding each task's package (esetask.cpp) */
extern paf::thread::RMutex s_packageMutex;

inline void EseTask::doSetPackage(const BgdlTaskPackage *src)
{
	s_packageMutex.Lock();
	package = *src;
	s_packageMutex.Unlock();
}

/* the dispatcher's state, as Bgdl_8113A47C copies it (PLACEHOLDER) */
struct BgdlDispatcherState {
	int state;			/* 0: idle */
	int reserved[3];
};

/* (0x8113A434..) the job dispatcher, made by download_plugin (PLACEHOLDER names) */
class BgdlJobDispatcher {
public:
	__attribute__((noinline)) BgdlJobDispatcher() : m_queue(NULL), m_reserved(0)
	{
	}

	~BgdlJobDispatcher();						/* (0x8113A674) */
	static BgdlJobDispatcher *Bgdl_8113A434(void);
	void Bgdl_8113A47C(BgdlDispatcherState *state, bool second);
	int Bgdl_8113A4FC(paf::common::SharedPtr<paf::job::JobItem> &job);
	void Bgdl_8113A644(void);					/* stops it */
	void Bgdl_8113A6A8(void);					/* starts it */

	void *m_queue;
	int m_reserved;
};

/* log_job_manager.cpp's (0x81123E48): the log job manager's job queue (PLACEHOLDER names) */
class BgdlJobQueue {
public:
	BgdlJobQueue();						/* "SceLogJobManagerJob" made */
	~BgdlJobQueue();

	static BgdlJobQueue *Bgdl_81123E48(void);
	int Bgdl_81123E3C(paf::common::SharedPtr<paf::job::JobItem> job);

	paf::job::JobQueue *m_queue;
};

int bgdlCancelTask(SceUInt32 id);				/* (0x8110AEC4) */

#endif
