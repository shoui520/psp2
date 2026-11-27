/*
 * The game plugin's license server: SceNpKdc loaded with its work memory, and an application's
 * license, an additional content's license or a download fetched from the store with the
 * session's EPW, each on a thread of its own ("GameUpdateLicenseSub",
 * "GameUpdateAddContLicenseSub", "GameDownloadLicenseSub") joined under the server's mutex. A
 * license is written first as "_<rif name>" in the drive's temp/license folder, then moved to its
 * place (0x8109FEA4..0x810A12B6). The file's name and the functions' are ours (no record names
 * them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_dirent.h>
#include <kernel/iofilemgr_stat.h>
#include <kernel/threadmgr.h>
#include <kernel/constant.h>
#include <sysmodule/sysmodule.h>
#include <sysmodule/sysmodule_ids.h>
#include <npdrm/npdrm.h>
#include <np/np_kdc/np_kdc.h>
#include <std/stdio.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <file/localfile.h>

#include "game_plugin.h"
#include "game_appinfo.h"
#include "license_updater.h"

using namespace paf;

/* the request threads' priority and stack */
#define GAME_LICENSE_THREAD_PRIORITY	(SCE_KERNEL_DEFAULT_PRIORITY_USER + 10)
#define GAME_LICENSE_THREAD_STACK_SIZE	SCE_KERNEL_32KiB

/* NpKdc's work memory, and the room a license is downloaded into */
#define GAME_LICENSE_KDC_WORK_SIZE	SCE_KERNEL_32KiB
#define GAME_LICENSE_BUF_SIZE		512

/* an application's license folders, and those of a drive's additional contents and temp files */
#define GAME_LICENSE_APP_DIR		"ux0:license/app"
#define GAME_LICENSE_PSP_DIR		"ux0:pspemu/PSP/LICENSE"
#define GAME_LICENSE_DRIVE_PSP_DIR	"pspemu/PSP/LICENSE"
#define GAME_LICENSE_DRIVE_ADDCONT_DIR	"license/addcont"
#define GAME_LICENSE_DRIVE_TEMP_DIR	"temp/license"

/* a request run on its own thread: the server's function called with the request's arguments */
class GameLicenseUpdateThread : public thread::Thread {
public:
	GameLicenseUpdateThread(GameLicenseServer *server)
		: thread::Thread(GAME_LICENSE_THREAD_PRIORITY, GAME_LICENSE_THREAD_STACK_SIZE,
				 "GameUpdateLicenseSub", NULL),
		  m_server(server), m_result(0), m_args(NULL)
	{
	}

	virtual ~GameLicenseUpdateThread()
	{
	}

	virtual void EntryFunction()
	{
		m_result = m_server->DoUpdateLicense(m_args);
	}

	GameLicenseServer *m_server;
	int m_result;
	const GameLicenseUpdateArgs *m_args;
};

class GameAddcontLicenseUpdateThread : public thread::Thread {
public:
	GameAddcontLicenseUpdateThread(GameLicenseServer *server)
		: thread::Thread(GAME_LICENSE_THREAD_PRIORITY, GAME_LICENSE_THREAD_STACK_SIZE,
				 "GameUpdateAddContLicenseSub", NULL),
		  m_server(server), m_result(0), m_args(NULL)
	{
	}

	virtual ~GameAddcontLicenseUpdateThread()
	{
	}

	virtual void EntryFunction()
	{
		m_result = m_server->DoUpdateAddcontLicense(m_args);
	}

	GameLicenseServer *m_server;
	int m_result;
	GameAddcontLicenseUpdateArgs *m_args;
};

class GameLicenseDownloadThread : public thread::Thread {
public:
	GameLicenseDownloadThread(GameLicenseServer *server)
		: thread::Thread(GAME_LICENSE_THREAD_PRIORITY, GAME_LICENSE_THREAD_STACK_SIZE,
				 "GameDownloadLicenseSub", NULL),
		  m_server(server), m_result(0), m_args(NULL)
	{
	}

	virtual ~GameLicenseDownloadThread()
	{
	}

	virtual void EntryFunction()
	{
		m_result = m_server->DoDownloadLicense(m_args);
	}

	GameLicenseServer *m_server;
	int m_result;
	const GameLicenseDownloadArgs *m_args;
};

GameLicenseServer::GameLicenseServer()
	: m_mutex("SceGameUpdateLicense", 0), m_licensePath((const char *)NULL, 0),
	  m_tempPath((const char *)NULL, 0), m_kdcWork(NULL), m_isPsp(SCE_FALSE),
	  m_started(false)
{
}

GameLicenseServer::~GameLicenseServer()
{
}

/* NpKdc loaded and started on its work memory (0x8109FF30) */
int GameLicenseServer::Init(void)
{
	int ret = sceSysmoduleLoadModuleInternal(SCE_SYSMODULE_INTERNAL_NP_KDC);

	if (ret >= 0) {
		m_kdcWork = sce_paf_malloc(GAME_LICENSE_KDC_WORK_SIZE);
		if (m_kdcWork == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		ret = SceNpKdc_26F8A1B9(GAME_LICENSE_KDC_WORK_SIZE, m_kdcWork);
		if (ret != 0) {
			if (m_kdcWork != NULL) {
				sce_paf_free(m_kdcWork);
				m_kdcWork = NULL;
			}
		} else {
			m_started = true;
		}
	}
	return ret;
}

/* NpKdc stopped and unloaded (0x8109FF84) */
int GameLicenseServer::Term(void)
{
	if (!m_started)
		return 0;

	m_started = false;
	SceNpKdc_06CB1007();
	if (m_kdcWork != NULL) {
		sce_paf_free(m_kdcWork);
		m_kdcWork = NULL;
	}
	sceSysmoduleUnloadModuleInternal(SCE_SYSMODULE_INTERNAL_NP_KDC);
	return 0;
}

/* an application's license fetched again (0x8109FFB6) */
int GameLicenseServer::UpdateLicense(const GameLicenseUpdateArgs *args)
{
	if (!m_started)
		return GAME_ERROR_NOT_STARTED;

	m_mutex.Lock();

	GameLicenseUpdateThread *thread = new GameLicenseUpdateThread(this);

	thread->m_args = args;

	int ret = thread->Start();

	if (ret >= 0) {
		thread->Join();
		ret = thread->m_result;
	}
	delete thread;

	m_mutex.Unlock();
	return ret;
}

/* an additional content's license fetched again, its period given back in args (0x810A0040) */
int GameLicenseServer::UpdateAddcontLicense(GameAddcontLicenseUpdateArgs *args)
{
	if (!m_started)
		return GAME_ERROR_NOT_STARTED;

	m_mutex.Lock();

	GameAddcontLicenseUpdateThread *thread = new GameAddcontLicenseUpdateThread(this);

	thread->m_args = args;

	int ret = thread->Start();

	if (ret >= 0) {
		thread->Join();
		ret = thread->m_result;
	}
	delete thread;

	m_mutex.Unlock();
	return ret;
}

/* a license downloaded into the temp folder (0x810A00C6); Sony leaves the mutex locked on no memory */
int GameLicenseServer::DownloadLicense(const GameLicenseDownloadArgs *args)
{
	if (!m_started)
		return GAME_ERROR_NOT_STARTED;

	m_mutex.Lock();

	GameLicenseDownloadThread *thread = new GameLicenseDownloadThread(this);

	if (thread == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	thread->m_args = args;

	int ret = thread->Start();

	if (ret >= 0) {
		thread->Join();
		ret = thread->m_result;
	}
	delete thread;

	m_mutex.Unlock();
	return ret;
}

/* the downloaded license file (0x810A0154) */
string GameLicenseServer::GetDownloadPath(void)
{
	return m_tempPath;
}

/* the folder it is downloaded into (0x810A016C) */
string GameLicenseServer::GetDownloadDir(void)
{
	return string(GAME_LICENSE_SERVER_TEMP_DIR);
}

/* the files in ux0:temp/license removed (0x810A0198) */
int GameLicenseServer::ClearTempDir(void)
{
	char path[256];
	SceIoDirent dirent;

	sce_paf_snprintf(path, sizeof(path), "%s", GAME_LICENSE_SERVER_TEMP_DIR);

	int ret = sceIoDopen(path);
	SceUID fd = ret;

	if (fd < 0)
		return ret;

	while (sceIoDread(fd, &dirent) != 0) {
		if ((dirent.d_name[0] != '.' || dirent.d_name[1] != '\0') &&
		    (dirent.d_name[0] != '.' || dirent.d_name[1] != '.' || dirent.d_name[2] != '\0')) {
			sce_paf_snprintf(path, sizeof(path), "%s/%s", GAME_LICENSE_SERVER_TEMP_DIR,
					 dirent.d_name);
			ret = sceIoRemove(path);
		}
	}
	sceIoDclose(fd);
	return ret;
}

/*
 * The downloaded license moved to its place: renamed over the old one for a Vita application,
 * copied over it for a PSP one (another drive), then removed (0x810A0250).
 */
int GameLicenseServer::CommitLicense(void)
{
	int ret = 0;

	if (!m_isPsp) {
		ret = sceIoRemove(m_licensePath.c_str());
		return sceIoRename(m_tempPath.c_str(), m_licensePath.c_str());
	}

	common::SharedPtr<File> in;

	in = LocalFile::Open(m_tempPath.c_str(), SCE_O_RDONLY, 0, &ret);
	if (in.get() == NULL)
		return ret;

	int64_t size = in->GetFileSize();

	if (size < 0) {
		in->Close();
		return (int)size;
	}

	void *buf = sce_paf_malloc((size_t)size);

	if (buf == NULL) {
		in->Close();
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	ret = in->Read(buf, (size_t)size);
	in->Close();
	if (ret < 0) {
		sce_paf_free(buf);
		return ret;
	}

	common::SharedPtr<File> out;

	out = LocalFile::Open(m_licensePath.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606,
			      &ret);
	if (out.get() == NULL) {
		sce_paf_free(buf);
		return ret;
	}

	int written = out->Write(buf, (size_t)size);

	if (written >= 0) {
		ret = 0;
	} else {
		ret = written;
		sce_paf_free(buf);
		buf = NULL;
	}
	out->Close();
	if (buf != NULL)
		sce_paf_free(buf);

	ret = sceIoRemove(m_tempPath.c_str());
	return ret;
}

/* the downloaded license written over the temp file (0x810A062A) */
int GameLicenseServer::WriteTempLicense(const void *license, SceSize size)
{
	int ret = 0;
	common::SharedPtr<File> file;

	file = LocalFile::Open(m_tempPath.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606,
			       &ret);
	if (ret == 0 && file.get() != NULL) {
		ret = file->Write(license, size);
		if (ret >= 0)
			ret = 0;
		file->Close();
	}
	return ret;
}

/* ux0:temp/license made, and the temp file "_<rifName>" of size bytes in it (0x810A0768) */
int GameLicenseServer::CreateTempLicense(const char *rifName, SceSize size)
{
	char path[256];

	sce_paf_snprintf(path, sizeof(path), "%s%s", "ux0:", "temp");
	sceIoMkdir(path, 0606);
	sce_paf_snprintf(path, sizeof(path), "%s", GAME_LICENSE_SERVER_TEMP_DIR);

	int ret = sceIoMkdir(path, 0606);
	void *buf = sce_paf_malloc(size);

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	sce_paf_memset(buf, 0, 4);
	sce_paf_snprintf(path, sizeof(path), "%s/_%s", GAME_LICENSE_SERVER_TEMP_DIR, rifName);

	common::SharedPtr<File> file;

	file = LocalFile::Open(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606, &ret);
	if (ret == 0 && file.get() != NULL) {
		ret = file->Write(buf, size);
		if (ret >= 0)
			ret = 0;
		file->Close();
		m_tempPath = path;
	}
	sce_paf_free(buf);
	return ret;
}

/*
 * An application's license fetched again: its file found in ux0:license/app/<title ID> (or the
 * PSP's license folder), the license downloaded and moved there, the temp folders removed
 * (0x810A0958).
 */
int GameLicenseServer::DoUpdateLicense(const GameLicenseUpdateArgs *args)
{
	int ret;
	SceSize size;
	int maxCount = 0;
	int currentCount = 0;
	char path[256];
	char dir[256];
	SceIoStat stat;

	m_isPsp = args->isPsp;
	ClearTempDir();

	void *buf = sce_paf_malloc(GAME_LICENSE_BUF_SIZE);

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	if (m_isPsp) {
		sce_paf_snprintf(path, sizeof(path), "%s", GAME_LICENSE_PSP_DIR);
		ret = sceIoGetstat(path, &stat);
		if (ret >= 0) {
			sce_paf_snprintf(path, sizeof(path), "%s/%s", GAME_LICENSE_PSP_DIR,
					 args->rifName);
			m_licensePath = path;
		}
	} else {
		sce_paf_snprintf(path, sizeof(path), "%s/%s", GAME_LICENSE_APP_DIR, args->titleId);
		ret = sceIoGetstat(path, &stat);
		if (ret >= 0) {
			sce_paf_snprintf(path, sizeof(path), "%s/%s/%s", GAME_LICENSE_APP_DIR,
					 args->titleId, args->rifName);
			m_licensePath = path;
		}
	}

	if (ret >= 0) {
		ret = CreateTempLicense(args->rifName, GAME_LICENSE_BUF_SIZE);
		if (ret == 0) {
			ret = SceNpKdc_78E56B9B(args->contentId, args->epw, buf,
						GAME_LICENSE_BUF_SIZE, &size, &maxCount,
						&currentCount);
			if (ret == 0) {
				ret = WriteTempLicense(buf, size);
				if (ret == 0)
					ret = CommitLicense();
			}
		}
	}

	sce_paf_free(buf);
	ClearTempDir();
	sce_paf_snprintf(dir, sizeof(dir), "%s", GAME_LICENSE_SERVER_TEMP_DIR);
	sceIoRmdir(dir);
	sce_paf_snprintf(dir, sizeof(dir), "%s%s", "ux0:", "temp");
	sceIoRmdir(dir);
	return ret;
}

/* the files in drive's temp/license removed; no such folder is no error (0x810A0B28) */
int GameLicenseServer::ClearDriveTempDir(const char *drive)
{
	char path[256];
	SceIoStat stat;
	SceIoDirent dirent;

	sce_paf_snprintf(path, sizeof(path), "%s%s", drive, GAME_LICENSE_DRIVE_TEMP_DIR);

	int ret = sceIoGetstat(path, &stat);

	if (ret < 0) {
		if (ret == SCE_ERROR_ERRNO_ENOENT)
			return 0;
		return ret;
	}

	SceUID fd = sceIoDopen(path);

	ret = fd;
	if (fd < 0)
		return ret;

	while (sceIoDread(fd, &dirent) != 0) {
		if ((dirent.d_name[0] != '.' || dirent.d_name[1] != '\0') &&
		    (dirent.d_name[0] != '.' || dirent.d_name[1] != '.' || dirent.d_name[2] != '\0')) {
			sce_paf_snprintf(path, sizeof(path), "%s%s/%s", drive,
					 GAME_LICENSE_DRIVE_TEMP_DIR, dirent.d_name);
			ret = sceIoRemove(path);
		}
	}
	sceIoDclose(fd);
	return ret;
}

/* drive's temp/license emptied and removed, then its temp folder (0x810A0C14) */
__attribute__((noinline)) int GameLicenseServer::RemoveDriveTempDir(const char *drive)
{
	char path[256];
	SceIoStat stat;

	ClearDriveTempDir(drive);

	sce_paf_snprintf(path, sizeof(path), "%s%s", drive, GAME_LICENSE_DRIVE_TEMP_DIR);
	if (sceIoGetstat(path, &stat) == 0)
		sceIoRmdir(path);

	sce_paf_snprintf(path, sizeof(path), "%s%s", drive, "temp");

	int ret = sceIoGetstat(path, &stat);

	if (ret == 0)
		return sceIoRmdir(path);
	return ret;
}

/* the downloaded license's period, and whether it has ended by the secure clock (0x810A0CA2) */
__attribute__((noinline)) int GameLicenseServer::GetLicensePeriod(void *license, SceSize size,
								  bool *expired, SceUInt64 *startTime,
								  SceUInt64 *expireTime)
{
	char contentId[48];
	SceRtcTick now;

	*expired = false;
	sce_paf_memset(contentId, 0, sizeof(contentId));
	*startTime = 0;
	*expireTime = 0;
	if (size < GAME_LICENSE_BUF_SIZE)
		sce_paf_memset((char *)license + size, 0, GAME_LICENSE_BUF_SIZE - size);

	int err = sceNpDrmGetRifInfo((const SceNpDrmLicense *)license, size, 1, contentId, NULL,
				     NULL, NULL, NULL, NULL, startTime, expireTime, NULL);
	int ret = 0;

	if (err < 0)
		ret = err;
	if (ret == 0) {
		ret = vshRtcGetCurrentSecureTick(&now);
		if (ret >= 0) {
			if (*expireTime < now.tick)
				*expired = true;
			return 0;
		}
	}
	return ret;
}

/* drive's temp/license made, and the temp file "_<rifName>" of size bytes in it (0x810A0D5A) */
int GameLicenseServer::CreateDriveTempLicense(const char *drive, const char *rifName,
					      SceSize size)
{
	char path[256];

	sce_paf_snprintf(path, sizeof(path), "%s%s", drive, "temp");
	sceIoMkdir(path, 0606);
	sce_paf_snprintf(path, sizeof(path), "%s%s", drive, GAME_LICENSE_DRIVE_TEMP_DIR);

	int ret = sceIoMkdir(path, 0606);
	void *buf = sce_paf_malloc(size);

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	sce_paf_memset(buf, 0, 4);
	sce_paf_snprintf(path, sizeof(path), "%s%s/_%s", drive, GAME_LICENSE_DRIVE_TEMP_DIR,
			 rifName);

	common::SharedPtr<File> file;

	file = LocalFile::Open(path, SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606, &ret);
	if (ret == 0 && file.get() != NULL) {
		ret = file->Write(buf, size);
		if (ret >= 0)
			ret = 0;
		file->Close();
		m_tempPath = path;
	}
	sce_paf_free(buf);
	return ret;
}

/*
 * An additional content's license fetched again: its file found in the drive's
 * license/addcont/<title ID>/<content folder> (or its PSP license folder), the license downloaded,
 * its period given back, and moved there unless it has ended; the drive's temp folders removed
 * (0x810A0F40).
 */
int GameLicenseServer::DoUpdateAddcontLicense(GameAddcontLicenseUpdateArgs *args)
{
	bool expired = false;
	int maxCount = 0;
	int currentCount = 0;
	int ret;
	SceSize size;
	char path[256];
	SceIoStat stat;

	m_isPsp = args->isPsp;
	ClearDriveTempDir(args->drive);

	void *buf = sce_paf_malloc(GAME_LICENSE_BUF_SIZE);

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	if (!m_isPsp) {
		sce_paf_snprintf(path, sizeof(path), "%s%s/%s/%s", args->drive,
				 GAME_LICENSE_DRIVE_ADDCONT_DIR, args->titleId, args->addcontId);
		ret = sceIoGetstat(path, &stat);
		if (ret >= 0) {
			sce_paf_snprintf(path, sizeof(path), "%s%s/%s/%s/%s", args->drive,
					 GAME_LICENSE_DRIVE_ADDCONT_DIR, args->titleId,
					 args->addcontId, args->rifName);
			m_licensePath = path;
		}
	} else {
		sce_paf_snprintf(path, sizeof(path), "%s%s", args->drive,
				 GAME_LICENSE_DRIVE_PSP_DIR);
		ret = sceIoGetstat(path, &stat);
		if (ret >= 0) {
			sce_paf_snprintf(path, sizeof(path), "%s%s/%s", args->drive,
					 GAME_LICENSE_DRIVE_PSP_DIR, args->rifName);
			m_licensePath = path;
		}
	}

	if (ret >= 0) {
		ret = CreateDriveTempLicense(args->drive, args->rifName, GAME_LICENSE_BUF_SIZE);
		if (ret == 0) {
			ret = SceNpKdc_78E56B9B(args->contentId, args->epw, buf,
						GAME_LICENSE_BUF_SIZE, &size, &maxCount,
						&currentCount);
			if (ret == 0) {
				SceUInt64 startTime = 0;
				SceUInt64 expireTime = 0;

				ret = GetLicensePeriod(buf, size, &expired, &startTime,
						       &expireTime);
				if (ret == 0) {
					args->startTime = startTime;
					args->expireTime = expireTime;
					if (expired) {
						ret = GAME_LICENSE_SERVER_ERROR_NO_LICENSE;
					} else {
						ret = WriteTempLicense(buf, size);
						if (ret == 0)
							ret = CommitLicense();
					}
				}
			}
		}
	}

	sce_paf_free(buf);
	RemoveDriveTempDir(args->drive);
	return ret;
}

/* a license downloaded into ux0:temp/license, left there (0x810A112E) */
__attribute__((noinline)) int
GameLicenseServer::DoDownloadLicense(const GameLicenseDownloadArgs *args)
{
	if (args == NULL)
		return GAME_ERROR_INVALID_ARGUMENT;

	int maxCount = 0;
	int currentCount = 0;
	SceSize size = 0;

	ClearTempDir();

	int ret = CreateTempLicense(args->rifName, GAME_LICENSE_BUF_SIZE);

	if (ret == 0) {
		void *buf = sce_paf_malloc(GAME_LICENSE_BUF_SIZE);

		if (buf != NULL) {
			ret = SceNpKdc_78E56B9B(args->contentId, args->epw, buf,
						GAME_LICENSE_BUF_SIZE, &size, &maxCount,
						&currentCount);
			if (ret == 0)
				ret = WriteTempLicense(buf, size);
		} else {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		}
		if (buf != NULL)
			sce_paf_free(buf);
	}
	return ret;
}
