/*
 * The command executor: runs one command at a time, 20 bytes (Sony's file: a file the checks do not name; the class name is ours). Its fields and
 * methods are written with a file the checks do not name.
 */
#ifndef NP_ACTIVITY_COMPONENT_COMMAND_EXECUTOR_COMPONENT_COMMAND_EXECUTOR_H
#define NP_ACTIVITY_COMPONENT_COMMAND_EXECUTOR_COMPONENT_COMMAND_EXECUTOR_H

#include "../abstract_component.h"

class ComponentCommandExecutor : public Component {
public:
	ComponentCommandExecutor();
	virtual ~ComponentCommandExecutor();
};

#endif
