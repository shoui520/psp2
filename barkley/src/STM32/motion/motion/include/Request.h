/*
 * Request: a transfer a driver runs for its caller: the buffer, how much of it is
 * left, and the result, reported to a callback that is given the request.
 */
#ifndef REQUEST_H
#define REQUEST_H

#include "stm32f10x_type.h"
#include "Callback.h"

class Request {
public:
	typedef Callback<void (Request *)> Done;

	Request() {}

	void Set(void *buf, u32 size, Done::Func func, void *arg)
	{
		done_.Set(func, arg);
		buf_ = (u8 *)buf;
		size_ = size;
		cur_ = (u8 *)buf;
		remaining_ = size;
	}

	void SetResult(s32 result)
	{
		result_ = result;
	}

	s32 Result() const
	{
		return result_;
	}

	void Notify()
	{
		done_(this);
	}

	/* the callback, if one is set, called once */
	void NotifyOnce()
	{
		done_.TakeIfSet(this);
	}

	void Complete(s32 result)
	{
		SetResult(result);
		Notify();
	}

	void Finish(s32 result)
	{
		SetResult(result);
		NotifyOnce();
	}

	Done done_;
	u8 *buf_;
	u32 size_;
	u8 *cur_;
	u32 remaining_;
	s32 result_;
};

#endif
