/*
 * paf::ui::Resolution (TOOL 0x81146b20..0x81147828): the screen resolutions the layouts are
 * made for, the ones in use, and the table of the twelve a layout may be made in. No record or
 * string names the file.
 */
#include <stddef.h>
#include <stdint.h>
#include <common/common_internal.h>
#include <graphics/surface/surface_display.h>
#include <widget/core/environment.h>
#include <widget/w_scene.h>

namespace paf {

/* Sony has the append of a resolution out of line. */
template<>
__attribute__((noinline)) int vector<ui::SceneResolution, allocator<ui::SceneResolution> >::push_back(
	const ui::SceneResolution &val)
{
	int result;

	if (count + 1 <= limit || (result = reserve(limit + 10)) != 0) {
		new (&ptr[count]) ui::SceneResolution(val);
		result = ++count;
	}
	return result;
}

namespace ui {

/* the twelve resolutions: qHD, HD and full HD, each for the normal orientation, left, right
   and the pad key sides (ids 0 to 11 as GetResolutionId counts them) */
static SceneResolution s_resolution_table[12];

/* the sizes and the resource variants of the table (Sony has the numbers in the data and sets
   the names from the static initializer) */
static void SetResolutionTableEntry(int32_t index, uint32_t id, uint32_t base_id, const char *name,
				    const char *base_name, int32_t width, int32_t height,
				    int32_t oriented_width, int32_t oriented_height)
{
	SceneResolution *entry = &s_resolution_table[index];

	entry->m_id = id;
	entry->m_base_id = base_id;
	entry->m_resource.m_name = name;
	entry->m_base_resource.m_name = base_name;
	entry->m_viewport_width = width;
	entry->m_viewport_height = height;
	entry->m_scissor_width = width;
	entry->m_scissor_height = height;
	entry->m_width = oriented_width;
	entry->m_height = oriented_height;
}

static void InitResolutionTable()
{
	SetResolutionTableEntry(0, 1, 15, "landscape_padkey_footside", "landscape", 960, 544, 960, 544);
	SetResolutionTableEntry(1, 2, 16, "portrait_padkey_leftside", "portrait", 960, 544, 544, 960);
	SetResolutionTableEntry(2, 0, 15, "landscape_padkey_headside", "landscape", 960, 544, 960, 544);
	SetResolutionTableEntry(3, 3, 16, "portrait_padkey_rightside", "portrait", 960, 544, 544, 960);
	SetResolutionTableEntry(4, 5, 17, "half_hd_landscape_padkey_footside", "half_hd", 1280, 725, 1280, 725);
	SetResolutionTableEntry(5, 6, 18, "half_hd_portrait_padkey_leftside", "half_hd_portrait", 1280, 725, 725, 1280);
	SetResolutionTableEntry(6, 4, 17, "half_hd_landscape_padkey_headside", "half_hd", 1280, 725, 1280, 725);
	SetResolutionTableEntry(7, 7, 18, "half_hd_portrait_padkey_rightside", "half_hd_portrait", 1280, 725, 725, 1280);
	SetResolutionTableEntry(8, 9, 19, "full_hd_landscape_padkey_footside", "full_hd", 1920, 1088, 1920, 1088);
	SetResolutionTableEntry(9, 10, 20, "full_hd_landscape_padkey_footside", "full_hd_portrait", 1920, 1088, 1088, 1920);
	SetResolutionTableEntry(10, 8, 19, "full_hd_landscape_padkey_footside", "full_hd", 1920, 1088, 1920, 1088);
	SetResolutionTableEntry(11, 11, 20, "full_hd_landscape_padkey_footside", "full_hd_portrait", 1920, 1088, 1088, 1920);
}

static struct ResolutionTableInit {
	ResolutionTableInit()
	{
		InitResolutionTable();
	}
} s_resolution_table_init;

vector<SceneResolution> Resolution::s_default_table;
vector<uint32_t> Resolution::s_default_ids;

void Resolution::Init(vector<SceneResolution> *table, vector<uint32_t> *ids)
{
	vector<SceneResolution> *use_table = &s_default_table;
	vector<uint32_t> *use_ids = &s_default_ids;

	if (s_default_table.size() == 0) {
		for (int32_t i = 0; i < 6; i++) {
			s_default_table.push_back(s_resolution_table[i * 2]);
			s_default_table.push_back(s_resolution_table[i * 2 + 1]);
		}
	}
	if (s_default_ids.size() == 0) {
		static const uint32_t default_ids[] = { 1 };

		for (uint32_t i = 0; i < 1; i++)
			s_default_ids.push_back(default_ids[i]);
	}

	if (table != NULL) {
		if (table != &s_default_table) {
			use_table = new vector<SceneResolution>(*table);
			m_own_resolutions = true;
		}
	}
	m_resolutions = use_table;
	if (ids != NULL) {
		if (ids != &s_default_ids) {
			use_ids = new vector<uint32_t>(*ids);
			m_own_ids = true;
		}
	}
	m_ids = use_ids;
}

Resolution::Resolution(const Resolution &obj) : m_own_resolutions(false), m_own_ids(false)
{
	Init(obj.m_resolutions, obj.m_ids);
}

Resolution::Resolution(vector<uint32_t> *ids) : m_own_resolutions(false), m_own_ids(false)
{
	Init(NULL, ids);
}

Resolution::Resolution(vector<SceneResolution> *table, vector<uint32_t> *ids)
	: m_own_resolutions(false), m_own_ids(false)
{
	Init(table, ids);
}

Resolution::Resolution(uint32_t mask) : m_own_resolutions(false), m_own_ids(false)
{
	vector<uint32_t> ids;

	if ((mask & 0x0F000000) == 0)
		mask |= 0x01000000;
	if ((mask & 0x01000000) != 0) {
		if ((mask & 1) != 0)
			ids.push_back(1);
		if ((mask & 2) != 0)
			ids.push_back(5);
		if ((mask & 4) != 0)
			ids.push_back(9);
	}
	if ((mask & 0x02000000) != 0) {
		if ((mask & 1) != 0)
			ids.push_back(2);
		if ((mask & 2) != 0)
			ids.push_back(6);
		if ((mask & 4) != 0)
			ids.push_back(10);
	}
	if ((mask & 0x04000000) != 0) {
		if ((mask & 1) != 0)
			ids.push_back(0);
		if ((mask & 2) != 0)
			ids.push_back(4);
		if ((mask & 4) != 0)
			ids.push_back(8);
	}
	if ((mask & 0x08000000) != 0) {
		if ((mask & 1) != 0)
			ids.push_back(3);
		if ((mask & 2) != 0)
			ids.push_back(7);
		if ((mask & 4) != 0)
			ids.push_back(11);
	}
	if (ids.size() == 0)
		common_internal::on_error();
	Init(NULL, &ids);
}

Resolution::~Resolution()
{
	if (m_own_resolutions && m_resolutions != NULL)
		delete m_resolutions;
	if (m_own_ids && m_ids != NULL)
		delete m_ids;
}

void Resolution::GetScreenSize(uint32_t *width, uint32_t *height)
{
	int32_t max_width = g_screen_width_qhd;
	int32_t max_height = g_screen_height_qhd;

	*width = 0;
	*height = 0;
	ScePafWidget_608BC317(&max_width, &max_height);

	uint32_t best_width = *width;
	uint32_t best_height = *height;

	for (vector<uint32_t>::iterator it = m_ids->begin(); it != m_ids->end(); ++it) {
		SceneResolution *resolution = ScePafWidget_CDC90BA2(this, *it);

		if (resolution != NULL) {
			uint32_t w = resolution->m_scissor_width;
			uint32_t h = resolution->m_scissor_height;

			if ((uint32_t)max_width >= w && w > best_width && h <= (uint32_t)max_height &&
			    h > best_height) {
				*width = w;
				best_width = *width;
				best_height = resolution->m_scissor_height;
				*height = best_height;
			}
		}
	}
	if (best_width == 0) {
		common_internal::on_error();
		best_height = *height;
	}
	if (best_height == 0)
		common_internal::on_error();
}

uint32_t Resolution::GetResolutionIdHD(uint32_t width, uint32_t height, int32_t orientation)
{
	if (width >= g_screen_width_full_hd && height >= g_screen_height_full_hd &&
	    (ScePafWidget_9F450B64(this, 9) || ScePafWidget_9F450B64(this, 10) ||
	     ScePafWidget_9F450B64(this, 8) || ScePafWidget_9F450B64(this, 11))) {
		if (orientation > 1) {
			if (orientation <= 2)
				return ScePafWidget_9F450B64(this, 10) ? 10 : 255;
			if (orientation <= 3)
				return ScePafWidget_9F450B64(this, 11) ? 11 : 255;
		} else if (orientation >= 0) {
			if (orientation <= 0)
				return ScePafWidget_9F450B64(this, 9) ? 9 : 255;
			return ScePafWidget_9F450B64(this, 8) ? 8 : 255;
		}
	}
	if (width >= g_screen_width_hd && height >= g_screen_height_hd &&
	    (ScePafWidget_9F450B64(this, 5) || ScePafWidget_9F450B64(this, 6) ||
	     ScePafWidget_9F450B64(this, 4) || ScePafWidget_9F450B64(this, 7))) {
		if (orientation > 1) {
			if (orientation <= 2)
				return ScePafWidget_9F450B64(this, 6) ? 6 : 255;
			if (orientation <= 3)
				return ScePafWidget_9F450B64(this, 7) ? 7 : 255;
		} else if (orientation >= 0) {
			if (orientation <= 0)
				return ScePafWidget_9F450B64(this, 5) ? 5 : 255;
			return ScePafWidget_9F450B64(this, 4) ? 4 : 255;
		}
	}
	if (orientation <= 1) {
		if (orientation >= 0) {
			if (orientation <= 0)
				return ScePafWidget_9F450B64(this, 1) ? 1 : 255;
			return ScePafWidget_9F450B64(this, 0) ? 0 : 255;
		}
		return 255;
	}
	if (orientation <= 2)
		return ScePafWidget_9F450B64(this, 2) ? 2 : 255;
	if (orientation > 3)
		return 255;
	return ScePafWidget_9F450B64(this, 3) ? 3 : 255;
}

uint32_t Resolution::GetResolutionId(int32_t type, int32_t orientation)
{
	if (type >= 0 && type <= 3) {
		if (orientation > 1) {
			if (orientation <= 2)
				return ScePafWidget_9F450B64(this, 2) ? 2 : 255;
			if (orientation <= 3)
				return ScePafWidget_9F450B64(this, 3) ? 3 : 255;
		} else if (orientation >= 0) {
			if (orientation <= 0)
				return ScePafWidget_9F450B64(this, 1) ? 1 : 255;
			return ScePafWidget_9F450B64(this, 0) ? 0 : 255;
		}
	}
	if (type >= 4 && type <= 7) {
		if (orientation > 1) {
			if (orientation <= 2)
				return ScePafWidget_9F450B64(this, 6) ? 6 : 255;
			if (orientation <= 3)
				return ScePafWidget_9F450B64(this, 7) ? 7 : 255;
		} else if (orientation >= 0) {
			if (orientation <= 0)
				return ScePafWidget_9F450B64(this, 5) ? 5 : 255;
			return ScePafWidget_9F450B64(this, 4) ? 4 : 255;
		}
	}
	if (type < 8 || type > 11)
		return 255;
	if (orientation <= 1) {
		if (orientation >= 0) {
			if (orientation <= 0)
				return ScePafWidget_9F450B64(this, 9) ? 9 : 255;
			return ScePafWidget_9F450B64(this, 8) ? 8 : 255;
		}
		return 255;
	}
	if (orientation <= 2)
		return ScePafWidget_9F450B64(this, 10) ? 10 : 255;
	if (orientation > 3)
		return 255;
	return ScePafWidget_9F450B64(this, 11) ? 11 : 255;
}

extern "C" bool ScePafWidget_9F450B64(const Resolution *param, uint32_t id)
{
	vector<uint32_t>::iterator it = param->m_ids->begin();
	vector<uint32_t>::iterator end = param->m_ids->end();
	vector<uint32_t>::iterator found = it;

	for (; it != end; found = it) {
		if (*it == id)
			break;
		++it;
	}
	return found != end;
}

extern "C" uint32_t ScePafWidget_730C0464(const Resolution *param, uint32_t id)
{
	if (ScePafWidget_9F450B64(param, id))
		return id;
	return 255;
}

extern "C" SceneResolution *ScePafWidget_CDC90BA2(const Resolution *param, uint32_t id)
{
	SceneResolution *result = NULL;
	vector<SceneResolution>::iterator it = param->m_resolutions->begin();
	vector<SceneResolution>::iterator end = param->m_resolutions->end();

	if (it != end) {
		for (; it != end; ++it) {
			if (it->m_id == id) {
				result = &*it;
				break;
			}
		}
	}
	return result;
}

extern "C" SceneResolution *ScePafWidget_615501F0(const Resolution *param, const IDParam &id)
{
	SceneResolution *result = NULL;
	vector<SceneResolution>::iterator it = param->m_resolutions->begin();
	vector<SceneResolution>::iterator end = param->m_resolutions->end();

	if (it != end) {
		uint32_t idhash = id.m_idhash;

		for (; it != end; ++it) {
			if (it->m_resource.m_value == idhash || it->m_base_resource.m_value == idhash) {
				result = &*it;
				break;
			}
		}
	}
	return result;
}

extern "C" int32_t ScePafWidget_36505C86(const Resolution *param, const IDParam &id)
{
	vector<SceneResolution>::iterator it = param->m_resolutions->begin();
	vector<SceneResolution>::iterator end = param->m_resolutions->end();

	if (it == end)
		return -1;

	uint32_t idhash = id.m_idhash;

	for (; it != end; ++it) {
		if (it->m_resource.m_value == idhash)
			return it->m_id;
		if (it->m_base_resource.m_value == idhash)
			return it->m_base_id;
	}
	return -1;
}

extern "C" ResolutionResource ScePafWidget_E963EAEE(const Resolution *param, uint32_t id)
{
	vector<SceneResolution>::iterator it = param->m_resolutions->begin();
	vector<SceneResolution>::iterator end = param->m_resolutions->end();

	for (; it != end; ++it) {
		if (it->m_id == id)
			return it->m_resource;
		if (it->m_base_id == id)
			return it->m_base_resource;
	}

	ResolutionResource empty;

	empty.m_name = "";
	empty.m_value = 0;
	return empty;
}

extern "C" void ScePafWidget_608BC317(int32_t *width, int32_t *height)
{
	graph::DisplaySurface::GetMaximumResolution(width, height);
}

}
}
