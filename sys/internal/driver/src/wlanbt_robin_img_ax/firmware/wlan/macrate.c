/*
 * Rates, frame durations and the MAC's timing registers: the control
 * response rate, the durations a frame carries, the transmit request a frame
 * is described by and the interframe timing. The filename is inferred; the
 * ROM routines it calls are named for their callers (robin/rom.h).
 */
#include <stdint.h>
#include "robin/baslot.h"
#include "robin/mac.h"
#include "robin/macrate.h"
#include "robin/rom.h"
#include "robin/runtime.h"
#include "robin/txring.h"

#define MAC_13C_FIXED_RESPONSE		0x00000001u	/* the response rate is not in bits 3-6 */
#define MAC_13C_SLOT_FLAG		0x00008000u
#define MAC_TX_CONTROL_RATE_FIXED	0x00001000u	/* the control word's rate field is set */
#define MAC_TX_CONTROL_RATE_MASK	0x00000f00u
#define MAC_TX_RATES			14
#define WLAN_RATE_LAST			12
#define WLAN_RATE_FIRST_OFDM		5
#define WLAN_RATE_LAST_CCK		3

/* The response rate the MAC is configured for (bits 3-6 of its configuration word); 0 when it picks one. */
unsigned int wlanMacResponseRate(void)
{
	uint32_t config = MAC_13C_CONFIG;
	unsigned int rate = 0;

	if (!(config & MAC_13C_FIXED_RESPONSE))
		rate = (config << 25) >> 28;
	return rate;
}

/*
 * The rate a control response goes out at: the rate itself when the set holds
 * one of the same class (CCK or OFDM) at or below it, otherwise whatever the ROM
 * chooses for the set.
 */
unsigned int wlanRateResponseSelect(unsigned int rate, uint32_t rateSet)
{
	uint8_t index;
	uint8_t lowest;

	if (rate == 0 || rate > WLAN_RATE_LAST)
		return rate;
	lowest = WLAN_RATE_LAST_CCK;
	if (rate <= WLAN_RATE_LAST_CCK)
		lowest = 0;
	for (index = rate; index > lowest; index--) {
		if ((1u << index) & rateSet)
			return rate;
	}
	return romLibManageContext(rate, rateSet);
}

/* The time of the acknowledgement for a frame sent at a rate, 0 for a frame that wants none. */
uint16_t wlanAckTime(uint32_t rateSet, unsigned int rate, uint32_t flags)
{
	uint16_t time = 0;

	if (!(flags & MAC_TX_FLAG_NO_ACK))
		time = romLibUpdateState((uint8_t)wlanRateResponseSelect(rate, rateSet));
	return time;
}

unsigned int wlanMacAckTime(void)
{
	return romLibUpdateState(wlanMacResponseRate());
}

/* The airtime of a frame plus twice its acknowledgement's (the duration it announces). */
uint16_t wlanFrameDuration(uint32_t rateSet, unsigned int rate, uint32_t flags, unsigned int length,
			   unsigned int wmm)
{
	uint16_t ack = 0;

	if (!(flags & MAC_TX_FLAG_NO_ACK))
		ack = wlanAckTime(rateSet, rate, flags) * 2;
	return romLibConfigureState(rate, flags, length, wmm) + ack;
}

uint16_t wlanFrameDurationMac(int a, int b, int c, int d, int e)
{
	int16_t ack = wlanMacAckTime();

	return romLibCheckState(a, b, c, d, e) + ack;
}

/* Programs the eight 64-bit timer slots with one value and the limit they share. */
void wlanMacTimersProgram(uint32_t low, uint32_t high, uint32_t limit)
{
	unsigned int offset;

	for (offset = 0xa0; offset < 0xe0; offset += 8) {
		MAC_REG(offset) = low;
		MAC_REG(offset + 4) = high;
	}
	MAC_REG(0xe0) = limit;
	romLibPrepareState(low, high, limit);
}

/* Sets the event masks the MAC runs with (the registers wlanMacEvents() acknowledges). */
void wlanMacEventMasksSet(void)
{
	MAC_REG(0x510) = 0x7bdf8703;
	MAC_REG(0x540) = 0x3fffff3d;
}

void wlanMacE80Bit2Set(void)
{
	MAC_E80_CONTROL |= 4;
}

void wlanMacE80Bit2Clear(void)
{
	MAC_E80_CONTROL &= ~4u;
}

/* Sets the rate field of a transmit control word (bits 8-12: the rate, at most 14, and its flag). */
void wlanTxControlRateSet(uint32_t *control, unsigned int rate)
{
	if (rate > MAC_TX_RATES)
		rate = MAC_TX_RATES;
	*control = (*control & ~0x1f00u) | (rate << 8) | MAC_TX_CONTROL_RATE_FIXED;
}

/*
 * Chooses the rate of a transmit (the control word's own when it holds one, the
 * RTS rate for a frame that sends an RTS, else the data rate) and records it
 * in the control word and as the request's first byte, one more than the rate.
 */
int wlanTxRateInit(uint32_t *control, struct WlanMacTxRequest *request, unsigned int dataRate,
		   unsigned int rtsRate)
{
	uint32_t word = *control;
	unsigned int rate = dataRate;

	if (word & MAC_TX_CONTROL_RATE_FIXED)
		rate = (word << 20) >> 28;
	else if (__aeabi_uread4(request->flags) & MAC_TX_FLAG_RTS)
		rate = rtsRate;
	if (rate > MAC_TX_RATES)
		rate = MAC_TX_RATES;
	word = (word & ~MAC_TX_CONTROL_RATE_MASK) | (rate << 8);
	*control = word;
	request->mode = rate + 1;
	request->reserved1[0] = 0;
	return 0;
}

/*
 * Starts a transmit request for a frame (its length first, then the header):
 * a group-addressed frame wants no acknowledgement, and rts asks for a
 * request to send before it.
 */
void wlanTxRequestInit(struct WlanMacTxRequest *request, const uint8_t *frame, int rts)
{
	const uint16_t *address;
	uint32_t flags;

	__aeabi_uwrite4((uint32_t)frame, request->frame);
	request->hwStatus = 0;
	request->hwRetries = 0;
	request->hwFlags = 0;
	__aeabi_uwrite4(0, request->flags);
	address = (const uint16_t *)(frame + 6);
	if ((address[0] == 0xffff && address[1] == 0xffff && address[2] == 0xffff) || (frame[6] & 1))
		__aeabi_uwrite4(MAC_TX_FLAG_NO_ACK, request->flags);
	if (rts != 0) {
		flags = __aeabi_uread4(request->flags);
		if (!(flags & MAC_TX_FLAG_NO_ACK))
			__aeabi_uwrite4((flags & 0xffffff5f) | MAC_TX_FLAG_RTS, request->flags);
	}
	flags = __aeabi_uread4(request->flags);
	__aeabi_uwrite4((flags & ~0x400u) | 0x1000, request->flags);
}

/* Ends the request's lifetime a number of time units (1024 microseconds; 0 is the longest) after now. */
void wlanTxLifetimeSet(struct WlanMacTxRequest *request, uint64_t now, int lifetime)
{
	int64_t units = lifetime != 0 ? lifetime : 0x7fff;
	uint64_t expiry = now + (uint64_t)(units << 10);

	__aeabi_uwrite4((uint32_t)expiry, &request->expiry[0]);
	__aeabi_uwrite4(expiry >> 32, &request->expiry[4]);
}

/*
 * The highest and lowest rate of each class in a rate set: the OFDM rates
 * (indexes 5 to 12) and the CCK rates (0 to 3); a class with none keeps
 * its first and last index.
 */
void wlanRateSetLimits(uint32_t *rateSet, uint32_t value, uint8_t *ofdmHigh, uint8_t *ofdmLow,
		       uint8_t *cckHigh, uint8_t *cckLow)
{
	unsigned int index;

	*rateSet = value;
	*ofdmHigh = WLAN_RATE_FIRST_OFDM;
	for (index = WLAN_RATE_LAST; index >= WLAN_RATE_FIRST_OFDM;) {
		if (*rateSet & (1u << index)) {
			*ofdmHigh = index;
			break;
		}
		index = (uint8_t)(index - 1);
	}
	*ofdmLow = WLAN_RATE_LAST;
	for (index = WLAN_RATE_FIRST_OFDM; index <= WLAN_RATE_LAST;) {
		if (*rateSet & (1u << index)) {
			*ofdmLow = index;
			break;
		}
		index = (uint8_t)(index + 1);
	}
	*cckHigh = 0;
	for (index = WLAN_RATE_LAST_CCK; index != 255;) {
		if (*rateSet & (1u << index)) {
			*cckHigh = index;
			break;
		}
		index = (uint8_t)(index - 1);
	}
	*cckLow = WLAN_RATE_LAST_CCK;
	for (index = 0; index <= WLAN_RATE_LAST_CCK;) {
		if (*rateSet & (1u << index)) {
			*cckLow = index;
			break;
		}
		index = (uint8_t)(index + 1);
	}
}

/* Sets bit 15 of the configuration word for enable 1 when the MAC has a response rate, else clears it. */
void wlanMacConfigBit15Update(int enable)
{
	unsigned int rate = wlanMacResponseRate();

	if (enable == 1 && rate != 0)
		MAC_13C_CONFIG |= MAC_13C_SLOT_FLAG;
	else
		MAC_13C_CONFIG &= ~MAC_13C_SLOT_FLAG;
}

void wlanMacSelectConfig(int mode)
{
	romLibSelectContext(mode);
	wlanMacConfigBit15Update(mode);
}

/* Programs the interframe timing registers for the slot time (long when longSlot is set). */
void wlanMacSlotConfigure(int longSlot)
{
	wlanMacSelectConfig(longSlot != 0 ? 0 : 1);
	if (((MAC_13C_CONFIG << 28) != 0)) {
		MAC_REG(0x1ac) = (MAC_REG(0x1ac) & 0xffff0000) + 0xb0;
		MAC_REG(0x1c8) = (MAC_REG(0x1c8) & ~0xff00u) + 0x7e00;
		MAC_REG(0x1cc) = (MAC_REG(0x1cc) & ~0xff00u) + 0x6f00;
		MAC_REG(0x1d0) = (MAC_REG(0x1d0) & ~0xff00u) + 0x6800;
	} else {
		MAC_REG(0x1ac) = (MAC_REG(0x1ac) & 0xffff0000) + 0x110;
		MAC_REG(0x1c8) = (MAC_REG(0x1c8) & ~0xff00u) + 0xde00;
		MAC_REG(0x1cc) = (MAC_REG(0x1cc) & ~0xff00u) + 0xcf00;
		MAC_REG(0x1d0) = (MAC_REG(0x1d0) & ~0xff00u) + 0xc800;
	}
	MAC_REG(0x1a8) = (MAC_REG(0x1a8) & 0xffff0000) + 0x160;
}

/*
 * Gives the ROM the response rate for each rate below fourteen: the first count
 * map to count, the rest to the response the rate set selects.
 */
void wlanRateResponseTableBuild(uint32_t rateSet, int count)
{
	int index;

	for (index = 0; index < count; index++)
		romLibConvertContext((uint8_t)index, count);
	while (index < MAC_TX_RATES) {
		romLibConvertContext((uint8_t)index, wlanRateResponseSelect((uint8_t)index, rateSet));
		index++;
	}
}

/*
 * Sets the rate fields of a transmit request: the flags of the rate's class,
 * the rate code the ROM gives for the index (bits 3-9), a fixed value in bits
 * 10-12 and the parameter nibble the ROM's hook gives for the rate. flag
 * and extra are stored as flags of the rate; fixed keeps bit 15 clear.
 */
void wlanTxRequestRateSet(struct WlanMacTxRequest *request, unsigned int rate, int flag,
			  int extra, int fixed)
{
	uint8_t low = request->rate & 0xff;
	uint8_t high;
	unsigned int code;
	uint8_t parameter;

	if (rate < 14)
		low &= ~1u;
	else
		low |= 1;
	low = (low & 0xfd) | ((flag & 1) << 1);
	if (rate < 22)
		low &= ~4u;
	else
		low |= 4;
	code = romLibHandleState(rate);
	request->rate = (request->rate & 0xfc07) | (low & 7) | ((code & 0x7f) << 3);
	high = ((request->rate >> 8) & 0xe3) + 8;
	high &= 0x9f;
	if (!fixed && ((request->rate >> 3) & 0x7f) != 0)
		high |= 0x80;
	else
		high &= 0x7f;
	request->rate = (request->rate & 0xff) | (high << 8);
	parameter = (request->reserved42[0] & 0xa0) | (wlanRateParameterHook(rate) & 0xf);
	parameter &= 0x7f;
	if (request->rate & 1)
		parameter |= (extra & 1) << 7;
	request->reserved42[0] = parameter;
}
