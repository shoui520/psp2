/*
 * The launch white lists of a PlayStation TV: version_launch.dat names the lists and their
 * versions, list_launch_vita.dat and list_launch_emu.dat the Vita and PSP applications it may
 * start (or may not, for a black list), list_launch_teleport.dat the list kept in tm0:teleport.
 * The system's copies are in vs0:data/internal/launch, the ones in use in ur0:game/launch; newer
 * ones come from ux0:game/launch or from the network. Sony's mutex names the feature
 * ("SceGameUpdateWhiteList"); the files' names and the functions' are ours.
 */
#ifndef _WHITE_LIST_H
#define _WHITE_LIST_H

#include <scetypes.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <file/file.h>

/* what the white list functions give back (values only) */
#define GAME_WHITE_LIST_ERROR_NO_MEMORY		-2146430608	/* 0x80101170 */
#define GAME_WHITE_LIST_ERROR_NOT_FOUND		-2146430606	/* 0x80101172 */
#define GAME_WHITE_LIST_ERROR_BAD_FORMAT	-2146430604	/* 0x80101174 */
#define GAME_WHITE_LIST_ERROR_OLD_VERSION	-2146430602	/* 0x80101176 */
#define GAME_WHITE_LIST_ERROR_BAD_FILE		-2146430601	/* 0x80101177 */
#define GAME_WHITE_LIST_ERROR_NOT_OPENED	-2146430600	/* 0x80101178 */
#define GAME_WHITE_LIST_ERROR_NOT_FORCED	-2146430599	/* 0x80101179 */
#define GAME_WHITE_LIST_ERROR_SHORT_READ	-2146430598	/* 0x8010117A */
#define GAME_WHITE_LIST_ERROR_SHORT_WRITE	-2146430597	/* 0x8010117B */
#define GAME_WHITE_LIST_ERROR_BAD_MAC		-2146430595	/* 0x8010117D */
#define GAME_WHITE_LIST_ERROR_BAD_FTT		-2146430594	/* 0x8010117E */
#define GAME_WHITE_LIST_ERROR_BLACK_LIST	-2146430593	/* 0x8010117F */

/* a list's kind, the letter after its second ':' */
#define GAME_WHITE_LIST_TYPE_WHITE	1	/* 'w': only the applications it lists */
#define GAME_WHITE_LIST_TYPE_BLACK	2	/* 'b': all but the applications it lists */

/* one list in version_launch.dat: its version and where the network has it */
typedef struct GameWhiteListEntry {
	SceUInt32 version;
	char url[132];
} GameWhiteListEntry;

/* version_launch.dat read */
typedef struct GameWhiteListVersion {
	int format;
	GameWhiteListEntry vita;
	GameWhiteListEntry emu;
	GameWhiteListEntry teleport;
} GameWhiteListVersion;

/* the first line of list_launch_teleport.dat, and where its entries start */
typedef struct GameWhiteListTeleportHeader {
	int format;
	SceUInt32 version;
	int type;
	SceUInt32 headerSize;
} GameWhiteListTeleportHeader;

/* white_list.cpp */
int gameWhiteListUpdate(void);
int gameWhiteListDownload(void);
int gameWhiteListVitaCheck(const paf::string &titleId, bool *allowed);
int gameWhiteListEmuCheck(const paf::string &titleId, bool *allowed);
bool gameWhiteListCheckIsOn(void);
int gameWhiteListReadTeleportFile(paf::common::SharedPtr<paf::File> file, char *buf,
				  SceSize *size);
int gameWhiteListParseTeleportHeader(const char *buf, SceSize size,
				     GameWhiteListTeleportHeader *header);
int gameWhiteListRequestTeleportUpdate(void);

/*
 * PLACEHOLDER: the file helpers, in the file after ftt_license.cpp (0x810A5744..), not
 * written yet; the prototypes are taken from the calls
 */
int gameWhiteListDownloadFile(const char *url, const char *path, const SceUInt64 *maxSize);
int gameWhiteListFileUnwrap(const char *src, const char *dst, SceSize *maxSize,
			    bool removeSource);
int gameWhiteListFileRead(paf::common::SharedPtr<paf::File> file, char *buf, SceSize *size);
int gameWhiteListTeleportHeaderRead(const char *buf, SceSize size,
				    GameWhiteListTeleportHeader *header);
int gameWhiteListTeleportForceSet(void);
int gameWhiteListTeleportForceCheck(void);
bool gameWhiteListUx0Exists(void);
int gameWhiteListDirRecreate(const char *dir);
int gameWhiteListDirRemove(const char *dir);
int gameWhiteListFileCopy(const char *src, const char *dst);
int gameWhiteListFileSign(paf::common::SharedPtr<paf::File> file, const char *key, int kind);
int gameWhiteListFileVerify(paf::common::SharedPtr<paf::File> file, const char *key, int kind);

#endif /* _WHITE_LIST_H */
