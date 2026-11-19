/*
 * applauncher_idulayout.cpp: the home's pages laid out again from the database, with the
 * backgrounds an older system kept in ux0:shell/bgimage.bgd carried over (0x810BD534-
 * 0x810C1BF4). The file's name and the names here are ours (PLACEHOLDER): no record of the 3.6x
 * shell names them.
 */
#include <stddef.h>
#include <string.h>
#include <scetypes.h>
#include <kernel/iofilemgr.h>
#include <kernel/iofilemgr_stat.h>
#include <regmgr/regmgr.h>
#include <plugin.h>
#include <std/stdio.h>
#include <std/stdlib.h>
#include <std/string>
#include <std/vector>
#include <std/list>
#include <std/algorithm>
#include <common/shared_ptr.h>
#include <common/string_util.h>
#include <buffer/buffer.h>
#include <file/localfile.h>
#include <rtc/rtc.h>
#include "../../../shell.h"
#include "../../livespace_db/lsdb_keys.h"
#include "applauncher.h"
#include "applauncher_home.h"

/* Sony calls these out of line here */
PAF_VECTOR_NOINLINE_PUSH_BACK(paf::common::SharedPtr<LsdbPage>,
			      paf::allocator<paf::common::SharedPtr<LsdbPage> >)
PAF_VECTOR_INLINE_DTOR(int)
PAF_VECTOR_NOINLINE_PUSH_BACK(LsdbIconListPtr, paf::allocator<LsdbIconListPtr>)

using namespace paf;

/* the old system's backgrounds: a line "<page key>,<path>" per page */
#define APPLAUNCHER_BGIMAGE_FILE	"ux0:shell/bgimage.bgd"
#define APPLAUNCHER_LAYOUT_FILE		"ux0:shell/applayout.ald"	/* PLACEHOLDER name */
#define APPLAUNCHER_BGIMAGE_SIZE_MIN	3
#define APPLAUNCHER_BGIMAGE_SIZE_MAX	0x4000

/* where a background carried over is written, and where its image may come from */
#define APPLAUNCHER_TABPAPER_DIR	"ur0:shell/tabpaper/"
#define APPLAUNCHER_BGIMAGE_DEVICE	"photo0:"

/* PLACEHOLDER: an icon's type for an application (livespace_db names only the folder's) */
#define APPLAUNCHER_IDU_ICON_APP	0

/* PLACEHOLDER: the folder ids of folders taken off (as applauncherPagesRemoveIcons takes them) */
#define APPLAUNCHER_IDU_REMOVED_ID_MIN	-20000000
#define APPLAUNCHER_IDU_REMOVED_ID_END	-10000000

/* PLACEHOLDER: the photo export library's conversion of an image file into a page's texture
   (0x8136D050): what it reads and writes, and the sizes of what it wrote */
#define PHOTO_EXPORT_CONVERT_JPEG	1
#define PHOTO_EXPORT_CONVERT_PNG	2

struct PhotoExportConvertParam {
	const char *srcPath;
	const char *dstPath;
	int reserved;
	int type;			/* PHOTO_EXPORT_CONVERT_* */
};

struct PhotoExportConvertResult {
	int imageWidth;
	int imageHeight;
	int texWidth;
	int texHeight;
};

int photoExportConvertFile(const PhotoExportConvertParam *param, void *workMemory,
			   SceBool (*cancelFunc)(void *), void *userdata,
			   PhotoExportConvertResult *result);

/* PLACEHOLDER name: the path of the page's background in the old system's lines, when it is an
   image of photo0: (0x810BD534) */
string bgimageFindPath(const string &key, const vector<string> &lines)
{
	static const string s_empty;

	for (unsigned int i = 0; i < lines.size(); i++) {
		const string &line = lines[i];
		list<string> tokens;

		common::string_util::tokenize(tokens, line, 0, ',');
		if (tokens.size() == 2) {
			list<string>::iterator it = tokens.begin();

			if (it->find(key) != string::npos) {
				++it;
				if (it->find(APPLAUNCHER_BGIMAGE_DEVICE) == 0)
					return *it;
				return s_empty;
			}
		}
	}
	return s_empty;
}

/* PLACEHOLDER name: the old system's lines read, without their BOM and line ends; 0 without
   the file or with one too small or too large (0x810BD820) */
int bgimageRead(vector<string> *lines)
{
	LocalFile file;
	LocalFile::OpenArg arg;

	arg.file_type = File::Type_Local;
	arg.filename = APPLAUNCHER_BGIMAGE_FILE;
	arg.flag = SCE_O_RDONLY;
	arg.mode = SCE_STM_FMT;
	if (file.Open(&arg) != 0)
		return 0;

	int64_t size = file.GetFileSize();

	if (size < APPLAUNCHER_BGIMAGE_SIZE_MIN)
		return 0;
	if (size > APPLAUNCHER_BGIMAGE_SIZE_MAX)
		return 0;

	common::SharedPtr<MallocBuffer> buffer = MallocBuffer::Allocate(size);

	if (buffer.get() == NULL)
		return 0;
	if (size != file.Read(buffer->GetAddr(), buffer->GetSize()))
		return 0;

	char *p = (char *)buffer->GetAddr();
	char *end = p + size;

	if (p[0] == '\xEF' && p[1] == '\xBB' && p[2] == '\xBF')
		p += 3;
	if (end <= p)
		return 0;

	while (end > p) {
		char *line = p;

		for (char *q = p; end > q; q++) {
			char c = *q;

			if (c == '\n' || c == '\r') {
				p = q + 1;
				if (end > p && c == '\r' && q[1] == '\n')
					p = q + 2;
				*q = '\0';
				break;
			}
		}
		sce_paf_printf_dummy();
		lines->push_back(string(line));
	}
	return 1;
}

/* PLACEHOLDER name: the old system's backgrounds carried over to the pages, each converted into
   ur0:shell/tabpaper/ and saved with its page (0x810BDB10) */
void bgimageMigrate(const vector<string> *keys, LsdbPageIconLists *pages,
		    unsigned int maxPages)
{
	vector<string> lines;

	if (!bgimageRead(&lines))
		return;

	vector<int> pageNos;
	LsdbPageList pageList;

	sce_paf_printf_dummy();
	for (unsigned int i = 0; i < pages->size(); i++) {
		common::SharedPtr<LsdbPage> page(new LsdbPage);

		page->bgColor = shellStartupGetPageColor();
		if (keys->size() > i && maxPages > i && (*keys)[i].length() != 0) {
			string path = bgimageFindPath((*keys)[i], lines);

			if (path.length() != 0) {
				rtc::Tick tick;
				char dst[1024];
				PhotoExportConvertParam param;
				PhotoExportConvertResult result;

				rtc::GetCurrentTick(&tick);
				sce_paf_snprintf(dst, sizeof(dst), "%s%llu.dds",
						 APPLAUNCHER_TABPAPER_DIR, tick);
				param.srcPath = path.c_str();
				param.dstPath = dst;
				param.reserved = 0;
				if (path.find(".png") != string::npos ||
				    path.find(".PNG") != string::npos)
					param.type = PHOTO_EXPORT_CONVERT_PNG;
				else
					param.type = PHOTO_EXPORT_CONVERT_JPEG;

				int ret = photoExportConvertFile(&param, NULL, NULL, NULL, &result);

				sce_paf_printf_dummy();
				if (ret == 0) {
					page->themeFile = dst;
					page->imageHeight = result.imageHeight;
					page->texWidth = result.texWidth;
					page->texHeight = result.texHeight;
					page->imageWidth = result.imageWidth;
				}
			}
		}
		pageNos.push_back(i);
		pageList.push_back(page);
	}
	SceLsdb_6769DBC4(g_shellLsdbClient->client, &pageNos, &pageList);
}

/* PLACEHOLDER name: the pages' icons compared slot by slot with the database's, and a folder's
   titles the database added found; nothing is done with what differs (0x810BDEE4) */
void iduLayoutCompare(LsdbPageIconLists *pages)
{
	int count = SceLsdb_94128795(g_shellLsdbClient->client);
	LsdbPageIconLists current;

	if (count != 0)
		current.reserve(count);
	for (int i = 0; i < count; i++)
		current.push_back(LsdbIconListPtr(new LsdbIconList));
	SceLsdb_D74AC354(g_shellLsdbClient->client, 0, count, &current);

	for (unsigned int i = 0; i < pages->size(); i++) {
		LsdbIconList *before = (*pages)[i].get();
		LsdbIconList *after = current[i].get();
		LsdbIconPtr beforeSlots[LSDB_PAGE_ICONS];
		LsdbIconPtr afterSlots[LSDB_PAGE_ICONS];

		for (int j = 0; j < (int)before->size(); j++) {
			LsdbIconPtr icon = (*before)[j];

			beforeSlots[icon->pos] = icon;
		}
		for (int j = 0; j < (int)after->size(); j++) {
			LsdbIconPtr icon = (*after)[j];

			afterSlots[icon->pos] = icon;
		}

		for (int j = 0; j < LSDB_PAGE_ICONS; j++) {
			LsdbIconPtr a = beforeSlots[j];
			LsdbIconPtr b = afterSlots[j];

			if (a.get() != NULL && b.get() != NULL) {
				if (a->titleId == b->titleId && a->icon0Type == b->icon0Type &&
				    a->title == b->title && a->iconPath == b->iconPath) {
					/* the same icon */
				}
				if (a->folder.get() != NULL && b->folder.get() != NULL) {
					vector<string> added = b->folder->titleIds;

					for (vector<string>::iterator it = added.begin();
					     it != added.end();) {
						if (find(a->folder->titleIds.begin(),
							 a->folder->titleIds.end(), *it) !=
						    a->folder->titleIds.end())
							it = added.erase(it);
						else
							++it;
					}
				}
			}
		}
	}
}

/* PLACEHOLDER name: a folder icon whose folder was taken off removed from the page (0x810BE6BE) */
int iduLayoutRemoveFolder(const LsdbIconPtr *icon, int pageNo)
{
	LsdbFolder *folder = (*icon)->folder.get();

	if (folder->iconCount == 0)
		return 0;

	int id = folder->id;

	if (id < APPLAUNCHER_IDU_REMOVED_ID_MIN || id >= APPLAUNCHER_IDU_REMOVED_ID_END)
		return 0;

	int slot = (*icon)->pos;
	vector<ApplauncherRemovedIcon> icons;
	ApplauncherRemovedIcon removed;

	removed.m_id = id;
	removed.m_pageIndex = pageNo;
	removed.m_slot = slot;
	icons.push_back(removed);
	applauncherPagesRemoveIcons(&icons);
	return 1;
}

/* PLACEHOLDER name: an icon moved in the database (0x810BE760) */
int iduLayoutMoveIcon(int fromPage, int fromPos, int toPage, int toPos, const LsdbIconPtr *icon)
{
	vector<int> fromPages;
	vector<int> fromPositions;
	vector<int> toPages;
	vector<int> toPositions;

	fromPages.push_back(fromPage);
	fromPositions.push_back(fromPos);
	toPages.push_back(toPage);
	toPositions.push_back(toPos);
	if (fromPage < 0)
		sce_paf_printf_dummy();
	if ((*icon)->type == APPLAUNCHER_IDU_ICON_APP)
		sce_paf_printf_dummy();
	else
		sce_paf_printf_dummy();
	return SceLsdb_BF7AFB42(g_shellLsdbClient->client, &fromPages, &fromPositions, &toPages,
				&toPositions);
}

/* PLACEHOLDER name: an icon's title taken out of a folder; 0 when the folder does not hold it
   (0x810BE8F4) */
int iduLayoutFolderRemoveTitle(const LsdbIconPtr *folder, const LsdbIconPtr *icon)
{
	LsdbFolder *titles = (*folder)->folder.get();

	for (vector<string>::iterator it = titles->titleIds.begin(); it != titles->titleIds.end();
	     ++it) {
		if (*it == (*icon)->titleId) {
			if ((*icon)->type == APPLAUNCHER_IDU_ICON_APP)
				(*folder)->folder->titleIds.erase(it);
			(*folder)->folder->iconCount--;
			return 1;
		}
	}
	return 0;
}

/* PLACEHOLDER name: an icon taken from its list (the one at *index of the page fromPage) into
   another at the position given; out of the folder it was in, the folder's icon removed with
   its last title, and into the folder newFolder (0x810BE9A6) */
int iduLayoutMoveToList(int *index, int pos, const LsdbIconPtr *icon, LsdbIconList *from,
			int fromPage, const LsdbIconPtr *folder, LsdbPageIconLists *pages,
			int folderPage, LsdbIconList *to, const LsdbIconPtr *newFolder)
{
	from->erase(from->begin() + *index);
	(*index)--;
	(*icon)->pos = pos;
	to->push_back(*icon);

	if (folder->get() != NULL && (*folder)->folder.get() != NULL) {
		iduLayoutFolderRemoveTitle(folder, icon);
		if ((*folder)->folder->iconCount != 0) {
			iduLayoutRemoveFolder(folder, folderPage);
		} else {
			LsdbIconListPtr &list = (*pages)[folderPage];

			for (unsigned int i = 0; i < list->size(); i++) {
				const LsdbIconPtr &other = list->at(i);

				if (other->folder.get() != NULL &&
				    other->folder->id == (*folder)->folder->id) {
					list->erase(list->begin() + i);
					if (folderPage == fromPage)
						(*index)--;
					break;
				}
			}
		}
	}

	if (newFolder->get() != NULL) {
		if ((*icon)->type == APPLAUNCHER_IDU_ICON_APP)
			(*newFolder)->folder->titleIds.push_back((*icon)->titleId);
		(*newFolder)->folder->iconCount++;
	}
	return 0;
}

/* PLACEHOLDER name: the last page with a free slot, and its first free slot; 0 when every page
   is full (0x810BEB5E) */
int iduLayoutFindFreeSlot(int *pageNo, int *slot, LsdbPageIconLists *pages)
{
	for (int i = pages->size() - 1; i >= 0; i--) {
		LsdbIconList *icons = (*pages)[i].get();
		bool used[LSDB_PAGE_ICONS];

		memset(used, 0, sizeof(used));
		for (unsigned int j = 0; j < icons->size(); j++)
			used[(*icons)[j]->pos] = true;

		int free = -1;

		for (int j = 0; j < LSDB_PAGE_ICONS; j++) {
			if (!used[j]) {
				free = j;
				break;
			}
		}
		if (free >= 0) {
			*pageNo = i;
			*slot = free;
			return 1;
		}
	}
	return 0;
}

/* PLACEHOLDER name: the icons of a folder read from the database (0x810BEC04) */
int iduLayoutGetFolderIcons(LsdbIconListPtr *icons, int folderId)
{
	LsdbPageIconLists lists;

	lists.push_back(LsdbIconListPtr(new LsdbIconList));

	int ret = SceLsdb_D74AC354(g_shellLsdbClient->client, folderId, 1, &lists);

	if (ret == 0)
		*icons = lists[0];
	return ret;
}

/* PLACEHOLDER name: an empty folder's icon taken off its page; *index moves back when it was
   before it on the same page (0x810BED74) */
int iduLayoutRemoveFolderIcon(int *index, int fromPage, const LsdbIconPtr *folder,
			      LsdbPageIconLists *pages, int folderPage)
{
	if ((*folder)->folder->iconCount == 0) {
		LsdbIconListPtr &list = (*pages)[folderPage];

		for (unsigned int i = 0; i < list->size(); i++) {
			const LsdbIconPtr &other = list->at(i);

			if (other->folder.get() != NULL &&
			    other->folder->id == (*folder)->folder->id) {
				list->erase(list->begin() + i);
				if (folderPage == fromPage)
					(*index)--;
				break;
			}
		}
	}
	return 1;
}

/* PLACEHOLDER name: whether the list holds a system application's icon or the power icon
   (0x810BEE32) */
int iduLayoutHasSystemIcon(const LsdbIconListPtr *icons)
{
	int found = 0;

	for (unsigned int i = 0; i < (*icons)->size(); i++) {
		const LsdbIconPtr &icon = (*icons)->at(i);

		if (icon.get() != NULL &&
		    ((icon->type == APPLAUNCHER_IDU_ICON_APP && applauncherIconIsSystem(&icon)) ||
		     icon->type == APPLAUNCHER_ICON_TYPE_POWER)) {
			found = 1;
			break;
		}
	}
	return found;
}

/* PLACEHOLDER name: the first page with a free slot, and its first free slot; 0 when every page
   is full (0x810BEE84) */
int iduLayoutFindFirstFreeSlot(int *pageNo, int *slot, LsdbPageIconLists *pages)
{
	for (unsigned int i = 0; i < pages->size(); i++) {
		LsdbIconList *icons = (*pages)[i].get();
		bool used[LSDB_PAGE_ICONS];

		memset(used, 0, sizeof(used));
		for (unsigned int j = 0; j < icons->size(); j++)
			used[(*icons)[j]->pos] = true;

		int free = -1;

		for (int j = 0; j < LSDB_PAGE_ICONS; j++) {
			if (!used[j]) {
				free = j;
				break;
			}
		}
		if (free >= 0) {
			*pageNo = i;
			*slot = free;
			return 1;
		}
	}
	return 0;
}

/* PLACEHOLDER name: an icon moved into the first free slot of a page before its own, and the
   move saved (0x810BEF3E) */
int iduLayoutMoveToFreeSlot(LsdbIconList *from, const LsdbIconPtr *icon, int fromPage,
			    LsdbPageIconLists *pages, int *index)
{
	int pageNo = -1;
	int slot = -1;

	if (iduLayoutFindFirstFreeSlot(&pageNo, &slot, pages) && pageNo < fromPage) {
		LsdbIconPtr folder;
		LsdbIconPtr newFolder;
		int pos = (*icon)->pos;

		iduLayoutMoveToList(index, slot, icon, from, fromPage, &folder, pages, -1,
				    (*pages)[pageNo].get(), &newFolder);
		iduLayoutMoveIcon(fromPage, pos, pageNo, slot, icon);
	}
	return 1;
}

/* PLACEHOLDER name: the icon at a position of a list, and its index (0x810BF07E) */
int iduLayoutFindIconAt(LsdbIconList *icons, int pos, LsdbIconPtr *icon, int *index)
{
	for (unsigned int i = 0; i < icons->size(); i++) {
		if (pos == (*icons)[i]->pos) {
			*index = i;
			*icon = (*icons)[i];
			return 1;
		}
	}
	return 0;
}

/* PLACEHOLDER name: where an application is: its page and slot, or, inside a folder, the
   folder's id and the slot in it, with the folder's page and icon (0x810BF12A) */
int iduLayoutFindTitle(int *pageNo, int *pos, int *folderPage, LsdbIconPtr *folderIcon,
		       const string *titleId, LsdbPageIconLists *pages)
{
	for (unsigned int i = 0; i < pages->size(); i++) {
		LsdbIconList *icons = (*pages)[i].get();

		for (unsigned int j = 0; j < icons->size(); j++) {
			const LsdbIconPtr &icon = (*icons)[j];

			if (*titleId == icon->titleId) {
				*pageNo = i;
				*pos = icon->pos;
				return 1;
			}

			LsdbFolder *folder = icon->folder.get();

			if (folder) {
				int found = 0;

				for (vector<string>::iterator it = folder->titleIds.begin();
				     it != folder->titleIds.end(); ++it) {
					if (*it == *titleId)
						found = 1;
				}
				if (found) {
					LsdbPageIconLists lists;

					lists.push_back(LsdbIconListPtr(new LsdbIconList));

					int ret = SceLsdb_D74AC354(g_shellLsdbClient->client,
								   icon->folder->id, 1, &lists);

					if (ret != 0) {
						sce_paf_printf_dummy();
					} else {
						LsdbIconList *inner = lists[0].get();

						for (unsigned int k = 0; k < inner->size(); k++) {
							if (*titleId == (*inner)[k]->titleId) {
								*pageNo = icon->folder->id;
								*pos = (*inner)[k]->pos;
								*folderPage = i;
								*folderIcon = icon;
								return 1;
							}
						}
					}
				}
			}
		}
	}
	sce_paf_printf_dummy();
	return 0;
}

/* PLACEHOLDER name: a page's icons, or a folder's when the number is a folder id */
static LsdbIconListPtr iduLayoutLoadIcons(int folderId)
{
	LsdbIconListPtr icons;
	LsdbPageIconLists lists;

	lists.push_back(LsdbIconListPtr(new LsdbIconList));
	if (SceLsdb_D74AC354(g_shellLsdbClient->client, folderId, 1, &lists) == 0)
		icons = lists[0];
	return icons;
}

/* PLACEHOLDER name: an application moved to a page and slot, swapped with the icon there, and
   the move saved; 0 when it is not found or already there (0x810BF460) */
int iduLayoutMoveTitle(const string *titleId, int toPage, int toPos, LsdbPageIconLists *pages)
{
	if (titleId->length() == 0)
		return 0;

	int pageNo = -1;
	int pos = -1;
	int folderPage = -1;
	LsdbIconPtr folder;

	if (!iduLayoutFindTitle(&pageNo, &pos, &folderPage, &folder, titleId, pages))
		return 0;
	if (toPage == pageNo && toPos == pos)
		return 0;

	/* Sony's scope: the list is released before the message below */
	{
		LsdbIconListPtr from = (pageNo < APPLAUNCHER_IDU_REMOVED_ID_MIN ||
					pageNo >= APPLAUNCHER_IDU_REMOVED_ID_END) ?
				       (*pages)[pageNo] : iduLayoutLoadIcons(pageNo);

		for (int index = 0; index < (int)from->size(); index++) {
			LsdbIconPtr icon = from->at(index);
			LsdbIconPtr other;

			if (pos == icon->pos) {
				sce_paf_printf_dummy();

				int otherIndex = 0;
				int found = iduLayoutFindIconAt((*pages)[toPage].get(), toPos,
								&other, &otherIndex);
				vector<int> fromPages;
				vector<int> fromPositions;
				vector<int> toPages;
				vector<int> toPositions;

				fromPages.push_back(pageNo);
				fromPositions.push_back(pos);
				toPages.push_back(toPage);
				toPositions.push_back(toPos);
				sce_paf_printf_dummy();
				if (found) {
					fromPages.push_back(toPage);
					fromPositions.push_back(toPos);
					toPages.push_back(pageNo);
					toPositions.push_back(pos);
					sce_paf_printf_dummy();
				}
				SceLsdb_BF7AFB42(g_shellLsdbClient->client, &fromPages,
						 &fromPositions, &toPages, &toPositions);
				sce_paf_printf_dummy();

				if (!other.get()) {
					LsdbIconPtr newFolder;

					iduLayoutMoveToList(&index, toPos, &icon, from.get(),
							    pageNo, &folder, pages, folderPage,
							    (*pages)[toPage].get(), &newFolder);
				} else {
					LsdbIconList *to = (*pages)[toPage].get();

					(*from)[index] = other;
					(*to)[otherIndex] = icon;
					icon->pos = toPos;
					other->pos = pos;
					if (folder.get() && folder->folder.get()) {
						iduLayoutFolderRemoveTitle(&folder, &icon);
						if (other.get()) {
							if (other->type == APPLAUNCHER_IDU_ICON_APP)
								folder->folder->titleIds.push_back(
									other->titleId);
							folder->folder->iconCount++;
						}
					}
				}
				return 1;
			}
		}
	}
	sce_paf_printf_dummy();
	return 0;
}

/* PLACEHOLDER name: no title in [first, last) */
static int iduLayoutPageEmpty(const vector<string> *titles, unsigned int first, unsigned int last)
{
	for (unsigned int j = first; j < last; j++) {
		if ((*titles)[j].length() != 0)
			return 0;
	}
	return 1;
}

/* PLACEHOLDER name: the titles of ux0:shell/applayout.ald, ten to a page, up to "--data--", and
   the page backgrounds listed after "--bgimage--"; pages without a title are dropped with their
   background, and at most 100 titles are kept (0x810BFE96) */
int iduLayoutRead(vector<string> *titles, vector<string> *bgimages)
{
	LocalFile file;
	LocalFile::OpenArg arg;

	arg.file_type = File::Type_Local;
	arg.filename = APPLAUNCHER_LAYOUT_FILE;
	arg.flag = SCE_O_RDONLY;
	arg.mode = SCE_STM_FMT;
	if (file.Open(&arg) != 0)
		return 0;

	int64_t size = file.GetFileSize();

	if (size < APPLAUNCHER_BGIMAGE_SIZE_MIN)
		return 0;
	if (size > APPLAUNCHER_BGIMAGE_SIZE_MAX)
		return 0;

	common::SharedPtr<MallocBuffer> buffer = MallocBuffer::Allocate(size);

	if (buffer.get() == NULL)
		return 0;
	if (size != file.Read(buffer->GetAddr(), buffer->GetSize()))
		return 0;

	char *p = (char *)buffer->GetAddr();
	char *end = p + size;

	if (p[0] == '\xEF' && p[1] == '\xBB' && p[2] == '\xBF')
		p += 3;
	if (end <= p)
		return 0;

	int bgimage = 0;

	while (end > p) {
		char *line = p;

		for (char *q = p; end > q; q++) {
			char c = *q;

			if (c == '\n' || c == '\r') {
				p = q + 1;
				if (end > p && c == '\r' && q[1] == '\n')
					p = q + 2;
				*q = '\0';
				break;
			}
		}

		char *comma = sce_paf_strchr(line, ',');

		if (comma)
			*comma = '\0';
		sce_paf_printf_dummy();

		if (sce_paf_strncmp(line, "--data--", sce_paf_strlen("--data--")) == 0)
			break;
		if (sce_paf_strncmp(line, "--bgimage--", sce_paf_strlen("--bgimage--")) == 0) {
			bgimage = 1;
		} else if (sce_paf_strncmp(line, "WelcomePark", sizeof("WelcomePark")) == 0) {
			if (!bgimage)
				titles->push_back(string("NPXS10007"));
		} else if (sce_paf_strncmp(line, "VideoPlayer", sizeof("VideoPlayer")) == 0) {
			if (!bgimage)
				titles->push_back(string("NPXS10010"));
		} else if (sce_paf_strncmp(line, "PS4Link", sizeof("PS4Link")) == 0) {
			if (!bgimage) {
				int ps4link = 0;

				if (sceRegMgrGetKeyInt("/CONFIG/SPECIFIC", "idu_ps4link",
						       &ps4link) != 0 || ps4link != 1)
					titles->push_back(string(""));
				else
					titles->push_back(string("NPXS10013"));
			}
		} else if (bgimage) {
			bgimages->push_back(string(line));
		} else {
			titles->push_back(string(line));
		}
	}

	unsigned int page = 0;

	for (unsigned int i = 0; i < titles->size();) {
		unsigned int last = i + LSDB_PAGE_ICONS;

		if (titles->size() < last)
			last = titles->size();

		if (!iduLayoutPageEmpty(titles, i, last)) {
			i += LSDB_PAGE_ICONS;
			page++;
		} else {
			if (bgimages->size() > page) {
				vector<string>::iterator it = bgimages->begin() + page;

				bgimages->erase(it, it + 1);
			}
			titles->erase(titles->begin() + i, titles->begin() + last);
		}
	}
	if (titles->size() > 100)
		titles->resize(100, string());
	return 1;
}
