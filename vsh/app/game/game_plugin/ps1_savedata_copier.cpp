/*
 * The game plugin's install of a PlayStation game's memory cards: two empty memory card images
 * (SCEVMC0.VMP, SCEVMC1.VMP), the saved data's param.sfo with its MACs and the game's ICON0.PNG
 * written in a new folder of ux0:pspemu/PSP/SAVEDATA, renamed to the game's title ID at the end
 * (0x81080444..0x81081F28). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <sceerror.h>
#include <kernel/iofilemgr.h>
#include <libkernel/libkernel.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>
#include <dir/dir.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

#define GAME_PS1_SAVEDATA_DIR	"ux0:pspemu/PSP/SAVEDATA"

/* the tries at a new folder's name, from the system time */
#define GAME_PS1_NEW_DIR_TRIES	1000

/*
 * A memory card image as the PSP emulator keeps it (.VMP): a 128-byte header, then the card's
 * 128 KiB of 128-byte frames.
 */
#define GAME_VMP_HEADER_SIZE	128
#define GAME_VMP_FRAME_SIZE	128
#define GAME_VMP_SIZE		(GAME_VMP_HEADER_SIZE + 0x20000)

/* the saved data's SAVEDATA_PARAMS: its flags and its three MACs */
#define GAME_SAVEDATA_PARAMS_SIZE	128
#define GAME_SAVEDATA_PARAMS_BIND	0x01	/* flags: the bind MAC is there */
#define GAME_SAVEDATA_PARAMS_SDK270	0x40	/* flags: the SDK 2.70 MAC is there */
#define GAME_SAVEDATA_MAC_COMMON	16	/* each MAC's offset in it */
#define GAME_SAVEDATA_MAC_BIND		32
#define GAME_SAVEDATA_MAC_SDK270	112
#define GAME_SAVEDATA_MAC_SIZE		16

/* the MACs' modes */
#define GAME_SAVEDATA_MAC_MODE_COMMON	1
#define GAME_SAVEDATA_MAC_MODE_SDK270	5
#define GAME_SAVEDATA_MAC_MODE_BIND	6

/* a SAVEDATA_FILE_LIST entry: a file's name and its hash */
struct GameSavedataFile {
	char name[13];
	SceUInt8 hash[16];
};

/* the memory cards' names */
static const char *s_cardNames[] = { "SCEVMC0.VMP", "SCEVMC1.VMP" };

/* an empty card's header: "\0PMV", its size, the salt seed and the card's SHA-1 */
static const SceUInt8 s_vmpHeader[GAME_VMP_HEADER_SIZE] = {
	0x00, 0x50, 0x4D, 0x56, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
	0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11, 0x12, 0x13,
	0x77, 0xCF, 0xFD, 0xF5, 0xE4, 0x67, 0xBC, 0xEC, 0xE9, 0x8D, 0x6E, 0x1A,
	0x6F, 0x44, 0x0F, 0x2A, 0x98, 0x6B, 0x2C, 0xC8,
};

/* the one file of the saved data, the emulator's CONFIG.BIN, with its hash */
static const GameSavedataFile s_configFile = {
	"CONFIG.BIN",
	{ 0x27, 0x91, 0x91, 0x8F, 0xA1, 0x7C, 0x08, 0x01, 0xFA, 0xA7, 0x1E, 0xD5, 0xE7, 0xD2, 0x5A,
	  0x84 },
};

GamePs1SavedataCopier::GamePs1SavedataCopier(GameCancelFunc cancel, void *arg,
					     const GameJobProgress *progress, string *detail,
					     float scale)
	: m_cancel(cancel), m_arg(arg), m_progress(*progress, scale), m_detail(detail)
{
	m_weights[0] = 0.05f;
	m_weights[1] = 0.05f;
	m_weights[2] = 0.1f;
	m_weights[3] = 0.2f;
	m_weights[4] = 0.2f;
	m_weights[5] = 0.1f;
	m_weights[6] = 0.2f;
	m_weights[7] = 0.05f;
}

GamePs1SavedataCopier::~GamePs1SavedataCopier()
{
}

/* the game's ICON0.PNG, from its EBOOT.PBP, written to iconPath (0x810804C6) */
__attribute__((noinline)) int GamePs1SavedataCopier::CopyIcon(const string *pbpPath,
							      const string *iconPath,
							      GameJobProgress *progress)
{
	GamePbpHeader header;
	int ret = gamePbpReadHeader(pbpPath->c_str(), &header, NULL);

	if (ret < 0) {
		gamePathGetMountedDrive(*pbpPath, m_detail);
		return ret;
	}

	if (progress != NULL)
		progress->Update(25.0f);

	SceSize size = header.icon1Offset - header.icon0Offset;
	char *buf = new char[size];

	if (buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	ret = gameFileReadAt(pbpPath, header.icon0Offset, size, buf);
	if (ret < 0) {
		delete[] buf;
		return ret;
	}

	if (progress != NULL)
		progress->Update(50.0f);

	ret = gameFileWrite(iconPath, buf, size);
	if (ret < 0) {
		delete[] buf;
		return ret;
	}

	if (progress != NULL)
		progress->Update(100.0f);

	delete[] buf;
	return 0;
}

/* a MAC of the param.sfo in buf, its size padded to 16 bytes with zeros */
static inline int gameSavedataMac(SceUInt mode, void *buf, SceSize size, SceSize alignedSize,
				  void *mac)
{
	MsSaveBBMacContext context;
	int ret = SceLibKernel_1693032E(&context, mode);

	if (ret < 0)
		return ret;

	sce_paf_memset((char *)buf + size, 0, alignedSize - size);

	ret = sceSblGcAuthMgrMsSaveBBMacUpdate(&context, buf, alignedSize);
	if (ret < 0)
		return ret;

	ret = SceLibKernel_774AE3CB(&context, mac, NULL);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The param.sfo's SAVEDATA_PARAMS made: its bind, SDK 2.70 and common MACs, each over the
 * param.sfo with the MACs before it, stored into it in turn (0x81080636).
 */
__attribute__((noinline)) int GamePs1SavedataCopier::SetMacs(SfoHandle *sfo, void *sfoBuf,
							     SceSize sfoSize, void *params)
{
	char *flags = (char *)params;
	void *param;
	int ret = sfoFindParam(sfo, "SAVEDATA_PARAMS", &param);

	if (ret < 0 && ret != SFO_ERROR_NOT_FOUND)
		return ret;

	SceSize alignedSize = (sfoSize + 15) & ~15;

	ret = gameSavedataMac(GAME_SAVEDATA_MAC_MODE_BIND, sfoBuf, sfoSize, alignedSize,
			      flags + GAME_SAVEDATA_MAC_BIND);
	if (ret < 0)
		return ret;

	gameDebugDump("bind mac", flags + GAME_SAVEDATA_MAC_BIND, GAME_SAVEDATA_MAC_SIZE);
	flags[0] |= GAME_SAVEDATA_PARAMS_BIND;
	ret = sfoSetParamValue(sfo, param, params, GAME_SAVEDATA_PARAMS_SIZE);
	if (ret < 0)
		return ret;

	flags[0] |= GAME_SAVEDATA_PARAMS_SDK270;
	flags[1] = 0;
	ret = sfoSetParamValue(sfo, param, params, GAME_SAVEDATA_PARAMS_SIZE);
	if (ret < 0)
		return ret;

	ret = gameSavedataMac(GAME_SAVEDATA_MAC_MODE_SDK270, sfoBuf, sfoSize, alignedSize,
			      flags + GAME_SAVEDATA_MAC_SDK270);
	if (ret < 0)
		return ret;

	gameDebugDump("SDK270 mac", flags + GAME_SAVEDATA_MAC_SDK270, GAME_SAVEDATA_MAC_SIZE);
	ret = sfoSetParamValue(sfo, param, params, GAME_SAVEDATA_PARAMS_SIZE);
	if (ret < 0)
		return ret;

	ret = gameSavedataMac(GAME_SAVEDATA_MAC_MODE_COMMON, sfoBuf, sfoSize, alignedSize,
			      flags + GAME_SAVEDATA_MAC_COMMON);
	if (ret < 0)
		return ret;

	gameDebugDump("common mac", flags + GAME_SAVEDATA_MAC_COMMON, GAME_SAVEDATA_MAC_SIZE);
	ret = sfoSetParamValue(sfo, param, params, GAME_SAVEDATA_PARAMS_SIZE);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The saved data's parameters added to the new param.sfo: the game's title, empty saved data
 * texts, its parental level, the category "MS", CONFIG.BIN, the folder's name and an empty
 * SAVEDATA_PARAMS, left in params (0x810807EE).
 */
__attribute__((noinline)) int GamePs1SavedataCopier::AddParams(SfoHandle *sfo,
							       const string *dirName,
							       const string *title,
							       int parentalLevel, void *params)
{
	sce_paf_memset(&m_param, 0, sizeof(m_param));

	sce_paf_strncpy(m_param.title, title->c_str(), sizeof(m_param.title) - 1);
	m_param.title[sizeof(m_param.title) - 1] = '\0';

	int ret = sfoAddParam(sfo, "TITLE", m_param.title, GAME_SFO_FORMAT_TEXT,
			      gameStrnlen(m_param.title, sizeof(m_param.title)) + 1,
			      sizeof(m_param.title));

	if (ret < 0)
		return ret;

	m_param.savedataTitle[0] = '\0';
	ret = sfoAddParam(sfo, "SAVEDATA_TITLE", m_param.savedataTitle, GAME_SFO_FORMAT_TEXT, 1,
			  sizeof(m_param.savedataTitle));
	if (ret < 0)
		return ret;

	m_param.savedataDetail[0] = '\0';
	ret = sfoAddParam(sfo, "SAVEDATA_DETAIL", m_param.savedataDetail, GAME_SFO_FORMAT_TEXT, 1,
			  sizeof(m_param.savedataDetail));
	if (ret < 0)
		return ret;

	ret = sfoAddParam(sfo, "PARENTAL_LEVEL", &parentalLevel, GAME_SFO_FORMAT_INTEGER,
			  sizeof(parentalLevel), sizeof(parentalLevel));
	if (ret < 0)
		return ret;

	ret = sfoAddParam(sfo, "CATEGORY", "MS", GAME_SFO_FORMAT_TEXT, 3, 4);
	if (ret < 0)
		return ret;

	sce_paf_memset(m_param.savedataFileList, 0, sizeof(m_param.savedataFileList));
	sce_paf_memcpy(m_param.savedataFileList, &s_configFile, sizeof(s_configFile));
	ret = sfoAddParam(sfo, "SAVEDATA_FILE_LIST", m_param.savedataFileList,
			  GAME_SFO_FORMAT_BINARY, sizeof(m_param.savedataFileList),
			  sizeof(m_param.savedataFileList));
	if (ret < 0)
		return ret;

	sce_paf_strncpy(m_param.savedataDirectory, dirName->c_str(),
			sizeof(m_param.savedataDirectory) - 1);
	m_param.savedataDirectory[sizeof(m_param.savedataDirectory) - 1] = '\0';
	ret = sfoAddParam(sfo, "SAVEDATA_DIRECTORY", m_param.savedataDirectory,
			  GAME_SFO_FORMAT_TEXT,
			  gameStrnlen(m_param.savedataDirectory,
				      sizeof(m_param.savedataDirectory)) + 1,
			  sizeof(m_param.savedataDirectory));
	if (ret < 0)
		return ret;

	sce_paf_memset(params, 0, GAME_SAVEDATA_PARAMS_SIZE);
	ret = sfoAddParam(sfo, "SAVEDATA_PARAMS", params, GAME_SFO_FORMAT_BINARY,
			  GAME_SAVEDATA_PARAMS_SIZE, GAME_SAVEDATA_PARAMS_SIZE);
	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The saved data's PARAM.SFO written into dir: made with its parameters, written out, opened
 * again from that copy for its MACs (0x810809B6).
 */
__attribute__((noinline)) int GamePs1SavedataCopier::WriteParam(const string *dir,
								const string *dirName,
								const string *title,
								int parentalLevel)
{
	SfoHandle *sfo = NULL;
	SceSize size = 0;
	string path;
	int ret = sfoCreate(&sfo);

	if (ret < 0)
		return ret;

	SceUInt8 params[GAME_SAVEDATA_PARAMS_SIZE];

	sce_paf_memset(params, 0, sizeof(params));
	ret = AddParams(sfo, dirName, title, parentalLevel, params);

	common::SharedPtr<File> file;
	void *buf = NULL;

	if (ret >= 0) {
		sfoGetSize(sfo, &size);
		buf = sce_paf_memalign(4, size);
		if (buf == NULL) {
			ret = GAME_ERROR_NO_INFO_MEMORY;
		} else {
			ret = sfoWrite(buf, size, sfo);
			if (ret >= 0) {
				sfoClose(sfo);
				ret = sfoOpen(buf, size, &sfo);
				if (ret >= 0) {
					ret = SetMacs(sfo, buf, size, params);
					if (ret >= 0) {
						path = *dir + string("/PARAM.SFO");
						file = LocalFile::Open(path.c_str(),
								       SCE_O_RDWR | SCE_O_CREAT |
								       SCE_O_TRUNC, 6, &ret);
						if (ret >= 0) {
							ret = file->Write(buf, size);
							if (ret < 0) {
								file->Close();
							} else {
								file->Close();
								ret = 0;
							}
						}
					}
				}
			}
		}
	}

	if (sfo != NULL)
		sfoClose(sfo);
	if (buf != NULL)
		sce_paf_free(buf);

	return ret;
}

/*
 * An empty memory card after its header: the card's "MC" frame, 15 free directory frames, 20
 * frames of the broken sector list, the rest of the card's header blocks erased (0xFF) and its
 * data blocks zero (0x81080CE0).
 */
__attribute__((noinline)) int GamePs1SavedataCopier::MakeCard(void *card)
{
	SceUInt8 *dst = (SceUInt8 *)card;
	SceUInt8 frame[GAME_VMP_FRAME_SIZE];
	int i;

	sce_paf_memcpy(dst, s_vmpHeader, sizeof(s_vmpHeader));

	sce_paf_memset(frame, 0, sizeof(frame));
	frame[0] = 'M';
	frame[1] = 'C';
	frame[127] = 0x0E;
	sce_paf_memcpy(dst + GAME_VMP_HEADER_SIZE, frame, sizeof(frame));

	frame[1] = 0;
	frame[127] = 0xA0;
	frame[0] = 0xA0;
	frame[9] = 0xFF;
	frame[8] = 0xFF;
	dst += GAME_VMP_HEADER_SIZE + GAME_VMP_FRAME_SIZE;
	for (i = 0; i < 15; i++) {
		sce_paf_memcpy(dst, frame, sizeof(frame));
		dst += sizeof(frame);
	}

	frame[0] = 0xFF;
	frame[1] = 0xFF;
	frame[2] = 0xFF;
	frame[3] = 0xFF;
	frame[127] = 0;
	for (i = 0; i < 20; i++) {
		sce_paf_memcpy(dst, frame, sizeof(frame));
		dst += sizeof(frame);
	}

	sce_paf_memset(dst, 0xFF, 28 * GAME_VMP_FRAME_SIZE);
	sce_paf_memset((SceUInt8 *)card + GAME_VMP_HEADER_SIZE + 0x2000, 0, 0x1E000);
	return 0;
}

/*
 * A new folder in parent, named prefix and the system time, made; its name left in name
 * (0x81080DBE).
 */
__attribute__((noinline)) int GamePs1SavedataCopier::MakeNewDir(const string *parent,
								const char *prefix, string *name)
{
	for (unsigned int i = 0; i < GAME_PS1_NEW_DIR_TRIES; i++) {
		SceKernelSysClock time;

		if (sceKernelGetSystemTime(&time) != 0)
			continue;

		char dirName[64];

		sce_paf_snprintf(dirName, sizeof(dirName), "%s%020llu", prefix, time.quad);

		string path = *parent + string("/") + string(dirName);

		if (Dir::IsExist(path.c_str()))
			continue;

		*name = dirName;
		Dir::CreateRecursive(path.c_str(), 6);
		return 0;
	}

	return GAME_ERROR_NO_MEMORY;
}

int GamePs1SavedataCopier::Run(const string *pbpPath, GameAppRegistration *reg)
{
	LsdbAppInfo::iterator end = reg->info->end();
	LsdbAppInfo::iterator it = reg->info->find(LSDB_KEY_TITLE_ID);

	if (it == end)
		return GAME_ERROR_NO_VALUE;

	string titleId = gameLsdbValueString(it->second);
	string savedataPath = string("ux0:pspemu/PSP/SAVEDATA/") + titleId;
	LocalFileStat stat;
	int ret = LocalFile::Getstat(savedataPath.c_str(), &stat);

	if (ret == 0)
		return 0;

	if (ret < 0 && ret != GAME_PAF_ERROR_NOT_FOUND) {
		gamePathGetMountedDrive(savedataPath, m_detail);
		return ret;
	}

	string title;

	gameAppInfoGetTitle(&reg->info, &title);

	int parentalLevel = 0;

	it = reg->info->find(LSDB_KEY_PARENTAL_LEVEL);
	if (it != end)
		parentalLevel = (int)gameLsdbValueInteger(it->second);

	m_progress.Set(gameSumPercent(0, m_weights, GAME_PS1_SAVEDATA_STEPS));
	m_progress.Set(gameSumPercent(1, m_weights, GAME_PS1_SAVEDATA_STEPS));

	string newDir;

	{
		string parent(GAME_PS1_SAVEDATA_DIR);

		ret = MakeNewDir(&parent, "_NEW_", &newDir);
	}
	if (ret < 0) {
		gamePathGetMountedDrive(string("ux0:"), m_detail);
		return ret;
	}

	m_progress.Set(gameSumPercent(2, m_weights, GAME_PS1_SAVEDATA_STEPS));

	string newPath = string(GAME_PS1_SAVEDATA_DIR) + string("/") + newDir;
	char *card = new char[GAME_VMP_SIZE];

	if (card == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	for (unsigned int i = 0; i < 2; i++) {
		string cardPath = newPath + string("/") + string(s_cardNames[i]);

		MakeCard(card);

		ret = sceIoOpen(cardPath.c_str(), SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 0606);
		if (ret >= 0) {
			SceUID fd = ret;
			SceSSize written = sceIoWrite(fd, card, GAME_VMP_SIZE);

			sceIoClose(fd);
			if (written != GAME_VMP_SIZE)
				ret = SCE_ERROR_ERRNO_EIO;
			else
				ret = 0;
		}

		if (ret != 0) {
			delete[] card;
			return ret;
		}
	}

	delete[] card;

	m_progress.Set(gameSumPercent(4, m_weights, GAME_PS1_SAVEDATA_STEPS));

	ret = WriteParam(&newPath, &titleId, &title, parentalLevel);
	if (ret < 0) {
		gamePathGetMountedDrive(newPath, m_detail);
		return ret;
	}

	m_progress.Set(gameSumPercent(5, m_weights, GAME_PS1_SAVEDATA_STEPS));

	GameJobProgress *iconProgress = new GameJobProgress(m_progress, m_weights[6]);

	if (iconProgress == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	string iconPath = newPath + string("/ICON0.PNG");

	ret = CopyIcon(pbpPath, &iconPath, iconProgress);
	if (ret < 0) {
		gamePathGetMountedDrive(*pbpPath, m_detail);
		return ret;
	}

	m_progress.Set(gameSumPercent(6, m_weights, GAME_PS1_SAVEDATA_STEPS));

	string finalPath = string(GAME_PS1_SAVEDATA_DIR) + string("/") + titleId;

	ret = LocalFile::RenameFile(newPath.c_str(), finalPath.c_str());
	if (ret < 0) {
		gamePathGetMountedDrive(newPath, m_detail);
		return ret;
	}

	m_progress.Set(gameSumPercent(7, m_weights, GAME_PS1_SAVEDATA_STEPS));
	return 0;
}
