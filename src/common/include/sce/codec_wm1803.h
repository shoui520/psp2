/* SceCodec's hardware-information dependencies. */
#pragma once

int ksceSysconGetHardwareInfo(void);
/* The WM1803 caller supplies one 16-byte hardware-information buffer. */
int ksceSysconGetHardwareInfo2(void *info);
