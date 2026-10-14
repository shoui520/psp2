/*
 * SceSDbgSdio (os0:kd/sdbgsdio.skprx): SceSDbgSdioForKernel.
 *
 * Four byte-stream channels to the development host over the debug SDIO link.
 */

#pragma once

#include <psp2kern/types.h>

int sceSDbgSdioInit(void);

/* names unknown (NID) */
int SceSDbgSdioForKernel_2EC24C58(SceUInt32 channel, void *buf, SceSize size);	/* read */
int SceSDbgSdioForKernel_7876806C(SceUInt32 channel, const void *buf, SceSize size); /* write */
int SceSDbgSdioForKernel_356C6621(void);					/* free memblock */
