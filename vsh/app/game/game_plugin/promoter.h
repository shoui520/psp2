/*
 * The game plugin's promoters: an application's folder made an installed application, its
 * record built and registered (promoter.cpp, 0x810695D0..0x81069D54), and the promoters of each
 * kind of content built on it. PLACEHOLDER names: the shell's "GamePromoter" work pool names the
 * family; the classes and their methods are named from what they do.
 */

#ifndef GAME_PROMOTER_H
#define GAME_PROMOTER_H

#include <scetypes.h>
#include <std/string>

#include "game_plugin.h"
#include "game_appinfo.h"

/* the length of a UTF-8 string in characters, up to the first invalid one (0x810695D0) */
int gameUtf8Length(const char *str);

/*
 * A UTF-8 string converted to UTF-16 into dst, size bytes with its terminator; an invalid or
 * unencodable character gives 0. The characters converted (0x81069622).
 */
int gameUtf8ToUtf16(SceWChar16 *dst, SceSize size, const char *src);

/* what a promoter reads its application's information from (each kind its own) */
class GamePromoteSource {
public:
	virtual ~GamePromoteSource();

	/* the application's information set into reg */
	virtual int SetInfo(GameAppRegistration *reg) = 0;
};

/* a promotion, as the plugin runs it (vtable 0x8152EC20) */
class GamePromoterIf {
public:
	virtual ~GamePromoterIf()
	{
	}

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg) = 0;
};

/* what gameAppRegister passes on to applauncher_plugin with a promotion (PLACEHOLDER names) */
struct GameRegisterParam {
	bool replace;			/* not set in promoter.cpp */
	bool registerEnd;		/* gameAppRegisterEnd after the registration */
	bool noLicense;			/* the #_up_status from no license */
	bool updateApp;			/* an update: registered with mode 2 */
	bool updateData;		/* likewise */
};

/* the stages of a promotion, for its progress */
#define GAME_PROMOTE_STAGES	4

/*
 * An application's folder at path promoted, its record built from the promoter's source and
 * registered (constructor 0x81069748, destructor 0x81069878, vtable 0x8152EC34), 124 bytes.
 */
class GamePromoter : public GamePromoterIf {
public:
	GamePromoter(const char *path, const GameJobProgress *progress, int userData, float scale);
	virtual ~GamePromoter();

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	/* the folder made ready for the promotion */
	virtual int Prepare(bool flag) = 0;

	/* the source of the application's information, deleted by the caller */
	virtual int CreateSource(GamePromoteSource **source) = 0;

	/* the record built from source, and the application's entry */
	virtual int SetInfo(GamePromoteSource *source, bool registerApp, GameAppRegistration *reg);

	/* what is left once the record is built */
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg)
	{
		return 0;
	}

	/* the progress after stage, of GAME_PROMOTE_STAGES */
	void SetProgress(int stage)
	{
		float percent = gameSumPercent(stage, m_weights, GAME_PROMOTE_STAGES);

		if (m_progress.percent == NULL || percent < *m_progress.percent ||
		    percent > 100.0f)
			return;

		*m_progress.percent = percent;
		if (m_progress.func != NULL)
			m_progress.func(m_progress.arg, *m_progress.percent);
		m_progress.busy = 0;
	}

	paf::string m_path;			/* the folder promoted */
	LsdbAppInfoPtr m_info;			/* the record it had */
	bool m_install;				/* registered as an install */
	bool m_checkVersion;			/* cleared by the PSM promoter */
	bool m_keepLiveArea;
	bool m_notifyTopmenu;
	paf::string m_name;			/* the folder's name, after its last '/' */
	paf::string m_appDir;
	paf::string m_dir;
	paf::string m_licensePath;		/* the #_up_status's license folder, when set */
	GameJobProgress m_progress;
	float m_weights[GAME_PROMOTE_STAGES];	/* each stage's part of the progress */
	int m_userData;
	GameRegisterParam m_param;
};

typedef char GamePromoterSize[sizeof(GamePromoter) == 124 ? 1 : -1];

/* promote_patch.cpp: an application's patch promoted (0x8103FB34..0x81045DF6) */

/* the APP_VER of info as a number, 0 when it has none (0x81040852) */
int gameAppInfoGetAppVer(const LsdbAppInfoPtr *info, SceUInt32 *version);

/* the APP_VER of the game card's patch of titleId (0x810408CC) */
int gameGcPatchGetAppVer(const paf::string &titleId, SceUInt32 *version);

/* whether a patch of titleId is in ux0:patch, grw0:patch, the game card's gro0:patch */
bool gameUx0PatchExists(const paf::string &titleId);		/* 0x81040DEE */
bool gameGrw0PatchExists(const paf::string &titleId);		/* 0x81041010 */
bool gameGcPatchExists(const paf::string &titleId);		/* 0x81041232 */

/*
 * Whether the card's title needs its patch installed over what storage holds: storage 1 when
 * grw0 has none, storage 2 when ux0 has none or an older one (0x81041454).
 */
bool gameAppIsUpdateNeeded(const LsdbAppInfoPtr *info, const paf::string &titleId,
			   const int *storage);

/* gameUx0PatchExists, for another file (0x810414D8) */
bool gamePatchExists(const paf::string &titleId);

/*
 * PLACEHOLDER: the check of an update against the installed application, defined in another
 * file of the plugin (constructor 0x8104B81C, Check 0x8104D4CA, vtable 0x8152EB1C), 40 bytes.
 */
class GameUpdateChecker {
public:
	GameUpdateChecker(const paf::string &path, bool flag1, bool flag2, GameJobProgress *progress,
			  int userData);
	virtual ~GameUpdateChecker();

	int Check(const LsdbAppInfoPtr *installed, LsdbAppInfoPtr *update);

	paf::string m_path;
	paf::string m_name;
	bool m_flag1;
	bool m_otherFlag;
	bool m_flag2;
	GameJobProgress *m_progress;
	int m_userData;
};

/*
 * PLACEHOLDER: what an application folder's source is built from, 92 bytes. The constructors
 * that read it are in another file of the plugin (0x8107A6F0, 0x8107A90A); its names are ours.
 */
struct GameAppSourceParam {
	GameAppSourceParam(const char *dir, const char *path, const char *name, const char *sysDir,
			   const char *metaDir, const LsdbAppInfoPtr &info)
		: dir(dir), path(path), name(name), sysDir(sysDir), metaDir(metaDir),
		  appPath(path), info(info)
	{
		options[0] = 0;
		options[1] = 0;
		options[2] = 0;
	}

	paf::string dir;		/* the folder read: the mount point, else the path */
	paf::string path;		/* the folder promoted */
	paf::string name;		/* its name */
	int options[3];			/* 0 from the patch promoter */
	paf::string sysDir;		/* dir's sce_sys */
	paf::string metaDir;		/* the LiveArea's folder (ur0:appmeta/<name>) */
	paf::string appPath;		/* the folder promoted, again */
	LsdbAppInfoPtr info;		/* the record it had */
};

/*
 * PLACEHOLDER: the source of an application folder's information, defined in another file of
 * the plugin (constructor 0x810475B2, vtable 0x8152EADC), 172 bytes; laid out when that file is
 * written.
 */
class GameAppSource : public GamePromoteSource {
public:
	GameAppSource(const GameAppSourceParam &param);

	virtual int SetInfo(GameAppRegistration *reg);

	SceUInt8 m_body[168];		/* PLACEHOLDER: laid out with its file */
};

typedef char GameAppSourceSize[sizeof(GameAppSource) == 172 ? 1 : -1];

/*
 * PLACEHOLDER: the application's LiveArea brought into ur0:appmeta, defined in another file of
 * the plugin (constructor 0x8104DAC4, vtable 0x8152EB40), 64 bytes; its names are ours.
 */
class GameLiveAreaCopier {
public:
	GameLiveAreaCopier(bool flag, const paf::string &name, GameCancelFunc cancel, void *arg,
			   const GameJobProgress *progress, int userData, float scale);
	virtual ~GameLiveAreaCopier();

	/* the LiveArea of reg's application, read from dir */
	virtual int Run(GameAppRegistration *reg, const paf::string *dir);

	bool m_flag;
	paf::string m_name;
	GameCancelFunc m_cancel;
	void *m_arg;
	GameJobProgress m_progress;
	float m_weights[3];
	int m_userData;
};

typedef char GameLiveAreaCopierSize[sizeof(GameLiveAreaCopier) == 64 ? 1 : -1];

/* the drives a patch is promoted on */
#define GAME_PATCH_DRIVE_GRW0	1
#define GAME_PATCH_DRIVE_UX0	2

/*
 * An application's patch promoted: its folder moved from the drive's temp/promote into its
 * patch folder (constructor 0x8103FB34, destructor 0x8103FBA0, vtable 0x8152EA88), 148 bytes.
 * PLACEHOLDER names.
 */
class GamePatchPromoter : public GamePromoter {
public:
	GamePatchPromoter(const char *path, const LsdbAppInfoPtr *sfo, bool flag1, bool flag2,
			  const GameJobProgress *progress, int userData, float scale);
	virtual ~GamePatchPromoter();

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);
	virtual int Prepare(bool flag);
	virtual int CreateSource(GamePromoteSource **source);
	virtual int SetInfo(GamePromoteSource *source, bool registerApp, GameAppRegistration *reg);
	virtual int Finish(GameAppRegistration *reg, GameCancelFunc cancel, void *arg);

	/* the promoted folder moved into the drive's patch folder (0x8103FC64) */
	int MovePatch();

	/* the game card's patch copied into the drive's temp/promote (0x810414E0) */
	int CopyCardPatch(GameJobProgress *progress);

	/*
	 * When m_move is set, the game card's patch copied into the drive's temp/promote and the
	 * card's application mounted with it (0x81041C0C).
	 */
	int MountCardPatch();

	/* the patch's param.sfo read into info (0x81042EA0) */
	int ReadSfo(LsdbAppInfoPtr *info);

	/* the installed application's record checked against the patch's (0x81042154) */
	int CheckInstalled(LsdbAppInfoPtr *installed, bool flag);

	const LsdbAppInfoPtr *m_sfo;	/* the parameters an update keeps, when set */
	paf::string m_mountPoint;	/* unmounted when set */
	bool m_flag1;
	bool m_flag2;
	bool m_move;			/* MovePatch after the promotion */
	int m_drive;			/* GAME_PATCH_DRIVE_* */
};

typedef char GamePatchPromoterSize[sizeof(GamePatchPromoter) == 148 ? 1 : -1];

/*
 * install_job.cpp: the install of a package of GameInstallParam (0x81045DF6..0x8104639C).
 *
 * PLACEHOLDER: the promoters of each kind of package, defined in other files of the plugin
 * (constructors 0x81073E90, 0x8107832C, 0x81076694, 0x81077624, on the base 0x81096008); their
 * names are ours, from the kind each one is made for.
 */
class GameAppInstaller : public GamePromoterIf {		/* kind 0, 160 bytes */
public:
	GameAppInstaller(const GameInstallParam *param, GameJobProgress *progress,
			 paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[156];		/* PLACEHOLDER: laid out with its file */
};

class GamePatchInstaller : public GamePromoterIf {		/* kind 1, 156 bytes */
public:
	GamePatchInstaller(const GameInstallParam *param, GameJobProgress *progress,
			   paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[152];		/* PLACEHOLDER: laid out with its file */
};

class GameLicenseInstaller : public GamePromoterIf {		/* kind 2, 128 bytes */
public:
	GameLicenseInstaller(const GameInstallParam *param, GameJobProgress *progress,
			     paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[124];		/* PLACEHOLDER: laid out with its file */
};

class GameAddcontInstaller : public GamePromoterIf {		/* kind 3, 172 bytes */
public:
	GameAddcontInstaller(const GameInstallParam *param, GameJobProgress *progress,
			     paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[168];		/* PLACEHOLDER: laid out with its file */
};

/* the kinds of GameInstallParam */
#define GAME_INSTALL_KIND_APP		0
#define GAME_INSTALL_KIND_PATCH		1
#define GAME_INSTALL_KIND_LICENSE	2
#define GAME_INSTALL_KIND_ADDCONT	3

/*
 * The install of a package of GameInstallParam: the promoter of its kind run on the job's
 * registration (constructor 0x81045E92, destructor 0x81046052, vtable 0x8152EAAC), 300 bytes.
 * PLACEHOLDER names.
 */
class GameInstallJob {
public:
	GameInstallJob(const GameInstallParam *param, bool flag, bool (*isStopRequested)(void),
		       int arg4);
	virtual ~GameInstallJob();

	/* the install run: the promoter of the package's kind, then the progress at 100 */
	int Run();

	/* the parameters checked; the package's name taken from its path (0x81045DF6) */
	int CheckParam();

	/* the promoter of param's kind (0x81046196) */
	int CreatePromoter(const GameInstallParam *param, GamePromoterIf **promoter,
			   GameJobProgress *progress, paf::string *errorTitleId);

	int m_result;				/* CheckParam's */
	GameInstallParam m_param;
	GameAppRegistration m_reg;
	paf::string m_name;			/* the package's name, after its path's last '/' */
	paf::string m_otherName;		/* not read in this file */
	bool m_flag;				/* the promoter's registerApp */
	bool m_otherFlag;			/* not read in this file */
	bool (*m_isStopRequested)(void);	/* the promoter's cancel */
	int m_arg4;				/* its argument */
	float m_percent;
	GameJobProgress m_progress;
	paf::string m_errorTitleId;		/* the title ID an error names */
};

typedef char GameInstallJobSize[sizeof(GameInstallJob) == 300 ? 1 : -1];

/*
 * install_ex_job.cpp: the install of a package folder of GameInstallExParam, by its install
 * information (0x8104639C..0x81046D04).
 *
 * PLACEHOLDER: the promoters of each content type, defined in other files of the plugin; their
 * names are ours, from the content type each one is made for.
 */
/* a PSP game without PBOOT.PBP (constructor 0x81071890), 156 bytes */
class GamePspInstaller : public GamePromoterIf {
public:
	GamePspInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                 GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[152];		/* PLACEHOLDER: laid out with its file */
};

/* a PSP game with PBOOT.PBP (constructor 0x810729E4), 152 bytes */
class GamePspPbootInstaller : public GamePromoterIf {
public:
	GamePspPbootInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                      GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[148];		/* PLACEHOLDER: laid out with its file */
};

/* content type 17 (constructor 0x81073D34), 124 bytes */
class GameType17Installer : public GamePromoterIf {
public:
	GameType17Installer(const char *path, const SceNpDrmInstallInfo *info,
	                    GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[120];		/* PLACEHOLDER: laid out with its file */
};

/* an application (GD) (constructor 0x8106D3EC), 156 bytes */
class GameGdInstaller : public GamePromoterIf {
public:
	GameGdInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[152];		/* PLACEHOLDER: laid out with its file */
};

/* additional content (AC) (constructor 0x8106D0B0), 132 bytes */
class GameAcInstaller : public GamePromoterIf {
public:
	GameAcInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[128];		/* PLACEHOLDER: laid out with its file */
};

/* a PlayStation Mobile game (constructor 0x810713C0), 184 bytes */
class GamePsmInstaller : public GamePromoterIf {
public:
	GamePsmInstaller(const char *path, const SceNpDrmInstallInfo *info,
	                 GameJobProgress *progress, paf::string *errorTitleId);

	virtual int Run(GameAppRegistration *reg, bool registerApp, GameCancelFunc cancel,
			void *arg);

	SceUInt8 m_body[180];		/* PLACEHOLDER: laid out with its file */
};

/* the content types an install information names (values from the switch at 0x810468E2) */
#define GAME_CONTENT_TYPE_17		17	/* PLACEHOLDER: what it is is not read */
#define GAME_CONTENT_TYPE_GD		21
#define GAME_CONTENT_TYPE_PSM		24
#define GAME_CONTENT_TYPE_PSM_UNITY	29

/*
 * The install of a package folder of GameInstallExParam: the promoter of its content type run
 * on the job's registration (constructor 0x810465F8, vtable 0x8152EABC), 1336 bytes.
 * PLACEHOLDER names.
 */
class GameInstallExJob {
public:
	GameInstallExJob(const GameInstallExParam *param, bool flag, int arg3, int arg4);
	virtual ~GameInstallExJob();

	/* the install run: the promoter of the content type, then the progress at 100 */
	int Run();

	/* the parameters checked, and the folder's install information read (0x8104654A) */
	int CheckParam();

	/* the promoter of info's content type, for the folder at path (0x810468E2) */
	static int CreatePromoter(const char *path, const SceNpDrmInstallInfo *info,
				  GamePromoterIf **promoter, GameJobProgress *progress,
				  paf::string *errorTitleId);

	int m_result;				/* CheckParam's */
	GameInstallExParam m_param;
	bool m_readInfo;			/* CheckParam reads m_info */
	SceNpDrmInstallInfo m_info __attribute__((aligned(8)));	/* at 304 in Sony's layout */
	GameAppRegistration m_reg;
	float m_percent;
	GameJobProgress m_progress;
	bool m_flag;				/* the promoter's registerApp */
	bool m_otherFlag;			/* not read in this file */
	bool m_flag2;				/* not read in this file */
	int m_arg3;				/* the promoter's cancel */
	int m_arg4;				/* its argument */
	paf::string m_errorTitleId;		/* the title ID an error names */
};

typedef char GameInstallExJobSize[sizeof(GameInstallExJob) == 1336 ? 1 : -1];

#endif
