/*
 * location_perm_svc.cpp: thin facade wrappers forwarding permission API calls to
 * LocationPermissionServer and PositionManager (0x81199430-0x8119951C, sti 0x811994AA).
 * The file and function names are ours (PLACEHOLDER).
 */
#include "location_service.h"

extern void *locationGetContext(void);			/* 0x8118BA84 */
extern int locationPosMgrSetAppPermByTitleId(void *pm, const char *titleId, int perm); /* 0x8118DC8A */
extern int locationPosMgrGetMainPerm(void *pm, int *out); /* 0x8118DDA6 */
extern int locationPosMgrSetMainPerm(void *pm, int val); /* 0x8118DDB6 */
extern int locationPosMgrGetParentalPerm(void *pm, int *out); /* 0x8118DE0C */
extern int locationPosMgrSetParentalPerm(void *pm, int val); /* 0x8118DE1C */
extern int locationPermServerStart(void);		/* 0x811996E0 */
extern int locationPermServerStop(void);		/* 0x811997F2 */

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
	return locationPosMgrSetAppPermByTitleId(locationGetContext(), titleId, perm);
}

/* 0x81199454 */
int locationPermSvcGetMainPerm(int unused, int *out)
{
	(void)unused;
	return locationPosMgrGetMainPerm(locationGetContext(), out);
}

/* 0x81199464 */
int locationPermSvcSetMainPerm(int unused, int val)
{
	(void)unused;
	return locationPosMgrSetMainPerm(locationGetContext(), val);
}

/* 0x81199474 */
int locationPermSvcGetParentalPerm(int unused, int *out)
{
	(void)unused;
	return locationPosMgrGetParentalPerm(locationGetContext(), out);
}

/* 0x81199484 */
int locationPermSvcSetParentalPerm(int unused, int val)
{
	(void)unused;
	return locationPosMgrSetParentalPerm(locationGetContext(), val);
}

/* 0x81199494 */
int locationPermSvcSetPermissionMode(int unused, int mode)
{
	(void)unused;
	return locationSvcSetPermissionMode(mode);
}

/* 0x8119949E */
int locationPermSvcSetAppPermission(int unused, const char *titleId)
{
	(void)unused;
	return locationSvcSetAppPermission(titleId, true);
}
