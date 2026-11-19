/*
 * The licenses of an application's additional contents as its addcont/info folder keeps them,
 * for the license updater: the folder made ready (its instv.dat), the licenses read, one removed,
 * one written, for a Vita application and for a PSP one, under the "SceGameUpdateAddcontLicense"
 * mutex (0x81098680..0x8109FEA4). NPXS10079 carries the same code. The file's name and the
 * functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <file/localfile.h>
#include <dir/dir.h>

#include "game_plugin.h"
#include "license_updater.h"

using namespace paf;

/* the addcont/info functions' errors (values only) */
#define GAME_ADDCONT_INFO_ERROR_INVALID_ARG	-2146416480	/* 0x8010A1A0 */
#define GAME_ADDCONT_INFO_ERROR_IO		-2146416479	/* 0x8010A1A1 */
#define GAME_ADDCONT_INFO_ERROR_BAD_DATA	-2146416478	/* 0x8010A1A2: no magic */
#define GAME_ADDCONT_INFO_ERROR_NO_INSTV	-2146416477	/* 0x8010A1A3: no instv.dat */
#define GAME_ADDCONT_INFO_ERROR_NOT_FOUND	-2146416475	/* 0x8010A1A5: no such license */
#define GAME_ADDCONT_INFO_ERROR_OLD_INSTV	-2146416474	/* 0x8010A1A6: an older version */

/* what instv.dat starts with */
#define GAME_ADDCONT_INSTV_MAGIC	"ACIV"
/* what a PSP application's instp.dat starts with */
#define GAME_ADDCONT_INSTP_MAGIC	"ACIP"
/* the version this code writes; an older one is not read */
#define GAME_ADDCONT_INSTV_VERSION	1

/* instv.dat's (and instp.dat's) header, 76 bytes, followed by count GameAddcontLicenseInfo */
struct GameAddcontInstv {
	char magic[4];		/* GAME_ADDCONT_INSTV_MAGIC */
	SceUInt32 version;	/* GAME_ADDCONT_INSTV_VERSION */
	SceUInt32 count;	/* the licenses after the header */
	SceUInt8 padding[64];
};

typedef char GameAddcontInstvSize[sizeof(GameAddcontInstv) == 76 ? 1 : -1];

static thread::RMutex s_addcontInfoMutex("SceGameUpdateAddcontLicense", 0);

/* whether path is on ux0: (0x81098680) */
static __attribute__((noinline)) bool gameAddcontInfoIsUx0(const string &path)
{
	return path.find("ux0:", 0, sce_paf_strlen("ux0:")) != string::npos;
}

/* the folder the new files are written in first: temp/plus of path's drive (0x810986F0) */
static __attribute__((noinline)) string gameAddcontInfoTempDir(const string &path)
{
	const char *drive = "grw0:";

	/* Sony asks once without using the answer */
	gameAddcontInfoIsUx0(path);
	if (gameAddcontInfoIsUx0(path))
		drive = "ux0:";

	string dir(drive);

	dir += "temp/plus";
	return dir;
}

/*
 * The addcont/info folder at path made ready: an instv.dat of the current version kept, any other
 * replaced by a new one, written in the temp folder and moved in (0x8109879C).
 */
int gameAddcontInfoPrepare(const char *path)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);
	int ret = 0;

	if (path == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	bool grw0 = sce_paf_strncmp(path, "grw0:addcont/info", 18) == 0;

	if (sce_paf_strncmp(path, "ux0:addcont/info", 17) == 0 || grw0)
		ret = 0;
	else
		ret = GAME_ADDCONT_INFO_ERROR_INVALID_ARG;
	if (ret < 0)
		return ret;

	string instvPath(path);

	instvPath += "/";
	instvPath += "instv.dat";

	string tempDir = gameAddcontInfoTempDir(instvPath);
	GameAddcontInstv instv;

	if (LocalFile::Exists(instvPath.c_str())) {
		common::SharedPtr<File> file = LocalFile::Open(instvPath.c_str(),
							       SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

		if (ret < 0)
			return GAME_ADDCONT_INFO_ERROR_IO;

		sce_paf_memset(&instv, 0, sizeof(instv));
		if (file->Read(&instv, sizeof(instv)) != sizeof(instv)) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
		if (file.get() != NULL)
			file->Close();

		if (sce_paf_strncmp(instv.magic, GAME_ADDCONT_INSTV_MAGIC, 4) == 0) {
			if (instv.version == GAME_ADDCONT_INSTV_VERSION)
				return 0;

			ret = LocalFile::RemoveFile(instvPath.c_str());
			if (ret < 0)
				return GAME_ADDCONT_INFO_ERROR_IO;
		} else {
			return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
		}
	}

	if (!LocalFile::Exists(tempDir.c_str())) {
		ret = Dir::CreateRecursive(tempDir.c_str());
		if (ret < 0)
			return GAME_ADDCONT_INFO_ERROR_IO;
	}

	string tempPath(tempDir);

	tempPath += "/";
	tempPath += "addcont_instv.dat";

	common::SharedPtr<File> file =
		LocalFile::Open(tempPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	sce_paf_memset(&instv, 0, sizeof(instv));
	sce_paf_memcpy(instv.magic, GAME_ADDCONT_INSTV_MAGIC, 4);
	instv.version = GAME_ADDCONT_INSTV_VERSION;
	instv.count = 0;
	if (file->Write(&instv, sizeof(instv)) != sizeof(instv)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}
	if (file.get() != NULL)
		file->Close();

	ret = LocalFile::RenameFile(tempPath.c_str(), instvPath.c_str());
	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;
	return 0;
}

/*
 * info written into the addcont/info folder at path: its instv.dat copied to the temp folder with
 * the license of info's content ID replaced, or info added after the others, then moved back
 * (0x81099174).
 */
int gameAddcontInfoWrite(const char *path, const GameAddcontLicenseInfo *info)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);
	int ret = 0;

	if (path == NULL || info == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	bool grw0 = sce_paf_strncmp(path, "grw0:addcont/info", 18) == 0;

	if (sce_paf_strncmp(path, "ux0:addcont/info", 17) == 0 || grw0)
		ret = 0;
	else
		ret = GAME_ADDCONT_INFO_ERROR_INVALID_ARG;
	if (ret < 0)
		return ret;

	string instvPath(path);

	instvPath += "/";
	instvPath += "instv.dat";

	if (!LocalFile::Exists(instvPath.c_str()))
		return GAME_ADDCONT_INFO_ERROR_NO_INSTV;

	common::SharedPtr<File> file =
		LocalFile::Open(instvPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv instv;

	sce_paf_memset(&instv, 0, sizeof(instv));
	if (file->Read(&instv, sizeof(instv)) != sizeof(instv)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (sce_paf_strncmp(instv.magic, GAME_ADDCONT_INSTV_MAGIC, 4) != 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
	}

	if (instv.version < GAME_ADDCONT_INSTV_VERSION) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_OLD_INSTV;
	}

	string tempDir = gameAddcontInfoTempDir(instvPath);

	if (!LocalFile::Exists(tempDir.c_str())) {
		ret = Dir::CreateRecursive(tempDir.c_str());
		if (ret < 0)
			return GAME_ADDCONT_INFO_ERROR_IO;
	}

	string tempPath(tempDir);

	tempPath += "/";
	tempPath += "addcont_instv.dat";

	if (LocalFile::Exists(tempPath.c_str())) {
		ret = LocalFile::RemoveFile(tempPath.c_str());
		if (ret < 0) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
	}

	common::SharedPtr<File> temp =
		LocalFile::Open(tempPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv header;

	sce_paf_memset(&header, 0, sizeof(header));
	sce_paf_memcpy(header.magic, GAME_ADDCONT_INSTV_MAGIC, 4);
	header.version = GAME_ADDCONT_INSTV_VERSION;
	header.count = instv.count + 1;
	if (temp->Write(&header, sizeof(header)) != sizeof(header)) {
		if (file.get() != NULL)
			file->Close();
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	bool found = false;

	for (SceUInt32 i = 0; i < instv.count; i++) {
		char entry[sizeof(GameAddcontLicenseInfo)];

		sce_paf_memset(entry, 0, sizeof(entry));
		if (file->Read(entry, sizeof(entry)) != sizeof(entry)) {
			if (file.get() != NULL)
				file->Close();
			if (temp.get() != NULL)
				temp->Close();
			ret = LocalFile::RemoveFile(tempPath.c_str());
			return GAME_ADDCONT_INFO_ERROR_IO;
		}

		if (sce_paf_strncmp(entry, info->contentId, sizeof(info->contentId)) != 0) {
			if (temp->Write(entry, sizeof(entry)) != sizeof(entry)) {
				if (file.get() != NULL)
					file->Close();
				if (temp.get() != NULL)
					temp->Close();
				ret = LocalFile::RemoveFile(tempPath.c_str());
				return GAME_ADDCONT_INFO_ERROR_IO;
			}
		} else {
			found = true;
			if (temp->Write(info, sizeof(*info)) != sizeof(*info)) {
				if (file.get() != NULL)
					file->Close();
				if (temp.get() != NULL)
					temp->Close();
				ret = LocalFile::RemoveFile(tempPath.c_str());
				return GAME_ADDCONT_INFO_ERROR_IO;
			}
		}
	}

	if (file.get() != NULL)
		file->Close();

	if (found) {
		if (temp->Seek(0, SCE_SEEK_SET) < 0) {
			if (temp.get() != NULL)
				temp->Close();
			ret = LocalFile::RemoveFile(tempPath.c_str());
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
		header.count--;
		if (temp->Write(&header, sizeof(header)) != sizeof(header)) {
			if (temp.get() != NULL)
				temp->Close();
			ret = LocalFile::RemoveFile(tempPath.c_str());
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
	} else if (temp->Write(info, sizeof(*info)) != sizeof(*info)) {
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (temp.get() != NULL)
		temp->Close();

	ret = LocalFile::RemoveFile(instvPath.c_str());
	if (ret < 0) {
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	ret = LocalFile::RenameFile(tempPath.c_str(), instvPath.c_str());
	if (ret < 0)
		return ret;
	return 0;
}

/*
 * The license of contentId removed from the addcont/info folder at path: its instv.dat copied to
 * the temp folder without it and moved back. With wholeId clear only the entitlement part of each
 * content ID (its 9 characters from the 7th) is compared (0x8109A558).
 */
static __attribute__((noinline)) int gameAddcontInfoRemoveEntry(const char *path,
								 const char *contentId,
								 bool wholeId)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);
	int ret = 0;

	if (path == NULL || contentId == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	bool grw0 = sce_paf_strncmp(path, "grw0:addcont/info", 18) == 0;

	if (sce_paf_strncmp(path, "ux0:addcont/info", 17) == 0 || grw0)
		ret = 0;
	else
		ret = GAME_ADDCONT_INFO_ERROR_INVALID_ARG;
	if (ret < 0)
		return ret;

	string instvPath(path);

	instvPath += "/";
	instvPath += "instv.dat";

	if (!LocalFile::Exists(instvPath.c_str()))
		return GAME_ADDCONT_INFO_ERROR_NO_INSTV;

	common::SharedPtr<File> file =
		LocalFile::Open(instvPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv instv;

	sce_paf_memset(&instv, 0, sizeof(instv));
	if (file->Read(&instv, sizeof(instv)) != sizeof(instv)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (sce_paf_strncmp(instv.magic, GAME_ADDCONT_INSTV_MAGIC, 4) != 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
	}

	if (instv.version < GAME_ADDCONT_INSTV_VERSION) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_OLD_INSTV;
	}

	string tempDir = gameAddcontInfoTempDir(instvPath);

	if (!LocalFile::Exists(tempDir.c_str())) {
		ret = Dir::CreateRecursive(tempDir.c_str());
		if (ret < 0)
			return GAME_ADDCONT_INFO_ERROR_IO;
	}

	string tempPath(tempDir);

	tempPath += "/";
	tempPath += "addcont_instv.dat";

	if (LocalFile::Exists(tempPath.c_str())) {
		ret = LocalFile::RemoveFile(tempPath.c_str());
		if (ret < 0) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
	}

	common::SharedPtr<File> temp =
		LocalFile::Open(tempPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	GameAddcontInstv header;

	sce_paf_memset(&header, 0, sizeof(header));
	sce_paf_memcpy(header.magic, GAME_ADDCONT_INSTV_MAGIC, 4);
	header.version = GAME_ADDCONT_INSTV_VERSION;
	header.count = 0;
	if (temp->Write(&header, sizeof(header)) != sizeof(header)) {
		if (file.get() != NULL)
			file->Close();
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	bool found = false;
	SceUInt32 count = 0;

	for (SceUInt32 i = 0; i < instv.count; i++) {
		char entry[sizeof(GameAddcontLicenseInfo)];

		sce_paf_memset(entry, 0, sizeof(entry));
		if (file->Read(entry, sizeof(entry)) != sizeof(entry)) {
			if (file.get() != NULL)
				file->Close();
			if (temp.get() != NULL)
				temp->Close();
			ret = LocalFile::RemoveFile(tempPath.c_str());
			return GAME_ADDCONT_INFO_ERROR_IO;
		}

		string entryId(entry);
		bool match;

		if (wholeId)
			match = entryId == string(contentId);
		else
			match = entryId.substr(7, 9) == string(contentId);

		if (match) {
			found = true;
		} else {
			if (temp->Write(entry, sizeof(entry)) != sizeof(entry)) {
				if (file.get() != NULL)
					file->Close();
				if (temp.get() != NULL)
					temp->Close();
				ret = LocalFile::RemoveFile(tempPath.c_str());
				return GAME_ADDCONT_INFO_ERROR_IO;
			}
			count++;
		}
	}

	if (file.get() != NULL) {
		file->Close();
		file.reset();
	}

	if (!found) {
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_NOT_FOUND;
	}

	if (temp->Seek(0, SCE_SEEK_SET) < 0) {
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}
	instv.count = count;
	if (temp->Write(&instv, sizeof(instv)) != sizeof(instv)) {
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (temp.get() != NULL)
		temp->Close();

	ret = LocalFile::RemoveFile(instvPath.c_str());
	if (ret < 0) {
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	ret = LocalFile::RenameFile(tempPath.c_str(), instvPath.c_str());
	return 0;
}

/*
 * Whether the addcont/info folder at path has a license of one of dirs, the additional contents'
 * folders, matched with the entitlement part of each content ID (0x8109B7A6).
 */
int gameAddcontInfoCheck(const char *path, vector<string> *dirs, bool *found)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);

	*found = false;

	int ret = 0;

	if (path == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	string instvPath(path);

	instvPath += "/";
	instvPath += "instv.dat";

	if (!LocalFile::Exists(instvPath.c_str()))
		return GAME_ADDCONT_INFO_ERROR_NO_INSTV;

	common::SharedPtr<File> file =
		LocalFile::Open(instvPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv instv;

	sce_paf_memset(&instv, 0, sizeof(instv));
	if (file->Read(&instv, sizeof(instv)) != sizeof(instv)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (sce_paf_strncmp(instv.magic, GAME_ADDCONT_INSTV_MAGIC, 4) != 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
	}

	if (instv.version < GAME_ADDCONT_INSTV_VERSION) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_OLD_INSTV;
	}

	for (SceUInt32 i = 0; i < instv.count; i++) {
		char entry[sizeof(GameAddcontLicenseInfo)];

		sce_paf_memset(entry, 0, sizeof(entry));
		if (file->Read(entry, sizeof(entry)) != sizeof(entry)) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}

		string entryId(entry);
		string id = entryId.substr(7, 9);

		for (SceUInt32 j = 0; j < dirs->size(); j++) {
			if ((*dirs)[j] == id) {
				*found = true;
				break;
			}
		}
		if (*found)
			break;
	}

	if (file.get() != NULL)
		file->Close();
	return 0;
}

/*
 * The licenses in the addcont/info folder at path of the additional contents in dirs, matched
 * with the entitlement part of each content ID, into infos (0x8109BD1C).
 */
int gameAddcontInfoRead(const char *path, const vector<string> *dirs,
			vector<GameAddcontLicenseInfo> *infos)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);
	int ret = 0;

	infos->clear();

	if (path == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	string instvPath(path);

	instvPath += "/";
	instvPath += "instv.dat";

	if (!LocalFile::Exists(instvPath.c_str()))
		return GAME_ADDCONT_INFO_ERROR_NO_INSTV;

	common::SharedPtr<File> file =
		LocalFile::Open(instvPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv instv;

	sce_paf_memset(&instv, 0, sizeof(instv));
	if (file->Read(&instv, sizeof(instv)) != sizeof(instv)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (sce_paf_strncmp(instv.magic, GAME_ADDCONT_INSTV_MAGIC, 4) != 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
	}

	if (instv.version < GAME_ADDCONT_INSTV_VERSION) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_OLD_INSTV;
	}

	for (SceUInt32 i = 0; i < instv.count; i++) {
		GameAddcontLicenseInfo info;

		sce_paf_memset(&info, 0, sizeof(info));
		if (file->Read(&info, sizeof(info)) != sizeof(info)) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}

		string entryId(info.contentId);
		string id = entryId.substr(7, 9);

		for (SceUInt32 j = 0; j < dirs->size(); j++) {
			if ((*dirs)[j] == id)
				infos->push_back(info);
		}
	}

	if (file.get() != NULL)
		file->Close();
	return 0;
}

/* the license of contentId removed from the addcont/info folder at path (0x8109C328) */
int gameAddcontInfoRemove(const char *path, const char *contentId)
{
	return gameAddcontInfoRemoveEntry(path, contentId, true);
}

/*
 * A PSP application's addcont/info folder at path made ready, as gameAddcontInfoPrepare does
 * with instp.dat (0x8109C332).
 */
int gamePspAddcontInfoPrepare(const char *path)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);
	int ret = 0;

	if (path == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	if (sce_paf_strncmp(path, "ux0:addcont/info", 17) == 0)
		ret = 0;
	else
		ret = GAME_ADDCONT_INFO_ERROR_INVALID_ARG;
	if (ret < 0)
		return ret;

	string instpPath(path);

	instpPath += "/";
	instpPath += "instp.dat";

	string tempDir = gameAddcontInfoTempDir(instpPath);
	GameAddcontInstv instp;

	if (LocalFile::Exists(instpPath.c_str())) {
		common::SharedPtr<File> file = LocalFile::Open(instpPath.c_str(),
							       SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

		if (ret < 0)
			return GAME_ADDCONT_INFO_ERROR_IO;

		sce_paf_memset(&instp, 0, sizeof(instp));
		if (file->Read(&instp, sizeof(instp)) != sizeof(instp)) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
		if (file.get() != NULL)
			file->Close();

		if (sce_paf_strncmp(instp.magic, GAME_ADDCONT_INSTP_MAGIC, 4) == 0) {
			if (instp.version == GAME_ADDCONT_INSTV_VERSION)
				return 0;

			ret = LocalFile::RemoveFile(instpPath.c_str());
			if (ret < 0)
				return GAME_ADDCONT_INFO_ERROR_IO;
		} else {
			return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
		}
	}

	if (!LocalFile::Exists(tempDir.c_str())) {
		ret = Dir::CreateRecursive(tempDir.c_str());
		if (ret < 0)
			return GAME_ADDCONT_INFO_ERROR_IO;
	}

	string tempPath(tempDir);

	tempPath += "/";
	tempPath += "addcont_instp.dat";

	common::SharedPtr<File> file =
		LocalFile::Open(tempPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	sce_paf_memset(&instp, 0, sizeof(instp));
	sce_paf_memcpy(instp.magic, GAME_ADDCONT_INSTP_MAGIC, 4);
	instp.version = GAME_ADDCONT_INSTV_VERSION;
	instp.count = 0;
	if (file->Write(&instp, sizeof(instp)) != sizeof(instp)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}
	if (file.get() != NULL)
		file->Close();

	ret = LocalFile::RenameFile(tempPath.c_str(), instpPath.c_str());
	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;
	return 0;
}

/*
 * info written into a PSP application's addcont/info folder at path, as gameAddcontInfoWrite
 * does with instp.dat (0x8109CCEC).
 */
int gamePspAddcontInfoWrite(const char *path, const GameAddcontLicenseInfo *info)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);
	int ret = 0;

	if (path == NULL || info == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	if (sce_paf_strncmp(path, "ux0:addcont/info", 17) == 0)
		ret = 0;
	else
		ret = GAME_ADDCONT_INFO_ERROR_INVALID_ARG;
	if (ret < 0)
		return ret;

	string instpPath(path);

	instpPath += "/";
	instpPath += "instp.dat";

	if (!LocalFile::Exists(instpPath.c_str()))
		return GAME_ADDCONT_INFO_ERROR_NO_INSTV;

	common::SharedPtr<File> file =
		LocalFile::Open(instpPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv instp;

	sce_paf_memset(&instp, 0, sizeof(instp));
	if (file->Read(&instp, sizeof(instp)) != sizeof(instp)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (sce_paf_strncmp(instp.magic, GAME_ADDCONT_INSTP_MAGIC, 4) != 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
	}

	if (instp.version < GAME_ADDCONT_INSTV_VERSION) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_OLD_INSTV;
	}

	string tempDir = gameAddcontInfoTempDir(instpPath);

	if (!LocalFile::Exists(tempDir.c_str())) {
		ret = Dir::CreateRecursive(tempDir.c_str());
		if (ret < 0)
			return GAME_ADDCONT_INFO_ERROR_IO;
	}

	string tempPath(tempDir);

	tempPath += "/";
	tempPath += "addcont_instp.dat";

	if (LocalFile::Exists(tempPath.c_str())) {
		ret = LocalFile::RemoveFile(tempPath.c_str());
		if (ret < 0) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
	}

	common::SharedPtr<File> temp =
		LocalFile::Open(tempPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv header;

	sce_paf_memset(&header, 0, sizeof(header));
	sce_paf_memcpy(header.magic, GAME_ADDCONT_INSTP_MAGIC, 4);
	header.version = GAME_ADDCONT_INSTV_VERSION;
	header.count = instp.count + 1;
	if (temp->Write(&header, sizeof(header)) != sizeof(header)) {
		if (file.get() != NULL)
			file->Close();
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	bool found = false;

	for (SceUInt32 i = 0; i < instp.count; i++) {
		char entry[sizeof(GameAddcontLicenseInfo)];

		sce_paf_memset(entry, 0, sizeof(entry));
		if (file->Read(entry, sizeof(entry)) != sizeof(entry)) {
			if (file.get() != NULL)
				file->Close();
			if (temp.get() != NULL)
				temp->Close();
			ret = LocalFile::RemoveFile(tempPath.c_str());
			return GAME_ADDCONT_INFO_ERROR_IO;
		}

		if (sce_paf_strncmp(entry, info->contentId, sizeof(info->contentId)) != 0) {
			if (temp->Write(entry, sizeof(entry)) != sizeof(entry)) {
				if (file.get() != NULL)
					file->Close();
				if (temp.get() != NULL)
					temp->Close();
				ret = LocalFile::RemoveFile(tempPath.c_str());
				return GAME_ADDCONT_INFO_ERROR_IO;
			}
		} else {
			found = true;
			if (temp->Write(info, sizeof(*info)) != sizeof(*info)) {
				if (file.get() != NULL)
					file->Close();
				if (temp.get() != NULL)
					temp->Close();
				ret = LocalFile::RemoveFile(tempPath.c_str());
				return GAME_ADDCONT_INFO_ERROR_IO;
			}
		}
	}

	if (file.get() != NULL)
		file->Close();

	if (found) {
		if (temp->Seek(0, SCE_SEEK_SET) < 0) {
			if (temp.get() != NULL)
				temp->Close();
			ret = LocalFile::RemoveFile(tempPath.c_str());
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
		header.count--;
		if (temp->Write(&header, sizeof(header)) != sizeof(header)) {
			if (temp.get() != NULL)
				temp->Close();
			ret = LocalFile::RemoveFile(tempPath.c_str());
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
	} else if (temp->Write(info, sizeof(*info)) != sizeof(*info)) {
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (temp.get() != NULL)
		temp->Close();

	ret = LocalFile::RemoveFile(instpPath.c_str());
	if (ret < 0) {
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	ret = LocalFile::RenameFile(tempPath.c_str(), instpPath.c_str());
	if (ret < 0)
		return ret;
	return 0;
}

/*
 * The license of contentId removed from a PSP application's addcont/info folder at path, as
 * gameAddcontInfoRemoveEntry does with instp.dat (0x8109E0B2).
 */
static __attribute__((noinline)) int gamePspAddcontInfoRemoveEntry(const char *path,
								    const char *contentId,
								    bool wholeId)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);
	int ret = 0;

	if (path == NULL || contentId == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	if (sce_paf_strncmp(path, "ux0:addcont/info", 17) == 0)
		ret = 0;
	else
		ret = GAME_ADDCONT_INFO_ERROR_INVALID_ARG;
	if (ret < 0)
		return ret;

	string instpPath(path);

	instpPath += "/";
	instpPath += "instp.dat";

	if (!LocalFile::Exists(instpPath.c_str()))
		return GAME_ADDCONT_INFO_ERROR_NO_INSTV;

	common::SharedPtr<File> file =
		LocalFile::Open(instpPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv instp;

	sce_paf_memset(&instp, 0, sizeof(instp));
	if (file->Read(&instp, sizeof(instp)) != sizeof(instp)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (sce_paf_strncmp(instp.magic, GAME_ADDCONT_INSTP_MAGIC, 4) != 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
	}

	if (instp.version < GAME_ADDCONT_INSTV_VERSION) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_OLD_INSTV;
	}

	string tempDir = gameAddcontInfoTempDir(instpPath);

	if (!LocalFile::Exists(tempDir.c_str())) {
		ret = Dir::CreateRecursive(tempDir.c_str());
		if (ret < 0)
			return GAME_ADDCONT_INFO_ERROR_IO;
	}

	string tempPath(tempDir);

	tempPath += "/";
	tempPath += "addcont_instp.dat";

	if (LocalFile::Exists(tempPath.c_str())) {
		ret = LocalFile::RemoveFile(tempPath.c_str());
		if (ret < 0) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}
	}

	common::SharedPtr<File> temp =
		LocalFile::Open(tempPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	GameAddcontInstv header;

	sce_paf_memset(&header, 0, sizeof(header));
	sce_paf_memcpy(header.magic, GAME_ADDCONT_INSTP_MAGIC, 4);
	header.version = GAME_ADDCONT_INSTV_VERSION;
	header.count = 0;
	if (temp->Write(&header, sizeof(header)) != sizeof(header)) {
		if (file.get() != NULL)
			file->Close();
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	bool found = false;
	SceUInt32 count = 0;

	for (SceUInt32 i = 0; i < instp.count; i++) {
		char entry[sizeof(GameAddcontLicenseInfo)];

		sce_paf_memset(entry, 0, sizeof(entry));
		if (file->Read(entry, sizeof(entry)) != sizeof(entry)) {
			if (file.get() != NULL)
				file->Close();
			if (temp.get() != NULL)
				temp->Close();
			ret = LocalFile::RemoveFile(tempPath.c_str());
			return GAME_ADDCONT_INFO_ERROR_IO;
		}

		string entryId(entry);
		bool match;

		if (wholeId)
			match = entryId == string(contentId);
		else
			match = entryId.substr(7, 9) == string(contentId);

		if (match) {
			found = true;
		} else {
			if (temp->Write(entry, sizeof(entry)) != sizeof(entry)) {
				if (file.get() != NULL)
					file->Close();
				if (temp.get() != NULL)
					temp->Close();
				ret = LocalFile::RemoveFile(tempPath.c_str());
				return GAME_ADDCONT_INFO_ERROR_IO;
			}
			count++;
		}
	}

	if (file.get() != NULL) {
		file->Close();
		file.reset();
	}

	if (!found) {
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_NOT_FOUND;
	}

	if (temp->Seek(0, SCE_SEEK_SET) < 0) {
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}
	instp.count = count;
	if (temp->Write(&instp, sizeof(instp)) != sizeof(instp)) {
		if (temp.get() != NULL)
			temp->Close();
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (temp.get() != NULL)
		temp->Close();

	ret = LocalFile::RemoveFile(instpPath.c_str());
	if (ret < 0) {
		ret = LocalFile::RemoveFile(tempPath.c_str());
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	ret = LocalFile::RenameFile(tempPath.c_str(), instpPath.c_str());
	return 0;
}

/*
 * Whether a PSP application's addcont/info folder at path has a license of one of dirs, as
 * gameAddcontInfoCheck does with instp.dat (0x8109F2E4).
 */
int gamePspAddcontInfoCheck(const char *path, vector<string> *dirs, bool *found)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);

	*found = false;

	int ret = 0;

	if (path == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	string instpPath(path);

	instpPath += "/";
	instpPath += "instp.dat";

	if (!LocalFile::Exists(instpPath.c_str()))
		return GAME_ADDCONT_INFO_ERROR_NO_INSTV;

	common::SharedPtr<File> file =
		LocalFile::Open(instpPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv instp;

	sce_paf_memset(&instp, 0, sizeof(instp));
	if (file->Read(&instp, sizeof(instp)) != sizeof(instp)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (sce_paf_strncmp(instp.magic, GAME_ADDCONT_INSTP_MAGIC, 4) != 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
	}

	if (instp.version < GAME_ADDCONT_INSTV_VERSION) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_OLD_INSTV;
	}

	for (SceUInt32 i = 0; i < instp.count; i++) {
		char entry[sizeof(GameAddcontLicenseInfo)];

		sce_paf_memset(entry, 0, sizeof(entry));
		if (file->Read(entry, sizeof(entry)) != sizeof(entry)) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}

		string entryId(entry);
		string id = entryId.substr(7, 9);

		for (SceUInt32 j = 0; j < dirs->size(); j++) {
			if ((*dirs)[j] == id) {
				*found = true;
				break;
			}
		}
		if (*found)
			break;
	}

	if (file.get() != NULL)
		file->Close();
	return 0;
}

/*
 * The licenses in a PSP application's addcont/info folder at path of the additional contents in
 * dirs, as gameAddcontInfoRead does with instp.dat (0x8109F85A).
 */
int gamePspAddcontInfoRead(const char *path, const vector<string> *dirs,
			   vector<GameAddcontLicenseInfo> *infos)
{
	thread::RMutexLock lock(&s_addcontInfoMutex);
	int ret = 0;

	infos->clear();

	if (path == NULL)
		return GAME_ADDCONT_INFO_ERROR_INVALID_ARG;

	string instpPath(path);

	instpPath += "/";
	instpPath += "instp.dat";

	if (!LocalFile::Exists(instpPath.c_str()))
		return GAME_ADDCONT_INFO_ERROR_NO_INSTV;

	common::SharedPtr<File> file =
		LocalFile::Open(instpPath.c_str(), SCE_O_RDWR | SCE_O_CREAT, 6, &ret);

	if (ret < 0)
		return GAME_ADDCONT_INFO_ERROR_IO;

	GameAddcontInstv instp;

	sce_paf_memset(&instp, 0, sizeof(instp));
	if (file->Read(&instp, sizeof(instp)) != sizeof(instp)) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_IO;
	}

	if (sce_paf_strncmp(instp.magic, GAME_ADDCONT_INSTP_MAGIC, 4) != 0) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_BAD_DATA;
	}

	if (instp.version < GAME_ADDCONT_INSTV_VERSION) {
		if (file.get() != NULL)
			file->Close();
		return GAME_ADDCONT_INFO_ERROR_OLD_INSTV;
	}

	for (SceUInt32 i = 0; i < instp.count; i++) {
		GameAddcontLicenseInfo info;

		sce_paf_memset(&info, 0, sizeof(info));
		if (file->Read(&info, sizeof(info)) != sizeof(info)) {
			if (file.get() != NULL)
				file->Close();
			return GAME_ADDCONT_INFO_ERROR_IO;
		}

		string entryId(info.contentId);
		string id = entryId.substr(7, 9);

		for (SceUInt32 j = 0; j < dirs->size(); j++) {
			if ((*dirs)[j] == id)
				infos->push_back(info);
		}
	}

	if (file.get() != NULL)
		file->Close();
	return 0;
}

/* the license of contentId removed from a PSP application's addcont/info folder (0x8109FE66) */
int gamePspAddcontInfoRemove(const char *path, const char *contentId)
{
	return gamePspAddcontInfoRemoveEntry(path, contentId, true);
}
