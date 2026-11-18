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
	int licenseFlags;			/* the license's, given back */
	int skuFlags;
	SceUInt64 expireTime;
};

/* what it is asked to download a license with (0x810A00C6) */
struct GameLicenseDownloadArgs {
	const char *contentId;
	const char *epw;
	const char *rifName;
};

/*
 * PLACEHOLDER: the license server, defined in another file of the plugin (constructor 0x8109FEA4,
 * 72 bytes): NpKdc loaded and given its work memory (0x8109FF30), unloaded (0x8109FF84), and each
 * request run on a thread of its own, joined, under its mutex.
 */
class GameLicenseServer {
public:
	GameLicenseServer();
	~GameLicenseServer();

	int Init(void);
	void Term(void);

	/* PLACEHOLDER names: 0x8109FFB6, 0x810A0040, 0x810A00C6 */
	int UpdateLicense(const GameLicenseUpdateArgs *args);
	int UpdateAddcontLicense(GameAddcontLicenseUpdateArgs *args);
	int DownloadLicense(const GameLicenseDownloadArgs *args);

	/* PLACEHOLDER names: where the download was written, its folder (0x810A016C) and its
	   license file (0x810A0154) */
	paf::string GetDownloadDir(void);
	paf::string GetDownloadPath(void);

	SceUInt8 otherFields[72];
};

typedef char GameLicenseServerSize[sizeof(GameLicenseServer) == 72 ? 1 : -1];

/* an additional content's license as addcont/info keeps it, 128 bytes */
struct GameAddcontLicenseInfo {
	char contentId[48];
	int licenseFlags;
	int skuFlags;
	SceUInt64 expireTime;			/* the license fetched again once this is past */
	SceUInt8 reserved[64];
};

typedef char GameAddcontLicenseInfoSize[sizeof(GameAddcontLicenseInfo) == 128 ? 1 : -1];

/*
 * PLACEHOLDER names: addcont/info of a PSP application (0x8109F85A, 0x8109FE66, 0x8109CCEC) and of
 * a Vita one (0x8109BD1C, 0x8109C328, 0x81099174), in another file of the plugin: the licenses of
 * the additional contents in dirs read, one removed, one written.
 */
int gamePspAddcontInfoRead(const char *path, const paf::vector<paf::string> *dirs,
			   paf::vector<GameAddcontLicenseInfo> *infos);
int gamePspAddcontInfoRemove(const char *path, const char *contentId);
int gamePspAddcontInfoWrite(const char *path, const GameAddcontLicenseInfo *info);
int gameAddcontInfoRead(const char *path, const paf::vector<paf::string> *dirs,
			paf::vector<GameAddcontLicenseInfo> *infos);
int gameAddcontInfoRemove(const char *path, const char *contentId);
int gameAddcontInfoWrite(const char *path, const GameAddcontLicenseInfo *info);

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
