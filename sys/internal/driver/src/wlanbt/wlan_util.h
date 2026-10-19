/* The WLAN utility methods (wlan_util.c) the other wlanbt files call. */
#ifndef WLANBT_WLAN_UTIL_H
#define WLANBT_WLAN_UTIL_H

#include "wlanbt_internal.h"

int wlanParseSecurityInformationElement(WlanSecurityInfo *security,
	const WlanInformationElement *element);
int wlanCopyInformationElement(const void *buffer, unsigned int length, int id,
			       void *output, unsigned int *outputSize,
			       unsigned int *offset);

#endif
