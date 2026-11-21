/*
 * esetask.cpp: the background download library's tasks and their manager (0x810FCD10 to the
 * file's static initialiser at 0x81110620). Sony's 0.990 prototype names the file
 * (vsh/app/shell/src/esecron/src/esetask.cpp) and some functions; the other names are ours
 * (PLACEHOLDER).
 */
#include <stddef.h>
#include <stdarg.h>
#include <string.h>
#include <scetypes.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/ctype.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <dir/dir.h>
#include <file/localfile.h>
#include <timer/timer.h>
#include <job/job.h>
#include <common/shared_ptr.h>
#include <common/char_encoding.h>
#include <plugin.h>
#include <versatileparam/idparam.h>
#include <vshbridge/vshbridge.h>

#include "../include/esebase.hpp"
#include "../include/esetask.hpp"
#include "../include/eseparam.hpp"

#include "shell.h"
#include "console_info/console_info.h"
#include "src/livespace_db/lsdb_keys.h"
#include "src/livespace_db/livespace_db_internal.h"
#include "bgdl_api.h"
#include <graphics/surface/surface.h>
#include <modulemgr/module_info.h>
#include <kernel/sysmem/memblock.h>
#include "../../../../game/game_plugin/game_plugin.h"

PAF_VECTOR_INLINE_DTOR(BgdlTaskEntry)
PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbValue, paf::allocator<LsdbValue>)
PAF_VECTOR_NOINLINE_PUSH_BACK(BgdlTaskInfo, paf::allocator<BgdlTaskInfo>)
PAF_VECTOR_NOINLINE_CLEAR(BgdlTaskInfo, paf::allocator<BgdlTaskInfo>)

using namespace paf;

/* the task's param file (eseparam.cpp; PLACEHOLDER names: not written yet) */
int Bgdl_8112260C(SceUInt32 id, int key, void *value, SceSize size);		/* read */
int Bgdl_811233A4(SceUInt32 id, int key, const void *value, SceSize size);	/* write */
int Bgdl_8112278A(SceUInt32 id, int key, string *value);			/* read a string */
int Bgdl_81122700(SceUInt32 id, int key, BgdlTaskPath *value);		/* read a string into a path */

int bgdlMtpRunCopy(const MtpBgCopyRunParam *param);	/* bgdl_mtp.cpp */
int bgdlMtpCancelCopy(SceUInt32 id);			/* bgdl_mtp.cpp */
int bgdlMtpGetCopyProgress(SceUInt32 id, MtpBgCopyProgress *progress);	/* bgdl_mtp.cpp */
int Bgdl_811120A0(BgdlTaskPath *path, bool recursive);	/* removes a folder (PLACEHOLDER) */
int Bgdl_81112112(string *path, bool recursive);
int Bgdl_81123BC0(const char *titleId, LsdbAppInfoPtr *info);	/* PLACEHOLDER: the title's row */
/* PLACEHOLDER: the titles a query finds (AppInfoQClient) */
int Bgdl_811123BA(LsdbQueryCondition *conditions, paf::vector<string> *titleIds);
/* PLACEHOLDER: the title's tasks gone, told (0x8112803E(2, titleId, 0)) */
int Bgdl_8112816E(const char *titleId);
int Bgdl_811219B8(EseTask *task);	/* PLACEHOLDER: the task's content installed */
string Bgdl_81113DE6(const char *titleId);	/* PLACEHOLDER: the title's icon (livespace_db) */
bool Bgdl_811144CE(const string *titleId);	/* PLACEHOLDER: the game being run is the title */
/* game_plugin's promote dialog, opened (GAME_PLUGIN_FUNC_DIALOG_OPEN) */
/* game_plugin's interface function 62: the version in a patch's package file (PLACEHOLDER) */
#define BGDL_GAME_PLUGIN_FUNC_PATCH_VERSION	62
typedef void (*BgdlPatchVersionFunc)(const string *file, SceUInt8 *major, SceUInt8 *minor,
				     SceUInt8 *patch);
GamePluginInterface *Bgdl_811115BA(void);		/* the next file's: game_plugin's interface */
string Bgdl_811115D0(string dir, int number);		/* the next file's: dir + "/" + "%04d" */
typedef int (*BgdlDialogOpenFunc)(const string &title, intrusive_ptr<graph::Surface> icon,
				  const wstring &text, bool frame, const float *progress, bool second);
int Bgdl_81113898(const LsdbAppInfoPtr *info, int *kind);	/* PLACEHOLDER: its storage kind */
/* PLACEHOLDER: whether the record names the title (the game plugin's, next to
 * gameAppRecordNamesTitle) */
int Game_81372130(const char *titleId, const LsdbAppInfoPtr *record);
bool Bgdl_81113900(const LsdbAppInfoPtr *info, string *titleId);	/* PLACEHOLDER: its title id */
int Bgdl_811138CC(const LsdbAppInfoPtr *info, int *kind);	/* PLACEHOLDER: game_plugin's first function */
int Bgdl_811121FE(const char *path, int mode);
int Bgdl_8111212C(const char *folder, const char *name);	/* PLACEHOLDER: folder/name removed */
SceUInt64 Bgdl_81112310(SceUInt64 size, SceUInt32 cluster);	/* size in whole clusters (PLACEHOLDER) */
int Bgdl_8111234C(const char *device, BgdlStorageInfo *info);	/* the device's space (PLACEHOLDER) */
int Bgdl_8111240E(const char *path, SceUInt64 size);		/* a file made that size (PLACEHOLDER) */
int Bgdl_81111724(const char *path, int flag, SceUInt64 reserved);	/* PLACEHOLDER: removes a file */
int Bgdl_81111CDC(const char *url, const char *folder, string *out, bool flag);	/* PLACEHOLDER */
int Bgdl_811221E0(const char *src, int max, string *out);	/* a secret stored scrambled (PLACEHOLDER) */
string Bgdl_81113C72(SceUInt32 id, bool flag);			/* PLACEHOLDER: a game's folder */
int Bgdl_811137F6(bool mount);					/* PLACEHOLDER: grw0: */
void Bgdl_81122168(bool clean);					/* PLACEHOLDER */
extern "C" int sceNpCommonGetNpEnviroment(char *environment);
void Bgdl_81113890(int value);					/* PLACEHOLDER */

/* the PSP emulator's system software version (sceCompatGetPspSystemSoftwareVersion) */
extern "C" int SceCompatForVsh_9C107CC9(void);
extern "C" int sceKernelGetSystemSwVersion(SceKernelSystemSwVersion *info);

/*
 * Another file's (0x812A99AE on): the game update check as the shell links it, its context,
 * its parameters and what it finds (PLACEHOLDER names).
 */
struct BgdlUpdateParam {
	void *(*malloc)(size_t size);
	void (*free)(void *ptr);
	int mode;			/* 0; 1: a background patch; 2: an internal one */
	char titleId[16];
	char appVer[8];
	char contentId[48];
	SceUInt32 systemVersion;
	SceUInt32 pspVersion;
	char key235[13];		/* set by the check */
	char padding[3];
	int option;			/* PLACEHOLDER name: 1 for bgdlCheckPatchSpace's check */
};

struct BgdlUpdateCore {
	void *work[3];
};

struct BgdlUpdateState {
	SceSize size;
	int state;			/* 2: a patch to get */
	char appVer[8];
};

struct BgdlUpdateInfo {
	char reserved[20];
	char name[384];
	SceUInt32 count;		/* the patch's files */
	SceUInt32 flags;		/* 0x80: not to get */
};

struct BgdlUpdateEntry {
	SceUInt64 size;
	char digest[20];
	char name[276];
};

int Bgdl_812A99AE(BgdlUpdateCore *core, int option, const BgdlUpdateParam *param);
int Bgdl_812A9BB0(BgdlUpdateCore *core, BgdlUpdateState *state, BgdlUpdateInfo *info);
int Bgdl_812A985C(BgdlUpdateCore *core, SceUInt32 index, BgdlUpdateEntry *entry);
int Bgdl_812A9708(BgdlUpdateCore *core);

/* a title a patch is looked for (PLACEHOLDER names) */
/* another file's (constructor 0x81114642): a package file's check (PLACEHOLDER) */
class BgdlPackageCheck {
public:
	BgdlPackageCheck();
	void Bgdl_81114AFC(int mode, int kind, const char *name, int arg3, int arg4, int arg5,
			   int arg6, int arg7);
	int Bgdl_8118FD0C(void);
	void Bgdl_81115C22(char *digest);

	int reserved0;
	int reserved1;
	string reserved2;
	string reserved3;
	string reserved4;
	int reserved5;
	int reserved6;
	char reserved7[32];
	string reserved8;
	bool reserved9;
	bool reserved10;
	string reserved11;
};		/* makes a folder (PLACEHOLDER) */

/* the next file's (0x8113A434): the object whose job queue runs the library's jobs */
/* the dispatcher's state, as Bgdl_8113A47C copies it (PLACEHOLDER) */
struct BgdlDispatcherState {
	int state;			/* 0: idle */
	int reserved[3];
};

class BgdlJobDispatcher {	/* PLACEHOLDER names */
public:
	static BgdlJobDispatcher *Bgdl_8113A434(void);
	void Bgdl_8113A47C(BgdlDispatcherState *state, bool second);
	int Bgdl_8113A4FC(common::SharedPtr<job::JobItem> &job);
};

int Bgdl_812A9954(const char *titleId, int kind);	/* PLACEHOLDER: another file's */

/* the next file's (0x8111076A): game_plugin held while ux0:pspemu is changed (PLACEHOLDER) */
class BgdlGamePluginLock {
public:
	BgdlGamePluginLock(bool lock);
	~BgdlGamePluginLock();
	void Lock(void);		/* (0x8111072E) */

	bool m_locked;
};
bool Bgdl_81112284(const char *path);			/* the path is there (LocalFile::Getstat) */

/* the next file's (0x81110808): the Marlin licence folder made while the manager starts */
class BgdlLicenseFolder {	/* PLACEHOLDER names */
public:
	void Bgdl_81110808(void);
	void Bgdl_8111080E(void);
	void Bgdl_8111081C(void);
	/* (0x8111102A) file copied in as ur0:/MLN_ROOT/LICENSE/<contentId>.MNL */
	int Bgdl_8111102A(const char *file, const char *contentId);
	/* (0x81111432) the content's licence read, and removed when remove is set */
	int Bgdl_81111432(const char *contentId, bool remove);

	bool m_active;
};

/* PLACEHOLDER name: the copy did not complete (libmtp's errors other than its 0x80546600 group) */
#define BGDL_ERROR_COPY_FAILED		0x80103A24
#define BGDL_ERROR_CANCELLED		0x80103A1C	/* PLACEHOLDER name */

/* PLACEHOLDER name: the task's param file names no file (key 203 empty) */
#define BGDL_ERROR_NO_FILE		0x80103A1F

static BgdlManager *s_manager = NULL;		/* (0x81544788) */
/* the next file's (0x81123E48): a second job queue (PLACEHOLDER names) */
class BgdlJobQueue {
public:
	static BgdlJobQueue *Bgdl_81123E48(void);
	int Bgdl_81123E3C(common::SharedPtr<job::JobItem> job);

	job::JobQueue *m_queue;
};

/* PLACEHOLDER names: a content server's request, as 0x811049A4 is given it */
struct BgdlServerParam {
	SceUInt32 value[5];
};

struct BgdlServerQuery {
	int handle;			/* the first argument of 0x8111B72A */
	BgdlServerParam param;		/* passed as a copy */
	char url[1];			/* PLACEHOLDER: its length is not known yet */
};

struct BgdlServerIds {
	char reserved;
	char id0[32];
	char id1[32];
	char id2[1];			/* PLACEHOLDER: its length is not known yet */
};

/* the server's description of a content (key 232), 120 bytes (PLACEHOLDER names) */
struct BgdlServerData {
	int reserved0;
	int category;			/* 6, 7, 14-16: a game's (key 114 1); 24, 29, 31 */
	SceUInt32 attribute;		/* 0x10, bit 31: no patch looked for */
	char reserved1[28];
	char contentId[48];		/* the package's key 217 */
	char titleId[32];		/* key 220, the folder it goes to */
};

/* the next file's (0x81115C58): what the content server answered (PLACEHOLDER names) */
struct BgdlServerInfo {
	BgdlServerInfo()
	{
		Clear();
	}

	void Clear(void);		/* (0x81115C58) */

	bool flag;
	int status;
	int httpStatus;
	int reserved0;
	SceUInt64 size;
	string url;
	string titleId;
	string contentId;
	string message;			/* the server's, with an HTTP 401 */
	SceUInt64 value0;
	SceUInt64 value1;
	SceUInt64 value2;
	SceUInt64 value3;
	SceUInt64 value4;
	int kind;
	int reserved1;
	BgdlServerData data;
};

/* the answer as 0x811049A4's caller is given it, 0x2E0 bytes (PLACEHOLDER names) */
struct BgdlContentInfo {
	bool flag;
	int status;
	int httpStatus;
	int kind;
	SceUInt64 value0;
	SceUInt64 value1;
	SceUInt64 value2;
	SceUInt64 value3;
	SceUInt64 value4;
	SceUInt64 size;
	char url[193];
	char titleId[32];
	char contentId[64];
	char message[127];
	char data[256];
};

/* PLACEHOLDER name: the server answered with an HTTP error */
#define BGDL_ERROR_SERVER		0x80103A23

/* PLACEHOLDER name: the request names no URL */
#define BGDL_ERROR_NO_URL		0x80103A0A

/* PLACEHOLDER names: what AddContentTask finds wrong in the server's description */
#define BGDL_ERROR_BAD_KIND		0x80103A21
#define BGDL_ERROR_NO_SIZE		0x80103A2A

/* PLACEHOLDER name: QAF does not allow a fake AC install on grw0: */
#define BGDL_ERROR_NO_FAKE_AC		0x80103A43

/* PLACEHOLDER names: what Bgdl_81107BDE finds wrong */
#define BGDL_ERROR_NO_LICENSE		0x80103A35	/* the content's licence not stored or read */
#define BGDL_ERROR_NO_KEY230		0x80103A36	/* a package of kind 9 names no file 230 */
#define BGDL_ERROR_BAD_NAME		0x80103A0C	/* the file's name not made from key 222 */

/* the next file's (PLACEHOLDER names) */
int Bgdl_8111296E(const char *folder, int *parts, SceUInt64 size, int first, int *count);	/* split in FAT32 parts */
int Bgdl_8111322E(const char *file, const char *folder, string *path);	/* copied to sce_sys/package/work.bin */
int Bgdl_81111B96(const string *name, const string *base, string *out);	/* base with name's extension */
int Bgdl_8111431C(string *str, const char *from, const char *to);	/* 1: from replaced by to */

/* (0x81415650) PLACEHOLDER name: the kinds that go into a game's own folder */
__attribute__((noinline)) bool bgdlKindIsGame(int kind)
{
	return kind == 12 || kind == 8 || kind == 7 || kind == 6;
}

/* (0x81415668) PLACEHOLDER name: the server's categories of a game */
__attribute__((noinline)) int bgdlCategoryIsGame(int category)
{
	switch (category) {
	case 6:
	case 7:
	case 14:
	case 15:
	case 16:
		return 1;
	default:
		return 0;
	}
}

/* game_plugin's (PLACEHOLDER names: game_plugin.h does not name them yet) */
int Bgdl_81114092(paf::vector<string> *titles);		/* the game card's titles */
int Bgdl_81371C70(const char *titleId, int *found);	/* 1: the title has a row */

/* download_plugin's name for the PSM runtime's package (PLACEHOLDER name) */
#define DOWNLOAD_PLUGIN_STR_PSM_RUNTIME	0xC398A7A7

int Bgdl_8111FEF8(const char *device, const char *url, int flags, int reserved0, MtpBgCopyHost *host,
		  int reserved1, const char *folder, const char *name, int *status, BgdlServerInfo *server,
		  string *path);		/* esehttp.cpp: a file got into folder (PLACEHOLDER) */

int Bgdl_8111B72A(int handle, int reserved0, const char *device, const char *url,
		  const char *id0, const char *id1, const char *id2, const char *contentId,
		  BgdlServerInfo *info, BgdlServerParam *param, MtpBgCopyHost *host, int *checkResult,
		  char *digest);

/* the server's kinds as the caller's (PLACEHOLDER names) */
struct BgdlKindMap {
	int server;
	int kind;
};

/* (0x8150A170) */
static const BgdlKindMap s_kindMap[] = {
	{ 1, 1 }, { 2, 2 }, { 3, 3 }, { 4, 4 }, { 5, 5 }, { 6, 6 },
	{ 7, 7 }, { 8, 8 }, { 9, 9 }, { 12, 12 }, { 0, 0 },
};

/* the next file's (0x81110708): the time a block of code took (PLACEHOLDER names) */
class BgdlScopeTimer {
public:
	BgdlScopeTimer(const char *name) : m_name(name)
	{
		m_start = get_time_internal();
	}

	~BgdlScopeTimer()
	{
		m_end = get_time_internal();
		Report();
	}

	void Report(void);		/* (0x81110708) the time taken, in milliseconds */

	string m_name;
	SceUInt64 m_start;
	SceUInt64 m_end;
	SceUInt64 m_elapsed;
};

/* the next file's (0x811107D0, 0x811107F0): the power kept on while it lives (PLACEHOLDER names) */
class BgdlPowerLock {
public:
	BgdlPowerLock(bool lock, int arg);
	~BgdlPowerLock();

	bool m_locked;
	int m_arg;			/* PLACEHOLDER: kept, not read */
};

/* PLACEHOLDER name: a task marked against removal (m_removing) while it lives */
class BgdlTaskMark {
public:
	BgdlTaskMark(SceUInt32 id, EseTask *task) : id(id), marked(false), task(task)
	{
	}

	~BgdlTaskMark()
	{
		if (marked == true) {
			marked = false;
			task->m_removing = false;
		}
	}

	SceUInt32 id;
	bool marked;
	EseTask *task;
};

/* PLACEHOLDER name: a task found still running when its device was prepared */
#define BGDL_ERROR_INTERRUPTED		0x80103A44

/* an NP environment's PSM runtime server (PLACEHOLDER names) */
struct BgdlPsmServer {
	char env[17];			/* sceNpCommonGetNpEnviroment's name */
	char url[256];
};

#define BGDL_PSM_SERVER_MAX	7

/* (0x815099F8) PLACEHOLDER names: the PSM runtime's server for each NP environment */
static const BgdlPsmServer s_psmServers[BGDL_PSM_SERVER_MAX] = {
	{ "np", "http://psm-runtime.np.dl.playstation.net/psm-runtime/" },
	{ "mgmt", "http://psm-runtime.mgmt.dl.playstation.net/psm-runtime/" },
	{ "e1-np", "http://psm-runtime.e1-np.dl.playstation.net/psm-runtime/" },
	{ "sp-int", "http://psm-runtime.np.dl.playstation.net/psm-runtime/" },
	{ "prod-qa", "http://psm-runtime.np.dl.playstation.net/psm-runtime/" },
	{ "kurokawa", "http://192.168.92.250/download/psm/" },
	{ "", "" },
};
static const BgdlPsmServer *s_psmServer = &s_psmServers[0];	/* (0x8154478C) the console's */
static SceUInt64 s_handled = 0;			/* (0x81544790) the requests the thread took */
static SceUInt64 s_wakeupsHandled = 0;		/* (0x81544798) the wakeups the thread took */
static SceUInt64 s_requested = 0;		/* (0x815447A0) the requests made */
static SceUInt64 s_wakeups = 0;			/* (0x815447A8) */
static thread::RMutex s_packageMutex("SceBgdlRMutexPackage", 0);	/* (0x81563150) */
static thread::RMutex s_progressMutex("SceBgdlRMutexProgress", 0);	/* (0x81563170) */
static thread::RMutex s_updateMutex("SceBgdlRMutexUpdate", 0);		/* (0x81563190) guards s_manager */

/* (0x810FCD10) the task's folder on a device: its download folder, or its work folder */
int BgdlTaskPath::initialize(const char *device, int taskId, bool work)
{
	string path(device);
	int ret;

	if (!work)
		path += "bgdl/t";
	else
		path += "bgdl/w";
	char name[24] = "";

	sce_paf_snprintf(name, sizeof(name) - 1, "/%08x", taskId);
	path += name;
	ret = Reserve(BGDL_PATH_FOLDER_LENGTH + 1);
	if (ret != 0)
		return ret;
	sce_paf_snprintf(m_begin, m_end - m_begin, "%s", path.c_str());
	return 0;
}

/* (0x810FCF36) the task's download folder, -1 and ids from 0 on ux0:, the others on ur0: */
int BgdlTaskPath::initialize(int taskId, SceSize size)
{
	int ret;

	ret = Reserve(size);
	if (ret != 0)
		return ret;
	if (taskId == -2)
		sce_paf_snprintf(m_begin, size, "ur0:bgdl/t/%08x", taskId);
	else if (taskId == -1)
		sce_paf_snprintf(m_begin, size, "ux0:bgdl/t/%08x", taskId);
	else if (taskId >= 0)
		sce_paf_snprintf(m_begin, size, "ux0:bgdl/t/%08x", taskId);
	else
		sce_paf_snprintf(m_begin, size, "ur0:bgdl/t/%08x", taskId);
	return 0;
}

/* (0x810FCFEE) a file in the folder */
char *BgdlTaskPath::SetName(const char *name)
{
	sce_paf_strncpy(m_begin + BGDL_PATH_FOLDER_LENGTH, name, sce_paf_strlen(name) + 1);
	return m_begin;
}

/* (0x810FD00E) a file in the folder, by a format; the buffer grows to hold it */
int BgdlTaskPath::format(const char *format, ...)
{
	va_list ap;
	int room;
	int len;
	int ret;

	va_start(ap, format);
	room = m_end - m_begin - BGDL_PATH_FOLDER_LENGTH;
	len = sce_paf_vsnprintf(m_begin + BGDL_PATH_FOLDER_LENGTH, room, format, ap);
	if (len >= 0 && len < room)
		return 0;
	BgdlTaskPath path;

	ret = path.Reserve(len + room + 1);
	if (ret != 0)
		return ret;
	sce_paf_strncpy(path.m_begin, m_begin, path.m_end - path.m_begin);
	sce_paf_vsnprintf(path.m_begin + BGDL_PATH_FOLDER_LENGTH, len + 1, format, ap);
	Swap(path);
	return 0;
}

/* (0x810FD106) the folder alone */
char *BgdlTaskPath::ClearName(void)
{
	sce_paf_strncpy(m_begin + BGDL_PATH_FOLDER_LENGTH, "", sce_paf_strlen("") + 1);
	return m_begin;
}

/* (0x810FD12E) */
bool BgdlTaskPath::Exists(const char *path)
{
	if (path == NULL)
		return false;
	return Dir::IsExist(path);
}

/* (0x810FD13C) */
bool BgdlTaskPath::Exists(void)
{
	if (m_begin == NULL)
		return false;
	return Dir::IsExist(m_begin);
}


/* (0x810FD14C) */
void BgdlProgress::Start(SceUInt64 total, SceUInt64 base, SceUInt64 reserved, SceUInt64 interval)
{
	m_done = 0;
	m_startTime = 0;
	m_lastTime = 0;
	m_reportTime = 0;
	m_reserved0 = 0;
	m_reserved1 = 0;
	m_total = total;
	m_base = base;
	m_current = 0;
	m_interval = interval;
	m_startTime = get_time_internal();
	m_reportTime = m_startTime;
}

/* (0x810FD19A) 1 when an interval has passed since the last report */
int BgdlProgress::Update(SceUInt64 current)
{
	int ret = 0;
	SceUInt64 now;

	now = get_time_internal();
	m_lastTime = now;
	m_current = current;
	m_done = current - m_base;
	if (now - m_reportTime > m_interval) {
		ret = 1;
		m_reportTime = now;
	}
	return ret;
}

/* (0x810FD1F2) */
SceUInt64 BgdlProgress::GetSpeed(void)
{
	SceUInt64 speed = 0;
	SceUInt64 elapsed;

	if (m_total == (SceUInt64)-1 || m_total == 0)
		return speed;
	if (m_done == (SceUInt64)-1 || m_done == 0)
		return speed;
	elapsed = m_lastTime - m_startTime;
	if (elapsed == 0)
		return speed;
	speed = (SceUInt64)((double)m_done / (double)elapsed * 1000000.0);
	return speed;
}

/* (0x810FD2F0) */
SceUInt64 BgdlProgress::GetRemaining(void)
{
	SceUInt64 speed;

	speed = GetSpeed();
	if (speed > 0)
		return (m_total - m_current) / speed;
	return 0;
}

/* (0x810FD328) */
int bgdlStringToInt(const string &str)
{
	int value;

	sce_paf_memset(&value, 0, sizeof(value));
	if (str.length() != 0)
		return sce_paf_atoi(str.c_str());
	return value;
}

/* (0x810FD366) */
int bgdlUIntToString(const SceUInt32 *value, string *str)
{
	char buf[16] = "";
	SceUInt32 v;

	sce_paf_memset(&v, 0, sizeof(v));
	v = *value;
	sce_paf_snprintf(buf, sizeof(buf) - 1, "%u", v);
	str->assign(buf, sce_paf_strlen(buf));
	return 0;
}

/* (0x810FD412) */
int bgdlGetInt(const int *value)
{
	int v;

	sce_paf_memset(&v, 0, sizeof(v));
	v = *value;
	return v;
}

/* (0x810FD442) */
int bgdlParseInt(const string &str)
{
	if (str.length() != 0)
		return sce_paf_atoi(str.c_str());
	return 0;
}

/* (0x810FD454) */
int bgdlFormatUInt(SceUInt32 value, string *str)
{
	char buf[16] = "";

	sce_paf_snprintf(buf, sizeof(buf) - 1, "%u", value);
	str->assign(buf, sce_paf_strlen(buf));
	return 0;
}

/* the first of [first, last) whose id is not below the task's (Dinkumware's lower_bound) */
static inline EseTask **bgdlLowerBound(EseTask **first, EseTask **last, EseTask *const &task)
{
	int count = last - first;

	while (count > 0) {
		int half = count / 2;
		EseTask **mid = first + half;

		if ((*mid)->id < task->id) {
			first = ++mid;
			count -= half + 1;
		} else {
			count = half;
		}
	}
	return first;
}

/* (0x810FD4EA) */
int BgdlTaskList::insert(EseTask *task)
{
	BgdlArray<EseTask *> list;
	EseTask **first;
	EseTask **last;
	EseTask **dst;
	EseTask **it;
	int count;
	int half;
	int ret;

	ret = list.Reserve((char *)m_end - (char *)m_begin + sizeof(EseTask *));
	if (ret != 0)
		return ret;
	first = m_begin;
	last = m_end;
	it = first;
	count = ((char *)last - (char *)first) / (int)sizeof(EseTask *);
	while (count > 0) {
		half = count / 2;
		if (it[half]->id < task->id) {
			it += half + 1;
			count = count - half - 1;
		} else {
			count = half;
		}
	}
	if (it != last && !(task->id < (*it)->id))
		return BGDL_ERROR_ALREADY_EXISTS;
	dst = list.m_begin;
	while (first != it)
		*dst++ = *first++;
	*dst++ = task;
	while (it != last)
		*dst++ = *it++;
	Swap(list);
	return 0;
}

/* (0x810FD5E6) */
void BgdlTaskList::Erase(EseTask **pos)
{
	EseTask **it;

	if (pos >= m_end)
		return;
	for (it = pos + 1; it != m_end; it++)
		it[-1] = *it;
	Reserve((char *)m_end - (char *)m_begin - sizeof(EseTask *));
}

/* (0x810FD64E) */
EseTask **BgdlTaskList::Find(EseTask *task)
{
	EseTask **it;

	it = bgdlLowerBound(m_begin, m_end, task);
	if (it != m_end && task->id < (*it)->id)
		return m_end;
	return it;
}

/* (0x810FD6BA) */
void BgdlTaskList::Clear(void)
{
	EseTask **it;

	for (it = m_begin; it != m_end; it++) {
		if (*it != NULL) {
			delete *it;
			*it = NULL;
		}
	}
	Reserve(0);
}

/* (0x810FD758) the flags in mask made value's, once the thread has seen the request */
void BgdlManager::UpdateFlags(SceUInt32 value, SceUInt32 mask)
{
	SceUInt32 flags;
	SceUInt64 request;
	bool done;

	if (m_error != 0)
		return;
	flags = (m_flags & ~mask) | (value & mask);
	if (m_flags == flags)
		return;
	m_taskListMutex.Lock();
	request = ++s_requested;
	if (m_runningTask != NULL) {
		m_cancelMutex.Lock();
		if (!m_cancelled) {
			m_cancelled = true;
			if (m_cancelHandler != NULL)
				m_cancelHandler(m_cancelHandlerArg);
		}
		m_cancelMutex.Unlock();
	}
	m_taskListCond.Notify();
	m_taskListMutex.Unlock();
	for (;;) {
		m_taskListMutex.Lock();
		done = s_handled >= request;
		m_taskListMutex.Unlock();
		if (done)
			break;
		thread::Sleep(16);
	}
	m_taskListMutex.Lock();
	m_taskListMutex.Unlock();
	m_flags = flags;
	m_taskListMutex.Lock();
	s_wakeups++;
	m_taskListCond.Notify();
	m_taskListMutex.Unlock();
}

/* (0x810FD862) */
int bgdlManagerNotify(int value, int mask)
{
	s_updateMutex.Lock();
	if (s_manager != NULL) {
		s_manager->UpdateFlags(value, mask);
		s_updateMutex.Unlock();
		return 0;
	}
	s_updateMutex.Unlock();
	return BGDL_ERROR_NOT_STARTED;
}

/* (0x810FD8A4) */
int Job_EseSetPermission::Run()
{
	SceUInt32 mask = m_mask;
	SceUInt32 value = m_value;

	s_updateMutex.Lock();
	if (s_manager != NULL)
		s_manager->UpdateFlags(value, mask);
	s_updateMutex.Unlock();
	return 0;
}

/* (0x810FD8E0) */
BgdlManager *bgdlManagerGet(void)
{
	return s_manager;
}

/* (0x810FD8EC) */
bool BgdlManager::IsCancelled(void)
{
	bool cancelled;

	m_cancelMutex.Lock();
	cancelled = m_cancelled;
	m_cancelMutex.Unlock();
	return cancelled;
}

/* (0x810FD90C) */
void BgdlManager::NotifyListeners(BgdlEvent *event)
{
	BgdlListener *listener;

	m_listenersMutex.Lock();
	if (m_listenerCount != 0) {
		for (listener = m_listeners; listener != m_listeners + BGDL_LISTENER_MAX; listener++) {
			if (listener->func != NULL)
				listener->func(event, listener->arg);
		}
	}
	m_listenersMutex.Unlock();
}

/* (0x810FD94C) */
int EseTask::setRunStatus(int status)
{
	BgdlEvent event;
	int ret;

	if (m_ParamCache.runStatus == status)
		return 0;
	m_ParamCache.runStatus = status;
	ret = Bgdl_811233A4(id, 101, &status, sizeof(status));
	event.type = 4;
	event.id = id;
	event.param = m_ParamCache;
	event.result = result;
	event.reserved = 0;
	event.value = value;
	s_manager->NotifyListeners(&event);
	if (ret != 0)
		return ret;
	return 0;
}

/* (0x810FDA08) */
int EseTask::setState(int state, int *current, bool force)
{
	BgdlEvent event;
	int saved = 0;
	int ret;

	ret = Bgdl_8112260C(id, 109, &saved, sizeof(saved));
	if (ret != 0)
		return ret;
	if (current != NULL)
		*current = saved;
	if (state > saved || force == true) {
		m_ParamCache.state = state;
		if (current != NULL)
			*current = state;
		ret = Bgdl_811233A4(id, 109, &state, sizeof(state));
		event.type = 3;
		event.id = id;
		event.result = 0;
		event.reserved = 0;
		event.value = 0;
		event.param = m_ParamCache;
		if (m_ParamCache.state == 4)
			event.value = value;
		s_manager->NotifyListeners(&event);
	}
	return ret;
}

/* (0x810FDAEC) the task's folder: the one its param file names (key 229), or its own */
int EseTask::GetFolder(BgdlTaskPath *path, bool fromParam)
{
	BgdlTaskPath folder;
	int ret;

	if (fromParam == true) {
		string name;

		ret = Bgdl_8112278A(id, 229, &name);
		if (ret == 0 && name.length() != 0) {
			path->assign(name.c_str());
			return 0;
		}
	}
	ret = folder.initialize(id, 52);
	if (ret != 0)
		return ret;
	path->Swap(folder);
	return 0;
}

/* (0x810FDC1C) */
EseTask *BgdlManager::FindTask(SceUInt32 id)
{
	EseTask **first = m_tasks.m_begin;
	EseTask **last = m_tasks.m_end;
	int count = last - first;

	while (count > 0) {
		int half = count / 2;
		EseTask **mid = first + half;

		if ((*mid)->id < id) {
			first = ++mid;
			count -= half + 1;
		} else {
			count = half;
		}
	}
	if (first != last && id < (*first)->id)
		first = last;
	if (first == last)
		return NULL;
	return *first;
}

/* the task's folder as a string, empty when no such task is listed (inlined where it is used) */
static __attribute__((always_inline)) string taskFolder(SceUInt32 id, bool fromParam)
{
	string folder("");

	if (s_manager != NULL) {
		EseTask *task = s_manager->FindTask(id);

		if (task != NULL) {
			BgdlTaskPath path;

			if (task->GetFolder(&path, fromParam) == 0)
				folder = path.m_begin;
		}
	}
	return folder;
}

/* (0x810FDC72) */
string bgdlTaskFolder(SceUInt32 id, bool fromParam)
{
	return taskFolder(id, fromParam);
}

/*
 * (0x810FDD12) the task's file (key 203): in the folder its param file names
 * (key 229) when fromParam is set and one is named, else in its own folder
 */
int EseTask::getFilePath(BgdlTaskPath *path, bool fromParam)
{
	BgdlTaskPath file;
	BgdlTaskPath full;
	int ret;

	if (fromParam == true) {
		string folder;

		ret = Bgdl_8112278A(id, 229, &folder);
		if (ret == 0 && folder.length() != 0) {
			string name;
			const char *src;

			ret = Bgdl_8112278A(id, 203, &name);
			if (ret != 0)
				return ret;
			src = folder.c_str();
			if (src[sce_paf_strlen(src)] != '/')
				folder += "/";
			folder.append(name);
			path->assign(folder.c_str());
			return 0;
		}
	}
	ret = Bgdl_81122700(id, 203, &file);
	if (ret != 0)
		return ret;
	if (file.m_end == file.m_begin)
		return BGDL_ERROR_NO_FILE;
	ret = full.initialize(id, file.m_end - file.m_begin + 20);
	if (ret != 0)
		return ret;
	ret = full.format("/%s", file.m_begin);
	if (ret != 0)
		return ret;
	path->Swap(full);
	return 0;
}

/* (0x810FE038) */
bool BgdlManager::SetCancelHandler(void (*handler)(void *arg), void *arg)
{
	bool cancelled;

	m_cancelMutex.Lock();
	cancelled = m_cancelled;
	if (cancelled == false) {
		m_cancelHandler = handler;
		m_cancelHandlerArg = arg;
	}
	m_cancelMutex.Unlock();
	return cancelled == false;
}

static void bgdlManagerOnResume(int code, const void *data, SceSize size, void *arg);
static void bgdlMtpNotify(void *arg, const MtpBgCopyProgress *progress);
static int bgdlMtpAccept(void);

/* libmtp asks whether the copy is to stop (PLACEHOLDER name) */
class BgdlMtpCopyHost : public MtpBgCopyHost {
public:
	BgdlMtpCopyHost(EseTask *task) : m_task(task)
	{
	}

	virtual int isRequested()
	{
		{
			thread::RMutexLock lock(&s_manager->m_cancelMutex);
		}
		return s_manager->IsCancelled();
	}

	EseTask *m_task;
};

/* the server request stopped when *cancel is set (PLACEHOLDER name) */
class BgdlCancelHost : public MtpBgCopyHost {
public:
	BgdlCancelHost(const bool *cancel) : m_cancel(cancel)
	{
	}

	virtual int isRequested()
	{
		return *m_cancel;
	}

	const bool *m_cancel;
};

/* (0x810FE06C) an MTP task: libmtp copies its file, and the result goes to its param file */
int EseTask::copyMtp(int *runStatus)
{
	BgdlManager *manager = s_manager;
	bool done;
	int ret = 0;
	int current;

	manager->SetCancelHandler(NULL, NULL);
	current = 0;
	setState(1, &current, false);
	thread::Sleep(500);
	setState(2, &current, false);
	thread::Sleep(500);

	BgdlMtpCopyHost host(this);
	MtpBgCopyRunParam param;

	param.id = id;
	param.arg = this;
	param.notify = bgdlMtpNotify;
	param.accept = bgdlMtpAccept;
	param.host = &host;
	param.file = "";
	{
		BgdlTaskPath file;
		BgdlTaskPath folder;

		ret = getFilePath(&file, false);
		if (ret == 0 && file.m_end != file.m_begin)
			param.file = file.m_begin;
		param.path = taskFolder(id, false);
		ret = GetFolder(&folder, true);
		if (ret == 0)
			param.paramFolder = folder.m_begin;
	}

	ret = bgdlMtpRunCopy(&param);
	*runStatus = 0;
	switch (ret) {
	case 0:
	case (int)0x80546600:
	case (int)0x80546611:
	case (int)0x80546621:
		break;
	default:
		ret = BGDL_ERROR_COPY_FAILED;
		return ret;
	}

	done = false;
	if (ret == 0) {
		setState(4, NULL, false);
		done = true;
	}
	m_ParamCache.runStatus = *runStatus;
	m_ParamCache.key102 = done;
	m_ParamCache.ready = done;
	{
		EseParam file(id);

		if (file.openWrite() == 0 && file.write(101, runStatus, sizeof(*runStatus)) == 0 &&
		    file.write(104, &ret, sizeof(ret)) == 0 &&
		    file.write(102, &done, sizeof(done)) == 0)
			file.commit();
	}

	BgdlEvent event;

	event.type = 3;
	event.id = id;
	event.param = m_ParamCache;
	event.result = ret;
	event.reserved = 0;
	event.value = 0;
	s_manager->NotifyListeners(&event);
	return ret;
}

/*
 * (0x810FE448) runs the task by its type; a task that failed while it was to run again
 * waits up to 5 seconds, or until it is cancelled, before the next one runs
 */
int EseTask::invoke(void)
{
	SceUInt64 start = get_time_internal();
	int status;
	int ret;
	int next;

	ret = setRunStatus(2);
	status = ret;
	if (ret == 0) {
		next = 1;
		switch (m_ParamCache.type) {
		case 0:
			ret = execute(&next);
			break;
		case 1:
			ret = copyMtp(&next);
			break;
		default:
			ret = 0;
			next = 0;
			break;
		}
		if (next == 0) {
			result = ret;
			Bgdl_811233A4(id, 104, &ret, sizeof(ret));
		}
		setRunStatus(next);
		status = ret;
		if (status == 0)
			return 0;
	}
	if (status != 0 && m_ParamCache.runStatus == 1) {
		float deadline = (float)start + 5000000.0f;

		while (!s_manager->IsCancelled() && (float)get_time_internal() < deadline)
			thread::Sleep(16);
	}
	return status;
}

/* a task is done: no error, not to run again, key 113 set */
static inline bool taskIsReady(const EseTask *task)
{
	return task->result == 0 && task->m_ParamCache.runStatus == 0 && task->m_ParamCache.key113;
}

/*
 * (0x810FE5A4) the manager's list of its tasks and appmgr's background download queue made
 * again from the task list (only while ux0: is there)
 */
void BgdlManager::UpdateQueueStatus(void)
{
	EseTask **it;

	m_hasPending = false;
	for (it = m_tasks.m_begin; it != m_tasks.m_end; it++) {
		EseParamCache param = (*it)->m_ParamCache;

		if (param.key107 == 5 || param.key111 == 2) {
			m_hasPending = true;
			break;
		}
	}
	if (m_ux0Ready == false)
		return;

	m_taskListMutex.Lock();
	sce_paf_memset(&m_summaries, 0, sizeof(m_summaries));
	sce_paf_memset(m_queue, 0, sizeof(m_queue));
	{
		BgdlTaskPackage package;
		BgdlTaskSummary *entry = m_summaries.entries;
		int count = 0;

		for (it = m_tasks.m_begin; it != m_tasks.m_end; it++) {
			EseTask *task = *it;

			if (task->m_ParamCache.type != 0)
				continue;
			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			if (package.key220.length() == 0)
				continue;

			entry->param.type = (*it)->m_ParamCache.type;
			entry->param.key107 = (*it)->m_ParamCache.key107;
			entry->param.runStatus = (*it)->m_ParamCache.runStatus;
			entry->param.key102 = (*it)->m_ParamCache.key102;
			entry->param.ready = (*it)->m_ParamCache.ready;
			entry->param.key108 = (*it)->m_ParamCache.key108;
			entry->param.state = (*it)->m_ParamCache.state;
			entry->param.key112 = (*it)->m_ParamCache.key112;
			entry->param.key113 = (*it)->m_ParamCache.key113;
			entry->param.key110 = (*it)->m_ParamCache.key110;
			entry->param.key111 = (*it)->m_ParamCache.key111;
			entry->param.kind = (*it)->m_ParamCache.kind;
			entry->param.key115 = (*it)->m_ParamCache.key115;
			entry->param.key116 = (*it)->m_ParamCache.key116;
			entry->param.reserved = (*it)->m_ParamCache.reserved;
			entry->ready = taskIsReady(*it);
			sce_paf_snprintf(entry->titleId, sizeof(entry->titleId), "%s",
					 package.key220.c_str());
			if (package.key217.length() != 0)
				sce_paf_snprintf(entry->contentId, sizeof(entry->contentId), "%s",
						 package.key217.c_str());
			if (count < BGDL_QUEUE_MAX) {
				if (package.key217.length() != 0)
					sce_paf_snprintf(m_queue[count].contentId,
							 sizeof(m_queue[count].contentId) - 1, "%s",
							 package.key217.c_str());
				if (entry->param.key107 == 9)
					m_queue[count].state = 1;
				else if (taskIsReady(*it))
					m_queue[count].state = 1;
				else
					m_queue[count].state = 0;
				switch (entry->param.key107) {
				case 12:
					m_queue[count].type = 4;
					break;
				case 9:
					m_queue[count].type = 3;
					break;
				case 8:
					m_queue[count].type = 1;
					break;
				case 7:
					m_queue[count].type = 2;
					break;
				default:
					m_queue[count].type = 0;
					break;
				}
			}
			count++;
			entry++;
		}
		vshAppMgrBgdlSetQueueStatus(m_queue);
	}
	m_taskListMutex.Unlock();
}

/*
 * (0x810FE8F0) a task removed: its MTP copy cancelled, or its PSP download in
 * ux0:pspemu/bgdl removed, then its folder, and the listeners told (event 1)
 */
int BgdlManager::deleteTask(SceUInt32 id, const EseParamCache *param)
{
	BgdlTaskPath folder;
	int ret;

	ret = folder.initialize(id, 20);
	if (ret != 0)
		return ret;
	{
		string path("");
		EseParam file(id);

		if (file.open() == 0) {
			if (file.read(229, &path) == 0 && path.length() != 0) {
				if (sce_paf_strncmp(path.c_str(), "ux0:pspemu/bgdl", 15) == 0) {
					BgdlGamePluginLock lock(true);

					Bgdl_81112112(&path, true);
				}
			} else if (param->type == 1) {
				bgdlMtpCancelCopy(id);
			}
		}
	}

	EseParam lock(id);

	ret = 0;
	if (folder.Exists() && (ret = Bgdl_811120A0(&folder, true)) != 0)
		return ret;

	BgdlEvent event;

	event.type = 1;
	event.id = id;
	event.param = *param;
	event.result = 0;
	event.reserved = 0;
	event.value = 0;
	NotifyListeners(&event);
	return ret;
}

/* (0x810FEA80) a finished task counted in ur0:ci by its kind (PLACEHOLDER name) */
void bgdlCountFinished(const EseTask *task)
{
	EseParamCache param;

	if (task == NULL)
		return;
	param = task->m_ParamCache;
	if (param.runStatus != 0)
		return;
	if (task->result != 0 && !param.key113)
		return;
	if (param.type != 0)
		return;
	switch (param.key107) {
	case 1:
		consoleInfoFileAddInt(41, 1, 0);
		break;
	case 2:
		consoleInfoFileAddInt(40, 1, 0);
		break;
	case 3:
	case 4:
		consoleInfoFileAddInt(39, 1, 0);
		break;
	case 6:
		switch (param.kind) {
		case 0:
			consoleInfoFileAddInt(32, 1, 0);
			break;
		case 1:
		case 2:
			consoleInfoFileAddInt(33, 1, 0);
			break;
		default:
			consoleInfoFileAddInt(38, 1, 0);
			break;
		}
		break;
	case 7:
	case 8:
		consoleInfoFileAddInt(38, 1, 0);
		break;
	default:
		consoleInfoFileAddInt(43, 1, 0);
		break;
	}
}

/* (0x810FEB22) a finished application or content download turns its auto download off */
void bgdlUnsetAutoDownload(const EseTask *task)
{
	SceUInt32 kind;

	if (task == NULL)
		return;
	kind = task->m_ParamCache.key107;
	if (kind != 12 && kind != 8 && kind != 7 && kind != 6)
		return;
	if (task->m_ParamCache.key107 == 8)
		return;

	BgdlTaskPackage package;

	s_packageMutex.Lock();
	package = task->package;
	s_packageMutex.Unlock();
	if (package.key217.length() != 0) {
		Job_EseUnsetAutoDownload *item =
			new Job_EseUnsetAutoDownload(package.key217.c_str());
		common::SharedPtr<job::JobItem> job(item);

		BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
	}
}

/*
 * (0x810FED12) a task's run is over: it is removed when it ran without error and is done (or while
 * libmtp's copy may go on), otherwise counted and its follow-up jobs queued; the listeners
 * are told (event 3) and the queue made again
 */
int BgdlManager::finishTask(EseTask *task, int result)
{
	SceUInt32 id = task->id;
	SceUInt64 value = task->value;
	EseParamCache param;
	bool removed = false;
	bool notify = false;
	bool removeRunning = false;
	int ret = 0;

	m_taskListMutex.Lock();
	m_cancelMutex.Lock();
	if (m_cancelled)
		m_lastId = id - 1;
	m_cancelled = false;
	m_cancelHandler = NULL;
	m_cancelHandlerArg = NULL;
	m_cancelMutex.Unlock();
	m_runningTask = NULL;
	if ((result == 0 && task->m_ParamCache.key102) || m_removeRunning) {
		EseTask **pos = m_tasks.Find(task);

		if (pos != m_tasks.m_end) {
			task->m_ParamCache.ready = !m_removeRunning;
			param = task->m_ParamCache;
			bgdlCountFinished(task);
			delete *pos;
			m_tasks.Erase(pos);
			removed = true;
			notify = true;
		}
	} else if (result == 0) {
		task->m_ParamCache.ready = !task->m_ParamCache.key113;
		param = task->m_ParamCache;
		bgdlCountFinished(task);
		if (task->m_ParamCache.key110)
			bgdlUnsetAutoDownload(task);
		notify = true;
		if (task->m_ParamCache.key107 == 6 || task->m_ParamCache.key107 == 12) {
			Job_EseGDEvent *item = new Job_EseGDEvent(id);
			common::SharedPtr<job::JobItem> job(item);

			BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
		}
		if (task->m_ParamCache.key107 == 8) {
			BgdlTaskPackage package;

			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			if (package.key220.length() != 0)
				Bgdl_812A9954(package.key220.c_str(), 5);
		}
	}
	m_taskListMutex.Unlock();

	if (notify) {
		BgdlEvent event;

		event.type = 3;
		event.id = id;
		event.param = param;
		event.result = ret;
		event.reserved = ret;
		event.value = value;
		NotifyListeners(&event);
	}
	if (removed) {
		deleteTask(id, &param);
		m_taskListMutex.Lock();
		m_removeRunning = removeRunning;
		m_taskListMutex.Unlock();
	}
	UpdateQueueStatus();
	return ret;
}

/* (0x810FF102) */
bool BgdlManager::IsSuspended(void)
{
	return m_suspended;
}

/*
 * (0x810FF108) the next task to run, once every request is taken and tasks may run (flag 2):
 * the first of the lowest priority (key 108) that is to run, after the last one run, else
 * from the start of the list
 */
bool BgdlManager::NextTask(EseTask **next)
{
	EseTask *task = NULL;
	EseTask **first;
	EseTask **last;
	EseTask **it;
	int priority;

	if (s_manager->m_suspended == true)
		return false;
	s_handled = s_requested;
	s_wakeupsHandled = s_wakeups;
	if (s_requested > s_wakeups)
		return false;
	if ((m_flags & 2) == 0)
		return false;

	first = m_tasks.m_begin;
	last = m_tasks.m_end;
	for (priority = 0; priority <= 3; priority++) {
		for (it = m_tasks.m_begin; it != last; it++) {
			if ((*it)->m_ParamCache.key108 == priority && (*it)->m_ParamCache.runStatus == 1 &&
			    m_lastId < (*it)->id) {
				task = *it;
				break;
			}
		}
		if (task != NULL)
			break;
		for (it = m_tasks.m_begin; it != last; it++) {
			if ((*it)->m_ParamCache.key108 == priority && (*it)->m_ParamCache.runStatus == 1 &&
			    (*it)->id <= m_lastId) {
				task = *it;
				break;
			}
		}
		if (task != NULL)
			break;
	}
	if (task == NULL)
		return false;
	m_runningTask = task;
	m_lastId = task->id;
	*next = task;
	return true;
}

/* (0x810FF214) the tasks run one at a time, from the system's resume on, until it is stopped */
void BgdlManagerThread::EntryFunction()
{
	BgdlManager *manager = m_manager;

	if (manager == NULL)
		return;
	manager->UpdateQueueStatus();
	while (manager->m_resumed == false)
		thread::Sleep(1000);
	for (;;) {
		EseTask *task;

		manager->m_taskListMutex.Lock();
		task = NULL;
		while (manager->m_stopping == false && manager->NextTask(&task) == false)
			manager->m_taskListCond.Wait(0);
		manager->m_taskListMutex.Unlock();
		if (task == NULL)
			break;
		manager->finishTask(task, task->invoke());
	}
}

/* the first task whose id is not below id */
static inline EseTask **bgdlLowerBoundId(EseTask **first, EseTask **last, SceUInt32 id)
{
	int count = last - first;

	while (count > 0) {
		int half = count / 2;

		if ((*(first + half))->id >= id) {
			count = half;
		} else {
			first += half + 1;
			count -= half + 1;
		}
	}
	return first;
}

/* (0x810FF2AE) */
Job_EseRegistPatchByTitleID::Job_EseRegistPatchByTitleID(const BgdlTaskPackage &package,
							 const bool *flag)
	: paf::job::JobItem("Job_EseRegistPatchByTitleID", NULL), m_flag(false)
{
	m_package = package;
	m_flag = flag != NULL ? *flag : false;
}

/*
 * (0x810FF37C) a task removed by id: at once when it is not running, else the running task is
 * cancelled and the thread waited for (finishTask removes it)
 */
int BgdlManager::RemoveTask(SceUInt32 id)
{
	BgdlTaskPackage package;
	EseParamCache param;
	EseTask **pos;
	EseTask *task;
	bool marked;

	m_taskListMutex.Lock();
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end) {
		m_taskListMutex.Unlock();
		return 0;
	}
	m_taskListMutex.Unlock();

	task = *pos;
	marked = false;
	if (task->m_removing == false) {
		marked = true;
		task->m_removing = true;
	} else if (task->m_removing == true) {
		return 0;
	}

	m_taskListMutex.Lock();
	if (*pos != m_runningTask) {
		EseTask *removed = *pos;
		int ret;

		if (removed != NULL) {
			param = removed->m_ParamCache;
			s_packageMutex.Lock();
			package = removed->package;
			s_packageMutex.Unlock();
			delete *pos;
		}
		m_tasks.Erase(pos);
		m_taskListMutex.Unlock();
		ret = deleteTask(id, &param);
		if (param.key107 == 8 && ret == 0 && package.key220.length() != 0)
			Bgdl_812A9954(package.key220.c_str(), 2);
		UpdateQueueStatus();
		return 0;
	}

	m_removeRunning = true;
	m_cancelMutex.Lock();
	if (m_cancelled == false) {
		void (*handler)(void *arg) = m_cancelHandler;

		m_cancelled = true;
		if (handler != NULL)
			handler(m_cancelHandlerArg);
	}
	m_cancelMutex.Unlock();
	while (m_removeRunning) {
		m_taskListMutex.Unlock();
		thread::Sleep(200);
		m_taskListMutex.Lock();
	}
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end && marked == true)
		marked = false;
	m_taskListMutex.Unlock();
	if (marked == true)
		task->m_removing = false;
	return 0;
}

/* (0x810FF6CC) the running task's status, for the shell to show */
void BgdlManager::SetStatus(const BgdlTaskStatus *status)
{
	m_progressMutex.Lock();
	memcpy(&m_status, status, 60);
	m_progressMutex.Unlock();
}

/* (0x8141550C) */
void EseTask::setProgress(bool report, SceUInt64 total, SceUInt64 current)
{
	status.id = id;
	status.current = current;
	status.total = total;
	transfer.Start(total, current, 0, 512000);
	if (report == true)
		s_manager->SetStatus(&status);
}

/* (0x8141559C) */
void EseTask::setPackage(const BgdlTaskPackage *src)
{
	s_packageMutex.Lock();
	package = *src;
	s_packageMutex.Unlock();
}

/* a key the param file may leave out */
static inline bool bgdlKeyOptional(int ret)
{
	return ret == 0 || ret == (int)BGDL_ERROR_NO_KEY;
}

/* a key the param file may leave out or have empty */
static inline bool bgdlKeyOptionalOrEmpty(int ret)
{
	return ret == 0 || ret == (int)BGDL_ERROR_NO_KEY || ret == (int)BGDL_ERROR_INVALID_ARGUMENT;
}

/* (0x810FF6F4) the task's state read from its param file, its progress made from it */
int EseTask::Load(void)
{
	bool key102, key110, key112, key113, key115, key116;
	int type, key107, runStatus, taskResult, key108, state, key111, kind;
	int fileNumber, fileCount;
	int ret;

	result = BGDL_ERROR_NOT_LOADED;
	m_ParamCache.type = 15;
	m_ParamCache.key107 = 0;
	m_ParamCache.runStatus = 0;
	m_ParamCache.key102 = 0;
	m_ParamCache.ready = 0;
	m_ParamCache.key108 = 1;
	m_ParamCache.state = 0;
	m_ParamCache.key112 = 0;
	m_ParamCache.key113 = 0;
	m_ParamCache.key110 = 0;
	m_ParamCache.kind = 0;
	m_ParamCache.reserved = 0;

	EseParam file(id);

	ret = file.open();
	if (ret != 0)
		return ret;
	ret = file.read(100, &type, 4);
	if (ret != 0)
		return ret;
	ret = file.read(107, &key107, 4);
	if (ret != 0)
		return ret;
	ret = file.read(101, &runStatus, 4);
	if (ret != 0)
		return ret;
	ret = file.read(102, &key102, 1);
	if (ret != 0)
		return ret;
	ret = file.read(104, &taskResult, 4);
	if (ret != 0)
		return ret;
	key108 = 1;
	file.read(108, &key108, 4);
	state = 0;
	ret = file.read(109, &state, 4);
	if (!bgdlKeyOptional(ret))
		return ret;
	key110 = false;
	ret = file.read(110, &key110, 1);
	if (!bgdlKeyOptional(ret))
		return ret;
	key112 = false;
	ret = file.read(112, &key112, 1);
	if (!bgdlKeyOptional(ret))
		return ret;
	key113 = false;
	ret = file.read(113, &key113, 1);
	if (!bgdlKeyOptionalOrEmpty(ret))
		return ret;
	key111 = 7;
	ret = file.read(111, &key111, 4);
	if (!bgdlKeyOptional(ret))
		return ret;
	kind = 0;
	ret = file.read(114, &kind, 4);
	if (!bgdlKeyOptionalOrEmpty(ret))
		return ret;
	key115 = false;
	ret = file.read(115, &key115, 1);
	if (!bgdlKeyOptionalOrEmpty(ret))
		return ret;
	key116 = false;
	ret = file.read(116, &key116, 1);
	if (!bgdlKeyOptionalOrEmpty(ret))
		return ret;

	BgdlTaskPath key220;

	ret = file.read(220, &key220);
	if (ret == 0) {
		if (key220.m_end != key220.m_begin)
			package.key220 = key220.m_begin;
	} else if (ret != (int)BGDL_ERROR_NO_KEY && ret != (int)BGDL_ERROR_INVALID_ARGUMENT) {
		return ret;
	}
	{
		BgdlTaskPath key217;

		ret = file.read(217, &key217);
		if (ret == 0) {
			if (key217.m_end != key217.m_begin)
				package.key217 = key217.m_begin;
		} else if (ret != (int)BGDL_ERROR_NO_KEY && ret != (int)BGDL_ERROR_INVALID_ARGUMENT) {
			return ret;
		}
	}

	result = taskResult;
	m_ParamCache.type = type;
	m_ParamCache.key107 = key107;
	m_ParamCache.runStatus = runStatus;
	m_ParamCache.key102 = key102;
	m_ParamCache.key108 = key108;
	m_ParamCache.state = state;
	m_ParamCache.key110 = key110;
	m_ParamCache.key112 = key112;
	m_ParamCache.kind = kind;
	m_ParamCache.key115 = key115;
	m_ParamCache.key116 = key116;
	m_ParamCache.key113 = key113;
	m_ParamCache.ready = (taskResult | key113) == 0;
	m_ParamCache.key111 = key111;

	if (m_ParamCache.type == 0) {
		SceUInt64 size = 0;
		SceUInt64 current = 0;
		SceUInt64 filesTotal;
		SceUInt64 filesBase;
		int i;

		file.read(208, &current, 8);
		file.read(206, &size, 8);
		status.current = current;
		status.total = size;
		status.id = id;
		transfer.Start(size, current, 0, 512000);

		fileNumber = 0;
		file.read(500, &fileNumber, 4);
		fileCount = -1;
		file.read(501, &fileCount, 4);
		if (fileNumber == 0 || fileCount == -1)
			return 0;
		filesTotal = 0;
		ret = file.read(505, &filesTotal, 8);
		if (ret != 0)
			return ret;
		filesBase = 0;
		for (i = 0; i < fileCount; i++) {
			ret = file.read(5002 + 32 * i, &size, 8);
			if (ret != 0)
				return ret;
			filesBase += size;
		}
		status.fileCount = fileCount + 1;
		status.fileNumber = fileNumber;
		status.filesBase = filesBase;
		status.filesCurrent = filesBase;
		status.filesTotal = filesTotal;
		status.filesRemaining = 0;
		s_manager->SetStatus(&status);

		files.Start(filesTotal, filesBase, 0, 512000);
		status.id = id;
		status.current = current;
		if (transfer.Update(current))
			status.remaining = transfer.GetRemaining();
		if (status.fileNumber >= 2) {
			status.filesCurrent = status.filesBase + current;
			if (files.Update(status.filesCurrent))
				status.filesRemaining = files.GetRemaining();
		}
		s_manager->SetStatus(&status);
	} else if (m_ParamCache.type == 1) {
		MtpBgCopyProgress progress;

		if (bgdlMtpGetCopyProgress(id, &progress) == 0) {
			BgdlTaskCopy current;

			current.name = progress.name;
			current.progress = progress.progress;
			current.number = progress.number;
			current.total = progress.total;
			current.remaining = progress.remaining;
			current.reserved = progress.reserved;
			copyResult = 0;
			copy = current;
		}
	}
	return 0;
}

/* (0x810FFF22) a task read again from its param file; a task more urgent than the running one stops it */
int BgdlManager::ReloadTask(SceUInt32 id)
{
	EseTask **pos;
	EseTask *task;
	int ret;

	m_taskListMutex.Lock();
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_NO_TASK;
	}
	task = *pos;
	if (m_runningTask == task ||
	    (task->m_ParamCache.runStatus == 0 && task->m_ParamCache.key113)) {
		m_taskListMutex.Unlock();
		return 0;
	}
	ret = task->Load();
	if (ret != 0) {
		m_taskListMutex.Unlock();
		return ret;
	}
	if (task->m_ParamCache.runStatus != 0 || task->result != 0) {
		ret = task->setRunStatus(1);
		if (ret != 0) {
			m_taskListMutex.Unlock();
			return ret;
		}
		if (m_runningTask != NULL &&
		    m_runningTask->m_ParamCache.key108 > task->m_ParamCache.key108) {
			m_cancelMutex.Lock();
			if (m_cancelled == false) {
				void (*handler)(void *arg) = m_cancelHandler;

				m_cancelled = true;
				if (handler != NULL)
					handler(m_cancelHandlerArg);
			}
			m_cancelMutex.Unlock();
		}
	} else {
		BgdlEvent event;

		event.type = 3;
		event.id = id;
		event.param = task->m_ParamCache;
		event.result = 0;
		event.reserved = 0;
		event.value = 0;
		NotifyListeners(&event);
	}
	m_taskListCond.Notify();
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x811000D4) */
int BgdlManager::AddTask(SceUInt32 *id, int type, SceUInt32 key111, const char *key105,
			 bool key115, bool key116)
{
	SceUInt32 count;
	SceUInt32 temp;
	SceUInt32 *next;
	int ret;
	int i;

	m_taskListMutex.Lock();
	count = m_tasks.m_end - m_tasks.m_begin;
	if ((key116 == true && count >= BGDL_QUEUE_MAX - 2) || count >= BGDL_QUEUE_MAX) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_QUEUE_FULL;
	}
	m_taskListMutex.Unlock();

	/* the keys are written in a work folder first */
	if (key111 > 1) {
		temp = (SceUInt32)-1;
		if (key111 == 2)
			temp = (SceUInt32)-2;
	} else {
		temp = (SceUInt32)-1;
	}
	if (*id >= (SceUInt32)-2 && temp != *id)
		temp = *id;
	{
		BgdlTaskPath work;

		ret = work.initialize(temp, 20);
		if (ret != 0)
			return ret;
		if (work.Exists())
			Bgdl_811120A0(&work, true);
		ret = Bgdl_811121FE(work.m_begin, 6);
		if (ret != 0)
			return ret;
	}
	{
		EseParam file(temp);
		int runStatus, key107, result, key108, state, kind;
		bool key102, key110, key112, key113;

		ret = file.openWrite();
		if (ret != 0)
			return ret;
		ret = file.write(100, &type, 4);
		if (ret != 0)
			return ret;
		runStatus = 0;
		ret = file.write(101, &runStatus, 4);
		if (ret != 0)
			return ret;
		key102 = false;
		ret = file.write(102, &key102, 1);
		if (ret != 0)
			return ret;
		key107 = 0;
		ret = file.write(107, &key107, 4);
		if (ret != 0)
			return ret;
		result = BGDL_ERROR_NOT_LOADED;
		ret = file.write(104, &result, 4);
		if (ret != 0)
			return ret;
		key108 = 1;
		ret = file.write(108, &key108, 4);
		if (ret != 0)
			return ret;
		state = 0;
		ret = file.write(109, &state, 4);
		if (ret != 0)
			return ret;
		key110 = false;
		ret = file.write(110, &key110, 1);
		if (ret != 0)
			return ret;
		key112 = false;
		ret = file.write(112, &key112, 1);
		if (ret != 0)
			return ret;
		key113 = false;
		ret = file.write(113, &key113, 1);
		if (ret != 0)
			return ret;
		kind = 0;
		ret = file.write(114, &kind, 4);
		if (ret != 0)
			return ret;
		ret = file.write(115, &key115, 1);
		if (ret != 0)
			return ret;
		ret = file.write(116, &key116, 1);
		if (ret != 0)
			return ret;
		ret = file.write(111, &key111, 4);
		if (ret != 0)
			return ret;
		if (key105 != NULL && sce_paf_strlen(key105) != 0) {
			ret = file.write(105, key105, sce_paf_strlen(key105) + 1);
			if (ret != 0)
				return ret;
		}
		ret = file.commit();
		if (ret != 0)
			return ret;
	}

	/* then the folder takes the next free id */
	next = &m_nextId;
	if (temp == (SceUInt32)-2)
		next = &m_nextInternalId;
	++*next;

	BgdlTaskPath folder;

	{
		BgdlTaskPath work;

		ret = work.initialize(temp, 20);
		if (ret != 0)
			return ret;
		for (i = 0; i < 16; i++) {
			char *from;
			char *to;

			ret = folder.initialize(*next, 20);
			if (ret != 0)
				return ret;
			if (folder.Exists())
				Bgdl_811120A0(&folder, true);
			from = work.ClearName();
			to = folder.ClearName();
			ret = LocalFile::RenameFile(from, to);
			if (ret == 0)
				break;
			++*next;
			thread::Sleep(10);
		}
		if (ret != 0)
			return ret;
	}

	EseTask *task = new EseTask(*next);

	if (task == NULL) {
		if (folder.Exists())
			Bgdl_811120A0(&folder, true);
		return BGDL_ERROR_NO_MEMORY;
	}
	ret = task->Load();
	if (ret != 0) {
		if (folder.Exists())
			Bgdl_811120A0(&folder, true);
		delete task;
		return ret;
	}

	BgdlEvent event;

	event.type = 0;
	event.id = *next;
	event.param = task->m_ParamCache;
	event.result = 0;
	event.reserved = 0;
	event.value = 0;
	m_taskListMutex.Lock();
	ret = m_tasks.insert(task);
	if (ret == 0)
		m_taskListCond.Notify();
	m_taskListMutex.Unlock();
	if (ret != 0) {
		if (folder.Exists())
			Bgdl_811120A0(&folder, true);
		delete task;
		return ret;
	}
	NotifyListeners(&event);
	*id = *next;
	return 0;
}

/* (0x8110088A) */
int BgdlManager::CheckStorage(const char *titleId, bool *grw0)
{
	int kind;
	int ret;

	*grw0 = false;
	if (titleId == NULL)
		return BGDL_ERROR_NULL_TITLE;

	LsdbAppInfoPtr info(new LsdbAppInfo());

	if (Bgdl_81123BC0(titleId, &info) != 0)
		return BGDL_ERROR_NO_APP;
	ret = Bgdl_81113898(&info, &kind);
	if (ret != 0)
		return ret;
	if (kind <= 1) {
		if (kind <= 0)
			return BGDL_ERROR_BAD_STORAGE;
		if (m_ux0Ready == true)
			return 0;
		if (Bgdl_81112284("grw0:")) {
			*grw0 = true;
			return 0;
		}
		return BGDL_ERROR_NO_STORAGE;
	}
	if (kind > 4)
		return BGDL_ERROR_BAD_STORAGE;
	if (m_ux0Ready != true)
		return BGDL_ERROR_NO_UX0;
	return 0;
}

/* the game plugin's function 38 (0x8100A00C; PLACEHOLDER: game_plugin.h does not name it yet) */
#define BGDL_GAME_PLUGIN_FUNC_38	38

/* (0x81100B0A) PLACEHOLDER name: 0, or BGDL_ERROR_BUSY while the dispatcher is not idle */
int bgdlDispatcherIdle(void)
{
	BgdlDispatcherState state;

	BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A47C(&state, false);
	if (state.state != 0 || state.state == 3)
		return BGDL_ERROR_BUSY;
	return 0;
}

/* (0x81100B4C) PLACEHOLDER name: the game plugin's function 38 */
int bgdlGamePluginCheck(void)
{
	Plugin *plugin = Plugin::Find("game_plugin");
	int ret;

	if (plugin == NULL) {
		ret = BGDL_ERROR_NO_MEMORY;
	} else {
		GamePluginInterface *game = (GamePluginInterface *)plugin->GetInterface(1);

		ret = BGDL_ERROR_NO_MEMORY;
		if (game != NULL)
			ret = ((int (*)(void))game->func[BGDL_GAME_PLUGIN_FUNC_38])();
	}
	if (ret == (int)0x80010010)
		return BGDL_ERROR_GAME_PLUGIN;
	return ret;
}

/* (0x81100B98) PLACEHOLDER name: the space the request's package files take on the device */
int bgdlPackageSize(const BgdlPackageRequest *request, const BgdlStorageInfo *info,
		    SceUInt64 *size)
{
	common::SharedPtr<File> list;
	int ret = 0;

	*size = 0;
	list = LocalFile::Open(request->listPath, SCE_O_RDONLY | 0x10000, 6, &ret);
	if (ret == 0) {
		BgdlFileCloser closer(list);
		BgdlPackageEntry entry;
		SceUInt32 i;

		for (i = 0; ; i++) {
			sce_paf_memset(&entry, 0, sizeof(entry));
			if (list->Read(&entry, sizeof(entry)) <= 0)
				break;
			*size += Bgdl_81112310(entry.size, info->clusterSize) + info->clusterSize;
		}
		if (request->fileCount > i)
			ret = BGDL_ERROR_LIST_SHORT;
		else
			ret = 0;
	}
	return ret;
}

/* (0x81100E8C) */
int BgdlManager::AddPackageTask(const BgdlPackageRequest *request, BgdlPackageResult *result)
{
	bool grw0 = false;
	bool key102 = false;
	int ret = 0;
	BgdlTaskPath folder;
	SceUInt64 needed = 0;
	SceUInt64 firstSize = 0;
	BgdlTaskPackage package;
	int check = 0;
	SceUInt32 id = (SceUInt32)-1;
	const char *device;

	Bgdl_81113890(1);
	if (request == NULL || result == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
		goto out;
	}
	if (request->titleId[0] == '\0' || sce_paf_strlen(request->titleId) == 0) {
		ret = BGDL_ERROR_NO_TITLE_ID;
		goto out;
	}
	if (request->listPath[0] == '\0') {
		ret = BGDL_ERROR_NULL_TITLE;
		goto out;
	}
	if (LocalFile::GetFileSize(request->listPath) <= 0) {
		ret = BGDL_ERROR_NO_LIST;
		goto out;
	}
	ret = CheckStorage(request->titleId, &grw0);
	if (ret == (int)BGDL_ERROR_NO_APP) {
		if (m_ux0Ready != true)
			goto out;
		ret = 0;
		grw0 = false;
	} else if (ret != 0) {
		goto out;
	}
	{
		BgdlStorageInfo info;

		if (grw0 == true) {
			ret = bgdlDispatcherIdle();
			if (ret != 0)
				goto out;
			device = "grw0:";
		} else {
			device = "ux0:";
		}
		if (grw0 == true) {
			Bgdl_811137F6(false);
			ret = bgdlGamePluginCheck();
			if (ret != 0)
				goto out;
		}
		ret = Bgdl_8111234C(device, &info);
		if (ret != 0)
			goto out;
		ret = bgdlPackageSize(request, &info, &needed);
		if (ret != 0)
			goto out;
		if (info.free < needed) {
			ret = BGDL_ERROR_UX0_FULL;
			if (grw0 == true)
				ret = BGDL_ERROR_GRW0_FULL;
			result->needed = needed;
			goto out;
		}
		id = bgdlWorkId(request->type, true);
		if (grw0 == true) {
			if (id == (SceUInt32)-1)
				id = (SceUInt32)-2;
			if (!Bgdl_81112284("grw0:")) {
				ret = BGDL_ERROR_NO_STORAGE;
				goto out;
			}
			ret = Bgdl_811137F6(true);
			if (ret != 0)
				goto out;
		}
		{
			string name("");

			if (request->name[0] != '\0')
				name = request->name;
			ret = AddTask(&id, 0, request->type, name.c_str(),
				      (request->flags & BGDL_REQUEST_FLAG_KEY115) != 0,
				      (request->flags & BGDL_REQUEST_FLAG_KEY116) != 0);
			if (ret != 0)
				goto done;
			ret = folder.initialize(id, 20);
			if (ret != 0)
				goto done;

			EseParam file(id);
			bool sized, key218, key205;
			int key501, key502, key114;

			ret = file.openWrite();
			if (ret != 0)
				goto done;
			sized = grw0 != true;
			ret = file.write(112, &sized, 1);
			if (ret != 0)
				goto done;
			key218 = request->type == 0;
			ret = file.write(218, &key218, 1);
			if (ret != 0)
				goto done;
			ret = file.write(102, &key102, 1);
			if (ret != 0)
				goto done;
			key205 = false;
			ret = file.write(205, &key205, 1);
			if (ret != 0)
				goto done;
			package.reserved2 = 0;
			package.reserved3 = 0;
			if (request->titleId[0] != '\0' && sce_paf_strlen(request->titleId) != 0) {
				ret = file.write(220, request->titleId, sce_paf_strlen(request->titleId) + 1);
				if (ret != 0)
					goto done;
				package.key220 = request->titleId;
			}
			if (request->contentId[0] != '\0' && sce_paf_strlen(request->contentId) != 0) {
				ret = file.write(217, request->contentId,
						 sce_paf_strlen(request->contentId) + 1);
				if (ret != 0)
					goto done;
				package.key217 = request->contentId;
			}
			if (request->key235[0] != '\0' && sce_paf_strlen(request->key235) != 0) {
				ret = file.write(235, request->key235, sce_paf_strlen(request->key235) + 1);
				if (ret != 0)
					goto done;
			}

			string icon;

			if (request->iconPath[0] != '\0') {
				ret = Bgdl_81111CDC(request->iconPath, folder.m_begin, &icon, true);
				if (ret != 0)
					icon.assign((const char *)NULL, 0);
			}
			if (icon.length() != 0) {
				ret = file.write(106, icon.c_str(), icon.length() + 1);
				if (ret != 0)
					goto done;
			}
			ret = file.write(503, &request->key503, 2);
			if (ret != 0)
				goto done;
			ret = file.write(504, &request->key504, 2);
			if (ret != 0)
				goto done;
			ret = file.write(500, &request->fileCount, 4);
			if (ret != 0)
				goto done;
			key501 = -1;
			ret = file.write(501, &key501, 4);
			if (ret != 0)
				goto done;
			key502 = 0;
			ret = file.write(502, &key502, 4);
			if (ret != 0)
				goto done;
			key114 = request->kind == 1;
			ret = file.write(114, &key114, 4);
			if (ret != 0)
				goto done;

			common::SharedPtr<File> list;

			list = LocalFile::Open(request->listPath, SCE_O_RDONLY | 0x10000, 6, &ret);
			if (ret != 0)
				goto done;

			BgdlFileCloser closer(list);
			char fileName[32] = { 0 };
			char number[8] = { 0 };
			string dest("");
			string gameFolder("");
			SceUInt64 extra = 0;
			SceUInt64 total = 0;
			BgdlGamePluginLock lock(false);
			BgdlPackageEntry entry;
			SceUInt32 i;
			int k;

			if (grw0 == true) {
				dest = "grw0:fgdl";
			} else {
				dest = folder.m_begin;
				extra = 0x2000000;
				if (request->kind == 1) {
					lock.Lock();
					gameFolder = Bgdl_81113C72(id, true);
					if (gameFolder.length() != 0)
						dest = gameFolder.c_str();
					else
						dest = "";
					if (dest.length() == 0) {
						ret = BGDL_ERROR_NO_GAME_FOLDER;
						goto done;
					}
				}
			}
			dest += "/";
			dest += "p";
			k = 0;
			for (i = 0; ; i++) {
				sce_paf_memset(&entry, 0, sizeof(entry));
				if (list->Read(&entry, sizeof(entry)) <= 0)
					break;
				total += entry.size;
				needed += Bgdl_81112310(entry.size + extra, info.clusterSize) +
					  info.clusterSize;
				ret = file.write(5002 + k, &entry.size, 8);
				if (ret != 0)
					break;
				ret = file.write(5000 + k, entry.name, sce_paf_strlen(entry.name) + 1);
				if (ret != 0)
					break;
				ret = file.write(5001 + k, entry.digest, 20);
				if (ret != 0)
					break;
				{
					BgdlPackageCheck packageCheck;
					char digest[32];

					packageCheck.Bgdl_81114AFC(grw0 ? 5 : 6, 2, entry.name, 0, 0, 0, 1, 0);
					check = packageCheck.Bgdl_8118FD0C();
					packageCheck.Bgdl_81115C22(digest);
					ret = file.write(5004 + k, &check, 4);
					if (ret == 0)
						ret = file.write(5005 + k, digest, 32);
					if (ret != 0)
						break;
				}
				sce_paf_snprintf(fileName, 31, "f_%04d_%02d.pkg", i, 0);
				ret = file.write(5003 + k, fileName, sce_paf_strlen(fileName) + 1);
				if (ret != 0)
					break;
				sce_paf_snprintf(number, 7, "%04d", i);
				{
					string path;

					path = dest;
					path += "/";
					path += number;
					if (Dir::IsExist(path.c_str()) == 1)
						Dir::RemoveRecursive(path.c_str());
					ret = Bgdl_811121FE(path.c_str(), 6);
					if (ret != 0)
						break;
					if (sized) {
						path += "/";
						path += fileName;
						Bgdl_8111240E(path.c_str(), entry.size + extra);
					}
					if (i == 0)
						firstSize = entry.size;
				}
				k += 32;
			}
			if (request->fileCount > i) {
				ret = BGDL_ERROR_LIST_SHORT;
				goto done;
			}
			ret = 0;
			if (gameFolder.length() != 0) {
				ret = file.write(229, gameFolder.c_str(),
						 sce_paf_strlen(gameFolder.c_str()) + 1);
				if (ret != 0)
					goto done;
			}
			ret = file.write(206, &firstSize, 8);
			if (ret != 0)
				goto done;

			SceUInt64 current = 0;

			ret = file.write(208, &current, 8);
			if (ret != 0)
				goto done;
			ret = file.write(505, &total, 8);
			if (ret != 0)
				goto done;
			if (grw0 == true) {
				int key108;

				ret = file.write(110, &grw0, 1);
				if (ret != 0)
					goto done;
				key108 = 0;
				ret = file.write(108, &key108, 4);
				if (ret != 0)
					goto done;

				string fgdl("grw0:fgdl");

				ret = file.write(229, fgdl.c_str(), sce_paf_strlen(fgdl.c_str()) + 1);
				if (ret != 0)
					goto done;
			}

			int key107 = 8;

			ret = file.write(107, &key107, 4);
			if (ret != 0)
				goto done;

			EseTask *task = FindTask(id);

			if (task == NULL) {
				ret = BGDL_ERROR_NO_TASK;
				goto done;
			}
			task->m_ParamCache.key107 = key107;
			task->m_ParamCache.key102 = key102;
			task->status.id = task->id;
			task->status.current = current;
			task->status.total = firstSize;
			task->transfer.Start(firstSize, current, 0, 512000);
			s_packageMutex.Lock();
			task->package = package;
			s_packageMutex.Unlock();
			if (grw0 == true)
				task->m_ParamCache.key110 = 1;
			ret = file.commit();
		}
done:
		;
	}
	if (ret == 0 && grw0 == false)
		ret = ReloadTask(id);
	if (ret == 0 && result != NULL)
		result->id = id;
out:
	if (ret != 0) {
		RemoveTask(id);
	} else {
		UpdateQueueStatus();
		if (package.key220.length() != 0)
			Bgdl_812A9954(package.key220.c_str(), 0);
	}
	if (grw0 == true) {
		if (ret != 0)
			Bgdl_811137F6(false);
		else
			ret = 1;
	}
	if (request != NULL && request->listPath[0] != '\0')
		Bgdl_81111724(request->listPath, 0, 29600);
	return ret;
}

/* (0x8110255E) PLACEHOLDER name */
int bgdlAddPackageTask(const BgdlPackageRequest *request, BgdlPackageResult *result)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->AddPackageTask(request, result);
}

/* (0x81102582) PLACEHOLDER name: the work folder of a device's tasks, "ux0:bgdl/w" or "ur0:bgdl/w" */
int bgdlWorkFolder(string *path, int device)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (device > 0 && device <= 2)
		*path = "ux0:bgdl";
	else
		*path = "ur0:bgdl";
	*path += "/";
	*path += "w";
	return 0;
}

/* (0x811026A0) PLACEHOLDER name: a device's patch list in its work folder */
int bgdlPatchFile(string *path, int device)
{
	int ret;

	ret = bgdlWorkFolder(path, device);
	if (ret != 0)
		return ret;
	*path += "/";
	switch (device) {
	case 1:
		*path += "patch_internal.bin";
		break;
	case 2:
		*path += "patch_bg.bin";
		break;
	default:
		*path += "patch.bin";
		break;
	}
	return ret;
}

/* (0x81102868) PLACEHOLDER name: the update check's parameters for a title */
int bgdlUpdateParamInit(const BgdlTaskPackage *title, const bool *disc, LsdbAppInfoPtr *app,
			int mode, BgdlUpdateParam *param)
{
	SceKernelSystemSwVersion version = { 0 };
	const char *appVer;
	const char *titleId;
	const char *contentId;
	int pspVersion;
	int ret;

	if (param == NULL)
		return -1;
	sce_paf_memset(param, 0, sizeof(*param));
	pspVersion = SceCompatForVsh_9C107CC9();
	sce_paf_memset(&version, 0, sizeof(version));
	version.size = sizeof(version);
	ret = sceKernelGetSystemSwVersion(&version);
	if (ret < 0)
		return ret;
	appVer = "01.00";
	titleId = title->key220.c_str();
	contentId = title->key217.c_str();
	if (*disc != true) {
		LsdbAppInfo *info = app->get();

		if (info != NULL) {
			LsdbAppInfo::iterator it = info->find(LSDB_KEY_APP_VER);

			if (it != info->end()) {
				if (it->second.type == LSDB_VALUE_TEXT && it->second.text != NULL)
					appVer = it->second.text->c_str();
				else
					appVer = "";
			}
		}
	}
	if (titleId != NULL)
		sce_paf_strncpy(param->titleId, titleId, 13);
	if (appVer != NULL)
		sce_paf_strncpy(param->appVer, appVer, 6);
	if (contentId != NULL)
		sce_paf_strncpy(param->contentId, contentId, 48);
	switch (mode) {
	case 1:
		param->mode = 2;
		break;
	case 2:
		param->mode = 1;
		break;
	default:
		param->mode = 0;
		break;
	}
	param->systemVersion = version.version;
	param->pspVersion = pspVersion;
	param->malloc = sce_paf_malloc;
	param->free = sce_paf_free;
	return 0;
}

/* (0x811029E6) PLACEHOLDER name: a title's patch looked for, and a task made to get it */
int bgdlRegistPatch(const BgdlTaskPackage *title, const bool *disc, LsdbAppInfoPtr *app, int mode)
{
	int ret = 0;
	BgdlPackageRequest request;
	BgdlPackageResult result;
	BgdlUpdateParam param;
	BgdlUpdateCore core;
	BgdlUpdateState state;
	BgdlUpdateInfo info;

	sce_paf_memset(&request, 0, sizeof(request));
	sce_paf_memset(&result, 0, sizeof(result));
	ret = bgdlUpdateParamInit(title, disc, app, mode, &param);
	if (ret != 0)
		return ret;
	ret = 0;
	{
		int err = Bgdl_812A99AE(&core, 0, &param);

		if (err != 0)
			return err;
	}
	sce_paf_memset(&state, 0, sizeof(state));
	sce_paf_memset(&info, 0, sizeof(info));
	state.size = sizeof(state);
	ret = Bgdl_812A9BB0(&core, &state, &info);
	if (ret >= 0) {
		if (state.state != 2 || (info.flags & 0x80) != 0) {
			if (state.state > 3) {
				if (state.state > 4)
					ret = 1;
				else
					ret = BGDL_ERROR_PATCH_STATE4;
			} else if (state.state <= 2) {
				ret = 1;
			} else {
				ret = BGDL_ERROR_PATCH_STATE3;
			}
		} else {
			string path;
			char listPath[256];

			ret = bgdlPatchFile(&path, 1);
			if (ret != 0)
				return ret;
			sce_paf_memset(listPath, 0, sizeof(listPath));
			sce_paf_strncpy(listPath, path.c_str(), sizeof(listPath));

			common::SharedPtr<File> list = LocalFile::Open(listPath,
				SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC | 0x10000, 6, &ret);

			if (ret == 0) {
				BgdlFileCloser closer(list);
				SceUInt32 i = 0;

				if (info.count != 0) {
					for (i = 0; ; ) {
						BgdlUpdateEntry update;
						BgdlPackageEntry entry;
						int written;

						sce_paf_memset(&update, 0, sizeof(update));
						ret = Bgdl_812A985C(&core, i, &update);
						sce_paf_memset(&entry, 0, sizeof(entry));
						entry.size = update.size;
						sce_paf_memcpy(entry.digest, update.digest, sizeof(entry.digest));
						sce_paf_snprintf(entry.name, 255, "%s", update.name);
						written = list->Write(&entry, sizeof(entry));
						if (written != sizeof(entry)) {
							ret = written;
							break;
						}
						if (++i >= info.count)
							break;
					}
				}
				if (ret == 0) {
					int major = 0;
					int minor = 0;
					int scale;
					int n;
					const char *p;

					request.fileCount = i;
					request.type = 3;
					request.key503 = 0;
					request.key504 = 0;
					p = state.appVer;
					scale = 1;
					n = 0;
					do {
						if (*p == '\0')
							break;
						n++;
						if (*p == '.')
							break;
						major = *p - '0' + major * scale;
						scale *= 10;
						p++;
						request.key503 = major;
					} while (n < 6);
					scale = 1;
					if (n < 6) {
						p = &state.appVer[n];
						do {
							if (*p == '\0' || *p == '.')
								break;
							minor = *p - '0' + minor * scale;
							n++;
							scale *= 10;
							p++;
							request.key504 = minor;
						} while (n < 6);
					}
					request.kind = mode == 2;
					request.flags &= ~(BGDL_REQUEST_FLAG_KEY115 | BGDL_REQUEST_FLAG_KEY116);
					sce_paf_memcpy(request.listPath, listPath, sizeof(request.listPath));
					if (param.titleId[0] != '\0')
						sce_paf_snprintf(request.titleId, 13, "%s", param.titleId);
					if (param.key235[0] != '\0')
						sce_paf_snprintf(request.key235, 13, "%s", param.key235);
					if (param.contentId[0] != '\0')
						sce_paf_snprintf(request.contentId, 51, "%s", param.contentId);
					if (sce_paf_strncmp(title->key220.c_str(), "PCSI00011", 10) == 0)
						sce_paf_snprintf(request.iconPath, 255, "%s",
								 "vs0:vsh/game/psm/runtime_icon.png");
					if (info.name[0] != '\0') {
						char *q = request.name;

						sce_paf_snprintf(request.name, 256, "%s", info.name);
						if (*q != '\0') {
							do {
								if (*q == '\n')
									*q = ' ';
							} while (*++q != '\0');
						}
					}
				}
			}
		}
	}
	Bgdl_812A9708(&core);
	if (ret == 0 && state.state == 2)
		return bgdlAddPackageTask(&request, &result);
	return ret;
}

/* (0x81102FFE) PLACEHOLDER name: an installed title's patch looked for in the background */
int bgdlRegistTitlePatch(const BgdlTaskPackage *title, const bool *disc)
{
	int ret;

	if (!Bgdl_81112284("ux0:"))
		return BGDL_ERROR_NO_UX0;
	if (title->key220.empty())
		return BGDL_ERROR_NULL_TITLE;
	ret = 0;

	LsdbAppInfoPtr info(new LsdbAppInfo());

	sce_paf_strncmp(title->key220.c_str(), "PCSI00011", 10);
	Bgdl_81123BC0(title->key220.c_str(), &info);
	bgdlRegistPatch(title, disc, &info, 1);
	return ret;
}

/* (0x8110313E) */
int Job_EseRegistPatchByTitleID::Run()
{
	bgdlRegistTitlePatch(&m_package, &m_flag);
	return 0;
}

/* (0x81103154) PLACEHOLDER name: bgdlRegistTitlePatch run by the job queue */
int bgdlRegistPatchByTitleID(const BgdlTaskPackage *package, const bool *disc)
{
	Job_EseRegistPatchByTitleID *item = new Job_EseRegistPatchByTitleID(*package, disc);
	common::SharedPtr<job::JobItem> job(item);

	BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A4FC(job);
	return 0;
}

/*
 * (0x81103224) PLACEHOLDER name: the patch lists made, the PSM runtime's server chosen for the
 * console's NP environment and the DRM install heap allocated
 */
void BgdlManager::Bgdl_81103224(bool clean)
{
	char env[17];
	SceKernelAllocMemBlockOpt opt;
	int i;

	Bgdl_811137F6(false);

	paf::string path("ur0:bgdl/w/patch.bin");

	Bgdl_8111240E(path.c_str(), 29600);
	if (m_ux0Ready == true) {
		paf::string ux0Path("ux0:bgdl/w/patch_internal.bin");

		Bgdl_8111240E(ux0Path.c_str(), 29600);
		ux0Path = "ux0:bgdl/w/patch_bg.bin";
		Bgdl_8111240E(ux0Path.c_str(), 29600);
	}
	Bgdl_81122168(clean);
	sce_paf_memset(env, 0, sizeof(env));
	s_psmServer = &s_psmServers[0];
	if (sceNpCommonGetNpEnviroment(env) == 0) {
		/* Sony's test reads the i-th character of the i-th name */
		for (i = 0; i < BGDL_PSM_SERVER_MAX && s_psmServers[i].env[i] != '\0'; i++) {
			if (sce_paf_strncmp(env, s_psmServers[i].env, sizeof(env)) == 0) {
				s_psmServer = &s_psmServers[i];
				break;
			}
		}
	}
	opt.size = sizeof(opt);
	opt.attr = SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_PHYCONT | SCE_KERNEL_ALLOC_MEMBLOCK_ATTR_HAS_ALIGNMENT;
	opt.alignment = 0x1000;
	opt.reserved = 0;
	opt.strBaseBlockName = NULL;
	m_drmHeap = sceKernelAllocMemBlock("SceNpDrmInstallHeap", SCE_KERNEL_MEMBLOCK_TYPE_USER_RW,
					   0x28000, &opt);
	if (m_drmHeap < 0)
		m_drmHeap = -1;
}

/* (0x811033C4) PLACEHOLDER name: the folders of ux0:pspemu/bgdl no task names (key 229) removed */
void BgdlManager::Bgdl_811033C4(void)
{
	paf::Dir dir;
	SceUInt32 i;

	if (dir.Open("ux0:pspemu/bgdl") != 0)
		return;

	paf::vector<paf::string> orphans;

	for (;;) {
		paf::DirEnt ent;

		if (dir.Read(&ent) != 0 || ent.name.length() == 0)
			break;
		if (sce_paf_strcmp(ent.name.c_str(), ".") == 0 ||
		    sce_paf_strcmp(ent.name.c_str(), "..") == 0)
			continue;

		paf::string path("ux0:pspemu/bgdl");
		bool found = false;

		path.append("/" + ent.name);
		for (EseTask **pos = m_tasks.m_begin; pos != m_tasks.m_end; pos++) {
			paf::string folder("");

			if (*pos != NULL && Bgdl_8112278A((*pos)->id, 229, &folder) == 0 &&
			    path == folder) {
				found = true;
				break;
			}
		}
		if (!found)
			orphans.push_back(paf::string(path.c_str()));
	}
	if (orphans.size() != 0) {
		BgdlGamePluginLock lock(true);

		for (i = 0; i < orphans.size(); i++)
			Bgdl_81112112(&orphans[i], true);
	}
}

/* (0x81103722) the run status and the result kept and written (keys 101 and 104) */
int EseTask::setResult(int status, int taskResult)
{
	BgdlEvent event;
	int ret;

	m_ParamCache.runStatus = status;
	result = taskResult;
	{
		EseParam param(id);

		ret = param.openWrite();
		if (ret != 0)
			return ret;
		ret = param.write(101, &status, sizeof(status));
		if (ret != 0)
			return ret;
		ret = param.write(104, &taskResult, sizeof(taskResult));
		if (ret != 0)
			return ret;
		ret = param.commit();
		if (ret != 0)
			return ret;
	}
	event.type = 4;
	event.id = id;
	event.param = m_ParamCache;
	event.result = result;
	event.reserved = 0;
	event.value = value;
	s_manager->NotifyListeners(&event);
	return 0;
}

/*
 * (0x81103834) a device's folders made ("bgdl/w", "bgdl/t"; all removed first when clean), its
 * tasks loaded (32 at most) and its next task id set
 */
int BgdlManager::prepare_device(const char *device, bool clean, int quota)
{
	SceUInt32 maxId;
	int workId;
	int ret;
	int i;

	if (device == NULL)
		return BGDL_ERROR_INVALID_ARGUMENT;

	BgdlTaskPath taskPath;
	string path;

	{
		BgdlScopeTimer timer("prepare_device");

		workId = -1;
		if (sce_paf_strcmp(device, "ur0:") == 0)
			workId = -2;
		path = device;
		path += "bgdl";
		if (clean == true) {
			ret = Dir::RemoveRecursive(path.c_str());
			if (ret != 0)
				return ret;
		}
		path = device;
		path += "bgdl/w";
		if (BgdlTaskPath::Exists(path.c_str())) {
			if (taskPath.initialize(device, workId, true) == 0 && taskPath.Exists())
				Bgdl_811120A0(&taskPath, true);
		} else {
			ret = Bgdl_811121FE(path.c_str(), 6);
			if (ret != 0)
				return ret;
		}
		path = device;
		path += "bgdl/t";
		if (BgdlTaskPath::Exists(path.c_str())) {
			if (taskPath.initialize(device, workId, false) == 0 && taskPath.Exists())
				Bgdl_811120A0(&taskPath, true);
		} else {
			ret = Bgdl_811121FE(path.c_str(), 6);
			if (ret != 0)
				return ret;
		}
	}
	{
		BgdlScopeTimer timer("prepare_device");
		const char *folder;
		Dir dir;

		maxId = 0;
		if (workId == -2)
			maxId = 0x80000000;
		folder = path.c_str();
		if (dir.Open(folder) == 0) {
			for (;;) {
				DirEnt ent;
				const char *name;
				SceUInt32 id;
				EseTask *task;

				if (dir.Read(&ent) != 0 || ent.name.length() == 0)
					break;
				if (sce_paf_strcmp(ent.name.c_str(), ".") == 0 ||
				    sce_paf_strcmp(ent.name.c_str(), "..") == 0)
					continue;
				/* a task's folder is its id, 8 hex digits */
				name = ent.name.c_str();
				if (name == NULL || sce_paf_strlen(name) != 8)
					continue;
				for (i = 0; i < 8; i++) {
					if ((sce_paf_look_ctype_table(name[i]) & 0x44) == 0)
						break;
				}
				if (i < 8)
					continue;
				id = sce_paf_strtoul(name, NULL, 16);
				if (maxId < id)
					maxId = id;
				task = new EseTask(id);
				if (task == NULL)
					break;
				if (task->Load() != 0) {
					delete task;
					Bgdl_8111212C(folder, ent.name.c_str());
					continue;
				}
				if (task->m_ParamCache.runStatus == 2 &&
				    task->setResult(0, BGDL_ERROR_INTERRUPTED) != 0) {
					delete task;
					continue;
				}
				if (task->m_ParamCache.key110) {
					delete task;
					Bgdl_8111212C(folder, ent.name.c_str());
					Bgdl_811137F6(false);
					continue;
				}
				if (m_tasks.insert(task) != 0) {
					delete task;
					Bgdl_8111212C(folder, ent.name.c_str());
					continue;
				}
				if ((SceSize)(m_tasks.m_end - m_tasks.m_begin) >= 32)
					break;
			}
		}
	}
	if (workId == -2)
		m_nextInternalId = maxId;
	else
		m_nextId = maxId;
	if (!clean) {
		if (workId == -2) {
			if (quota != 0 && quota != -2 && m_nextInternalId < (SceUInt32)quota)
				m_nextInternalId = quota;
		} else if (quota != 0 && quota != -1 && m_nextId < (SceUInt32)quota) {
			m_nextId = quota;
		}
	}
	return 0;
}

/* (0x8110401E) */
BgdlManager::BgdlManager()
	: m_thread(NULL),
	  m_taskListMutex("SceBgdlMutexEseTaskList", 0),
	  m_taskListCond(m_taskListMutex, "SceBgdlCondEseTaskList", 0),
	  m_cancelMutex("SceBgdlMutexEseCancel", 0),
	  m_listenersMutex("SceBgdlMutexEseListeners", 0),
	  m_syncHandlerMutex("SceBgdlRMutexEseSyncHandler", 0),
	  m_progressMutex("SceBgdlRMutexProgerss", 0),
	  m_error(0x80103A15),
	  m_stopping(false),
	  m_removeRunning(false),
	  m_cancelled(false),
	  m_cancelHandler(NULL),
	  m_cancelHandlerArg(NULL),
	  m_runningTask(NULL),
	  m_lastId(0),
	  m_nextId(0),
	  m_nextInternalId(0x80000000),
	  m_flags(0),
	  m_ux0Ready(true),
	  m_hold(false),
	  m_resumed(false),
	  m_suspended(false),
	  m_copyResult(0),
	  m_hasPending(false),
	  m_listenerCount(0)
{
	sce_paf_memset(m_listeners, 0, sizeof(m_listeners));
	sce_paf_memset(reserved9, 0, sizeof(reserved9));
	sce_paf_memset(reserved10, 0, sizeof(reserved10));
	m_ux0Ready = Bgdl_81112284("ux0:");
	SceShell_7FE4E732(SHELL_EVENT_POWER, bgdlManagerOnResume, this);
	sce_paf_memset(m_queue, 0, sizeof(m_queue));
}

/* (0x811041EC) the manager made once: ur0:'s tasks, then ux0:'s when it is there */
void bgdlManagerCreate(bool clean, const int *quota)
{
	s_updateMutex.Lock();
	if (s_manager == NULL) {
		BgdlManager *manager = new BgdlManager;
		int ux0Quota;
		int ur0Quota;
		BgdlLicenseFolder license;

		s_manager = manager;
		ux0Quota = -1;
		ur0Quota = -2;
		if (quota != NULL) {
			ux0Quota = quota[0];
			ur0Quota = quota[1];
		}
		license.Bgdl_81110808();
		license.Bgdl_8111081C();
		license.Bgdl_8111080E();
		manager->prepare_device("ur0:", clean, ur0Quota);
		if (manager->m_ux0Ready == true && manager->prepare_device("ux0:", clean, ux0Quota) == 0)
			manager->Bgdl_811033C4();
		manager->Bgdl_81103224(clean);
	}
	s_updateMutex.Unlock();
}

/* (0x811042C0) the manager's thread started */
void startMng(void)
{
	BgdlManager *manager;

	s_updateMutex.Lock();
	manager = s_manager;
	if (manager != NULL && manager->m_thread == NULL) {
		BgdlManagerThread *thread = new BgdlManagerThread;

		manager->m_thread = thread;
		if (thread != NULL) {
			thread->m_manager = manager;
			thread->Start();
			manager->m_error = 0;
		} else {
			manager->m_error = BGDL_ERROR_STOPPED;
		}
	}
	s_updateMutex.Unlock();
}

/* (0x81104346) every task's folders removed, while no manager is made */
bool bgdlRemoveFolders(void)
{
	if (s_manager != NULL)
		return false;
	Dir::RemoveRecursive("ux0:bgdl");
	Dir::RemoveRecursive("ur0:bgdl");
	return true;
}

/* (0x81104374) */
BgdlManager::~BgdlManager()
{
	SceShell_D4C7D2B7(SHELL_EVENT_POWER, bgdlManagerOnResume, this);
	m_tasks.Clear();
}

/* (0x811043F4) the running task cancelled, the thread told to end and waited for */
int BgdlManager::Stop(void)
{
	if (m_error != 0)
		return m_error;
	if (m_thread == NULL)
		return 0;
	m_taskListMutex.Lock();
	if (m_runningTask != NULL) {
		m_cancelMutex.Lock();
		if (!m_cancelled) {
			m_cancelled = true;
			if (m_cancelHandler != NULL)
				m_cancelHandler(m_cancelHandlerArg);
		}
		m_cancelMutex.Unlock();
	}
	m_stopping = true;
	m_taskListCond.Notify();
	m_taskListMutex.Unlock();
	m_thread->Join();
	delete m_thread;
	m_thread = NULL;
	m_error = BGDL_ERROR_STOPPED;
	return BGDL_ERROR_STOPPED;
}

/* (0x81104474) */
void bgdlManagerDestroy(void)
{
	s_updateMutex.Lock();
	if (s_manager != NULL) {
		s_manager->Stop();
		delete s_manager;
		s_manager = NULL;
	}
	s_updateMutex.Unlock();
}

/* (0x811044BC) the thread woken; *request the request it is to take */
void bgdlManagerWakeup(SceUInt64 *request)
{
	BgdlManager *manager;
	SceUInt64 count;

	s_updateMutex.Lock();
	manager = s_manager;
	if (manager != NULL) {
		manager->m_taskListMutex.Lock();
		count = ++s_requested;
		if (request != NULL)
			*request = count;
		if (manager->m_runningTask != NULL) {
			manager->m_cancelMutex.Lock();
			if (!manager->m_cancelled) {
				manager->m_cancelled = true;
				if (manager->m_cancelHandler != NULL)
					manager->m_cancelHandler(manager->m_cancelHandlerArg);
			}
			manager->m_cancelMutex.Unlock();
		}
		manager->m_taskListCond.Notify();
		manager->m_taskListMutex.Unlock();
	} else {
		count = ++s_requested;
		if (request != NULL)
			*request = count;
	}
	s_updateMutex.Unlock();
}

/* (0x81104574) UpdateFlags run by the job queue */
void bgdlSetPermission(SceUInt32 value, SceUInt32 mask)
{
	common::SharedPtr<job::JobItem> job(new Job_EseSetPermission(value, mask));

	BgdlJobQueue::Bgdl_81123E48()->Bgdl_81123E3C(job);
}

/* (0x811046BE) PLACEHOLDER name: the server's answer copied for the caller */
static __attribute__((noinline)) int bgdlCopyServerInfo(int result, const BgdlServerInfo *server,
							BgdlContentInfo *info)
{
	const BgdlKindMap *map;

	if (server == NULL || info == NULL)
		return BGDL_ERROR_NULL_TITLE;
	info->value1 = server->value1;
	info->value0 = server->value0;
	info->status = server->status;
	info->httpStatus = server->httpStatus;
	info->value2 = server->value2;
	info->value3 = server->value3;
	if (result == 0) {
		info->size = server->size;
		if (server->url.length() != 0)
			sce_paf_snprintf(info->url, sizeof(info->url) - 1, "%s", server->url.c_str());
		if (server->titleId.length() != 0)
			sce_paf_snprintf(info->titleId, sizeof(info->titleId) - 1, "%s",
					 server->titleId.c_str());
		if (server->contentId.length() != 0)
			sce_paf_snprintf(info->contentId, sizeof(info->contentId) - 1, "%s",
					 server->contentId.c_str());
		info->value4 = server->value4;
		info->value3 = server->value3;
		info->flag = server->flag;
		info->kind = 0;
		for (map = s_kindMap; map->server != 0; map++) {
			if (server->kind == map->server) {
				info->kind = map->kind;
				break;
			}
		}
		sce_paf_memset(info->data, 0, sizeof(info->data));
		sce_paf_memcpy(info->data, &server->data, sizeof(server->data));
	} else if (result == BGDL_ERROR_SERVER && server->httpStatus == 401 &&
		   server->message.length() != 0) {
		sce_paf_snprintf(info->message, sizeof(info->message) - 1, "%s",
				 server->message.c_str());
	}
	return result;
}

/* (0x811047E4) PLACEHOLDER name: a content asked of the server */
int BgdlManager::Bgdl_811047E4(const BgdlServerQuery *query, const BgdlServerIds *ids,
			       BgdlContentInfo *info, const bool *cancel)
{
	int ret;

	if (query == NULL || ids == NULL || info == NULL)
		return BGDL_ERROR_NULL_TITLE;

	BgdlServerParam param = query->param;
	BgdlServerInfo server;
	BgdlCancelHost host(cancel);

	ret = Bgdl_8111B72A(query->handle, 0, "ux0:", query->url, ids->id0, ids->id1, ids->id2, 0,
			    &server, &param, &host, 0, 0);
	return bgdlCopyServerInfo(ret, &server, info);
}

/* (0x811049A4) PLACEHOLDER name */
int bgdlServerQuery(const BgdlServerQuery *query, const BgdlServerIds *ids, BgdlContentInfo *info,
		    const bool *cancel)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->Bgdl_811047E4(query, ids, info, cancel);
}

/* (0x811049D4) a task made from a download request, its keys written, and started */
int BgdlManager::AddDownloadTask(SceUInt32 device, const BgdlDownloadRequest *request,
				 SceUInt32 *id)
{
	BgdlTaskPath folder;
	SceUInt32 taskId;
	int ret;

	Bgdl_81113890(1);
	taskId = (SceUInt32)-1;
	if (device > 1 && device == 2)
		taskId = (SceUInt32)-2;
	if (taskId == (SceUInt32)-1 && m_ux0Ready != true) {
		ret = BGDL_ERROR_NO_UX0;
		goto out;
	}
	if (request == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
		goto out;
	}
	if (request->url[0] == '\0') {
		ret = BGDL_ERROR_NO_URL;
		goto out;
	}
	{
		string url(request->url);

		if (url.length() == 0) {
			ret = BGDL_ERROR_NO_URL;
			goto out;
		}

		string name("");

		if (request->name[0] != '\0')
			name = request->name;
		ret = AddTask(&taskId, 0, device, name.c_str(),
			      (request->flags & BGDL_REQUEST_FLAG_KEY115) != 0,
			      (request->flags & BGDL_REQUEST_FLAG_KEY116) != 0);
		if (ret != 0)
			goto out;
		ret = folder.initialize(taskId, 20);
		if (ret != 0)
			goto out;
		{
			EseParam param(taskId);
			bool flag;

			ret = param.openWrite();
			if (ret != 0)
				goto out;
			ret = param.write(202, url.c_str(), url.length() + 1);
			if (ret != 0)
				goto out;
			if (request->iconPath[0] != '\0') {
				string icon("");

				if (Bgdl_81111CDC(request->iconPath, folder.m_begin, &icon, true) == 0) {
					ret = param.write(106, icon.c_str(), icon.length() + 1);
					if (ret != 0)
						goto out;
				}
			}
			if (request->key230[0] != '\0' && sce_paf_strlen(request->key230) != 0) {
				string file("");

				ret = Bgdl_81111CDC(request->key230, folder.m_begin, &file, true);
				if (ret != 0)
					goto out;
				ret = param.write(230, file.c_str(), file.length() + 1);
				if (ret != 0)
					goto out;
			}
			if (request->contentId[0] != '\0' && sce_paf_strlen(request->contentId) != 0) {
				ret = param.write(217, request->contentId,
						  sce_paf_strlen(request->contentId) + 1);
				if (ret != 0)
					goto out;
			}
			if (request->key222[0] != '\0' && sce_paf_strlen(request->key222) != 0) {
				ret = param.write(222, request->key222, sce_paf_strlen(request->key222) + 1);
				if (ret != 0)
					goto out;
			}
			if (request->key221[0] != '\0' && sce_paf_strlen(request->key221) != 0) {
				ret = param.write(221, request->key221, sce_paf_strlen(request->key221) + 1);
				if (ret != 0)
					goto out;
			}
			flag = device == 0;
			ret = param.write(218, &flag, sizeof(flag));
			if (ret != 0)
				goto out;
			flag = false;
			ret = param.write(102, &flag, sizeof(flag));
			if (ret != 0)
				goto out;
			if (request->key229[0] != '\0' && sce_paf_strlen(request->key229) != 0) {
				ret = param.write(229, request->key229, sce_paf_strlen(request->key229) + 1);
				if (ret != 0)
					goto out;
			}
			if (request->key203[0] != '\0' && sce_paf_strlen(request->key203) != 0 &&
			    device == 2) {
				ret = param.write(203, request->key203, sce_paf_strlen(request->key203) + 1);
				if (ret != 0)
					goto out;
			}
			ret = param.write(206, &request->key206, sizeof(request->key206));
			if (ret != 0)
				goto out;
			ret = param.write(208, &request->key208, sizeof(request->key208));
			if (ret != 0)
				goto out;
			if (device == 2) {
				BgdlPackageCheck check;
				char digest[32];
				int result = 0;

				check.Bgdl_81114AFC(6, 2, request->url, 0, 0, 0, 1, 0);
				result = check.Bgdl_8118FD0C();
				check.Bgdl_81115C22(digest);
				param.write(236, &result, sizeof(result));
				param.write(237, digest, sizeof(digest));
			}
			ret = param.commit();
			if (ret != 0)
				goto out;
		}
		if ((request->flags & BGDL_REQUEST_FLAG_NO_START) == 0)
			ret = ReloadTask(taskId);
		if (ret == 0)
			*id = taskId;
	}
out:
	if (ret != 0)
		RemoveTask(taskId);
	UpdateQueueStatus();
	return ret;
}

/* (0x81105306) PLACEHOLDER name */
int bgdlAddDownloadTask(SceUInt32 device, const BgdlDownloadRequest *request, SceUInt32 *id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->AddDownloadTask(device, request, id);
}

/* (0x8110532E) a task made for a content the server described: its files made their sizes, its
 * keys written, and started */
int BgdlManager::AddContentTask(const BgdlContentRequest *request, const BgdlContentAccount *account,
				const BgdlContentInfo *info, SceUInt32 *id)
{
	BgdlTaskPath folder;
	SceUInt32 taskId;
	int ret;

	Bgdl_81113890(1);
	if (request == NULL || account == NULL || info == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
		goto out;
	}
	taskId = (SceUInt32)-1;
	if (request->device != 0 && request->device != 1 && request->device == 2)
		taskId = (SceUInt32)-2;
	if (taskId == (SceUInt32)-1 && m_ux0Ready != true) {
		ret = BGDL_ERROR_NO_UX0;
		goto out;
	}
	if (request->url[0] == '\0') {
		ret = BGDL_ERROR_NO_URL;
		goto out;
	}
	{
		string url(request->url);

		if (url.length() == 0) {
			ret = BGDL_ERROR_NO_URL;
			goto out;
		}
		if (info->flag != true) {
			ret = info->status;
			goto out;
		}
		if (info->value2 < info->value3) {
			ret = BGDL_ERROR_UX0_FULL;
			goto out;
		}

		string name("");

		if (request->name[0] != '\0')
			name = request->name;
		ret = AddTask(&taskId, 0, request->device, name.c_str(),
			      (request->flags & BGDL_REQUEST_FLAG_KEY115) != 0,
			      (request->flags & BGDL_REQUEST_FLAG_KEY116) != 0);
		if (ret != 0)
			goto out;
		ret = folder.initialize(taskId, 20);
		if (ret != 0)
			goto out;
		if (info->url[0] == '\0' || sce_paf_strlen(info->url) == 0) {
			ret = BGDL_ERROR_NO_FILE;
			goto out;
		}
		if (folder.m_begin == NULL) {
			ret = BGDL_ERROR_NO_GAME_FOLDER;
			goto out;
		}
		{
			string file(folder.m_begin);

			file += "/";
			file += info->url;
			if (Bgdl_8111240E(file.c_str(), info->size) != 0) {
				ret = BGDL_ERROR_NO_GAME_FOLDER;
				goto out;
			}

			string contents(folder.m_begin);

			contents += "/";
			contents += "av_contents.bin";
			if (Bgdl_8111240E(contents.c_str(), 0x200000) != 0) {
				ret = BGDL_ERROR_NO_GAME_FOLDER;
				goto out;
			}
		}
		{
			EseParam param(taskId);
			const BgdlKindMap *map;
			bool key112, key218, key205, key102;
			SceUInt64 current;
			int kind;

			ret = param.openWrite();
			if (ret != 0)
				goto out;
			key112 = true;
			ret = param.write(112, &key112, sizeof(key112));
			if (ret != 0)
				goto out;
			ret = param.write(202, url.c_str(), url.length() + 1);
			if (ret != 0)
				goto out;
			if (account->user[0] != '\0' && sce_paf_strlen(account->user) != 0) {
				string user("");

				ret = Bgdl_811221E0(account->user, 31, &user);
				if (ret != 0)
					goto out;
				ret = param.write(200, user.c_str(), user.length() + 1);
				if (ret != 0)
					goto out;
			}
			if (account->password[0] != '\0' && sce_paf_strlen(account->password) != 0) {
				string password("");

				ret = Bgdl_811221E0(account->password, 31, &password);
				if (ret != 0)
					goto out;
				ret = param.write(201, password.c_str(), password.length() + 1);
				if (ret != 0)
					goto out;
			}
			if (account->key209[0] != '\0' && sce_paf_strlen(account->key209) != 0) {
				ret = param.write(209, account->key209, sce_paf_strlen(account->key209) + 1);
				if (ret != 0)
					goto out;
			}
			if (request->iconPath[0] != '\0') {
				string icon("");

				if (Bgdl_81111CDC(request->iconPath, folder.m_begin, &icon, true) == 0) {
					ret = param.write(106, icon.c_str(), icon.length() + 1);
					if (ret != 0)
						goto out;
				}
			}
			key218 = request->device == 0;
			ret = param.write(218, &key218, sizeof(key218));
			if (ret != 0)
				goto out;
			if (info->url[0] == '\0' || sce_paf_strlen(info->url) == 0) {
				ret = BGDL_ERROR_NO_FILE;
				goto out;
			}
			ret = param.write(203, info->url, sce_paf_strlen(info->url) + 1);
			if (ret != 0)
				goto out;
			if (info->size == 0 || info->size == (SceUInt64)-1) {
				ret = BGDL_ERROR_NO_SIZE;
				goto out;
			}
			ret = param.write(206, &info->size, sizeof(info->size));
			if (ret != 0)
				goto out;
			current = 0;
			ret = param.write(208, &current, sizeof(current));
			if (ret != 0)
				goto out;
			if (info->contentId[0] != '\0' && sce_paf_strlen(info->contentId) != 0) {
				ret = param.write(204, info->contentId, sce_paf_strlen(info->contentId) + 1);
				if (ret != 0)
					goto out;
			}
			kind = 0;
			for (map = s_kindMap; map->kind != 0; map++) {
				if (info->kind == map->kind) {
					kind = map->server;
					break;
				}
			}
			if (kind < 1 || kind > 3)
				ret = BGDL_ERROR_BAD_KIND;
			if (ret != 0)
				goto out;
			key205 = true;
			ret = param.write(205, &key205, sizeof(key205));
			if (ret != 0)
				goto out;
			key102 = true;
			ret = param.write(102, &key102, sizeof(key102));
			if (ret != 0)
				goto out;
			ret = param.write(107, &kind, sizeof(kind));
			if (ret != 0)
				goto out;

			EseTask *task = FindTask(taskId);

			if (task == NULL) {
				ret = BGDL_ERROR_NO_TASK;
				goto out;
			}
			task->m_ParamCache.key107 = kind;
			task->m_ParamCache.key102 = key102;
			task->status.id = task->id;
			task->status.current = current;
			task->status.total = info->size;
			task->transfer.Start(info->size, current, 0, 512000);
			ret = param.commit();
			if (ret != 0)
				goto out;
		}
		if ((request->flags & BGDL_REQUEST_FLAG_NO_START) == 0)
			ret = ReloadTask(taskId);
		if (ret == 0)
			*id = taskId;
	}
out:
	if (ret != 0 && (taskId != (SceUInt32)-1 || taskId != (SceUInt32)-2))
		RemoveTask(taskId);
	return ret;
}

/* (0x81106316) PLACEHOLDER name */
int bgdlAddContentTask(const BgdlContentRequest *request, const BgdlContentAccount *account,
		       const BgdlContentInfo *info, SceUInt32 *id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->AddContentTask(request, account, info, id);
}

/* (0x81106346) PLACEHOLDER name: 0 while the list has room for another task */
int bgdlCheckQueue(void)
{
	BgdlManager *manager;

	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	manager = s_manager;
	manager->m_taskListMutex.Lock();
	if ((SceSize)((char *)manager->m_tasks.m_end - (char *)manager->m_tasks.m_begin) /
	    sizeof(EseTask *) >= 32) {
		manager->m_taskListMutex.Unlock();
		return BGDL_ERROR_QUEUE_FULL;
	}
	manager->m_taskListMutex.Unlock();
	return 0;
}

/* (0x81106390) PLACEHOLDER name: the state of PCSI00011's task of kind 6, *id its id; 0x8000
 * when there is none */
int BgdlManager::Bgdl_81106390(SceUInt32 *id)
{
	BgdlTaskPackage package;
	EseTask **it;
	int ret = 0x8000;

	for (it = m_tasks.m_begin; it != m_tasks.m_end; it++) {
		EseTask *task = *it;

		s_packageMutex.Lock();
		package = task->package;
		s_packageMutex.Unlock();
		if (package.key220.length() != 0 &&
		    sce_paf_strncmp(package.key220.c_str(), "PCSI00011", 10) == 0 && *it != NULL &&
		    (*it)->m_ParamCache.key107 == 6) {
			int runStatus = (*it)->m_ParamCache.runStatus;
			int result;

			*id = (*it)->id;
			result = (*it)->result;
			if (runStatus == 2)
				ret = 3;
			else if (runStatus == 1)
				ret = 2;
			else if ((*it)->m_ParamCache.runStatus == 0 && (*it)->m_ParamCache.key113 &&
				 result == 0)
				ret = 4;
			else
				ret = result == 0 || result == BGDL_ERROR_COPY_FAILED ||
				      result == BGDL_ERROR_CANCELLED;
			break;
		}
	}
	return ret;
}

/* (0x8110652E) PLACEHOLDER name: 0 when ux0: has the room for the title's patch, 1 when there is
 * no patch to get */
int bgdlCheckPatchSpace(const BgdlTaskPackage *title, const bool *disc, LsdbAppInfoPtr *app,
			int mode)
{
	BgdlStorageInfo storage;
	BgdlUpdateParam param;
	BgdlUpdateCore core;
	BgdlUpdateState state;
	BgdlUpdateInfo info;
	SceUInt64 needed;
	int ret;

	if (!Bgdl_81112284("ux0:"))
		return BGDL_ERROR_NO_UX0;
	ret = bgdlCheckQueue();
	if (ret != 0)
		return ret;
	ret = Bgdl_8111234C("ux0:", &storage);
	if (ret != 0)
		return ret;
	needed = 6 * storage.clusterSize;
	if (storage.free < needed)
		return BGDL_ERROR_UX0_FULL;
	ret = bgdlUpdateParamInit(title, disc, app, mode, &param);
	if (ret != 0)
		return ret;
	param.option = 1;
	ret = Bgdl_812A99AE(&core, 0, &param);
	if (ret != 0)
		return ret;
	sce_paf_memset(&state, 0, sizeof(state));
	sce_paf_memset(&info, 0, sizeof(info));
	state.size = sizeof(state);
	ret = Bgdl_812A9BB0(&core, &state, &info);
	if (ret >= 0) {
		if (state.state != 2 || (info.flags & 0x80) != 0) {
			if (state.state > 3) {
				if (state.state > 4)
					ret = 1;
				else
					ret = BGDL_ERROR_PATCH_STATE4;
			} else if (state.state <= 2) {
				ret = 1;
			} else {
				ret = BGDL_ERROR_PATCH_STATE3;
			}
		} else {
			SceUInt32 i;

			for (i = 0; i < info.count; i++) {
				BgdlUpdateEntry entry;

				sce_paf_memset(&entry, 0, sizeof(entry));
				ret = Bgdl_812A985C(&core, i, &entry);
				if (ret != 0)
					break;
				needed += Bgdl_81112310(entry.size, storage.clusterSize) + storage.clusterSize;
			}
		}
	}
	Bgdl_812A9708(&core);
	if (ret == 0) {
		if (storage.free < needed)
			return BGDL_ERROR_UX0_FULL;
		if (state.state == 2)
			return 0;
	}
	return ret;
}

/* (0x8110671E) the PSM runtime (PCSI00011): its patch looked for when it is installed, else its
 * package's download task made, with the signature and the icon got from the console's PSM
 * server first */
int BgdlManager::AddPsmRuntimeTask(string *iconPath, int reserved0, int reserved1,
				   SceUInt64 *needed)
{
	SceUInt64 size;
	SceUInt32 taskId;
	int ret;

	if (m_ux0Ready != true)
		return BGDL_ERROR_NO_UX0;
	size = 0;
	taskId = (SceUInt32)-1;
	{
		LsdbAppInfoPtr app(new LsdbAppInfo());
		int state;

		if (Bgdl_81123BC0("PCSI00011", &app) == 0)
			goto patch;
		state = Bgdl_81106390(&taskId);
		if (state != 0x8000) {
			if (state != 0)
				goto patch;
			RemoveTask(taskId);
		}

		BgdlDownloadRequest request;
		BgdlContentInfo info;

		sce_paf_memset(&request, 0, sizeof(request));

		string dir("ux0:");

		dir += "bgdl/w";
		if (!Dir::IsExist(dir.c_str())) {
			ret = Dir::CreateRecursive(dir.c_str(), 6);
			if (ret != 0)
				goto done;
		}
		{
			string url(s_psmServer->url);

			url += "IP9100-PCSI00011_00-PSMRUNTIME000000.sig";

			string folder(dir);
			string path;
			BgdlServerInfo server;
			bool cancel = false;
			int status = 0;
			BgdlCancelHost host(&cancel);

			ret = Bgdl_8111FEF8("ux0:", url.c_str(), 0x1000000, 0, &host, 0, folder.c_str(), 0,
					    &status, &server, &path);
			if (ret != 0)
				goto done;
			request.key230[0] = '\0';
			sce_paf_snprintf(request.key230, 127, "%s", path.c_str());
		}
		request.iconPath[0] = '\0';
		sce_paf_snprintf(request.iconPath, 255, "%s", "vs0:vsh/game/psm/runtime_icon.png");
		{
			string url(s_psmServer->url);

			url += "IP9100-PCSI00011_00-PSMRUNTIME000000.png";

			string path;
			string folder(dir);
			BgdlServerInfo server;
			bool cancel = false;
			int status = 0;
			BgdlCancelHost host(&cancel);

			if (Bgdl_8111FEF8("ux0:", url.c_str(), 0x1000000, 0, &host, 0, folder.c_str(), 0,
					  &status, &server, &path) == 0) {
				request.iconPath[0] = '\0';
				sce_paf_snprintf(request.iconPath, 255, "%s", path.c_str());
			}
		}

		string url(s_psmServer->url);

		url += "IP9100-PCSI00011_00-PSMRUNTIME000000.pkg";
		request.url[0] = '\0';
		sce_paf_snprintf(request.url, 2047, "%s", url.c_str());

		wstring title(L"");
		string name("");
		Plugin *plugin = Plugin::Find("download_plugin");

		if (plugin != NULL) {
			title = plugin->GetString(IDParam(DOWNLOAD_PLUGIN_STR_PSM_RUNTIME));
			if (title.length() != 0) {
				string utf8;

				common::Utf16ToUtf8(title, &utf8);
				name = utf8;
			}
		}
		request.name[0] = '\0';
		if (name.length() != 0)
			sce_paf_snprintf(request.name, 256, "%s", name.c_str());
		else
			sce_paf_snprintf(request.name, 256, "%s", "PSM Runtime Package");
		request.titleId[0] = '\0';
		sce_paf_snprintf(request.titleId, 16, "%s", "PCSI00011");
		request.contentId[0] = '\0';
		sce_paf_snprintf(request.contentId, 51, "%s", "IP9100-PCSI00011_00-PSMRUNTIME000000");
		request.type = 0;
		sce_paf_memset(&info, 0, sizeof(info));
		request.platform = 0;
		ret = Bgdl_81107BDE(&request, &info, &taskId);
		if (ret == BGDL_ERROR_UX0_FULL && needed != NULL)
			*needed = size;
		if (request.key230[0] != '\0')
			LocalFile::RemoveFile(request.key230);
		if (request.iconPath[0] != '\0' &&
		    sce_paf_strcmp(request.iconPath, "vs0:vsh/game/psm/runtime_icon.png") != 0)
			LocalFile::RemoveFile(request.iconPath);
	}
	goto done;
patch:
	{
		BgdlTaskPackage title;
		bool disc;

		title.key220 = "PCSI00011";
		title.key217 = "IP9100-PCSI00011_00-PSMRUNTIME000000";

		LsdbAppInfoPtr app(new LsdbAppInfo());

		disc = false;
		ret = bgdlCheckPatchSpace(&title, &disc, &app, 1);
		if (ret == 0)
			ret = bgdlRegistPatchByTitleID(&title, NULL);
		else if (ret == BGDL_ERROR_UX0_FULL && needed != NULL)
			*needed = size;
	}
done:
	if (ret == 0)
		*iconPath = "vs0:vsh/game/psm/runtime_icon.png";
	return ret;
}

/* (0x81107512) the storage a game's additional content goes to: the game card's own titles, or
 * the title's row when there is no game card; *grw0 when it is grw0: */
int BgdlManager::CheckContentStorage(const BgdlDownloadRequest *request, bool *grw0)
{
	int ret = 0;

	*grw0 = false;

	paf::vector<string> titles;

	if (request == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
		goto out;
	}
	if (request->gameTitleId[0] == '\0' || sce_paf_strlen(request->gameTitleId) == 0) {
		ret = BGDL_ERROR_NO_TITLE_ID;
		goto out;
	}
	if (m_ux0Ready == true)
		goto out;
	if (!Bgdl_81112284("gro0:")) {
		int found;

		if (Bgdl_81371C70(request->gameTitleId, &found) == 1) {
			if (found > 0) {
				if (found <= 1) {
					ret = BGDL_ERROR_NO_APP;
					goto out;
				}
			} else if (found >= 0) {
				ret = BGDL_ERROR_NO_STORAGE;
				goto out;
			}
			ret = BGDL_ERROR_NO_APP;
		} else {
			ret = BGDL_ERROR_NO_APP;
		}
		goto out;
	}
	ret = Bgdl_81114092(&titles);
	if (ret != 0)
		goto out;
	{
		SceUInt32 i;
		int kind;

		for (i = 0; i < (SceUInt32)titles.size(); i++) {
			if (titles[i].length() != 0 &&
			    gameAppRecordNamesTitle(request->gameTitleId, titles[i].c_str()) == 1)
				break;
		}
		if (i >= (SceUInt32)titles.size()) {
			ret = BGDL_ERROR_NO_APP;
			if (titles.size() != 0)
				ret = BGDL_ERROR_NO_STORAGE;
			goto out;
		}

		const char *titleId = titles[i].c_str();
		LsdbAppInfoPtr app(new LsdbAppInfo());

		if (Bgdl_81123BC0(titleId, &app) != 0) {
			ret = BGDL_ERROR_NO_APP;
			goto out;
		}
		ret = Bgdl_81113898(&app, &kind);
		if (ret != 0)
			goto out;
		ret = 0;
		if (kind > 1) {
			if (kind <= 4) {
				ret = BGDL_ERROR_NO_APP;
				goto out;
			}
		} else if (kind > 0) {
			if (Bgdl_81112284("grw0:")) {
				*grw0 = true;
				if (vshSblQafMgrIsAllowFakeACInstall() == 0 && request->installMode >= 0)
					ret = BGDL_ERROR_NO_FAKE_AC;
			} else {
				ret = BGDL_ERROR_NO_STORAGE;
			}
			goto out;
		}
		ret = BGDL_ERROR_BAD_STORAGE;
	}
out:
	return ret;
}

/* PLACEHOLDER name: the task is not waiting to run */
#define BGDL_ERROR_NOT_WAITING		0x80103A1D

/* (0x811078DC) a task waiting to run given up */
int BgdlManager::CancelWaitingTask(SceUInt32 id)
{
	EseTask **first;
	EseTask **last;
	int count;

	m_taskListMutex.Lock();
	first = m_tasks.m_begin;
	last = m_tasks.m_end;
	count = last - first;
	while (count > 0) {
		int half = count / 2;
		EseTask **mid = first + half;

		if ((*mid)->id < id) {
			first = ++mid;
			count -= half + 1;
		} else {
			count = half;
		}
	}
	if (first != last && id < (*first)->id)
		first = m_tasks.m_end;
	if (first == last) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_NO_TASK;
	}
	if ((*first)->m_ParamCache.runStatus != 1) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_NOT_WAITING;
	}
	(*first)->setResult(0, BGDL_ERROR_CANCELLED);
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x81107986) */
int BgdlManager::GetTaskList(BgdlTaskEntry *list, int *count, int type)
{
	BgdlTaskEntry *end;
	EseTask **it;
	int max = 0;
	int n = 0;

	m_taskListMutex.Lock();
	if (list != NULL)
		max = *count;
	end = list + max;
	*count = 0;
	for (it = m_tasks.m_begin; it != m_tasks.m_end; it++) {
		if (type == -1 || type == (int)(*it)->m_ParamCache.type) {
			*count = ++n;
			if (list != end) {
				const EseParamCache *cache = &(*it)->m_ParamCache;

				list->id = (*it)->id;
				list->param.type = cache->type;
				list->param.key107 = cache->key107;
				list->param.runStatus = cache->runStatus;
				list->param.key102 = cache->key102;
				list->param.ready = cache->ready;
				list->param.key108 = cache->key108;
				list->param.state = cache->state;
				list->param.key112 = cache->key112;
				list->param.key113 = cache->key113;
				list->param.key110 = cache->key110;
				list->param.key111 = cache->key111;
				list->param.kind = cache->kind;
				list->param.key115 = cache->key115;
				list->param.key116 = cache->key116;
				list->param.reserved = cache->reserved;
				list->result = (*it)->result;
				list++;
				n = *count;
			}
		}
	}
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x81107A9E) the thread woken and waited for, then every task waiting to run given up */
int BgdlManager::CancelWaitingTasks(void)
{
	BgdlTaskEntry list[32];
	SceUInt64 request;
	int count;
	int ret;

	bgdlManagerWakeup(&request);
	for (;;) {
		SceUInt64 wanted = request;
		bool handled;

		s_updateMutex.Lock();
		if (s_manager != NULL) {
			BgdlManager *manager = s_manager;

			manager->m_taskListMutex.Lock();
			handled = s_handled >= wanted;
			manager->m_taskListMutex.Unlock();
			s_updateMutex.Unlock();
		} else {
			s_updateMutex.Unlock();
			handled = true;
		}
		if (handled)
			break;
		thread::Sleep(16);
	}
	count = 32;
	ret = GetTaskList(list, &count, -1);
	if (ret == 0) {
		int i;

		for (i = 0; (SceUInt32)i < (SceUInt32)count; i++) {
			if (list[i].param.runStatus == 1)
				CancelWaitingTask(list[i].id);
		}
	}
	s_updateMutex.Lock();
	if (s_manager != NULL) {
		BgdlManager *manager = s_manager;

		manager->m_taskListMutex.Lock();
		request = ++s_wakeups;
		manager->m_taskListCond.Notify();
		manager->m_taskListMutex.Unlock();
	} else {
		request = ++s_wakeups;
	}
	s_updateMutex.Unlock();
	return ret;
}

/*
 * (0x81107BDE) a download the content server describes made a task: the server asked first, the
 * storage it goes to checked, its files made their sizes, its keys written, and started; *id its
 * id. A file:// URL is a plain download (AddDownloadTask).
 */
int BgdlManager::Bgdl_81107BDE(const BgdlDownloadRequest *request, BgdlContentInfo *info,
			       SceUInt32 *id)
{
	BgdlTaskPath folder;
	string name;
	bool key112 = false;
	bool grw0 = false;
	bool key102 = true;
	bool key205 = true;
	int kind = 0;
	BgdlTaskPackage package;
	int key107 = 0;
	int checkResult = 0;
	BgdlScopeTimer timer("registStore");
	SceUInt32 taskId;
	bool skipPatch;
	bool keepPatch;
	int ret;

	Bgdl_81113890(1);
	if (request == NULL || info == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
		skipPatch = false;
		keepPatch = false;
		goto done;
	}
	if ((request->flags & BGDL_REQUEST_FLAG_STOP_ALL) != 0) {
		BgdlScopeTimer stop("stopTaskAll()");

		CancelWaitingTasks();
	}
	if (request->url[0] != '\0' && sce_paf_strlen(request->url) != 0 &&
	    sce_paf_private_strtok(request->url, "file://") != NULL) {
		skipPatch = false;
		keepPatch = false;
		ret = AddDownloadTask(request->type, request, id);
		goto done;
	}
	if (request->key230[0] != '\0' && sce_paf_strlen(request->key230) != 0 &&
	    request->key222[0] != '\0' && sce_paf_strlen(request->key222) != 0) {
		BgdlLicenseFolder license;

		license.Bgdl_81110808();
		if (license.Bgdl_8111102A(request->key230, request->key222) != 0) {
			ret = BGDL_ERROR_NO_LICENSE;
			license.Bgdl_8111080E();
			skipPatch = false;
			keepPatch = false;
			goto done;
		}
		license.Bgdl_8111080E();
	}
	taskId = bgdlWorkId(request->type, true);
	sce_paf_memset(info, 0, sizeof(*info));
	{
		BgdlServerInfo server;
		bool cancel = false;
		BgdlCancelHost host(&cancel);
		char digest[32];

		ret = bgdlCopyServerInfo(
			Bgdl_8111B72A(request->type,
				      ((request->flags & BGDL_REQUEST_FLAG_SERVER4) != 0) |
				      ((request->flags & BGDL_REQUEST_FLAG_KEY239) != 0) << 1,
				      taskId == (SceUInt32)-1 ? "ux0:" : "ur0:", request->url, NULL, NULL,
				      NULL, request->contentId[0] != '\0' ? request->contentId : NULL,
				      &server, NULL, &host, &checkResult, digest),
			&server, info);
		if (ret != 0) {
			skipPatch = false;
			keepPatch = false;
			goto done;
		}
		if (info->flag != true) {
			ret = info->status;
			skipPatch = false;
			keepPatch = false;
			goto done;
		}
		if (server.kind == 7)
			ret = CheckContentStorage(request, &grw0);
		if (ret != 0) {
			skipPatch = false;
			keepPatch = false;
			goto done;
		}
		if (grw0 == true) {
			BgdlStorageInfo storage;
			BgdlDispatcherState state;

			BgdlJobDispatcher::Bgdl_8113A434()->Bgdl_8113A47C(&state, false);
			if (state.state != 0) {
				ret = BGDL_ERROR_BUSY;
				skipPatch = false;
				keepPatch = false;
				goto done;
			}
			if (taskId == (SceUInt32)-1) {
				taskId = (SceUInt32)-2;
				ret = Bgdl_8111234C("ur0:", &storage);
				if (ret != 0) {
					skipPatch = false;
					keepPatch = false;
					goto done;
				}
				if (storage.free < 6 * storage.clusterSize) {
					ret = BGDL_ERROR_UX0_FULL;
					skipPatch = false;
					keepPatch = false;
					goto done;
				}
			}
			if (!Bgdl_81112284("grw0:")) {
				ret = BGDL_ERROR_NO_STORAGE;
				skipPatch = false;
				keepPatch = false;
				goto done;
			}
			Bgdl_811137F6(false);
			ret = bgdlGamePluginCheck();
			if (ret != 0) {
				skipPatch = false;
				keepPatch = false;
				goto done;
			}
			ret = Bgdl_8111234C("grw0:", &storage);
			if (ret != 0) {
				skipPatch = false;
				keepPatch = false;
				goto done;
			}
			if (storage.free < storage.clusterSize +
			    Bgdl_81112310(info->size, storage.clusterSize)) {
				ret = BGDL_ERROR_GRW0_FULL;
				skipPatch = false;
				keepPatch = false;
				goto done;
			}
			ret = Bgdl_811137F6(true);
			if (ret != 0) {
				skipPatch = false;
				keepPatch = false;
				goto done;
			}
		} else if (taskId == (SceUInt32)-1) {
			if (m_ux0Ready != true) {
				ret = BGDL_ERROR_NO_UX0;
				skipPatch = false;
				keepPatch = false;
				goto done;
			}
			if (info->value2 < info->value3) {
				ret = BGDL_ERROR_UX0_FULL;
				skipPatch = false;
				keepPatch = false;
				goto done;
			}
		}
		skipPatch = false;
		keepPatch = false;
		if (request->type == 6 && server.kind == 6) {
			if ((server.data.attribute & 0x10) != 0)
				keepPatch = true;
			else
				skipPatch = (int)server.data.attribute >= 0;
		}

		/* a package of kind 9: its file 230 copied into the title's sce_sys/package */
		if (server.kind == 9) {
			key107 = 9;

			string taskName("");

			if (request->name[0] != '\0')
				taskName = request->name;
			ret = AddTask(&taskId, 0, request->type, taskName.c_str(),
				      (request->flags & BGDL_REQUEST_FLAG_KEY115) != 0,
				      (request->flags & BGDL_REQUEST_FLAG_KEY116) != 0);
			if (ret != 0 || (ret = folder.initialize(taskId, 20)) != 0)
				goto done;
			if (request->key230[0] == '\0' && sce_paf_strlen(request->key230) == 0) {
				ret = BGDL_ERROR_NO_KEY230;
				goto done;
			}

			string file;
			string work(folder.m_begin);

			work += "/";
			work += server.data.titleId;

			string title(work);

			work += "/sce_sys/package";
			ret = Bgdl_811121FE(work.c_str(), 6);
			if (ret != 0 || (ret = Bgdl_8111322E(request->key230, title.c_str(), &file)) != 0)
				goto done;

			string icon;

			if (request->iconPath[0] != '\0')
				Bgdl_81111CDC(request->iconPath, folder.m_begin, &icon, true);

			EseTask *task = FindTask(taskId);

			{
				EseParam param(taskId);
				int key104;
				int key101;
				bool flag;

				ret = param.openWrite();
				if (ret != 0 ||
				    (ret = param.write(230, file.c_str(), file.length() + 1)) != 0 ||
				    (icon.length() != 0 &&
				     (ret = param.write(106, icon.c_str(), icon.length() + 1)) != 0) ||
				    (request->key234[0] != '\0' && sce_paf_strlen(request->key234) != 0 &&
				     (ret = param.write(234, request->key234,
							sce_paf_strlen(request->key234) + 1)) != 0))
					goto done;
				key205 = true;
				ret = param.write(205, &key205, sizeof(key205));
				if (ret != 0)
					goto done;
				key102 = false;
				ret = param.write(102, &key102, sizeof(key102));
				if (ret != 0)
					goto done;
				key112 = false;
				ret = param.write(112, &key112, sizeof(key112));
				if (ret != 0)
					goto done;
				package.key220 = server.data.titleId;
				package.reserved2 = server.data.category;
				package.reserved3 = server.data.attribute;
				package.key217 = server.data.contentId;
				ret = param.write(220, server.data.titleId,
						  sce_paf_strlen(server.data.titleId) + 1);
				if (ret != 0)
					goto done;
				ret = param.write(232, &server.data, sizeof(server.data));
				if (ret != 0)
					goto done;
				key104 = 0;
				ret = param.write(104, &key104, sizeof(key104));
				if (ret != 0)
					goto done;
				ret = param.write(107, &server.kind, sizeof(server.kind));
				if (ret != 0)
					goto done;
				if ((request->flags & BGDL_REQUEST_FLAG_KEY239) != 0) {
					flag = true;
					ret = param.write(239, &flag, sizeof(flag));
					if (ret != 0)
						goto done;
				}
				if (task == NULL) {
					ret = BGDL_ERROR_NO_TASK;
					goto done;
				}
				task->result = 0;
				task->setKey107(server.kind);
				task->setKey102(key102);
				task->setProgress(false, info->size, 0);
				task->setPackage(&package);
				task->setKey113(true);
				flag = true;
				ret = param.write(113, &flag, sizeof(flag));
				if (ret != 0)
					goto done;
				key101 = 0;
				ret = param.write(101, &key101, sizeof(key101));
				if (ret != 0)
					goto done;
				ret = param.commit();
			}
			if (ret == 0) {
				BgdlEvent event(3, taskId, 0, 0);

				event.param = task->m_ParamCache;
				NotifyListeners(&event);
				*id = taskId;
			}
			goto done;
		}

		if (info->url[0] == '\0' || sce_paf_strlen(info->url) == 0) {
			ret = BGDL_ERROR_NO_FILE;
			goto done;
		}
		name = info->url;

		string key222;
		string renamed;

		/* the file takes key 222's name */
		if (server.kind == 4) {
			if (request->key222[0] == '\0' && sce_paf_strlen(request->key222) == 0) {
				ret = BGDL_ERROR_NO_LIST;
				goto done;
			}
			if (request->key230[0] != '\0' && sce_paf_strlen(request->key230) != 0) {
			}
			key222 = request->key222;
			if (key222.length() == 0 || name.length() == 0) {
				ret = BGDL_ERROR_NO_LIST;
				goto done;
			}
			if (Bgdl_81111B96(&name, &key222, &renamed) != 0) {
				ret = BGDL_ERROR_BAD_NAME;
				goto done;
			}
			name = renamed;
		}
		if (request->url[0] == '\0') {
			ret = BGDL_ERROR_NO_URL;
			goto done;
		}

		string url(request->url);

		if (url.length() == 0) {
			ret = BGDL_ERROR_NO_URL;
			goto done;
		}

		string taskName("");

		if (request->name[0] != '\0')
			taskName = request->name;
		ret = AddTask(&taskId, 0, request->type, taskName.c_str(),
			      (request->flags & BGDL_REQUEST_FLAG_KEY115) != 0,
			      (request->flags & BGDL_REQUEST_FLAG_KEY116) != 0);
		if (ret != 0 || (ret = folder.initialize(taskId, 20)) != 0)
			goto done;
		for (const BgdlKindMap *map = s_kindMap; map->kind != 0; map++) {
			if (info->kind == map->kind) {
				key107 = map->server;
				break;
			}
		}

		/* its .MNV's licence, the .M4T, got first */
		if (server.kind == 4) {
			string m4t(request->url);

			if (Bgdl_8111431C(&m4t, ".MNV", ".M4T") == 1) {
				string dir(folder.m_begin);
				string m4tName(request->key222);

				m4tName += ".M4T";

				BgdlServerInfo m4tServer;
				bool m4tCancel = false;
				int status = 0;
				BgdlCancelHost m4tHost(&m4tCancel);

				int result = Bgdl_8111FEF8("ux0:", m4t.c_str(), 0x1000000, 0, &m4tHost, 0,
							   dir.c_str(), m4tName.c_str(), &status, &m4tServer,
							   NULL);

				if (status == 200 && result != 0) {
					ret = result;
					goto done;
				}
			}
		}

		string path;
		string gameFolder("");
		int parts = 0;

		if (grw0 == true) {
			path = "grw0:fgdl";
			path += "/";
			path += name.c_str();
		} else {
			const char *dir = folder.m_begin;
			BgdlGamePluginLock lock(false);

			if (bgdlKindIsGame(key107) == 1 && bgdlCategoryIsGame(server.data.category) != 0) {
				lock.Lock();
				gameFolder = Bgdl_81113C72(taskId, true);
				if (gameFolder.length() == 0) {
					ret = BGDL_ERROR_NO_GAME_FOLDER;
					goto done;
				}
				dir = gameFolder.c_str();
			}
			if (dir == NULL) {
				ret = BGDL_ERROR_NO_GAME_FOLDER;
				goto done;
			}
			path = dir;
			path += "/";
			path += name.c_str();
			if (bgdlKindIsGame(key107) == 1) {
				if (Bgdl_8111240E(path.c_str(), info->size + 0x2000000) != 0) {
					LocalFileStat stat;

					if (LocalFile::Getstat(path.c_str(), &stat) == 0)
						LocalFile::RemoveFile(path.c_str());
					parts = 0;
					if (Bgdl_8111296E(dir, &parts, info->size + 0x2000000, 0, &parts) != 0) {
						ret = BGDL_ERROR_NO_GAME_FOLDER;
						goto done;
					}
				}
			} else {
				string contents(dir);

				contents += "/";
				contents += "av_contents.bin";
				if (Bgdl_8111240E(contents.c_str(), 0x200000) != 0) {
					ret = BGDL_ERROR_NO_GAME_FOLDER;
					goto done;
				}
				if (Bgdl_8111240E(path.c_str(), info->size) != 0) {
					ret = BGDL_ERROR_NO_GAME_FOLDER;
					goto done;
				}
			}
		}
		{
			EseParam param(taskId);
			SceUInt64 key208;
			int key108;
			bool flag;

			ret = param.openWrite();
			if (ret != 0)
				goto done;
			if (parts >= 2)
				param.write(238, &parts, sizeof(parts));
			if (gameFolder.length() != 0) {
				ret = param.write(229, gameFolder.c_str(),
						  sce_paf_strlen(gameFolder.c_str()) + 1);
				if (ret != 0)
					goto done;
			}
			ret = param.write(203, name.c_str(), sce_paf_strlen(name.c_str()) + 1);
			if (ret != 0)
				goto done;
			key112 = grw0 != true;
			ret = param.write(112, &key112, sizeof(key112));
			if (ret != 0)
				goto done;
			ret = param.write(202, url.c_str(), url.length() + 1);
			if (ret != 0)
				goto done;
			if (request->iconPath[0] != '\0') {
				string icon;

				if (Bgdl_81111CDC(request->iconPath, folder.m_begin, &icon, true) == 0) {
					ret = param.write(106, icon.c_str(), icon.length() + 1);
					if (ret != 0)
						goto done;
				}
			}
			if (request->key230[0] != '\0' && sce_paf_strlen(request->key230) != 0) {
				string file;

				ret = Bgdl_81111CDC(request->key230, folder.m_begin, &file, true);
				if (ret != 0 ||
				    (ret = param.write(230, file.c_str(), file.length() + 1)) != 0)
					goto done;
			}
			if (request->contentId[0] != '\0' && sce_paf_strlen(request->contentId) != 0) {
				ret = param.write(217, request->contentId,
						  sce_paf_strlen(request->contentId) + 1);
				if (ret != 0)
					goto done;
			}
			if (request->key222[0] != '\0' && sce_paf_strlen(request->key222) != 0) {
				ret = param.write(222, request->key222, sce_paf_strlen(request->key222) + 1);
				if (ret != 0)
					goto done;
			}
			if (request->key221[0] != '\0' && sce_paf_strlen(request->key221) != 0) {
				ret = param.write(221, request->key221, sce_paf_strlen(request->key221) + 1);
				if (ret != 0)
					goto done;
			}
			if (request->key234[0] != '\0' && sce_paf_strlen(request->key234) != 0) {
				ret = param.write(234, request->key234, sce_paf_strlen(request->key234) + 1);
				if (ret != 0)
					goto done;
			}
			flag = request->type == 0;
			ret = param.write(218, &flag, sizeof(flag));
			if (ret != 0)
				goto done;
			if (info->size == 0 || info->size == (SceUInt64)-1) {
				ret = BGDL_ERROR_NO_SIZE;
				goto done;
			}
			ret = param.write(206, &info->size, sizeof(info->size));
			if (ret != 0)
				goto done;
			key208 = 0;
			ret = param.write(208, &key208, sizeof(key208));
			if (ret != 0)
				goto done;
			if (info->contentId[0] != '\0' && sce_paf_strlen(info->contentId) != 0) {
				ret = param.write(204, info->contentId, sce_paf_strlen(info->contentId) + 1);
				if (ret != 0)
					goto done;
			}
			key205 = true;
			key102 = true;
			switch (key107) {
			case 1:
			case 2:
			case 3:
			case 4:
			case 6:
				break;
			case 7:
			case 8:
				key205 = false;
				key102 = false;
				break;
			case 12:
				key205 = false;
				key102 = false;
				break;
			default:
				ret = BGDL_ERROR_BAD_KIND;
				goto done;
			}
			if (bgdlKindIsGame(key107) == 1) {
				package.key220 = server.data.titleId;
				package.reserved2 = server.data.category;
				package.reserved3 = server.data.attribute;
				if (request->platform != 2 || request->version < 0x1800000 ||
				    request->version >= 0x2000000)
					package.key217 = server.data.contentId;
				ret = param.write(220, server.data.titleId,
						  sce_paf_strlen(server.data.titleId) + 1);
				if (ret != 0)
					goto done;
				ret = param.write(232, &server.data, sizeof(server.data));
				if (key107 == 6) {
					switch (server.data.category) {
					case 6:
					case 7:
					case 14:
					case 15:
					case 16:
						kind = 1;
						break;
					case 24:
						kind = 2;
						break;
					case 29:
						kind = 3;
						break;
					case 31:
						kind = 0;
						break;
					default:
						kind = 0;
						break;
					}
					ret = param.write(114, &kind, sizeof(kind));
					if (ret != 0)
						goto done;
					if ((request->flags & BGDL_REQUEST_FLAG_KEY239) != 0) {
						flag = true;
						ret = param.write(239, &flag, sizeof(flag));
						if (ret != 0)
							goto done;
					}
				}
			}
			if (ret != 0)
				goto done;
			ret = param.write(205, &key205, sizeof(key205));
			if (ret != 0)
				goto done;
			ret = param.write(102, &key102, sizeof(key102));
			if (ret != 0)
				goto done;
			key108 = 1;
			if (grw0 == true) {
				ret = param.write(110, &grw0, sizeof(grw0));
				if (ret != 0)
					goto done;
				key108 = 0;
				ret = param.write(108, &key108, sizeof(key108));
				if (ret != 0)
					goto done;

				string fgdl("grw0:fgdl");

				ret = param.write(229, fgdl.c_str(), sce_paf_strlen(fgdl.c_str()) + 1);
				if (ret != 0)
					goto done;
			} else if (request->type == 4) {
				key108 = 0;
				ret = param.write(108, &key108, sizeof(key108));
				if (ret != 0)
					goto done;
			}
			if (checkResult != 0) {
				ret = param.write(236, &checkResult, sizeof(checkResult));
				if (ret != 0 || (ret = param.write(237, digest, sizeof(digest))) != 0)
					goto done;
			} else if (key107 > 2 && key107 <= 4) {
				BgdlPackageCheck check;

				check.Bgdl_81114AFC(grw0 ? 5 : 6, 1, request->url, 0, 0, 0, 1, 0);
				checkResult = check.Bgdl_8118FD0C();
				check.Bgdl_81115C22(digest);
				ret = param.write(236, &checkResult, sizeof(checkResult));
				if (ret == 0)
					ret = param.write(237, digest, sizeof(digest));
				if (ret != 0)
					goto done;
			}
			ret = param.write(107, &key107, sizeof(key107));
			if (ret != 0)
				goto done;

			EseTask *task = FindTask(taskId);

			if (task == NULL) {
				ret = BGDL_ERROR_NO_TASK;
				goto done;
			}
			task->setKey107(key107);
			task->setKey102(key102);
			task->setProgress(false, info->size, key208);
			task->setPackage(&package);
			if (grw0 == true)
				task->setKey110(grw0);
			task->m_ParamCache.key108 = key108;
			task->m_ParamCache.kind = kind;
			ret = param.commit();
			if (ret != 0)
				goto done;
			if (server.kind == 4 && request->key222[0] != '\0' &&
			    sce_paf_strlen(request->key222) != 0) {
				BgdlLicenseFolder license;

				license.Bgdl_81110808();
				ret = license.Bgdl_81111432(request->key222, true);
				if (ret != 0) {
					ret = BGDL_ERROR_NO_LICENSE;
					license.Bgdl_8111080E();
					goto done;
				}
				license.Bgdl_8111080E();
			}
		}
		if ((request->flags & BGDL_REQUEST_FLAG_NO_START) == 0 && grw0 != true)
			ret = ReloadTask(taskId);
		if (ret == 0)
			*id = taskId;
	}
done:
	if (ret != 0)
		RemoveTask(taskId);
	else
		UpdateQueueStatus();
	if (grw0 == true) {
		if (ret != 0)
			Bgdl_811137F6(false);
	} else if (ret == 0) {
		bgdlUnsetAutoDownload(FindTask(*id));
		if (kind != 0) {
			if (kind == 2 && key107 == 6) {
				string icon((const char *)NULL, 0);

				AddPsmRuntimeTask(&icon, 1, 0, NULL);
			}
		} else if ((key107 == 6 || key107 == 9) && package.key220.length() != 0 &&
			   skipPatch == false && keepPatch == false) {
			bool flag = false;

			if (request->type == 0)
				flag = (request->flags & BGDL_REQUEST_FLAG_SERVER4) != 0;
			bgdlRegistPatchByTitleID(&package, &flag);
		}
	}
	return ret;
}

/* (0x81109C84) PLACEHOLDER name */
int bgdlAddServerTask(const BgdlDownloadRequest *request, BgdlContentInfo *info, SceUInt32 *id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->Bgdl_81107BDE(request, info, id);
}

/*
 * (0x81109CAC) PLACEHOLDER name: the package files of a list added to the package task id
 * (key 107 8), after the ones it has (key 100)
 */
int BgdlManager::AddPackageFiles(SceUInt32 id, const BgdlPackageRequest *request)
{
	char number[8] = { 0 };
	char fileName[32] = { 0 };
	BgdlTaskPath folder;
	SceUInt32 start = 0;
	int ret;

	if (request == NULL || request->listPath[0] == '\0')
		return BGDL_ERROR_NULL_TITLE;
	ret = 0;
	if (LocalFile::GetFileSize(request->listPath) <= 0)
		return BGDL_ERROR_NO_LIST;
	ret = folder.initialize(id, 20);
	if (ret != 0)
		return ret;

	string base(folder.m_begin);
	string path;
	SceUInt64 total = 0;

	base += "/";
	base += "p";
	{
		EseParam param(id);
		int type;

		ret = param.open();
		if (ret != 0)
			return ret;
		ret = param.read(107, &type, 4);
		if (ret != 0)
			return ret;
		if (type != 8)
			return BGDL_ERROR_NO_TASK;
		ret = param.read(100, &start, 4);
		if (ret != 0)
			return ret;
	}

	EseParam file(id);

	ret = file.openWrite();
	if (ret != 0)
		return ret;

	common::SharedPtr<File> list;

	list = LocalFile::Open(request->listPath, SCE_O_RDONLY | 0x10000, 6, &ret);
	if (ret != 0)
		return ret;

	BgdlFileCloser closer(list);
	BgdlPackageEntry entry;
	SceUInt32 i;

	for (i = start; ; i++) {
		sce_paf_memset(&entry, 0, sizeof(entry));
		if (list->Read(&entry, sizeof(entry)) <= 0)
			break;
		ret = file.write(5000 + 32 * i, entry.name, sce_paf_strlen(entry.name) + 1);
		if (ret != 0)
			break;
		ret = file.write(5002 + 32 * i, &entry.size, 8);
		if (ret != 0)
			break;
		ret = file.write(5001 + 32 * i, entry.digest, 20);
		if (ret != 0)
			break;
		sce_paf_snprintf(fileName, 31, "f_%04d_%02d.pkg", i, 0);
		ret = file.write(5003 + 32 * i, fileName, sce_paf_strlen(fileName) + 1);
		if (ret != 0)
			break;
		sce_paf_snprintf(number, 7, "%04d", i);
		path = base;
		path += "/";
		path += number;
		if (Dir::IsExist(path.c_str()) == 1)
			Dir::RemoveRecursive(path.c_str());
		ret = Bgdl_811121FE(path.c_str(), 6);
		if (ret != 0)
			break;
		path += "/";
		path += fileName;
		ret = Bgdl_8111240E(path.c_str(), entry.size);
		total += entry.size;
	}
	if (request->fileCount + start > i)
		return BGDL_ERROR_LIST_SHORT;
	ret = file.write(505, &total, 8);
	if (ret != 0)
		return ret;
	ret = file.write(503, &request->key503, 2);
	if (ret != 0)
		return ret;
	ret = file.write(504, &request->key504, 2);
	if (ret != 0)
		return ret;
	ret = file.write(500, &i, 4);
	if (ret != 0)
		return ret;
	return file.commit();
}

/* (0x8110AA6A) PLACEHOLDER name */
int bgdlAddPackageFiles(SceUInt32 id, const BgdlPackageRequest *request)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->AddPackageFiles(id, request);
}

/* (0x8110AA8E) PLACEHOLDER name: the task's status */
int BgdlManager::GetTaskStatus(SceUInt32 id, BgdlTaskStatus *status)
{
	EseTask **pos;

	m_taskListMutex.Lock();
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_NO_TASK;
	}
	memcpy(status, &(*pos)->status, 60);
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x8110AB1E) PLACEHOLDER name */
int bgdlGetTaskStatus(SceUInt32 id, BgdlTaskStatus *status)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->GetTaskStatus(id, status);
}

/* (0x8110AB42) PLACEHOLDER name: the task's libmtp copy */
int BgdlManager::GetTaskCopy(SceUInt32 id, BgdlTaskCopyStatus *copy)
{
	EseTask **pos;
	EseTask *task;

	m_taskListMutex.Lock();
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_NO_TASK;
	}
	task = *pos;
	copy->result = task->copyResult;
	copy->copy = task->copy;
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x8110ABFC) PLACEHOLDER name */
int bgdlGetTaskCopy(SceUInt32 id, BgdlTaskCopyStatus *copy)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->GetTaskCopy(id, copy);
}

/* (0x8110AC20) PLACEHOLDER name: the task's cached keys */
int BgdlManager::GetTaskParam(SceUInt32 id, EseParamCache *param)
{
	EseTask **pos;

	m_taskListMutex.Lock();
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end || *pos == NULL) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_NO_TASK;
	}
	*param = (*pos)->m_ParamCache;
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x8110AD48) PLACEHOLDER name */
int bgdlGetTaskParam(SceUInt32 id, EseParamCache *param)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->GetTaskParam(id, param);
}

/* (0x8110AD6C) PLACEHOLDER name */
int bgdlRemoveTask(SceUInt32 id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->RemoveTask(id);
}

/* (0x8110AD8E) PLACEHOLDER name */
int bgdlReloadTask(SceUInt32 id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->ReloadTask(id);
}

/* (0x8110ADB0) PLACEHOLDER name: the thread woken and waited for, the task given up if waiting */
int BgdlManager::CancelTask(SceUInt32 id)
{
	SceUInt64 request;
	int ret;

	bgdlManagerWakeup(&request);
	for (;;) {
		SceUInt64 wanted = request;
		bool handled;

		s_updateMutex.Lock();
		if (s_manager != NULL) {
			BgdlManager *manager = s_manager;

			manager->m_taskListMutex.Lock();
			handled = s_handled >= wanted;
			manager->m_taskListMutex.Unlock();
			s_updateMutex.Unlock();
		} else {
			s_updateMutex.Unlock();
			handled = true;
		}
		if (handled)
			break;
		thread::Sleep(16);
	}
	ret = s_manager->CancelWaitingTask(id);
	s_updateMutex.Lock();
	if (s_manager != NULL) {
		BgdlManager *manager = s_manager;

		manager->m_taskListMutex.Lock();
		request = ++s_wakeups;
		manager->m_taskListCond.Notify();
		manager->m_taskListMutex.Unlock();
	} else {
		request = ++s_wakeups;
	}
	s_updateMutex.Unlock();
	return ret;
}

/* (0x8110AEC4) PLACEHOLDER name */
int bgdlCancelTask(SceUInt32 id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->CancelTask(id);
}

/* (0x8110AEE6) PLACEHOLDER name: every task neither running nor done read again */
int BgdlManager::ReloadTasks(void)
{
	BgdlTaskEntry list[32];
	SceUInt64 request;
	int count;
	int ret;

	bgdlManagerWakeup(&request);
	for (;;) {
		SceUInt64 wanted = request;
		bool handled;

		s_updateMutex.Lock();
		if (s_manager != NULL) {
			BgdlManager *manager = s_manager;

			manager->m_taskListMutex.Lock();
			handled = s_handled >= wanted;
			manager->m_taskListMutex.Unlock();
			s_updateMutex.Unlock();
		} else {
			s_updateMutex.Unlock();
			handled = true;
		}
		if (handled)
			break;
		thread::Sleep(16);
	}
	count = 32;
	ret = GetTaskList(list, &count, -1);
	if (ret == 0) {
		int i;

		for (i = 0; (SceUInt32)i < (SceUInt32)count; i++) {
			if (list[i].param.runStatus == 0 && !list[i].param.ready)
				ReloadTask(list[i].id);
		}
	}
	s_updateMutex.Lock();
	if (s_manager != NULL) {
		BgdlManager *manager = s_manager;

		manager->m_taskListMutex.Lock();
		request = ++s_wakeups;
		manager->m_taskListCond.Notify();
		manager->m_taskListMutex.Unlock();
	} else {
		request = ++s_wakeups;
	}
	s_updateMutex.Unlock();
	return ret;
}

/* (0x8110B028) PLACEHOLDER name */
int bgdlReloadTasks(void)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->ReloadTasks();
}

/* (0x8110B048) PLACEHOLDER name */
int bgdlCancelWaitingTasks(void)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->CancelWaitingTasks();
}

/*
 * (0x8110B068) PLACEHOLDER name: the tasks removed, only the done ones that are not running when
 * doneOnly, else every one, those waiting given up first
 */
int BgdlManager::RemoveTasks(bool doneOnly)
{
	BgdlTaskEntry list[32];
	SceUInt64 request;
	int count;
	int ret;

	if (!doneOnly)
		CancelWaitingTasks();
	bgdlManagerWakeup(&request);
	for (;;) {
		SceUInt64 wanted = request;
		bool handled;

		s_updateMutex.Lock();
		if (s_manager != NULL) {
			BgdlManager *manager = s_manager;

			manager->m_taskListMutex.Lock();
			handled = s_handled >= wanted;
			manager->m_taskListMutex.Unlock();
			s_updateMutex.Unlock();
		} else {
			s_updateMutex.Unlock();
			handled = true;
		}
		if (handled)
			break;
		thread::Sleep(16);
	}
	count = 32;
	ret = GetTaskList(list, &count, -1);
	if (ret == 0) {
		int i;

		if (doneOnly) {
			for (i = 0; (SceUInt32)i < (SceUInt32)count; i++) {
				if (list[i].param.runStatus == 0 && list[i].param.ready)
					RemoveTask(list[i].id);
			}
		} else {
			for (i = 0; (SceUInt32)i < (SceUInt32)count; i++)
				RemoveTask(list[i].id);
		}
	}
	s_updateMutex.Lock();
	if (s_manager != NULL) {
		BgdlManager *manager = s_manager;

		manager->m_taskListMutex.Lock();
		request = ++s_wakeups;
		manager->m_taskListCond.Notify();
		manager->m_taskListMutex.Unlock();
	} else {
		request = ++s_wakeups;
	}
	s_updateMutex.Unlock();
	return ret;
}

/* (0x8110B1DE) PLACEHOLDER name */
int bgdlRemoveTasks(bool doneOnly)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->RemoveTasks(doneOnly);
}

/* a task's entry added to a list (inlined where it is used) */
static __attribute__((always_inline)) void bgdlListTask(vector<BgdlTaskEntry> *list, const EseTask *task)
{
	BgdlTaskEntry entry;

	entry.id = task->id;
	entry.param = task->m_ParamCache;
	entry.result = task->result;
	list->push_back(entry);
}

/*
 * (0x8110B202) PLACEHOLDER name: the waiting tasks with no error and key 113 set that are for the
 * title: of the kind (key 111) asked for, or, for any other, its game data, patch or package
 * (key 107 8, 9 or 7)
 */
int BgdlManager::FindTitleTasks(vector<BgdlTaskEntry> *list, const string *titleId, int kind)
{
	if (titleId->length() == 0)
		return BGDL_ERROR_NULL_TITLE;
	list->clear();
	m_taskListMutex.Lock();
	{
		BgdlTaskPackage package;
		EseTask **it;

		for (it = m_tasks.m_begin; it != m_tasks.m_end; ++it) {
			EseTask *task = *it;
			EseParamCache param;
			int key107;

			if (task->result != 0)
				continue;
			param = task->m_ParamCache;
			if (param.runStatus != 0 || !param.key113)
				continue;
			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			key107 = param.key107;
			if (kind == 3 && param.kind == 3) {
				if (key107 == 6 && package.key220 == *titleId)
					bgdlListTask(list, *it);
			} else if (kind == 2 && param.kind == 2) {
				if (key107 == 6 && package.key220 == *titleId)
					bgdlListTask(list, *it);
			} else if (kind == 1 && param.kind == 1) {
				if (key107 == 8) {
					if (titleId->length() == 0 || package.key220 == *titleId)
						bgdlListTask(list, *it);
				} else if (key107 == 7) {
					bgdlListTask(list, *it);
				}
			} else if (key107 == 8) {
				if (package.key220 == *titleId)
					bgdlListTask(list, *it);
			} else if (key107 == 9) {
				if (package.key220 == *titleId)
					bgdlListTask(list, *it);
			} else if (key107 == 7 &&
				   gameAppRecordNamesTitle(package.key220.c_str(), titleId->c_str()) == 1) {
				bgdlListTask(list, *it);
			}
		}
	}
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x8110B7E2) PLACEHOLDER name: *count the waiting tasks for the title */
int bgdlCountTitleTasks(const char *titleId, SceUInt32 *count)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	if (titleId == NULL)
		return BGDL_ERROR_INVALID_ARGUMENT;
	*count = 0;

	vector<BgdlTaskEntry> list;
	string title(titleId);
	int ret;

	ret = s_manager->FindTitleTasks(&list, &title, 0);
	if (ret != 0)
		return ret;
	*count = list.size();
	return 0;
}

/*
 * (0x8110B8BC) PLACEHOLDER name: *count the listed tasks of the kind that are done, have no type
 * and match flags for the title: 1 its package (named in record), 2 its game data, 4 its patch,
 * 8 its content (key 107 7, 8, 9, 6); the first found ends it when list is NULL. list is only
 * emptied.
 */
int BgdlManager::CountTitleSummaries(const LsdbAppInfoPtr *record, const char *titleId, int kind,
				     SceUInt32 flags, vector<BgdlTaskEntry> *list, int *count)
{
	BgdlScopeTimer timer("queryConCache2");
	int found;
	int i;

	if (m_ux0Ready == false)
		return BGDL_ERROR_NO_UX0;
	if (list != NULL)
		list->clear();
	found = 0;
	for (i = 0; i < BGDL_QUEUE_MAX; i++) {
		BgdlTaskSummary *entry = &m_summaries.entries[i];
		int key107;

		if (entry->param.type != 0)
			continue;
		if (entry->titleId[0] == '\0')
			break;
		if (!entry->ready)
			continue;
		key107 = entry->param.key107;
		if (kind == 2 && entry->param.kind == 1) {
			if (key107 == 7) {
				if (flags & 1)
					found++;
			} else if (key107 == 8) {
				if ((flags & 2) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			} else if (key107 == 6) {
				if ((flags & 8) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			}
		} else if (kind == 1 && entry->param.kind == 0) {
			if (key107 == 8) {
				if ((flags & 2) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			} else if (key107 == 9) {
				if ((flags & 4) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			} else if (key107 == 6) {
				if ((flags & 8) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			} else if (key107 == 7) {
				if ((flags & 1) && Game_81372130(entry->titleId, record) == 1)
					found++;
			}
		} else if (kind == 4 && entry->param.kind == 2) {
			if (key107 == 9) {
				if ((flags & 4) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			} else if (key107 == 6) {
				if ((flags & 8) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			}
		} else if (kind == 5 && entry->param.kind == 3) {
			if (key107 == 9) {
				if ((flags & 4) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			} else if (key107 == 6) {
				if ((flags & 8) && sce_paf_strncmp(entry->titleId, titleId, 14) == 0)
					found++;
			}
		}
		if (found != 0 && list == NULL)
			break;
	}
	if (count != NULL)
		*count = found;
	return 0;
}

/*
 * (0x8110BB9C) PLACEHOLDER name: *count the listed tasks for the title of a livespace_db record,
 * by its kind (CountTitleSummaries)
 */
int BgdlManager::CountRecordTasks(const LsdbAppInfoPtr *record, SceUInt32 flags,
				  vector<BgdlTaskEntry> *list, int *count)
{
	BgdlScopeTimer timer("queryConCache");
	string titleId;
	int kind;
	int ret;

	if (!Bgdl_81113900(record, &titleId))
		return BGDL_ERROR_NO_TITLE_ID;
	if (titleId.length() == 0)
		return BGDL_ERROR_NO_TITLE_ID;
	ret = Bgdl_811138CC(record, &kind);
	if (ret != 0)
		return ret;
	if (kind > 2) {
		if (kind == 5)
			return CountTitleSummaries(record, titleId.c_str(), 5, flags, list, count);
	} else if (kind > 0) {
		return CountTitleSummaries(record, titleId.c_str(), kind, flags, list, count);
	}
	return BGDL_ERROR_BAD_KIND;
}

/* (0x8110BD6E) PLACEHOLDER name: a task for the record's title is listed */
bool bgdlRecordHasTasks(const LsdbAppInfoPtr *record, SceUInt32 flags)
{
	if (s_manager != NULL) {
		int count = 0;

		if (s_manager->CountRecordTasks(record, flags, NULL, &count) == 0 && count > 0)
			return true;
	}
	return false;
}

/* (0x8110BDC0) PLACEHOLDER name: a done task for the title (key 111 0) matching flags is listed */
bool bgdlTitleHasTasks(const char *titleId, SceUInt32 flags)
{
	if (s_manager != NULL && titleId != NULL) {
		int count = 0;
		LsdbAppInfoPtr record;

		new LsdbAppInfo();	/* Sony's: made, never kept */
		if (s_manager->CountTitleSummaries(&record, titleId, 1, flags, NULL, &count) == 0 &&
		    count > 0)
			return true;
	}
	return false;
}

/* (0x8110BEC2) PLACEHOLDER name: the same for a title's tasks of a kind (key 111 0 to 3) */
bool bgdlTitleHasTasksOfKind(const char *titleId, int type, SceUInt32 flags)
{
	if (s_manager != NULL && titleId != NULL) {
		int count = 0;
		LsdbAppInfoPtr record;
		int kind;

		new LsdbAppInfo();	/* Sony's: made, never kept */
		if (type == 1)
			kind = 2;
		else if (type == 2)
			kind = 4;
		else if (type == 3)
			kind = 5;
		else
			kind = 1;
		if (s_manager->CountTitleSummaries(&record, titleId, kind, flags, NULL, &count) == 0 &&
		    count > 0)
			return true;
	}
	return false;
}

/* (0x8110BFE4) PLACEHOLDER name: *count the waiting tasks of key 111 1 for the title */
int bgdlCountAppTasks(const char *titleId, SceUInt32 *count)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	*count = 0;

	vector<BgdlTaskEntry> list;
	string title(titleId != NULL ? titleId : "");
	int ret;

	ret = s_manager->FindTitleTasks(&list, &title, 1);
	if (ret != 0)
		return ret;
	*count = list.size();
	return 0;
}

/* (0x8110C0C0) PLACEHOLDER name: the tasks of a type (key 111; -1: all) listed */
int BgdlManager::ListTasks(vector<BgdlTaskEntry> *list, int type)
{
	EseTask **it;

	m_taskListMutex.Lock();
	list->clear();
	for (it = m_tasks.m_begin; it != m_tasks.m_end; ++it) {
		if (type == -1 || type == (*it)->m_ParamCache.type)
			bgdlListTask(list, *it);
	}
	m_taskListMutex.Unlock();
	return 0;
}

/*
 * (0x8110C1EC) PLACEHOLDER name: the title's waiting package tasks (key 111 0, key 114 1, key 107
 * 8) are cancelled and removed when no more than one title is installed with the game data the
 * title's record names (GAMEDATA_ID)
 */
int BgdlManager::RemoveGameDataTasks(const char *titleId)
{
	vector<BgdlTaskEntry> list;
	vector<BgdlTaskEntry> remove;
	string title;
	int ret = 0;
	SceUInt32 i;

	if (titleId == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
	} else {
		ret = ListTasks(&list, 0);
		if (ret != 0)
			return ret;

		BgdlTaskPackage package;

		title = titleId;
		remove.clear();
		for (i = 0; i < list.size(); i++) {
			EseTask *task = FindTask(list[i].id);
			EseParamCache param;

			if (task == NULL)
				continue;
			param = task->m_ParamCache;
			if (param.kind != 1)
				continue;
			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			if (package.key220.length() == 0 || package.key220 != title)
				continue;
			if (param.key107 != 8) {
				if (param.key107 == 6 && package.key220 == title)
					;	/* Sony's: compared, nothing done */
				continue;
			}

			LsdbAppInfoPtr app(new LsdbAppInfo());

			if (Bgdl_81123BC0(titleId, &app) != 0)
				continue;

			unsigned int key = LSDB_KEY_GAMEDATA_ID;
			const char *gameDataId = LsdbValueText((*app)[key]);

			if (gameDataId != NULL) {
				vector<string> titles;
				LsdbQueryCondition condition;

				condition.op = LSDB_QUERY_OP_EQUAL;
				condition.next = NULL;
				condition.key = LSDB_KEY_GAMEDATA_ID;
				condition.values.push_back(LsdbValue(gameDataId));
				if (Bgdl_811123BA(&condition, &titles) < 0)
					continue;
				if (titles.size() >= 2)
					continue;
				remove.push_back(list[i]);
			}
		}
		for (i = 0; i < remove.size(); i++) {
			CancelWaitingTask(remove[i].id);
			if (bgdlRemoveTask(remove[i].id) != 0)
				break;
		}
	}
	return ret;
}

/*
 * (0x8110C8B4) PLACEHOLDER name: the title's waiting game data and patch tasks (key 111 0, key
 * 114 0, key 107 8 or 9) cancelled and removed
 */
int BgdlManager::RemoveTitleTasks(const char *titleId)
{
	vector<BgdlTaskEntry> list;
	vector<BgdlTaskEntry> remove;
	string title;
	int ret = 0;
	SceUInt32 i;

	if (titleId == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
	} else {
		ret = ListTasks(&list, 0);
		if (ret != 0)
			return ret;

		BgdlTaskPackage package;

		title = titleId;
		remove.clear();
		for (i = 0; i < list.size(); i++) {
			EseTask *task = FindTask(list[i].id);
			EseParamCache param;

			if (task == NULL)
				continue;
			param = task->m_ParamCache;
			if (param.kind != 0)
				continue;
			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			if (package.key220.length() == 0)
				continue;
			if (param.key107 == 7)
				continue;
			if (package.key220 == title && (param.key107 == 8 || param.key107 == 9))
				remove.push_back(list[i]);
		}
		for (i = 0; i < remove.size(); i++) {
			CancelWaitingTask(remove[i].id);
			if (bgdlRemoveTask(remove[i].id) != 0)
				break;
		}
	}
	return ret;
}

/*
 * (0x8110CB90) PLACEHOLDER name: the waiting tasks for an installed title removed, by its
 * storage kind: 1 its game data and patches, 2 its packages (when their game data is its own)
 */
int BgdlManager::RemoveInstalledTitleTasks(const char *titleId)
{
	int ret;

	if (titleId == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
		goto out;
	}
	{
		LsdbAppInfoPtr app(new LsdbAppInfo());
		int kind;

		if (Bgdl_81123BC0(titleId, &app) != 0) {
			ret = BGDL_ERROR_NO_APP;
			goto out;
		}
		ret = Bgdl_811138CC(&app, &kind);
		if (ret != 0)
			goto out;
		ret = 0;
		if (kind <= 2 && kind > 0) {
			if (kind > 1)
				ret = RemoveGameDataTasks(titleId);
			else
				ret = RemoveTitleTasks(titleId);
		}
	}
out:
	UpdateQueueStatus();
	return ret;
}

/* (0x8110CD86) PLACEHOLDER name */
int bgdlDeletePrepare(const char *titleId)
{
	int ret;

	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	ret = s_manager->RemoveInstalledTitleTasks(titleId);
	if (ret == 0)
		Bgdl_8112816E(titleId);
	return ret;
}

/*
 * (0x8110CDB6) PLACEHOLDER name: the folder the task installs into (its folder, then the title)
 * is there; for game data (key 107 8) with key 500 not in its param file also its "0000"
 * folder, for a patch (9) the folder its param file names (key 230)
 */
int bgdlCheckTaskFolder(const EseTask *task)
{
	EseParamCache param;
	int ret;

	if (task == NULL)
		return BGDL_ERROR_INVALID_ARGUMENT;
	param = task->m_ParamCache;

	BgdlTaskPackage package;

	s_packageMutex.Lock();
	package = task->package;
	s_packageMutex.Unlock();
	if (package.key220.length() == 0)
		return BGDL_ERROR_NO_TITLE_ID;

	BgdlTaskPath path;

	ret = ((EseTask *)task)->GetFolder(&path, true);
	if (ret != 0)
		return ret;

	string dir(path.m_begin);

	dir += "/";
	dir += package.key220.c_str();
	if (Dir::IsExist(dir.c_str()) == 0)
		return BGDL_ERROR_NO_FOLDER;
	switch (param.key107) {
	case 6:
	case 7:
		ret = 0;
		break;
	case 8: {
		int value = 0;

		if (Bgdl_8112260C(task->id, 500, &value, sizeof(value)) != 0) {
			ret = 0;
		} else {
			char number[16] = {0};
			string sub;

			sce_paf_snprintf(number, sizeof(number) - 1, "%04d", 0);
			sub = dir;
			sub += "/";
			sub += number;
			if (Dir::IsExist(sub.c_str()) == 0)
				ret = BGDL_ERROR_NO_FOLDER;
			else
				ret = 0;
		}
		break;
	}
	case 9: {
		string folder;

		ret = Bgdl_8112278A(task->id, 230, &folder);
		if (ret == 0) {
			if (folder.length() == 0 || Dir::IsExist(folder.c_str()) == 0)
				ret = BGDL_ERROR_NO_FOLDER;
			else
				ret = 0;
		}
		break;
	}
	default:
		ret = BGDL_ERROR_NO_FOLDER;
		break;
	}
	if (ret != 0)
		return ret;
	return 0;
}

/*
 * (0x8110D34E) PLACEHOLDER name: an installed task's end. Done: its keys written, it is taken
 * off the list and its files deleted. Failed: when the content is there anyway (or the error
 * says so), the error becomes BGDL_ERROR_ALREADY_INSTALLED and only a content or key 107 12 task
 * keeps it; the error is written and told.
 */
void BgdlManager::FinishInstall(SceUInt32 id, EseTask *task, int *result, BgdlTaskMark *mark)
{
	BgdlEvent event;

	if (*result == 0) {
		bool key102 = true;
		bool key205 = true;
		int runStatus = 0;
		EseTask **pos;

		task->m_ParamCache.key102 = true;
		task->result = 0;
		task->m_ParamCache.runStatus = 0;
		task->m_ParamCache.ready = true;
		{
			EseParam file(id);

			if (file.openWrite() == 0 && file.write(101, &runStatus, sizeof(runStatus)) == 0 &&
			    file.write(102, &key102, sizeof(key102)) == 0 &&
			    file.write(205, &key205, sizeof(key205)) == 0 &&
			    file.write(104, result, sizeof(*result)) == 0)
				file.commit();
		}
		event.type = 3;
		event.id = id;
		event.reserved = 0;
		event.value = 0;
		event.param = task->m_ParamCache;
		event.result = *result;
		NotifyListeners(&event);
		m_taskListMutex.Lock();
		pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
		if (pos != m_tasks.m_end && id < (*pos)->id)
			pos = m_tasks.m_end;
		if (pos != m_tasks.m_end) {
			delete *pos;
			m_tasks.Erase(pos);
			if (mark != NULL && mark->marked == true)
				mark->marked = false;
		}
		m_taskListMutex.Unlock();
		deleteTask(id, &event.param);
		goto out;
	}

	switch (*result) {
	case (int)0x80101114:
	case (int)0x80101137:
	case (int)0x80101146:
	case (int)0x80870006:
		*result = BGDL_ERROR_ALREADY_INSTALLED;
		break;
	case (int)0x80871C06:
	case (int)0x80871C07:
	case (int)0x80103A55:
		break;
	case (int)0x8010111C:
		if (task->m_ParamCache.key107 == 7)
			break;
		/* fall through */
	default:
		if (bgdlCheckTaskFolder(task) == 0)
			*result = BGDL_ERROR_ALREADY_INSTALLED;
		break;
	}
	if (*result == BGDL_ERROR_ALREADY_INSTALLED && task->m_ParamCache.key107 != 6 &&
	    task->m_ParamCache.key107 != 12)
		goto out;
	{
		int runStatus = 0;

		task->result = *result;
		{
			EseParam file(id);

			if (file.openWrite() == 0 && file.write(101, &runStatus, sizeof(runStatus)) == 0 &&
			    file.write(104, result, sizeof(*result)) == 0)
				file.commit();
		}
		event.type = 3;
		event.id = id;
		event.reserved = 0;
		event.value = 0;
		event.param = task->m_ParamCache;
		event.result = *result;
		NotifyListeners(&event);
	}
out:
	UpdateQueueStatus();
}

/* (0x8110D722) PLACEHOLDER name: the task installed (power locked, marked against removal) */
int BgdlManager::InstallTask(const SceUInt32 *id)
{
	int result = 0;
	EseTask *task = FindTask(*id);

	if (task == NULL)
		return BGDL_ERROR_NO_TASK;

	BgdlPowerLock lock(true, 0);
	BgdlTaskMark mark(*id, task);

	if (task->m_removing == false) {
		mark.marked = true;
		task->m_removing = true;
	} else if (task->m_removing == true) {
		return BGDL_ERROR_TASK_BUSY;
	}
	if (result == 0)
		result = Bgdl_811219B8(task);
	FinishInstall(*id, task, &result, &mark);
	return result;
}

/*
 * (0x8110D7E2) PLACEHOLDER name: a description of each of the title's waiting tasks of the kind
 * (type 1 to 3, else 0) that flags asks for: 1 its package, 2 its game data, 4 its patch, 8 its
 * content (key 107 7, 8, 9, 6; content only for type 2 and 3)
 */
int BgdlManager::GetTitleTaskInfos(const char *titleId, vector<BgdlTaskInfo> *infos,
				   SceUInt32 flags, int type)
{
	string title;
	int ret;

	if (titleId == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
	} else {
		vector<BgdlTaskEntry> list;
		int kind;
		SceUInt32 i;

		title = titleId;
		if (type == 1)
			kind = 1;
		else if (type == 2)
			kind = 2;
		else if (type == 3)
			kind = 3;
		else
			kind = 0;
		ret = s_manager->FindTitleTasks(&list, &title, kind);
		if (ret != 0)
			return ret;
		for (i = 0; i < list.size(); i++) {
			EseTask *task = FindTask(list[i].id);
			int category;

			if (task == NULL)
				continue;
			switch (list[i].param.key107) {
			case 8:
				if ((flags & 2) == 0)
					continue;
				category = 2;
				break;
			case 9:
				if ((flags & 4) == 0)
					continue;
				category = 4;
				break;
			case 6:
				if (!((type == 2 && (flags & 8) != 0) || (type == 3 && (flags & 8) != 0)))
					continue;
				sce_paf_printf_dummy();
				category = 8;
				break;
			case 7:
				if ((flags & 1) == 0)
					continue;
				category = 1;
				break;
			default:
				continue;
			}

			BgdlTaskPackage package;
			EseParamCache param;

			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			param = task->m_ParamCache;

			BgdlTaskInfo info;
			bool hasContentId;

			sce_paf_memset(&info, 0, sizeof(info));
			info.taskId = list[i].id;
			info.url = package.key220;
			info.kind = type;
			info.category = category;
			info.reserved5 = false;
			info.path = titleId;
			info.reserved0 = false;
			info.reserved1 = false;
			info.title = "";
			info.titleId = "";
			info.contentId = "";
			hasContentId = false;
			info.reserved6 = param.key116 && type == 0 && category == 2;
			{
				EseParam file(info.taskId);

				if (file.open() == 0) {
					file.read(106, &info.title);
					file.read(105, &info.titleId);
					file.read(239, &hasContentId, sizeof(hasContentId));
					if (hasContentId == true)
						file.read(230, &info.contentId);
				}
			}
			infos->push_back(info);
		}
		ret = 0;
	}
	return ret;
}

/*
 * (0x8110DB8C) PLACEHOLDER name: a task's icon read from icon, else from fallback (*fromFallback
 * then set), else indicator_plugin's (0xBB2891AC); 0 when there is one
 */
int bgdlLoadTaskIcon(const string *icon, const string *fallback, intrusive_ptr<graph::Surface> *surface,
		     bool *fromFallback)
{
	int error = 0;

	*surface = NULL;
	*fromFallback = false;
	if (icon->length() != 0) {
		graph::Surface::LoadOption option;
		common::SharedPtr<File> file = LocalFile::Open(icon->c_str(), SCE_O_RDONLY, 0, &error);

		if (error == 0) {
			Plugin *plugin = Plugin::Find("download_plugin");

			if (plugin != NULL)
				*surface = graph::Surface::Load(plugin->m_surface_pool, file, &option);
			file->Close();
		}
	}
	if (surface->get() == NULL && fallback->length() != 0) {
		graph::Surface::LoadOption option;
		common::SharedPtr<File> file = LocalFile::Open(fallback->c_str(), SCE_O_RDONLY, 0, &error);

		if (error == 0) {
			Plugin *plugin = Plugin::Find("download_plugin");

			if (plugin != NULL) {
				*surface = graph::Surface::Load(plugin->m_surface_pool, file, &option);
				if (surface->get() != NULL)
					*fromFallback = true;
			}
			file->Close();
		}
	}
	if (surface->get() == NULL) {
		Plugin *plugin = Plugin::Find("indicator_plugin");

		if (plugin != NULL)
			*surface = plugin->GetTexture(IDParam("", 0xBB2891AC));
	}
	if (surface->get() != NULL)
		return 0;
	return -1;
}

/*
 * (0x8110DFA4) PLACEHOLDER name: game_plugin's promote dialog opened for the task, with its icon
 * (for game data the title's own as the fallback) and download_plugin's text 0x2A862367; the
 * dialog, or -1
 */
int bgdlOpenTaskDialog(BgdlTaskInfo *info)
{
	bool fromFallback = false;
	intrusive_ptr<graph::Surface> icon;
	int ret = -1;
	string fallback("");

	if (info->category == 2 && info->url.length() != 0)
		fallback = Bgdl_81113DE6(info->url.c_str());
	if (bgdlLoadTaskIcon(&info->title, &fallback, &icon, &fromFallback) == 0) {
		Plugin *game = Plugin::Find("game_plugin");

		if (game != NULL) {
			GamePluginInterface *iface = (GamePluginInterface *)game->GetInterface(1);

			if (iface != NULL) {
				wstring text(L"");
				Plugin *download = Plugin::Find("download_plugin");

				if (download != NULL)
					text = download->GetString(IDParam("", 0x2A862367));
				thread::RMutex::main_thread_mutex.Lock();
				ret = ((BgdlDialogOpenFunc)iface->func[GAME_PLUGIN_FUNC_DIALOG_OPEN])(
					info->titleId, icon, text, fromFallback, &info->progress, false);
				thread::RMutex::main_thread_mutex.Unlock();
			}
		}
	}
	return ret;
}

/*
 * (0x8110E19C) PLACEHOLDER name: the title's waiting tasks of the type (1 to 3, else 0; for 2
 * and 3 its content too) installed one by one, each under game_plugin's promote dialog. On a
 * failure *failed describes the task (path the title). *patched is set when a patch was
 * installed; game data of key 116 gives BGDL_ERROR_GAME_DATA_RESTART.
 */
int BgdlManager::InstallTitleTasks(const char *titleId, int type, BgdlTaskInfo *failed,
				   bool *patched)
{
	vector<BgdlTaskInfo> infos;
	int ret;

	if (titleId == NULL) {
		ret = BGDL_ERROR_INVALID_ARGUMENT;
	} else {
		SceUInt32 flags = 7;
		int kind;

		if (type == 1) {
			kind = 1;
		} else if (type == 2) {
			kind = 2;
			flags = 15;
		} else if (type == 3) {
			kind = 3;
			flags = 15;
		} else {
			kind = 0;
		}
		ret = GetTitleTaskInfos(titleId, &infos, flags, kind);
		if (ret == 0) {
			if (infos.size() != 0) {
				bool restart = false;
				SceUInt32 i;

				for (i = 0; i < infos.size(); i++) {
					int dialog;

					infos[i].reserved1 = true;
					infos[i].progress = 0;
					dialog = bgdlOpenTaskDialog(&infos[i]);
					ret = InstallTask((const SceUInt32 *)&infos[i].taskId);
					if (dialog >= 0) {
						Plugin *game = Plugin::Find("game_plugin");

						if (game != NULL) {
							GamePluginInterface *iface =
								(GamePluginInterface *)game->GetInterface(1);

							if (iface != NULL) {
								thread::RMutex::main_thread_mutex.Lock();
								((void (*)(int))iface->func[GAME_PLUGIN_FUNC_DIALOG_CLOSE])(
									dialog);
								thread::RMutex::main_thread_mutex.Unlock();
							}
						}
					}
					if (ret != 0) {
						if (failed != NULL) {
							*failed = infos[i];
							failed->url = infos[i].url;
							failed->path = infos[i].path;
							failed->title = infos[i].title;
							failed->titleId = infos[i].titleId;
							failed->path = titleId;
						}
						break;
					}
					if (infos[i].category == 2) {
						if (titleId != NULL)
							Bgdl_812A9954(titleId, 1);
						restart = infos[i].reserved6;
					}
					if (infos[i].category == 4 && patched != NULL)
						*patched = true;
				}
				if (ret == 0 && restart == true)
					ret = BGDL_ERROR_GAME_DATA_RESTART;
			} else {
				ret = 0;
			}
		}
	}
	return ret;
}

/* (0x8110E400) PLACEHOLDER name */
int bgdlInstallTitleTasks(const char *titleId, BgdlTaskInfo *failed, bool *patched)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->InstallTitleTasks(titleId, 0, failed, patched);
}

/* (0x8110E430) PLACEHOLDER name */
int bgdlInstallTitleTasksType1(const char *titleId, BgdlTaskInfo *failed)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->InstallTitleTasks(titleId, 1, failed, NULL);
}

/* (0x8110E45E) PLACEHOLDER name */
int bgdlInstallTitleTasksType2(const char *titleId, BgdlTaskInfo *failed)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->InstallTitleTasks(titleId, 2, failed, NULL);
}

/* (0x8110E48C) PLACEHOLDER name: type, flags and patched are not read */
int bgdlInstallTitleTasksType3(const char *titleId, int type, SceUInt32 flags, BgdlTaskInfo *failed,
			       bool *patched)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->InstallTitleTasks(titleId, 3, failed, NULL);
}

/*
 * (0x8110E4B8) PLACEHOLDER name: InstallTitleTasks for the kinds flags asks for, the promote
 * dialog only when dialog is set
 */
int BgdlManager::InstallTitleTasks(const char *titleId, int type, SceUInt32 flags, bool dialog,
				   BgdlTaskInfo *failed, bool *patched)
{
	vector<BgdlTaskInfo> infos;
	int ret;

	if (titleId == NULL) {
		ret = BGDL_ERROR_INVALID_ARGUMENT;
	} else {
		ret = GetTitleTaskInfos(titleId, &infos, flags, type);
		if (ret == 0) {
			if (infos.size() != 0) {
				int id = -1;
				SceUInt32 i;

				for (i = 0; i < infos.size(); i++) {
					infos[i].reserved1 = true;
					infos[i].progress = 0;
					if (dialog == true)
						id = bgdlOpenTaskDialog(&infos[i]);
					ret = InstallTask((const SceUInt32 *)&infos[i].taskId);
					if (dialog == true && id >= 0) {
						Plugin *game = Plugin::Find("game_plugin");

						if (game != NULL) {
							GamePluginInterface *iface =
								(GamePluginInterface *)game->GetInterface(1);

							if (iface != NULL) {
								thread::RMutex::main_thread_mutex.Lock();
								((void (*)(int))iface->func[GAME_PLUGIN_FUNC_DIALOG_CLOSE])(
									id);
								thread::RMutex::main_thread_mutex.Unlock();
							}
						}
					}
					if (ret != 0) {
						if (failed != NULL) {
							*failed = infos[i];
							failed->url = infos[i].url;
							failed->path = infos[i].path;
							failed->title = infos[i].title;
							failed->titleId = infos[i].titleId;
							failed->path = titleId;
						}
						break;
					}
					if (infos[i].category == 2 && titleId != NULL)
						Bgdl_812A9954(titleId, 1);
					if (infos[i].category == 4 && patched != NULL)
						*patched = true;
				}
			} else {
				ret = 0;
			}
		}
	}
	return ret;
}

/* (0x8110E6DE) PLACEHOLDER name */
int bgdlInstallTitleTasksOfKinds(const char *titleId, int type, SceUInt32 flags, bool dialog,
				 BgdlTaskInfo *failed, bool *patched)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->InstallTitleTasks(titleId, type, flags, dialog, failed, patched);
}

/* (0x8110E716) PLACEHOLDER name: the title the task is for (its package's) */
int bgdlGetTaskTitle(SceUInt32 id, string *titleId)
{
	EseTask *task;

	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	task = s_manager->FindTask(id);
	if (task == NULL)
		return BGDL_ERROR_NO_TASK;

	BgdlTaskPackage package;

	s_packageMutex.Lock();
	package = task->package;
	s_packageMutex.Unlock();
	if (package.key220.length() == 0)
		return BGDL_ERROR_NO_TITLE_ID;
	*titleId = package.key220.c_str();
	return 0;
}

/*
 * (0x8110E884) PLACEHOLDER name: *count the waiting content tasks (key 107 6 or 12, key 113 set,
 * no error) that are not being handled, listed in list when it is not NULL
 */
int bgdlCountTasks(SceUInt32 *count, vector<BgdlTaskEntry> *list)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	*count = 0;

	vector<BgdlTaskEntry> tasks;
	SceUInt32 i;
	int ret;

	ret = s_manager->ListTasks(&tasks, 0);
	if (ret != 0)
		return ret;
	for (i = 0; i < tasks.size(); i++) {
		if (Bgdl_81114524(tasks[i].id) == true)
			continue;
		if (!tasks[i].param.key113)
			continue;
		if (tasks[i].param.key107 != 6 && tasks[i].param.key107 != 12)
			continue;
		if (tasks[i].result == 0) {
			++*count;
			if (list != NULL)
				list->push_back(tasks[i]);
		}
	}
	return 0;
}

/*
 * (0x8110E99A) PLACEHOLDER name: a description of each waiting content task (key 107 6 or 12,
 * key 113 set, not running, no error), but for content of the game being run
 */
int BgdlManager::GetContentTaskInfos(vector<BgdlTaskInfo> *infos)
{
	vector<BgdlTaskEntry> list;
	SceUInt32 i;
	int ret;

	infos->clear();
	ret = s_manager->ListTasks(&list, 0);
	if (ret != 0)
		return ret;
	for (i = 0; i < list.size(); i++) {
		EseTask *task = FindTask(list[i].id);
		EseParamCache param;

		if (task == NULL)
			continue;
		if (task->m_ParamCache.runStatus != 0 || task->result != 0 || !task->m_ParamCache.key113)
			continue;
		if (list[i].param.key107 != 6 && list[i].param.key107 != 12)
			continue;

		BgdlTaskPackage package;

		s_packageMutex.Lock();
		package = task->package;
		s_packageMutex.Unlock();
		param = task->m_ParamCache;
		if (param.key107 == 6 && param.kind == 2 && Bgdl_811144CE(&package.key220) == true)
			continue;

		BgdlTaskInfo info;
		bool hasContentId;

		sce_paf_memset(&info, 0, sizeof(info));
		if (list[i].param.key107 == 12)
			info.category = 16;
		else if (list[i].param.key107 == 6)
			info.category = 8;
		info.taskId = list[i].id;
		info.url = package.key220;
		if (param.kind == 1)
			info.kind = 1;
		else if (param.kind == 2)
			info.kind = 2;
		else if (param.kind == 3)
			info.kind = 3;
		else
			info.kind = 0;
		info.reserved0 = false;
		info.reserved1 = false;
		info.reserved5 = false;
		info.path = package.key220;
		info.title = "";
		info.titleId = "";
		hasContentId = false;
		{
			EseParam file(info.taskId);

			if (file.open() == 0) {
				info.contentId = "";
				file.read(106, &info.title);
				file.read(105, &info.titleId);
				file.read(239, &hasContentId, sizeof(hasContentId));
				if (hasContentId == true)
					file.read(230, &info.contentId);
			}
		}
		infos->push_back(info);
	}
	return 0;
}

/* (0x8110ED40) PLACEHOLDER name */
int bgdlGetContentTaskInfos(vector<BgdlTaskInfo> *infos)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->GetContentTaskInfos(infos);
}

/* (0x8110ED62) PLACEHOLDER name */
int bgdlInstallTask(const SceUInt32 *id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->InstallTask(id);
}

/* (0x8110ED8A) PLACEHOLDER name */
int bgdlGetTitleTaskInfos(const char *titleId, vector<BgdlTaskInfo> *infos, SceUInt32 flags, int type)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->GetTitleTaskInfos(titleId, infos, flags, type);
}

/* (0x8110EDBA) PLACEHOLDER name: a description of the task, when it waits to be installed */
int BgdlManager::GetTaskInfo(SceUInt32 id, BgdlTaskInfo *info)
{
	EseTask *task = FindTask(id);
	EseParamCache param;

	if (task == NULL)
		return BGDL_ERROR_NO_TASK;
	if (task->m_ParamCache.runStatus != 0 || !task->m_ParamCache.key113 || task->result != 0)
		return BGDL_ERROR_NOT_WAITING;

	BgdlTaskPackage package;

	s_packageMutex.Lock();
	package = task->package;
	s_packageMutex.Unlock();
	param = task->m_ParamCache;

	bool hasContentId;

	sce_paf_memset(info, 0, sizeof(*info));
	info->taskId = id;
	info->url = package.key220;
	info->reserved0 = false;
	info->reserved1 = false;
	if (param.kind == 1)
		info->kind = 1;
	else if (param.kind == 2)
		info->kind = 2;
	else if (param.kind == 3)
		info->kind = 3;
	else
		info->kind = 0;
	if (param.key107 == 7)
		info->category = 1;
	else if (param.key107 == 8)
		info->category = 2;
	else if (param.key107 == 9)
		info->category = 4;
	else
		info->category = 8;
	info->reserved5 = false;
	info->path = package.key220;
	info->title = "";
	info->titleId = "";
	info->contentId = "";
	hasContentId = false;
	{
		EseParam file(info->taskId);

		if (file.open() == 0) {
			file.read(106, &info->title);
			file.read(105, &info->titleId);
			file.read(239, &hasContentId, sizeof(hasContentId));
			if (hasContentId == true)
				file.read(230, &info->contentId);
		}
	}
	return 0;
}

/* (0x8110F018) PLACEHOLDER name */
int bgdlGetTaskInfo(SceUInt32 id, BgdlTaskInfo *info)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->GetTaskInfo(id, info);
}

/* (0x8110F03C) PLACEHOLDER name: the same as bgdlInstallTask */
int bgdlInstallTask2(const SceUInt32 *id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->InstallTask(id);
}

/*
 * (0x8110F064) PLACEHOLDER name: a waiting task whose install found what it installs already
 * there is made to wait again: its result cleared in it and its param file
 */
int BgdlManager::ResetInstalledTask(SceUInt32 id)
{
	int ret = 0;
	EseTask *task = FindTask(id);

	if (task == NULL)
		return BGDL_ERROR_NO_TASK;
	if (task->m_ParamCache.runStatus != 0 || !task->m_ParamCache.key113)
		return BGDL_ERROR_NO_TASK;
	switch (task->result) {
	case 0:
	case BGDL_ERROR_ALREADY_INSTALLED:
		break;
	default:
		return BGDL_ERROR_NO_TASK;
	}
	if (task->result != BGDL_ERROR_ALREADY_INSTALLED)
		return BGDL_ERROR_NO_TASK;
	task->result = 0;
	{
		EseParam file(id);

		ret = file.openWrite();
		if (ret == 0) {
			ret = file.write(104, &ret, sizeof(ret));
			if (ret == 0)
				ret = file.commit();
		}
	}
	if (ret == 0) {
		BgdlEvent event;

		event.type = 3;
		event.id = id;
		event.reserved = 0;
		event.value = 0;
		event.param = task->m_ParamCache;
		event.result = task->result;
		NotifyListeners(&event);
		UpdateQueueStatus();
		if (task->m_ParamCache.key107 == 6)
			shellEventServerSend(1, NULL, 0);
	}
	return ret;
}

/* (0x8110F1B6) PLACEHOLDER name */
int bgdlResetInstalledTask(SceUInt32 id)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->ResetInstalledTask(id);
}

/* (0x8110F1D8) PLACEHOLDER name */
void BgdlManager::SetHold(bool hold)
{
	m_hold = hold;
}

/* (0x8110F1DE) PLACEHOLDER name */
void bgdlHold(void)
{
	if (s_manager != NULL)
		s_manager->m_hold = true;
}

/* (0x8110F1F4) PLACEHOLDER name */
void bgdlSuspend(bool reserved)
{
	if (s_manager != NULL)
		s_manager->m_suspended = true;
}

/* (0x8110F208) PLACEHOLDER name */
int bgdlAddPsmRuntimeTask(paf::string *iconPath, int arg1, SceUInt64 *needed)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->AddPsmRuntimeTask(iconPath, 0, arg1, needed);
}

/*
 * (0x8110F238) PLACEHOLDER name: the version (entries 503 and 504) of the PSM runtime's waiting
 * patch task; true when one is found. Without one, 1.0 when the runtime is installed.
 */
bool BgdlManager::GetPsmRuntimeVersion(SceUInt16 *version)
{
	bool found = false;
	vector<BgdlTaskInfo> infos;
	SceUInt32 i;

	version[0] = 0;
	version[1] = 0;
	if (GetTitleTaskInfos("PCSI00011", &infos, 2, 0) == 0) {
		for (i = 0; i < infos.size(); i++) {
			if (Bgdl_8112260C(infos[i].taskId, 503, &version[0], sizeof(version[0])) == 0 &&
			    Bgdl_8112260C(infos[i].taskId, 504, &version[1], sizeof(version[1])) == 0) {
				found = true;
				goto out;
			}
		}
	}
	{
		LsdbAppInfoPtr app(new LsdbAppInfo());

		if (Bgdl_81123BC0("PCSI00011", &app) == 0) {
			version[1] = 0;
			version[0] = 1;
		}
	}
out:
	return found;
}

/* (0x8110F3A2) PLACEHOLDER name */
bool bgdlGetPsmRuntimeVersion(SceUInt16 *version)
{
	if (s_manager == NULL)
		return false;
	return s_manager->GetPsmRuntimeVersion(version);
}

/* (0x8110F3BE) PLACEHOLDER name: the task waits to be installed */
bool bgdlTaskIsWaiting(SceUInt32 id)
{
	EseTask *task = NULL;

	if (s_manager != NULL) {
		task = s_manager->FindTask(id);
		if (task == NULL || task->m_ParamCache.runStatus != 0 || !task->m_ParamCache.key113 ||
		    task->result != 0)
			task = NULL;
	}
	return task != NULL;
}

/*
 * (0x8110F3FA) PLACEHOLDER name: a task of the query's kind (key 111), not yet done (state below
 * 3), running or failed, has the query's content id as its entry 234
 */
int BgdlManager::HasContentTask(const BgdlContentQuery *query)
{
	EseTask **it;
	int found;

	if (query == NULL)
		return BGDL_ERROR_NULL_TITLE;
	m_taskListMutex.Lock();

	string value;

	found = 0;
	for (it = m_tasks.m_begin; it != m_tasks.m_end; it++) {
		if (((*it)->m_ParamCache.runStatus != 0 ||
		     ((*it)->result != 0 && !(*it)->m_ParamCache.key113)) &&
		    query->kind == (*it)->m_ParamCache.key111 && (*it)->m_ParamCache.state < 3) {
			value.clear();
			if (Bgdl_8112278A((*it)->id, 234, &value) == 0 && value.length() != 0 &&
			    sce_paf_strncmp(value.c_str(), query->contentId, sizeof(query->contentId)) == 0) {
				found = 1;
				break;
			}
		}
	}
	m_taskListMutex.Unlock();
	return found;
}

/* (0x8110F4EA) PLACEHOLDER name */
int bgdlHasContentTask(const BgdlContentQuery *query)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->HasContentTask(query);
}

/*
 * (0x8110F50C) PLACEHOLDER name: the tasks of the query's kind (key 111; 7: any), not yet done,
 * running or failed, whose package is the query's content, counted
 */
int BgdlManager::CountContentTasks(const BgdlContentQuery *query)
{
	EseTask **it;
	int count;

	if (query == NULL)
		return BGDL_ERROR_NULL_TITLE;
	m_taskListMutex.Lock();

	string contentId(query->contentId);

	count = 0;
	for (it = m_tasks.m_begin; it != m_tasks.m_end; it++) {
		if (((*it)->m_ParamCache.runStatus != 0 ||
		     ((*it)->result != 0 && !(*it)->m_ParamCache.key113)) &&
		    (query->kind == 7 || query->kind == (*it)->m_ParamCache.key111) &&
		    (*it)->m_ParamCache.state < 3) {
			BgdlTaskPackage package;
			EseTask *task = *it;

			s_packageMutex.Lock();
			package = task->package;
			s_packageMutex.Unlock();
			if (package.key220.length() != 0 && package.key217 == contentId)
				count++;
		}
	}
	m_taskListMutex.Unlock();
	return count;
}

/* (0x8110F70C) PLACEHOLDER name: grw0: unmounted */
int bgdlUnmountGrw0(void)
{
	Bgdl_811137F6(false);
	return 0;
}

/* (0x8110F718) PLACEHOLDER name */
bool bgdlHasPending(void)
{
	if (s_manager == NULL)
		return false;
	return s_manager->m_hasPending;
}

/* (0x8110F72C) PLACEHOLDER name: the folder the task's title is downloaded into */
static __attribute__((noinline)) string bgdlTaskTitleFolder(EseTask *task)
{
	string dir("");
	BgdlTaskPath path;

	if (task->GetFolder(&path, true) == 0) {
		dir = path.m_begin;
		dir += "/";

		BgdlTaskPackage package;

		s_packageMutex.Lock();
		package = task->package;
		s_packageMutex.Unlock();
		if (package.key220.length() != 0)
			dir += package.key220.c_str();
	}
	return dir;
}

/*
 * (0x8110F94A) PLACEHOLDER name: the version game_plugin reads from the last package file of the
 * title's waiting patch task
 */
int BgdlManager::GetPatchVersion(const char *titleId, SceUInt8 *version)
{
	vector<BgdlTaskInfo> infos;

	if (version != NULL && GetTitleTaskInfos(titleId, &infos, 2, 0) == 0 && infos.size() != 0) {
		string dir = bgdlTaskTitleFolder(FindTask(infos[0].taskId));

		if (dir.length() != 0) {
			int count = 0;

			if (Bgdl_8112260C(infos[0].taskId, 500, &count, sizeof(count)) == 0) {
				string file = Bgdl_811115D0(dir, count > 0 ? count - 1 : 0);

				if (file.length() != 0) {
					GamePluginInterface *iface = Bgdl_811115BA();

					if (iface != NULL)
						((BgdlPatchVersionFunc)iface->func[BGDL_GAME_PLUGIN_FUNC_PATCH_VERSION])(
							&file, &version[0], &version[1], &version[2]);
				}
			}
		}
	}
	return 0;
}

/* (0x8110FB04) PLACEHOLDER name */
int bgdlGetPatchVersion(const char *titleId, SceUInt8 *version)
{
	if (s_manager == NULL)
		return BGDL_ERROR_NOT_STARTED;
	return s_manager->GetPatchVersion(titleId, version);
}

/* (0x8110FB28) PLACEHOLDER name: what listing the tasks of type 1 returns */
int bgdlListMtpTasks(void)
{
	if (s_manager == NULL)
		return 0;

	vector<BgdlTaskEntry> list;

	return s_manager->ListTasks(&list, 1);
}

/* (0x8110FB76) PLACEHOLDER name: an MTP copy may be asked for */
int BgdlManager::CheckMtpContent(const BgdlMtpContent *content)
{
	int ret;

	if (content == NULL)
		return BGDL_ERROR_NULL_TITLE;
	ret = bgdlCheckQueue();
	if (ret != 0)
		return ret;
	if (bgdlListMtpTasks() >= 15)
		return BGDL_ERROR_MTP_QUEUE_FULL;
	switch (content->kind) {
	case 0:
	case 1:
	case 2:
	case 3:
		return 0;
	}
	return BGDL_ERROR_NULL_TITLE;
}

/* (0x8110FC3E) PLACEHOLDER name: reserved is not read */
int bgdlCheckMtpContent(const BgdlMtpContent *content, int reserved)
{
	if (s_manager == NULL)
		return BGDL_ERROR_MTP_NOT_STARTED;
	return s_manager->CheckMtpContent(content);
}

/*
 * (0x8110FC62) PLACEHOLDER name: an MTP copy made a task of type 1 (key 107 1 to 3 by the
 * content's kind, else 6), with its mtp_contents.bin made that size first; *id its id
 */
int BgdlManager::AddMtpTask(const BgdlMtpRequest *request, SceUInt32 *id)
{
	bool key112 = false;
	bool key102 = false;
	bool key218 = false;
	BgdlTaskPath folder;
	bool key205 = true;
	int key107 = 0;
	int key108 = 1;
	SceUInt32 taskId = (SceUInt32)-1;
	string path("");
	int key114 = 0;
	int ret;

	Bgdl_81113890(1);
	if (request == NULL) {
		ret = BGDL_ERROR_NULL_TITLE;
		goto fail;
	}
	ret = CheckMtpContent(&request->content);
	if (ret != 0)
		goto fail;
	if (request->type == 0 || request->type == 1)
		taskId = (SceUInt32)-1;
	else if (request->type == 2)
		taskId = (SceUInt32)-2;
	else
		taskId = (SceUInt32)-1;
	if (taskId == (SceUInt32)-1 && m_ux0Ready != true) {
		ret = BGDL_ERROR_NO_UX0;
		goto fail;
	}
	{
		string name("");

		if (request->name[0] != '\0')
			name = request->name;
		ret = AddTask(&taskId, 1, request->type, name.c_str(),
			      (request->flags & BGDL_REQUEST_FLAG_KEY115) != 0,
			      (request->flags & BGDL_REQUEST_FLAG_KEY116) != 0);
		if (ret != 0)
			goto out;
		ret = folder.initialize(taskId, 20);
		if (ret != 0)
			goto out;
		if (folder.m_begin == NULL) {
			ret = BGDL_ERROR_NO_MEMORY;
			goto fail;
		}

		EseParam param(taskId);

		ret = param.openWrite();
		if (ret != 0)
			goto out;
		if ((request->content.options & BGDL_MTP_OPTION_CONTENTS) != 0 &&
		    request->content.size != 0) {
			key112 = true;
			path = folder.m_begin;
			path += "/";
			path += "mtp_contents.bin";
			Bgdl_8111240E(path.c_str(), request->content.size);
		}
		if (request->content.kind == 3) {
			BgdlGamePluginLock lock(true);
			string game = Bgdl_81113C72(taskId, true);

			if (game.length() != 0) {
				ret = param.write(229, game.c_str(), sce_paf_strlen(game.c_str()) + 1);
				if (ret != 0)
					goto out;
			}
		}
		ret = param.write(114, &key114, sizeof(key114));
		if (ret != 0)
			goto out;
		if (request->iconPath[0] != '\0') {
			string icon;

			if (Bgdl_81111CDC(request->iconPath, folder.m_begin, &icon, true) == 0) {
				ret = param.write(106, icon.c_str(), icon.length() + 1);
				if (ret != 0)
					goto out;
			}
		}
		ret = param.write(218, &key218, sizeof(key218));
		if (ret != 0)
			goto out;
		ret = param.write(205, &key205, sizeof(key205));
		if (ret != 0)
			goto out;
		ret = param.write(102, &key102, sizeof(key102));
		if (ret != 0)
			goto out;
		ret = param.write(108, &key108, sizeof(key108));
		if (ret != 0)
			goto out;
		switch (request->content.kind) {
		case 0:
			key107 = 1;
			break;
		case 1:
			key107 = 2;
			break;
		case 2:
			key107 = 3;
			break;
		default:
			key107 = 6;
			break;
		}
		ret = param.write(107, &key107, sizeof(key107));
		if (ret != 0)
			goto out;

		EseTask *task = FindTask(taskId);

		if (task == NULL) {
			ret = BGDL_ERROR_NO_TASK;
			goto fail;
		}
		task->m_ParamCache.key107 = key107;
		task->m_ParamCache.key102 = key102;
		task->m_ParamCache.key108 = key108;
		task->m_ParamCache.key112 = key112;
		ret = param.commit();
		if (ret != 0)
			goto out;
	}
	if (id != NULL) {
		sce_paf_memset(id, 0, sizeof(*id));
		*id = taskId;
	}
	if ((request->flags & BGDL_REQUEST_FLAG_NO_START) == 0)
		ret = ReloadTask(taskId);
out:
	if (ret == 0)
		goto done;
fail:
	RemoveTask(taskId);
done:
	UpdateQueueStatus();
	return ret;
}

/* (0x811103CA) PLACEHOLDER name */
int bgdlAddMtpTask(const BgdlMtpRequest *request, SceUInt32 *id)
{
	if (s_manager == NULL)
		return 0;
	return s_manager->AddMtpTask(request, id);
}

/* (0x811103EA) PLACEHOLDER name: func told of the manager's events, in the first free slot */
int BgdlManager::AddListener(void (*func)(BgdlEvent *event, void *arg), void *arg)
{
	BgdlListener *listener;

	m_listenersMutex.Lock();
	for (listener = m_listeners; listener != m_listeners + BGDL_LISTENER_MAX; listener++) {
		if (listener->func == NULL) {
			listener->func = func;
			listener->arg = arg;
			m_listenerCount++;
			m_listenersMutex.Unlock();
			return 0;
		}
	}
	m_listenersMutex.Unlock();
	return BGDL_ERROR_LISTENERS_FULL;
}

/* (0x81110448) PLACEHOLDER name */
int BgdlManager::RemoveListener(void (*func)(BgdlEvent *event, void *arg), void *arg)
{
	BgdlListener *listener;

	m_listenersMutex.Lock();
	for (listener = m_listeners; listener != m_listeners + BGDL_LISTENER_MAX; listener++) {
		if (listener->func == func && listener->arg == arg) {
			listener->func = NULL;
			m_listenerCount--;
			break;
		}
	}
	m_listenersMutex.Unlock();
	return 0;
}

/* (0x811104A8) PLACEHOLDER name: a task has the id */
int BgdlManager::CheckTask(SceUInt32 id)
{
	EseTask **pos;

	m_taskListMutex.Lock();
	pos = bgdlLowerBoundId(m_tasks.m_begin, m_tasks.m_end, id);
	if (pos != m_tasks.m_end && id < (*pos)->id)
		pos = m_tasks.m_end;
	if (pos == m_tasks.m_end) {
		m_taskListMutex.Unlock();
		return BGDL_ERROR_NO_TASK;
	}
	m_taskListMutex.Unlock();
	return 0;
}

/* (0x81110524) */
static void bgdlManagerOnResume(int code, const void *data, SceSize size, void *arg)
{
	if (s_manager != NULL)
		s_manager->m_resumed = true;
}

/* (0x81110538) libmtp's progress, kept in the task */
static void bgdlMtpNotify(void *arg, const MtpBgCopyProgress *progress)
{
	EseTask *task = (EseTask *)arg;

	if (task != NULL && progress != NULL) {
		BgdlTaskCopy copy;

		copy.name = progress->name;
		copy.progress = progress->progress;
		copy.number = progress->number;
		copy.total = progress->total;
		copy.remaining = progress->remaining;
		copy.reserved = progress->reserved;
		task->copyResult = 0;
		task->copy = copy;
	}
}

/* (0x8111060C) */
static int bgdlMtpAccept(void)
{
	if (s_manager == NULL)
		return 0;
	return s_manager->m_removeRunning;
}
