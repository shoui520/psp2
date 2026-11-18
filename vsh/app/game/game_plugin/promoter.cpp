/*
 * The game plugin's promoter: an application's folder made an installed application, its record
 * built from what the promoter of its kind reads and registered with applauncher_plugin
 * (0x810695D0..0x81069D54), and the UTF-8 helpers at the file's start. The file's name and the
 * functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/string>
#include <common/shared_ptr.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"
#include "promoter.h"

using namespace paf;

/* the UTF-8 lead bytes of characters of two to four bytes */
#define GAME_UTF8_LEAD_MIN	0xC2
#define GAME_UTF8_LEAD_MAX	0xF4

int gameUtf8Length(const char *str)
{
	const SceUInt8 *p = (const SceUInt8 *)str;
	int length;

	for (length = 0; ; length++) {
		SceUInt32 c = *p++;

		if (c >= 0x80) {
			if (c >= GAME_UTF8_LEAD_MIN && c <= GAME_UTF8_LEAD_MAX) {
				int bits = c & 0x3F;
				SceUInt32 limit;

				for (limit = 0x800; ; limit <<= 5) {
					c = (bits << 6) | (*p++ & 0x3F);
					if (c < limit)
						break;
					bits = c & (limit - 1);
				}
			} else {
				c = 0;
			}
		}

		if (c == 0)
			break;
	}

	return length;
}

int gameUtf8ToUtf16(SceWChar16 *dst, SceSize size, const char *src)
{
	const SceUInt8 *p = (const SceUInt8 *)src;
	SceWChar16 units[2];
	SceWChar16 none = 0;
	SceSize left;
	int count;

	if (size < sizeof(SceWChar16))
		return 0;

	left = size - sizeof(SceWChar16);

	for (count = 0; ; count++) {
		SceUInt32 c = *p++;
		SceWChar16 *end;

		if (c >= 0x80) {
			if (c >= GAME_UTF8_LEAD_MIN && c <= GAME_UTF8_LEAD_MAX) {
				int bits = c & 0x3F;
				SceUInt32 limit;

				for (limit = 0x800; ; limit <<= 5) {
					c = (bits << 6) | (*p++ & 0x3F);
					if (c < limit)
						break;
					bits = c & (limit - 1);
				}
			} else {
				c = none;
			}
		}

		if (c == 0)
			break;

		if (c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF)) {
			units[0] = none;
			end = &units[1];
		} else if (c <= 0xFFFF) {
			units[0] = c;
			end = &units[1];
		} else {
			units[0] = ((c - 0x10000) >> 10) + 0xD800;
			units[1] = (c & 0x3FF) + 0xDC00;
			end = &units[2];
		}

		SceSize bytes = (SceSize)((char *)end - (char *)units) & ~1u;

		if (left < bytes)
			break;

		left -= bytes;

		const SceWChar16 *unit = units;

		for (SceSize n = end - units; n != 0; n--)
			*dst++ = *unit++;
	}

	*dst = none;

	return count;
}

GamePromoter::GamePromoter(const char *path, const GameJobProgress *progress, int userData,
			   float scale)
	: m_path(path), m_info(new LsdbAppInfo), m_install(false), m_checkVersion(true),
	  m_keepLiveArea(true), m_notifyTopmenu(true), m_progress(*progress, scale),
	  m_userData(userData)
{
	m_weights[0] = 0.3f;
	m_weights[1] = 0.3f;
	m_weights[2] = 0.4f;
	m_weights[3] = 0.0f;
	m_param.replace = false;
	m_param.registerEnd = true;
	m_param.noLicense = false;
	m_param.updateApp = false;
	m_param.updateData = false;
}

GamePromoter::~GamePromoter()
{
	gameRemoveDirectory("ux0:temp/promote", 0, 0, 0, true);
}

/*
 * The promotion: the folder made ready, the record built from the source, the rest done, and
 * the application registered when registerApp is set (0x8106999E).
 */
int GamePromoter::Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
		      void *arg)
{
	string::size_type pos = m_path.rfind("/", string::npos, sce_paf_strlen("/"));

	if (pos == string::npos)
		return GAME_ERROR_NO_DRIVE;

	m_name = m_path.substr(pos + 1, m_path.length() - pos);

	int ret = Prepare(true);

	if (ret < 0)
		return ret;

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	SetProgress(0);

	GamePromoteSource *source = NULL;

	ret = CreateSource(&source);
	if (ret < 0) {
		if (source != NULL)
			delete source;
		return ret;
	}

	ret = SetInfo(source, registerApp, reg);
	if (ret < 0) {
		if (source != NULL)
			delete source;
		return ret;
	}

	if (source != NULL)
		delete source;

	if (cancel != NULL && cancel(arg) != 0)
		return GAME_ERROR_ABORTED;

	SetProgress(1);

	ret = Finish(reg, cancel, arg);
	if (ret < 0)
		return ret;

	if (registerApp) {
		int mode = m_install;

		if (m_install && (m_param.updateApp || m_param.updateData))
			mode = 2;

		ret = gameAppRegister(reg, mode, m_keepLiveArea, m_notifyTopmenu, &m_param);
		if (ret < 0)
			return ret;
	}

	SetProgress(2);

	if (m_param.registerEnd && registerApp && m_notifyTopmenu)
		gameAppRegisterEnd(&reg->info);

	return 0;
}

/*
 * The record built: the source's information, the install time kept from the old record, the
 * change time now, the LiveArea path, the #_up_status from the license folder when there is one,
 * and the entry's short title and paths (0x81069C3E).
 */
int GamePromoter::SetInfo(GamePromoteSource *source, bool registerApp, GameAppRegistration *reg)
{
	int ret = source->SetInfo(reg);

	if (ret < 0)
		return ret;

	ret = gameAppInfoCopyTime(LSDB_KEY_CTIME, &m_info, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetTimeNow(LSDB_KEY_MTIME, &reg->info);
	if (ret < 0)
		return ret;

	ret = gameAppInfoSetLiveAreaPath(&reg->info, &m_appDir, &m_dir);
	if (ret < 0)
		return ret;

	if (m_licensePath.length() != 0) {
		ret = gameAppInfoSetUpStatus(&reg->info, &m_licensePath, &m_dir, m_userData,
					     &m_param.noLicense);
		if (ret < 0)
			return ret;
	}

	gameAppInfoGetStitle(&reg->info, &reg->entry->otherText);

	ret = gameAppEntryFill(reg, &m_appDir, &m_dir);
	if (ret < 0)
		return ret;

	return 0;
}
