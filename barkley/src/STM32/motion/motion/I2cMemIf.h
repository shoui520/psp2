/*
 * I2cMemIf: a device on the I2C bus read and written like a memory: its register
 * address (regSize bytes) and the data follow each other in one buffer, sent and
 * received by DMA. Each interrupt event takes the transfer one step on (step_); the
 * end, or the error, is reported to the request's callback.
 */
#ifndef I2CMEMIF_H
#define I2CMEMIF_H

#include "I2c.h"

/* a transfer: the request and its direction */
class I2cRequest : public Request {
public:
	I2cRequest() {}

	void Set(void *buf, u16 size, Done::Func func, void *arg, s8 write)
	{
		Request::Set(buf, size, func, arg);
		write_ = write;
	}

	s8 write_;
};

/* the last transfer that failed, and the bus's state then */
struct I2cMemIfStatus {
	s8 state;
	s8 write;
	u16 size;
	s32 result;
	u16 sr1;
	u16 sr2;
};

class I2cMemIf : public I2c {
public:
	typedef void (I2cMemIf::*Step)(u32 event);

	I2cMemIf(s8 port, u8 address, u16 regSize);
	s32 Init(const I2C_InitTypeDef *init, u8 eventPreemption, u8 eventSub,
		 u8 errorPreemption, u8 errorSub, u32 txPriority, u32 rxPriority);
#if MOTION_IMAGE != 0
	s32 Reinit(const I2C_InitTypeDef *init);
	void DeInit();
#endif
	void Start(void *buf, u16 size, Request::Done::Func func, void *arg, s8 write);

	void Read(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
		Start(buf, size, func, arg, false);
	}

	void Write(void *buf, u16 size, Request::Done::Func func, void *arg)
	{
		Start(buf, size, func, arg, true);
	}

	const I2cMemIfStatus *Status() const
	{
		return &status_;
	}

	virtual void IsrEvent()
	{
		NextStep();
	}

	virtual void IsrError();

private:
	void SetStep(Step step)
	{
		step_ = step;
	}

	void NextStep()
	{
		(this->*step_)(I2C_GetLastEvent(regs_));
	}

	void OnStart(u32 event);
	void OnRegisterSent(u32 event);
	void OnRestart(u32 event);
	void OnReceiveByte(u32 event);
	void OnDmaWait(u32 event);
	void OnSendEnd(u32 event);
	void Finish(s32 result);
	void Fail(s32 result);
	static void DmaSendCallback(void *arg, Request *request);
	void DmaCallbackSend(Request *request);
	static void DmaReceiveCallback(void *arg, Request *request);
	void DmaCallbackReceive(Request *request);

	u8 address_;			/* the device's, shifted left */
	u16 regSize_;
	I2cRequest request_;
	I2cRequest *current_;
	Step step_;
	bool stopSent_;
	I2cMemIfStatus status_;
};

#endif
