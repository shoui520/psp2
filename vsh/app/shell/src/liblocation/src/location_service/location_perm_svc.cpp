/*
 * location_perm_svc.cpp: thin facade wrappers forwarding permission API calls to
 * LocationPermissionServer and PositionManager (0x81199430-0x8119951C, sti 0x811994AA).
 * The file and function names are ours (PLACEHOLDER).
 */
#include "location_internal.h"

/* 0x81199430 */
int locationPermSvcStart(void)
{
	return locationPermServerStart();
}

/* 0x81199438 */
int locationPermSvcStop(void)
{
	return locationPermServerStop();
}

/* 0x81199440 */
int locationPermSvcSetAppPerm(int unused, const char *titleId, int perm)
{
	(void)unused;
	return locationGetContext()->SetAppPermByTitleId(titleId, perm);
}

/* 0x81199454 */
int locationPermSvcGetMainPerm(int unused, int *out)
{
	(void)unused;
	return locationGetContext()->GetMainPerm(out);
}

/* 0x81199464 */
int locationPermSvcSetMainPerm(int unused, int val)
{
	(void)unused;
	return locationGetContext()->SetMainPerm(val);
}

/* 0x81199474 */
int locationPermSvcGetParentalPerm(int unused, int *out)
{
	(void)unused;
	return locationGetContext()->GetParentalPerm(out);
}

/* 0x81199484 */
int locationPermSvcSetParentalPerm(int unused, int val)
{
	(void)unused;
	return locationGetContext()->SetParentalPerm(val);
}

/* 0x81199494 */
int locationPermSvcGetGlobalPerm(int unused, int *out)
{
	(void)unused;
	return locationSvcGetGlobalPerm(out);
}

/* 0x8119949E */
int locationPermSvcGetAppPermSetting(int unused, int *out)
{
	(void)unused;
	return locationSvcGetAppPermSetting(out, true);
}
