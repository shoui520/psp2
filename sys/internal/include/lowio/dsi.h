/*
 * SceLowio (os0:kd/lowio.skprx): SceDsiForDriver, the DSI's exports, each declared once with its
 * definition as the prototype.
 */
#pragma once

#include <scetypes.h>
#include <stdint.h>

int SceDsiForDriver_6F8029A1(unsigned head);
int SceDsiForDriver_F2921E29(unsigned head, unsigned output);
int SceDsiForDriver_C2E85919(unsigned head, unsigned start);	/* vitasdk: ksceDsiStartDisplay */

int SceDsiForDriver_114D1413(unsigned head);	/* vitasdk: ksceDsiDisableHead */
/* vitasdk: ksceDsiDcsRead */
int SceDsiForDriver_3FB0DF1F(unsigned head, int command, void *data, unsigned size);
int SceDsiForDriver_4DF9E924(unsigned head);	/* vitasdk: ksceDsiGetPixelClock */
int SceDsiForDriver_5BE5AA9B(unsigned head);	/* vitasdk: ksceDsiEnableHead */
int SceDsiForDriver_7640F607(unsigned head);	/* vitasdk: ksceDsiSendBlankingPacket */
/* vitasdk: ksceDsiSetLanesAndPixelSize */
int SceDsiForDriver_78E6E3CF(unsigned head, unsigned lanes, unsigned pixelSize);
int SceDsiForDriver_8610B795(unsigned head, uint32_t numerator, uint32_t denominator);
/* vitasdk: ksceDsiGenericShortWrite */
int SceDsiForDriver_89C00D2F(unsigned head, int parameter0, int parameter1);
int SceDsiForDriver_97BFEA76(unsigned head, unsigned vic);	/* vitasdk: ksceDsiSetVic */
/* vitasdk: ksceDsiGenericReadRequest */
int SceDsiForDriver_98120684(unsigned head, int parameter, void *data, unsigned size);
/* vitasdk: ksceDsiGetVicResolution */
int SceDsiForDriver_B3A70C05(unsigned vic, SceUInt32 *width, SceUInt32 *height);
/* vitasdk: ksceDsiDcsShortWrite */
int SceDsiForDriver_BA6BC89F(unsigned head, int command, int parameter);
