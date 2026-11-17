/*
 * The daemon's requests SceLsdb exports without a client: an application's parameter bits and
 * values, the configuration, and an application's refresh.
 */
#include "livespace_db_internal.h"
#include "lsdb_keys.h"

extern "C" {

/* sets the bits of mask in an application's integer parameter to bits, and tells the listeners */
int SceLsdb_C36501F7(const paf::string *titleId, unsigned int key, const LsdbValue *bits,
	SceInt64 mask)
{
	return AppInfoDaemon::GetInstance()->UpdateAppInfoBits(titleId, key, bits, mask, true);
}

int SceLsdb_F624586E(const paf::string *titleId, unsigned int key, LsdbValue *value)
{
	return AppInfoDaemon::GetInstance()->GetAppInfoValue(titleId, key, value);
}

int SceLsdb_70A430B1(int key, const char *value)
{
	return AppInfoDaemon::GetInstance()->SetConfigString(key, value);
}

int SceLsdb_BCFEAC86(int key, SceInt64 bits, SceInt64 mask)
{
	return AppInfoDaemon::GetInstance()->UpdateConfigBits(key, bits, mask);
}

/* reads an application's parameters again, keeping its icon */
int SceLsdb_9EC8F00A(LsdbAppInfoPtr *app)
{
	AppInfoDaemon *daemon = AppInfoDaemon::GetInstance();
	LsdbIconPtr icon;
	paf::vector<LsdbAppInfoPtr> related;

	return daemon->RefreshApp(app, &icon, 0, &related, true);
}

}

/* SceLsdb_C36501F7 without telling the listeners */
int LsdbUpdateAppInfoBits(const paf::string *titleId, unsigned int key, const LsdbValue *bits,
	SceInt64 mask)
{
	return AppInfoDaemon::GetInstance()->UpdateAppInfoBits(titleId, key, bits, mask, false);
}
