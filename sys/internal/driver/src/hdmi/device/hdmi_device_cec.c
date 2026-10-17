/*
 * SceHdmi device layer: the transmitter's CEC controller (Sony's
 * device/hdmi_device_cec.c). Every register cec.c's protocol code needs is a
 * field of cecDevRegisters, read and written by _getReg/_setReg; the
 * interrupt handler turns the CEC interrupts into the "SceHdmiCECDev" events
 * cecDevTx waits for and into the callbacks cec.c registered. Internal names
 * are ours unless a comment says otherwise.
 */
#include <psp2kern/types.h>
#include <psp2kern/kernel/threadmgr.h>
#include <psp2kern/kernel/cpu/spinlock.h>
#include <fw.h>
#include <sysmem/debug.h>
#include "../hdmi_internal.h"

int sceKernelDelayThread(SceUInt32 usec);
int ScePervasiveForDriver_26B51075(void);	/* the CEC clock on */
int ScePervasiveForDriver_D185D235(void);	/* the CEC clock off */

/* The transmitter's register maps (I2C slave addresses) */
#define CEC_SLAVE_MAIN		0x7a
#define CEC_SLAVE_CEC		0x7c

#define HDMI_INTR_MASK_CEC	0x80fe

/* the event flag's bits: how a transmission ended */
#define CEC_DEV_EVENT_TX_READY		0x1
#define CEC_DEV_EVENT_TX_RETRY_TIMEOUT	0x2
#define CEC_DEV_EVENT_TX_ARBITRATION	0x4

/* cecDevIntrHandler's status: the main map's 0x93 and 0x97 */
#define CEC_DEV_INTR_MAIN		0x0000ff
#define CEC_DEV_INTR_RX_READY1		0x010000
#define CEC_DEV_INTR_RX_READY2		0x020000
#define CEC_DEV_INTR_RX_READY3		0x040000
#define CEC_DEV_INTR_RX_READY		0x070000
#define CEC_DEV_INTR_TX_RETRY_TIMEOUT	0x080000
#define CEC_DEV_INTR_TX_ARBITRATION	0x100000
#define CEC_DEV_INTR_TX_READY		0x200000

#define CEC_DEV_RX_BUFFERS	3
#define CEC_DEV_FRAME_SIZE	16
#define CEC_DEV_TX_TIMEOUT	1000000	/* us */

/* CecDevRegister.access */
#define CEC_REG_READ	0x1
#define CEC_REG_WRITE	0x2

/* A register, or a field (bits msb..lsb) of one */
typedef struct CecDevRegister {
	SceUInt8 id;		/* 0x00: its index in cecDevRegisters */
	SceUInt8 slave;		/* 0x01 */
	SceUInt8 reg;		/* 0x02 */
	SceUInt8 msb;		/* 0x03 */
	SceUInt8 lsb;		/* 0x04 */
	SceUInt8 size;		/* 0x05: bytes */
	SceUInt8 access;	/* 0x06: CEC_REG_* */
	SceUInt8 isField;	/* 0x07 */
} CecDevRegister;

enum CecDevRegisterId {
	CEC_REG_POWER_DOWN,		/* 0x00 */
	CEC_REG_RESERVED0,
	CEC_REG_RESERVED1,
	CEC_REG_CLOCK_ENABLE,
	CEC_REG_CLOCK_COUNT,
	CEC_REG_RESERVED2,
	CEC_REG_RESERVED3,
	CEC_REG_TX_FRAME,
	CEC_REG_TX_FRAME_LENGTH,	/* 0x08 */
	CEC_REG_TX_MODE,
	CEC_REG_TX_ENABLE,
	CEC_REG_TX_RETRY,
	CEC_REG_TX_RETRY_SFT,
	CEC_REG_TX_SFT_NEW,
	CEC_REG_TX_SFT_NEXT,
	CEC_REG_TX_LOW_DRIVE_COUNT,
	CEC_REG_TX_NACK_COUNT,			/* 0x10 */
	CEC_REG_RX1_FRAME,
	CEC_REG_RX1_FRAME_LENGTH,
	CEC_REG_RESERVED4,
	CEC_REG_RX3_ORDER,
	CEC_REG_RX2_ORDER,
	CEC_REG_RX1_ORDER,
	CEC_REG_RX2_FRAME,
	CEC_REG_RX2_FRAME_LENGTH,	/* 0x18 */
	CEC_REG_RX3_FRAME,
	CEC_REG_RX3_FRAME_LENGTH,
	CEC_REG_RX_BUFFERS_ENABLE,
	CEC_REG_RX3_CLEAR,
	CEC_REG_RX2_CLEAR,
	CEC_REG_RX1_CLEAR,
	CEC_REG_LOG_ADDR_MASK,
	CEC_REG_RESERVED5,	/* 0x20 */
	CEC_REG_RESERVED6,
	CEC_REG_RESERVED7,
	CEC_REG_RESERVED8,
	CEC_REG_LOG_ADDR_1,
	CEC_REG_LOG_ADDR_0,
	CEC_REG_LOG_ADDR_2,
	CEC_REG_CLOCK_DIVIDER,
	CEC_REG_POWER_MODE,		/* 0x28 */
	CEC_REG_RESERVED9,
	CEC_REG_SOFT_RESET,
	CEC_REG_TIMING0,
	CEC_REG_TIMING1,
	CEC_REG_TIMING2,
	CEC_REG_TIMING3,
	CEC_REG_TIMING4,
	CEC_REG_TIMING5,		/* 0x30 */
	CEC_REG_TIMING6,
	CEC_REG_TIMING7,
	CEC_REG_TIMING8,
	CEC_REG_TIMING9,
	CEC_REG_TIMING10,
	CEC_REG_TIMING11,
	CEC_REG_TIMING12,
	CEC_REG_TIMING13,		/* 0x38 */
	CEC_REG_TIMING14,
	CEC_REG_RESERVED10,
	CEC_REG_TIMING15,
	CEC_REG_TIMING16,
	CEC_REG_TIMING17,
	CEC_REG_TIMING18,
	CEC_REG_MAIN_POWER_DOWN,
	CEC_REG_MAIN_HPD_OVERRIDE,		/* 0x40 */
	CEC_REG_INTR_STATUS,
	CEC_REG_INTR_TX_READY,
	CEC_REG_INTR_TX_ARBITRATION,
	CEC_REG_INTR_TX_RETRY_TIMEOUT,
	CEC_REG_INTR_RX_READY3,
	CEC_REG_INTR_RX_READY2,
	CEC_REG_INTR_RX_READY1,
	CEC_REG_COUNT,			/* 0x48 */
};

#define CEC_REG(_id, _slave, _reg, _msb, _lsb, _size, _access, _isField) \
	[_id] = { _id, _slave, _reg, _msb, _lsb, _size, _access, _isField }
#define RW	(CEC_REG_READ | CEC_REG_WRITE)
#define RO	CEC_REG_READ

static const CecDevRegister cecDevRegisters[CEC_REG_COUNT + 1] = {
	CEC_REG(CEC_REG_POWER_DOWN,		CEC_SLAVE_CEC, 0x05, 4, 4, 1, RW, 1),
	CEC_REG(CEC_REG_RESERVED0,			CEC_SLAVE_CEC, 0x6b, 1, 0, 1, RW, 1),
	CEC_REG(CEC_REG_RESERVED1,			CEC_SLAVE_CEC, 0x6c, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_CLOCK_ENABLE,		CEC_SLAVE_CEC, 0x6e, 7, 7, 1, RW, 1),
	CEC_REG(CEC_REG_CLOCK_COUNT,		CEC_SLAVE_CEC, 0x6e, 6, 0, 1, RW, 1),
	CEC_REG(CEC_REG_RESERVED2,		CEC_SLAVE_CEC, 0x6f, 7, 7, 1, RO, 1),
	CEC_REG(CEC_REG_RESERVED3,			CEC_SLAVE_CEC, 0x6f, 6, 0, 1, RO, 1),
	CEC_REG(CEC_REG_TX_FRAME,		CEC_SLAVE_CEC, 0x70, 0, 0, 16, RW, 0),
	CEC_REG(CEC_REG_TX_FRAME_LENGTH,	CEC_SLAVE_CEC, 0x80, 4, 0, 1, RW, 1),
	CEC_REG(CEC_REG_TX_MODE,		CEC_SLAVE_CEC, 0x81, 3, 1, 1, RW, 1),
	CEC_REG(CEC_REG_TX_ENABLE,		CEC_SLAVE_CEC, 0x81, 0, 0, 1, RW, 1),
	CEC_REG(CEC_REG_TX_RETRY,		CEC_SLAVE_CEC, 0x82, 6, 4, 1, RW, 1),
	CEC_REG(CEC_REG_TX_RETRY_SFT,			CEC_SLAVE_CEC, 0x82, 3, 0, 1, RW, 1),
	CEC_REG(CEC_REG_TX_SFT_NEW,		CEC_SLAVE_CEC, 0x83, 7, 4, 1, RW, 1),
	CEC_REG(CEC_REG_TX_SFT_NEXT,			CEC_SLAVE_CEC, 0x83, 3, 0, 1, RW, 1),
	CEC_REG(CEC_REG_TX_LOW_DRIVE_COUNT,		CEC_SLAVE_CEC, 0x84, 7, 4, 1, RO, 1),
	CEC_REG(CEC_REG_TX_NACK_COUNT,			CEC_SLAVE_CEC, 0x84, 3, 0, 1, RO, 1),
	CEC_REG(CEC_REG_RX1_FRAME,		CEC_SLAVE_CEC, 0x85, 0, 0, 16, RO, 0),
	CEC_REG(CEC_REG_RX1_FRAME_LENGTH,	CEC_SLAVE_CEC, 0x95, 4, 0, 1, RO, 1),
	CEC_REG(CEC_REG_RESERVED4,		CEC_SLAVE_CEC, 0x96, 6, 6, 1, RO, 1),
	CEC_REG(CEC_REG_RX3_ORDER,		CEC_SLAVE_CEC, 0x96, 5, 4, 1, RO, 1),
	CEC_REG(CEC_REG_RX2_ORDER,		CEC_SLAVE_CEC, 0x96, 3, 2, 1, RO, 1),
	CEC_REG(CEC_REG_RX1_ORDER,		CEC_SLAVE_CEC, 0x96, 1, 0, 1, RO, 1),
	CEC_REG(CEC_REG_RX2_FRAME,		CEC_SLAVE_CEC, 0x97, 0, 0, 16, RO, 0),
	CEC_REG(CEC_REG_RX2_FRAME_LENGTH,	CEC_SLAVE_CEC, 0xa7, 4, 0, 1, RO, 1),
	CEC_REG(CEC_REG_RX3_FRAME,		CEC_SLAVE_CEC, 0xa8, 0, 0, 16, RO, 0),
	CEC_REG(CEC_REG_RX3_FRAME_LENGTH,	CEC_SLAVE_CEC, 0xb8, 4, 0, 1, RO, 1),
	CEC_REG(CEC_REG_RX_BUFFERS_ENABLE,		CEC_SLAVE_CEC, 0xba, 3, 3, 1, RW, 1),
	CEC_REG(CEC_REG_RX3_CLEAR,		CEC_SLAVE_CEC, 0xba, 2, 2, 1, RW, 1),
	CEC_REG(CEC_REG_RX2_CLEAR,		CEC_SLAVE_CEC, 0xba, 1, 1, 1, RW, 1),
	CEC_REG(CEC_REG_RX1_CLEAR,		CEC_SLAVE_CEC, 0xba, 0, 0, 1, RW, 1),
	CEC_REG(CEC_REG_LOG_ADDR_MASK,	CEC_SLAVE_CEC, 0xbb, 6, 4, 1, RW, 1),
	CEC_REG(CEC_REG_RESERVED5,	CEC_SLAVE_CEC, 0xbb, 3, 3, 1, RW, 1),
	CEC_REG(CEC_REG_RESERVED6,	CEC_SLAVE_CEC, 0xbb, 2, 2, 1, RW, 1),
	CEC_REG(CEC_REG_RESERVED7,	CEC_SLAVE_CEC, 0xbb, 1, 1, 1, RW, 1),
	CEC_REG(CEC_REG_RESERVED8,	CEC_SLAVE_CEC, 0xbb, 0, 0, 1, RW, 1),
	CEC_REG(CEC_REG_LOG_ADDR_1,		CEC_SLAVE_CEC, 0xbc, 7, 4, 1, RW, 1),
	CEC_REG(CEC_REG_LOG_ADDR_0,		CEC_SLAVE_CEC, 0xbc, 3, 0, 1, RW, 1),
	CEC_REG(CEC_REG_LOG_ADDR_2,		CEC_SLAVE_CEC, 0xbd, 3, 0, 1, RW, 1),
	CEC_REG(CEC_REG_CLOCK_DIVIDER,		CEC_SLAVE_CEC, 0xbe, 7, 2, 1, RW, 1),
	CEC_REG(CEC_REG_POWER_MODE,		CEC_SLAVE_CEC, 0xbe, 1, 0, 1, RW, 1),
	CEC_REG(CEC_REG_RESERVED9,			CEC_SLAVE_CEC, 0xbf, 5, 0, 1, RW, 1),
	CEC_REG(CEC_REG_SOFT_RESET,		CEC_SLAVE_CEC, 0xc0, 0, 0, 1, RW, 1),
	CEC_REG(CEC_REG_TIMING0,		CEC_SLAVE_CEC, 0xc1, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING1,		CEC_SLAVE_CEC, 0xc3, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING2,		CEC_SLAVE_CEC, 0xc5, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING3,		CEC_SLAVE_CEC, 0xc7, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING4,		CEC_SLAVE_CEC, 0xc9, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING5,		CEC_SLAVE_CEC, 0xcb, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING6,		CEC_SLAVE_CEC, 0xcd, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING7,		CEC_SLAVE_CEC, 0xcf, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING8,		CEC_SLAVE_CEC, 0xd1, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING9,		CEC_SLAVE_CEC, 0xd3, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING10,		CEC_SLAVE_CEC, 0xd5, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING11,		CEC_SLAVE_CEC, 0xd7, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING12,		CEC_SLAVE_CEC, 0xd9, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING13,		CEC_SLAVE_CEC, 0xdb, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING14,		CEC_SLAVE_CEC, 0xde, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_RESERVED10,		CEC_SLAVE_CEC, 0xe0, 0, 0, 1, RW, 1),
	CEC_REG(CEC_REG_TIMING15,		CEC_SLAVE_CEC, 0xe1, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING16,		CEC_SLAVE_CEC, 0xe3, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING17,		CEC_SLAVE_CEC, 0xe5, 0, 0, 2, RW, 0),
	CEC_REG(CEC_REG_TIMING18,		CEC_SLAVE_CEC, 0xe7, 0, 0, 8, RW, 0),
	CEC_REG(CEC_REG_MAIN_POWER_DOWN,	CEC_SLAVE_MAIN, 0xe2, 0, 0, 1, RW, 1),
	CEC_REG(CEC_REG_MAIN_HPD_OVERRIDE,		CEC_SLAVE_MAIN, 0xd6, 6, 6, 1, RW, 1),
	CEC_REG(CEC_REG_INTR_STATUS,		CEC_SLAVE_MAIN, 0x93, 0, 0, 1, RW, 0),
	CEC_REG(CEC_REG_INTR_TX_READY,		CEC_SLAVE_MAIN, 0x97, 6, 6, 1, RW, 1),
	CEC_REG(CEC_REG_INTR_TX_ARBITRATION,	CEC_SLAVE_MAIN, 0x97, 5, 5, 1, RW, 1),
	CEC_REG(CEC_REG_INTR_TX_RETRY_TIMEOUT,	CEC_SLAVE_MAIN, 0x97, 4, 4, 1, RW, 1),
	CEC_REG(CEC_REG_INTR_RX_READY3,		CEC_SLAVE_MAIN, 0x97, 3, 3, 1, RW, 1),
	CEC_REG(CEC_REG_INTR_RX_READY2,		CEC_SLAVE_MAIN, 0x97, 2, 2, 1, RW, 1),
	CEC_REG(CEC_REG_INTR_RX_READY1,		CEC_SLAVE_MAIN, 0x97, 1, 1, 1, RW, 1),
	CEC_REG(CEC_REG_COUNT,			0, 0, 0, 0, 0, RW, 0),
};

#undef RW
#undef RO

/* The CEC controller's state */
typedef struct CecDev {
	SceUID eventFlag;		/* 0x00 */
	int (*txDone)(void);		/* 0x04: the CecDevCallbacks cec.c gave */
	int (*rx)(void);		/* 0x08 */
	int (*nop1)(void);		/* 0x0c */
	int (*disconnected)(void);	/* 0x10 */
	int (*nop2)(void);		/* 0x14 */
	int (*nop3)(void);		/* 0x18 */
	int (*connected)(void);		/* 0x1c */
	int (*standby)(void);		/* 0x20 */
	int (*isEnabled)(void);		/* 0x24 */
	int (*standbyDone)(void);	/* 0x28 */
	int hpd;			/* 0x2c: the sink's hot plug */
	SceKernelSpinlock lock;		/* 0x30: guards hpd and physicalAddress */
	SceUInt8 logicalAddress;	/* 0x34 */
	SceUInt8 physicalAddress[2];	/* 0x35 */
	SceUInt8 available;		/* 0x37 */
} CecDev;

static CecDev cecDev;

/* Sony's name (debug records). Read register (or field) id into data. */
static inline int _getReg(int id, void *data, SceUInt32 size)
{
	const CecDevRegister *reg = &cecDevRegisters[id];
	SceUInt8 value[CEC_DEV_FRAME_SIZE];
	SceUInt8 regSize;
	int ret;

	if (!(reg->access & CEC_REG_READ))
		return -1;
	regSize = reg->size;
	ret = sceHdmiDeviceI2cRead(1, reg->slave, reg->reg, value, regSize);
	if (ret < 0)
		return ret;
	if (reg->isField) {
		if (reg->msb < reg->lsb)
			SCE_PANIC(543);
		if (reg->msb > 7)
			SCE_PANIC(549);
		if (regSize != 1)
			SCE_PANIC(554);
		value[0] = ((SceUInt32)value[0] >> reg->lsb) & ((1 << (reg->msb + 1 - reg->lsb)) - 1);
	}
	if (size != 0)
		__builtin_memcpy(data, value, size);
	return 0;
}

/* Write data to register (or field) id. */
static inline int _setReg(int id, const void *data, SceUInt32 size)
{
	const CecDevRegister *reg = &cecDevRegisters[id];
	SceUInt8 value;
	SceUInt8 mask;
	int ret;

	if (!(reg->access & CEC_REG_WRITE))
		return -1;
	if (!reg->isField)
		return sceHdmiDeviceI2cWrite(1, reg->slave, reg->reg, data, size);
	ret = sceHdmiDeviceI2cRead(1, reg->slave, reg->reg, &value, 1);
	if (ret < 0)
		return ret;
	mask = ((1 << (reg->msb + 1 - reg->lsb)) - 1) << reg->lsb;
	value = (value & ~mask) | ((*(const SceUInt8 *)data << reg->lsb) & mask);
	return sceHdmiDeviceI2cWrite(1, reg->slave, reg->reg, &value, 1);
}

static inline int _setRegValue(int id, SceUInt8 value)
{
	return _setReg(id, &value, 1);
}

int _intrTxReady(SceUInt32 status)
{
	ksceKernelSetEventFlag(cecDev.eventFlag, CEC_DEV_EVENT_TX_READY);
	_setRegValue(CEC_REG_INTR_TX_READY, 1);
	return 0;
}

int _intrTxArbitration(SceUInt32 status)
{
	ksceKernelSetEventFlag(cecDev.eventFlag, CEC_DEV_EVENT_TX_ARBITRATION);
	_setRegValue(CEC_REG_INTR_TX_ARBITRATION, 1);
	return 0;
}

int _intrTxRetryTimeout(SceUInt32 status)
{
	ksceKernelSetEventFlag(cecDev.eventFlag, CEC_DEV_EVENT_TX_RETRY_TIMEOUT);
	_setRegValue(CEC_REG_INTR_TX_RETRY_TIMEOUT, 1);
	return 0;
}

int _intrMain(void)
{
	if (cecDev.txDone != NULL)
		cecDev.txDone();
	_setRegValue(CEC_REG_INTR_STATUS, 0xff);
	return 0;
}

int _intrRxReady(SceUInt32 status)
{
	if (cecDev.rx != NULL)
		cecDev.rx();
	if (status & CEC_DEV_INTR_RX_READY1)
		_setRegValue(CEC_REG_INTR_RX_READY1, 1);
	if (status & CEC_DEV_INTR_RX_READY2)
		_setRegValue(CEC_REG_INTR_RX_READY2, 1);
	if (status & CEC_DEV_INTR_RX_READY3)
		_setRegValue(CEC_REG_INTR_RX_READY3, 1);
	return 0;
}

/* Sony's name (debug records). */
int cecDevInitialize(const CecDevCallbacks *callbacks)
{
	SceUID eventFlag;

	cecDev.logicalAddress = 0xf;
	cecDev.nop2 = callbacks->nop2;
	cecDev.txDone = callbacks->txDone;
	cecDev.rx = callbacks->rx;
	cecDev.nop1 = callbacks->nop1;
	cecDev.disconnected = callbacks->disconnected;
	cecDev.nop3 = callbacks->nop3;
	cecDev.connected = callbacks->connected;
	cecDev.standby = callbacks->standby;
	cecDev.isEnabled = callbacks->isEnabled;
	cecDev.standbyDone = callbacks->standbyDone;
	cecDev.physicalAddress[0] = 0xff;
	cecDev.physicalAddress[1] = 0xff;
	cecDev.available = 1;
	eventFlag = ksceKernelCreateEventFlag("SceHdmiCECDev", SCE_EVENT_WAITMULTIPLE, 0, NULL);
	if (eventFlag < 0) {
		SCE_DBG_ASSERT_FAIL(954);
		return eventFlag;
	}
	cecDev.eventFlag = eventFlag;
	return 0;
}

int cecDevEnable(void)
{
	SceUInt8 data[3];
	int ret;

	if (!cecDev.available)
		return -1;
	while (sceHdmiDeviceI2cRead(1, CEC_SLAVE_CEC, 0, data, 3) != 0)
		sceKernelDelayThread(100000);
	ret = ScePervasiveForDriver_26B51075();
	if (ret < 0)
		return ret;
	ret = _setRegValue(CEC_REG_POWER_DOWN, 0);
	if (ret < 0)
		return ret;
	ret = _setRegValue(CEC_REG_MAIN_POWER_DOWN, 0);
	if (ret < 0)
		return ret;
	ret = _setRegValue(CEC_REG_POWER_MODE, 1);
	if (ret < 0)
		return ret;
	data[0] = 8;
	_setReg(CEC_REG_CLOCK_DIVIDER, data, 1);
	data[0] = 0x0d;
	data[1] = 0x2f;
	_setReg(CEC_REG_TIMING0, data, 2);
	data[0] = 0x0c;
	data[1] = 0x4e;
	_setReg(CEC_REG_TIMING1, data, 2);
	data[0] = 0x0e;
	data[1] = 0x10;
	_setReg(CEC_REG_TIMING2, data, 2);
	data[0] = 0x0a;
	data[1] = 0xd7;
	_setReg(CEC_REG_TIMING3, data, 2);
	data[0] = 0x09;
	data[1] = 0xf6;
	_setReg(CEC_REG_TIMING4, data, 2);
	data[0] = 0x0b;
	data[1] = 0xb8;
	_setReg(CEC_REG_TIMING5, data, 2);
	data[0] = 0x07;
	data[1] = 0x08;
	_setReg(CEC_REG_TIMING6, data, 2);
	data[0] = 0x05;
	data[1] = 0xb7;
	_setReg(CEC_REG_TIMING7, data, 2);
	data[0] = 0x08;
	data[1] = 0x5a;
	_setReg(CEC_REG_TIMING8, data, 2);
	data[0] = 0x01;
	data[1] = 0xc2;
	_setReg(CEC_REG_TIMING9, data, 2);
	data[0] = 0x04;
	data[1] = 0x65;
	_setReg(CEC_REG_TIMING10, data, 2);
	data[0] = 0x05;
	data[1] = 0x46;
	_setReg(CEC_REG_TIMING11, data, 2);
	data[0] = 0x03;
	data[1] = 0x14;
	_setReg(CEC_REG_TIMING12, data, 2);
	data[0] = 0x0a;
	data[1] = 0x8c;
	_setReg(CEC_REG_TIMING13, data, 2);
	data[0] = 0x00;
	data[1] = 0xbc;
	_setReg(CEC_REG_TIMING14, data, 2);
	data[0] = 0x00;
	data[1] = 0xe1;
	_setReg(CEC_REG_TIMING15, data, 2);
	data[0] = 0x02;
	data[1] = 0xa3;
	_setReg(CEC_REG_TIMING16, data, 2);
	data[0] = 0x03;
	data[1] = 0x84;
	_setReg(CEC_REG_TIMING17, data, 2);
	ret = _setRegValue(CEC_REG_MAIN_HPD_OVERRIDE, 1);
	if (ret < 0)
		return ret;
	ret = _setRegValue(CEC_REG_CLOCK_ENABLE, 1);
	if (ret < 0)
		return ret;
	ret = _setRegValue(CEC_REG_CLOCK_COUNT, 100);
	if (ret < 0)
		return ret;
	return hdmiDeviceEnableIntr(HDMI_INTR_MASK_CEC);
}

int cecDevDisable(void)
{
	if (!cecDev.available)
		return -1;
	hdmiDeviceDisableIntr(HDMI_INTR_MASK_CEC);
	_setRegValue(CEC_REG_MAIN_POWER_DOWN, 1);
	_setRegValue(CEC_REG_POWER_DOWN, 1);
	return ScePervasiveForDriver_D185D235();
}

int cecDevSuspend(void)
{
	if (cecDev.standby == NULL)
		return 0;
	return cecDev.standby();
}

int cecDevSuspendDone(void)
{
	if (cecDev.isEnabled == NULL)
		return 0;
	return cecDev.isEnabled();
}

int cecDevResume(void)
{
	if (cecDev.standbyDone == NULL)
		return 0;
	return cecDev.standbyDone();
}

int cecDevSetLogicalAddress(SceUInt8 address)
{
	int ret;

	if (address >= 0x10)
		return -1;
	ret = _setRegValue(CEC_REG_LOG_ADDR_MASK, 1);
	if (ret < 0)
		return ret;
	ret = _setReg(CEC_REG_LOG_ADDR_0, &address, 1);
	if (ret < 0)
		return ret;
	cecDev.logicalAddress = address;
	return 0;
}

int cecDevGetPhysicalAddress(SceUInt8 *physical)
{
	ksceKernelSpinlockLowLock(&cecDev.lock);
	physical[0] = cecDev.physicalAddress[0];
	physical[1] = cecDev.physicalAddress[1];
	ksceKernelSpinlockLowUnlock(&cecDev.lock);
	return 0;
}

int cecDevGetLogicalAddress(SceUInt8 *address)
{
	*address = cecDev.logicalAddress;
	return 0;
}

/* Sony's name (debug records). Send a frame and wait for the end. */
int cecDevTx(const SceUInt8 *frame, SceUInt32 length)
{
	SceUInt32 events = 0;
	SceUInt32 timeout;
	SceUInt8 data[CEC_DEV_FRAME_SIZE] = { 0 };
	int ret;

	if (length > CEC_DEV_FRAME_SIZE) {
		SCE_DBG_ASSERT(0, 1129);
		return -1;
	}
	if (length != 0)
		__builtin_memcpy(data, frame, length);
	ret = _setReg(CEC_REG_TX_FRAME, data, CEC_DEV_FRAME_SIZE);
	if (ret < 0)
		return ret;
	data[0] = length;
	ret = _setReg(CEC_REG_TX_FRAME_LENGTH, data, 1);
	if (ret < 0)
		return ret;
	data[0] = 1;
	ret = _setReg(CEC_REG_TX_ENABLE, data, 1);
	if (ret < 0)
		return ret;
	timeout = CEC_DEV_TX_TIMEOUT;
	ret = ksceKernelWaitEventFlag(cecDev.eventFlag, CEC_DEV_EVENT_TX_READY |
				      CEC_DEV_EVENT_TX_RETRY_TIMEOUT | CEC_DEV_EVENT_TX_ARBITRATION,
				      SCE_EVENT_WAITOR | SCE_EVENT_WAITCLEAR, &events, &timeout);
	if (ret < 0)
		return ret;
	if (events & CEC_DEV_EVENT_TX_ARBITRATION)
		return 1;
	return events & CEC_DEV_EVENT_TX_RETRY_TIMEOUT;
}

/* Read the frame in receive buffer id. */
int cecDevGetRxFrame(SceUInt8 id, SceUInt8 *frame, SceUInt32 length)
{
	int reg;

	if (id >= CEC_DEV_RX_BUFFERS)
		return -1;
	if (frame == NULL)
		return -1;
	if (length > CEC_DEV_FRAME_SIZE)
		return -1;
	if (id == 1)
		reg = CEC_REG_RX2_FRAME;
	else if (id == 2)
		reg = CEC_REG_RX3_FRAME;
	else
		reg = CEC_REG_RX1_FRAME;
	return _getReg(reg, frame, length);
}

/* The length of the frame in receive buffer id. */
int cecDevGetRxLength(SceUInt8 id, SceUInt32 *length)
{
	SceUInt8 value;
	int reg, ret;

	if (id >= CEC_DEV_RX_BUFFERS)
		return -1;
	if (length == NULL)
		return -1;
	if (id == 1)
		reg = CEC_REG_RX2_FRAME_LENGTH;
	else if (id == 2)
		reg = CEC_REG_RX3_FRAME_LENGTH;
	else
		reg = CEC_REG_RX1_FRAME_LENGTH;
	ret = _getReg(reg, &value, 1);
	if (ret == 0)
		*length = value;
	return ret;
}

int cecDevIntrHandler(SceUInt32 status)
{
	if (status & CEC_DEV_INTR_MAIN)
		_intrMain();
	if (status & CEC_DEV_INTR_TX_RETRY_TIMEOUT)
		_intrTxRetryTimeout(status);
	if (status & CEC_DEV_INTR_TX_ARBITRATION)
		_intrTxArbitration(status);
	if (status & CEC_DEV_INTR_RX_READY)
		_intrRxReady(status);
	if (status & CEC_DEV_INTR_TX_READY)
		_intrTxReady(status);
	return 0;
}

/* Sony's name (debug records). Free receive buffer id. */
int cecDevRxDone(SceUInt8 id)
{
	if (id >= CEC_DEV_RX_BUFFERS) {
		SCE_DBG_ASSERT_LEVEL(0, 0, 1289);
		return -1;
	}
	if (id == 0) {
		_setRegValue(CEC_REG_RX1_CLEAR, 1);
		_setRegValue(CEC_REG_RX1_CLEAR, 0);
	} else if (id == 1) {
		_setRegValue(CEC_REG_RX2_CLEAR, 1);
		_setRegValue(CEC_REG_RX2_CLEAR, 0);
	} else {
		_setRegValue(CEC_REG_RX3_CLEAR, 1);
		_setRegValue(CEC_REG_RX3_CLEAR, 0);
	}
	return 0;
}

/* The receive buffers holding a frame, oldest first. */
int cecDevRxGetPending(SceUInt8 *ids, SceUInt32 *count)
{
	SceUInt8 order[CEC_DEV_RX_BUFFERS] = { 0, 0, 0 };
	SceUInt32 latest;
	int i, n, buffer;

	*count = 0;
	_getReg(CEC_REG_RX1_ORDER, &order[0], 1);
	_getReg(CEC_REG_RX2_ORDER, &order[1], 1);
	_getReg(CEC_REG_RX3_ORDER, &order[2], 1);
	for (n = 0; n < CEC_DEV_RX_BUFFERS; n++) {
		buffer = CEC_DEV_RX_BUFFERS;
		latest = 0;
		for (i = 0; i < CEC_DEV_RX_BUFFERS; i++) {
			if (order[i] != 0 && order[i] > latest && order[i] <= 3) {
				buffer = i;
				latest = order[i];
			}
		}
		if (buffer != CEC_DEV_RX_BUFFERS) {
			order[buffer] = 0;
			ids[*count] = buffer;
			(*count)++;
		}
	}
	return 0;
}

void cecDevPoll(void)
{
}

void cecDevNotifyHpd(int on)
{
	int (*callback)(void);

	ksceKernelSpinlockLowLock(&cecDev.lock);
	if (cecDev.hpd != 0) {
		if (on != 0) {
			ksceKernelSpinlockLowUnlock(&cecDev.lock);
			return;
		}
		cecDev.hpd = on;
		ksceKernelSpinlockLowUnlock(&cecDev.lock);
		callback = cecDev.disconnected;
	} else {
		if (on == 0) {
			ksceKernelSpinlockLowUnlock(&cecDev.lock);
			return;
		}
		cecDev.hpd = on;
		ksceKernelSpinlockLowUnlock(&cecDev.lock);
		callback = cecDev.nop1;
	}
	if (callback != NULL)
		callback();
}

void cecDevNotifyRxSense(int on)
{
	if (on) {
		if (cecDev.nop2 != NULL)
			cecDev.nop2();
	} else {
		if (cecDev.nop3 != NULL)
			cecDev.nop3();
	}
}

void cecDevSetPhysicalAddress(const SceUInt8 *physical)
{
	ksceKernelSpinlockLowLock(&cecDev.lock);
	cecDev.physicalAddress[0] = physical[0];
	cecDev.physicalAddress[1] = physical[1];
	ksceKernelSpinlockLowUnlock(&cecDev.lock);
	if (cecDev.connected != NULL)
		cecDev.connected();
}

int cecDevSetMode(int mode)
{
	if ((SceUInt32)mode >= 0x10)
		return SCE_HDMI_ERROR_INVALID_ARGUMENT;
	return _setRegValue(CEC_REG_TX_MODE, mode);
}

