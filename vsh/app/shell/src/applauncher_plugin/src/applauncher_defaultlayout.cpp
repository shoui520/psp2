/*
 * applauncher_defaultlayout.cpp: the home's titles in their default order, the system
 * applications at the slots the theme gives them (0x810C9840-0x810C9AE6). The file's name and
 * the functions' names are ours (PLACEHOLDER).
 */
#include <system/system.h>
#include "applauncher_home.h"
#include "../../../theme.h"

/* the title put at the slot the theme gives it, the pages before it filled with empty slots;
   false when the theme gives it none (0x810C9840) */
static bool applauncherDefaultLayoutAdd(paf::vector<paf::string> *titles, const char *titleId,
					const IconParam *param)
{
	if (param->m_pagePos < 0 || param->m_iconPos < 0)
		return false;
	while (titles->size() < (SceUInt32)(param->m_pagePos * LSDB_PAGE_ICONS + LSDB_PAGE_ICONS))
		titles->push_back(paf::string(""));
	(*titles)[param->m_pagePos * LSDB_PAGE_ICONS + param->m_iconPos] = titleId;
	return true;
}

/* (0x810C996C) */
int applauncherDefaultLayout(paf::vector<paf::string> *titles)
{
	const HomeProperty *home = &shellThemeGetInfo()->get()->m_homeProperty;

	applauncherDefaultLayoutAdd(titles, "NPXS10003", &home->m_browser);
	applauncherDefaultLayoutAdd(titles, "NPXS10010", &home->m_video);
	applauncherDefaultLayoutAdd(titles, "NPXS10009", &home->m_music);
	applauncherDefaultLayoutAdd(titles, "NPXS10012", &home->m_ps3Link);
	applauncherDefaultLayoutAdd(titles, "NPXS10001", &home->m_party);
	applauncherDefaultLayoutAdd(titles, "NPXS10008", &home->m_trophy);
	applauncherDefaultLayoutAdd(titles, "NPXS10005", &home->m_map);
	applauncherDefaultLayoutAdd(titles, "NPXS10000", &home->m_near);
	applauncherDefaultLayoutAdd(titles, "NPXS10026", &home->m_hostCollabo);
	applauncherDefaultLayoutAdd(titles, "NPXS10007", &home->m_welcomePark);
	if (paf::system::Model::GetType() == paf::system::Model::Type_Dolce)
		applauncherDefaultLayoutAdd(titles, "NPXS10098", &home->m_ps4Link);
	else
		applauncherDefaultLayoutAdd(titles, "NPXS10013", &home->m_ps4Link);
	applauncherDefaultLayoutAdd(titles, "NPXS10006", &home->m_friend);
	applauncherDefaultLayoutAdd(titles, "NPXS10072", &home->m_email);
	applauncherDefaultLayoutAdd(titles, "NPXS10091", &home->m_calendar);
	applauncherDefaultLayoutAdd(titles, "NPXS10002", &home->m_store);
	applauncherDefaultLayoutAdd(titles, "NPXS10014", &home->m_message);
	applauncherDefaultLayoutAdd(titles, "NPXS10094", &home->m_parental);
	applauncherDefaultLayoutAdd(titles, "NPXS10004", &home->m_camera);
	applauncherDefaultLayoutAdd(titles, "NPXS10015", &home->m_settings);
	return 1;
}
