/*
 * BufferedUsartStream: a writer takes room in the queue with the interrupts off, copies
 * its bytes with them on, and the last writer out starts sending what is queued, if
 * nothing is being sent. Each piece sent is taken out of the queue; when the pieces
 * are done, what was queued meanwhile is sent next.
 */

#include <string.h>

extern "C" {
#include "cortexm3_macro.h"
}
#include "BufferedUsartStream.h"

BufferedUsartStream::BufferedUsartStream(s8 port, s8 mode, u8 *buffer, u32 size, u32 chunk)
	: Usart(port, mode), fifo_(buffer, size), chunk_(chunk), writers_(0), sending_(false)
{
#if MOTION_IMAGE != 0
	discard_ = true;
#endif
	segments_[2].buf = 0;
}

u32 BufferedUsartStream::Put(const u8 *buf, u32 size)
{
	u8 *second;
	u32 firstSize;
	u32 secondSize;
	u8 *first;
	u32 n;

#if MOTION_IMAGE != 0
	if (discard_)
		return size;
#endif
	__SETPRIMASK();
	writers_++;
	n = fifo_.Reserve(size, &first, &firstSize, &second, &secondSize);
	if (n == 0) {
		writers_--;
		__RESETPRIMASK();
		return 0;
	}
	__RESETPRIMASK();
	memcpy(first, buf, firstSize);
	if (second != 0)
		memcpy(second, buf + firstSize, secondSize);
	__SETPRIMASK();
	writers_--;
	if (writers_ != 0 || sending_) {
		__RESETPRIMASK();
	} else {
		n = fifo_.Peek(chunk_, &segments_[0].buf, &segments_[0].size, &segments_[1].buf,
			       &segments_[1].size);
		sending_ = true;
		__RESETPRIMASK();
		current_ = segments_;
		Usart::Send(current_->buf, current_->size, SendCallback, this);
	}
	return n;
}

void BufferedUsartStream::SendCallback(void *arg, Request *request)
{
	static_cast<BufferedUsartStream *>(arg)->OnSend(request);
}

void BufferedUsartStream::OnSend(Request *request)
{
	__SETPRIMASK();
	fifo_.Consume(current_->size);
	__RESETPRIMASK();
	current_ = current_ + 1;
	if (current_->buf != 0) {
		Usart::Send(current_->buf, current_->size, SendCallback, this);
		return;
	}
	__SETPRIMASK();
	if (writers_ != 0 ||
	    fifo_.Peek(chunk_, &segments_[0].buf, &segments_[0].size, &segments_[1].buf,
		       &segments_[1].size) == 0) {
		sending_ = false;
		__RESETPRIMASK();
	} else {
		sending_ = true;
		__RESETPRIMASK();
		current_ = segments_;
		Usart::Send(current_->buf, current_->size, SendCallback, this);
	}
}
