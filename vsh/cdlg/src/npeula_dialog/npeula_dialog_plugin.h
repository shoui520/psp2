/*
 * SceNpEulaDialogPlugin's own interface between its files: the dialog's controller, its page,
 * the index of the EULA's files on the server, its parser, and the job that fetches a file over
 * HTTP. The class names and this header's name are ours: no Sony path or record names them
 * (the module's strings name only "SceNpEulaMutex", "SceNPEULAJobManager" and
 * "SceNPEulaHttpJob").
 */
#ifndef NPEULA_DIALOG_NPEULA_DIALOG_PLUGIN_H
#define NPEULA_DIALOG_NPEULA_DIALOG_PLUGIN_H

#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <np/np_common.h>
#include <np_common/np_common.h>
#include <plugin.h>
#include <std/string>
#include <std/map>
#include <common/shared_ptr.h>
#include <buffer/buffer.h>
#include <job/job.h>
#include <thread/thread.h>
#include <graphics/surface/surface.h>
#include <widget/core/widget.h>
#include <widget/core/handler.h>
#include <common/state_check.h>
#include <http/http.h>

namespace paf {
namespace ui {
class Scene;
class RichText;
class ScrollBarTouch;
class BusyIndicator;
class Group;
class Button;
}
}

/* the plugin's errors (names ours) */
#define NPEULA_ERROR_HTTP_POOL		-2146407167	/* 0x80106D01: SceHttp's pool is too small */
#define NPEULA_ERROR_NOT_FOUND		-2146407166	/* 0x80106D02: no file for the account */
#define NPEULA_ERROR_NO_MEMORY		-2146407165	/* 0x80106D03 */
#define NPEULA_ERROR_HTTP_STATUS	-2146407164	/* 0x80106D04: not 200 OK */
#define NPEULA_ERROR_INDEX_FORMAT	-2146407162	/* 0x80106D06 */
#define NPEULA_ERROR_NAME_TOO_LONG	-2146407161	/* 0x80106D07: empty, or too long */
#define NPEULA_ERROR_TOO_MANY_FILES	-2146407160	/* 0x80106D08 */
#define NPEULA_ERROR_INDEX_LANGUAGE	-2146407159	/* 0x80106D09 */
#define NPEULA_ERROR_INDEX_COUNTRY	-2146407158	/* 0x80106D0A */
#define NPEULA_ERROR_INDEX_ID		-2146407157	/* 0x80106D0B: another title's index */
#define NPEULA_ERROR_INDEX_VERSION	-2146407156	/* 0x80106D0C */
#define NPEULA_ERROR_TEXT_SIZE		-2146407155	/* 0x80106D0D: empty, or over NPEULA_TEXT_MAX */

/* the most text the dialog shows */
#define NPEULA_TEXT_MAX			0x20000

/* the controller's states (names ours) */
enum {
	NPEULA_STATE_IDLE = 0,
	NPEULA_STATE_OPEN = 1,		/* the page opens */
	NPEULA_STATE_START = 2,		/* the index is fetched */
	NPEULA_STATE_TEXT_READY = 3,	/* the text has come */
	NPEULA_STATE_WAIT_TEXT = 4,	/* the text shows once its images have come and it is laid out */
	NPEULA_STATE_SET_TEXT = 5,
	NPEULA_STATE_ERROR = 6,
	NPEULA_STATE_ACCEPTED = 7,
	NPEULA_STATE_DECLINED = 8,
	NPEULA_STATE_UP_TO_DATE = 9,	/* the user has agreed to this version already */
	NPEULA_STATE_ABORTED = 10,
	NPEULA_STATE_CANCELED = 11,
	NPEULA_STATE_ERROR_SHOWN = 12,
	NPEULA_STATE_FINISH = 13,	/* the jobs end, then the dialog */
	NPEULA_STATE_CLOSED = 14	/* the only button of the notice */
};

class EulaDialog;

/* the dialog's page: the EULA's text with its images, the buttons, and the busy indicator */
class EulaView {
public:
	EulaView(EulaDialog *owner);
	virtual ~EulaView();
	virtual int Open(paf::Plugin *plugin);
	virtual int Close();
	virtual void SetBusy(bool busy);
	virtual void ShowText(bool show);
	virtual void SetText(const char *text, int size);
	virtual void SetBaseUrl(paf::string url);
	/* a button is pressed */
	virtual void OnButton(paf::ui::Widget *widget);
	/* the text asks for the size of an image (the event's first datum: its index) */
	virtual void OnImageSize(paf::ui::Widget *widget, paf::ui::Event *e);
	/* the text is laid out */
	virtual void OnLayout(paf::ui::Widget *widget);
	/* an item of the text is shown: an image item gets its image */
	virtual void OnItem(paf::ui::Widget *widget, paf::ui::Event *e);
	virtual void AddButtonListener(paf::ui::Widget *widget);
	virtual void RemoveButtonListener(paf::ui::Widget *widget);
	virtual void AddImageSizeListener(paf::ui::Widget *widget);
	virtual void RemoveImageSizeListener(paf::ui::Widget *widget);
	virtual void AddLayoutListener(paf::ui::Widget *widget);
	virtual void RemoveLayoutListener(paf::ui::Widget *widget);
	virtual void AddItemListener(paf::ui::Widget *widget);
	virtual void RemoveItemListener(paf::ui::Widget *widget);

	void SetImage(paf::intrusive_ptr<paf::graph::Surface> surface, int index);

	EulaDialog *m_owner;
	bool m_busy;
	paf::ui::Scene *m_page;
	paf::ui::RichText *m_text;
	paf::ui::ScrollBarTouch *m_scrollBar;
	paf::ui::BusyIndicator *m_busyIndicator;
	/* the accept and decline buttons */
	paf::ui::Group *m_buttons;
	/* cancels; closes the notice */
	paf::ui::Button *m_cancelButton;
	paf::ui::Button *m_acceptButton;
	paf::ui::Button *m_declineButton;
	paf::string m_baseUrl;
	/* the text is laid out */
	bool m_laidOut;
	/* 1: a notice with one button (CommonDialogNpEulaParam::option 1) */
	int m_mode;
};

/* the image of an index in the text, with its URL */
struct EulaImage {
	EulaImage() : url(paf::s_empty_name)
	{
	}

	paf::intrusive_ptr<paf::graph::Surface> surface;
	paf::string url;
};

class EulaHttpJob;

/* a job's function: (size, error) of each piece received, (result, received) at its end */
typedef void (*EulaHttpJobFunction)(int arg1, int arg2, EulaHttpJob *job);

/* what the HTTP request asks while it waits: whether the job is canceled */
class EulaHttpCancelCheck : public StateCheck {
public:
	EulaHttpCancelCheck(const bool *canceled) : m_canceledFlag(canceled), m_canceled(canceled)
	{
	}

	virtual int IsReady()
	{
		return *m_canceled;
	}

	/* set, never read */
	const bool *m_canceledFlag;
	const bool *m_canceled;
};

/* fetches a file of the EULA into a buffer; the dialog's functions get its pieces and its end */
class EulaHttpJob : public paf::job::JobItem {
public:
	EulaHttpJob(paf::string url, paf::common::SharedPtr<paf::Buffer> buffer,
		    EulaHttpJobFunction onReceive, EulaHttpJobFunction onFinish, EulaDialog *owner);
	EulaHttpJob(paf::string url, paf::common::SharedPtr<paf::Buffer> buffer,
		    EulaHttpJobFunction onReceive, EulaHttpJobFunction onFinish, EulaDialog *owner,
		    int id);
	virtual ~EulaHttpJob();
	virtual void Cancel();
	virtual int Run();
	virtual void Finish(int result);

	paf::common::SharedPtr<paf::Buffer> m_buffer;
	EulaDialog *m_owner;
	/* the image's index in the text */
	int m_id;
	EulaHttpJobFunction m_onReceive;
	EulaHttpJobFunction m_onFinish;
	paf::string m_url;
	int m_result;
	int m_received;
	paf::http::Transaction *m_transaction;
	bool m_canceled;
	EulaHttpCancelCheck m_check;
};

/* the dialog: fetches the index, then the account's text and its images, and shows them */
class EulaDialog {
public:
	EulaDialog();
	virtual ~EulaDialog();
	virtual paf::intrusive_ptr<paf::graph::Surface> GetImage(const paf::string &url, int index);
	virtual void StartIndex();
	virtual void SetText();
	virtual void ShowError();
	virtual void Finish(SceInt32 result, SceInt32 errorCode);
	virtual void Update(paf::Plugin *plugin);
	virtual void OnErrorDialog(SceInt32 value, SceInt32 button);
	virtual void OnTextReceive(int size, int error);
	virtual void OnTextFinished(int result, paf::common::SharedPtr<paf::Buffer> buffer, int size);
	virtual void OnIndexFinished(int result, paf::common::SharedPtr<paf::Buffer> buffer, int size);
	virtual void OnImageReceive(int size, int error);
	virtual void OnImageFinished(int result, int received, EulaHttpJob *job);

	void SetState(int state);
	void Init(paf::Plugin *plugin);
	void Term(paf::Plugin *plugin);
	bool IsJobQueueEmpty();

	paf::Plugin *m_plugin;
	int m_state;
	EulaView m_view;
	SceInt32 m_result;
	SceInt32 m_errorCode;
	SceNpCommunicationId m_npCommId;
	SceUInt32 m_version;
	SceUInt32 m_resultVersion;
	paf::common::SharedPtr<paf::Buffer> m_text;
	int m_textSize;
	/* the folder of the title's EULA on the server */
	paf::string m_url;
	paf::job::JobQueue *m_jobQueue;
	bool m_jobsCanceled;
	paf::map<int, EulaImage> m_images;
	paf::thread::Mutex m_mutex;
	bool m_errorShown;
	/* the caller asks for the text whatever version it agreed to */
	int m_option;
};

/* a file of the EULA: its country and language (none: any), its name and version */
class EulaFile {
public:
	EulaFile() : m_language(-1), m_version(0)
	{
		sceClibMemset(&m_country, 0, sizeof(m_country));
		m_country.data[0] = '\0';
		m_country.data[1] = '\0';
		m_name[0] = '\0';
	}

	virtual ~EulaFile()
	{
	}

	int Set(const SceNpCountryCode *country, const char *name, SceUInt32 version, int language);

	static int Create(EulaFile **file);
	static void Destroy(EulaFile *file);

	SceNpCountryCode m_country;
	int m_language;
	char m_name[64];
	SceUInt32 m_version;
};

#define NPEULA_INDEX_FILES_MAX		256

/* the index of the title's EULA: its files */
class EulaIndex {
public:
	EulaIndex(const SceNpCommunicationId *npCommId) : m_npCommId(*npCommId), m_count(0)
	{
		sceClibMemset(m_files, 0, sizeof(m_files));
	}

	virtual ~EulaIndex();

	int AddFile(const EulaFile *file);
	int Find(int language, const SceNpCountryCode *country, EulaFile **file);

	static int Create(const SceNpCommunicationId *npCommId, EulaIndex **index);
	static void Destroy(EulaIndex *index);

	SceNpCommunicationId m_npCommId;
	EulaFile *m_files[NPEULA_INDEX_FILES_MAX];
	int m_count;
};

/* the parser of index.xml: <eula><file ...>name</file>...</eula> */
class EulaIndexParser {
public:
	EulaIndexParser();
	virtual ~EulaIndexParser();

	int Init(const SceNpCommunicationId *npCommId);
	int Parse(const char *data, int size, int last);

	static int Create(EulaIndexParser **parser);
	static void Destroy(EulaIndexParser *parser);

	/* set to -1, never read */
	int m_level;
	int m_error;
	int m_state;
	EulaIndex *m_index;
	NpXmlParser m_parser;
	SceNpCommunicationId m_npCommId;
};

#endif
