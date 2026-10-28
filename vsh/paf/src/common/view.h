#ifndef PAF_COMMON_VIEW_H
#define PAF_COMMON_VIEW_H

#include <stddef.h>
#include "../std/string.h"

typedef struct Plugin Plugin;
typedef struct ViewController ViewController;
typedef struct View View;

typedef struct ViewControllerVTable {
	ViewController *(*Destroy)(ViewController *self);
	void (*Delete)(ViewController *self);
	void (*PluginOnInit)(ViewController *self, Plugin *plugin);
	void (*PluginOnStart)(ViewController *self, Plugin *plugin);
	void (*PluginOnStop)(ViewController *self, Plugin *plugin);
	void (*PluginOnExit)(ViewController *self, Plugin *plugin);
} ViewControllerVTable;

struct ViewController {
	const ViewControllerVTable *vtable;
};

typedef struct ViewVTable {
	View *(*Destroy)(View *self);
	void (*Delete)(View *self);
} ViewVTable;

struct View {
	const ViewVTable *vtable;
	ViewController *m_controller;
};

typedef char ViewControllerSizeCheck[sizeof(ViewController) == 4 ? 1 : -1];
typedef char ViewSizeCheck[sizeof(View) == 8 ? 1 : -1];

#define ViewDestroy ScePafCommon_17280204
#define ViewConstruct ScePafCommon_547491A7
#define ViewConstructBase ScePafCommon_9109458C
#define ViewDelete ScePafCommon_ED9652F0
#define ViewDestroyBase ScePafCommon_8BC826E1
#define ViewControllerPluginOnInit ScePafCommon_633D1A29
#define ViewControllerPluginOnStart ScePafCommon_DEC45734
#define ViewControllerPluginOnStop ScePafCommon_82307117
#define ViewControllerPluginOnExit ScePafCommon_F88BA3DC

View *ViewDestroy(View *self);
View *ViewConstruct(View *self, const PafString *pluginName,
		    const PafString *resourcePath, ViewController *controller);
View *ViewConstructBase(View *self, const PafString *pluginName,
			const PafString *resourcePath, ViewController *controller);
void ViewDelete(View *self);
View *ViewDestroyBase(View *self);
void ViewControllerPluginOnInit(ViewController *self, Plugin *plugin);
void ViewControllerPluginOnStart(ViewController *self, Plugin *plugin);
void ViewControllerPluginOnStop(ViewController *self, Plugin *plugin);
void ViewControllerPluginOnExit(ViewController *self, Plugin *plugin);
ViewController *ViewControllerDestroy(ViewController *self);
void ViewControllerDelete(ViewController *self);

#define ViewPluginOnInit ScePafCommon_1F998F92
#define ViewPluginOnStart ScePafCommon_0DC3E4F3
#define ViewPluginOnStop ScePafCommon_A7E01D66
#define ViewPluginOnExit ScePafCommon_7C7D3188

void ViewPluginOnInit(Plugin *plugin);
void ViewPluginOnStart(Plugin *plugin);
void ViewPluginOnStop(Plugin *plugin);
void ViewPluginOnExit(Plugin *plugin);

#endif
