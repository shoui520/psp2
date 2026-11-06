/*
 * SceLibLocationInternal: the system's own location client. It makes SceLibLocation's list in
 * the system mode with its own build of SceLibLocation's client code (location_client.cpp, the
 * list of location_client.h without confirmPending), and hands it to SceLibLocation
 * (SceLibLocation_7F74AA94). The export names are the NIDs'; the file names are ours.
 */
#ifndef LOCATION_INTERNAL_SRC_LOCATION_INTERNAL_H
#define LOCATION_INTERNAL_SRC_LOCATION_INTERNAL_H

#include <scetypes.h>
#include <rtc.h>
#include <liblocation.h>
#include "../../location_client/src/location_client.h"
#include "../../location_client/src/location_service_ipc_client.h"

extern "C" {
SceInt32 SceLibLocationInternal_3500A98C(SceUInt32 open, SceUInt32 mode);
SceInt32 SceLibLocationInternal_A8531ABF(SceLocationHandle handle, SceUInt32 timeout,
					 SceLocationLocationInfo *linfo);
SceInt32 SceLibLocationInternal_29267421(SceLocationHandle handle, SceRtcTick timestamp,
					 SceUInt32 timeout, SceLocationLocationInfo *linfo);
SceInt32 SceLibLocationInternal_EEF11F68(const LocationAllocator *allocator);
SceInt32 SceLibLocationInternal_209228A3(SceLocationHandle handle, const SceWChar16 *firstText,
					 const SceWChar16 *secondText, SceUInt32 arg);
}

#endif
