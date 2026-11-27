/*
 * A PSP application's EBOOT.PBP signature (__sce_ebootpbp): made by NpDrm for an installation,
 * from the EBOOT's hash or, for a multi-disc PlayStation game, its disc list, and checked
 * (0x81031C1C..0x810329E0). The file's name and the functions' are ours (no record names them);
 * the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the SDK's glue over SceVshBridge's imports (0x81417D28..0x81417D74) */
extern "C" {
int vshNpDrmEbootSigGenPsp(const char *ebootPath, const void *sha256, void *signature,
			   int swVersion);
int vshNpDrmEbootSigGenPs1(const char *ebootPath, const void *sha256, void *signature,
			   int swVersion);
int vshNpDrmEbootSigGenMultiDisc(const char *ebootPath, const void *discInfo, void *signature,
				 int swVersion);
int vshNpDrmEbootSigVerify(const char *ebootPath, const void *signature);
int vshNpDrmPspEbootVerify(const char *ebootPath, const void *signature);
}

/* an EBOOT's signature, and the old PSP one */
struct GameEbootSignature {
	SceUInt8 data[512];
};

struct GamePspEbootSignature {
	SceUInt8 data[256];
};

/* the disc list the PSP emulator carries, for a game without its own */
#define GAME_PSPEMU_DISCINFO	"vs0:app/NPXS10028/__sce_discinfo"

/*
 * The signature of a multi-disc PlayStation game, from the __sce_discinfo beside its EBOOT, or
 * from the PSP emulator's list without one (0x81031C1C).
 */
static __attribute__((noinline)) int gameEbootSigGenMultiDisc(const SceNpDrmInstallInfo *info,
							       const string &ebootPath,
							       void *signature, string *detail)
{
	int ret = 0;
	string::size_type pos = ebootPath.rfind("/", string::npos, sce_paf_strlen("/"));

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	string discInfoPath = ebootPath.substr(0, pos) + string("/__sce_discinfo");
	LocalFileStat stat;

	ret = LocalFile::Getstat(discInfoPath.c_str(), &stat);
	if (ret == 0) {
		GameDiscInfo *discInfo = new GameDiscInfo;

		if (discInfo == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		common::SharedPtr<File> file = LocalFile::Open(discInfoPath.c_str(), SCE_O_RDONLY, 0,
							       &ret);

		if (ret < 0) {
			gamePathGetMountedDrive(discInfoPath, detail);
			delete discInfo;
			return ret;
		}

		ret = file->Read(discInfo, sizeof(*discInfo));
		if (ret < 0) {
			gamePathGetMountedDrive(discInfoPath, detail);
			file->Close();
			delete discInfo;
			return ret;
		}

		file->Close();

		ret = vshNpDrmEbootSigGenMultiDisc(ebootPath.c_str(), discInfo, signature,
						   info->swVersion);
		if (ret < 0) {
			delete discInfo;
			return ret;
		}

		delete discInfo;
	} else {
		GamePs1DiscInfoParser *parser = new GamePs1DiscInfoParser();

		if (parser == NULL)
			return GAME_ERROR_NO_INFO_MEMORY;

		ret = parser->Load(string(GAME_PSPEMU_DISCINFO));
		if (ret < 0) {
			delete parser;
			return ret;
		}

		ret = parser->Generate(info, ebootPath, signature);
		if (ret < 0) {
			delete parser;
			return ret;
		}

		delete parser;
	}

	return 0;
}

/*
 * The signature of the installation's EBOOT made, from its hash (a PlayStation game's PS1
 * signature, or its disc list's), and written to sigPath (0x810321A4).
 */
int gameEbootSigGen(const SceNpDrmInstallInfo *info, const string &ebootPath, const void *sha256,
		    const string &sigPath, string *detail)
{
	int ret = 0;
	GameEbootSignature *signature = new GameEbootSignature;

	if (signature == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	common::SharedPtr<File> file;

	if (info->info.contentType == GAME_CONTENT_TYPE_PS1) {
		bool multiDisc = false;
		GamePbpHeader header;

		ret = gamePbpReadHeader(ebootPath.c_str(), &header, &multiDisc);
		if (ret < 0) {
			gamePathGetMountedDrive(ebootPath, detail);
		} else if (multiDisc) {
			ret = gameEbootSigGenMultiDisc(info, ebootPath, signature, detail);
		} else {
			ret = vshNpDrmEbootSigGenPs1(ebootPath.c_str(), sha256, signature,
						     info->swVersion);
		}
	} else {
		ret = vshNpDrmEbootSigGenPsp(ebootPath.c_str(), sha256, signature, info->swVersion);
	}

	if (ret >= 0) {
		file = LocalFile::Open(sigPath.c_str(), SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6,
				       &ret);
		if (ret < 0) {
			gamePathGetMountedDrive(sigPath, detail);
		} else {
			int size = file->Write(signature, sizeof(*signature));

			if (size < 0) {
				ret = size;
				gamePathGetMountedDrive(sigPath, detail);
			} else {
				ret = 0;
			}
		}

		if (file.get() != NULL)
			file->Close();
	}

	delete signature;

	return ret;
}

/* The old PSP signature at sigPath checked against the EBOOT (0x81032394). */
static __attribute__((noinline)) int gamePspEbootVerify(const string &ebootPath,
							 const string &sigPath, string *detail)
{
	int ret = 0;
	GamePspEbootSignature *signature = new GamePspEbootSignature;

	if (signature == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	common::SharedPtr<File> file = LocalFile::Open(sigPath.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret < 0) {
		gamePathGetMountedDrive(sigPath, detail);
	} else {
		ret = file->Read(signature, sizeof(*signature));
		if (ret < 0) {
			gamePathGetMountedDrive(sigPath, detail);
		} else {
			file->Close();
			ret = vshNpDrmPspEbootVerify(ebootPath.c_str(), signature);
			if (ret >= 0)
				ret = 0;
		}
	}

	if (file.get() != NULL)
		file->Close();

	delete signature;

	return ret;
}

/*
 * The signature at sigPath checked against the EBOOT: the old PSP one (256 bytes) only where
 * allowPsp is set (0x8103249E).
 */
static int gameEbootSigVerify(const string &ebootPath, const string &sigPath, bool allowPsp,
			      string *detail)
{
	LocalFileStat stat;
	int ret = LocalFile::Getstat(sigPath.c_str(), &stat);

	if (ret < 0)
		return ret;

	if (allowPsp) {
		if ((SceUInt32)stat.st_size == sizeof(GamePspEbootSignature)) {
			ret = gamePspEbootVerify(ebootPath, sigPath, detail);
			if (ret >= 0)
				return 0;
			return ret;
		}
	} else if ((SceUInt32)stat.st_size == sizeof(GamePspEbootSignature)) {
		return GAME_ERROR_OLD_EBOOT_SIG;
	}

	int err = 0;
	GameEbootSignature *signature = new GameEbootSignature;

	if (signature == NULL) {
		err = GAME_ERROR_NO_INFO_MEMORY;
	} else {
		common::SharedPtr<File> file = LocalFile::Open(sigPath.c_str(), SCE_O_RDONLY, 0,
							       &err);

		if (err < 0) {
			gamePathGetMountedDrive(sigPath, detail);
		} else {
			err = file->Read(signature, sizeof(*signature));
			if (err < 0) {
				gamePathGetMountedDrive(sigPath, detail);
			} else {
				file->Close();
				err = vshNpDrmEbootSigVerify(ebootPath.c_str(), signature);
				if (err >= 0)
					err = 0;
			}
		}

		if (file.get() != NULL)
			file->Close();

		delete signature;
	}

	if (err >= 0)
		return 0;

	return err;
}

/*
 * The EBOOT.PBP of the PSP application in dir checked against its __sce_ebootpbp, the PSP
 * emulator's work folder mounted (0x810326AA).
 */
int gameEbootCheck(const char *dir, string *detail)
{
	string ebootPath = dir + string("/EBOOT.PBP");
	string sigPath = dir + string("/__sce_ebootpbp");
	string mountPoint;
	int ret = gamePspemuWorkMount(&mountPoint);

	if (ret < 0)
		return ret;

	ret = gameEbootSigVerify(ebootPath, sigPath, false, detail);
	if (ret >= 0)
		ret = 0;

	gamePspemuWorkUnmount(&mountPoint);

	return ret;
}

/* The signature at sigPath checked against the EBOOT (0x81032952). */
int gameEbootSigCheck(const string &titleId, const string &ebootPath, const string &sigPath,
		      bool allowPsp, string *detail)
{
	int ret = gameEbootSigVerify(ebootPath, sigPath, allowPsp, detail);

	if (ret < 0)
		return ret;

	return 0;
}
