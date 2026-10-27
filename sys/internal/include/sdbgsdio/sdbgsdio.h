/*
 * SceSDbgSdio (os0:kd/sdbgsdio.skprx): SceSDbgSdioForKernel.
 *
 * Four byte-stream channels to the development host over the debug SDIO link.
 */

#pragma once

#include <scetypes.h>

int sceSDbgSdioInit(void);

/* names unknown (NID) */
int sceSDbgSdioRead(SceUInt32 channel, void *buf, SceSize size);	/* read */
int sceSDbgSdioWrite(SceUInt32 channel, const void *buf, SceSize size); /* write */
int sceSDbgSdioFini(void);					/* free memblock */
