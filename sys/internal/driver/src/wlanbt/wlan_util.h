/* The WLAN utility methods (wlan_util.c) the other wlanbt and robin files call. */
#ifndef WLANBT_WLAN_UTIL_H
#define WLANBT_WLAN_UTIL_H

#include "wlanbt_internal.h"

int wlanParseSecurityInformationElement(WlanSecurityInfo *security,
	const WlanInformationElement *element);
SceWlanVisitor *WlanVisitorInitialize(SceWlanVisitor *visitor);
int WlanVisitorEnter(SceWlanVisitor *visitor);
SceWlanVisitor *WlanVisitorLeave(SceWlanVisitor *visitor);
int WlanVisitorIsIdle(const SceWlanVisitor *visitor);
SceWlanVisitor *WlanVisitorClose(SceWlanVisitor *visitor, int result);
int wlanCopyInformationElement(const void *buffer, unsigned int length, int id,
			       void *output, unsigned int *outputSize,
			       unsigned int *offset);
int wlanCopyWpaInformationElement(const void *buffer, unsigned int length,
				  void *output, unsigned int *outputSize);
uint32_t *wlanEnableInformationElement(uint32_t *mask, unsigned int id);

#endif
