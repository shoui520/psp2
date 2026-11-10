/* libsas, the software audio synthesizer: the SDK's sas.h, as its users need it. */
#ifndef _SCE_SAS_SAS_H
#define _SCE_SAS_SAS_H

#include <scetypes.h>

/* Number of voices */

#define SCE_SAS_VOICE_MAX			(32)

#define SCE_SAS_RELEASE_VALID			(8)

/* Output modes */

#define SCE_SAS_OUTPUTMODE_STEREO		(0)	/* Stereo mode (2-channel output) */
#define SCE_SAS_OUTPUTMODE_MULTI		(1)	/* Multichannel mode (4-channel output) */

/* Effect types */

#define SCE_SAS_FX_TYPE_OFF			(-1)
#define SCE_SAS_FX_TYPE_ROOM			(0)
#define SCE_SAS_FX_TYPE_STUDIOA			(1)
#define SCE_SAS_FX_TYPE_STUDIOB			(2)
#define SCE_SAS_FX_TYPE_STUDIOC			(3)
#define SCE_SAS_FX_TYPE_HALL			(4)
#define SCE_SAS_FX_TYPE_SPACE			(5)
#define SCE_SAS_FX_TYPE_ECHO			(6)
#define SCE_SAS_FX_TYPE_DELAY			(7)
#define SCE_SAS_FX_TYPE_PIPE			(8)
#define SCE_SAS_FX_TYPE_COUNT			(9)

#endif
