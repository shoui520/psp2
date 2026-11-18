/*
 * A SELF's eboot parameters: read from its control information, and kept in a saved copy whose
 * check value is an HMAC-SHA256 under a key the secure processor decrypts (0x81032FD4..
 * 0x8103390C). The file's name and the functions' are ours (no record names them); the strings
 * are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <ss_mgr/ss_mgr.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the SDK's libhmac, linked in (target/include/libhmac.h; 0x8137E058) */
extern "C" SceInt32 sceHmacSha256Digest(const void *secretkey, SceUInt32 keylen,
					const void *plain, SceUInt32 datalen, SceUChar8 *digest);

/* the SDK's glue over SceVshBridge's import (0x81417D20) */
extern "C" int vshSblSsDecryptWithPortability(SceUInt32 keyType, void *iv,
					      ScePortabilityData *src, ScePortabilityData *dst);

/* the key type the check value's keys are decrypted with */
#define GAME_EBOOT_PARAM_KEY_TYPE	7

/* the size of the check value's key, and of its text */
#define GAME_EBOOT_PARAM_KEY_SIZE	0x20
#define GAME_EBOOT_PARAM_MAC_SIZE	0x20

/* what a decrypted key starts with */
#define GAME_EBOOT_PARAM_KEY_TAG	"EBP"

/* the digits a check value is written in */
#define GAME_HEX_DIGITS		"0123456789abcdef"

/* a SELF's control information of its eboot parameters */
#define GAME_SELF_CONTROL_INFO_EBOOT_PARAM	6

/* the start of a SELF's SCE header (the fields read) */
struct GameSelfHeader {
	SceUInt8 magic[4];
	SceUInt32 version;
	SceUInt16 keyRevision;
	SceUInt16 headerType;
	SceUInt32 metadataOffset;
	SceUInt64 headerLength;
	SceUInt64 dataLength;
	SceUInt8 reserved[0x10];
};

/* a SELF's extended header: where its parts are */
struct GameSelfExtHeader {
	SceUInt64 selfOffset;
	SceUInt64 appInfoOffset;
	SceUInt64 elfOffset;
	SceUInt64 phdrOffset;
	SceUInt64 shdrOffset;
	SceUInt64 sectionInfoOffset;
	SceUInt64 sceVersionOffset;
	SceUInt64 controlInfoOffset;
	SceUInt64 controlInfoSize;
	SceUInt64 padding;
};

/* a SELF's control information's header */
struct GameSelfControlInfo {
	SceUInt32 type;
	SceUInt32 size;
	SceUInt64 next;
};

/* the control information of the eboot parameters */
struct GameSelfEbootParamInfo {
	GameSelfControlInfo common;
	SceUInt32 header;
	SceUInt8 params[0xFC];
};

/* the check value's two keys, encrypted, and their IV */
static const ScePortabilityData s_encKey0 = {
	GAME_EBOOT_PARAM_KEY_SIZE,
	{
		0x3C, 0x32, 0xC3, 0xF8, 0x65, 0x8E, 0x32, 0xFE,
		0x4B, 0xCA, 0xC2, 0x69, 0x07, 0x07, 0x6D, 0xC6,
		0xA8, 0xF5, 0x3A, 0x6F, 0x9D, 0x57, 0xCB, 0xA5,
		0x31, 0x2C, 0x35, 0x55, 0xA7, 0xE2, 0xA5, 0xDA,
	},
};

static const ScePortabilityData s_encKey1 = {
	GAME_EBOOT_PARAM_KEY_SIZE,
	{
		0x3C, 0x67, 0xAD, 0x8F, 0xCA, 0x88, 0xB8, 0x85,
		0x8E, 0xC0, 0x21, 0xDD, 0x7E, 0x33, 0x1A, 0xF4,
		0x0A, 0x13, 0x2A, 0x13, 0x73, 0x3E, 0x86, 0xE7,
		0xE6, 0x87, 0x70, 0xD8, 0xEF, 0xBE, 0x4F, 0x09,
	},
};

static const SceUInt8 s_keyIv[0x10] = {
	0xFC, 0x9B, 0xA4, 0x1A, 0x36, 0xD5, 0x9B, 0x6F,
	0x2A, 0x48, 0x0D, 0x34, 0xE1, 0x78, 0x66, 0x0C,
};

/* 0x81032FD4 */
GameEbootParam::GameEbootParam()
{
	sce_paf_memset(&data, 0, sizeof(data));
}

/* 0x81032FF8, deleting 0x81033006 */
GameEbootParam::~GameEbootParam()
{
}

/*
 * The eboot parameters of the SELF at path read into data: its control information of type 6,
 * searched from the extended header's offset to the end of the SCE header (0x8103301C).
 */
static __attribute__((noinline)) int gameSelfReadEbootParam(const string &path,
							     GameEbootParamData *data)
{
	int ret = 0;
	GameSelfControlInfo info;
	GameSelfExtHeader extHeader;
	GameSelfHeader header;
	GameSelfEbootParamInfo param;
	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (file.get() == NULL)
		return ret;

	if (ret < 0)
		return ret;

	SceInt64 fileSize = file->GetFileSize();

	if (fileSize < 0) {
		ret = (int)fileSize;
		goto end;
	}

	ret = file->Read(&header, sizeof(header));
	if (ret < 0)
		goto end;

	if (header.magic[0] != 'S' || header.magic[1] != 'C' || header.magic[2] != 'E' ||
	    header.magic[3] != '\0') {
		ret = GAME_ERROR_BAD_SELF;
		goto end;
	}

	if ((SceUInt64)fileSize < header.headerLength) {
		ret = GAME_ERROR_BAD_SELF;
		goto end;
	}

	ret = file->Read(&extHeader, sizeof(extHeader));
	if (ret < 0)
		goto end;

	if ((extHeader.controlInfoOffset & 0xF) != 0) {
		ret = GAME_ERROR_BAD_SELF;
		goto end;
	}

	{
		SceUInt64 offset = extHeader.controlInfoOffset;

		do {
			SceInt64 pos = file->Seek(offset, SCE_SEEK_SET);

			if (pos < 0) {
				ret = (int)pos;
			} else {
				int size = file->Read(&info, sizeof(info));

				ret = 0;
				if (size < 0)
					ret = size;
			}
			if (ret < 0)
				goto end;

			if (info.type == GAME_SELF_CONTROL_INFO_EBOOT_PARAM) {
				sce_paf_memset(data, 0, sizeof(*data));

				pos = file->Seek(offset, SCE_SEEK_SET);
				if (pos < 0) {
					ret = (int)pos;
				} else {
					int size = file->Read(&param, sizeof(param));

					ret = 0;
					if (size < 0)
						ret = size;
				}
				if (ret < 0)
					goto end;

				data->header = param.header;
				sce_paf_memcpy(&data->attribute, param.params, sizeof(param.params));
				break;
			}

			offset += info.size;
		} while (offset < header.headerLength);
	}

	ret = 0;

end:
	file->Close();

	return ret;
}

/* The eboot parameters of the SELF at path read (0x8103328C). */
int GameEbootParam::Read(const string &path)
{
	int ret = gameSelfReadEbootParam(path, &data);

	if (ret < 0)
		return ret;

	return 0;
}

/*
 * The check value's key: whichever of the two decrypts to one starting "EBP" (0x810332A2).
 */
static __attribute__((noinline)) int gameEbootParamGetKey(void *key, SceSize size)
{
	ScePortabilityData key0;
	ScePortabilityData key1;
	int ret;

	if (key == NULL || size < GAME_EBOOT_PARAM_KEY_SIZE)
		return GAME_ERROR_INVALID_ARGUMENT;

	sce_paf_memset(&key0, 0, sizeof(key0));
	sce_paf_memset(&key1, 0, sizeof(key1));

	ret = vshSblSsDecryptWithPortability(GAME_EBOOT_PARAM_KEY_TYPE, (void *)s_keyIv,
					     (ScePortabilityData *)&s_encKey0, &key0);
	if (ret != 0)
		return ret;

	ret = vshSblSsDecryptWithPortability(GAME_EBOOT_PARAM_KEY_TYPE, (void *)s_keyIv,
					     (ScePortabilityData *)&s_encKey1, &key1);
	if (ret != 0)
		return ret;

	gameDebugDump("enc key0", s_encKey0.msg, GAME_EBOOT_PARAM_KEY_SIZE);
	gameDebugDump("dec key0", key0.msg, GAME_EBOOT_PARAM_KEY_SIZE);
	gameDebugDump("enc key1", s_encKey1.msg, GAME_EBOOT_PARAM_KEY_SIZE);
	gameDebugDump("dec key1", key1.msg, GAME_EBOOT_PARAM_KEY_SIZE);

	if (sce_paf_strncmp((const char *)key0.msg, GAME_EBOOT_PARAM_KEY_TAG, 3) == 0) {
		sce_paf_memcpy(key, key0.msg, GAME_EBOOT_PARAM_KEY_SIZE);
	} else {
		if (sce_paf_strncmp((const char *)key1.msg, GAME_EBOOT_PARAM_KEY_TAG, 3) != 0)
			return GAME_ERROR_EBOOT;
		sce_paf_memcpy(key, key1.msg, GAME_EBOOT_PARAM_KEY_SIZE);
	}

	return 0;
}

/*
 * The check value of size bytes at buf: its HMAC-SHA256, as text of hexadecimal digits in mac
 * (0x810333C4).
 */
static __attribute__((noinline)) int gameEbootParamMac(const void *buf, SceSize size, char *mac,
							SceSize macSize)
{
	SceUChar8 digest[0x20];
	int ret = gameEbootParamGetKey(mac, macSize);

	if (ret >= 0) {
		ret = sceHmacSha256Digest(mac, macSize, buf, size, digest);
		if (ret >= 0) {
			unsigned int i = 0;
			SceSize len = 0;

			ret = 0;
			do {
				if (len + 2 > macSize)
					break;

				SceUChar8 byte = digest[i];

				mac[0] = GAME_HEX_DIGITS[(byte >> 4) & 0xF];
				i++;
				len += 2;
				mac[1] = GAME_HEX_DIGITS[byte & 0xF];
				mac += 2;
			} while (i < sizeof(digest));
		}
	}

	return ret;
}

/*
 * The eboot parameters from the saved copy at path, its check value checked; without a copy,
 * none (0x81033456).
 */
int GameEbootParam::Load(const string &path)
{
	int ret = 0;
	SceUChar8 mac[GAME_EBOOT_PARAM_MAC_SIZE];
	SceUChar8 savedMac[GAME_EBOOT_PARAM_MAC_SIZE];
	common::SharedPtr<File> file = LocalFile::Open(path.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret < 0) {
		if (ret == GAME_PAF_ERROR_NOT_FOUND) {
			sce_paf_memset(&data, 0, sizeof(data));
			data.header = 1;
			return 0;
		}
		return ret;
	}

	int size = file->GetFileSize();

	if (size >= 0) {
		if (size != sizeof(data)) {
			ret = GAME_ERROR_BAD_PARAM_SIZE;
		} else {
			ret = file->Read(&data, sizeof(data));
			if (ret >= 0) {
				file->Close();

				sce_paf_memset(mac, 0, sizeof(mac));
				sce_paf_memcpy(savedMac, data.digest, sizeof(savedMac));
				gameDebugDump("ripped mac", savedMac, sizeof(savedMac));
				sce_paf_memset(data.digest, 0, sizeof(data.digest));

				ret = gameEbootParamMac(&data, sizeof(data), (char *)mac, sizeof(mac));
				if (ret < 0)
					return ret;

				gameDebugDump("calced mac", mac, sizeof(mac));

				if (sce_paf_memcmp(savedMac, mac, sizeof(mac)) != 0)
					return GAME_ERROR_EBOOT;

				return 0;
			}
		}
	}

	if (file.get() != NULL)
		file->Close();

	return ret;
}

/*
 * The eboot parameters of the SELF at selfPath saved at path with their check value
 * (0x8103375E).
 */
int GameEbootParam::Save(const string &selfPath, const string &path)
{
	int ret = 0;
	SceUChar8 mac[GAME_EBOOT_PARAM_MAC_SIZE];
	int err = gameSelfReadEbootParam(selfPath, &data);

	if (err < 0)
		return err;

	ret = err;
	sce_paf_memset(data.digest, 0, sizeof(data.digest));

	err = gameEbootParamMac(&data, sizeof(data), (char *)mac, sizeof(mac));
	if (err < 0)
		return err;

	ret = err;
	sce_paf_memcpy(data.digest, mac, sizeof(data.digest));
	gameDebugDump("calc mac", data.digest, sizeof(data.digest));

	common::SharedPtr<File> file = LocalFile::Open(path.c_str(),
						       SCE_O_RDWR | SCE_O_CREAT | SCE_O_TRUNC, 6, &ret);

	if (file.get() == NULL)
		return ret;

	if (ret < 0)
		return ret;

	ret = file->Write(&data, sizeof(data));
	if (ret >= 0) {
		file->Close();
		return 0;
	}

	if (file.get() != NULL)
		file->Close();

	return ret;
}
