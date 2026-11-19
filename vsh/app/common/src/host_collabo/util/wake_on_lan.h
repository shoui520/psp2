/* Wake-on-LAN for the host collaboration (wake_on_lan.cpp). */
#ifndef _HOST_COLLABO_WAKE_ON_LAN_H
#define _HOST_COLLABO_WAKE_ON_LAN_H

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* wakes the host whose MAC address (6 bytes) is mac */
int sceHostCollaboUtilSendWakeOnLanRequestTo(const SceUInt8 *mac);

#ifdef __cplusplus
}
#endif

#endif /* _HOST_COLLABO_WAKE_ON_LAN_H */
