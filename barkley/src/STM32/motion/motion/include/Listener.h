/*
 * Listener: a function the motion processor calls with each frame it completes.
 */
#ifndef LISTENER_H
#define LISTENER_H

class MotionFrame;

class Listener {
public:
	typedef void (*Func)(void *arg, const MotionFrame *frame);

	Listener() : func_(0), arg_(0) {}

	void Clear()
	{
		func_ = 0;
	}

	void Set(Func func, void *arg)
	{
		func_ = func;
		arg_ = arg;
	}

	bool IsSet() const
	{
		return func_ != 0;
	}

	void Notify(const MotionFrame *frame) const
	{
		if (func_ != 0)
			Call(frame);
	}

private:
	void Call(const MotionFrame *frame) const
	{
		func_(arg_, frame);
	}

	Func func_;
	void *arg_;
};

#endif
