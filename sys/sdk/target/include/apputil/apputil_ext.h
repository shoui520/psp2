/*
 * apputil/apputil_ext.h: the application utility's video data drive, under Sony's names and with
 * Sony's definitions (SDK Release 03.570.011, target/include/apputil/apputil_ext.h). SceAppUtilExt
 * exports the functions.
 */
#ifndef _SCE_APPUTIL_EXT_H
#define _SCE_APPUTIL_EXT_H

#include <apputil.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SCE_APPUTIL_DRIVE_NAME_VIDEO	"video0:"

SceInt32 sceAppUtilExtVideoMount(void);
SceInt32 sceAppUtilExtVideoUmount(void);

#ifdef __cplusplus
}
#endif

#endif /* _SCE_APPUTIL_EXT_H */
