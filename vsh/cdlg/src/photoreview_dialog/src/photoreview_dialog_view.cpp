/*
 * ScePhotoReviewDialogPlugin's view (PhotoReviewView): opens the dialog's page, decodes the
 * photo on a background job (ScePhotoReviewPhotoDecodeJob), supports pinch/double-tap/stick
 * zoom and pan, exports the photo on request (ScePhotoReviewPhotoSaveJob), and launches the
 * PS Store if the MakerNote carries a content ID. Job names and resource strings are Sony's;
 * class, method and file names are ours.
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <sceerror.h>
#include <photo_export.h>
#include <std/new>
#include <std/string>
#include <std/string.h>
#include <common/job_slots.h>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <framework.h>
#include <graphics/surface/surface.h>
#include <graphics/surface/surface_pool.h>
#include <image/image.h>
#include <inputdevice/inputdevice.h>
#include <inputdevice/pad/pad.h>
#include <inputdevice/touchscreen/touchscreen.h>
#include <job/job.h>
#include <math/ngp/v2.h>
#include <math/ngp/v4.h>
#include <math/rect.h>
#include <plugin.h>
#include <timer/timer.h>
#include <versatileparam/idparam.h>
#include <widget/w_appicon_simple.h>
#include <widget/w_busyindicator.h>
#include <widget/w_button.h>
#include <widget/w_plane.h>
#include <widget/w_scene.h>
#include <widget/w_scroll_view_base.h>
#include "../../msg_dialog/msg_dialog_util.h"
#include "exif_reader.h"
#include "photo_app_launch_job.h"
#include "photoreview_dialog_plugin.h"

using namespace paf;

namespace paf {
const char s_empty_name[] = "";
}

/* resource ID hashes (Sony's RCO) */
#define PHOTOREVIEW_PAGE_ID			0xC4634C57U
#define PHOTOREVIEW_BTN_BACK_ID			0xC74A06B0U
#define PHOTOREVIEW_BTN_SAVE_ID			0x77F456F6U
#define PHOTOREVIEW_SCROLL_VIEW_ID		0xE03DABECU
#define PHOTOREVIEW_PHOTO_PLANE_ID		0xFB74F705U
#define PHOTOREVIEW_BG_PLANE_ID			0x9FA39B7DU
#define PHOTOREVIEW_CONTENT_PLANE_ID		0x9C9138BFU
#define PHOTOREVIEW_TEX_STORE_ID		0x99B69B8BU
#define PHOTOREVIEW_BUSY_INDICATOR_ID		0xA1AFC402U
#define PHOTOREVIEW_ZOOM_LIMIT_TMPL_ID		0x657366B0U
#define PHOTOREVIEW_ZOOM_LIMIT_PLANE_ID		0x8EE11C05U

/* message text ID hashes */
#define PHOTOREVIEW_MSG_SAVE_CONFIRM		0x13AFB3A4U
#define PHOTOREVIEW_MSG_SAVING			0x626A1CCAU
#define PHOTOREVIEW_MSG_ERR_CANNOT_DISPLAY	0x35F8E580U
#define PHOTOREVIEW_MSG_ERR_NO_SPACE		0xE2A2DDCCU
#define PHOTOREVIEW_MSG_ERR_CANNOT_SAVE		0x640EE2A7U
#define PHOTOREVIEW_MSG_ERR_UNSUPPORTED		0x37B4D36AU
#define PHOTOREVIEW_MSG_ERR_CORRUPT		0x07E31811U
#define PHOTOREVIEW_MSG_ERR_MEMORY_CARD		0xE5EA6CFCU
#define PHOTOREVIEW_MSG_MEMORY_CARD_NAME	0x93777E84U
#define PHOTOREVIEW_MSG_ERR_LIMIT		0xDC27698FU
#define PHOTOREVIEW_MSG_ERR_GENERIC		0xC51AA4A4U

/* photo review / export error codes */
#define PHOTOREVIEW_ERR_DECODE			((SceInt32)0x80103701)
#define PHOTOREVIEW_ERR_NO_SPACE		((SceInt32)0x80103702)
#define PHOTOREVIEW_ERR_CANNOT_SAVE		((SceInt32)0x80103703)
#define PHOTOREVIEW_ERR_04			((SceInt32)0x80103704)
#define PHOTOREVIEW_ERR_UNSUPPORTED		((SceInt32)0x80103705)
#define PHOTOREVIEW_ERR_06			((SceInt32)0x80103706)
#define PHOTOREVIEW_ERR_CORRUPT			((SceInt32)0x80103707)

#define PHOTOEXPORT_ERR_NO_MEMORY_CARD		((SceInt32)0x80101A03)
#define PHOTOEXPORT_ERR_04			((SceInt32)0x80101A04)
#define PHOTOEXPORT_ERR_05			((SceInt32)0x80101A05)
#define PHOTOEXPORT_ERR_06			((SceInt32)0x80101A06)
#define PHOTOEXPORT_ERR_NO_SPACE		((SceInt32)0x80101A07)
#define PHOTOEXPORT_ERR_08			((SceInt32)0x80101A08)
#define PHOTOEXPORT_ERR_09			((SceInt32)0x80101A09)
#define PHOTOEXPORT_ERR_0A			((SceInt32)0x80101A0A)
#define PHOTOEXPORT_ERR_CANCELED		((SceInt32)0x80101A0B)
#define PHOTOEXPORT_ERR_0C			((SceInt32)0x80101A0C)
#define PHOTOEXPORT_ERR_LIMIT			((SceInt32)0x80101A0D)

/* job slots */
enum {
	PHOTOREVIEW_SLOT_DECODE = 1,
	PHOTOREVIEW_SLOT_SAVE = 2,
	PHOTOREVIEW_SLOT_LAUNCH = 3
};

static const math::v2 s_fitSize2(960.0f, 480.0f);
static const math::v4 s_fitSize(960.0f, 480.0f, 0.0f, 0.0f);
static const math::v4 s_viewSizeInfobar(960.0f, 512.0f, 0.0f, 0.0f);
static const math::v4 s_viewSizeFull(960.0f, 544.0f, 0.0f, 0.0f);
static const math::v4 s_zeroPos1(0.0f, 0.0f, 0.0f, 0.0f);
static const math::v4 s_zeroPos2(0.0f, 0.0f, 0.0f, 0.0f);
static const math::v4 s_btnPosInfobar(-448.0f, 208.0f, 0.0f, 0.0f);
static const math::v4 s_btnPosFull(-448.0f, 240.0f, 0.0f, 0.0f);
static const math::v4 s_infobarSize(960.0f, 32.0f, 0.0f, 0.0f);
static const math::v4 s_storeButtonPos(-48.0f, -156.0f, 0.0f, 0.0f);
static const math::v4 s_storeButtonSize(64.0f, 64.0f, 0.0f, 0.0f);
static const math::v2 s_storeButtonFocusSize(74.0f, 74.0f);
static const IDParam s_storeButtonId("button_store");

static float s_pinchStartDist;

class PhotoDecodeJob : public common::SlotJobItem {
public:
	PhotoDecodeJob(const string &path, graph::SurfacePool *pool,
		       memory::HeapAllocator *decodeHeap)
		: SlotJobItem("ScePhotoReviewPhotoDecodeJob", NULL), m_path(path), m_pool(pool),
		  m_option(), m_unused(0), m_surface(), m_result(0), m_imageSize(), m_contentId()
	{
		m_option.decode_heap = decodeHeap;
		m_option.cancel = DecodeCancelCB;
		m_option.cancel_userdata = this;
	}

	virtual ~PhotoDecodeJob()
	{
	}

	static bool DecodeCancelCB(void *userdata)
	{
		return ((PhotoDecodeJob *)userdata)->IsCanceled();
	}

	virtual int Run()
	{
		int32_t err = -1;
		float maxBytes = (float)(m_pool->GetFreeSize() - 256);
		common::SharedPtr<image::Image> img =
			image::Image::Open(m_path.c_str(), NULL, image::ImageFormat_None, 0, 0);

		if (img.get() != NULL) {
			image::ImageExtent ext = img->GetExtent();
			image::ImageFormat fmt = img->GetFormat();

			m_imageSize = math::v4((float)ext.w, (float)ext.h, 0.0f, 0.0f);
			if (m_option.scale.w <= 0 || m_option.scale.h <= 0) {
				int32_t w = ext.w;
				int32_t h = ext.h;

				while ((float)(w * h) * 4.0f > maxBytes) {
					w /= 2;
					h /= 2;
				}
				m_option.scale.w = w;
				m_option.scale.h = h;
				m_option.keep_aspect = false;
				m_option.direct_load = true;
			}
			img.reset();

			if (fmt == image::ImageFormat_Jpg) {
				MpoInfoResult mpoResult = { 0, 0, 0 };

				if (m_option.decode_heap != NULL) {
					MpoInfo mpoInfo(m_option.data_load_heap);
					int ret = mpoInfo.Get(string(m_path.c_str()), &mpoResult);

					if (ret >= 0 && (mpoResult.kind >= 7 || mpoResult.kind == 0)) {
						mpoResult.width = 0;
						mpoResult.height = 0;
					}
				}
				m_option.read_size = mpoResult.height;
				m_option.read_offset = mpoResult.width;
			}

			common::SharedPtr<File> file =
				LocalFile::Open(m_path.c_str(), 0x10001, 0, &err);

			if (file.get() != NULL) {
				intrusive_ptr<graph::Surface> surf =
					graph::Surface::Load(m_pool, file, &m_option);

				m_surface = surf;
				err = m_option.error;
				if (m_surface.get() != NULL && m_surface->GetMemory() != NULL &&
				    fmt == image::ImageFormat_Jpg) {
					MakerNoteReader reader(m_option.decode_heap);

					if (reader.Parse(file) == 0)
						reader.GetString(2, m_contentId);
				}
				file->Close();
			}
		}
		return err;
	}

	virtual void Finish(int result)
	{
		m_result = result;
		SlotJobItem::Finish(result);
	}

	string m_path;
	graph::SurfacePool *m_pool;
	graph::Surface::LoadOption m_option;
	int32_t m_unused;
	intrusive_ptr<graph::Surface> m_surface;
	int32_t m_result;
	math::v4 m_imageSize;
	string m_contentId;
};

class PhotoSaveJob : public common::SlotJobItem {
public:
	PhotoSaveJob(const string &path, void *workMemory, const string &photoTitle,
		     const string &gameTitle, const string &gameComment)
		: SlotJobItem("ScePhotoReviewPhotoSaveJob", NULL), m_path(path),
		  m_workMemory(workMemory), m_result(0), m_photoTitle(photoTitle),
		  m_gameTitle(gameTitle), m_gameComment(gameComment)
	{
	}

	virtual ~PhotoSaveJob()
	{
	}

	virtual int Run()
	{
		ScePhotoExportParam param;

		sce_paf_memset(&param, 0, sizeof(param));
		param.photoTitle = (const SceChar8 *)m_photoTitle.c_str();
		param.gameTitle = (const SceChar8 *)m_gameTitle.c_str();
		param.gameComment = (const SceChar8 *)m_gameComment.c_str();
		return scePhotoExportFromFile((const SceChar8 *)m_path.c_str(), &param,
					      m_workMemory, NULL, NULL, NULL, 0);
	}

	virtual void Finish(int result)
	{
		m_result = result;
		SlotJobItem::Finish(result);
	}

	string m_path;
	void *m_workMemory;
	int32_t m_result;
	string m_photoTitle;
	string m_gameTitle;
	string m_gameComment;
};

template <typename T>
static T *FindTypedChild(DialogPage *page, const IDParam &id, int32_t opt)
{
	ui::Widget *w = page->FindChild(id, opt);

	if (w != NULL && w->IsInherit(T::TypeName()))
		return (T *)w;
	return NULL;
}

template <typename T>
static T *FindTypedWidgetChild(ui::Widget *parent, const IDParam &id, int32_t opt)
{
	ui::Widget *w = parent->FindChild(id, opt);

	if (w != NULL && w->IsInherit(T::TypeName()))
		return (T *)w;
	return NULL;
}

static ui::ScrollView *GetScrollParent(ui::Widget *widget)
{
	ui::Widget *p = widget->m_parent->m_parent;

	if (p != NULL && p->IsInherit(ui::ScrollView::TypeName()))
		return (ui::ScrollView *)p;
	return NULL;
}

static math::v4 GetPinchMin(ui::Widget *w)
{
	if (w->m_pinch_limit == NULL)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	return math::v4(w->m_pinch_limit[0].x, w->m_pinch_limit[0].y, w->m_pinch_limit[0].z, 0.0f);
}

static math::v4 GetPinchMax(ui::Widget *w)
{
	if (w->m_pinch_limit == NULL)
		return math::v4(0.0f, 0.0f, 0.0f, 0.0f);
	return math::v4(w->m_pinch_limit[1].x, w->m_pinch_limit[1].y, w->m_pinch_limit[1].z, 0.0f);
}

PhotoReviewView::PhotoReviewView(Plugin *plugin)
	: DialogPage(plugin, IDParam(s_empty_name, PHOTOREVIEW_PAGE_ID), Plugin::PageOpenParam()),
	  m_path(), m_mode(0), m_uiShown(false), m_hasFatalError(false), m_fatalError(0),
	  m_saveConfirmDialog(), m_saveWaitDialog(), m_errorDialog(),
	  m_photoSize(0.0f, 0.0f, 0.0f, 0.0f), m_fitSize(0.0f, 0.0f, 0.0f, 0.0f),
	  m_reserved624(0.0f, 0.0f, 0.0f, 0.0f), m_reserved640(0.0f, 0.0f), m_decodeHeap(NULL),
	  m_workMemory(NULL), m_infobarShown(1), m_bgColor(1.0f, 1.0f, 1.0f, 1.0f),
	  m_viewSize(s_viewSizeFull), m_photoTitle(""), m_gameTitle(""), m_gameComment(""),
	  m_storeContentId()
{
	m_jobSlots.Grow(4);
	m_jobSlots.SetRemovedFunction(JobRemovedCB, this);
	m_openParam.option = 2;
	m_openParam.timer = new paf::Timer(21.0f, 0, -1, NULL);
}

PhotoReviewView::~PhotoReviewView()
{
}

void PhotoReviewView::SetBgColor(SceInt32 infobarShown, const math::v4 &color)
{
	m_infobarShown = infobarShown;
	m_viewSize = (infobarShown != 0) ? s_viewSizeInfobar : s_viewSizeFull;
	m_bgColor = color;
}

int PhotoReviewView::SetPhoto(const string &path, SceInt32 mode, memory::HeapAllocator *decodeHeap,
			      void *workMemory)
{
	m_path = path;
	m_mode = mode;
	m_decodeHeap = decodeHeap;
	m_workMemory = workMemory;
	return 0;
}

int PhotoReviewView::SetExportParam(const string &photoTitle, const string &gameTitle,
				    const string &gameComment)
{
	m_photoTitle = photoTitle;
	m_gameTitle = gameTitle;
	m_gameComment = gameComment;
	return 0;
}

int PhotoReviewView::Open()
{
	m_hasFatalError = false;
	m_uiShown = false;
	m_fatalError = 0;
	int ret = DialogPage::Open();

	AddInputHandler(this);
	EnableInputHandler(this);
	return ret;
}

int PhotoReviewView::Close()
{
	DisableInputHandler(this);
	RemoveInputHandler(this);
	m_jobSlots.CancelAll();
	return DialogPage::Close();
}

void PhotoReviewView::StartDecode(void)
{
	graph::SurfacePool *pool = m_plugin->m_surface_pool;
	common::SharedPtr<job::JobItem> job(new PhotoDecodeJob(m_path, pool, m_decodeHeap));

	m_jobSlots.Enqueue(PHOTOREVIEW_SLOT_DECODE, job::JobQueue::default_queue, job);
}

void PhotoReviewView::ShowUi(void)
{
	ShowChild(IDParam(s_empty_name, PHOTOREVIEW_BTN_BACK_ID), 100.0f);
	if (m_mode != 1)
		ShowChild(IDParam(s_empty_name, PHOTOREVIEW_BTN_SAVE_ID), 100.0f);
	if (m_storeContentId.length() != 0)
		ShowChild(s_storeButtonId, 100.0f);
	m_uiShown = true;

	ui::ScrollView *sv = FindTypedChild<ui::ScrollView>(
		this, IDParam(s_empty_name, PHOTOREVIEW_SCROLL_VIEW_ID), 0);

	if (sv != NULL) {
		ScePafWidget_929C3278(sv);
		sv->ReleaseFocus();
		sv->EnableFocusEvent(false);
		sv->m_focus_direction_off = true;
	}

	ui::Widget *backBtn = FindChild(IDParam(s_empty_name, PHOTOREVIEW_BTN_BACK_ID), 0);

	if (backBtn != NULL)
		backBtn->SetFocus(NULL);
}

void PhotoReviewView::HideUi(void)
{
	HideChild(IDParam(s_empty_name, PHOTOREVIEW_BTN_BACK_ID), 0.0f);
	HideChild(IDParam(s_empty_name, PHOTOREVIEW_BTN_SAVE_ID), 0.0f);
	HideChild(s_storeButtonId, 0.0f);
	m_uiShown = false;

	ui::ScrollView *sv = FindTypedChild<ui::ScrollView>(
		this, IDParam(s_empty_name, PHOTOREVIEW_SCROLL_VIEW_ID), 0);

	if (sv != NULL) {
		ScePafWidget_00AA6519(sv);
		sv->EnableFocusEvent(true);
		sv->m_focus_direction_off = false;
	}
}

void PhotoReviewView::OpenSaveConfirmDialog(void)
{
	GuiDialogParam param;

	param.plugin = m_plugin;
	param.textId.m_idhash = PHOTOREVIEW_MSG_SAVE_CONFIRM;
	m_saveConfirmDialog.AddListener(this);
	if (m_saveConfirmDialog.Open(param) != 0)
		m_saveConfirmDialog.RemoveListener(this);
}

void PhotoReviewView::OpenSaveWaitDialog(void)
{
	if (m_plugin == NULL)
		return;

	GuiDialogParam param;

	param.plugin = m_plugin;
	param.textId.m_idhash = PHOTOREVIEW_MSG_SAVING;
	m_saveWaitDialog.Open(param);
}

void PhotoReviewView::OpenErrorDialog(SceInt32 error)
{
	if (m_errorDialog.IsOpen() || m_plugin == NULL)
		return;

	GuiDialogParam param;

	param.plugin = m_plugin;
	param.type = error;
	param.option.flags = 10;

	switch (error) {
	case PHOTOREVIEW_ERR_DECODE:
		param.textId.m_idhash = PHOTOREVIEW_MSG_ERR_CANNOT_DISPLAY;
		break;
	case PHOTOREVIEW_ERR_NO_SPACE:
	case PHOTOEXPORT_ERR_NO_SPACE:
		param.textId.m_idhash = PHOTOREVIEW_MSG_ERR_NO_SPACE;
		break;
	case PHOTOREVIEW_ERR_CANNOT_SAVE:
	case PHOTOEXPORT_ERR_08:
	case PHOTOEXPORT_ERR_09:
	case PHOTOEXPORT_ERR_0A:
		param.textId.m_idhash = PHOTOREVIEW_MSG_ERR_CANNOT_SAVE;
		break;
	case PHOTOREVIEW_ERR_UNSUPPORTED:
		param.textId.m_idhash = PHOTOREVIEW_MSG_ERR_UNSUPPORTED;
		break;
	case PHOTOREVIEW_ERR_CORRUPT:
		param.textId.m_idhash = PHOTOREVIEW_MSG_ERR_CORRUPT;
		break;
	case PHOTOEXPORT_ERR_NO_MEMORY_CARD:
		param.text = MsgDialogFormat(
			m_plugin->GetString(IDParam(s_empty_name, PHOTOREVIEW_MSG_ERR_MEMORY_CARD)),
			m_plugin->GetString(IDParam(s_empty_name, PHOTOREVIEW_MSG_MEMORY_CARD_NAME)));
		break;
	case PHOTOEXPORT_ERR_LIMIT:
		param.textId.m_idhash = PHOTOREVIEW_MSG_ERR_LIMIT;
		break;
	default:
		param.textId.m_idhash = PHOTOREVIEW_MSG_ERR_GENERIC;
		param.defaultButton = 1;
		break;
	}

	m_errorDialog.AddListener(this);
	if (param.defaultButton == 1 || param.textId.m_idhash == PHOTOREVIEW_MSG_ERR_GENERIC)
		param.buttonCount = 1;
	if (m_errorDialog.Open(param) != 0)
		m_errorDialog.RemoveListener(this);
}

void PhotoReviewView::DisablePhotoGestures(void)
{
	ui::Plane *plane = FindTypedChild<ui::Plane>(
		this, IDParam(s_empty_name, PHOTOREVIEW_PHOTO_PLANE_ID), 0);

	if (plane == NULL)
		return;
	plane->DisableEvent(0xFFF0);
	plane->SetGestureMask(plane->m_touch_gesture_mask & ~0x32U, true);
	plane->SetDragLimitMin(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	plane->SetDragLimitMax(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
}

void PhotoReviewView::OnInit(int32_t type, ui::Handler *self, ui::Event *e)
{
	ui::Widget *bg = FindChild(IDParam(s_empty_name, PHOTOREVIEW_BG_PLANE_ID), 2);

	if (bg != NULL)
		bg->SetColor(m_bgColor, NULL, 0x10001, 0, 0.0f, NULL, NULL);

	ui::Widget *content = FindChild(IDParam(s_empty_name, PHOTOREVIEW_CONTENT_PLANE_ID), 2);

	if (content != NULL)
		content->SetColor(m_bgColor, NULL, 0x10001, 0, 0.0f, NULL, NULL);

	ListenDecide(IDParam(s_empty_name, PHOTOREVIEW_BTN_BACK_ID));
	if (m_mode == 1)
		HideChild(IDParam(s_empty_name, PHOTOREVIEW_BTN_SAVE_ID), 0.0f);
	else
		ListenDecide(IDParam(s_empty_name, PHOTOREVIEW_BTN_SAVE_ID));

	ui::Plane *contentPlane = FindTypedChild<ui::Plane>(
		this, IDParam(s_empty_name, PHOTOREVIEW_CONTENT_PLANE_ID), 2);

	if (contentPlane != NULL) {
		ui::AppIconSimple *storeBtn = new ui::AppIconSimple(contentPlane, NULL);

		if (storeBtn != NULL) {
			storeBtn->m_name = s_storeButtonId;
			storeBtn->EnableTap(true);
			intrusive_ptr<graph::Surface> tex =
				m_plugin->GetTexture(IDParam(s_empty_name, PHOTOREVIEW_TEX_STORE_ID));

			storeBtn->SetTexture(tex, 0, 0);
			storeBtn->SetAnchor(2, 0, 0, 0);
			storeBtn->SetPos(s_storeButtonPos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
			storeBtn->SetSize(s_storeButtonSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
			storeBtn->SetAppearance(0);
			storeBtn->Hide(0xff, NULL, NULL, 0.0f);
			storeBtn->AddEventCallback(0x10000001, StoreButtonCB, this);
			ui::FocusShape shape;

			shape.m_shape = 4;
			shape.m_size = s_storeButtonFocusSize;
			shape.m_changed = true;
			ScePafWidget_13B246E0(storeBtn, &shape);
		}
	}

	ShowUi();

	ui::Plane *contentPlane2 = FindTypedChild<ui::Plane>(
		this, IDParam(s_empty_name, PHOTOREVIEW_CONTENT_PLANE_ID), 2);

	if (contentPlane2 != NULL) {
		if (m_infobarShown != 0) {
			math::v4 infobarDelta(0.0f, s_infobarSize.extract_y(), 0.0f, 0.0f);
			math::v4 size = *contentPlane2->GetSize(0) - infobarDelta;
			math::v4 pos = contentPlane2->GetPos(0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f)) -
				       infobarDelta * 0.5f;

			contentPlane2->SetPos(pos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
			contentPlane2->SetSize(size, NULL, 0x10004, 0, 0.0f, NULL, NULL);
		}
		contentPlane2->SetColor(math::v4(1.0f, 1.0f, 1.0f, 0.0f), NULL, 0x10001, 0, 0.0f,
					NULL, NULL);

		ui::ScrollView *sv = FindTypedChild<ui::ScrollView>(
			this, IDParam(s_empty_name, PHOTOREVIEW_SCROLL_VIEW_ID), 0);

		if (sv != NULL) {
			sv->SetVerticalScrollBar(NULL);
			sv->SetHorizontalScrollBar(NULL);
			sv->m_is_auto_bound = true;
			math::v4 csize = *contentPlane2->GetSize(0);
			math::v2 minSize(csize.extract_x(), csize.extract_y());

			sv->SetMinViewSize(minSize);

			ui::BusyIndicator *busy = FindTypedWidgetChild<ui::BusyIndicator>(
				sv, IDParam(s_empty_name, PHOTOREVIEW_BUSY_INDICATOR_ID), 0);

			if (busy != NULL)
				busy->Start();
		}
	}

	Attach(FindChild(IDParam(s_empty_name, PHOTOREVIEW_PHOTO_PLANE_ID), 0), 7);
	DisablePhotoGestures();
	MakeButton(FindChild(IDParam(s_empty_name, PHOTOREVIEW_CONTENT_PLANE_ID), 0));
}

void PhotoReviewView::OnDecide(int32_t type, ui::Handler *self, ui::Event *e)
{
	ui::Widget *w = (ui::Widget *)self;

	if (w->m_name.GetIDHash() == PHOTOREVIEW_BTN_BACK_ID) {
		if (m_jobSlots.Contains(PHOTOREVIEW_SLOT_DECODE))
			m_jobSlots.Cancel(PHOTOREVIEW_SLOT_DECODE);
		Notify(PHOTOREVIEW_EVENT_OK, NULL);
	} else if (w->m_name.GetIDHash() == PHOTOREVIEW_BTN_SAVE_ID) {
		OpenSaveConfirmDialog();
	}
}

void PhotoReviewView::OnButton(int32_t type, ui::Handler *self, ui::Event *e)
{
	if ((uint8_t)e->GetDetail() == 29) {
		if (m_uiShown)
			HideUi();
		else
			ShowUi();
	}
}

void PhotoReviewView::OnHidden()
{
	Notify(PHOTOREVIEW_EVENT_CLOSED, NULL);
}

void PhotoReviewView::OnGesture01(int32_t type, ui::Handler *self, ui::Event *e)
{
	if (!m_jobSlots.Contains(PHOTOREVIEW_SLOT_SAVE)) {
		if (m_uiShown)
			HideUi();
		else
			ShowUi();
	}
}

void PhotoReviewView::UpdatePhotoDragLimit(ui::Widget *widget)
{
	ui::Plane *plane = NULL;
	ui::ScrollView *sv = NULL;

	if (widget != NULL && widget->IsInherit(ui::Plane::TypeName()))
		plane = (ui::Plane *)widget;
	if (plane != NULL)
		sv = GetScrollParent(plane);
	if (plane != NULL && sv != NULL) {
		math::v4 pSize = *plane->GetSize(1);
		math::v4 sSize = *sv->GetSize(0);

		if (sSize.extract_x() > pSize.extract_x())
			pSize.set_x(sSize.extract_x());
		if (sSize.extract_y() > pSize.extract_y())
			pSize.set_y(sSize.extract_y());
		plane->SetHitBox(3, pSize, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	}
}

int PhotoReviewView::ClampFocus(math::v2 *focus, const math::rect &viewRect,
				const math::rect &photoRect)
{
	if (viewRect.width() > photoRect.width()) {
		focus->set_x(0.0f);
	} else if (viewRect.right() > photoRect.right()) {
		focus->set_x(focus->extract_x() - (viewRect.right() - photoRect.right()));
	} else if (viewRect.left() < photoRect.left()) {
		focus->set_x(focus->extract_x() - (viewRect.left() - photoRect.left()));
	}

	if (viewRect.height() > photoRect.height()) {
		focus->set_y(0.0f);
	} else if (viewRect.top() > photoRect.top()) {
		focus->set_y(focus->extract_y() - (viewRect.top() - photoRect.top()));
		return 0;
	} else if (viewRect.bottom() < photoRect.bottom()) {
		focus->set_y(focus->extract_y() - (viewRect.bottom() - photoRect.bottom()));
		return 0;
	}
	return 0;
}

void PhotoReviewView::OnGesture13(int32_t type, ui::Handler *self, ui::Event *e)
{
	if ((uint8_t)e->GetDetail() == 27)
		return;
	HideUi();

	math::v2 tapPos((float)e->m_data[0], (float)e->m_data[1]);
	ui::Widget *w = (ui::Widget *)self;

	if (w == NULL || !w->IsInherit(ui::Plane::TypeName()))
		return;
	ui::ScrollView *sv = GetScrollParent(w);

	if (sv == NULL)
		return;

	math::v4 minSize = GetPinchMin(w);
	math::v4 maxSize = GetPinchMax(w);
	math::v4 curSize = *w->GetSize(0);

	if (curSize.extract_x() == minSize.extract_x()) {
		math::v4 targetSize = maxSize;
		math::v2 curFocus = sv->GetViewFocus();
		math::v2 newFocus =
			curFocus + tapPos * (maxSize.extract_x() / curSize.extract_x() - 1.0f);
		math::v4 svSize = *sv->GetSize(0);
		math::rect viewRect(newFocus, math::v2(svSize.extract_x(), svSize.extract_y()));
		math::rect photoRect(math::v2(0.0f, 0.0f),
				     math::v2(targetSize.extract_x(), targetSize.extract_y()));

		ClampFocus(&newFocus, viewRect, photoRect);
		sv->SetMinViewSize(math::v2(maxSize.extract_x(), maxSize.extract_y()));
		w->SetSize(targetSize, new paf::Timer(300.0f, 0, -1, NULL), 0x10004, 1, 0.0f,
			   ZoomAnimEndCB, this);
		sv->SetViewFocus(newFocus, new paf::Timer(300.0f, 0, -1, NULL), false, false);
	} else {
		math::v2 zeroFocus(0.0f, 0.0f);

		sv->SetViewFocus(zeroFocus, new paf::Timer(300.0f, 0, -1, NULL), false, false);
		sv->SetMinViewSize(math::v2(maxSize.extract_x(), maxSize.extract_y()));
		w->SetSize(minSize, new paf::Timer(300.0f, 0, -1, NULL), 0x10004, 1, 0.0f,
			   ZoomAnimEndCB, this);
	}
	UpdatePhotoDragLimit(w);
	ScePafMisc_CAABF0B0(127, 0.25f);
}

void PhotoReviewView::OnGesture14(int32_t type, ui::Handler *self, ui::Event *e)
{
	HideUi();
	ui::Widget *w = (ui::Widget *)self;

	if (w == NULL || !w->IsInherit(ui::Plane::TypeName()))
		return;

	const inputdevice::touchscreen::Data *t1 = NULL;
	const inputdevice::touchscreen::Data *t2 = NULL;

	s_pinchStartDist = 0.0f;
	inputdevice::touchscreen::GetData(0, (uint32_t)e->m_data[0], &t1);
	inputdevice::touchscreen::GetData(0, (uint32_t)e->m_data[1], &t2);
	if (t1 != NULL && t2 != NULL) {
		math::v2 p1((float)t1->x, (float)t1->y);
		math::v2 p2((float)t2->x, (float)t2->y);

		s_pinchStartDist = sce::Vectormath::Simd::Aos::length((p2 - p1).getImpl()).getAsFloat();
	}

	Plugin::TemplateOpenParam tmplParam;

	if (m_plugin->TemplateOpen(GetRoot(),
				   IDParam(s_empty_name, PHOTOREVIEW_ZOOM_LIMIT_TMPL_ID),
				   tmplParam) == 0) {
		ui::Widget *limitPlane =
			FindChild(IDParam(s_empty_name, PHOTOREVIEW_ZOOM_LIMIT_PLANE_ID), 0);

		if (limitPlane != NULL) {
			limitPlane->SetForward(w);
			limitPlane->SetMetaAlpha(0.0f, NULL, 0x10003, 0, 0.0f, NULL, NULL);
			limitPlane->DisableEvent(0xFFF0);
			limitPlane->SetSize(m_viewSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);

			ui::Plane *contentPlane = FindTypedChild<ui::Plane>(
				this, IDParam(s_empty_name, PHOTOREVIEW_CONTENT_PLANE_ID), 2);

			if (contentPlane != NULL) {
				math::v4 csize = *contentPlane->GetSize(0);
				math::v4 cpos = contentPlane->GetPos(
					0, NULL, math::v4(0.0f, 0.0f, 0.0f, 0.0f));

				limitPlane->SetPos(cpos, NULL, 0x10000, 0, 0.0f, NULL, NULL);
				limitPlane->SetSize(csize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
			}
		}
	}
}

void PhotoReviewView::OnGesture15(int32_t type, ui::Handler *self, ui::Event *e)
{
	e->m_type = (e->m_type & 0xF0FFFFFF) | 0x02000000;
	ui::Widget *w = (ui::Widget *)self;

	UpdatePhotoDragLimit(w);
	if (w == NULL || !w->IsInherit(ui::Plane::TypeName()))
		return;

	math::v4 curSize = *w->GetSize(0);
	math::v4 minSize = GetPinchMin(w);
	math::v4 maxSize = GetPinchMax(w);
	bool atMin = (curSize.extract_x() <= minSize.extract_x() + 1.0f ||
		      curSize.extract_y() <= minSize.extract_y() + 1.0f);
	bool atMax = (!atMin && (curSize.extract_x() >= maxSize.extract_x() - 1.0f ||
				 curSize.extract_y() >= maxSize.extract_y() - 1.0f));

	if (atMin || atMax) {
		const inputdevice::touchscreen::Data *t1 = NULL;
		const inputdevice::touchscreen::Data *t2 = NULL;

		inputdevice::touchscreen::GetData(0, (uint32_t)e->m_data[0], &t1);
		inputdevice::touchscreen::GetData(0, (uint32_t)e->m_data[1], &t2);
		if (t1 != NULL && t2 != NULL) {
			math::v2 p1((float)t1->x, (float)t1->y);
			math::v2 p2((float)t2->x, (float)t2->y);
			float dist = sce::Vectormath::Simd::Aos::length((p2 - p1).getImpl()).getAsFloat();
			float alpha = ((atMin && dist < s_pinchStartDist) ||
				       (atMax && dist > s_pinchStartDist))
					      ? 0.3f
					      : 0.0f;
			ui::Widget *limitPlane =
				FindChild(IDParam(s_empty_name, PHOTOREVIEW_ZOOM_LIMIT_PLANE_ID), 0);

			if (limitPlane != NULL)
				limitPlane->SetMetaAlpha(alpha, new paf::Timer(100.0f, 0, -1, NULL),
							 0x10003, 2, 0.0f, NULL, NULL);
		}
	} else {
		ui::Widget *limitPlane =
			FindChild(IDParam(s_empty_name, PHOTOREVIEW_ZOOM_LIMIT_PLANE_ID), 0);

		if (limitPlane != NULL)
			limitPlane->SetMetaAlpha(0.0f, new paf::Timer(100.0f, 0, -1, NULL),
						 0x10003, 2, 0.0f, NULL, NULL);
	}
}

void PhotoReviewView::OnGesture16(int32_t type, ui::Handler *self, ui::Event *e)
{
	UpdatePhotoDragLimit((ui::Widget *)self);
	ui::Widget *limitPlane =
		FindChild(IDParam(s_empty_name, PHOTOREVIEW_ZOOM_LIMIT_PLANE_ID), 0);

	if (limitPlane != NULL)
		limitPlane->DestroyWidget();
}

void PhotoReviewView::EnablePhotoGestures(void)
{
	ui::Plane *plane = FindTypedChild<ui::Plane>(
		this, IDParam(s_empty_name, PHOTOREVIEW_PHOTO_PLANE_ID), 0);

	if (plane == NULL)
		return;
	plane->EnableEvent(0xFFF0);
	plane->SetDragLimitMin(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	plane->SetDragLimitMax(math::v4(0.0f, 0.0f, 0.0f, 0.0f));
	plane->SetDragMode(272);
	plane->SetPinchMode(512);
	plane->SetGestureMask(plane->m_touch_gesture_mask | 0x32U, true);
}

void PhotoReviewView::UpdatePhotoLimits(const math::v4 &fitSize, float aspect)
{
	ui::Plane *plane = FindTypedChild<ui::Plane>(
		this, IDParam(s_empty_name, PHOTOREVIEW_PHOTO_PLANE_ID), 0);

	if (plane == NULL)
		return;

	plane->SetPinchLimitMin(fitSize);
	math::v4 maxSize = fitSize * 4.0f;

	if (m_viewSize.extract_x() > maxSize.extract_x()) {
		maxSize.set_x(m_viewSize.extract_x());
		maxSize.set_y(m_viewSize.extract_x() / aspect);
	} else if (m_viewSize.extract_y() > maxSize.extract_y()) {
		maxSize.set_y(m_viewSize.extract_y());
		maxSize.set_x(m_viewSize.extract_y() * aspect);
	}

	math::v4 capSize = fitSize * 20.0f;

	if (maxSize.extract_x() > capSize.extract_x()) {
		maxSize.set_x(capSize.extract_x());
		maxSize.set_y(capSize.extract_x() / aspect);
	}
	if (maxSize.extract_y() > capSize.extract_y()) {
		maxSize.set_y(capSize.extract_y());
		maxSize.set_x(capSize.extract_y() * aspect);
	}
	plane->SetPinchLimitMax(maxSize);
}

void PhotoReviewView::OnEvent(EventDispatcher *sender, int32_t event, void *arg)
{
	if (sender == &m_saveConfirmDialog) {
		m_saveConfirmDialog.RemoveListener(this);
		if (event == GUI_DIALOG_EVENT_CLOSED) {
			HideUi();
			OpenSaveWaitDialog();
			common::SharedPtr<job::JobItem> job(new PhotoSaveJob(
				m_path, m_workMemory, m_photoTitle, m_gameTitle, m_gameComment));

			m_jobSlots.Enqueue(PHOTOREVIEW_SLOT_SAVE, job::JobQueue::default_queue, job);
		}
	} else if (sender == &m_errorDialog) {
		m_errorDialog.RemoveListener(this);
		if (event == GUI_DIALOG_EVENT_CLOSED) {
			ShowUi();
			if (m_hasFatalError) {
				SceInt32 err = m_fatalError;

				Notify(PHOTOREVIEW_EVENT_RESULT, &err);
			}
		}
	}
}

void PhotoReviewView::OnJobRemoved(uint32_t index, common::SharedPtr<job::JobItem> item)
{
	if (index == PHOTOREVIEW_SLOT_DECODE) {
		PhotoDecodeJob *job = (PhotoDecodeJob *)item.get();

		if (job == NULL)
			return;
		if (job->m_result != 0) {
			OpenErrorDialog(PHOTOREVIEW_ERR_DECODE);
			m_hasFatalError = true;
			m_fatalError = PHOTOREVIEW_ERR_DECODE;
			return;
		}

		m_storeContentId = job->m_contentId;
		intrusive_ptr<graph::Surface> surf = job->m_surface;

		m_photoSize = job->m_imageSize;

		ui::ScrollView *sv = FindTypedChild<ui::ScrollView>(
			this, IDParam(s_empty_name, PHOTOREVIEW_SCROLL_VIEW_ID), 0);

		if (sv != NULL) {
			sv->m_is_auto_bound = true;
			math::v4 svSize = *sv->GetSize(0);

			sv->SetMinViewSize(math::v2(svSize.extract_x(), svSize.extract_y()));
			sv->m_is_auto_snap = false;
		}

		ui::Plane *plane = FindTypedChild<ui::Plane>(
			this, IDParam(s_empty_name, PHOTOREVIEW_PHOTO_PLANE_ID), 0);

		if (plane != NULL) {
			if (m_fitSize == math::v4(0.0f, 0.0f, 0.0f, 0.0f)) {
				float aspect = m_photoSize.extract_x() / m_photoSize.extract_y();
				math::v4 fit(0.0f, 0.0f, 0.0f, 0.0f);

				if (aspect * s_fitSize.extract_y() > s_fitSize.extract_x()) {
					fit.set_x(s_fitSize.extract_x());
					fit.set_y(s_fitSize.extract_x() / aspect);
				} else {
					fit.set_x(aspect * s_fitSize.extract_y());
					fit.set_y(s_fitSize.extract_y());
				}
				if (fit.extract_x() >= 0.0f && fit.extract_x() < 1.0f)
					fit.set_x(1.0f);
				if (fit.extract_y() >= 0.0f && fit.extract_y() < 1.0f)
					fit.set_y(1.0f);
				m_fitSize = fit;
				plane->SetSize(fit, NULL, 0x10004, 0, 0.0f, NULL, NULL);
				UpdatePhotoLimits(fit, aspect);
				plane->SetHitBox(3, m_viewSize, math::v4(0.0f, 0.0f, 0.0f, 0.0f));
				if (!plane->m_focused)
					plane->m_focus_in_group = false;
				EnablePhotoGestures();
			}
			plane->SetTexture(surf, 0, 0);
		}

		ui::Button *saveBtn = FindTypedChild<ui::Button>(
			this, IDParam(s_empty_name, PHOTOREVIEW_BTN_SAVE_ID), 0);

		if (saveBtn != NULL)
			saveBtn->Enable(false);

		ui::Button *storeBtn = FindTypedChild<ui::Button>(this, s_storeButtonId, 0);

		if (storeBtn != NULL && m_storeContentId.length() != 0)
			storeBtn->Enable(false);

		if (m_uiShown)
			ShowUi();

		ui::BusyIndicator *busy = FindTypedChild<ui::BusyIndicator>(
			this, IDParam(s_empty_name, PHOTOREVIEW_BUSY_INDICATOR_ID), 0);

		if (busy != NULL) {
			busy->Stop();
			busy->Hide(0xff, NULL, NULL, 0.0f);
		}
	} else if (index == PHOTOREVIEW_SLOT_SAVE) {
		PhotoSaveJob *job = (PhotoSaveJob *)item.get();

		if (job == NULL)
			return;
		m_saveWaitDialog.Close();
		if (job->m_result != 0) {
			if (job->m_result != PHOTOEXPORT_ERR_CANCELED)
				OpenErrorDialog(job->m_result);
		} else {
			ShowUi();
		}
	}
}

void PhotoReviewView::JobRemovedCB(uint32_t index, common::SharedPtr<job::JobItem> item,
				   void *userdata)
{
	((PhotoReviewView *)userdata)->OnJobRemoved(index, item);
}

void PhotoReviewView::OnZoomAnimEnd(paf::Timer *t, ui::AnimListener *listener)
{
	ui::Widget *w = listener->m_widget;

	if (w == NULL || !w->IsInherit(ui::Plane::TypeName()))
		return;
	ui::ScrollView *sv = GetScrollParent(w);

	if (sv != NULL)
		sv->SetMinViewSize(math::v2(m_viewSize.extract_x(), m_viewSize.extract_y()));
}

void PhotoReviewView::ZoomAnimEndCB(paf::Timer *t, ui::AnimListener *listener, void *data1,
				    void *data2, void *data3)
{
	((PhotoReviewView *)data1)->OnZoomAnimEnd(t, listener);
}

void PhotoReviewView::OnStoreButtonDecide(int32_t type, ui::Handler *self)
{
	ui::Widget *w = (ui::Widget *)self;

	if (w->m_name.GetIDHash() == s_storeButtonId.GetIDHash() && m_storeContentId.length() != 0) {
		string uri = string("psns:browse?content=") + m_storeContentId;
		common::SharedPtr<job::JobItem> job(new PhotoAppLaunchJob(uri));

		m_jobSlots.Enqueue(PHOTOREVIEW_SLOT_LAUNCH, job::JobQueue::default_queue, job);
	}
}

void PhotoReviewView::StoreButtonCB(int32_t type, ui::Handler *self, ui::Event *e, void *userdata)
{
	if (userdata != NULL)
		((PhotoReviewView *)userdata)->OnStoreButtonDecide(type, self);
}

void PhotoReviewView::HandleZoomInput(const inputdevice::pad::Data *data)
{
	ui::Plane *plane = FindTypedChild<ui::Plane>(
		this, IDParam(s_empty_name, PHOTOREVIEW_PHOTO_PLANE_ID), 0);

	if (plane == NULL)
		return;

	math::v4 maxSize = GetPinchMax(plane);
	math::v4 minSize = GetPinchMin(plane);
	math::v4 curSize = *plane->GetSize(0);
	uint16_t ry = data->ry;

	if (ry > 0xFE) {
		math::v4 nextSize = curSize * 0.98f;

		if (nextSize.extract_x() >= minSize.extract_x() &&
		    nextSize.extract_y() >= minSize.extract_y()) {
			ui::ScrollView *sv = GetScrollParent(plane);

			if (sv != NULL) {
				math::v2 focus = sv->GetViewFocus() * 0.98f;

				sv->SetViewFocus(focus, NULL, true, true);
			}
			plane->SetSize(nextSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
		} else {
			plane->SetSize(minSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
		}
	} else if (ry == 0) {
		math::v4 nextSize = curSize * 1.02f;

		if (nextSize.extract_x() <= maxSize.extract_x() &&
		    nextSize.extract_y() <= maxSize.extract_y()) {
			ui::ScrollView *sv = GetScrollParent(plane);

			if (sv != NULL) {
				math::v2 focus = sv->GetViewFocus() * 1.02f;

				sv->SetViewFocus(focus, NULL, true, true);
			}
			plane->SetSize(nextSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
		} else {
			plane->SetSize(maxSize, NULL, 0x10004, 0, 0.0f, NULL, NULL);
		}
	}
}

void PhotoReviewView::OnPadInput(const inputdevice::pad::Data *data,
				 const inputdevice::pad::Data *pre)
{
	if (!IsOpen())
		return;
	if (Framework::Instance()->GetEnvironmentInstance()->m_focus_enabled)
		HandleZoomInput(data);
}
