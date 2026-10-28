#ifndef PAF_COMMON_VIEW_H
#define PAF_COMMON_VIEW_H

#include <stddef.h>

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
#define ViewDelete ScePafCommon_ED9652F0
#define ViewDestroyBase ScePafCommon_8BC826E1
#define ViewControllerPluginOnInit ScePafCommon_633D1A29
#define ViewControllerPluginOnStart ScePafCommon_DEC45734
#define ViewControllerPluginOnStop ScePafCommon_82307117
#define ViewControllerPluginOnExit ScePafCommon_F88BA3DC

View *ViewDestroy(View *self);
void ViewDelete(View *self);
View *ViewDestroyBase(View *self);
void ViewControllerPluginOnInit(ViewController *self, Plugin *plugin);
void ViewControllerPluginOnStart(ViewController *self, Plugin *plugin);
void ViewControllerPluginOnStop(ViewController *self, Plugin *plugin);
void ViewControllerPluginOnExit(ViewController *self, Plugin *plugin);
ViewController *ViewControllerDestroy(ViewController *self);
void ViewControllerDelete(ViewController *self);

#endif
