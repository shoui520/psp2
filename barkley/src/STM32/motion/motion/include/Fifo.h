/*
 * Fifo: a queue of bytes in a caller's buffer, filled and emptied in place: Reserve
 * gives the room for the bytes to add, Peek the bytes queued, each as one or two
 * pieces (the second from the buffer's start when the first reaches its end). One byte
 * of the buffer stays free, so a full queue is told from an empty one.
 */
#ifndef FIFO_H
#define FIFO_H

#include "stm32f10x_type.h"

class Fifo {
public:
	Fifo(u8 *buffer, u32 size)
	{
		buffer_ = buffer;
		size_ = size;
		head_ = 0;
		tail_ = 0;
	}

	/* room for up to size bytes, taken: how much */
	u32 Reserve(u32 size, u8 **first, u32 *firstSize, u8 **second, u32 *secondSize)
	{
		u32 n = size;

		*first = buffer_ + head_;
		if (head_ >= tail_) {
			u32 end = size_ - head_;

			if (tail_ + end - 1 < size)
				n = tail_ + end - 1;
			if (n >= end) {
				*firstSize = end;
				*second = buffer_;
				*secondSize = n - end;
				head_ = *secondSize;
			} else {
				*firstSize = n;
				*second = 0;
				*secondSize = 0;
				head_ += n;
			}
		} else {
			if (tail_ - head_ - 1 < size)
				n = tail_ - head_ - 1;
			*firstSize = n;
			*second = 0;
			*secondSize = 0;
			head_ += n;
		}
		return n;
	}

	/* up to size of the bytes queued, left in the queue: how many */
	u32 Peek(u32 size, u8 **first, u32 *firstSize, u8 **second, u32 *secondSize)
	{
		u32 n = size;

		*first = buffer_ + tail_;
		if (head_ < tail_) {
			u32 end = size_ - tail_;

			if (end < size) {
				*firstSize = end;
				*second = buffer_;
				if (head_ + end < size)
					n = head_ + end;
				*secondSize = n - end;
			} else {
				*firstSize = size;
				*second = 0;
				*secondSize = 0;
			}
		} else {
			if (head_ - tail_ < size)
				n = head_ - tail_;
			*firstSize = n;
			*second = 0;
			*secondSize = 0;
		}
		return n;
	}

	/* the first size bytes queued taken out */
	void Consume(u32 size)
	{
		tail_ += size;
		if (tail_ >= size_)
			tail_ -= size_;
	}

private:
	u8 *buffer_;
	u32 size_;
	u32 head_;		/* where the next byte goes */
	u32 tail_;		/* the first byte queued */
};

#endif
