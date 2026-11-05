/*
 * Firmware selection, set by the build (cmake/fw.cmake).
 */

#pragma once

#ifndef FW_VERSION
#error "FW_VERSION not set; configure with -DFW=<id>"
#endif

#ifndef FW_IS_400
#error "FW_IS_400 not set; cmake/fw/<id>.cmake sets it"
#endif

/*
 * The firmwares are chosen for their roles, not their age: 4.00-DEBUGGER is the build with the
 * most debug information (most likely a 3.65 debug build, whatever its number), 3.65-CEX the
 * retail build, 3.650.011-TOOL the devkit build, and 3.630.011-DEBUGGER a firmware from an
 * internal SCE unit with vs0 intact. FW_IS_400 selects the 4.00 build's code; nothing selects
 * it by comparing FW_VERSION, which is only the version number the firmware carries.
 */
#define FW_SINCE(v)  (FW_VERSION >= (v))
#define FW_BEFORE(v) (FW_VERSION < (v))

/* development kit firmware (TOOL, DEBUGGER) vs retail (CEX) */
#ifdef FW_TYPE_CEX
#define FW_DEVKIT 0
#else
#define FW_DEVKIT 1
#endif
