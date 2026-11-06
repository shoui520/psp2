/*
 * The location and heading services' methods (location_service_ipc_client.cpp, Sony's file; the
 * function names are ours, after the API that calls each).
 */
#ifndef LOCATION_CLIENT_SRC_LOCATION_SERVICE_IPC_CLIENT_H
#define LOCATION_CLIENT_SRC_LOCATION_SERVICE_IPC_CLIENT_H

#include <scetypes.h>
#include <liblocation.h>
#include <rtc.h>
#include "../../../../../../../lowlayer/src/ipmi/client.h"

/* the location service's methods */
#define LOCATION_METHOD_OPEN			0x12340000
#define LOCATION_METHOD_CLOSE			0x12340001
#define LOCATION_METHOD_REOPEN			0x12340002
#define LOCATION_METHOD_GET_METHOD		0x12340003
#define LOCATION_METHOD_GET_LOCATION_RESULT	0x12340005
#define LOCATION_METHOD_CANCEL_GET_LOCATION	0x12340006
#define LOCATION_METHOD_START_LOCATION_CALLBACK	0x12340007
#define LOCATION_METHOD_STOP_LOCATION_CALLBACK	0x12340008
#define LOCATION_METHOD_GET_HEADING		0x12340009
#define LOCATION_METHOD_START_HEADING_CALLBACK	0x1234000A
#define LOCATION_METHOD_STOP_HEADING_CALLBACK	0x1234000B
#define LOCATION_METHOD_CONFIRM			0x1234000C
#define LOCATION_METHOD_GET_CONFIRM_RESULT	0x1234000E
#define LOCATION_METHOD_CONFIRM_ABORT		0x1234000F
#define LOCATION_METHOD_GET_PERMISSION		0x12340010
#define LOCATION_METHOD_DENY_APPLICATION	0x12340011
#define LOCATION_METHOD_SET_GPS_EMULATION_FILE	0x12340012
#define LOCATION_METHOD_GET_LOCATION		0x12340014
#define LOCATION_METHOD_GET_LOCATION_WIFI_HISTORY		0x12340015
#define LOCATION_METHOD_GET_LOCATION_WIFI_HISTORY_RESULT	0x12340016
#define LOCATION_METHOD_GET_VALUE		0x12340017
#define LOCATION_METHOD_CANCEL_QUERY_LOCATION_WIFI_HISTORY	0x12340018

/* the confirmation dialog's data the service reads */
#define LOCATION_CONFIRM_DATA_SIZE		2048
/* the emulation file's path */
#define LOCATION_GPS_EMULATION_PATH_SIZE	256

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

/* the Wi-Fi history methods (liblocation_extension, liblocation_internal) and their arguments */
struct GetLocationWifiHistoryParam {
	SceLocationHandle handle;
	bool query;
	/* false from liblocation_extension, true from liblocation_internal */
	bool option;
	SceRtcTick timestamp;
	SceUInt32 timeout;
};

struct GetLocationWifiHistoryResultParam {
	SceLocationHandle handle;
	bool query;
	bool option;
};

int locationServiceGetLocationWifiHistory(IPMI::Client *client, SceLocationHandle handle,
					  bool query, bool option, SceRtcTick timestamp,
					  SceUInt32 timeout, SceLocationLocationInfo *linfo);
int locationServiceGetLocationWifiHistoryResult(IPMI::Client *client, SceLocationHandle handle,
						bool query, bool option,
						SceLocationLocationInfo *linfo);
int locationServiceCancelQueryLocationWifiHistory(IPMI::Client *client,
						  SceLocationHandle handle);

#endif
