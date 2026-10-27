/*
 * keyring.c: Keyring slot read/write helpers, SNVS key seed derivation via
 * Bigmac keyslot 0x214, and SNVS management sector initialization
 * (0x8023a2..0x8026a7).
 *
 * Function and variable names here are ours (descriptive).
 */

#include "second_loader.h"

/* ============================================================================
 * .rodata (0x80bf14..0x80bf73)
 * ============================================================================ */

static const uint8_t s_keyringSeed514Cipher[32] = {
	0xadu, 0x20u, 0x89u, 0xbfu, 0x8bu, 0x38u, 0x66u, 0x32u,
	0xdcu, 0x08u, 0xefu, 0xa2u, 0x98u, 0xccu, 0x1du, 0xaau,
	0xf3u, 0x93u, 0x7fu, 0x2eu, 0x92u, 0xdbu, 0xf5u, 0x12u,
	0x7cu, 0x17u, 0xdau, 0xfbu, 0xcfu, 0x82u, 0x66u, 0xaeu,
};

static const uint8_t s_keyringSeed514Iv[16] = {
	0x69u, 0xebu, 0xc4u, 0x82u, 0x4bu, 0x4eu, 0x69u, 0x91u,
	0x7eu, 0xd8u, 0xbfu, 0x56u, 0x98u, 0xffu, 0xadu, 0x31u,
};

static const uint8_t s_keyringSeed515Cipher[32] = {
	0xa5u, 0xd4u, 0x21u, 0xfeu, 0xd4u, 0x87u, 0xe8u, 0x7eu,
	0xdfu, 0x0cu, 0xdfu, 0xd9u, 0x1bu, 0x6bu, 0xafu, 0x9eu,
	0xa8u, 0x1cu, 0x1eu, 0x83u, 0x1bu, 0x56u, 0x14u, 0xddu,
	0xc3u, 0x7bu, 0x45u, 0xc1u, 0x0au, 0xadu, 0x06u, 0xe7u,
};

static const uint8_t s_keyringSeed515Iv[16] = {
	0xf8u, 0xf6u, 0x45u, 0xf5u, 0x14u, 0x39u, 0xcfu, 0x7eu,
	0x74u, 0xebu, 0xb2u, 0x0eu, 0x3eu, 0x29u, 0x47u, 0x47u,
};

/* ============================================================================
 * .bss (0x80cd60..0x80cd6b)
 * ============================================================================ */

static uint32_t s_snvsInitialized;
static uint32_t s_snvsBit1Enabled;
static uint32_t s_snvsSd0Enabled;

/* ============================================================================
 * Forward Declarations (internal to keyring.c)
 * ============================================================================ */

static void keyringInitHook(void);
static void keyringCleanupHook(void);
static int deriveKeyringSlots514And515(void);
static int initSnvsBaseSlotsThunk(void);

/* ============================================================================
 * Functions (0x8023a2..0x8026a7)
 * ============================================================================ */

/* 0x8023a2 */
int initSnvsAndKeyringState(int tryResumeContext)
{
	uint8_t dummyCtx;
	uint8_t mgmtFlags[4];
	uint8_t mgmtStatus[4];
	int attempts;
	int status;

	keyringInitHook();
	status = initSnvsBaseSlotsThunk();
	if (status != 0)
		goto done;
	status = deriveKeyringSlots514And515();
	if (status != 0)
		goto done;

	attempts = 2 - (tryResumeContext == 0);
	for (;;) {
		status = loadAndSetupResumeOrColdBootKeys(&dummyCtx, tryResumeContext);
		if (status != 0)
			goto done;
		status = noopResumeHook();
		if (status != 0)
			goto done;

		slMemset(mgmtFlags, 0, 8u);
		status = readSnvsManagementSector(mgmtFlags, mgmtStatus);
		if (status == 0)
			break;

		if (attempts == 2) {
			debugLogError(1, 'M', (uint32_t)status, 0);
			attempts = 1;
		} else {
			attempts--;
			if (attempts == 0)
				goto done;
		}
		tryResumeContext = 0;
	}

	if ((mgmtFlags[0] & 1u) != 0)
		s_snvsSd0Enabled = 1u;
	if ((mgmtFlags[0] & 2u) != 0)
		s_snvsBit1Enabled = 1u;
	s_snvsInitialized = 1u;
	writeKeyringSlotBytes(0x50bu, mgmtFlags, 8u);

done:
	keyringCleanupHook();
	return status;
}

/* 0x802486 */
int isSnvsSd0FlagEnabled(void)
{
	return (int)s_snvsSd0Enabled;
}

/* 0x80248c */
int isSnvsBit1FlagEnabled(void)
{
	return (int)s_snvsBit1Enabled;
}

/* 0x802492 */
int isSnvsInitialized(void)
{
	return (int)s_snvsInitialized;
}

/* 0x802498 */
void writeKeyringSlotWords(uint32_t slot, const uint32_t *words)
{
	KEYRING_CTRL->data = words[0];
	KEYRING_CTRL->reserved0[0] = words[1];
	KEYRING_CTRL->reserved0[1] = words[2];
	KEYRING_CTRL->reserved0[2] = words[3];
	KEYRING_CTRL->reserved0[3] = words[4];
	KEYRING_CTRL->reserved0[4] = words[5];
	KEYRING_CTRL->reserved0[5] = words[6];
	KEYRING_CTRL->reserved0[6] = words[7];
	KEYRING_CTRL->protection = slot & 0xffffu;
}

/* 0x80250c */
int writeKeyringSlotBytes(uint32_t slot, const void *data, uint32_t size)
{
	uint32_t slotBuf[8];

	if (size - 1u >= 32u)
		return (int)0x800f0016;

	slMemcpy(slotBuf, data, size);
	if (size != 32u)
		slMemset((uint8_t *)slotBuf + size, 0, 32u - size);
	writeKeyringSlotWords(slot, slotBuf);
	return 0;
}

/* 0x80255c */
void initKeyringBaseSeeds(void)
{
	uint32_t zeroSlot[8];

	slMemset(zeroSlot, 0, sizeof(zeroSlot));
	writeKeyringSlotWords(0x516u, zeroSlot);
}

/* 0x802580 */
void readKeyringSlotWords(unsigned int slot, uint32_t *dst)
{
	volatile uint32_t *src = &KEYRING_SLOTS[slot].words[0];

	dst[0] = src[0];
	dst[1] = src[1];
	dst[2] = src[2];
	dst[3] = src[3];
	dst[4] = src[4];
	dst[5] = src[5];
	dst[6] = src[6];
	dst[7] = src[7];
}

/* 0x8025ca */
int decryptSeedWithKeyslot214(void *dst32, const void *src32, const void *iv16)
{
	BigmacContext ctx;
	uint32_t keyslot = 0x214u;
	uint8_t ivCopy[16];
	int status;

	bigmacInitContext(&ctx);
	bigmacSetChannelIndex(&ctx, 0);
	slMemcpy(ivCopy, iv16, sizeof(ivCopy));
	status = bigmacAes128CbcDecryptKeyslot(dst32, src32, 32u, &keyslot, ivCopy, &ctx);
	if (status == 0)
		status = bigmacWaitChannel(&ctx);
	return status;
}

/* 0x80263e */
void keyringNoop1(void)
{
}

/* 0x802640 */
static void keyringInitHook(void)
{
}

/* 0x802642 */
void keyringNoop2(void)
{
}

/* 0x802644 */
static void keyringCleanupHook(void)
{
}

/* 0x802646 */
static int deriveKeyringSlots514And515(void)
{
	uint8_t seed514[32];
	uint8_t seed515[32];
	int status;

	slMemset(seed514, 0, sizeof(seed514));
	status = decryptSeedWithKeyslot214(seed514, s_keyringSeed514Cipher, s_keyringSeed514Iv);
	if (status != 0)
		return status;
	status = writeKeyringSlotBytes(0x514u, seed514, sizeof(seed514));
	if (status != 0)
		return status;

	slMemset(seed515, 0, sizeof(seed515));
	status = decryptSeedWithKeyslot214(seed515, s_keyringSeed515Cipher, s_keyringSeed515Iv);
	if (status != 0)
		return status;
	return writeKeyringSlotBytes(0x515u, seed515, sizeof(seed515));
}

/* 0x8026a4 */
static int initSnvsBaseSlotsThunk(void)
{
	return initSnvsBaseKeyringSlots();
}
