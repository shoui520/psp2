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

/* promote_patch.cpp: an application's patch promoted (0x8103FB34..0x8104639C) */

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

#endif
