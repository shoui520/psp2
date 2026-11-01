/*
 * paf::VersatileParam, as far as converted C++ uses it: a parameter object
 * read from a resource element. Names after ScePaf-RE, the virtual functions
 * in its order.
 */
#ifndef PAF_VERSATILEPARAM_VERSATILEPARAM_H
#define PAF_VERSATILEPARAM_VERSATILEPARAM_H

#include <stdint.h>
#include <std/memory>
#include <common/shared_ptr.h>
#include <cxml/cxml.h>

namespace paf {

class File;
class Plugin;

class VersatileParam {
public:
	virtual const char *GetParamName() const;
	virtual ~VersatileParam();
	virtual int32_t Set();
	virtual int32_t Get();
	virtual int32_t Create(int32_t mode);
	virtual int32_t Destroy(int32_t mode);
	virtual int32_t OnUpdate();
	virtual void OnDraw();
	virtual int32_t AttachPlugin(Plugin *plugin);
	virtual int32_t Read(cxml::Element *elem);
	virtual int32_t Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent);
};

}

#endif
