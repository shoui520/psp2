/*
 * I2cMemIf: the I2C bus to the sensors, with DMA.
 */
#ifndef I2CMEMIF_H
#define I2CMEMIF_H

#include "motion.h"

class I2cMemIf {
public:
	void DmaCallbackSend();
	void DmaCallbackReceive();
	void IsrError();
};

#endif
