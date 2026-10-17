/*
 * SceExcpmgr: handler-chain registration, the default/panic handler C parts
 * and the saved-register report. Shared by os0:kd/excpmgr.skprx and the
 * secure-world slb2:kbl/SceExcpmgr, which Sony built from one excpmgr.c (the
 * debug records of both carry the same source lines).
 *
 * This header holds function definitions: include it from exactly one C file
 * of a module, after providing
 *
 *	g_excpHandlers			the module's ExcpHandlerTable
 *	_sceKernelPanicHandlerCommon()	a declaration of the module's panic report
 *
 * The kernel import names used here are the os0 ones; the secure-world module
 * maps them to its ForTZS imports before including.
 */

#pragma once

#include <psp2kern/types.h>
#include <sysmem/debug.h>
#include <excpmgr/excpmgr.h>
#include <excpmgr/excpmgr_cpu.h>

/*
 * Sony's source file for the debug records below is excpmgr.c, the file every
 * includer is; restored at the end of this header.
 */
#pragma push_macro("__FILE_NAME__")
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wbuiltin-macro-redefined"
#undef __FILE_NAME__
#define __FILE_NAME__ "excpmgr.c"
#pragma GCC diagnostic pop

#include "excpmgr_handler_cpu.h"
#include "excpmgr_handler_register.h"
#include "excpmgr_handler_report.h"
#include "excpmgr_handler_default.h"

#pragma pop_macro("__FILE_NAME__")
