/*
 * module.cpp: SceLibXml module entry points (0x81000000-0x8100000F).
 */
#include "libSceXml_internal.h"

extern "C" {

int module_start(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	return 0;
}

int module_stop(SceSize arglen, const void *argp)
{
	(void)arglen;
	(void)argp;
	bxceDeleteDefaultAllocator();
	return 0;
}

} /* extern "C" */
