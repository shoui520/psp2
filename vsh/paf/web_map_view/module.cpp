/*
 * ScePafWebMapView (vs0:sys/external/libpaf_web_map_view.suprx): paf's web view widget as a
 * library, for the apps that show a page inside their own screens (initialsetup, tel_reg,
 * webbrowser). Its start runs the constructors of its static objects, its stop their
 * destructors, and its one plain export registers the widget's factories with paf. The names
 * are ours.
 */
#include <stddef.h>
#include <scetypes.h>
#include <plugin.h>
#include <widget/factory.h>

extern "C" {
#include "../src/std/stdc++/atexit.h"
}

using namespace paf;

extern "C" void *__dso_handle;
void *__dso_handle;

/* SNC's linker-provided constructor bounds */
extern "C" void (*__start__Zpreinit_array[])(void);
extern "C" void (*__stop__Zpreinit_array[])(void);
extern "C" void (*__start__Zinit_array[])(void);
extern "C" void (*__stop__Zinit_array[])(void);

/* the web view widget's factories (web_view.cpp) */
extern ui::FactoryFuncTable g_factoryWebView;

extern "C" int module_start(SceSize argSize, const void *argp)
{
	void (**init)(void);

	__dso_handle = &__dso_handle;
	for (init = __start__Zpreinit_array; init < __stop__Zpreinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	for (init = __start__Zinit_array; init < __stop__Zinit_array; ++init) {
		if (*init != NULL)
			(*init)();
	}
	return 0;
}

extern "C" int module_stop(SceSize argSize, const void *argp)
{
	__cxa_finalize(&__dso_handle);
	return 0;
}

extern "C" void ScePafWebMapView_5E7D0BE7(void)
{
	Plugin::RegistFactoryTable(g_factoryWebView.m_name, g_factoryWebView);
}
