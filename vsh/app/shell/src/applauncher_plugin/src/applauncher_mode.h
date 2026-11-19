/*
 * applauncher_mode.h: the applauncher's modes. ModeBase and the mode classes are Sony's (their
 * names are the strings their GetName returns: ModeBase, LauncherMode, AppTabEditMode, ...);
 * the manager that keeps them, its table and the names here are ours (PLACEHOLDER): nothing in
 * the shell names them.
 *
 * The modes form a tree: each table entry names its parent mode and makes its mode. Changing
 * to a mode deletes the modes the current one has above the common parent and makes the ones
 * down to the new mode, on the main thread's next tick.
 */
#ifndef APPLAUNCHER_MODE_H
#define APPLAUNCHER_MODE_H

#include <stddef.h>
#include <scetypes.h>
#include <std/vector>

class ModeBase {
public:
	ModeBase();
	/* the class's name (0x810C2100) */
	virtual const char *GetName();
	virtual ~ModeBase();

	/* PLACEHOLDER: the name GetName returns */
	static const char *s_className;
};

PAF_VECTOR_INLINE_DTOR(ModeBase *)

/* PLACEHOLDER: no mode yet, and the parent of the root mode */
#define MODE_MANAGER_NONE	(-65535)
/* PLACEHOLDER: the mode asked for when every mode is to go */
#define MODE_MANAGER_EXIT	(-1)

/* PLACEHOLDER: a mode of the table: its parent's index and how it is made */
struct ModeEntry {
	int parent;
	ModeBase *(*create)(void);
};

/* PLACEHOLDER: a manager's modes */
struct ModeTable {
	const ModeEntry *entries;
	int count;
};

/* PLACEHOLDER: the manager of a tree of modes (0x810C2130-0x810C236E). The table is its base:
   Sony's constructor copies it before it sets the vtable */
class ModeManager : public ModeTable {
public:
	ModeManager(const ModeTable &table);
	virtual ~ModeManager();
	/* called after every update; nothing in the base */
	virtual void OnUpdate() {}

	/* the mode asked for made current: the modes on the way deleted and made */
	void Update();
	/* the mode to change to at the next update, unless every mode is going */
	void ChangeMode(int mode)
	{
		if (m_currentMode != MODE_MANAGER_EXIT)
			m_nextMode = mode;
	}

	static void UpdateCallback(void *manager);

	paf::vector<ModeBase *> m_modes;	/* the made modes, by index; NULL for the others */
	int m_nextMode;
	int m_currentMode;
	int m_previousMode;
};

/* PLACEHOLDER: a mode made for a table entry */
template<class T> ModeBase *ModeCreate()
{
	return new T;
}

/* PLACEHOLDER: the applauncher's modes, by index in its table (0x815079A4), each under its
   parent */
#define APPLAUNCHER_MODE_LAUNCHER		0	/* LauncherMode, the root */
#define APPLAUNCHER_MODE_EDIT			1	/* AppTabEditMode */
#define APPLAUNCHER_MODE_SET_THEME		2	/* AppTabSetThemeMode, under the edit */
#define APPLAUNCHER_MODE_CONTEXT_MENU		3	/* AppTabContextMenuMode, under the edit */
#define APPLAUNCHER_MODE_ICON_DEL		4	/* the rest under the context menu */
#define APPLAUNCHER_MODE_CHANGE_LOG		5
#define APPLAUNCHER_MODE_INTELLECTUAL_PROPERTY	6
#define APPLAUNCHER_MODE_ICON_INFO		7
#define APPLAUNCHER_MODE_ABOUT_THIS_GAME	8
#define APPLAUNCHER_MODE_COUNT			9

/* Sony's modes, as far as the manager's file uses them: written in the files after this one */
class LauncherMode : public ModeBase {
public:
	LauncherMode();
	virtual const char *GetName();
	virtual ~LauncherMode();
};

class AppTabEditMode : public ModeBase {
public:
	AppTabEditMode();
	virtual const char *GetName();
	virtual ~AppTabEditMode();
};

class AppTabSetThemeMode : public ModeBase {
public:
	AppTabSetThemeMode();
	virtual const char *GetName();
	virtual ~AppTabSetThemeMode();
};

class AppTabContextMenuMode : public ModeBase {
public:
	AppTabContextMenuMode();
	virtual const char *GetName();
	virtual ~AppTabContextMenuMode();
};

/* PLACEHOLDER: the modes of 8 bytes keep one word their own files name */
class AppTabIconDelMode : public ModeBase {
public:
	AppTabIconDelMode();
	virtual const char *GetName();
	virtual ~AppTabIconDelMode();

	SceUInt32 reserved;
};

class AppTabChangeLogMode : public ModeBase {
public:
	AppTabChangeLogMode();
	virtual const char *GetName();
	virtual ~AppTabChangeLogMode();

	SceUInt32 reserved;
};

class AppTabIntellectualProperty : public ModeBase {
public:
	AppTabIntellectualProperty();
	virtual const char *GetName();
	virtual ~AppTabIntellectualProperty();

	SceUInt32 reserved;
};

class AppTabIconInfoMode : public ModeBase {
public:
	AppTabIconInfoMode();
	virtual const char *GetName();
	virtual ~AppTabIconInfoMode();

	SceUInt32 reserved;
};

class AppTabAboutThisGameMode : public ModeBase {
public:
	AppTabAboutThisGameMode();
	virtual const char *GetName();
	virtual ~AppTabAboutThisGameMode();

	SceUInt32 reserved;
};

/* PLACEHOLDER: the applauncher's manager (0x810C1FE6-0x810C2100, applauncher_modemanager.cpp) */
class ApplauncherModeManager;
extern ApplauncherModeManager *g_applauncherModeManager;

class ApplauncherModeManager : public ModeManager {
public:
	ApplauncherModeManager(const ModeTable &table) : ModeManager(table)
	{
		g_applauncherModeManager = this;
		ChangeMode(APPLAUNCHER_MODE_LAUNCHER);
	}
	virtual ~ApplauncherModeManager();
};


#endif
