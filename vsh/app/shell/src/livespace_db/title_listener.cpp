/*
 * The listener of the daemon's application events that drops a deleted application's events
 * and LiveArea.
 */
#include "livespace_db_internal.h"

static LsdbEventListener *s_titleListener;

/* an application deleted (event 1): its events and its LiveArea go with it */
static void LsdbTitleListenerOnEvent(int event, void *info, void *arg)
{
	LsdbAppInfo *app = ((LsdbAppInfoPtr *)info)->get();
	LsdbAppInfo::iterator it = app->find(LSDB_KEY_TITLE_ID);

	if (it == app->end())
		return;

	const char *titleId = LsdbValueText(it->second);

	if (event == 1)
		NeDaemon::GetInstance()->DeleteTitleEvents(titleId);
	if (event == 1) {
		LiDaemon *daemon = LiDaemon::GetInstance();
		paf::string id(titleId);

		daemon->Delete(&id);
	}
}

int LsdbTitleListenerInit(void)
{
	int result = 0;

	s_titleListener = new LsdbEventListener;
	if (s_titleListener == NULL)
		return LSDB_ERROR_NO_MEMORY;
	SceLsdb_BF481B29(s_titleListener, LsdbTitleListenerOnEvent, NULL);
	return result;
}

int LsdbTitleListenerTerm(void)
{
	LsdbEventListener *listener = s_titleListener;

	if (listener != NULL) {
		SceLsdb_5FCAF61C(listener);
		delete listener;
	}
	s_titleListener = NULL;
	return 0;
}
