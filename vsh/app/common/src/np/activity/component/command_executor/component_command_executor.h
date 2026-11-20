/*
 * The command executor: runs one command at a time, its request, response and parse (Sony's
 * file: a file the checks do not name; the class and method names are ours).
 */
#ifndef NP_ACTIVITY_COMPONENT_COMMAND_EXECUTOR_COMPONENT_COMMAND_EXECUTOR_H
#define NP_ACTIVITY_COMPONENT_COMMAND_EXECUTOR_COMPONENT_COMMAND_EXECUTOR_H

#include "../abstract_component.h"

class AbstractCommand;

class ComponentCommandExecutor : public Component {
public:
	ComponentCommandExecutor();
	virtual ~ComponentCommandExecutor();

	virtual int SetCommand(AbstractCommand *command);
	virtual int Execute();
	virtual int ClearCommand();

	AbstractCommand *m_command;
};

#endif
