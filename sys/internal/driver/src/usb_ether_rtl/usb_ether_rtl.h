#ifndef SCE_USB_ETHER_RTL_H
#define SCE_USB_ETHER_RTL_H

#include <stddef.h>
#include <usb_ether_smsc/usb_ether_smsc.h>
#include <sysmem/sysroot.h>

int sceRegMgrGetKeyInt(const char *category, const char *name, int *value);
void *__memcpy_chk(void *destination, const void *source, size_t length,
		   size_t destinationSize);

#endif
