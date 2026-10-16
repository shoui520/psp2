/*
 * SceMtpIfForDriver as its kernel clients call it (exports of
 * os0:kd/bootimage/mtpif, see its source/trans.c). Names are not known;
 * the roles are.
 */
#pragma once

#include <psp2kern/types.h>

const void *SceMtpIfForDriver_16D7DBCC(int port);		/* the port (1) */
int SceMtpIfForDriver_5245D237(const void *port);		/* start the driver */
int SceMtpIfForDriver_E6453F21(const void *port);		/* stop the driver */
int SceMtpIfForDriver_2D9318E1(const void *port);		/* connected? */
int SceMtpIfForDriver_92CD107E(const void *port);		/* reset */
int SceMtpIfForDriver_05408B5B(const void *port, SceUInt32 timeout);	/* wait to connect */
int SceMtpIfForDriver_8D16B997(const void *port, int phase);	/* change phase */
int SceMtpIfForDriver_117F7519(const void *port, void *command);	/* receive a command */
int SceMtpIfForDriver_5766D257(const void *port, const void *response);	/* send the response */
int SceMtpIfForDriver_EDC3AB26(const void *port, const void *buffer, SceUInt32 size,
			       SceUInt32 total);		/* send data */
int SceMtpIfForDriver_F0FA2D86(const void *port, void *buffer, SceUInt32 size);	/* receive data */
