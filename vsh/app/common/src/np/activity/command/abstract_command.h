/*
 * The base of the library's commands: one request to the activity server, its response read
 * and parsed, run by the command executor (Sony's file: command/abstract_command.cpp; the method
 * names are ours, written with abstract_command.cpp).
 */
#ifndef NP_ACTIVITY_COMMAND_ABSTRACT_COMMAND_H
#define NP_ACTIVITY_COMMAND_ABSTRACT_COMMAND_H

#include <scetypes.h>
#include "../common/activity_object.h"

class ComponentAuth;
class ComponentHttp;
class ComponentXmlParser;
class ComponentXmlGenerator;
class ComponentUtility;

class AbstractCommand : public ActivityObject {
public:
	AbstractCommand(ComponentAuth *auth, ComponentHttp *http, ComponentXmlParser *xmlParser,
		ComponentXmlGenerator *xmlGenerator, ComponentUtility *utility);
	virtual ~AbstractCommand();

	virtual int Initialize();
	virtual int SendRequest();
	virtual int ReceiveData(void **data, SceSize *size, int *done);
	virtual int ParseData(void *data, SceSize size, int *done);
	virtual int EndParse();
	virtual int EndRequest();
	virtual int GetResult();
	virtual int BeginReceive();
	/* 0: the command's request cannot go over the connection the context opened */
	virtual int IsAvailable();
	virtual int SetResultCode(const int *code);
	virtual SceSize GetRequestBufferSize();
	virtual SceSize GetResponseBufferSize();
	virtual int Finalize();
};

#endif
