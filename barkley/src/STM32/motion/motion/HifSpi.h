/*
 * HifSpi: the host interface on the SPI link to the Vita, the MCU the slave. Each
 * transfer the host makes carries a frame each way: the host's, parsed when it is in,
 * and the frame built for it, set up before the host selects the port.
 */
#ifndef HIFSPI_H
#define HIFSPI_H

#include "MotionHif.h"
#include "Spi.h"

class HifSpi : public MotionHif {
public:
	HifSpi(MotionProcessor *processor, MotionFrame *frames, u32 frameCount,
	       const u32 *thresholds, SpiSlave *spi, Output *line0, Output *line1);
	void Init();
	void Start();
	void Suspend();
	void Resume();
	virtual void OnFrame(const MotionFrame *frame);
	virtual void OnSend();
	virtual void OnReady();

private:
	static void ReadCallback(void *arg, Request *request);
	void SpiReadCallback(Request *request);
	static void WriteCallback(void *arg, Request *request);
	void SpiWriteCallback(Request *request);

	/* the size of each transfer */
	u16 PacketSize()
	{
		return FrameSize();
	}

	void SetPacketSize(u16 size)
	{
		SetFrameSize(size);
	}

	SpiSlave *spi_;
	Output *line0_;			/* two outputs to the host, kept and not used */
	Output *line1_;
	bool receiving_;		/* a transfer set up */
	bool pending_;			/* ready while none was */
	u8 rxBuffer_[MOTION_FRAME_MAX];
};

#endif
