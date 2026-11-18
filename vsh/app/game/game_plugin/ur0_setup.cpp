/*
 * An application set up in ur0: its appmeta (icons, livearea) made from its package, for a game,
 * an EG or ME application or a PSM one (0x81035AA8..0x8103C120). The file's name and the
 * class's are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* where a PSM application's appmeta is made, and where it goes */
#define GAME_TEMP_APPMETA_DIR	"ur0:temp/appmeta"
#define GAME_APPMETA_DIR	"ur0:appmeta"

/* the space a PSM application's appmeta needs */
#define GAME_PSM_APPMETA_SIZE	0x100000

/* the livearea files every PSM application gets from the system */
static const char *s_psmLiveAreaFiles[] = {
	"LA_bg.png",
	"LA_DeveloperLink.png",
};

/* 0x81035AA8 */
GameUr0Setup::GameUr0Setup(const LsdbAppInfoPtr *info, int type)
	: m_info(*info), m_type(type), m_error(0), m_storage(0), m_upStatus(0),
	  m_mounted(false), m_dataMounted(false), m_promoteAttrBit2(false)
{
	int ret;

	if ((*info)->size() != 0 && type < 3) {
		LsdbAppInfo::iterator it = (*info)->find(LSDB_KEY_TITLE_ID);

		if (it == (*info)->end()) {
			ret = GAME_ERROR_NO_VALUE;
			goto end;
		}

		m_titleId = LsdbValueText(it->second);

		it = (*info)->find(LSDB_KEY_CATEGORY);
		if (it == (*info)->end()) {
			ret = GAME_ERROR_NO_VALUE;
			goto end;
		}

		m_category = LsdbValueText(it->second);

		it = (*info)->find(LSDB_KEY_ORG_PATH);
		if (it == (*info)->end()) {
			ret = GAME_ERROR_NO_VALUE;
			goto end;
		}

		m_orgPath = LsdbValueText(it->second);

		ret = gameAppGetStorage(info, &m_storage);
		if (ret >= 0) {
			it = (*info)->find(LSDB_KEY_UP_STATUS);
			if (it != (*info)->end())
				m_upStatus = (SceUInt32)gameLsdbValueInteger(it->second);

			if (gameAppInfoPromoteAttrBit2(info))
				m_promoteAttrBit2 = true;
		}
	} else {
		ret = GAME_ERROR_INVALID_ARGUMENT;
	}

end:
	m_error = ret;
}

/* 0x81035CEE, deleting 0x81035DD4 */
GameUr0Setup::~GameUr0Setup()
{
}

/*
 * A PSM application's livearea in dir: its template.xml written from the record, its icon as the
 * startup image and the system's background and developer link (0x81035DE4).
 */
int GameUr0Setup::SetupPsmLiveArea(const string &dir, GamePsmAppInfo *appInfo)
{
	string contentsDir = dir + string("/livearea/contents");
	int ret = Dir::CreateRecursive(contentsDir.c_str(), 6);

	if (ret < 0)
		return ret;

	float percent = 0;
	GameJobProgress progress(&percent);
	GamePsmLiveAreaWriter *writer = new GamePsmLiveAreaWriter(NULL, NULL, &progress, NULL, 1.0f);

	if (writer == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	writer->m_appInfo = appInfo;

	string templatePath = dir + string("/livearea/contents/template.xml");

	ret = writer->Write(templatePath, &m_info);
	if (ret < 0) {
		delete writer;
		return ret;
	}

	delete writer;

	string iconPath = dir + string("/icon0.dds");
	string startupPath = dir + string("/livearea/contents/startup.dds");

	ret = gameCopyFileWithPool(iconPath.c_str(), startupPath.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	const char **name = s_psmLiveAreaFiles;

	for (SceUInt32 i = 0; i < 2; i++, name++) {
		string src = string("vs0:vsh/game/psm/contents/") + string(*name);
		string dst = dir + string("/livearea/contents/") + string(*name);

		ret = gameCopyFileWithPool(src.c_str(), dst.c_str(), 0, 0, 0, 0);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * A PSM application's icons in dir, as its type wants them, and its livearea for a type 0 one
 * (0x810364B4).
 */
int GameUr0Setup::SetupPsmIcons(const string &dir, GamePsmAppInfo *appInfo)
{
	int ret;

	Dir::CreateRecursive(dir.c_str(), 6);

	if (m_type == 1 || m_type == 2 || m_type == 0) {
		string iconPath = dir + string("/icon0.dds");

		ret = appInfo->WriteIcon(iconPath);
		if (ret < 0)
			return ret;
	}

	if (m_type == 0 || m_type == 2) {
		string picPath = dir + string("/pic0.png");

		ret = appInfo->WritePic(picPath);
		if (ret < 0)
			return ret;
	}

	if (m_type != 0)
		return 0;

	ret = SetupPsmLiveArea(dir, appInfo);
	if (ret >= 0)
		return 0;

	return ret;
}

/*
 * A PSM application's appmeta made in ur0:temp/appmeta from its app.info and moved into
 * ur0:appmeta (0x81036730).
 */
int GameUr0Setup::SetupPsm(void)
{
	string tempDir(GAME_TEMP_APPMETA_DIR);
	string appmetaDir = string(GAME_TEMP_APPMETA_DIR "/") + m_titleId;
	int ret = gameUr0CheckFreeSpace(GAME_PSM_APPMETA_SIZE, &m_detail);

	if (ret < 0)
		return ret;

	string appInfoPath = m_orgPath + string("/RO/Application/app.info");
	LsdbAppInfoPtr info(new LsdbAppInfo);

	if (info.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	GamePsmAppInfo *appInfo = new GamePsmAppInfo();

	if (appInfo == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = appInfo->Load(appInfoPath.c_str(), &m_info, &info);
	if (ret >= 0) {
		gameRemoveDirectoryContents(GAME_TEMP_APPMETA_DIR, 0, 0, 0, true);
		Dir::CreateRecursive(GAME_APPMETA_DIR, 6);

		ret = SetupPsmIcons(appmetaDir, appInfo);
		if (ret >= 0) {
			if (m_type == 1 || m_type == 2) {
				string dst = string(GAME_APPMETA_DIR "/") + m_titleId;

				ret = gameMoveTree(appmetaDir.c_str(), dst.c_str(), NULL, NULL, &m_detail);
				if (ret < 0)
					return ret;

				ret = gameRemoveDirectoryContents(tempDir.c_str(), 0, 0, 0, true);
				if (ret < 0)
					goto end;
			} else {
				ret = gameAppmetaCommit(&tempDir, &m_titleId, 0);
				if (ret >= 0)
					ret = gameRemoveDirectoryContents(tempDir.c_str(), 0, 0, 0,
									  true);
			}
		}
	}

end:
	delete appInfo;

	return ret;
}

/* the livearea the PSP emulator gives every PlayStation game */
#define GAME_PS1_LIVEAREA_DIR	"vs0:app/NPXS10028/sce_sys/ps1_livearea/contents"

/* A PlayStation game's livearea in dir, the PSP emulator's (0x81036E36). */
int GameUr0Setup::SetupPs1LiveArea(const string &dir)
{
	string templateSrc(GAME_PS1_LIVEAREA_DIR "/template.xml");
	string dst = dir + string("/livearea/contents/template.xml");
	int ret = gameCopyFileWithPool(templateSrc.c_str(), dst.c_str(), 0, 0, 0, 0);

	if (ret < 0)
		return ret;

	string bgSrc(GAME_PS1_LIVEAREA_DIR "/bg0.png");

	dst = dir + string("/livearea/contents/bg0.png");
	ret = gameCopyFileWithPool(bgSrc.c_str(), dst.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	string pocketStationOffSrc(GAME_PS1_LIVEAREA_DIR "/pokesute_off.png");

	dst = dir + string("/livearea/contents/pokesute_off.png");
	ret = gameCopyFileWithPool(pocketStationOffSrc.c_str(), dst.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	string pocketStationOnSrc(GAME_PS1_LIVEAREA_DIR "/pokesute_on.png");

	dst = dir + string("/livearea/contents/pokesute_on.png");
	ret = gameCopyFileWithPool(pocketStationOnSrc.c_str(), dst.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	string ps1Src(GAME_PS1_LIVEAREA_DIR "/ps1.png");

	dst = dir + string("/livearea/contents/ps1.png");
	ret = gameCopyFileWithPool(ps1Src.c_str(), dst.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	return 0;
}

/* the livearea the PSP emulator gives every PSP application */
#define GAME_PSP_LIVEAREA_DIR	"vs0:app/NPXS10028/sce_sys/psp_livearea/contents"

/* A PSP application's livearea in dir, the PSP emulator's (0x8103766E). */
int GameUr0Setup::SetupPspLiveArea(const string &dir)
{
	string templateSrc(GAME_PSP_LIVEAREA_DIR "/template.xml");
	string dst = dir + string("/livearea/contents/template.xml");
	int ret = gameCopyFileWithPool(templateSrc.c_str(), dst.c_str(), 0, 0, 0, 0);

	if (ret < 0)
		return ret;

	string bgSrc(GAME_PSP_LIVEAREA_DIR "/bg0.png");

	dst = dir + string("/livearea/contents/bg0.png");
	ret = gameCopyFileWithPool(bgSrc.c_str(), dst.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * An EG or ME application's icons in dir from its EBOOT.PBP's ICON0, and its livearea for a type
 * 0 one: the EBOOT's icon as the startup image and the PSP emulator's template (0x81037978).
 */
int GameUr0Setup::SetupPspIcons(const string &dir, const string *ebootPath)
{
	int kind = 0;
	GamePbpHeader header;

	Dir::CreateRecursive(dir.c_str(), 6);

	int ret = gamePbpReadHeader(ebootPath->c_str(), &header, NULL);

	if (ret < 0)
		return ret;

	string startupPath;
	string iconPath;
	SceSize size = header.icon1Offset - header.icon0Offset;
	SceUInt8 *icon = new SceUInt8[size];

	if (icon == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameFileReadAt(ebootPath, header.icon0Offset, size, icon);
	if (ret < 0)
		goto end;

	if (m_type == 1 || m_type == 2 || m_type == 0) {
		ret = gameAppInfoGetCategoryKind(&m_info, &kind);
		if (ret < 0)
			goto end;

		int attributeKind = 0;

		if (kind == 2)
			attributeKind = gameAppInfoGetAttributeKind(&m_info);

		iconPath = dir + string("/icon0.dds");

		ret = gameIconWrite(kind, attributeKind, &iconPath, icon, size);
		if (ret < 0)
			goto end;
	}

	if (m_type == 0) {
		string contentsDir = dir + string("/livearea/contents");

		ret = Dir::CreateRecursive(contentsDir.c_str(), 6);
		if (ret < 0)
			goto end;

		startupPath = dir + string("/livearea/contents/startup.png");

		ret = gameFileWrite(&startupPath, icon, size);
		if (ret < 0)
			goto end;

		if (m_category == "EG")
			ret = SetupPspLiveArea(dir);
		else if (m_category == "ME")
			ret = SetupPs1LiveArea(dir);
		else
			ret = GAME_ERROR_NOT_APP_PACKAGE;

		if (ret < 0)
			goto end;

		ret = 0;
	}

end:
	delete[] icon;

	return ret;
}

/* where the PSP emulator keeps PSP games and their updates */
#define GAME_PSPEMU_GAME_DIR	"ux0:pspemu/PSP/GAME"

/* the game whose update's PBOOT.PBP is never booted */
#define GAME_PSP_NO_PBOOT_ID	"NPJH90038"

/* the digits of a PBP's version compared */
#define GAME_PBP_VERSION_DIGITS	6

/*
 * The file a PSP game boots: its EBOOT.PBP, or the update in the PSP emulator's folder (by disc
 * ID or title ID) whose PBOOT.PBP's version is the highest, or with ATTRIBUTE bit 0 its
 * PARAM.PBP (0x81037E78). 0 with path untouched when the game has no EBOOT.PBP.
 */
int GameUr0Setup::GetPspBootPath(string *path)
{
	string gameDir(GAME_PSPEMU_GAME_DIR);
	string bootPath = m_orgPath + string("/EBOOT.PBP");
	LocalFileStat stat;

	if (LocalFile::Getstat(bootPath.c_str(), &stat) < 0)
		return 0;

	LsdbAppInfoPtr ebootSfo(new LsdbAppInfo);

	if (ebootSfo.get() == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	int ebootVersion = -1;
	SceUInt8 attribute = 0;
	int ret = gamePbpReadSfo(&bootPath, &ebootSfo);

	if (ret < 0 || (ret = gameSfoCheck(&ebootSfo)) < 0)
		return ret;

	LsdbAppInfo::iterator it = ebootSfo->find(LsdbKeyHash("ATTRIBUTE"));

	if (it != ebootSfo->end())
		attribute = (SceUInt8)gameLsdbValueInteger(it->second);

	it = ebootSfo->find(LSDB_KEY_APP_VER);
	if (it == ebootSfo->end()) {
		ebootVersion = 0;
	} else if (gameVersionToNumber(LsdbValueText(it->second), GAME_PBP_VERSION_DIGITS,
				       (SceUInt32 *)&ebootVersion) < 0) {
		ebootVersion = 0;
	}

	string discPbootPath;
	string gameDataId;

	ret = gamePspGetGameDataId(&ebootSfo, &gameDataId);
	if (ret < 0)
		return ret;

	if (gameDataId.length() != 0) {
		discPbootPath = gameDir + string("/") + gameDataId + string("/PBOOT.PBP");
		if (LocalFile::Getstat(discPbootPath.c_str(), &stat) < 0)
			discPbootPath.assign(NULL, 0);

		if (discPbootPath.length() != 0 && gameDataId == GAME_PSP_NO_PBOOT_ID) {
			*path = bootPath;
			return 0;
		}
	}

	string titlePbootPath;
	string paramPath;

	titlePbootPath = gameDir + string("/") + m_titleId + string("/PBOOT.PBP");
	if (LocalFile::Getstat(titlePbootPath.c_str(), &stat) < 0)
		titlePbootPath.assign(NULL, 0);

	paramPath = gameDir + string("/") + m_titleId + string("/PARAM.PBP");
	if (LocalFile::Getstat(paramPath.c_str(), &stat) < 0)
		paramPath.assign(NULL, 0);

	LsdbAppInfoPtr titleSfo(new LsdbAppInfo);
	LsdbAppInfoPtr discSfo(new LsdbAppInfo);
	LsdbAppInfoPtr paramSfo(new LsdbAppInfo);
	int titleVersion = -1;
	int discVersion = -1;

	if (titlePbootPath.length() != 0 && gamePbpReadSfo(&titlePbootPath, &titleSfo) == 0) {
		/* Sony checks the EBOOT's param.sfo again here */
		ret = gameSfoCheck(&ebootSfo);
		if (ret < 0)
			return ret;

		LsdbAppInfo::iterator end = titleSfo->end();

		it = titleSfo->find(LSDB_KEY_APP_VER);
		if (it == end) {
			titleVersion = 0;
		} else if (gameVersionToNumber(LsdbValueText(it->second), GAME_PBP_VERSION_DIGITS,
					       (SceUInt32 *)&titleVersion) < 0) {
			titleVersion = 0;
		}
	}

	if (discPbootPath.length() != 0 && gamePbpReadSfo(&discPbootPath, &discSfo) == 0) {
		ret = gameSfoCheck(&discSfo);
		if (ret < 0)
			return ret;

		LsdbAppInfo::iterator end = discSfo->end();

		it = discSfo->find(LSDB_KEY_APP_VER);
		if (it == end) {
			discVersion = 0;
		} else if (gameVersionToNumber(LsdbValueText(it->second), GAME_PBP_VERSION_DIGITS,
					       (SceUInt32 *)&discVersion) < 0) {
			discVersion = 0;
		}
	}

	if (paramPath.length() != 0) {
		ret = gamePbpReadSfo(&paramPath, &paramSfo);
		if (ret < 0)
			return ret;

		ret = gameSfoCheck(&paramSfo);
		if (ret < 0)
			return ret;
	}

	LsdbAppInfo unused;	/* Sony's, not used */

	if (discVersion >= titleVersion && ebootVersion <= discVersion) {
		titlePbootPath.assign(NULL, 0);
		titleSfo->clear();
		bootPath = discPbootPath;
		titleSfo = discSfo;
	} else if (discVersion < titleVersion && ebootVersion <= titleVersion) {
		discPbootPath.assign(NULL, 0);
		discSfo->clear();
		bootPath = titlePbootPath;
		titleSfo = titleSfo;
	}

	string boot = bootPath;

	if ((attribute & 1) != 0 && paramPath.length() != 0)
		boot = paramPath;

	*path = boot;

	return 0;
}

/*
 * An EG or ME application's appmeta made in ur0:temp/appmeta from the file it boots and moved
 * into ur0:appmeta, the PSP emulator's work folder mounted (0x8103931A). Nothing is made for a
 * game without an EBOOT.PBP.
 */
int GameUr0Setup::SetupPsp(void)
{
	string bootPath;
	string tempDir;
	string appmetaDir;
	string dst;
	string mountPoint;
	int ret = gameUr0CheckFreeSpace(GAME_PSM_APPMETA_SIZE, &m_detail);

	if (ret < 0)
		return ret;

	ret = gamePspemuWorkMount(&mountPoint);
	if (ret < 0)
		goto end;

	ret = GetPspBootPath(&bootPath);
	if (ret < 0)
		goto end;

	if (bootPath.length() == 0) {
		ret = 0;
		goto end;
	}

	gameRemoveDirectoryContents(GAME_TEMP_APPMETA_DIR, 0, 0, 0, true);
	Dir::CreateRecursive(GAME_APPMETA_DIR, 6);

	tempDir = string(GAME_TEMP_APPMETA_DIR);
	appmetaDir = tempDir + string("/") + m_titleId;

	ret = SetupPspIcons(appmetaDir, &bootPath);
	if (ret < 0)
		goto end;

	if (m_type == 1 || m_type == 2) {
		dst = string(GAME_APPMETA_DIR "/") + m_titleId;

		ret = gameMoveTree(appmetaDir.c_str(), dst.c_str(), NULL, NULL, &m_detail);
		if (ret < 0)
			goto end;

		ret = gameRemoveDirectoryContents(tempDir.c_str(), 0, 0, 0, true);
	} else {
		ret = gameAppmetaCommit(&tempDir, &m_titleId, 0);
		if (ret < 0)
			goto end;

		ret = gameRemoveDirectoryContents(tempDir.c_str(), 0, 0, 0, true);
	}

end:
	gamePspemuWorkUnmount(&mountPoint);

	return ret;
}

/* the update status of an application whose patch carries a retail livearea */
#define GAME_UP_STATUS_RETAIL_LIVEAREA	3

/*
 * A game's appmeta in dir from its mounted package: its icon and background, and for a type 0
 * one its livearea (the patch's retail one when it has one). A package on storage 3, or on 2
 * mounted as data, keeps them at its root rather than in sce_sys (0x810397F4).
 */
int GameUr0Setup::SetupGameAppmeta(const string &dir)
{
	bool noSceSys = false;
	int ret;

	if (m_storage == 3 || (m_storage == 2 && m_dataMounted))
		noSceSys = true;

	if (m_type == 1 || m_type == 2 || m_type == 0) {
		string src = m_mountPoint + string("/sce_sys/icon0.png");

		if (noSceSys)
			src = m_mountPoint + string("/icon0.png");

		string dst = dir + string("/icon0.png");

		ret = gameImageConvert(&src, &dst, gameIcon0Format(), &m_detail);
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
			return ret;
	}

	if (m_type == 0 || m_type == 2) {
		string src = m_mountPoint + string("/sce_sys/pic0.png");

		if (noSceSys)
			src = m_mountPoint + string("/pic0.png");

		string dst = dir + string("/pic0.png");

		ret = gameImageConvert(&src, &dst, gamePic0Format(), &m_detail);
		if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND)
			return ret;
	}

	if (m_type != 0)
		return 0;

	string src;
	string dst = dir + string("/livearea");

	if (m_upStatus == 0 || m_upStatus == 1) {
		src = m_mountPoint + string("/sce_sys/livearea");
		if (noSceSys)
			src = m_mountPoint + string("/livearea");
	} else if (m_upStatus == GAME_UP_STATUS_RETAIL_LIVEAREA) {
		src = m_mountPoint + string("/sce_sys/retail/livearea");
		if (noSceSys)
			src = m_mountPoint + string("/retail/livearea");

		if (!Dir::IsExist(src.c_str())) {
			src = m_mountPoint + string("/sce_sys/livearea");
			if (noSceSys)
				src = m_mountPoint + string("/livearea");
		}
	}

	ret = gameCopyFileWithPool(src.c_str(), dst.c_str(), 0, 0, 0, 0);
	if (ret >= 0 || ret == GAME_PAF_ERROR_NOT_FOUND)
		return 0;

	return ret;
}

/* the largest param.sfo a game card's appmeta keeps */
#define GAME_GC_PARAM_SFO_MAX	0x8000

/* The game card's param.sfo copied into dir as gc_param.sfo (0x8103A56A). */
int GameUr0Setup::SetupGcParamSfo(const string &dir)
{
	string src("gro0:gc/param.sfo");
	string dst = dir + string("/gc_param.sfo");
	LocalFileStat stat;
	int ret = LocalFile::Getstat(src.c_str(), &stat);

	if (ret < 0)
		return ret;

	if (stat.st_size > GAME_GC_PARAM_SFO_MAX)
		return GAME_ERROR_BAD_PARAM_SIZE;

	ret = gameCopyFileWithPool(src.c_str(), dst.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	return 0;
}

/* the extra EBOOTs (eboot2.bin, ...) a game's appmeta keeps the parameters of */
#define GAME_EXTRA_EBOOT_COUNT	1

/*
 * A game's appmeta in dir: unless skipParams, a game card's param.sfo, the package's parameters
 * and its eboot.bin's (and eboot2.bin's) parameters, then its icons and livearea (0x8103A7FE).
 */
int GameUr0Setup::SetupGameDir(const string &dir, bool skipParams)
{
	int ret;

	if (skipParams)
		goto appmeta;

	if (m_type == 0 && (m_storage == 1 || m_storage == 2)) {
		ret = SetupGcParamSfo(dir.c_str());
		if (ret < 0)
			return ret;
	}

	if (m_type != 0 && m_type != 2)
		goto appmeta;

	ret = gameAppmetaCopyParams(m_mountPoint.c_str(), dir.c_str(), &m_detail);
	if (ret < 0)
		return ret;

	{
		LocalFileStat stat;
		string ebootPath = m_mountPoint + string("/eboot.bin");
		string paramPath = dir + string("/ebootparam.bin");

		ret = gameAppmetaWriteEbootParam(&ebootPath, &paramPath, &m_detail);
		if (ret < 0)
			return ret;

		for (SceUInt32 i = 0; i < GAME_EXTRA_EBOOT_COUNT; i++) {
			char name[32];

			sce_paf_snprintf(name, sizeof(name), "/eboot%u.bin", i + 2);
			ebootPath = m_orgPath + string(name);

			if (LocalFile::Getstat(ebootPath.c_str(), &stat) == 0) {
				paramPath = dir + string("/ebootparam2.bin");

				ret = gameAppmetaWriteEbootParam(&ebootPath, &paramPath, &m_detail);
				if (ret < 0)
					return ret;
			}
		}
	}

appmeta:
	ret = SetupGameAppmeta(dir);
	if (ret >= 0)
		return 0;

	return ret;
}

/* the free space a game's appmeta needs on ur0: */
#define GAME_APPMETA_SIZE	0x500000

/*
 * A game's appmeta made in ur0:temp/appmeta and moved into ur0:appmeta, copied for a type 1 or 2
 * one, committed otherwise (0x8103AD14).
 */
int GameUr0Setup::SetupGameAppmetaDir(bool skipParams)
{
	int ret = gameUr0CheckFreeSpace(GAME_APPMETA_SIZE, &m_detail);

	if (ret < 0)
		return ret;

	string tempDir(GAME_TEMP_APPMETA_DIR);
	string dir = tempDir + string("/") + m_titleId;

	ret = Dir::CreateRecursive(dir.c_str(), 6);
	if (ret < 0) {
		gamePathGetMountedDrive(dir, &m_detail);
		return ret;
	}

	ret = SetupGameDir(dir, skipParams);
	if (ret < 0)
		return ret;

	if (m_type == 1 || m_type == 2) {
		string dst = string(GAME_APPMETA_DIR "/") + m_titleId;

		ret = gameMoveTree(dir.c_str(), dst.c_str(), NULL, NULL, &m_detail);
		if (ret < 0)
			return ret;

		ret = gameRemoveDirectoryContents(tempDir.c_str(), 0, 0, 0, true);
		if (ret < 0)
			return ret;
	} else {
		ret = gameAppmetaCommit(&tempDir, &m_titleId, 0);
		if (ret < 0)
			return ret;

		ret = gameRemoveDirectoryContents(tempDir.c_str(), 0, 0, 0, true);
		if (ret < 0)
			return ret;
	}

	return 0;
}

/*
 * The appmeta of a package that keeps its attribute bit 2 (a promoted one): its icon and
 * param.sfo from the mounted package, committed under titleId (0x8103B1F2).
 */
int GameUr0Setup::SetupPromoteAppmeta(const string *titleId)
{
	string detail;
	int ret = gameUr0CheckFreeSpace(GAME_APPMETA_SIZE, &detail);

	if (ret < 0)
		return ret;

	string dir = string(GAME_TEMP_APPMETA_DIR "/") + *titleId;

	ret = Dir::CreateRecursive(dir.c_str(), 6);
	if (ret < 0)
		return ret;

	string iconSrc = m_mountPoint + string("/icon0.png");
	string iconDst = dir + string("/icon0.png");

	ret = gameCopyFileWithPool(iconSrc.c_str(), iconDst.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	string paramSrc = m_mountPoint + string("/param.sfo");
	string paramDst = dir + string("/param.sfo");

	ret = gameCopyFileWithPool(paramSrc.c_str(), paramDst.c_str(), 0, 0, 0, 0);
	if (ret < 0)
		return ret;

	ret = gameAppmetaCommit(&dir, titleId, 1);
	if (ret < 0)
		return ret;

	return ret;
}

/*
 * The package mounted: nothing for one in vs0:; a game card's (gro0:app) with its patch from
 * grw0:patch on storage 1, or on storage 2 as data when ux0:appmeta has it; one in pd0:app
 * mounted, any other as data (0x8103B8F4).
 */
int GameUr0Setup::Mount(void)
{
	int ret = 0;
	string patchPath;

	if (m_orgPath.find("vs0:", 0, sce_paf_strlen("vs0:")) != string::npos) {
		patchPath.clear();
		goto end;
	}

	if (m_orgPath.find("gro0:app/", 0, sce_paf_strlen("gro0:app/")) != string::npos) {
		switch (m_storage) {
		case 1:
			m_mounted = true;
			patchPath = string("grw0:patch/") + m_titleId;
			break;
		case 2: {
			string appmetaPath = string("ux0:appmeta/") + m_titleId;

			if (Dir::IsExist(appmetaPath.c_str())) {
				m_dataMounted = true;
			} else {
				m_mounted = true;
				patchPath.clear();
			}
			break;
		}
		default:
			return GAME_ERROR_BAD_STORAGE;
		}
	} else if (m_orgPath.find("pd0:app/", 0, sce_paf_strlen("pd0:app/")) != string::npos) {
		m_mounted = true;
		patchPath.clear();
	} else {
		m_dataMounted = true;
	}

	if (m_mounted) {
		if (m_storage == 1 || m_storage == 2) {
			string appPath = string("gro0:app/") + m_titleId;

			if (!Dir::IsExist(appPath.c_str()))
				return GAME_ERROR_NO_GC_APP;
		}

		if (patchPath.length() != 0 && Dir::IsExist(patchPath.c_str()))
			ret = gameGameDataMount(m_orgPath.c_str(), patchPath.c_str(), NULL,
						&m_mountPoint);
		else
			ret = gameGameDataMount(m_orgPath.c_str(), NULL, NULL, &m_mountPoint);
		if (ret < 0)
			return ret;
	}

	if (m_dataMounted) {
		ret = gameAppDataMount(string(m_titleId.c_str()), &m_mountPoint, true);
		if (ret < 0)
			return ret;
	}

end:
	return ret;
}

/* whether the package's category is a game's: gd, gp, gdc, gpc or gdb (0x8103BEBE) */
bool GameUr0Setup::IsGameCategory(void)
{
	return m_category == "gd" || m_category == "gp" || m_category == "gdc" ||
	       m_category == "gpc" || m_category == "gdb";
}

/*
 * A game's appmeta made from its mounted package, unmounted again: a promoted one's from its
 * icon and param.sfo only; the others' fully, without their parameters on storage 3 or on a
 * storage 2 unit with ux0: (0x8103BF62).
 */
int GameUr0Setup::SetupGame(void)
{
	if (!gameDriveIsNotData(m_category))
		return 0;

	if (!IsGameCategory())
		return 0;

	bool skipParams = false;
	int ret = Mount();

	if (ret < 0)
		goto unmount;

	gameRemoveDirectoryContents(GAME_TEMP_APPMETA_DIR, 0, 0, 0, true);
	Dir::CreateRecursive(GAME_APPMETA_DIR, 6);

	if (m_promoteAttrBit2) {
		ret = SetupPromoteAppmeta(&m_titleId);
		goto unmount;
	}

	switch (m_storage) {
	case 1:
		break;
	case 2:
		if (Dir::IsExist("ux0:"))
			skipParams = true;
		break;
	case 3:
		skipParams = true;
		break;
	case 4:
		break;
	default:
		ret = GAME_ERROR_NO_STORAGE;
		goto unmount;
	}

	ret = SetupGameAppmetaDir(skipParams);

unmount:
	if (m_mountPoint.length() != 0) {
		if (m_mounted && gameAppUnmount(&m_mountPoint) < 0)
			return ret;
		if (m_dataMounted)
			gameAppDataUnmount(&m_mountPoint);
	}

	return ret;
}

/*
 * The application's appmeta in ur0:appmeta set up by its category: a game's, a PSP
 * application's or a PSM one's (0x8103C03E).
 */
int GameUr0Setup::Run(void)
{
	if (m_error < 0)
		return m_error;

	if (gameCategoryIsGame(m_category.c_str()) || m_category == "gdb")
		return SetupGame();

	if (gameCategoryIsEg(m_category.c_str()) || gameCategoryIsMe(m_category.c_str()))
		return SetupPsp();

	if (gameCategoryIsPsm(m_category.c_str()))
		return SetupPsm();

	return GAME_ERROR_UNKNOWN_CATEGORY;
}
