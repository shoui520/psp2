/* Copy engines of the SoC used for moving frame data. */
#ifndef ROBIN_DMA_H
#define ROBIN_DMA_H

#include <stdint.h>

typedef void (*WlanDmaCompletionHook)(void);

extern WlanDmaCompletionHook wlanDmaCompletionHook;

void wlanDmaMoveForward(uint8_t *destination, const uint8_t *source, uint32_t length);
void wlanDmaMoveBackward(uint8_t *destination, const uint8_t *source, uint32_t length);
void wlanDmaStartCopyB(uint8_t *destination, const uint8_t *source, uint32_t length);

#endif /* ROBIN_DMA_H */
