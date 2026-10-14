/*
 * The CELP encoder's excitation search (0x8ca638, with its helpers 0x8c9ad0,
 * 0x856d8a, 0x854dd8 and 0x854ec4).
 *
 * PLACEHOLDER: not yet converted. The encoder cannot produce the pulse
 * positions, the signs and the gain index of a subframe until this function
 * is written; it leaves them as the state has them (cleared by the setup).
 */
#include "celp.h"

void celpEncSearchExcitation(CelpEncoderState *state)
{
	(void)state;
}
