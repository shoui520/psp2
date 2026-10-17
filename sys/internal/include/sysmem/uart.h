/*
 * SceUartForKernel (os0:kd/sysmem.skprx, serial.c): the UART, each export declared once, with
 * sysmem's definition as the prototype.
 */
#pragma once

#include <scetypes.h>

#ifdef __cplusplus
extern "C" {
#endif

int SceUartForKernel_16780BC3(int port);	/* vitasdk: ksceUartReadAvailable */
int SceUartForKernel_430C48F1(int port, int c);	/* vitasdk: ksceUartWrite */
int SceUartForKernel_4C02AA05(int port);	/* vitasdk: ksceUartInit */
int SceUartForKernel_4E97D3AD(int port);	/* vitasdk: ksceUartRead */
int SceUartForTZS_3AFD5E71(int port, int c);

#ifdef __cplusplus
}
#endif
