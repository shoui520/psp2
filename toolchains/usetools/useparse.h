/* USE instruction parser: text in the syntax of Imagination's disassembler to a USE_INST. */
#ifndef USEPARSE_H
#define USEPARSE_H

#include "sgxsupport.h"
#include "sgxdefs.h"
#include "use.h"
#include "useasm.h"
#include "usetab.h"

#define USEPARSE_ERROR_SIZE	256

/* resolves a label name to its number; returns 0 when the name is not a label */
typedef int (*UseParseLabelFn)(void *context, const char *name, IMG_UINT32 *label);

/*
 * Parses one instruction at instruction address `address` into `inst`. Returns 1, or 0 with
 * the reason in `error` (USEPARSE_ERROR_SIZE bytes).
 */
int UseParseInstruction(PCSGX_CORE_DESC target, const char *text, IMG_UINT32 address,
			PUSE_INST inst, UseParseLabelFn label, void *labelContext, char *error);

#endif
