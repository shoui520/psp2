/*
 * RingBuffer: a queue of items in a caller's array; when full, the oldest item gives
 * way to the newest.
 */
#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include "stm32f10x_type.h"

template <typename T>
class RingBuffer {
public:
	RingBuffer(T *buffer, u32 size) : head_(0), tail_(0), buffer_(buffer), size_(size) {}

	void Clear()
	{
		head_ = 0;
		tail_ = 0;
	}

	T *Push(const T *item)
	{
		T *slot = Alloc();

		*slot = *item;
		return slot;
	}

	T *Pop();

	u32 Count() const
	{
		if (head_ >= tail_)
			return head_ - tail_;
		return head_ + size_ - tail_;
	}

private:
	T *Alloc();

	u32 head_;
	u32 tail_;
	T *buffer_;
	u32 size_;
};

template <typename T>
T *RingBuffer<T>::Alloc()
{
	u32 next = head_ + 1;
	T *slot;

	if (next == size_)
		next = 0;
	if (next == tail_ && ++tail_ == size_)
		tail_ = 0;
	slot = &buffer_[head_];
	head_ = next;
	return slot;
}

template <typename T>
T *RingBuffer<T>::Pop()
{
	T *item;

	if (head_ == tail_)
		return 0;
	item = &buffer_[tail_++];
	if (tail_ == size_)
		tail_ = 0;
	return item;
}

#endif
