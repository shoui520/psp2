/*
 * sdk_version.h: the SDK release a module was built with, under Sony's name (SDK Release
 * 03.570.011, target/include/sdk_version.h). The SDK a module's original was built with is a
 * build parameter (its MODULE_SDK_VERSION, given as SCE_PSP2_SDK_VERSION); 03.570.011 is the
 * release of the SDK the tree builds with.
 */
#ifndef _SCE_SDK_VERSION_SDK_VERSION_H
#define _SCE_SDK_VERSION_SDK_VERSION_H

#ifndef SCE_PSP2_SDK_VERSION
#define SCE_PSP2_SDK_VERSION	0x03570011
#endif

#endif
