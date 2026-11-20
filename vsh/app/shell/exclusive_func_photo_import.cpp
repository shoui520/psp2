/*
 * exclusive_func_photo_import.cpp: the photo import's exclusive function (0x810E2F4E-0x810E396A):
 * while it runs, the photo import dialog (photoimport_dialog_plugin with libcdlg_photoimport,
 * from host0: where the QA flags allow it) is loaded into the manager's block, and the photo the
 * user picks, or why none was, is given to the caller once the dialog is unloaded again.
 * "StartExclusiveFuncPhotoImport", "AbortExclusiveFuncPhotoImport", "ExclusiveFuncPhotoImport"
 * and "ExclusiveFuncPhotoImportFinish" are Sony's names (0.990's strings); the file's and the
 * other names are ours (PLACEHOLDER).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/string>
#include <std/string.h>
#include <common/shared_ptr.h>
#include <memory/heap_allocator.h>
#include <graphics/surface/surface_pool.h>
#include <file/localfile.h>
#include <plugin.h>
#include <ss_mgr/ss_mgr.h>
#include <photoimport_dialog.h>
#include "shell.h"
#include "exclusive_func.h"

using namespace paf;

/* the block's use: the dialog's surface pool, its private heap, the item it fills */
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_SURFACE_POOL_SIZE	0x100000
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_HEAP_SIZE		0x100000
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_ITEM_OFFSET		0x300000
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_MEM_SIZE		0x200520

/* the dialog's load options */
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_DRAW_PRIORITY	40
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_MODULE_OPTION	1
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_GRAPHICS_FLAG	7

/* the dialog's arguments: one photo, the process type (PLACEHOLDER: its value's meaning) */
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_SELECT_MODE		0
#define EXCLUSIVE_FUNC_PHOTO_IMPORT_PROCESS_TYPE	0x10000

/*
 * ExclusiveFuncPhotoImport (1068 bytes): the caller's callback and its argument, the dialog's
 * heap and surface pool while it is loaded, the item it fills, and the result the callback gets.
 */
class ExclusiveFuncPhotoImport : public ExclusiveFunc {
public:
	ExclusiveFuncPhotoImport(ExclusiveFuncPhotoImportCallback callback, void *arg)
		: m_callback(callback), m_arg(arg), m_heap(NULL), m_surfacePool(NULL),
		  m_itemData(NULL)
	{
		sce_paf_memset(&m_result, 0, sizeof(m_result));
	}

	virtual ~ExclusiveFuncPhotoImport();

	virtual int Start(const ExclusiveFuncMemBlock *mem);
	virtual int Stop();

	/* the block divided between the dialog's surface pool, heap and item (0x810E3248) */
	int Setup(const ExclusiveFuncMemBlock *mem);
	void Finish(int result);

	ExclusiveFuncPhotoImportCallback m_callback;
	void *m_arg;
	memory::HeapAllocator *m_heap;
	graph::SurfacePool *m_surfacePool;
	ScePhotoImportDialogItemData *m_itemData;
	ExclusiveFuncPhotoImportResult m_result;
};

static ExclusiveFuncPtr s_photoImportFunc;

static void ExclusiveFuncPhotoImportFinish(int32_t result, void *arg);

/* (0x810E2F4E) */
int StartExclusiveFuncPhotoImport(ExclusiveFuncPhotoImportCallback callback, void *arg)
{
	s_photoImportFunc.reset();

	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (mgr == NULL)
		return -1;

	s_photoImportFunc = common::SharedPtr<ExclusiveFuncPhotoImport>(
		new ExclusiveFuncPhotoImport(callback, arg));
	if (s_photoImportFunc.get() == NULL)
		return -1;
	mgr->Request(&s_photoImportFunc);
	return 0;
}

/* (0x810E3174) */
int AbortExclusiveFuncPhotoImport(void)
{
	ExclusiveFuncMgr *mgr = ExclusiveFuncMgr::GetInstance();

	if (s_photoImportFunc.get() != NULL && mgr != NULL) {
		mgr->Cancel(&s_photoImportFunc);
		return 0;
	}
	s_photoImportFunc.reset();
	return -1;
}

/* (0x810E320A, 0x810E3226) */
ExclusiveFuncPhotoImport::~ExclusiveFuncPhotoImport()
{
}

/* (0x810E3248) */
__attribute__((noinline)) int ExclusiveFuncPhotoImport::Setup(const ExclusiveFuncMemBlock *mem)
{
	char *base = (char *)mem->base;

	if (base == NULL || mem->size < EXCLUSIVE_FUNC_PHOTO_IMPORT_MEM_SIZE)
		return -1;

	memory::HeapAllocator::Option option;

	option.disable_dbgprint = false;
	option.entry_max = 0;
	option.warning_size = 0;
	option.disable_assert_on_alloc_failure = true;
	option.memblk_type = memory::AllocationBlockType_User;
	option.alignment = 0;

	m_surfacePool = new graph::SurfacePool(base, EXCLUSIVE_FUNC_PHOTO_IMPORT_SURFACE_POOL_SIZE,
					       "photoimport_dialog_plugin", &option);
	if (m_surfacePool == NULL)
		return -1;
	m_heap = new memory::HeapAllocator(base + EXCLUSIVE_FUNC_PHOTO_IMPORT_SURFACE_POOL_SIZE,
					   EXCLUSIVE_FUNC_PHOTO_IMPORT_HEAP_SIZE,
					   "ScePhotoImportPluginPrivateHeap");
	if (m_heap != NULL) {
		m_itemData = (ScePhotoImportDialogItemData *)
			(base + EXCLUSIVE_FUNC_PHOTO_IMPORT_ITEM_OFFSET);
		return 0;
	}
	m_itemData = NULL;
	if (m_surfacePool != NULL) {
		delete m_surfacePool;
		m_surfacePool = NULL;
	}
	return -1;
}

/* (0x810E3746) the dialog loaded: the function started */
static void exclusiveFuncPhotoImportOnLoaded(Plugin *plugin)
{
	ExclusiveFuncPhotoImport *func =
		(ExclusiveFuncPhotoImport *)(int32_t)plugin->GetArg("exfunc_this", NULL);

	if (func != NULL) {
		if (plugin != NULL)
			plugin->GetInterface(1);
		func->SetStarted();
	}
}

/* (0x810E37B2) the dialog unloaded: its memory freed, the function stopped and the caller told */
static void exclusiveFuncPhotoImportOnUnloaded(void *arg)
{
	ExclusiveFuncPhotoImport *func = (ExclusiveFuncPhotoImport *)arg;

	if (func == NULL)
		return;

	memory::HeapAllocator *heap = func->m_heap;

	func->m_itemData = NULL;
	if (heap != NULL) {
		delete heap;
		func->m_heap = NULL;
	}
	if (func->m_surfacePool != NULL) {
		delete func->m_surfacePool;
		func->m_surfacePool = NULL;
	}
	func->SetStopped();
	if (func->m_callback != NULL)
		func->m_callback(&func->m_result, func->m_arg);
	s_photoImportFunc = ExclusiveFuncPtr();
}

/* (0x810E3334) */
int ExclusiveFuncPhotoImport::Start(const ExclusiveFuncMemBlock *mem)
{
	m_result.path[0] = '\0';
	m_result.result = EXCLUSIVE_FUNC_PHOTO_IMPORT_ABORTED;
	m_result.error = 0;

	int ret = Setup(mem);

	if (ret < 0)
		return ret;

	const char *resourceFile = "vs0:data/external/cdlg/photoimport_dialog_plugin.rco";
	const char *moduleFile = "vs0:sys/external/libcdlg_photoimport.suprx";

	if (sceSblQafMgrIsAllowRemoteSysmoduleLoad() != 0 &&
	    LocalFile::IsFile("host0:vs0/sys/external/libcdlg_photoimport.suprx")) {
		resourceFile = "host0:vs0/data/external/cdlg/photoimport_dialog_plugin.rco";
		moduleFile = "host0:vs0/sys/external/libcdlg_photoimport.suprx";
	}

	Plugin::InitParam param;

	param.name = "photoimport_dialog_plugin";
	param.resource_file = resourceFile;
	param.caller_name = "__main__";
	param.module_file = moduleFile;
	param.draw_priority = EXCLUSIVE_FUNC_PHOTO_IMPORT_DRAW_PRIORITY;
	param.module_interface_version = 0;
	param.module_option = EXCLUSIVE_FUNC_PHOTO_IMPORT_MODULE_OPTION;
	param.unload_callback = exclusiveFuncPhotoImportOnUnloaded;
	param.param = this;
	param.notify_callback = ExclusiveFuncPhotoImportFinish;
	param.arguments[string("exfunc_this")] = Plugin::Argument((int32_t)this);
	param.arguments[string("select_mode")] =
		Plugin::Argument(EXCLUSIVE_FUNC_PHOTO_IMPORT_SELECT_MODE);
	param.arguments[string("process_type")] =
		Plugin::Argument(EXCLUSIVE_FUNC_PHOTO_IMPORT_PROCESS_TYPE);
	param.arguments[string("category")] =
		Plugin::Argument((int32_t)SCE_PHOTOIMPORT_DIALOG_CATEGORY_DEFAULT);
	param.arguments[string("item_data")] = Plugin::Argument((int32_t)m_itemData);
	param.surface_pool = m_surfacePool;
	param.private_heap_size = 0;
	param.private_heap = m_heap;
	param.graphics_flag = EXCLUSIVE_FUNC_PHOTO_IMPORT_GRAPHICS_FLAG;
	Plugin::LoadAsync(param, exclusiveFuncPhotoImportOnLoaded, 0);
	return 0;
}

/* (0x810E3730) */
int ExclusiveFuncPhotoImport::Stop()
{
	Plugin::UnloadAsync("photoimport_dialog_plugin", NULL, 0);
	return 0;
}

/* (0x810E3870) the dialog's result: the photo picked, or why none was; the dialog closed */
void ExclusiveFuncPhotoImport::Finish(int result)
{
	const ScePhotoImportDialogItemData *item = m_itemData;
	ExclusiveFuncPhotoImportResult photo;

	sce_paf_memset(&photo, 0, sizeof(photo));
	switch (result) {
	case 0:
		photo.result = EXCLUSIVE_FUNC_PHOTO_IMPORT_OK;
		sce_paf_strncpy(photo.path, (const char *)item->fileData.fileName, sizeof(photo.path));
		photo.orientation = item->dataSub.orientation;
		photo.format = item->dataSub.format;
		break;
	case 1:
		photo.result = EXCLUSIVE_FUNC_PHOTO_IMPORT_CANCELED;
		break;
	case 2:
		photo.result = EXCLUSIVE_FUNC_PHOTO_IMPORT_ABORTED;
		break;
	default:
		photo.result = EXCLUSIVE_FUNC_PHOTO_IMPORT_ERROR;
		photo.error = result;
		break;
	}
	m_result = photo;
	AbortExclusiveFuncPhotoImport();
}

/* (0x810E3930) the dialog's notification: its result */
static void ExclusiveFuncPhotoImportFinish(int32_t result, void *arg)
{
	if (arg != NULL)
		((ExclusiveFuncPhotoImport *)arg)->Finish(result);
}
