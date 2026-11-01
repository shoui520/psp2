/*
 * paf::VersatileParam: a parameter object read from a resource element. Names after
 * ScePaf-RE, the virtual functions in its order.
 */
#ifndef PAF_VERSATILEPARAM_VERSATILEPARAM_H
#define PAF_VERSATILEPARAM_VERSATILEPARAM_H

#include <stdint.h>
#include <std/list>
#include <std/memory>
#include <std/map>
#include <std/string>
#include <common/shared_ptr.h>
#include <cxml/cxml.h>
#include <versatileparam/idparam.h>
#include <math/ngp/v2.h>
#include <math/ngp/v3.h>
#include <math/ngp/v4.h>

namespace paf {

namespace ui {
class ResolutionData;
}

class File;
class Plugin;
class ResourceParam;

class VersatileParam {
public:
	VersatileParam();

	/* the element name of the parameter's type */
	static const char m_param_name[];
	static const char *ParamName();

	virtual const char *GetParamName() const;
	virtual ~VersatileParam();
	virtual int32_t Set()
	{
		return 0;
	}

	virtual int32_t Get()
	{
		return 0;
	}

	virtual int32_t Create(int32_t mode);
	virtual int32_t Destroy(int32_t mode);
	virtual int32_t OnUpdate();
	virtual void OnDraw();
	virtual int32_t AttachPlugin(Plugin *plugin)
	{
		m_plugin = plugin;
		return 0;
	}

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

/*
 * A parameter object's values as a debug view shows them: each value the view may change is
 * kept with its last state, and IsDirty writes a change made on either side to the other.
 * The release build draws nothing. Names after ScePaf-RE.
 */
class VisualParam {
public:
	class ParamBase {
	public:
		ParamBase(VisualParam *visual_param, const string &title)
			: m_visual_param(visual_param), m_title(title)
		{
		}

		virtual ~ParamBase()
		{
		}

		virtual bool IsDirty()
		{
			return false;
		}

		virtual void Draw()
		{
		}

		VisualParam *m_visual_param;
		string m_title;
	};

	template<typename T>
	class ParamWithDirtyFlag : public ParamBase {
	public:
		ParamWithDirtyFlag(VisualParam *visual_param, const string &title, T *var)
			: ParamBase(visual_param, title), m_var(var), m_prev_value(*var), m_curr_value(*var)
		{
		}

		virtual ~ParamWithDirtyFlag()
		{
		}

		virtual bool IsDirty()
		{
			bool dirty = !(m_curr_value == m_prev_value);

			if (dirty)
				*m_var = m_curr_value;
			else
				m_curr_value = *m_var;
			m_prev_value = m_curr_value;
			return dirty;
		}

		T *m_var;
		T m_prev_value;
		T m_curr_value;
	};

	template<typename T>
	class MinMaxParam : public ParamWithDirtyFlag<T> {
	public:
		MinMaxParam(VisualParam *visual_param, const string &title, T *var,
			    const T &min_value, const T &max_value)
			: ParamWithDirtyFlag<T>(visual_param, title, var), m_min(min_value),
			  m_max(max_value), m_org_value(*var)
		{
		}

		virtual ~MinMaxParam()
		{
		}

		T m_min;
		T m_max;
		T m_org_value;
	};

	class BoolParam : public ParamWithDirtyFlag<bool> {
	public:
		BoolParam(VisualParam *visual_param, const string &title, bool *var)
			: ParamWithDirtyFlag<bool>(visual_param, title, var), m_org_value(*var)
		{
		}

		virtual ~BoolParam()
		{
		}

		virtual void Draw();

		uint32_t reserved;
		bool m_org_value;
	};

	class IntParam : public MinMaxParam<int32_t> {
	public:
		IntParam(VisualParam *visual_param, const string &title, int32_t *var,
			 int32_t min_value, int32_t max_value)
			: MinMaxParam<int32_t>(visual_param, title, var, min_value, max_value)
		{
		}

		virtual ~IntParam()
		{
		}

		virtual void Draw();
	};

	class FloatParam : public MinMaxParam<float> {
	public:
		FloatParam(VisualParam *visual_param, const string &title, float *var,
			   float min_value, float max_value)
			: MinMaxParam<float>(visual_param, title, var, min_value, max_value)
		{
		}

		virtual ~FloatParam()
		{
		}

		virtual void Draw();
	};

	class Vec2Param : public MinMaxParam<math::v2> {
	public:
		Vec2Param(VisualParam *visual_param, const string &title, math::v2 *var,
			  const math::v2 &min_value, const math::v2 &max_value)
			: MinMaxParam<math::v2>(visual_param, title, var, min_value, max_value)
		{
		}

		virtual ~Vec2Param()
		{
		}

		virtual void Draw();
	};

	class Vec3Param : public MinMaxParam<math::v3> {
	public:
		Vec3Param(VisualParam *visual_param, const string &title, math::v3 *var,
			  const math::v3 &min_value, const math::v3 &max_value)
			: MinMaxParam<math::v3>(visual_param, title, var, min_value, max_value)
		{
		}

		virtual ~Vec3Param()
		{
		}

		virtual void Draw();
	};

	class Vec4Param : public MinMaxParam<math::v4> {
	public:
		Vec4Param(VisualParam *visual_param, const string &title, math::v4 *var,
			  const math::v4 &min_value, const math::v4 &max_value)
			: MinMaxParam<math::v4>(visual_param, title, var, min_value, max_value)
		{
		}

		virtual ~Vec4Param()
		{
		}

		virtual void Draw();
	};

	class IdParamParam : public ParamBase {
	public:
		IdParamParam(VisualParam *visual_param, const string &title, IDParam *var)
			: ParamBase(visual_param, title), m_var(var), m_org_value(*var)
		{
		}

		virtual ~IdParamParam()
		{
		}

		virtual void Draw();

		IDParam *m_var;
		IDParam m_org_value;
	};

	/*
	 * A value given for each screen resolution: the entry of the current resolution, else
	 * of the default one (names ours).
	 */
	class ResolutionParam : public ParamBase {
	public:
		ResolutionParam(VisualParam *visual_param, const char *title,
				ui::ResolutionData *data, int32_t num, const uint32_t *id,
				const uint32_t *default_id, const math::v3 &min_value,
				const math::v3 &max_value);

		virtual ~ResolutionParam()
		{
		}

		virtual bool IsDirty();
		virtual void Draw();

		ui::ResolutionData *FindData();

		math::v3 m_prev_value;
		math::v3 m_curr_value;
		ui::ResolutionData *m_data;
		int32_t m_num;
		const uint32_t *m_id;
		const uint32_t *m_default_id;
		math::v3 m_min;
		math::v3 m_max;
	};

	class TextureParam : public ParamBase {
	public:
		TextureParam(VisualParam *visual_param, const string &title, IDParam *var)
			: ParamBase(visual_param, title), m_var(var), m_org_value(*var)
		{
		}

		virtual ~TextureParam()
		{
		}

		virtual void Draw();

		IDParam *m_var;
		IDParam m_org_value;
	};

	VisualParam(VersatileParam *versatile_param, Plugin *plugin);
	~VisualParam();

	void AddParam(const char *title, bool *var, int32_t visual_type);
	void AddParam(const char *title, int32_t *var, int32_t min_value, int32_t max_value,
		      int32_t visual_type);
	void AddParam(const char *title, float *var, float min_value, float max_value,
		      int32_t visual_type);
	void AddParam(const char *title, math::v2 *var, const math::v2 &min_value,
		      const math::v2 &max_value, int32_t visual_type);
	void AddParam(const char *title, math::v3 *var, const math::v3 &min_value,
		      const math::v3 &max_value, int32_t visual_type);
	void AddParam(const char *title, math::v4 *var, const math::v4 &min_value,
		      const math::v4 &max_value, int32_t visual_type);
	void AddParam(const char *title, IDParam *var);
	void AddParam(const char *title, ui::ResolutionData *data, int32_t num, const uint32_t *id,
		      const uint32_t *default_id, const math::v3 &min_value,
		      const math::v3 &max_value);
	void AddTextureParam(const char *title, IDParam *var);
	void SetIDName(IDParam *id);
	void Draw();
	void SetIdParamParam(void *handle, const IDParam &value);
	void SetTextureParam(void *handle, const IDParam &value);

	bool GetDirty()
	{
		return m_dirty;
	}

	void SetDirty(bool flag)
	{
		m_dirty = flag;
	}

	VersatileParam *m_versatile_param;
	Plugin *m_plugin;
	IDParam m_id;
	bool m_dirty;
	list<ParamBase *> m_param_list;
};

}

#endif
