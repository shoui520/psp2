/*
 * SceMtpIfDriver (os0:kd/bootimage/mtpif): SceMtpIfForDriver, the MTP
 * interface between the USB transport (SceUsbMtp) and its kernel clients.
 * Names are not known; the comments give the roles.
 */
#pragma once

#include <scetypes.h>
#include <usbmtp/usbmtp.h>

int SceMtpIfForDriver_92CD107E(const void *port);	/* reset */
int SceMtpIfForDriver_117F7519(const void *port, void *command);	/* receive a command */
int SceMtpIfForDriver_5766D257(const void *port, const void *response);	/* send the response */
/* send data */
int SceMtpIfForDriver_EDC3AB26(const void *port, void *buffer,
			     SceUInt32 size, SceUInt32 totalSize);
int SceMtpIfForDriver_33F1826F(const void *port, MtpIfTransfer *transfer);
int SceMtpIfForDriver_F0FA2D86(const void *port, void *buffer, SceUInt32 size);	/* receive data */
int SceMtpIfForDriver_27D40294(const void *port, MtpIfTransfer *transfer);
int SceMtpIfForDriver_B5E78858(const void *port, const SceUInt32 *command, SceUInt32 *event);
int SceMtpIfForDriver_F1EC5992(const void *port);
int SceMtpIfForDriver_05408B5B(const void *port, SceUInt32 timeout);	/* wait to connect */
int SceMtpIfForDriver_2D9318E1(const void *port);	/* connected? */
int SceMtpIfForDriver_8D16B997(const void *port, int phase);	/* change phase */
const void *SceMtpIfForDriver_16D7DBCC(int port);	/* the port (1) */
int sceMtpIfStartPort(const void *port);	/* start the driver */
int SceMtpIfForDriver_E6453F21(const void *port);	/* stop the driver */
int SceMtpIfForDriver_CB685AC0(int port);
int SceMtpIfForDriver_FDF8C3C0(int port);
