/*
 * Stream: where the C library's output goes (its __write, by handle), and a port that
 * sends and receives the host's data. UsartStream: a USART port, the output sent byte
 * by byte, waited for.
 */
#ifndef STREAM_H
#define STREAM_H

#include "Usart.h"

class Stream {
public:
	Stream() {}

	/* the bytes written: how many, or -1 */
	virtual u32 Write(const u8 *buf, u32 size)
	{
		return (u32)-1;
	}

	virtual void Send(void *buf, u32 size, Request::Done::Func func, void *arg) {}
	virtual void Receive(void *buf, u32 size, Request::Done::Func func, void *arg) {}
};

class UsartStream : public Usart, public Stream {
public:
	UsartStream(s8 port, s8 mode) : Usart(port, mode) {}

	virtual u32 Write(const u8 *buf, u32 size)
	{
		return Usart::Write(buf, size);
	}
};

#endif
