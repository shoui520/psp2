/*
 * The MAC's second cipher engine, the one in the register file at
 * 0x9000a000 (the first, WEP and CCMP for received and sent frames, is
 * robin/wep.h). It does CCM for CCMP (and SMS4 for WAPI, robin/wapi.h) on
 * frames the firmware itself encrypts or decrypts, and AES-CMAC is built
 * from the AES block function in SQRAM. No Marvell source names the
 * registers; the names say what the firmware writes.
 */
#ifndef ROBIN_CCM_H
#define ROBIN_CCM_H

#include <stdint.h>
#include "robin/soc.h"
#include "robin/wlan.h"

/* Register block of the engine; `block` is the byte offset of the block (always 0 here). */
#define CCM_BASE			0x9000a000u
#define CCM_REG(block, offset)		REG32(CCM_BASE + (block) + (offset))
#define CCM_CONTROL(block)		CCM_REG(block, 0x00)	/* bit 0: start; bits 8-9: input pending; bit 3: failed */
#define CCM_MODE(block)			CCM_REG(block, 0x04)	/* the length in bits 16-31, the mode below */
#define CCM_KEY(block)			((volatile uint32_t *)(CCM_BASE + (block) + 0x08))
#define CCM_KEY2(block)			((volatile uint32_t *)(CCM_BASE + (block) + 0x18))
#define CCM_SOURCE(block)		CCM_REG(block, 0x38)
#define CCM_DESTINATION(block)		CCM_REG(block, 0x3c)
#define CCM_BLOCK0(block)		((volatile uint32_t *)(CCM_BASE + (block) + 0x40))	/* B0 */
#define CCM_BLOCK1(block)		((volatile uint32_t *)(CCM_BASE + (block) + 0x50))	/* first AAD block */
#define CCM_BLOCK2(block)		((volatile uint8_t *)(CCM_BASE + (block) + 0x60))	/* second AAD block */
#define CCM_BLOCK3(block)		((volatile uint8_t *)(CCM_BASE + (block) + 0x80))	/* third AAD block (WAPI) */
#define CCM_STATUS(block)		CCM_REG(block, 0x70)

#define CCM_CONTROL_START		0x01u
#define CCM_CONTROL_FAILED		0x08u
#define CCM_CONTROL_INPUT_PENDING	0x300u

/* Bits of CCM_MODE below the length. */
#define CCM_MODE_CCMP_ENCRYPT		0x400u
#define CCM_MODE_CCMP_DECRYPT		0x408u
#define CCM_MODE_HOLD			0x010u	/* set by the start hook */
#define CCM_MODE_INPUT_HOLD		0x400u	/* set and cleared around a transfer */

/* The engine's interrupt line, masked while the crypto test (sqram/crypto.c) polls the engine. */
#define SOC_IRQ_CCM_ENGINE		0x00020000u

/* CRYPTO_MODE (robin/wep.h) bit that hands the engine over to this block. */
#define CRYPTO_MODE_CCM_ENGINE		0x8000u

/* Parameters the start-up code fixes (wlanCcmSetup()). */
extern uint32_t wlanCcmAadLength;	/* 22: the AAD without the QoS control */
extern uint32_t wlanCcmAadLengthQos;	/* 24 */
extern uint32_t wlanCcmIvLow;		/* 4 */
extern uint32_t wlanCcmIvHigh;		/* 4: the 8-byte CCMP header is these two */
extern void (*wlanCcmHookPrepare)(uint8_t *data, const uint32_t *key, uint32_t length,
				  const WlanWirelessFrame *frame);
extern void (*wlanCcmHookStart)(void);
extern uint32_t wlanCcmPollLoops;	/* delay loops between two status reads */

void wlanCcmSetup(void);
void wlanCcmHooksInit(void);
void wlanCcmPrepare(uint8_t *data, const uint32_t *key, uint32_t length,
		    const WlanWirelessFrame *frame);
void wlanCcmStart(void);
void wlanCcmInputHold(void);
void wlanCcmInputRelease(void);
uint32_t wlanCcmWaitSet(uint32_t mask, uint32_t block);
uint32_t wlanCcmWaitClear(uint32_t mask, uint32_t block);

int wlanCcmDecrypt(const WlanWirelessFrame *frame, uint8_t *data, uint32_t length,
		   const uint32_t *key, uint32_t priority, uint32_t block);
int wlanCcmEncrypt(const WlanWirelessFrame *frame, uint32_t length, const uint32_t *key,
		   uint8_t *source, uint32_t priority, uint32_t block);

int mrvl_aes_128(const uint8_t *key, const uint8_t *input, uint8_t *output);
void xor_128(const uint8_t *left, const uint8_t *right, uint8_t *output);
void leftshift_onebit(const uint8_t *input, uint8_t *output);
void generate_subkey(const uint8_t *key, uint8_t *subkey1, uint8_t *subkey2);
void padding(const uint8_t *input, uint8_t *output, int length);


#endif /* ROBIN_CCM_H */
