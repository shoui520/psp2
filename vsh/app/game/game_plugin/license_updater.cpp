/*
 * The game plugin's license updater: an application's license and its additional contents'
 * licenses fetched again from the store, a PlayStation Store session opened and its EPW asked for
 * first, the license server doing the rest on threads of its own (0x8106471C..0x810673D8). The
 * file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>
#include <appmgr.h>
#include <npdrm/npdrm.h>
#include <np/np_commerce2.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "license_updater.h"

using namespace paf;

/*
 * The shell's own copy of SceNpCommerce2's library (0x812BD5D4..): the context made for the
 * shell, the session's requests and their end, the EPW request and its text result, as the
 * module's exports of the same code are named.
 */
extern "C" {
int SceNpCommerce2_AA090F5E(SceUInt32 version, SceUInt32 *ctxId);		/* 0x812BD5D4 */
int SceNpCommerce2_C737A929(SceUInt32 ctxId, SceUInt32 *reqId);		/* 0x812BD6CC */
int SceNpCommerce2_2DC2A0F9(SceUInt32 reqId);					/* 0x812BD6F8 */
int SceNpCommerce2_B7387C96(SceUInt32 reqId, void *buf, SceSize bufLen,
			    SceSize *fillSize);					/* 0x812BD6AC */
int SceNpCommerce2_07C16199(SceUInt32 ctxId, SceUInt32 *reqId);		/* 0x812BD8B8 */
int SceNpCommerce2_D35F548B(SceUInt32 reqId);					/* 0x812BD8E4 */
int SceNpCommerce2_5953F48D(GameStoreEpw *result, const void *data, SceSize size); /* 0x812BD906 */
}

/* the requests' result buffer */
#define GAME_STORE_BUF_SIZE		0x10000

/* where an application is a PSP game, and where a PSP game's licenses are */
#define GAME_PSP_GAME_DIR		"ux0:pspemu/PSP/GAME"
#define GAME_PSP_LICENSE_DIR		"ux0:pspemu/PSP/LICENSE/"

/* what the store answers for a license it does not give again (value only) */
#define GAME_STORE_ERROR_REJECTED		-2141879518	/* 0x80558322 */

/* an additional content's license fetched again a day past its time at the latest */
#define GAME_ADDCONT_LICENSE_GRACE_HOURS	24

/* where the title ID and the additional content's ID are in a content ID */
#define GAME_CONTENT_ID_TITLE_ID_POS	7
#define GAME_CONTENT_ID_TITLE_ID_LEN	9
#define GAME_CONTENT_ID_LABEL_POS	20
#define GAME_CONTENT_ID_LABEL_LEN	16

static GameLicenseServer *s_server;

GameLicenseUpdater::GameLicenseUpdater()
	: m_ctxId(0), m_sessionReqId(0), m_epwReqId(0), m_buf(NULL)
{
	s_server = new GameLicenseServer;
}

GameLicenseUpdater::~GameLicenseUpdater()
{
	if (s_server != NULL)
		delete s_server;
}

/* The PSP game license rifName read, and its content ID taken (0x8106476E). */
int GameLicenseUpdater::ReadPspLicense(const char *rifName, char *contentId)
{
	int ret = 0;
	string path = string(GAME_PSP_LICENSE_DIR) + string(rifName);
	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret < 0)
		return ret;

	int size = (int)file->GetFileSize();

	if (size < 0) {
		file->Close();
		return size;
	}

	SceNpDrmLicense *license = new SceNpDrmLicense;

	if (license == NULL) {
		file->Close();
		return GAME_ERROR_NO_INFO_MEMORY;
	}

	if ((unsigned int)size > sizeof(SceNpDrmLicense)) {
		file->Close();
		delete license;
		return GAME_ERROR_BAD_LICENSE_SIZE;
	}

	ret = file->Read(license, size);
	if (ret < 0) {
		file->Close();
		delete license;
		return ret;
	}

	file->Close();

	if ((unsigned int)size < 4)
		sce_paf_memset((char *)license + size, 0, 4 - size);

	ret = sceNpDrmGetRifInfo(license, size, 1, contentId, NULL, NULL, NULL, NULL, NULL, NULL,
				 NULL, NULL);
	delete license;

	return 0;
}

/* The license file name of the PSP game at path: its content ID, from its EBOOT, and ".rif" (0x81064BCA). */
int GameLicenseUpdater::GetPspRifName(const char *path, char *rifName)
{
	int ret = 0;
	char buf[GAME_PSP_CONTENT_ID_SIZE];

	sce_paf_memset(buf, 0, sizeof(buf));
	sce_paf_snprintf(buf, sizeof(buf), "%s/EBOOT.PBP", path);

	GamePbpHeader header;

	sce_paf_memset(&header, 0, sizeof(header));
	ret = gamePbpReadHeader(buf, &header, NULL);
	if (ret < 0)
		return ret;

	common::SharedPtr<File> file = LocalFile::Open(buf, SCE_O_RDONLY, 0, &ret);

	if (ret != 0)
		return ret;

	if (file->Seek(header.dataPspOffset + GAME_PSP_CONTENT_ID_OFFSET, SCE_SEEK_SET) < 0) {
		file->Close();
		return ret;
	}

	sce_paf_memset(buf, 0, sizeof(buf));
	ret = file->Read(buf, sizeof(buf));
	if (ret < 0) {
		file->Close();
		return ret;
	}

	file->Close();

	if (sce_paf_snprintf(rifName, GAME_PSP_CONTENT_ID_SIZE, "%s.rif", buf) >
	    GAME_PSP_CONTENT_ID_SIZE)
		return GAME_ERROR_NO_DRIVE;

	return 0;
}

/* The store opened: its buffer, the shell's context and a session (0x81064E5E). */
__attribute__((noinline)) int GameLicenseUpdater::OpenStore(void)
{
	SceSize size = 0;

	m_buf = sce_paf_malloc(GAME_STORE_BUF_SIZE);
	if (m_buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ret = SceNpCommerce2_AA090F5E(SCE_NP_COMMERCE2_VERSION, &m_ctxId);

	if (ret >= 0) {
		sceNpCommerce2CreateSessionCreateReq(m_ctxId, &m_sessionReqId);
		ret = sceNpCommerce2CreateSessionStart(m_sessionReqId);
		if (ret == 0)
			SceNpCommerce2_B7387C96(m_sessionReqId, m_buf, GAME_STORE_BUF_SIZE, &size);
		sceNpCommerce2DestroyReq(m_sessionReqId);
		m_sessionReqId = 0;

		return ret;
	}

	if (m_buf != NULL) {
		sce_paf_free(m_buf);
		m_buf = NULL;
	}

	return ret;
}

/* The session's EPW asked for, into m_epw. */
inline int GameLicenseUpdater::RequestEpw(void)
{
	SceSize size = 0;

	SceNpCommerce2_07C16199(m_ctxId, &m_epwReqId);

	int ret = SceNpCommerce2_D35F548B(m_epwReqId);

	if (ret == 0)
		SceNpCommerce2_B7387C96(m_epwReqId, m_buf, GAME_STORE_BUF_SIZE, &size);
	sceNpCommerce2DestroyReq(m_epwReqId);
	m_epwReqId = 0;

	if (ret >= 0)
		ret = SceNpCommerce2_5953F48D(&m_epw, m_buf, size);

	return ret;
}

/* The session ended, the context and the buffer given back. */
inline void GameLicenseUpdater::CloseStore(void)
{
	SceSize size = 0;

	SceNpCommerce2_C737A929(m_ctxId, &m_sessionReqId);
	if (SceNpCommerce2_2DC2A0F9(m_sessionReqId) == 0)
		SceNpCommerce2_B7387C96(m_sessionReqId, m_buf, GAME_STORE_BUF_SIZE, &size);
	sceNpCommerce2DestroyReq(m_sessionReqId);
	m_sessionReqId = 0;

	if (m_ctxId != 0) {
		sceNpCommerce2DestroyCtx(m_ctxId);
		m_ctxId = 0;
	}

	if (m_buf != NULL) {
		sce_paf_free(m_buf);
		m_buf = NULL;
	}
}

int GameLicenseUpdater::Update(const char *path, const char *titleId)
{
	string workDir;

	if (s_server == NULL)
		return GAME_ERROR_NOT_STARTED;

	SceBool isPsp = sce_paf_strncmp(GAME_PSP_GAME_DIR, path, sizeof(GAME_PSP_GAME_DIR) - 1) == 0;
	int ret = OpenStore();
	bool mounted = false;

	if (ret == 0) {
		ret = RequestEpw();

		if (ret == 0) {
			if (!isPsp) {
				ret = sceAppMgrCheckRifGD(path, &m_license);
				if (ret == GAME_LICENSE_ERROR_EXPIRED || ret >= 0)
					ret = sceNpDrmGetRifName(m_rifName, m_license.accountId);
			} else {
				ret = gamePspemuWorkMount(&workDir);
				if (ret >= 0) {
					mounted = true;
					ret = GetPspRifName(path, m_rifName);
					if (ret >= 0)
						ret = ReadPspLicense(m_rifName, m_license.contentId);
				}
			}

			if (ret >= 0) {
				ret = s_server->Init();
				if (ret == 0) {
					GameLicenseUpdateArgs args = {
						m_license.contentId, m_epw.epw, titleId, m_rifName, isPsp
					};

					ret = s_server->UpdateLicense(&args);
					s_server->Term();
				}
			}
		}
	}

	CloseStore();

	if (mounted)
		gamePspemuWorkUnmount(&workDir);

	return ret;
}

/*
 * Whether the additional content contentId of the application on drive is still there: its
 * PARAM.PBP for a PSP game, its param.sfo otherwise (0x810650FA).
 */
bool GameLicenseUpdater::AddcontParamExists(SceBool isPsp, const string &drive,
					   const string &contentId)
{
	string titleId = contentId.substr(GAME_CONTENT_ID_TITLE_ID_POS, GAME_CONTENT_ID_TITLE_ID_LEN);
	string path("");

	if (!isPsp) {
		string label = contentId.substr(GAME_CONTENT_ID_LABEL_POS, GAME_CONTENT_ID_LABEL_LEN);

		path = drive + string("addcont/") + titleId + string("/") + label + string("/") +
		       string("sce_sys/param.sfo");
	} else {
		path = drive + string("pspemu/PSP/GAME/") + titleId + string("/PARAM.PBP");
	}

	return LocalFile::Exists(path.c_str());
}

/*
 * The folders of titleId's additional contents, into dirs: the eight its record names, or the
 * PSP game's own (0x81065948).
 */
int GameLicenseUpdater::GetAddcontDirs(SceBool isPsp, const char *titleId, vector<string> *dirs)
{
	dirs->clear();

	if (!isPsp) {
		LsdbAppInfoPtr info(new LsdbAppInfo);
		int ret = gameAppGetInfo(titleId, &info);

		if (ret < 0)
			return ret;

		unsigned int keys[] = {
			LSDB_KEY_INSTALL_DIR_ADDCONT,
			LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_1, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_2,
			LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_3, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_4,
			LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_5, LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_6,
			LSDB_KEY_INSTALL_DIR_ADDCONT_ADD_7,
		};

		for (int i = 0; i < 8; i++) {
			string dir(LsdbValueText((*info)[keys[i]]));

			if (dir.length() == 0)
				continue;

			dirs->push_back(dir);
		}

		return 0;
	}

	dirs->push_back(string(titleId));

	return 0;
}

/* where the application titleId was installed from, "" without a record (0x81065CBA) */
string GameLicenseUpdater::GetAppPath(const string &titleId)
{
	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (gameAppGetInfo(titleId.c_str(), &info) < 0)
		return string("");

	LsdbAppInfo::iterator it = info->find(LSDB_KEY_ORG_PATH);

	if (it == info->end())
		return string("");

	return string(LsdbValueText(it->second));
}

/* the drive the application titleId is installed on (0x81065F1E) */
int GameLicenseUpdater::GetDrive(const char *titleId, string *drive)
{
	*drive = "";

	int storage = 0;
	int ret = gameTitleGetStorage(titleId, &storage);

	if (ret < 0)
		return ret;

	switch (storage) {
	case GAME_STORAGE_GRO0:
		*drive = "grw0:";
		return 0;
	case GAME_STORAGE_GRO0_OTHER:
	case GAME_STORAGE_UX0:
		*drive = "ux0:";
		return 0;
	}

	return GAME_ERROR_BAD_STORAGE;
}

int GameLicenseUpdater::UpdateAddcont(const char *titleId)
{
	if (s_server == NULL)
		return GAME_ERROR_NOT_STARTED;

	string drive("");
	int ret = GetDrive(titleId, &drive);

	if (ret < 0)
		return ret;

	string infoPath = drive + string("addcont/info");
	string appPath = GetAppPath(string(titleId));

	if (appPath.length() == 0)
		return GAME_ERROR_NO_VALUE;

	SceBool isPsp = sce_paf_strncmp(GAME_PSP_GAME_DIR, appPath.c_str(),
				     sizeof(GAME_PSP_GAME_DIR) - 1) == 0;
	vector<string> dirs;

	ret = GetAddcontDirs(isPsp, titleId, &dirs);
	if (ret < 0)
		return ret;

	if (dirs.size() == 0)
		return 0;

	vector<GameAddcontLicenseInfo> infos;

	{
		string path(infoPath.c_str());

		if (isPsp)
			ret = gamePspAddcontInfoRead(path.c_str(), &dirs, &infos);
		else
			ret = gameAddcontInfoRead(path.c_str(), &dirs, &infos);
	}

	if (ret < 0)
		return ret;

	if (infos.size() == 0)
		return 0;

	SceRtcTick now;

	ret = vshRtcGetCurrentSecureTick(&now);
	if (ret < 0)
		return ret;

	ret = OpenStore();
	if (ret < 0)
		return ret;

	ret = RequestEpw();

	if (ret < 0) {
		CloseStore();
		return ret;
	}

	string workDir;
	bool mounted = false;
	bool failed = false;

	for (unsigned int i = 0; i < infos.size(); i++) {
		workDir = "";
		mounted = false;

		GameAddcontLicenseInfo *info = &infos[i];

		if (info->expireTime > now.tick)
			continue;

		SceRtcTick limit;

		ret = sceRtcTickAddHours(&limit, (const SceRtcTick *)&info->expireTime,
					 GAME_ADDCONT_LICENSE_GRACE_HOURS);
		if (ret < 0) {
			failed = true;
			break;
		}

		bool overdue = limit.tick < now.tick;
		string contentId(info->contentId);

		if (!AddcontParamExists(isPsp, drive, contentId)) {
			if (isPsp)
				ret = gamePspAddcontInfoRemove(infoPath.c_str(), contentId.c_str());
			else
				ret = gameAddcontInfoRemove(infoPath.c_str(), contentId.c_str());
			if (ret < 0) {
				failed = true;
				break;
			}
			continue;
		}

		string rifName("");
		string addcontTitleId =
			contentId.substr(GAME_CONTENT_ID_TITLE_ID_POS, GAME_CONTENT_ID_TITLE_ID_LEN);
		string label = contentId.substr(GAME_CONTENT_ID_LABEL_POS, GAME_CONTENT_ID_LABEL_LEN);

		if (!isPsp) {
			SceUInt64 accountId = 0;

			ret = gameNpGetAccountId(&accountId);
			if (ret < 0) {
				failed = true;
				break;
			}

			char name[GAME_PSP_CONTENT_ID_SIZE];

			sce_paf_memset(name, 0, sizeof(name));
			ret = sceNpDrmGetRifName(name, accountId);
			if (ret < 0) {
				failed = true;
				break;
			}

			rifName = name;
		} else {
			ret = gamePspemuWorkMount(&workDir);
			if (ret < 0) {
				failed = true;
				break;
			}

			mounted = true;
			rifName = contentId + string(".rif");
		}

		ret = s_server->Init();
		if (ret != 0) {
			failed = true;
			break;
		}

		GameAddcontLicenseUpdateArgs args = {
			contentId.c_str(), m_epw.epw, addcontTitleId.c_str(), rifName.c_str(), isPsp,
			drive.c_str(), label.c_str(), 0, 0
		};

		ret = s_server->UpdateAddcontLicense(&args);
		if (ret != 0) {
			if (ret == GAME_LICENSE_SERVER_ERROR_NO_LICENSE) {
				if (isPsp)
					ret = gamePspAddcontInfoRemove(infoPath.c_str(),
								       contentId.c_str());
				else
					ret = gameAddcontInfoRemove(infoPath.c_str(),
								    contentId.c_str());
				if (ret < 0) {
					s_server->Term();
					failed = true;
					break;
				}
			} else if (ret == GAME_STORE_ERROR_REJECTED) {
				if (overdue) {
					if (isPsp)
						ret = gamePspAddcontInfoRemove(infoPath.c_str(),
									       contentId.c_str());
					else
						ret = gameAddcontInfoRemove(infoPath.c_str(),
									    contentId.c_str());
					if (ret < 0) {
						s_server->Term();
						failed = true;
						break;
					}
				}
			} else {
				s_server->Term();
				failed = true;
				break;
			}
		} else if (args.expireTime == GAME_LICENSE_NO_EXPIRY) {
			if (isPsp)
				ret = gamePspAddcontInfoRemove(infoPath.c_str(), contentId.c_str());
			else
				ret = gameAddcontInfoRemove(infoPath.c_str(), contentId.c_str());
			if (ret < 0) {
				s_server->Term();
				failed = true;
				break;
			}
		} else {
			GameAddcontLicenseInfo updated;

			sce_paf_memset(&updated, 0, sizeof(updated));
			sce_paf_memcpy(updated.contentId, contentId.c_str(), sizeof(updated.contentId));
			updated.startTime = args.startTime;
			updated.expireTime = args.expireTime;

			if (isPsp)
				ret = gamePspAddcontInfoWrite(infoPath.c_str(), &updated);
			else
				ret = gameAddcontInfoWrite(infoPath.c_str(), &updated);
			if (ret < 0) {
				s_server->Term();
				failed = true;
				break;
			}
		}

		s_server->Term();

		if (mounted)
			gamePspemuWorkUnmount(&workDir);
	}

	CloseStore();

	if (!failed)
		return 0;

	if (mounted)
		gamePspemuWorkUnmount(&workDir);

	return ret;
}

/* The free trial license of titleId marked activated (0x81066EF4). */
int GameLicenseUpdater::ActivateFtt(const string &titleId)
{
	string fttName;
	int ret = gameNpDrmGetFttName(&fttName);

	if (ret < 0)
		return ret;

	string path = string("ux0:license/app/") + string(titleId);

	path += "/";
	path.append(fttName);

	ret = gameLicenseActivate(&path);
	if (ret < 0)
		return ret;

	return 0;
}

int GameLicenseUpdater::Prepare(const string &titleId, string *contentId, string *dir,
				string *path)
{
	dir->clear();
	path->clear();

	string rifName;

	if (s_server == NULL)
		return GAME_ERROR_NOT_STARTED;

	int ret = OpenStore();

	if (ret >= 0) {
		ret = RequestEpw();

		if (ret >= 0) {
			ret = gameNpDrmGetRifName(&rifName);
			if (ret >= 0) {
				ret = s_server->Init();
				if (ret >= 0) {
					ret = ActivateFtt(titleId);
					if (ret >= 0) {
						GameLicenseDownloadArgs args = {
							contentId->c_str(), m_epw.epw, rifName.c_str()
						};

						ret = s_server->DownloadLicense(&args);
						if (ret >= 0) {
							*dir = s_server->GetDownloadDir();
							*path = s_server->GetDownloadPath();
						}
					}
					s_server->Term();
				}
			}
		}

		CloseStore();
	}

	return ret;
}
