/*
 * BufferedUsartStream: a USART port whose output is queued and sent by its interrupts,
 * at most a given size at a time; the writers do not wait. With discard set, the
 * output goes nowhere (image 00 has no discard).
 */
#ifndef BUFFEREDUSARTSTREAM_H
#define BUFFEREDUSARTSTREAM_H

#include "Fifo.h"
#include "Stream.h"

class BufferedUsartStream : public Usart, public Stream {
public:
	BufferedUsartStream(s8 port, s8 mode, u8 *buffer, u32 size, u32 chunk);

	virtual u32 Write(const u8 *buf, u32 size)
	{
		return Put(buf, size);
	}

	virtual void Send(void *buf, u32 size, Request::Done::Func func, void *arg)
	{
		Usart::Send(buf, size, func, arg);
	}

	virtual void Receive(void *buf, u32 size, Request::Done::Func func, void *arg)
	{
		Usart::Receive(buf, size, func, arg);
	}

#if MOTION_IMAGE != 0
	void SetDiscard(bool discard)
	{
		discard_ = discard;
	}
#endif

private:
	/* a piece of the queue being sent */
	struct Segment {
		u8 *buf;
		u32 size;
	};

	u32 Put(const u8 *buf, u32 size);
	static void SendCallback(void *arg, Request *request);
	void OnSend(Request *request);

	Fifo fifo_;
	u32 chunk_;			/* the most sent at a time */
	volatile s32 writers_;		/* those filling the queue */
	bool sending_;
	Segment segments_[3];		/* what is sent, ended by a null buf */
	Segment *volatile current_;
#if MOTION_IMAGE != 0
	bool discard_;
#endif
};

#endif
