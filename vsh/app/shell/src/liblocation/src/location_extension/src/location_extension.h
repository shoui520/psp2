/*
 * SceLibLocationExtension: sceLocationGetLocationWifiHistory and its query, on the handles of
 * SceLibLocation (whose list and handle objects it borrows: location_client.h). The functions
 * other than the exports, and the file names, are ours.
 */
#ifndef LOCATION_EXTENSION_SRC_LOCATION_EXTENSION_H
#define LOCATION_EXTENSION_SRC_LOCATION_EXTENSION_H

#include <scetypes.h>
#include <rtc.h>
#include <liblocation.h>
#include "../../location_client/src/location_client.h"

/* location_extension.cpp */
void setClientList(IpcClientList *list);
int getLocationWifiHistory(IpcClientList *list, IpcClientInfo *info, bool query,
			   SceRtcTick timestamp, SceUInt32 timeout, SceLocationLocationInfo *linfo);
int cancelQueryLocationWifiHistory(IpcClientList *list, IpcClientInfo *info);

/* location_service_ipc_client.cpp: the location service's Wi-Fi history methods */
int locationServiceGetLocationWifiHistory(IPMI::Client *client, SceLocationHandle handle,
					  bool query, bool option, SceRtcTick timestamp,
					  SceUInt32 timeout, SceLocationLocationInfo *linfo);
int locationServiceGetLocationWifiHistoryResult(IPMI::Client *client, SceLocationHandle handle,
						bool query, bool option,
						SceLocationLocationInfo *linfo);
int locationServiceCancelQueryLocationWifiHistory(IPMI::Client *client,
						  SceLocationHandle handle);

#endif
