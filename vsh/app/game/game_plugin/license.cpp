/*
 * SceShell's game plugin: a PSP game's license (its RIF in ux0:pspemu/PSP/LICENSE) and the
 * license validity period the game's information shows.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <rtc.h>
#include <npdrm/npdrm.h>
#include <std/string.h>
#include <std/string>
#include <common/char_encoding.h>
#include <file/localfile.h>

#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* where a PSP game's licenses are, by content ID */
#define GAME_PSP_LICENSE_DIR		"ux0:pspemu/PSP/LICENSE/"

/* where the content ID is in a PSP game's DATA.PSP, and its size */
#define GAME_PSP_CONTENT_ID_OFFSET	0x560
#define GAME_PSP_CONTENT_ID_SIZE	48

/* the expireTime of a license without an end */
#define GAME_LICENSE_NO_EXPIRY		0x7FFFFFFFFFFFFFFFLL

/* the license period's errors (values only) */
#define GAME_ERROR_LICENSE_NO_EXPIRY	-2146430656	/* 0x80101140 */
#define GAME_LICENSE_ERROR_EXPIRED	-2139095023	/* 0x80800011 */
#define GAME_LICENSE_ERROR_NOT_STARTED	-2139095016	/* 0x80800018 */

/* the SDK's glue over SceVshBridge's import (0x81417C70) */
extern "C" int vshRtcGetCurrentSecureTick(SceRtcTick *tick);

/* the time as the system's text, "-" without an end (0x81343D8E). PLACEHOLDER: its name */
wstring gameLicenseTimeToText(const SceUInt64 *time);

GamePspLicense::GamePspLicense()
{
	m_valid = false;
	m_result = 0;
	sce_paf_memset(&m_startTime, 0, sizeof(m_startTime));
	sce_paf_memset(&m_expireTime, 0, sizeof(m_expireTime));
}

GamePspLicense::~GamePspLicense()
{
}

/* The license at rifPath read and checked against the NP account (0x8101F254). */
int GamePspLicense::ReadRif(const string &rifPath)
{
	int ret = 0;
	SceUInt64 accountId = 0;

	int r = gameNpGetAccountId(&accountId);

	if (r < 0)
		return r;

	ret = r;

	common::SharedPtr<File> file = LocalFile::Open(rifPath.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret < 0)
		return ret;

	int size = file->GetFileSize();

	if (size < 0) {
		file->Close();
		return size;
	}

	SceNpDrmLicense *license = new SceNpDrmLicense;

	if (license == NULL) {
		file->Close();
		return GAME_ERROR_NO_INFO_MEMORY;
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

	ret = sceNpDrmGetRifInfo(license, size, 1, NULL, &accountId, NULL, NULL, NULL, NULL,
				 &m_startTime, &m_expireTime, NULL);
	m_result = ret;
	if ((ret != 0 || accountId != 0) && ret != GAME_PAF_ERROR_NOT_FOUND)
		m_valid = true;
	else
		m_valid = false;

	delete license;

	return 0;
}

/* The license's file name: the content ID in the game's DATA.PSP, with ".rif" (0x8101F51A). */
int GamePspLicense::GetRifName(const string &ebootPath, string *rifName)
{
	int ret = 0;
	GamePbpHeader header;

	sce_paf_memset(&header, 0, sizeof(header));

	int r = gamePbpReadHeader(ebootPath.c_str(), &header, NULL);

	if (r < 0)
		return r;

	ret = r;

	common::SharedPtr<File> file = LocalFile::Open(ebootPath.c_str(), SCE_O_RDONLY, 0, &ret);

	if (ret != 0)
		return ret;

	if (file->Seek(header.dataPspOffset + GAME_PSP_CONTENT_ID_OFFSET, SCE_SEEK_SET) < 0) {
		file->Close();
		return ret;
	}

	char contentId[GAME_PSP_CONTENT_ID_SIZE];

	sce_paf_memset(contentId, 0, sizeof(contentId));

	ret = file->Read(contentId, sizeof(contentId));
	if (ret < 0) {
		file->Close();
		return ret;
	}

	file->Close();

	*rifName = string(contentId) + string(".rif");

	if (rifName->length() > GAME_PSP_CONTENT_ID_SIZE)
		return GAME_ERROR_NO_DRIVE;

	return 0;
}

/* The license of the PSP game in path read (0x8101F8B2). */
int GamePspLicense::Check(const string &path)
{
	string rifName;
	string ebootPath = path + string("/EBOOT.PBP");

	int ret = GetRifName(ebootPath, &rifName);

	if (ret < 0)
		return ret;

	string name;
	string::size_type pos = path.rfind("/", string::npos, sce_paf_strlen("/"));

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	name = path.substr(pos + 1, path.length() - pos);

	ret = ReadRif(string(GAME_PSP_LICENSE_DIR) + rifName);
	if (ret >= 0)
		ret = 0;

	return ret;
}

int gameLicenseGetRemaining(const SceUInt64 *expireTime, SceUInt64 *remaining)
{
	SceRtcTick now;

	if (*expireTime == GAME_LICENSE_NO_EXPIRY) {
		*remaining = 0;
		return GAME_ERROR_LICENSE_NO_EXPIRY;
	}

	int ret = vshRtcGetCurrentSecureTick(&now);

	if (ret < 0)
		return ret;

	if (*expireTime > now.tick) {
		*remaining = *expireTime - now.tick;
		return 0;
	}

	*remaining = 0;

	return GAME_LICENSE_ERROR_EXPIRED;
}

int gameLicenseFormatTime(const SceUInt64 *time, char *text, SceSize size)
{
	wstring wtext = gameLicenseTimeToText(time);
	string utf8;

	common::Utf16ToUtf8(wtext, &utf8);

	if (size < utf8.length() + 1) {
		text[0] = '-';
		text[1] = '\0';
	} else {
		sce_paf_strncpy(text, utf8.c_str(), utf8.length());
		text[utf8.length()] = '\0';
	}

	return 0;
}

int gameLicenseCheckPeriod(const SceUInt64 *startTime, const SceUInt64 *expireTime)
{
	SceRtcTick now;

	if (*expireTime == GAME_LICENSE_NO_EXPIRY)
		return 0;

	int ret = vshRtcGetCurrentSecureTick(&now);

	if (ret < 0)
		return ret;

	if (now.tick < *startTime)
		return GAME_LICENSE_ERROR_NOT_STARTED;

	if (*expireTime > now.tick)
		return 0;

	return GAME_LICENSE_ERROR_EXPIRED;
}
