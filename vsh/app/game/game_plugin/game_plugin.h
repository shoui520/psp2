/*
 * game_plugin.h: the game plugin, a module of its own in Sony's early builds
 * (vs0:vsh/game/game_plugin) and linked into SceShell since. The names are ours (no record
 * names them).
 */
#ifndef _GAME_PLUGIN_H
#define _GAME_PLUGIN_H

#include <scetypes.h>
#include <plugin.h>
#include <std/string>
#include <std/vector>
#include <common/shared_ptr.h>
#include <memory/pool_allocator.h>

/*
 * PLACEHOLDER: the plugin's GetInterface(1) table, 65 functions (0x815431FC), each
 * written out with the plugin's code.
 */
struct GamePluginInterface {
	void (*func[65])(void);
};
extern GamePluginInterface gamePluginInterface;

/* the plugin's main object, made at start and deleted at stop (0x8102B296, 0x8102B2BA) */
void gamePluginCreate(paf::Plugin *plugin);
void gamePluginDestroy(void);

/*
 * The plugin's work pool, 64 KiB blocks, lent to one user at a time while the plugin runs
 * (0x8102B5F6), and given back (0x8102B6CC).
 */
int gamePluginGetPool(const char *user, paf::common::SharedPtr<paf::memory::PoolAllocator> *pool);
void gamePluginReleasePool(paf::common::SharedPtr<paf::memory::PoolAllocator> *pool);

/* a debug dump of a buffer, empty in this build (0x814098DC) */
void gameDebugDump(const char *label, const void *data, SceSize size);

/*
 * An installation's directory mounted with its keystone, from grw0: when dir is on it, else
 * from ux0:, the mount point left in mountPoint (0x8101E23C), and unmounted (0x8101E434).
 */
int gamePfsMount(const paf::string &dir, const paf::string &name, const void *keystone,
		 paf::string *mountPoint);
int gamePfsUnmount(paf::string *mountPoint);

/* a package's content type: additional content */
#define GAME_CONTENT_TYPE_ADDCONT	22

/* the plugin's error for an argument out of range (value only) */
#define GAME_ERROR_INVALID_ARGUMENT	-2146430720	/* 0x80101100 */

/* the plugin's errors several of its files return (values only) */
#define GAME_ERROR_NO_INFO_MEMORY	-2146430718	/* 0x80101102: no memory for an information */
#define GAME_ERROR_NO_VALUE		-2146430713	/* 0x80101107: a key the record does not have */
#define GAME_ERROR_NO_MEMORY		-2146430709	/* 0x8010110B */
#define GAME_ERROR_NO_DRIVE		-2146430682	/* 0x80101126: a path without a drive */

/* paf's error for a file that does not exist (value only) */
#define GAME_PAF_ERROR_NOT_FOUND	-2135994334	/* 0x80AF5022 */

/*
 * PLACEHOLDER: what 0x8136D9D4 reads from a split package's head.bin, 0x78 bytes; only the
 * content type and the body's size, in 32-byte blocks, are known.
 */
struct GamePackageInfo {
	SceUInt32 headerFields[1];
	SceUInt32 contentType;
	SceUInt32 moreHeaderFields[1];
	SceUInt32 bodyBlocks;
	SceUInt32 otherFields[26];
};

/*
 * PLACEHOLDER: an installation's package, its head.bin's information first and the
 * application's keystone at 0x278; the rest is not known yet.
 */
struct GameInstallPackage {
	GamePackageInfo info;
	SceUInt8 otherFields[0x278 - sizeof(GamePackageInfo)];
	SceUInt8 keystone[0x60];
};

/* the title IDs of the additional content an installation left (0x8102B8C4) */
int gamePluginGetAddcontTitles(paf::vector<paf::string> *titles);

/*
 * The plugin's counted mounts (mounter.cpp): an application's game data, with its patch and its
 * license (rif, written to ur0:temp/rif/work.bin, when given), the mount point left in
 * mountPoint (0x8101DE16), and unmounted (0x8101E0B2); the same without writing the license
 * (0x8101DBA0); an application's data (0x8101E56E, 0x8101E6F6); the PSP emulator's work
 * directory (0x8101E84C, 0x8101E9B6); a theme (0x8101ECBA, 0x8101EAE2; 0x8101EF4E); and the drive
 * a mount point stands for (0x8101D89C).
 */
int gameAppMount(const char *appPath, const char *patchPath, const void *rif, SceSize rifSize,
		 paf::string *mountPoint);
int gameAppUnmount(paf::string *mountPoint);
int gameGameDataMount(const char *appPath, const char *patchPath, const char *rifPath,
		      paf::string *mountPoint);
int gameAppDataMount(const paf::string &titleId, paf::string *mountPoint);
int gameAppDataUnmount(paf::string *mountPoint);
int gamePspemuWorkMount(paf::string *mountPoint);
int gamePspemuWorkUnmount(paf::string *mountPoint);
int gameThemeMount(const paf::string &contentId, const paf::string &themePath, const void *rif,
		   SceSize rifSize, paf::string *mountPoint);
int gameThemeDataMount(const paf::string &contentId, const paf::string &themePath,
		       const paf::string &rifPath, paf::string *mountPoint);
int gameThemeUnmount(paf::string *mountPoint);
void gameMountGetDrive(const paf::string &path, paf::string *drive);

/*
 * The drive a path is reached by: the drive of the mount that holds it, else the path's own
 * drive; drive is left as it is when neither is found (0x8101BFB4).
 */
void gamePathGetMountedDrive(const paf::string &path, paf::string *drive);

/*
 * A file or a directory tree removed, by a remover of its own (0x8101BA08); with mapErrors, an
 * error is translated by gameFileToPafError (0x8101C212). PLACEHOLDER: the meaning of the three words is not known.
 */
int gameRemoveDirectory(const char *path, int arg1, int arg2, int arg3, bool mapErrors);

/*
 * A file copied, by a job of its own, with buf as its work memory (0x8101AB80). PLACEHOLDER:
 * the meaning of the other words is not known; this file passes 0 for each.
 */
int gameCopyFile(const char *src, const char *dst, int arg2, int arg3, void *buf,
		 SceSize bufSize, int arg6, int arg7);

/* a split package's information read from its head.bin at path, with buf as work memory */
int gamePackageReadInfo(GamePackageInfo *info, const char *path, void *buf, SceSize bufSize);

/* The NP account ID, 0 when the registry has none (0x81018610). */
int gameNpGetAccountId(SceUInt64 *accountId);

/*
 * A PSP game's license: the RIF in ux0:pspemu/PSP/LICENSE named by the content ID in its
 * EBOOT.PBP, its check's result and its validity period (license.cpp; 32 bytes).
 */
class GamePspLicense {
public:
	GamePspLicense();
	virtual ~GamePspLicense();

	int ReadRif(const paf::string &rifPath);
	int GetRifName(const paf::string &ebootPath, paf::string *rifName);
	int Check(const paf::string &path);

	int m_result;
	bool m_valid;
	SceUInt64 m_startTime;
	SceUInt64 m_expireTime;
};

/*
 * The time left until a license's expireTime (0x8101FBEA), that time as text, "-" when it
 * does not fit in size bytes (0x8101FC78), and whether now is in the period (0x8101FD0E).
 */
int gameLicenseGetRemaining(const SceUInt64 *expireTime, SceUInt64 *remaining);
int gameLicenseFormatTime(const SceUInt64 *time, char *text, SceSize size);
int gameLicenseCheckPeriod(const SceUInt64 *startTime, const SceUInt64 *expireTime);

#endif
