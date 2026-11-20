/*
 * The command executor: runs the context's one command, its request, response and parse (Sony's
 * file: no check names it; the path is ours, after the component's class)
 */
#include <stddef.h>
#include "../../np_activity.h"
#include "../../command/abstract_command.h"
#include "component_command_executor.h"

ComponentCommandExecutor::ComponentCommandExecutor() : m_command(NULL)
{
}

ComponentCommandExecutor::~ComponentCommandExecutor()
{
}

int ComponentCommandExecutor::SetCommand(AbstractCommand *command)
{
	m_command = command;
	return 0;
}

/* reads the response and parses it, piece by piece, until either says it is done */
__attribute__((noinline)) int ComponentCommandExecutor::receive()
{
	int parseDone = 0;
	void *data = NULL;
	SceSize size = 0;
	int receiveDone;
	int done;
	int ret = 0;

	m_command->GetContentLength();
	do {
		ret = CheckAbort(ret);
		if (ret == NP_ACTIVITY_ERROR_ABORTED)
			break;
		receiveDone = 0;
		ret = m_command->ReceiveData(&data, &size, &receiveDone);
		done = 1;
		if (ret >= 0)
			done = receiveDone;
		if (!ACTIVITY_FAILED(ret)) {
			ret = m_command->ParseData(data, size, &parseDone);
			if (ret < 0)
				done = 1;
			if (parseDone != 0)
				done = 1;
		}
	} while (done == 0);
	return ret;
}

int ComponentCommandExecutor::Execute()
{
	int ret;

	if (m_command == NULL)
		return NP_ACTIVITY_ERROR_INVALID_STATE;

	ret = CheckAbort(0);
	if (ret == NP_ACTIVITY_ERROR_ABORTED)
		return ret;
	ret = m_command->Initialize();
	if (ACTIVITY_FAILED(ret))
		return ret;
	if (CheckAbort(ret) == NP_ACTIVITY_ERROR_ABORTED) {
		m_command->EndRequest();
		return NP_ACTIVITY_ERROR_ABORTED;
	}
	ret = m_command->SendRequest();
	if (ACTIVITY_FAILED(ret)) {
		m_command->EndRequest();
		return ret;
	}
	ret = receive();
	if (ACTIVITY_FAILED(ret)) {
		m_command->EndParse();
		m_command->EndRequest();
		return ret;
	}
	ret = m_command->EndParse();
	if (ACTIVITY_FAILED(ret)) {
		m_command->EndRequest();
		return ret;
	}
	ret = m_command->EndRequest();
	if (ACTIVITY_FAILED(ret))
		return ret;
	return m_command->GetResult();
}

int ComponentCommandExecutor::ClearCommand()
{
	m_command = NULL;
	return 0;
}
