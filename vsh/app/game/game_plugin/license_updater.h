/*
 * The game plugin's license updater (license_updater.cpp): an application's license and its
 * additional contents' licenses fetched again from the store, through a PlayStation Store session
 * and its EPW, by the license server's threads ("GameUpdateLicenseSub",
 * "GameUpdateAddContLicenseSub", "GameDownloadLicenseSub"). The names are ours (no record names
 * them); the thread names are Sony's.
 */
#ifndef _GAME_PLUGIN_LICENSE_UPDATER_H
#define _GAME_PLUGIN_LICENSE_UPDATER_H

#include <scetypes.h>
#include <np/np_commerce2.h>
#include <appmgr/appmgr_license.h>
#include <std/string>
#include <std/vector>
#include <thread/thread.h>

/* what the license server is asked to update an application's license with (0x8109FFB6) */
struct GameLicenseUpdateArgs {
	const char *contentId;
	const char *epw;			/* the store session's EPW */
	const char *titleId;
	const char *rifName;			/* the license's file name */
	SceBool isPsp;				/* stored as a word */
};

/* what it is asked to update an additional content's license with (0x810A0040) */
struct GameAddcontLicenseUpdateArgs {
	const char *contentId;
	const char *epw;
	const char *titleId;
	const char *rifName;
	SceBool isPsp;
	const char *drive;
	const char *addcontId;
	SceUInt64 startTime;			/* the license's period, given back */
	SceUInt64 expireTime;
};

/* what it is asked to download a license with (0x810A00C6) */
struct GameLicenseDownloadArgs {
	const char *contentId;
	const char *epw;
	const char *rifName;
};

/* what the license server answers for an additional content's license past its period */
#define GAME_LICENSE_SERVER_ERROR_NO_LICENSE	-2146416473	/* 0x801048A7 */

/* the folder a license is downloaded into before it is moved to its place */
#define GAME_LICENSE_SERVER_TEMP_DIR		"ux0:temp/license"

/*
 * The license server (license_server.cpp, 72 bytes): NpKdc loaded and given its work memory, and
 * each request run on a thread of its own and joined, under its mutex. A license is downloaded
 * into the temp folder, then moved to its place. The names are ours (no record names them).
 */
class GameLicenseServer {
public:
	GameLicenseServer();
	~GameLicenseServer();

	int Init(void);
	int Term(void);

	int UpdateLicense(const GameLicenseUpdateArgs *args);
	int UpdateAddcontLicense(GameAddcontLicenseUpdateArgs *args);
	int DownloadLicense(const GameLicenseDownloadArgs *args);

	/* the downloaded license file, and the folder it is in */
	paf::string GetDownloadPath(void);
	paf::string GetDownloadDir(void);

private:
	friend class GameLicenseUpdateThread;
	friend class GameAddcontLicenseUpdateThread;
	friend class GameLicenseDownloadThread;

	int ClearTempDir(void);
	int CommitLicense(void);
	int WriteTempLicense(const void *license, SceSize size);
	int CreateTempLicense(const char *rifName, SceSize size);
	int DoUpdateLicense(const GameLicenseUpdateArgs *args);
	int ClearDriveTempDir(const char *drive);
	int RemoveDriveTempDir(const char *drive);
	int GetLicensePeriod(void *license, SceSize size, bool *expired,
			     SceUInt64 *startTime, SceUInt64 *expireTime);
	int CreateDriveTempLicense(const char *drive, const char *rifName, SceSize size);
	int DoUpdateAddcontLicense(GameAddcontLicenseUpdateArgs *args);
	int DoDownloadLicense(const GameLicenseDownloadArgs *args);

	paf::thread::Mutex m_mutex;
	paf::string m_licensePath;		/* where the license goes */
	paf::string m_tempPath;			/* where it is downloaded */
	void *m_kdcWork;			/* NpKdc's work memory */
	SceBool m_isPsp;			/* the request's: a PSP application's license */
	bool m_started;				/* Init done */
};

typedef char GameLicenseServerSize[sizeof(GameLicenseServer) == 72 ? 1 : -1];

/* an additional content's license as addcont/info keeps it, 128 bytes */
struct GameAddcontLicenseInfo {
	char contentId[48];
	SceUInt64 startTime;			/* the license's period */
	SceUInt64 expireTime;			/* the license fetched again once this is past */
	SceUInt8 reserved[64];
};

typedef char GameAddcontLicenseInfoSize[sizeof(GameAddcontLicenseInfo) == 128 ? 1 : -1];

/*
 * The licenses of an application's additional contents in its addcont/info folder, of a Vita
 * application and of a PSP one (addcont_info.cpp; the names are ours, no record gives Sony's).
 */
int gameAddcontInfoPrepare(const char *path);
int gameAddcontInfoWrite(const char *path, const GameAddcontLicenseInfo *info);
int gameAddcontInfoCheck(const char *path, paf::vector<paf::string> *dirs, bool *found);
int gameAddcontInfoRead(const char *path, const paf::vector<paf::string> *dirs,
			paf::vector<GameAddcontLicenseInfo> *infos);
int gameAddcontInfoRemove(const char *path, const char *contentId);
int gamePspAddcontInfoPrepare(const char *path);
int gamePspAddcontInfoWrite(const char *path, const GameAddcontLicenseInfo *info);
int gamePspAddcontInfoCheck(const char *path, paf::vector<paf::string> *dirs, bool *found);
int gamePspAddcontInfoRead(const char *path, const paf::vector<paf::string> *dirs,
			   paf::vector<GameAddcontLicenseInfo> *infos);
int gamePspAddcontInfoRemove(const char *path, const char *contentId);

/* the store's result of an EPW request, as SceNpCommerce2's text result fills it (96 bytes) */
struct GameStoreEpw {
	SceNpCommerce2CommonData commonData;
	const char *epw;
};

/*
 * The plugin's license updater (constructor 0x8106471C, 272 bytes): the license read from its
 * file, its file name, the store session and its EPW, the request buffer.
 */
class GameLicenseUpdater {
public:
	GameLicenseUpdater();
	~GameLicenseUpdater();

	/* the license of the application at path updated (0x81064EFE) */
	int Update(const char *path, const char *titleId);

	/* the licenses of titleId's additional contents updated (0x81065FC0) */
	int UpdateAddcont(const char *titleId);

	/* titleId's license downloaded: its folder and its file (0x8106715C) */
	int Prepare(const paf::string &titleId, paf::string *contentId, paf::string *dir,
		    paf::string *path);

private:
	int ReadPspLicense(const char *rifName, char *contentId);
	int GetPspRifName(const char *path, char *rifName);
	int OpenStore(void);
	int RequestEpw(void);
	void CloseStore(void);
	bool AddcontParamExists(SceBool isPsp, const paf::string &drive, const paf::string &contentId);
	int GetAddcontDirs(SceBool isPsp, const char *titleId, paf::vector<paf::string> *dirs);
	paf::string GetAppPath(const paf::string &titleId);
	int GetDrive(const char *titleId, paf::string *drive);
	int ActivateFtt(const paf::string &titleId);

	AppmgrLicenseInfo m_license;
	char m_rifName[48];
	GameStoreEpw m_epw;
	SceUInt32 m_ctxId;			/* the store context */
	SceUInt32 m_sessionReqId;		/* the session's request, then its end's */
	SceUInt32 m_epwReqId;
	void *m_buf;				/* the requests' results */
};

typedef char GameLicenseUpdaterSize[sizeof(GameLicenseUpdater) == 272 ? 1 : -1];

#endif /* _GAME_PLUGIN_LICENSE_UPDATER_H */
