/*
 * ScePmMgrForDriver (os0:kd/sysmem.skprx, pmmgr.c): each export declared once, with sysmem's
 * definition as the prototype.
 */
#pragma once

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* sceSblPmMgrGetProductMode: 1 on a product mode console (the KBL parameters' boot type) */
typedef SceUInt8 SceProductMode;

int sceSblPmMgrGetProductMode(SceProductMode *mode);
int ScePmMgrForDriver_BD1F193B(void);	/* vitasdk: kscePmMgrIsExternalBootMode */

#ifdef __cplusplus
}
#endif
