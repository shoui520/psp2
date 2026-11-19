/*
 * The free-trial license files: a 48-byte header ("RFTT", the file's MAC, where the data after
 * the license starts and its size), the 80-byte license and the data; the MAC's key is made
 * from the license's seed. The files are read and changed under Sony's
 * "SceGameUpdateWhiteListFtt" lock; the file and function names are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <std/stdio.h>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <thread/thread.h>
#include <datetime/datetime.h>
#include <file/file.h>

#include "ftt_license.h"
#include "white_list.h"

using namespace paf;

#define GAME_FTT_MAGIC		"RFTT"

/* a free-trial license file's first 128 bytes */
typedef struct GameFttFile {
	char magic[4];
	char mac[32];		/* HMAC-SHA256 of the file, taken with these bytes zero */
	SceUInt8 reserved[4];
	SceUInt32 dataOffset;	/* where the data after the license starts */
	SceUInt32 dataSize;
	GameFttLicense license;
} GameFttFile;

static thread::Mutex s_fttMutex("SceGameUpdateWhiteListFtt", 0);

/* the file's MAC written, keyed by seed (0x810A4C4C) */
static __attribute__((noinline)) int gameFttSign(common::SharedPtr<File> file, SceUInt64 seed)
{
	char key[16];
	char text[32];

	sce_paf_memset(text, 0, sizeof(text));
	sce_paf_snprintf(text, sizeof(text), "%016llx", seed);
	sce_paf_memset(key, 0, sizeof(key));
	sce_paf_memcpy(key, text, sizeof(key));

	int ret = gameWhiteListFileSign(file, key, offsetof(GameFttFile, mac));

	if (ret < 0)
		return ret;
	return 0;
}

/* the file's MAC checked, keyed by seed */
static inline int gameFttVerify(common::SharedPtr<File> file, SceUInt64 seed)
{
	char text[32];
	char key[16];

	sce_paf_memset(text, 0, sizeof(text));
	sce_paf_snprintf(text, sizeof(text), "%016llx", seed);
	sce_paf_memset(key, 0, sizeof(key));
	sce_paf_memcpy(key, text, sizeof(key));
	return gameWhiteListFileVerify(file, key, offsetof(GameFttFile, mac));
}

/* a header read from file checked: its magic and the file's MAC (0x810A4D3A) */
static __attribute__((noinline)) int gameFttCheck(common::SharedPtr<File> file,
						  const GameFttFile *ftt)
{
	int ret = 0;

	if (sce_paf_strncmp(ftt->magic, GAME_FTT_MAGIC, 4) != 0)
		ret = GAME_WHITE_LIST_ERROR_BAD_FTT;
	if (ret < 0)
		return ret;

	ret = gameFttVerify(file, ftt->license.macSeed);
	if (ret == GAME_WHITE_LIST_ERROR_BAD_MAC)
		ret = GAME_WHITE_LIST_ERROR_BAD_FTT;
	return ret;
}

/* whether the file is open and holds a whole header */
static inline int gameFttCheckSize(common::SharedPtr<File> file)
{
	if (file.get() == NULL)
		return GAME_WHITE_LIST_ERROR_BAD_FILE;

	int64_t size = file->GetFileSize();

	if (size < 0)
		return (int)size;
	if (size < (int64_t)sizeof(GameFttFile))
		return GAME_WHITE_LIST_ERROR_BAD_FILE;
	return 0;
}

/* the header read from the start of the file */
static inline int gameFttReadHeader(common::SharedPtr<File> file, GameFttFile *ftt)
{
	file->Seek(0, SCE_SEEK_SET);

	int n = file->Read(ftt, sizeof(*ftt));

	if (n < 0)
		return n;
	if (n != sizeof(*ftt))
		return GAME_WHITE_LIST_ERROR_BAD_FTT;
	return 0;
}

/* the license read from a free-trial license file (0x810A4EC4) */
int gameFttLicenseRead(common::SharedPtr<File> file, GameFttLicense *license)
{
	thread::MutexLock lock(&s_fttMutex);
	GameFttFile ftt;

	sce_paf_memset(license, 0, sizeof(*license));

	int ret = gameFttCheckSize(file);

	if (ret < 0)
		return ret;
	ret = gameFttReadHeader(file, &ftt);
	if (ret < 0)
		return ret;
	ret = gameFttCheck(file, &ftt);
	if (ret < 0)
		return ret;

	sce_paf_memcpy(license, &ftt.license, sizeof(*license));

	rtc::Tick time1 = license->time1;
	datetime::DateTime date1 = datetime::DateTime::ConvertRtcTickToDateTime(&time1);
	rtc::Tick time2 = license->time2;
	datetime::DateTime date2 = datetime::DateTime::ConvertRtcTickToDateTime(&time2);

	return ret;
}

/* the size of the data after the license; the file is left at the data (0x810A5148) */
int gameFttLicenseGetDataSize(common::SharedPtr<File> file, SceSize *size)
{
	thread::MutexLock lock(&s_fttMutex);
	GameFttFile ftt;

	*size = 0;

	int ret = gameFttCheckSize(file);

	if (ret < 0)
		return ret;
	ret = gameFttReadHeader(file, &ftt);
	if (ret < 0)
		return ret;
	ret = gameFttCheck(file, &ftt);
	if (ret < 0)
		return ret;

	if (file->Seek(ftt.dataOffset, SCE_SEEK_SET) != ftt.dataOffset)
		return GAME_WHITE_LIST_ERROR_BAD_FTT;
	*size = ftt.dataSize;
	return ret;
}

/* flag set in the license's flags, and the file's MAC written again (0x810A53CC) */
int gameFttLicenseSetFlag(common::SharedPtr<File> file, SceUInt32 flag)
{
	thread::MutexLock lock(&s_fttMutex);
	GameFttFile ftt;

	int ret = gameFttCheckSize(file);

	if (ret < 0)
		return ret;
	ret = gameFttReadHeader(file, &ftt);
	if (ret < 0)
		return ret;
	ret = gameFttCheck(file, &ftt);
	if (ret < 0)
		return ret;

	if (file->Seek(offsetof(GameFttFile, license.flags), SCE_SEEK_SET) !=
	    offsetof(GameFttFile, license.flags))
		return GAME_WHITE_LIST_ERROR_BAD_FTT;

	SceUInt32 flags = ftt.license.flags | flag;
	int n = file->Write(&flags, sizeof(flags));

	if (n < 0)
		return n;
	if (n != sizeof(flags))
		return GAME_WHITE_LIST_ERROR_SHORT_WRITE;

	ret = gameFttSign(file, ftt.license.macSeed);
	if (ret < 0)
		return ret;
	return 0;
}
