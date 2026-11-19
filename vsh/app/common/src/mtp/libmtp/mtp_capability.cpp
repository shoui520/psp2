/*
 * The host's capabilityInfo ("CAPABILITY:"): the parser of the document a host sends, the
 * functions and formats it keeps of it, and what the shell asks of them (SceLibMtp_94040962).
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>
#include <std/vector>
#include <system/system.h>
#include <regmgr/regmgr.h>
#include <bXCe/bxce_internal.h>
#include "mtp_internal.h"

/* a function's type attribute */
#define MTP_CAPABILITY_FUNCTION_PHOTO			1
#define MTP_CAPABILITY_FUNCTION_MUSIC			2
#define MTP_CAPABILITY_FUNCTION_VIDEO			3
#define MTP_CAPABILITY_FUNCTION_GAME			4
#define MTP_CAPABILITY_FUNCTION_BACKUP			5
#define MTP_CAPABILITY_FUNCTION_ACCOUNT_LINK		6
#define MTP_CAPABILITY_FUNCTION_SYSTEM_UPDATE		7
#define MTP_CAPABILITY_FUNCTION_NETWORK_RPC		8
#define MTP_CAPABILITY_FUNCTION_CONNECTION_HANDOVER	9

/* an option's name attribute */
#define MTP_CAPABILITY_OPTION_PHYSICAL_VIEW		1
#define MTP_CAPABILITY_OPTION_PLAYLIST			2
#define MTP_CAPABILITY_OPTION_VIEW_REFRESH		3
#define MTP_CAPABILITY_OPTION_CONTENTS_SEARCH		4
#define MTP_CAPABILITY_OPTION_COPY_BACKGROUND		5

/* a format's contentType attribute */
#define MTP_CAPABILITY_CONTENT_IMAGE_JPEG		1
#define MTP_CAPABILITY_CONTENT_IMAGE_PNG		2
#define MTP_CAPABILITY_CONTENT_IMAGE_TIFF		3
#define MTP_CAPABILITY_CONTENT_IMAGE_BMP		4
#define MTP_CAPABILITY_CONTENT_IMAGE_GIF		5
#define MTP_CAPABILITY_CONTENT_IMAGE_MPO		6
#define MTP_CAPABILITY_CONTENT_PHOTOCAM_MOVIE		7
#define MTP_CAPABILITY_CONTENT_AUDIO_MP3		8
#define MTP_CAPABILITY_CONTENT_AUDIO_MP4		9
#define MTP_CAPABILITY_CONTENT_AUDIO_WAV		10
#define MTP_CAPABILITY_CONTENT_VIDEO_MP4		11
#define MTP_CAPABILITY_CONTENT_MARLIN_EST		12
#define MTP_CAPABILITY_CONTENT_MARLIN_VOD		13
#define MTP_CAPABILITY_CONTENT_VITA_APP			14
#define MTP_CAPABILITY_CONTENT_DOLCE_APP		15
#define MTP_CAPABILITY_CONTENT_PSP_GAME			16
#define MTP_CAPABILITY_CONTENT_PSP_SAVEDATA		17
#define MTP_CAPABILITY_CONTENT_PS_GAME			18
#define MTP_CAPABILITY_CONTENT_PSM_APP			19

/* a format's codec, videoCodec and audioCodec attributes */
#define MTP_CAPABILITY_CODEC_AAC			1
#define MTP_CAPABILITY_CODEC_LPCM			2
#define MTP_CAPABILITY_CODEC_MPEG4			3
#define MTP_CAPABILITY_CODEC_AVC			4

/* why the last connection ended: these keep the host's capabilityInfo */
#define MTP_STATUS_REASON_NONE				0
#define MTP_STATUS_REASON_3				3
#define MTP_STATUS_REASON_8				8

/* what SceLibMtp_94040962 asks: a function's option is 0x<function bit>000<option> */
#define MTP_CAPABILITY_QUERY_PHOTO			0x10000
#define MTP_CAPABILITY_QUERY_VIDEO			0x20000
#define MTP_CAPABILITY_QUERY_MUSIC			0x40000
#define MTP_CAPABILITY_QUERY_GAME			0x80000
#define MTP_CAPABILITY_QUERY_CONTENTS_SEARCH		6
#define MTP_CAPABILITY_QUERY_VIEW_REFRESH		7
#define MTP_CAPABILITY_QUERY_COPY_BACKGROUND		9

/* a format element */
struct MtpCapabilityFormat {
	MtpCapabilityFormat() : contentType(0), videoCodec(0), audioCodec(0), systemBackup(0) {}

	int contentType;	/* MTP_CAPABILITY_CONTENT_* */
	int videoCodec;		/* MTP_CAPABILITY_CODEC_* */
	int audioCodec;		/* MTP_CAPABILITY_CODEC_* */
	int systemBackup;	/* type="systemBackup" */
};

PAF_VECTOR_INLINE_DTOR(MtpCapabilityFormat)
PAF_VECTOR_INLINE_DTOR(int)

/* vector<T>'s copy constructor out of line: Sony calls it for a function's vectors */
#define MTP_VECTOR_NOINLINE_COPY(T) \
	namespace paf { \
	template<> inline __attribute__((noinline)) \
	vector<T, allocator<T> >::vector(const vector<T, allocator<T> > &src) : alloc(src.alloc) \
	{ \
		count = 0; \
		limit = 0; \
		T *data = alloc.allocate(src.limit); \
		if (data == NULL) \
			alloc.report_failure(); \
		ptr = data; \
		if (data != NULL) { \
			count = src.count; \
			limit = src.limit; \
			for (size_type i = 0; i < count; ++i) \
				new (&ptr[i]) T(src.ptr[i]); \
		} \
	} \
	}

MTP_VECTOR_NOINLINE_COPY(MtpCapabilityFormat)
MTP_VECTOR_NOINLINE_COPY(int)

/* a function element, with its format and option elements */
struct MtpCapabilityFunction {
	int type;		/* MTP_CAPABILITY_FUNCTION_* */
	int interface;
	paf::vector<MtpCapabilityFormat> formats;
	paf::vector<int> options;	/* MTP_CAPABILITY_OPTION_* */
};

PAF_VECTOR_NOINLINE_PUSH_BACK(MtpCapabilityFunction, paf::allocator<MtpCapabilityFunction>)
PAF_VECTOR_NOINLINE_CLEAR(MtpCapabilityFunction, paf::allocator<MtpCapabilityFunction>)

/* the host's capabilityInfo */
struct MtpCapabilityInfo {
	MtpCapabilityInfo() : valid(false) {}

	bool valid;		/* the document's end was read */
	paf::string version;
	paf::vector<MtpCapabilityFunction> functions;
};

static MtpCapabilityInfo *s_capabilityInfo;

/* the capabilityInfo element's handler: the element it reads, kept until its end */
class CapabilityInfoHandler : public bXCe::Sax::DocumentHandler {
public:
	CapabilityInfoHandler() : m_format(NULL), m_option(NULL), m_function(NULL) {}
	virtual ~CapabilityInfoHandler();
	int Initialize();
	int Terminate();
	virtual int startDocument(void *userData);
	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList);
	virtual int characters(void *userData, const char *ch, int length);
	virtual int endElement(void *userData, const char *name, int length);
	virtual int endDocument(void *userData);
	virtual int skippedText(void *userData, int tokenType, const char *text, int length);
	virtual int entityData(void *userData, int entityType, const char *name, int length);
	virtual void fatalError(void *userData, int errCode);

private:
	int parseOptionAttribute(const bXCe::Attr *attr);
	int parseFormatAttribute(const bXCe::Attr *attr);
	int parseFunctionAttribute(const bXCe::Attr *attr);

	MtpCapabilityFormat *m_format;
	int *m_option;
	MtpCapabilityFunction *m_function;
};

int CapabilityInfoHandler::Terminate()
{
	if (m_format != NULL) {
		delete m_format;
		m_format = NULL;
	}
	if (m_option != NULL) {
		delete m_option;
		m_option = NULL;
	}
	if (m_function != NULL) {
		delete m_function;
		m_function = NULL;
	}
	return 0;
}

CapabilityInfoHandler::~CapabilityInfoHandler()
{
	Terminate();
}

int CapabilityInfoHandler::Initialize()
{
	m_format = new MtpCapabilityFormat;
	if (m_format == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	m_option = new int(0);
	if (m_option == NULL) {
		if (m_format != NULL)
			delete m_format;
		m_format = NULL;
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	m_function = new MtpCapabilityFunction;
	if (m_function == NULL) {
		if (m_format != NULL)
			delete m_format;
		m_format = NULL;
		if (m_option != NULL)
			delete m_option;
		m_option = NULL;
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	return 0;
}

int CapabilityInfoHandler::startDocument(void *userData)
{
	return 0;
}

int CapabilityInfoHandler::parseOptionAttribute(const bXCe::Attr *attr)
{
	const char *name;
	const char *value;
	int nameLength;
	int valueLength;

	if (attr == NULL)
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	name = attr->getName();
	value = attr->getValue();
	nameLength = attr->getNameLength();
	valueLength = attr->getValueLength();
	if (sce_paf_strncmp("name", name, nameLength + 1) == 0) {
		if (sce_paf_strncmp("physicalView", value, valueLength + 1) == 0)
			*m_option = MTP_CAPABILITY_OPTION_PHYSICAL_VIEW;
		else if (sce_paf_strncmp("playlist", value, valueLength + 1) == 0)
			*m_option = MTP_CAPABILITY_OPTION_PLAYLIST;
		else if (sce_paf_strncmp("viewRefresh", value, valueLength + 1) == 0)
			*m_option = MTP_CAPABILITY_OPTION_VIEW_REFRESH;
		else if (sce_paf_strncmp("contentsSearch", value, valueLength + 1) == 0)
			*m_option = MTP_CAPABILITY_OPTION_CONTENTS_SEARCH;
		else if (sce_paf_strncmp("copyBackground", value, valueLength + 1) == 0)
			*m_option = MTP_CAPABILITY_OPTION_COPY_BACKGROUND;
	}
	return 0;
}

int CapabilityInfoHandler::parseFormatAttribute(const bXCe::Attr *attr)
{
	const char *name;
	const char *value;
	int nameLength;
	int valueLength;

	if (attr == NULL)
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	name = attr->getName();
	value = attr->getValue();
	nameLength = attr->getNameLength() + 1;
	valueLength = attr->getValueLength();
	if (sce_paf_strncmp("contentType", name, nameLength) == 0) {
		if (m_function->type == MTP_CAPABILITY_FUNCTION_PHOTO) {
			if (sce_paf_strncmp("image/jpeg", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_IMAGE_JPEG;
			else if (sce_paf_strncmp("image/png", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_IMAGE_PNG;
			else if (sce_paf_strncmp("image/tiff", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_IMAGE_TIFF;
			else if (sce_paf_strncmp("image/bmp", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_IMAGE_BMP;
			else if (sce_paf_strncmp("image/gif", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_IMAGE_GIF;
			else if (sce_paf_strncmp("image/x-mpo", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_IMAGE_MPO;
			else if (sce_paf_strncmp("video/x-photocam-movie", value,
						 valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_PHOTOCAM_MOVIE;
		} else if (m_function->type == MTP_CAPABILITY_FUNCTION_MUSIC) {
			if (sce_paf_strncmp("audio/mp3", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_AUDIO_MP3;
			else if (sce_paf_strncmp("audio/mp4", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_AUDIO_MP4;
			else if (sce_paf_strncmp("audio/wav", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_AUDIO_WAV;
		} else if (m_function->type == MTP_CAPABILITY_FUNCTION_VIDEO) {
			if (sce_paf_strncmp("video/mp4", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_VIDEO_MP4;
			else if (sce_paf_strncmp("video/x-marlin-est", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_MARLIN_EST;
			else if (sce_paf_strncmp("video/x-marlin-vod", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_MARLIN_VOD;
		} else if (m_function->type == MTP_CAPABILITY_FUNCTION_GAME) {
			if (sce_paf_strncmp("vitaApp", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_VITA_APP;
			else if (sce_paf_strncmp("dolceApp", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_DOLCE_APP;
			else if (sce_paf_strncmp("PSPGame", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_PSP_GAME;
			else if (sce_paf_strncmp("PSPSaveData", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_PSP_SAVEDATA;
			else if (sce_paf_strncmp("PSGame", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_PS_GAME;
			else if (sce_paf_strncmp("PSMApp", value, valueLength + 1) == 0)
				m_format->contentType = MTP_CAPABILITY_CONTENT_PSM_APP;
		}
	} else if (sce_paf_strncmp("codec", name, nameLength) == 0) {
		if (m_function->type == MTP_CAPABILITY_FUNCTION_MUSIC) {
			if (sce_paf_strncmp("AAC", value, valueLength + 1) == 0)
				m_format->audioCodec = MTP_CAPABILITY_CODEC_AAC;
			else if (sce_paf_strncmp("LPCM", value, valueLength + 1) == 0)
				m_format->audioCodec = MTP_CAPABILITY_CODEC_LPCM;
		}
	} else if (sce_paf_strncmp("videoCodec", name, nameLength) == 0) {
		if (m_function->type == MTP_CAPABILITY_FUNCTION_VIDEO) {
			if (sce_paf_strncmp("MPEG4", value, valueLength + 1) == 0)
				m_format->videoCodec = MTP_CAPABILITY_CODEC_MPEG4;
			else if (sce_paf_strncmp("AVC", value, valueLength + 1) == 0)
				m_format->videoCodec = MTP_CAPABILITY_CODEC_AVC;
		}
	} else if (sce_paf_strncmp("audioCodec", name, nameLength) == 0) {
		if (m_function->type == MTP_CAPABILITY_FUNCTION_VIDEO &&
		    sce_paf_strncmp("AAC", value, valueLength + 1) == 0)
			m_format->audioCodec = MTP_CAPABILITY_CODEC_AAC;
	} else if (sce_paf_strncmp("type", name, nameLength) == 0) {
		if (sce_paf_strncmp("systemBackup", value, valueLength + 1) == 0)
			m_format->systemBackup = 1;
	}
	return 0;
}

int CapabilityInfoHandler::parseFunctionAttribute(const bXCe::Attr *attr)
{
	const char *name;
	const char *value;
	int nameLength;
	int valueLength;

	if (attr == NULL)
		return SCE_MTP_ERROR_NOT_INITIALIZED;
	name = attr->getName();
	value = attr->getValue();
	nameLength = attr->getNameLength() + 1;
	valueLength = attr->getValueLength();
	if (sce_paf_strncmp("type", name, nameLength) == 0) {
		if (sce_paf_strncmp("photo", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_PHOTO;
		else if (sce_paf_strncmp("music", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_MUSIC;
		else if (sce_paf_strncmp("video", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_VIDEO;
		else if (sce_paf_strncmp("game", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_GAME;
		else if (sce_paf_strncmp("backup", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_BACKUP;
		else if (sce_paf_strncmp("accountLink", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_ACCOUNT_LINK;
		else if (sce_paf_strncmp("systemUpdate", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_SYSTEM_UPDATE;
		else if (sce_paf_strncmp("networkRpc", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_NETWORK_RPC;
		else if (sce_paf_strncmp("connectionHandover", value, valueLength + 1) == 0)
			m_function->type = MTP_CAPABILITY_FUNCTION_CONNECTION_HANDOVER;
	} else if (sce_paf_strncmp("interface", name, nameLength) == 0) {
		m_function->interface = sce_paf_atoi(value);
	}
	return 0;
}

int CapabilityInfoHandler::startElement(void *userData, const char *name, int length,
	const bXCe::AttributeList *attrList)
{
	const bXCe::Attr *attr;
	const char *attrName;
	const char *value;
	int num;
	int i;

	num = attrList->getLength();
	if (sce_paf_strncmp("capabilityInfo", name, length + 1) == 0) {
		for (i = 0; i < num; i++) {
			attr = (const bXCe::Attr *)attrList->getAttribute(i);
			if (attr == NULL)
				continue;
			attrName = attr->getName();
			value = attr->getValue();
			if (sce_paf_strncmp("version", attrName, attr->getNameLength() + 1) == 0)
				s_capabilityInfo->version = value;
		}
	} else if (sce_paf_strncmp("function", name, length + 1) == 0) {
		m_function->formats.clear();
		m_function->options.clear();
		m_function->type = 0;
		m_function->interface = 0;
		for (i = 0; i < num; i++) {
			attr = (const bXCe::Attr *)attrList->getAttribute(i);
			if (attr != NULL)
				parseFunctionAttribute(attr);
		}
	} else if (sce_paf_strncmp("format", name, length + 1) == 0) {
		m_format->contentType = 0;
		m_format->videoCodec = 0;
		m_format->audioCodec = 0;
		m_format->systemBackup = 0;
		for (i = 0; i < num; i++) {
			attr = (const bXCe::Attr *)attrList->getAttribute(i);
			if (attr != NULL)
				parseFormatAttribute(attr);
		}
	} else if (sce_paf_strncmp("option", name, length + 1) == 0) {
		*m_option = 0;
		for (i = 0; i < num; i++) {
			attr = (const bXCe::Attr *)attrList->getAttribute(i);
			if (attr != NULL)
				parseOptionAttribute(attr);
		}
	}
	return 0;
}

int CapabilityInfoHandler::characters(void *userData, const char *ch, int length)
{
	return 0;
}

int CapabilityInfoHandler::endElement(void *userData, const char *name, int length)
{
	if (sce_paf_strncmp("capabilityInfo", name, length + 1) == 0)
		s_capabilityInfo->valid = true;
	else if (sce_paf_strncmp("function", name, length + 1) == 0)
		s_capabilityInfo->functions.push_back(*m_function);
	else if (sce_paf_strncmp("format", name, length + 1) == 0)
		m_function->formats.push_back(*m_format);
	else if (sce_paf_strncmp("option", name, length + 1) == 0)
		m_function->options.push_back(*m_option);
	return 0;
}

int CapabilityInfoHandler::endDocument(void *userData)
{
	return 0;
}

int CapabilityInfoHandler::skippedText(void *userData, int tokenType, const char *text,
	int length)
{
	return 0;
}

int CapabilityInfoHandler::entityData(void *userData, int entityType, const char *name,
	int length)
{
	return 0;
}

void CapabilityInfoHandler::fatalError(void *userData, int errCode)
{
	MTP_ERR("CAPABILITY:", 451, "Error: 0x%x\n", errCode);
}

CapabilityInfoParser::~CapabilityInfoParser()
{
	if (m_handler != NULL) {
		m_handler->Terminate();
		delete m_handler;
	}
	if (m_parser != NULL) {
		m_parser->terminate();
		delete m_parser;
	}
}

/* forget the host's capabilityInfo; a connection that ended keeps it unless forced */
void mtpCapabilityInfoClear(bool force)
{
	MtpStatus status;
	paf::vector<MtpCapabilityFunction>::iterator it;

	if (!force && mtpDevGetIfType() == MTP_IF_TYPE_NET) {
		mtpStatusGet(&status);
		if (status.reason != MTP_STATUS_REASON_NONE && status.reason != MTP_STATUS_REASON_8 &&
		    status.reason != MTP_STATUS_REASON_3)
			return;
	}
	s_capabilityInfo->valid = false;
	s_capabilityInfo->version.clear();
	for (it = s_capabilityInfo->functions.begin(); it != s_capabilityInfo->functions.end();
	     ++it) {
		it->formats.clear();
		it->options.clear();
	}
	s_capabilityInfo->functions.clear();
}

int CapabilityInfoParser::Initialize()
{
	bXCe::Sax::Parser *parser;
	CapabilityInfoHandler *handler;
	int ret;

	mtpCapabilityInfoClear(true);
	parser = new bXCe::Sax::Parser;
	if (parser == NULL) {
		MTP_ERR("CAPABILITY:", 471, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = parser->initialize(NULL);
	if (ret < 0) {
		MTP_ERR("CAPABILITY:", 477, "Parser Initialize failed: 0x%x\n", ret);
		delete parser;
		return ret;
	}
	parser->setResolveEntity(true);
	parser->setResolveAttributeEntity(true);
	parser->setSkipIgnorableWhiteSpace(true);
	handler = new CapabilityInfoHandler;
	if (handler == NULL) {
		MTP_ERR("CAPABILITY:", 488, "no memory\n");
		delete parser;
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = handler->Initialize();
	if (ret < 0) {
		delete handler;
		delete parser;
		return ret;
	}
	parser->setDocumentHandler(handler);
	m_parser = parser;
	m_handler = handler;
	return 0;
}

int CapabilityInfoParser::Terminate()
{
	int ret = 0;
	int handlerRet = 0;

	if (m_handler != NULL) {
		handlerRet = m_handler->Terminate();
		delete m_handler;
		m_handler = NULL;
	}
	if (m_parser != NULL) {
		ret = m_parser->terminate();
		delete m_parser;
		m_parser = NULL;
	}
	if (handlerRet != 0)
		return handlerRet;
	return ret;
}

int CapabilityInfoParser::Parse(const char *data, int length, bool isFinal)
{
	const SceUChar8 *end = (const SceUChar8 *)data + length;

	if (isFinal) {
		while (length != 0 && end[-1] == '\0') {
			end--;
			length--;
		}
	}
	return m_parser->parse(data, length, isFinal);
}

int mtpCapabilityInfoInitialize(void)
{
	if (s_capabilityInfo == NULL)
		s_capabilityInfo = new MtpCapabilityInfo;
	if (s_capabilityInfo == NULL)
		return SCE_MTP_ERROR_NO_MEMORY;
	return 0;
}

void mtpCapabilityInfoFinalize(void)
{
	mtpCapabilityInfoClear(false);
	/* Sony's test is inverted: the info is never freed */
	if (s_capabilityInfo == NULL)
		s_capabilityInfo = NULL;
}

/* whether the host's function of a type has an option */
static int mtpCapabilityInfoHasOption(int type, int option, SceUChar8 *supported)
{
	MtpCapabilityInfo *info;
	paf::vector<MtpCapabilityFunction>::iterator it;
	paf::vector<int>::iterator opt;
	int found;

	*supported = false;
	info = s_capabilityInfo;
	found = 0;
	if (info == NULL || !info->valid)
		return SCE_MTP_ERROR_NO_HOST;
	for (it = info->functions.begin(); it != info->functions.end(); ++it) {
		if (it->type == type) {
			for (opt = it->options.begin(); opt != it->options.end(); ++opt) {
				if (*opt == option) {
					*supported = true;
					found = 1;
					break;
				}
			}
		}
		if (found)
			break;
	}
	return 0;
}

extern "C" {

/* whether the host or the console supports a feature */
int SceLibMtp_94040962(int feature, SceUChar8 *supported)
{
	MtpNetHost host;
	paf::vector<MtpCapabilityFunction>::iterator it;
	paf::vector<MtpCapabilityFormat>::iterator format;
	int found;
	int ret = 0;

	if (supported == NULL)
		return SCE_MTP_ERROR_INVALID_ARGUMENT;
	if ((SceUInt8)feature == MTP_CAPABILITY_QUERY_VIEW_REFRESH) {
		if (mtpDevGetIfType() == MTP_IF_TYPE_NET) {
			ret = mtpDevGetHostInfo(&host);
			if (ret == 0 && host.type == MTP_NET_HOST_TYPE_PS3) {
				*supported = true;
				return 0;
			}
		} else if (mtpDevGetIfType() == MTP_IF_TYPE_USB &&
			   mtpStatusGetPlatformClass() == MTP_PLATFORM_CLASS_PS) {
			*supported = true;
			return 0;
		}
	}
	switch (feature) {
	case 1:
		*supported = paf::system::Model::GetType() != 1;
		break;
	case 2:
		*supported = true;
		break;
	case 3:
	case 4:
		*supported = sceRegMgrSrvGetRegion() != MTP_REGION_CHINA;
		break;
	case 5:
		*supported = sceRegMgrSrvGetRegion() == 1;
		break;
	case 8:
		*supported = false;
		found = 0;
		if (s_capabilityInfo == NULL || !s_capabilityInfo->valid) {
			ret = SCE_MTP_ERROR_NO_HOST;
			break;
		}
		for (it = s_capabilityInfo->functions.begin();
		     it != s_capabilityInfo->functions.end(); ++it) {
			if (it->type == MTP_CAPABILITY_FUNCTION_VIDEO) {
				for (format = it->formats.begin(); format != it->formats.end();
				     ++format) {
					if (format->contentType == MTP_CAPABILITY_CONTENT_MARLIN_VOD) {
						*supported = true;
						found = 1;
						break;
					}
				}
			}
			if (found)
				break;
		}
		ret = 0;
		break;
	case 10:
		*supported = mtpBgCopyIsServiceIdle() != 0;
		break;
	case 11:
		*supported = mtpBgCopyNotifyService(0, 0) == 0;
		break;
	case MTP_CAPABILITY_QUERY_PHOTO | MTP_CAPABILITY_QUERY_CONTENTS_SEARCH:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_PHOTO,
			MTP_CAPABILITY_OPTION_CONTENTS_SEARCH, supported);
		break;
	case MTP_CAPABILITY_QUERY_PHOTO | MTP_CAPABILITY_QUERY_VIEW_REFRESH:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_PHOTO,
			MTP_CAPABILITY_OPTION_VIEW_REFRESH, supported);
		break;
	case MTP_CAPABILITY_QUERY_PHOTO | MTP_CAPABILITY_QUERY_COPY_BACKGROUND:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_PHOTO,
			MTP_CAPABILITY_OPTION_COPY_BACKGROUND, supported);
		break;
	case MTP_CAPABILITY_QUERY_VIDEO | MTP_CAPABILITY_QUERY_CONTENTS_SEARCH:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_VIDEO,
			MTP_CAPABILITY_OPTION_CONTENTS_SEARCH, supported);
		break;
	case MTP_CAPABILITY_QUERY_VIDEO | MTP_CAPABILITY_QUERY_VIEW_REFRESH:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_VIDEO,
			MTP_CAPABILITY_OPTION_VIEW_REFRESH, supported);
		break;
	case MTP_CAPABILITY_QUERY_VIDEO | MTP_CAPABILITY_QUERY_COPY_BACKGROUND:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_VIDEO,
			MTP_CAPABILITY_OPTION_COPY_BACKGROUND, supported);
		break;
	case MTP_CAPABILITY_QUERY_MUSIC | MTP_CAPABILITY_QUERY_CONTENTS_SEARCH:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_MUSIC,
			MTP_CAPABILITY_OPTION_CONTENTS_SEARCH, supported);
		break;
	case MTP_CAPABILITY_QUERY_MUSIC | MTP_CAPABILITY_QUERY_VIEW_REFRESH:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_MUSIC,
			MTP_CAPABILITY_OPTION_VIEW_REFRESH, supported);
		break;
	case MTP_CAPABILITY_QUERY_MUSIC | MTP_CAPABILITY_QUERY_COPY_BACKGROUND:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_MUSIC,
			MTP_CAPABILITY_OPTION_COPY_BACKGROUND, supported);
		break;
	case MTP_CAPABILITY_QUERY_GAME | MTP_CAPABILITY_QUERY_CONTENTS_SEARCH:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_GAME,
			MTP_CAPABILITY_OPTION_CONTENTS_SEARCH, supported);
		break;
	case MTP_CAPABILITY_QUERY_GAME | MTP_CAPABILITY_QUERY_VIEW_REFRESH:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_GAME,
			MTP_CAPABILITY_OPTION_VIEW_REFRESH, supported);
		break;
	case MTP_CAPABILITY_QUERY_GAME | MTP_CAPABILITY_QUERY_COPY_BACKGROUND:
		ret = mtpCapabilityInfoHasOption(MTP_CAPABILITY_FUNCTION_GAME,
			MTP_CAPABILITY_OPTION_COPY_BACKGROUND, supported);
		break;
	default:
		ret = SCE_MTP_ERROR_INVALID_ARGUMENT;
		break;
	}
	return ret;
}

}
