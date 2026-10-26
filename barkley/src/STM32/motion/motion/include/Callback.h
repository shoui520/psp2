/*
 * Callback: a function and the argument it is called with, the way the drivers report
 * an operation's end to the object that started it (a static member function that
 * calls the object's method).
 */
#ifndef CALLBACK_H
#define CALLBACK_H

template <typename Signature> class Callback;

template <>
class Callback<void ()> {
public:
	typedef void (*Func)(void *arg);

	Callback() : func_(0), arg_(0) {}

	void Set(Func func, void *arg)
	{
		func_ = func;
		arg_ = arg;
	}

	void operator()() const
	{
		func_(arg_);
	}

private:
	Func func_;
	void *arg_;
};

template <typename A1>
class Callback<void (A1)> {
public:
	typedef void (*Func)(void *arg, A1 a1);

	Callback() : func_(0), arg_(0) {}

	void Set(Func func, void *arg)
	{
		func_ = func;
		arg_ = arg;
	}

	void operator()(A1 a1) const
	{
		func_(arg_, a1);
	}

	/* the callback cleared, then called: it runs once */
	void Take(A1 a1)
	{
		Func func = func_;

		func_ = 0;
		func(arg_, a1);
	}

	void TakeIfSet(A1 a1)
	{
		if (func_ != 0)
			Take(a1);
	}

protected:
	Func func_;
	void *arg_;
};

template <typename A1, typename A2>
class Callback<void (A1, A2)> {
public:
	typedef void (*Func)(void *arg, A1 a1, A2 a2);

	Callback() : func_(0), arg_(0) {}

	void Set(Func func, void *arg)
	{
		func_ = func;
		arg_ = arg;
	}

	void operator()(A1 a1, A2 a2) const
	{
		func_(arg_, a1, a2);
	}

private:
	Func func_;
	void *arg_;
};

#endif
