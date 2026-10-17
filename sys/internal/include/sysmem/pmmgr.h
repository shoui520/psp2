/*
 * ScePmMgrForDriver (os0:kd/sysmem.skprx, pmmgr.c): each export declared once, with sysmem's
 * definition as the prototype.
 */
#pragma once

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ScePmMgrForDriver_2AC815A2: 1 on a product mode console (the KBL parameters' boot type) */
typedef SceUInt8 SceProductMode;

int ScePmMgrForDriver_2AC815A2(SceProductMode *mode);	/* vitasdk: kscePmMgrGetProductMode */
int ScePmMgrForDriver_BD1F193B(void);	/* vitasdk: kscePmMgrIsExternalBootMode */

#ifdef __cplusplus
}
#endif
