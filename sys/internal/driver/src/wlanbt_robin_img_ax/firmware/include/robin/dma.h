/* Copy engines of the SoC used for moving frame data. */
#ifndef ROBIN_DMA_H
#define ROBIN_DMA_H

#include <stdint.h>

/* Called once engine B's copy is started (0x54dfc); socEventHookInit() sets it to sysWaitForBbpCommand(). */
typedef volatile uint32_t *(*WlanDmaCompletionHook)(void);

extern WlanDmaCompletionHook wlanDmaCompletionHook;

void wlanDmaMoveForward(uint8_t *destination, const uint8_t *source, uint32_t length);
void wlanDmaMoveBackward(uint8_t *destination, const uint8_t *source, uint32_t length);
void wlanDmaStartCopyB(uint8_t *destination, const uint8_t *source, uint32_t length);

#endif /* ROBIN_DMA_H */
