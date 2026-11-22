/*
 * impose_listener.h: the listeners the impose menu (and the indicator's notices) add to their
 * widgets, each calling a method of an object, with an argument or without. Kept apart from
 * impose.h so a file can use them without that header's LSDB keys.
 */
#ifndef IMPOSE_LISTENER_H
#define IMPOSE_LISTENER_H

#include <stddef.h>
#include <scetypes.h>
#include <widget/core/handler.h>

/* PLACEHOLDER names: listeners calling a method of a part of the menu, with an argument or without */
template<class T>
class ImposePsMenuListener : public paf::ui::EventListener {
public:
	ImposePsMenuListener(T *obj, void (T::*func)()) : m_obj(obj), m_func(func)
	{
	}

	virtual int32_t Do(int32_t type, paf::ui::Handler *self, paf::ui::Event *e)
	{
		if (m_killed)
			return 0x80AF4101;
		(m_obj->*m_func)();
		return 0;
	}

	T *m_obj;
	void (T::*m_func)();
};

template<class T, class A>
class ImposePsMenuArgListener : public paf::ui::EventListener {
public:
	ImposePsMenuArgListener(T *obj, void (T::*func)(A), A arg)
		: m_obj(obj), m_func(func), m_arg(arg)
	{
	}

	virtual int32_t Do(int32_t type, paf::ui::Handler *self, paf::ui::Event *e)
	{
		if (m_killed)
			return 0x80AF4101;
		(m_obj->*m_func)(m_arg);
		return 0;
	}

	T *m_obj;
	void (T::*m_func)(A);
	A m_arg;
};

#endif
