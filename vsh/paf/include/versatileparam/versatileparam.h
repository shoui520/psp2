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
#include <versatileparam/idparam.h>

namespace paf {

class File;
class Plugin;
class ResourceParam;

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

	ResourceParam *m_serial;
	Plugin *m_plugin;
	IDParam m_id;
	cxml::Element *m_node;
};

/* the parameters read from a resource, by their tag and ID */
class VersatileParamManager {
public:
	void AddParam(const char *tag, const IDParam &id, VersatileParam *param);
};

}

#endif
