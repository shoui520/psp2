/*
 * paf::VersatileParam, as far as converted C++ uses it: a parameter object
 * read from a resource element. Names after ScePaf-RE, the virtual functions
 * in its order.
 */
#ifndef PAF_VERSATILEPARAM_VERSATILEPARAM_H
#define PAF_VERSATILEPARAM_VERSATILEPARAM_H

#include <stdint.h>
#include <std/memory>
#include <std/map>
#include <std/string>
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
	virtual int32_t Read(const cxml::Element *elem);
	virtual int32_t Write(common::SharedPtr<File> fp, uint32_t type, uint32_t indent);

	ResourceParam *m_serial;
	Plugin *m_plugin;
	IDParam m_id;
	cxml::Element *m_node;
};

/* the parameters read from a resource, by their tag and ID */
/* the parameters of a resource's table by their tag and ID, and their factories by type */
class VersatileParamManager {
public:
	typedef VersatileParam *(*FactoryFunction)();

	VersatileParamManager()
	{
	}

	~VersatileParamManager();

	typedef map<string, map<IDParam, VersatileParam *> > ParamTable;

	/*
	 * Every parameter, by tag and then by ID: the end has no ID, so a table's
	 * end compares equal whatever its ID (names ours).
	 */
	class iterator {
	public:
		iterator(ParamTable *table, ParamTable::iterator tag,
			 ParamTable::mapped_type::iterator id)
			: m_table(table), m_tag(tag), m_id(id)
		{
		}

		bool operator==(const iterator &other) const
		{
			return m_tag == other.m_tag && (m_tag == m_table->end() || m_id == other.m_id);
		}

		bool operator!=(const iterator &other) const
		{
			return !(*this == other);
		}

		iterator &operator++()
		{
			++m_id;
			if (m_id == m_tag->second.end()) {
				++m_tag;
				if (m_tag != m_table->end())
					m_id = m_tag->second.begin();
			}
			return *this;
		}

		string GetTag() const
		{
			return m_tag->first;
		}

		IDParam GetID() const
		{
			return m_id->first;
		}

		ParamTable *m_table;
		ParamTable::iterator m_tag;
		ParamTable::mapped_type::iterator m_id;
	};

	iterator begin()
	{
		if (m_param_table.begin() == m_param_table.end())
			return iterator(&m_param_table, m_param_table.end(), NULL);
		return iterator(&m_param_table, m_param_table.begin(),
				m_param_table.begin()->second.begin());
	}

	iterator end()
	{
		return iterator(&m_param_table, m_param_table.end(), NULL);
	}

	void AddFactory(const char *name, FactoryFunction factory);
	void AddParam(const char *tag, const IDParam &id, VersatileParam *param);
	VersatileParam *FindParam(const char *tag, const IDParam &id);
	VersatileParam *FindParam(const IDParam &id);

	/* names ours */
	map<string, FactoryFunction> m_factory_table;
	ParamTable m_param_table;
};

}

#endif
