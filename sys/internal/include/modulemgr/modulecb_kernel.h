/*
 * The module control block as kernel code outside modulemgr sees it (Sony's
 * modulemgr/modulecb_kernel.h): DTrace's struct modctl is the module's
 * SceModuleCB.
 */
#pragma once

#include <modulemgr/modulemgr_control.h>

#define modctl SceModuleCB

static inline const char *ModuleGetModuleName(const SceModuleCB *module)
{
	return module->name;
}
