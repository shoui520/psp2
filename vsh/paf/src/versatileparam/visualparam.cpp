/*
 * paf::VersatileParamManager, the parameter objects of a plugin's resources by tag and ID,
 * and paf::VisualParam, a parameter object's values as a debug view shows them (the file's
 * name ours).
 */
#include <stddef.h>
#include <stdint.h>
#include <std/algorithm>
#include <versatileparam/versatileparam.h>
#include <widget/core/widget.h>
#include <graphics/color.h>

namespace paf {

VersatileParamManager::~VersatileParamManager()
{
	for (ParamTable::iterator tag = m_param_table.begin(); tag != m_param_table.end(); ++tag) {
		for (ParamTable::mapped_type::iterator id = tag->second.begin();
		     id != tag->second.end(); ++id)
			delete id->second;
	}
}

void VersatileParamManager::AddFactory(const char *name, FactoryFunction factory)
{
	m_factory_table[string(name)] = factory;
}

void VersatileParamManager::AddParam(const char *tag, const IDParam &id, VersatileParam *param)
{
	if (m_param_table[string(tag)].end() != m_param_table[string(tag)].find(id))
		delete m_param_table[string(tag)][id];
	m_param_table[string(tag)][id] = param;
}

VersatileParam *VersatileParamManager::FindParam(const char *tag, const IDParam &id)
{
	ParamTable::iterator table = m_param_table.find(string(tag));

	if (table == m_param_table.end())
		return NULL;

	ParamTable::mapped_type::iterator param = table->second.find(id);

	if (param == table->second.end())
		return NULL;
	return param->second;
}

VersatileParam *VersatileParamManager::FindParam(const IDParam &id)
{
	for (ParamTable::iterator table = m_param_table.begin(); table != m_param_table.end();
	     ++table) {
		ParamTable::mapped_type::iterator param = table->second.find(id);

		if (param != table->second.end())
			return param->second;
	}
	return NULL;
}

/* the list's destructor would not delete the parameters it points to (name ours) */
template<typename T>
static void DeleteParam(T *param)
{
	delete param;
}

VisualParam::VisualParam(VersatileParam *versatile_param, Plugin *plugin)
	: m_versatile_param(versatile_param), m_plugin(plugin), m_id(s_empty_name, 0),
	  m_dirty(false)
{
}

VisualParam::~VisualParam()
{
	for_each(m_param_list.begin(), m_param_list.end(), DeleteParam<ParamBase>);
}

void VisualParam::AddParam(const char *title, bool *var, int32_t visual_type)
{
	m_param_list.push_back(new BoolParam(this, title, var));
}

void VisualParam::AddParam(const char *title, int32_t *var, int32_t min_value, int32_t max_value,
			   int32_t visual_type)
{
	m_param_list.push_back(new IntParam(this, title, var, min_value, max_value));
}

void VisualParam::AddParam(const char *title, float *var, float min_value, float max_value,
			   int32_t visual_type)
{
	m_param_list.push_back(new FloatParam(this, title, var, min_value, max_value));
}

void VisualParam::AddParam(const char *title, math::v2 *var, const math::v2 &min_value,
			   const math::v2 &max_value, int32_t visual_type)
{
	m_param_list.push_back(new Vec2Param(this, title, var, min_value, max_value));
}

void VisualParam::AddParam(const char *title, math::v3 *var, const math::v3 &min_value,
			   const math::v3 &max_value, int32_t visual_type)
{
	m_param_list.push_back(new Vec3Param(this, title, var, min_value, max_value));
}

void VisualParam::AddParam(const char *title, math::v4 *var, const math::v4 &min_value,
			   const math::v4 &max_value, int32_t visual_type)
{
	m_param_list.push_back(new Vec4Param(this, title, var, min_value, max_value));
}

void VisualParam::AddParam(const char *title, IDParam *var)
{
	m_param_list.push_back(new IdParamParam(this, title, var));
}

ui::ResolutionData *VisualParam::ResolutionParam::FindData()
{
	ui::ResolutionData *end = m_data + m_num;

	for (ui::ResolutionData *data = m_data; data < end; data++) {
		if (data->m_id == *m_id || data->m_id == *m_default_id)
			return data;
	}
	return NULL;
}

VisualParam::ResolutionParam::ResolutionParam(VisualParam *visual_param, const char *title,
					      ui::ResolutionData *data, int32_t num,
					      const uint32_t *id, const uint32_t *default_id,
					      const math::v3 &min_value, const math::v3 &max_value)
	: ParamBase(visual_param, title), m_data(data), m_num(num), m_id(id),
	  m_default_id(default_id), m_min(min_value), m_max(max_value)
{
	ui::ResolutionData *found = FindData();

	if (found != NULL)
		m_prev_value = m_curr_value =
			math::v3(found->m_value[0], found->m_value[1], found->m_value[2]);
	else
		m_prev_value = m_curr_value = math::v3(0.0f, 0.0f, 0.0f);
}

void VisualParam::AddParam(const char *title, ui::ResolutionData *data, int32_t num,
			   const uint32_t *id, const uint32_t *default_id,
			   const math::v3 &min_value, const math::v3 &max_value)
{
	m_param_list.push_back(new ResolutionParam(this, title, data, num, id, default_id,
						   min_value, max_value));
}

void VisualParam::AddTextureParam(const char *title, IDParam *var)
{
	m_param_list.push_back(new TextureParam(this, title, var));
}

void VisualParam::SetIDName(IDParam *id)
{
	AddParam("id", id);
	m_id = *id;
}

void VisualParam::Draw()
{
	m_versatile_param->Get();

	string title = m_id.m_id + " - " + m_versatile_param->GetParamName();
	bool dirty = false;

	for (list<ParamBase *>::iterator it = m_param_list.begin(); it != m_param_list.end();
	     ++it) {
		dirty = dirty || (*it)->IsDirty();
		(*it)->Draw();
	}
	if (dirty)
		m_dirty = true;
}

void VisualParam::SetIdParamParam(void *handle, const IDParam &value)
{
}

void VisualParam::SetTextureParam(void *handle, const IDParam &value)
{
}

void VisualParam::BoolParam::Draw()
{
}

void VisualParam::IntParam::Draw()
{
}

void VisualParam::FloatParam::Draw()
{
}

void VisualParam::Vec2Param::Draw()
{
}

void VisualParam::Vec3Param::Draw()
{
}

void VisualParam::Vec4Param::Draw()
{
}

void VisualParam::IdParamParam::Draw()
{
	m_title + " : " + m_var->m_id;
}

bool VisualParam::ResolutionParam::IsDirty()
{
	ui::ResolutionData *data = FindData();

	if (data == NULL)
		return false;

	bool dirty = !(m_curr_value == m_prev_value);

	if (dirty) {
		data->m_value[0] = m_curr_value.extract_x();
		data->m_value[1] = m_curr_value.extract_y();
		data->m_value[2] = m_curr_value.extract_z();
	} else {
		m_curr_value = math::v3(data->m_value[0], data->m_value[1], data->m_value[2]);
	}
	m_prev_value = m_curr_value;
	return dirty;
}

void VisualParam::ResolutionParam::Draw()
{
}

void VisualParam::TextureParam::Draw()
{
	m_title + " : " + m_var->m_id;
}

}
