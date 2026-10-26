/*
 * HifSpi: a transfer done, the host's frame is parsed and the next transfer set up at
 * once, with the frame built from what is queued; a failed read is reported and
 * parsed all the same.
 */

#include <stdio.h>

#include "HifSpi.h"

HifSpi::HifSpi(MotionProcessor *processor, MotionFrame *frames, u32 frameCount,
	       const u32 *thresholds, SpiSlave *spi, Output *line0, Output *line1)
	: MotionHif(processor, frames, frameCount, thresholds), spi_(spi), line0_(line0),
	  line1_(line1), receiving_(false), pending_(false)
{
	SetPacketSize(59);
}

void HifSpi::Init()
{
	receiving_ = false;
}

void HifSpi::Start()
{
	u32 size;
	u8 *frame;

	Reset();
	RegisterListener();
	frame = BuildFrame(&size);
	spi_->Receive(rxBuffer_, PacketSize(), ReadCallback, this);
	spi_->Send(frame, PacketSize(), WriteCallback, this);
	receiving_ = true;
}

void HifSpi::WriteCallback(void *arg, Request *request)
{
	static_cast<HifSpi *>(arg)->SpiWriteCallback(request);
}

void HifSpi::SpiWriteCallback(Request *request)
{
}

void HifSpi::ReadCallback(void *arg, Request *request)
{
	static_cast<HifSpi *>(arg)->SpiReadCallback(request);
}

void HifSpi::SpiReadCallback(Request *request)
{
	u32 size;
	u8 *frame;

	if (request->Result() != 0)
		printf("HifSpi::SpiReadCallback() failed, result=%d.\n", request->result_);
	receiving_ = false;
	ParseFrame(rxBuffer_);
	frame = BuildFrame(&size);
	spi_->Receive(rxBuffer_, PacketSize(), ReadCallback, this);
	spi_->Send(frame, PacketSize(), WriteCallback, this);
	receiving_ = true;
	if (pending_)
		pending_ = false;
}

void HifSpi::OnFrame(const MotionFrame *frame)
{
	MotionHif::OnFrame(frame);
}

void HifSpi::OnSend()
{
}

void HifSpi::OnReady()
{
	pending_ = !receiving_;
}

/* the port off until Resume */
void HifSpi::Suspend()
{
	spi_->Stop();
}

void HifSpi::Resume()
{
	spi_->Restart();
	Start();
}
