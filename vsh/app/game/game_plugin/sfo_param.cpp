/*
 * A param.sfo's parameters stored into an application's information, the file reader that
 * loads it, and the checks of the versions it gives (0x8103397C..0x81033F34). The file's name
 * and the functions' are ours (no record names them); the strings are Sony's.
 */

#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/string.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/localfile.h>

#include "../../shell/src/livespace_db/livespace_db_internal.h"
#include "../../shell/src/livespace_db/lsdb_keys.h"
#include "game_plugin.h"
#include "game_appinfo.h"

using namespace paf;

/* the library's error for a parameter the param.sfo does not have */
#define SFO_ERROR_NOT_FOUND	-2146412027	/* 0x80105A05 */

/* a parameter's key: the hash of its name (FNV-1a), 0 for an empty name */
static inline unsigned int gameSfoKeyHash(const char *name)
{
	size_t len = sce_paf_strlen(name);
	unsigned int hash = 0;

	if (len != 0) {
		hash = 2166136261u;
		for (size_t i = 0; i < len; i++)
			hash = (hash ^ (unsigned char)name[i]) * 16777619u;
	}

	return hash;
}

/*
 * The size bytes from offset of the file at path, into a new buffer *buf (freed again on
 * failure) (0x8103397C).
 */
int gameFileReadAlloc(const char *path, SceInt64 offset, void **buf, SceSize size)
{
	int ret = 0;

	*buf = sce_paf_memalign(4, size);
	if (*buf == NULL)
		return GAME_ERROR_NO_INFO_MEMORY;

	common::SharedPtr<File> file = LocalFile::Open(path, SCE_O_RDONLY, 0, &ret);

	if (ret != 0)
		goto error;

	if (offset > 0) {
		int pos = (int)file->Seek(offset, SCE_SEEK_SET);

		if (pos < 0) {
			ret = pos;
			goto error;
		}
	}

	ret = file->Read(*buf, size);
	if (ret < 0)
		goto error;

	file->Close();

	return 0;

error:
	if (file.get() != NULL)
		file->Close();

	if (*buf != NULL) {
		sce_paf_free(*buf);
		*buf = NULL;
	}

	return ret;
}

/*
 * The count parameters of params the param.sfo has stored into info: their length checked
 * (against the largest a parameter accepts too, with checkLength), texts that are not empty
 * and integers (0x81033AE0).
 */
static __attribute__((noinline)) int gameSfoStoreParams(SfoHandle *handle,
							 const GameSfoParam *params,
							 unsigned int count, LsdbAppInfoPtr *info,
							 bool checkLength)
{
	int ret = 0;

	for (unsigned int i = 0; i < count; i++, params++) {
		void *param;
		const char *key;
		SceSize length;
		SceSize maxLength;
		const void *value;
		int err = sfoFindParam(handle, params->name, &param);

		if (err < 0) {
			if (err == SFO_ERROR_NOT_FOUND)
				continue;
			return err;
		}

		err = sfoGetParamInfo(handle, param, &key, &length, &maxLength);
		if (err < 0)
			return err;

		if (checkLength && params->maxLength < maxLength)
			return GAME_ERROR_BAD_DATA;

		if (maxLength < length)
			return GAME_ERROR_BAD_DATA;

		err = sfoGetParamValue(handle, param, &value);
		if (err < 0)
			return err;

		unsigned int hash = params->key;

		if (hash == 0)
			hash = gameSfoKeyHash(params->name);

		if (params->format == GAME_SFO_FORMAT_TEXT) {
			if (*(const char *)value != '\0')
				gameLsdbValueSetText((**info)[hash], (const char *)value);
		} else if (params->format == GAME_SFO_FORMAT_INTEGER) {
			gameLsdbValueSetInteger((**info)[hash], *(const SceInt32 *)value);
		} else if (params->format == GAME_SFO_FORMAT_BINARY) {
			if (params->maxLength != sizeof(SceInt64))
				return GAME_ERROR_BAD_DATA;
			gameLsdbValueSetInteger((**info)[hash], *(const SceInt64 *)value);
		} else {
			return GAME_ERROR_BAD_DATA;
		}
	}

	return ret;
}

/* The parameters of the size-byte param.sfo at sfo stored into info (0x81033D70). */
int gameSfoLoadParams(const void *sfo, SceSize size, const GameSfoParam *params,
		      unsigned int count, LsdbAppInfoPtr *info, bool checkLength)
{
	SfoHandle *handle = NULL;
	int ret = sfoOpen(sfo, size, &handle);

	if (ret >= 0) {
		ret = gameSfoStoreParams(handle, params, count, info, checkLength);
		if (ret >= 0)
			ret = 0;

		if (handle != NULL)
			sfoClose(handle);
	}

	return ret;
}

/* Whether version is "N.NN" (0x81033DE0). */
int gameVersionCheckShort(const char *version)
{
	if (version != NULL && version[0] >= '0' && version[0] <= '9' && version[1] == '.' &&
	    version[2] >= '0' && version[2] <= '9' && version[3] >= '0' && version[3] <= '9' &&
	    version[4] == '\0')
		return 0;

	return GAME_ERROR_BAD_DATA;
}

/* Whether version is "NN.NN" (0x81033E22). */
int gameVersionCheck(const char *version)
{
	if (version != NULL && version[0] >= '0' && version[0] <= '9' && version[1] >= '0' &&
	    version[1] <= '9' && version[2] == '.' && version[3] >= '0' && version[3] <= '9' &&
	    version[4] >= '0' && version[4] <= '9' && version[5] == '\0')
		return 0;

	return GAME_ERROR_BAD_DATA;
}

/* Whether version is "NN.NNN" (0x81033E70). */
int gameSystemVersionCheck(const char *version)
{
	if (version != NULL && version[0] >= '0' && version[0] <= '9' && version[1] >= '0' &&
	    version[1] <= '9' && version[2] == '.' && version[3] >= '0' && version[3] <= '9' &&
	    version[4] >= '0' && version[4] <= '9' && version[5] >= '0' && version[5] <= '9' &&
	    version[6] == '\0')
		return 0;

	return GAME_ERROR_BAD_DATA;
}

/*
 * The digits of version, at most size - 1 characters, as a number, its dots left out
 * (0x81033ECC).
 */
int gameVersionToNumber(const char *version, SceSize size, SceUInt32 *number)
{
	SceSize len = 0;

	if (version != NULL) {
		while (len < size && version[len] != '\0')
			len++;
	}

	if (len > size - 1)
		return GAME_ERROR_BAD_DATA;

	SceUInt32 value = 0;
	int scale = 1;

	for (int i = (int)len - 1; i >= 0; i--) {
		if (version[i] != '.') {
			value += (version[i] - '0') * scale;
			scale *= 10;
		}
	}

	*number = value;

	return 0;
}
