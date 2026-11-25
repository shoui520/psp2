/*
 * statemap.h: SMC (State Machine Compiler) runtime base classes for
 * telephony_plugin state machines (TelSmsLogicMediatorSm, TelGeneralLogicSm).
 */
#ifndef TELEPHONY_COMMON_STATEMAP_H
#define TELEPHONY_COMMON_STATEMAP_H

#include <stddef.h>
#include <scetypes.h>
#include <std/list>

namespace statemap {

class State {
public:
	State(const char *name, int stateId)
		: m_name(name), m_stateId(stateId)
	{
	}
	virtual ~State() {}

	const char *getName() const { return m_name; }
	int getId() const { return m_stateId; }

protected:
	const char *m_name;
	int m_stateId;
};

class FSMContext {
public:
	explicit FSMContext(State &initialState)
		: m_state(&initialState), m_previousState(NULL),
		  m_transition(NULL), m_debugFlag(false)
	{
	}
	virtual ~FSMContext() {}

	virtual void enterStartState() = 0;

	bool isInTransition() const { return m_state == NULL; }

	State *getPreviousState() const { return m_previousState; }

	void setState(State &state)
	{
		m_state = &state;
	}

	void clearState()
	{
		m_previousState = m_state;
		m_state = NULL;
	}

	void pushState(State &state)
	{
		if (m_state != NULL)
			m_stateStack.push_back(m_state);
		m_state = &state;
	}

	void popState()
	{
		if (m_stateStack.size() > 0) {
			m_state = m_stateStack.back();
			m_stateStack.pop_back();
		}
	}

	void emptyStateStack()
	{
		while (m_stateStack.size() > 0)
			m_stateStack.pop_back();
	}

	const char *getTransition() const { return m_transition; }
	void setTransition(const char *transition) { m_transition = transition; }

	bool getDebugFlag() const { return m_debugFlag; }
	void setDebugFlag(bool flag) { m_debugFlag = flag; }

protected:
	paf::list<State *> m_stateStack;
	State *m_state;
	State *m_previousState;
	const char *m_transition;
	bool m_debugFlag;
};

} /* namespace statemap */

#endif /* TELEPHONY_COMMON_STATEMAP_H */
