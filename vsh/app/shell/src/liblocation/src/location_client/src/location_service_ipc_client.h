/*
 * The location and heading services' methods (location_service_ipc_client.cpp, Sony's file; the
 * function names are ours, after the API that calls each).
 */
#ifndef LOCATION_CLIENT_SRC_LOCATION_SERVICE_IPC_CLIENT_H
#define LOCATION_CLIENT_SRC_LOCATION_SERVICE_IPC_CLIENT_H

#include <scetypes.h>
#include <liblocation.h>
#include "../../../../../../../lowlayer/src/ipmi/client.h"

int locationServiceOpen(IPMI::Client *client, SceLocationHandle *handle, SceUInt32 lmethod,
			SceUInt32 hmethod, SceUInt32 mode, int *pStatus);
int locationServiceClose(IPMI::Client *client, SceLocationHandle handle);
int locationServiceReopen(IPMI::Client *client, SceLocationHandle handle, SceUInt32 lmethod,
			  SceUInt32 hmethod);
int locationServiceGetMethod(IPMI::Client *client, SceLocationHandle handle, SceUInt32 *lmethod,
			     SceUInt32 *hmethod);
int locationServiceGetLocationResult(IPMI::Client *client, SceLocationHandle handle,
				     SceLocationLocationInfo *linfo);
int locationServiceCancelGetLocation(IPMI::Client *client, SceLocationHandle handle);
int locationServiceStartLocationCallback(IPMI::Client *client, SceLocationHandle handle,
					 SceUInt32 distance);
int locationServiceStopLocationCallback(IPMI::Client *client, SceLocationHandle handle);
int locationServiceGetHeading(IPMI::Client *client, SceLocationHandle handle,
			      SceLocationHeadingInfo *hinfo);
int locationServiceStartHeadingCallback(IPMI::Client *client, SceLocationHandle handle,
					SceFloat32 difference);
int locationServiceStopHeadingCallback(IPMI::Client *client, SceLocationHandle handle);
int locationServiceConfirm(IPMI::Client *client, SceLocationHandle handle, SceUInt32 arg,
			   SceUInt32 dataArg, const void *data);
int locationServiceGetConfirmResult(IPMI::Client *client, SceLocationHandle handle,
				    SceLocationDialogResult *dialogResult);
int locationServiceConfirmAbort(IPMI::Client *client, SceLocationHandle handle);
int locationServiceGetPermission(IPMI::Client *client, SceLocationHandle handle,
				 SceLocationPermissionInfo *permission);
int locationServiceDenyApplication(IPMI::Client *client, SceLocationHandle handle);
int locationServiceSetGpsEmulationFile(IPMI::Client *client, const char *path);
int locationServiceGetLocation(IPMI::Client *client, SceLocationHandle handle, SceUInt32 timeout,
			       SceLocationLocationInfo *linfo);
int locationServiceGetValue(IPMI::Client *client, SceUInt32 *value);

#endif
