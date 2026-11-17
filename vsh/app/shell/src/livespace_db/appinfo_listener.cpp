/*
 * The shell's listener of the daemon's application events: SceLsdb_BF481B29 sets its
 * callback, SceLsdb_5FCAF61C removes it.
 */
#include "livespace_db_internal.h"

/* calls the listener's callback, if it has one */
static void LsdbCallEventListener(int event, void *info, unsigned int userData)
{
	LsdbEventListener *listener = (LsdbEventListener *)userData;

	if (listener->callback != NULL)
		listener->callback(event, info, listener->arg);
}

extern "C" {

int SceLsdb_5FCAF61C(LsdbEventListener *listener)
{
	if (listener->callback != NULL) {
		AppInfoDaemon::GetInstance()->RemoveListener(listener);
		listener->callback = NULL;
		listener->arg = NULL;
	}
	return 0;
}

int SceLsdb_BF481B29(LsdbEventListener *listener, LsdbEventCallback callback, void *arg)
{
	int result = 0;

	if (listener->callback != NULL) {
		AppInfoDaemon::GetInstance()->RemoveListener(listener);
		listener->callback = NULL;
		listener->arg = NULL;
	}
	if (callback != NULL) {
		listener->callback = callback;
		listener->arg = arg;
		result = AppInfoDaemon::GetInstance()->AddListener(LsdbCallEventListener, listener);
	}
	return result;
}

}
