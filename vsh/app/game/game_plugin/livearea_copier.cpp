/*
 * The game plugin's appmeta of an application: its icons, its LiveArea and its parameters copied
 * from its folder into ux0:appmeta or ur0:appmeta when it is registered (0x8104DAC4..0x81052054).
 * The file's name and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <kernel/iofilemgr.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <std/stdio.h>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <dir/dir.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the share of each step in a copier's progress: its checks, its copy and its commit */
#define GAME_LIVEAREA_WEIGHT_CHECK	0.1f
#define GAME_LIVEAREA_WEIGHT_COPY	0.8f
#define GAME_LIVEAREA_WEIGHT_COMMIT	0.1f

/* the room an application's appmeta needs on ux0: */
#define GAME_APPMETA_SIZE		0x500000

/* the largest card param.sfo copied */
#define GAME_GC_PARAM_MAX_SIZE		0x8000

GameLiveAreaCopier::GameLiveAreaCopier(bool flag, const string &name, GameCancelFunc cancel,
				       void *arg, const GameJobProgress *progress, string *detail,
				       float scale)
	: m_flag(flag), m_name(name), m_cancel(cancel), m_arg(arg), m_progress(*progress, scale),
	  m_detail(detail)
{
	m_weights[0] = GAME_LIVEAREA_WEIGHT_CHECK;
	m_weights[1] = GAME_LIVEAREA_WEIGHT_COPY;
	m_weights[2] = GAME_LIVEAREA_WEIGHT_COMMIT;
}

GameLiveAreaCopier::~GameLiveAreaCopier()
{
}

/*
 * The icon, the background and the LiveArea of the application in src copied into the appmeta
 * dst: a retail copy takes its retail and livearea folders whole, any other the livearea its
 * kind (the application's #_up_status) names (0x8104DBC6).
 */
static int gameLiveAreaCopyFiles(const string &src, const string &dst, unsigned int kind,
				 bool retail, GameJobProgress *progress, string *detail,
				 float scale)
{
	string from = src + string("/sce_sys/icon0.png");
	string to = dst + string("/icon0.png");
	int ret = gameImageConvert(&from, &to, gameIcon0Format(), detail);

	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
		return ret;

	from = src + string("/sce_sys/pic0.png");
	to = dst + string("/pic0.png");

	ret = gameImageConvert(&from, &to, gamePic0Format(), detail);
	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
		return ret;

	progress->Add(scale * 0.2f * 100.0f);

	string liveAreaFrom;
	string liveAreaTo;

	if (retail) {
		liveAreaFrom = src + string("/sce_sys/retail");
		liveAreaTo = dst + string("/retail");

		GameJobProgress *part = new GameJobProgress(*progress, scale * 0.4f);

		if (part == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		ret = gameCopyFileWithPool(liveAreaFrom.c_str(), liveAreaTo.c_str(), 0, 0, (int)part,
					   (int)detail);
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
			delete part;
			return ret;
		}

		delete part;

		liveAreaFrom = src + string("/sce_sys/livearea");
		liveAreaTo = dst + string("/livearea");

		part = new GameJobProgress(*progress, scale * 0.4f);
		if (part == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		ret = gameCopyFileWithPool(liveAreaFrom.c_str(), liveAreaTo.c_str(), 0, 0, (int)part,
					   (int)detail);
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
			delete part;
			return ret;
		}

		delete part;
	} else {
		liveAreaTo = dst + string("/livearea");

		if (kind == 0 || kind == 1) {
			liveAreaFrom = src + string("/sce_sys/livearea");
		} else if (kind == 3) {
			liveAreaFrom = src + string("/sce_sys/retail/livearea");
			if (!Dir::IsExist(liveAreaFrom.c_str()))
				liveAreaFrom = src + string("/sce_sys/livearea");
		}

		GameJobProgress *part = new GameJobProgress(*progress, scale * 0.8f);

		if (part == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		ret = gameCopyFileWithPool(liveAreaFrom.c_str(), liveAreaTo.c_str(), 0, 0, (int)part,
					   (int)detail);
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
			delete part;
			return ret;
		}

		delete part;
	}

	return 0;
}

/* the card's param.sfo copied into dir as gc_param.sfo (0x8104EC2A) */
static int gameGcParamCopy(const string &dir, string *detail)
{
	int ret;

	if (!Dir::IsExist(dir.c_str())) {
		ret = Dir::CreateRecursive(dir.c_str(), 6);
		if (ret < 0) {
			gamePathGetMountedDrive(dir, detail);
			return ret;
		}
	}

	string src("gro0:gc/param.sfo");
	string dst = dir + string("/gc_param.sfo");
	LocalFileStat stat;

	ret = LocalFile::Getstat(src.c_str(), &stat);
	if (ret < 0) {
		gamePathGetMountedDrive(src, detail);
		return ret;
	}

	if (stat.st_size > GAME_GC_PARAM_MAX_SIZE)
		return GAME_ERROR_BAD_PARAM_SIZE;

	ret = gameCopyFileWithPool(src.c_str(), dst.c_str(), 0, 0, 0, (int)detail);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The appmeta of the application in dir made in the application data mounted as "new": its
 * parameters (and the card's param.sfo when gcParam), its images and its EBOOTs' parameters
 * (0x8104EEEE).
 */
int GameLiveAreaCopier::Write(const string *dir, unsigned int kind, bool gcParam,
			      GameJobProgress *progress, float scale)
{
	string mountPoint;
	int ret = gameAppDataMount(string("new"), &mountPoint, false);

	if (ret < 0)
		return ret;

	if (!gcParam || (ret = gameGcParamCopy(mountPoint, m_detail)) >= 0) {
		ret = gameAppmetaCopyParams(*dir, mountPoint, m_detail);
		if (ret >= 0) {
			float step = scale * 0.1f * 100.0f;

			progress->Add(step);

			ret = gameLiveAreaCopyFiles(*dir, mountPoint, kind, true, progress, m_detail,
						    scale * 0.8f);
			if (ret >= 0) {
				LocalFileStat stat;
				string ebootPath = *dir + string("/eboot.bin");
				string paramPath = mountPoint + string("/ebootparam.bin");

				ret = gameAppmetaWriteEbootParam(&ebootPath, &paramPath, m_detail);
				if (ret >= 0) {
					for (unsigned int i = 0; i < 1; i++) {
						char name[32];

						sce_paf_snprintf(name, sizeof(name), "/eboot%u.bin", i + 2);
						ebootPath = *dir + string(name);
						if (LocalFile::Getstat(ebootPath.c_str(), &stat) == 0) {
							paramPath = mountPoint +
								    string("/ebootparam2.bin");
							ret = gameAppmetaWriteEbootParam(&ebootPath,
											 &paramPath,
											 m_detail);
							if (ret < 0)
								goto out;
						}

						progress->Add(step * i);
					}

					ret = 0;
				}
			}
		}
	}

out:
	if (mountPoint.length() != 0)
		gameAppDataUnmount(&mountPoint);

	return ret;
}

/*
 * The appmeta of the application in dir made and moved to ux0:appmeta/titleId when onUx0, else
 * only checked for room (0x8104F454).
 */
int GameLiveAreaCopier::CopyToNew(const string *dir, const string *titleId, unsigned int kind,
				  bool gcParam, bool onUx0)
{
	int ret = gameAppmetaCheckSpace(onUx0, GAME_APPMETA_SIZE, m_detail);

	if (ret < 0)
		return ret;

	gameRemoveDirectory("ux0:appmeta/new", 0, 0, 0, true);

	if (onUx0) {
		string tempDir("ux0:temp/appmeta");
		string appmetaDir = string("ux0:temp/appmeta/") + *titleId;

		ret = Write(dir, kind, gcParam, &m_progress, m_weights[1] * 0.5f);
		if (ret < 0)
			return ret;

		if (m_cancel != NULL && m_cancel(m_arg))
			return GAME_ERROR_ABORTED;

		ret = gameAppmetaCommitNew(titleId);
		if (ret < 0)
			return ret;

		m_progress.Add(m_weights[2] * 100.0f);
	} else {
		m_progress.Add(m_weights[2] * 100.0f);
	}

	return 0;
}

/* the appmeta of the application in dir made in ux0:appmeta/titleId (0x8104F72E) */
int GameLiveAreaCopier::CopyToUx0(const string *dir, const string *titleId, unsigned int kind,
				  bool gcParam)
{
	SceUInt64 total = 0;
	SceUInt64 free = 0;
	int ret = gameDeviceGetSpace("ux0:", &total, &free);

	if (ret < 0)
		return ret;

	if (free < GAME_APPMETA_SIZE)
		return GAME_ERROR_NO_APPMETA_SPACE;

	gameRemoveDirectory("ux0:appmeta/new", 0, 0, 0, true);
	gameRemoveDirectory("ux0:temp/appmeta", 0, 0, 0, true);

	ret = Write(dir, kind, gcParam, &m_progress, m_weights[1]);
	if (ret < 0)
		return ret;

	ret = gameAppmetaCommitNew(titleId);
	if (ret < 0)
		return ret;

	m_progress.Add(m_weights[2] * 100.0f);

	return 0;
}

/*
 * The appmeta of info's application, from its folder dir, made where the application is
 * installed (0x8104F830).
 */
int GameLiveAreaCopier::Copy(const LsdbAppInfoPtr *info, const string *dir)
{
	LsdbAppInfo::iterator end = (*info)->end();
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	int storage = 0;
	int ret = gameAppGetStorage(info, &storage);

	if (ret < 0)
		return ret;

	unsigned int kind = 0;

	it = (*info)->find(LSDB_KEY_UP_STATUS);
	if (it != end)
		kind = (unsigned int)gameLsdbValueInteger(it->second);

	bool hasRoom = false;

	ret = gameLsdbHasRoom(&hasRoom);
	if (ret < 0)
		return ret;

	if (!hasRoom) {
		m_progress.Add(m_weights[0] * 100.0f);

		switch (storage) {
		case GAME_STORAGE_GRO0:
		case GAME_STORAGE_GRO0_OTHER:
			return GAME_ERROR_TOO_MANY_ICONS;
		case GAME_STORAGE_UX0:
			ret = CopyToUx0(dir, &titleId, kind, storage == GAME_STORAGE_GRO0_OTHER);
			if (ret < 0)
				return ret;
			return 0;
		default:
			return GAME_ERROR_NO_STORAGE;
		}
	}

	gameRemoveDirectoryContents("ur0:temp/appmeta", 0, 0, 0, true);
	gameRemoveDirectory("ux0:temp/appmeta", 0, 0, 0, true);
	Dir::CreateRecursive("ur0:appmeta", 6);
	m_progress.Add(m_weights[0] * 100.0f);

	switch (storage) {
	case GAME_STORAGE_GRO0:
		ret = CopyToNew(dir, &titleId, kind, true, false);
		break;
	case GAME_STORAGE_GRO0_OTHER: {
		LocalFileStat stat;
		bool onUx0;

		ret = LocalFile::Getstat("ux0:", &stat);
		if (ret == 0) {
			Dir::CreateRecursive("ux0:appmeta", 6);
			onUx0 = true;
		} else if (ret == GAME_PAF_ERROR_NO_DEVICE) {
			onUx0 = false;
		} else {
			return ret;
		}

		ret = CopyToNew(dir, &titleId, kind, true, onUx0);
		break;
	}
	case GAME_STORAGE_UX0:
		Dir::CreateRecursive("ux0:appmeta", 6);
		ret = CopyToNew(dir, &titleId, kind, false, true);
		break;
	case GAME_STORAGE_PD0:
		Dir::CreateRecursive("ux0:appmeta", 6);
		ret = CopyToNew(dir, &titleId, kind, false, false);
		break;
	default:
		return GAME_ERROR_NO_STORAGE;
	}

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The icon kept in the application's free-trial license file (m_name) written to mountPoint,
 * once the license is checked to be info's content's (0x8104FC56).
 */
int GameLiveAreaCopier::CopyFttIcon(const string *mountPoint, const LsdbAppInfoPtr *info)
{
	int ret = 0;
	SceSize size = 0;
	char *buf = NULL;
	int n;
	string contentId;
	string fileContentId;
	common::SharedPtr<File> src;
	common::SharedPtr<File> dst;
	string iconPath = *mountPoint + string("/icon0.png");
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_CONTENT_ID);

	if (it == (*info)->end()) {
		ret = GAME_ERROR_NO_VALUE;
		goto end;
	}

	contentId = LsdbValueText(it->second);

	src = LocalFile::Open(m_name.c_str(), SCE_O_RDONLY, 0, &ret);
	if (ret < 0)
		goto end;

	{
		GameFttLicense license;

		sce_paf_memset(&license, 0, sizeof(license));
		ret = gameFttLicenseRead(src, &license);
		if (ret < 0)
			goto end;

		fileContentId = license.contentId;
	}

	if (contentId != fileContentId) {
		ret = GAME_ERROR_FTT_MISMATCH;
		goto end;
	}

	ret = gameFttLicenseGetDataSize(src, &size);
	if (ret < 0 || size == 0)
		goto end;

	buf = new char[size];
	if (buf == NULL) {
		ret = GAME_ERROR_NO_INFO_MEMORY;
		goto end;
	}

	n = src->Read(buf, size);
	if (n < 0) {
		ret = n;
		goto end;
	}
	if ((SceSize)n != size) {
		ret = GAME_ERROR_SHORT_IO;
		goto end;
	}

	src->Close();

	dst = LocalFile::Open(iconPath.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);
	if (ret < 0)
		goto end;

	n = dst->Write(buf, size);
	if (n < 0) {
		ret = n;
		goto end;
	}
	if ((SceSize)n != size) {
		ret = GAME_ERROR_SHORT_IO;
		goto end;
	}

	dst->Close();
	delete[] buf;
	buf = NULL;

end:
	if (buf != NULL)
		delete[] buf;
	if (dst.get() != NULL && dst->IsOpened())
		dst->Close();
	if (src.get() != NULL && src->IsOpened())
		src->Close();

	return ret;
}

/* the free-trial application's appmeta made in the application data mounted as "new" (0x8105020C) */
int GameLiveAreaCopier::WriteFtt(const LsdbAppInfoPtr *info, const string *dir)
{
	string mountPoint;
	int ret = gameAppDataMount(string("new"), &mountPoint, false);

	if (ret >= 0) {
		ret = CopyFttIcon(&mountPoint, info);
		if (ret >= 0)
			ret = gameAppmetaCopyParams(*dir, mountPoint, m_detail);
	}

	if (mountPoint.length() != 0)
		gameAppDataUnmount(&mountPoint);

	return ret;
}

/* the free-trial application's appmeta made in ux0:appmeta (0x810502CC) */
int GameLiveAreaCopier::CopyFtt(const LsdbAppInfoPtr *info, const string *dir)
{
	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == (*info)->end())
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	int ret = gameAppmetaCheckSpace(true, GAME_APPMETA_SIZE, m_detail);

	if (ret < 0)
		return ret;

	m_progress.Add(m_weights[0] * 100.0f);

	ret = WriteFtt(info, dir);
	if (ret < 0)
		return ret;

	m_progress.Add(m_weights[1] * 100.0f);

	ret = gameAppmetaCommitNew(&titleId);
	if (ret < 0)
		return ret;

	m_progress.Add(m_weights[2] * 100.0f);

	return ret;
}

/* the appmeta of reg's application, a game's, made from its folder dir (0x810504B0) */
int GameLiveAreaCopier::Run(GameAppRegistration *reg, const string *dir)
{
	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_CATEGORY);

	if (it == reg->info->end())
		return GAME_ERROR_NO_VALUE;

	string category(LsdbValueText(it->second));

	if (!gameDriveIsNotData(category))
		return 0;

	if (category == "gd" || category == "gp" || category == "gdc" || category == "gpc" ||
	    category == "gdb") {
		if (m_flag) {
			int ret = CopyFtt(&reg->info, dir);

			if (ret < 0)
				return ret;
		} else {
			int ret = Copy(&reg->info, dir);

			if (ret < 0)
				return ret;
		}
	}

	return 0;
}

/*
 * The LiveArea folder src copied over dst through the drive's temp/appmeta: dst, when there is
 * one, is kept as temp/appmeta/old (0x8105068C).
 */
static int gameLiveAreaReplace(const string *titleId, const string *src, const string *dst,
			       string *detail)
{
	string drive;
	int ret = gamePathGetDrive(*dst, &drive);

	if (ret < 0)
		return ret;

	string newDir = drive + string("temp/appmeta/new");
	string oldDir = drive + string("temp/appmeta/old");

	Dir::CreateRecursive("ur0:temp/appmeta", 6);

	ret = gameCopyFileWithPool(src->c_str(), newDir.c_str(), 0, 0, 0, (int)detail);
	if (ret < 0)
		return ret;

	LocalFileStat stat;

	if (LocalFile::Getstat(dst->c_str(), &stat) == 0) {
		ret = LocalFile::RenameFile(dst->c_str(), oldDir.c_str());
		if (ret < 0) {
			gamePathGetMountedDrive(oldDir, detail);
			return ret;
		}
	}

	ret = LocalFile::RenameFile(newDir.c_str(), dst->c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(*dst, detail);
		return ret;
	}

	return 0;
}

/*
 * An application's record prepared for its registration: keepLiveArea is set when its
 * application data holds a retail LiveArea, which is then brought into ur0:appmeta
 * (0x81050A8A).
 */
int gameAppPrepareRegister(LsdbAppInfoPtr *info, bool *keepLiveArea, string *detail)
{
	LsdbAppInfo::iterator end = (*info)->end();

	*keepLiveArea = false;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	int storage = 0;
	int ret = gameAppGetStorage(info, &storage);

	if (ret < 0)
		return ret;

	unsigned int kind = 0;

	it = (*info)->find(LSDB_KEY_UP_STATUS);
	if (it != end)
		kind = (unsigned int)gameLsdbValueInteger(it->second);

	bool hasRoom = false;
	bool replace = true;

	ret = gameLsdbHasRoom(&hasRoom);
	if (ret < 0)
		return ret;

	if (!hasRoom) {
		switch (storage) {
		case GAME_STORAGE_GRO0:
		case GAME_STORAGE_GRO0_OTHER:
			return GAME_ERROR_TOO_MANY_ICONS;
		case GAME_STORAGE_UX0:
			replace = false;
			break;
		default:
			return GAME_ERROR_NO_STORAGE;
		}
	}

	string mountPoint;
	string src = kind == 3 ?
		string("ux0:appmeta/") + titleId + string("/retail/livearea") :
		string("ux0:appmeta/") + titleId + string("/livearea");
	string dst = string("ur0:appmeta/") + titleId + string("/livearea");
	string retail = string("ux0:appmeta/") + titleId + string("/retail/livearea");
	LocalFileStat stat;

	ret = gameAppDataMount(string(titleId.c_str()), &mountPoint, true);
	if (ret < 0)
		return ret;

	ret = LocalFile::Getstat(retail.c_str(), &stat);
	if (ret >= 0) {
		*keepLiveArea = true;
		if (!replace || (ret = gameLiveAreaReplace(&titleId, &src, &dst, detail)) >= 0)
			ret = 0;
	} else if (ret == GAME_PAF_ERROR_NOT_FOUND) {
		ret = 0;
	} else {
		gamePathGetMountedDrive(retail, detail);
	}

	gameAppDataUnmount(&mountPoint);

	return ret;
}

/*
 * Whether info's application, on a game card, has a retail LiveArea (hasAppmeta) (0x8105159A).
 */
int gameAppInfoCheckAppmeta(LsdbAppInfoPtr *info, bool *hasAppmeta, string *detail)
{
	LsdbAppInfo::iterator end = (*info)->end();

	*hasAppmeta = false;

	LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId(LsdbValueText(it->second));
	unsigned int kind = 0;

	it = (*info)->find(LSDB_KEY_UP_STATUS);
	if (it != end)
		kind = (unsigned int)gameLsdbValueInteger(it->second);

	string mountPoint;
	string appDir = string("gro0:app/") + titleId;
	string src = kind == 3 ?
		string("gro0:app/") + titleId + string("/sce_sys/retail/livearea") :
		string("gro0:app/") + titleId + string("/sce_sys/livearea");
	string dst = string("ur0:appmeta/") + titleId + string("/livearea");
	string retail = string("gro0:app/") + titleId + string("/sce_sys/retail/livearea");
	LocalFileStat stat;
	int ret = gameGameDataMount(appDir.c_str(), NULL, NULL, &mountPoint);

	if (ret < 0)
		return ret;

	ret = LocalFile::Getstat(retail.c_str(), &stat);
	if (ret >= 0) {
		*hasAppmeta = true;
		ret = 0;
	} else if (ret == GAME_PAF_ERROR_NOT_FOUND) {
		ret = 0;
	} else {
		gamePathGetMountedDrive(retail, detail);
	}

	gameAppUnmount(&mountPoint);

	return ret;
}
