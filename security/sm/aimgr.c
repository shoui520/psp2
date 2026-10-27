/*
 * aimgr.c: Shared Console ID (keyring slot 0x509) and OpenPSID (keyring slot
 * 0x50d) accessors, product/sub-product target classification (SceSblAIMgr),
 * and big/little-endian byte-swap helpers for os0:sm secure modules.
 *
 * Internal function names are ours.
 */

#include "sm_common.h"

/* (name ours) */
void aimgrReadKeyringSlot32(uint32_t slotIndex, uint32_t *outWords8)
{
	uint32_t off = slotIndex * 0x20u;

	outWords8[0] = REG32(0xe0058000u + off);
	outWords8[1] = REG32(0xe0058004u + off);
	outWords8[2] = REG32(0xe0058008u + off);
	outWords8[3] = REG32(0xe005800cu + off);
	outWords8[4] = REG32(0xe0058010u + off);
	outWords8[5] = REG32(0xe0058014u + off);
	outWords8[6] = REG32(0xe0058018u + off);
	outWords8[7] = REG32(0xe005801cu + off);
}

/* (name ours) */
void aimgrClearConsoleId(void *buf16)
{
	memset(buf16, 0, 16u);
}

/* (name ours) */
void aimgrClearConsoleId2(void *buf16)
{
	memset(buf16, 0, 16u);
}

/* (name ours) */
void aimgrReadConsoleId(void *out16)
{
	uint32_t slot509[8];

	aimgrReadKeyringSlot32(0x509u, slot509);
	memcpy(out16, slot509, 16u);
}

/* (name ours) */
void aimgrReadConsoleIdAlias1(void *out16)
{
	aimgrReadConsoleId(out16);
}

/* (name ours) */
void aimgrReadConsoleIdAlias2(void *out16)
{
	aimgrReadConsoleId(out16);
}

/* (name ours) */
int aimgrIsTool(const uint8_t *cid)
{
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	return (prodCode == 0x0100u);
}

/* (name ours) */
int aimgrIsTest(const uint8_t *cid)
{
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	return (prodCode == 0x0101u);
}

/* (name ours) */
int aimgrIsDex(const uint8_t *cid)
{
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	return (prodCode == 0x0102u);
}

/* (name ours) */
int aimgrIsTestSub4(const uint8_t *cid)
{
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	if (prodCode == 0x0101u) {
		uint16_t subCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 6u));

		return (subCode == 4u);
	}
	return 0;
}

/* (name ours) */
int aimgrIsCexSub2(const uint8_t *cid)
{
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	if (prodCode == 0x0103u) {
		uint16_t subCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 6u));

		return (subCode == 2u);
	}
	return 0;
}

/* (name ours) */
uint16_t aimgrExtractProductSubCode(const uint8_t *cid, uint32_t rawWord2)
{
	(void)cid;
	return (uint16_t)htobe16Sm((uint16_t)(be32tohSm(rawWord2) >> 26));
}

/* (name ours) */
int aimgrIsCexSub16Factory36(const uint8_t *cid)
{
	uint16_t extraCode = aimgrExtractProductSubCode(cid, *(const uint32_t *)(const void *)(cid + 8u));
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	if (prodCode == 0x0103u &&
	    (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 6u)) == 0x0010u) {
		return ((uint16_t)be16tohSm(extraCode) == 0x0024u);
	}
	return 0;
}

/* (name ours) */
int aimgrIsRetailOrChord(const uint8_t *cid)
{
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	if (prodCode < 0x0103u || prodCode > 0x0111u)
		return 0;
	return (aimgrIsCexSub16Factory36(cid) == 0);
}

/* (name ours) */
int aimgrGetConsoleId(const uint8_t *rawCid, uint8_t *outCid)
{
	uint16_t f2;
	uint16_t f4;
	uint16_t f6;
	uint32_t w8;
	uint32_t wc;

	outCid[0] = rawCid[0];
	outCid[1] = rawCid[1];

	f2 = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(rawCid + 2u));
	outCid[2] = (uint8_t)f2;
	outCid[3] = (uint8_t)(f2 >> 8);

	f4 = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(rawCid + 4u));
	outCid[4] = (uint8_t)f4;
	outCid[5] = (uint8_t)(f4 >> 8);

	f6 = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(rawCid + 6u));
	outCid[6] = (uint8_t)f6;
	outCid[7] = (uint8_t)(f6 >> 8);

	w8 = *(const uint32_t *)(const void *)(rawCid + 8u);
	outCid[8] = (uint8_t)w8;
	outCid[9] = (uint8_t)(w8 >> 8);
	outCid[10] = (uint8_t)(w8 >> 16);
	outCid[11] = (uint8_t)(w8 >> 24);

	wc = *(const uint32_t *)(const void *)(rawCid + 12u);
	outCid[12] = (uint8_t)wc;
	outCid[13] = (uint8_t)(wc >> 8);
	outCid[14] = (uint8_t)(wc >> 16);
	outCid[15] = (uint8_t)(wc >> 15 ? (wc >> 24) : (wc >> 24));
	return 0;
}

/* (name ours) */
int aimgrGetOpenPsId(uint32_t unused, void *outOpenPsId16)
{
	uint32_t slot50d[8];

	(void)unused;
	aimgrReadKeyringSlot32(0x50du, slot50d);
	memcpy(outOpenPsId16, slot50d, 16u);
	return 0;
}

/* (name ours) */
int aimgrIsDolceHardware(void)
{
	uint8_t b2;

	(void)REG8(0xe0062360u);
	(void)REG8(0xe0062361u);
	b2 = REG8(0xe0062362u);
	(void)REG8(0xe0062363u);

	return (b2 == (uint8_t)'p' || b2 == (uint8_t)'r' || b2 == (uint8_t)'Q');
}

/* (name ours) */
int aimgrIsNotDolceHardware(void)
{
	return (aimgrIsDolceHardware() == 0);
}

/* (name ours) */
int aimgrIsGenuineVshTarget(const uint8_t *cid)
{
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	if ((prodCode == 0x0100u &&
	     (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 6u)) == 0x0408u) ||
	    ((uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 6u)) & 0x0200u) != 0u) {
		return 1;
	}
	if (aimgrIsCexSub16Factory36(cid) == 0)
		return 0;
	return (aimgrIsDolceHardware() != 0);
}

/* (name ours) */
int aimgrIsValidProductTarget(const uint8_t *cid)
{
	uint16_t prodCode = (uint16_t)be16tohSm(*(const uint16_t *)(const void *)(cid + 4u));

	if (prodCode < 0x0100u || prodCode > 0x0111u || aimgrIsGenuineVshTarget(cid) != 0) {
		if (aimgrIsCexSub16Factory36(cid) == 0)
			return 0;
		return (aimgrIsNotDolceHardware() != 0);
	}
	return 1;
}

/* (name ours) */
uint16_t htole16Sm(uint16_t v)
{
	return v;
}

/* (name ours) */
uint16_t le16tohSm(uint16_t v)
{
	return v;
}

/* (name ours) */
uint32_t htole32Sm(uint32_t v)
{
	return v;
}

/* (name ours) */
uint32_t le32tohSm(uint32_t v)
{
	return v;
}

/* (name ours) */
uint64_t *htole64Sm(uint64_t *out, const uint64_t *in)
{
	*out = *in;
	return out;
}

/* (name ours) */
uint64_t *le64tohSm(uint64_t *out, const uint64_t *in)
{
	*out = *in;
	return out;
}

/* (name ours) */
int16_t bswap16Sm(uint16_t v)
{
	union {
		uint8_t b[2];
		int16_t s;
	} u;

	u.b[0] = (uint8_t)(v >> 8);
	u.b[1] = (uint8_t)v;
	return u.s;
}

/* (name ours) */
int16_t htobe16Sm(uint16_t v)
{
	return bswap16Sm(v);
}

/* (name ours) */
int16_t be16tohSm(uint16_t v)
{
	return bswap16Sm(v);
}

/* (name ours) */
uint32_t bswap32Sm(uint32_t v)
{
	union {
		uint8_t b[4];
		uint32_t w;
	} u;

	u.b[0] = (uint8_t)(v >> 24);
	u.b[1] = (uint8_t)(v >> 16);
	u.b[2] = (uint8_t)(v >> 8);
	u.b[3] = (uint8_t)v;
	return u.w;
}

/* (name ours) */
uint32_t htobe32Sm(uint32_t v)
{
	return bswap32Sm(v);
}

/* (name ours) */
uint32_t be32tohSm(uint32_t v)
{
	return bswap32Sm(v);
}

/* (name ours) */
uint64_t *bswap64Sm(uint64_t *out, const uint64_t *in)
{
	uint8_t *b = (uint8_t *)out;
	uint32_t i;

	*out = *in;
	for (i = 0u; i < 4u; i++) {
		uint8_t tmp = b[i];

		b[i] = b[7u - i];
		b[7u - i] = tmp;
	}
	return out;
}

/* (name ours) */
uint64_t *htobe64Sm(uint64_t *out, const uint64_t *in)
{
	uint64_t val = *in;
	uint64_t res;

	bswap64Sm(&res, &val);
	*out = res;
	return out;
}

/* (name ours) */
uint64_t *be64tohSm(uint64_t *out, const uint64_t *in)
{
	uint64_t val = *in;
	uint64_t res;

	bswap64Sm(&res, &val);
	*out = res;
	return out;
}
