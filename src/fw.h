/*
 * Firmware selection, set by the build (cmake/fw.cmake).
 */

#pragma once

#ifndef FW_VERSION
#error "FW_VERSION not set; configure with -DFW=<id>"
#endif

#define FW_SINCE(v)  (FW_VERSION >= (v))
#define FW_BEFORE(v) (FW_VERSION < (v))

/* development kit firmware (TOOL, DEBUGGER) vs retail (CEX) */
#ifdef FW_TYPE_CEX
#define FW_DEVKIT 0
#else
#define FW_DEVKIT 1
#endif
