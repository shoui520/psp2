/*
 * The connection ("INIT:"): its state, what the shell is told about it, whether an operation
 * may run in it, and the host as the initiatorInfo it sends describes it.
 */
#include <stddef.h>
#include <scetypes.h>
#include <libkernel/libkernel.h>
#include <std/stdlib.h>
#include <string.h>
#include <std/string.h>
#include <regmgr/regmgr.h>
#include <bXCe/bxce_internal.h>
#include "mtp_internal.h"

typedef struct MtpPlatformName {
	const char *name;
	int type;
} MtpPlatformName;

typedef struct MtpPlatformNames {
	MtpPlatformName entry[11];
} MtpPlatformNames;

static MtpStatus s_status = {
	0, 0, 0, 0, { 0 }, 0, 0, { 0 }, -1, 0
};
static MtpHostVersion s_hostVersion = {
	SCE_MTP_ERROR_NOT_INITIALIZED
};
static int s_lastPlatformType = 0;	/* the host of the last connection */

/* the host's platformType */
static MtpPlatformNames s_platformNames = { {
	{ "PS3", MTP_PLATFORM_TYPE_PS3 },
	{ "WinXP32", MTP_PLATFORM_TYPE_WINXP32 },
	{ "WinXP64", MTP_PLATFORM_TYPE_WINXP64 },
	{ "WinVista32", MTP_PLATFORM_TYPE_WINVISTA32 },
	{ "WinVista64", MTP_PLATFORM_TYPE_WINVISTA64 },
	{ "Win7 32", MTP_PLATFORM_TYPE_WIN7_32 },
	{ "Win7 64", MTP_PLATFORM_TYPE_WIN7_64 },
	{ "Mac", MTP_PLATFORM_TYPE_MAC },
	{ "PC", MTP_PLATFORM_TYPE_PC },
	{ "PS", MTP_PLATFORM_TYPE_PS },
	{ "nasne", MTP_PLATFORM_TYPE_NASNE },
} };

int mtpStatusCheckProtocolVersion(void)
{
	MtpNetHost host;
	double osVersion;

	if (s_status.protocolVersion == MTP_PROTOCOL_VERSION)
		return 0;
	if (MTP_PROTOCOL_VERSION - s_status.protocolVersion < 0)
		return (int)0x8054001D;
	switch (s_status.platformClass) {
	case MTP_PLATFORM_CLASS_PS:
		if ((SceUInt32)s_status.protocolVersion < MTP_PROTOCOL_VERSION_PS)
			return (int)0x80540027;
		return 0;
	case MTP_PLATFORM_CLASS_PC:
		if (mtpCoreGetIfType() == MTP_IF_TYPE_NET) {
			mtpUtilMemset(&host, 0, sizeof(host));
			if (mtpCoreGetHostInfo(&host) == 0 && host.type == MTP_NET_HOST_TYPE_NASNE) {
				if ((SceUInt32)s_status.protocolVersion < MTP_PROTOCOL_VERSION_PS)
					return (int)0x80540042;
				return 0;
			}
		}
		osVersion = sce_paf_atof(s_status.osVersion);
		if ((SceUInt32)s_status.protocolVersion < MTP_PROTOCOL_VERSION_PC ||
		    ((mtpUtilStrncmp(s_hostVersion.platformSubtype, "Win", 16) != 0 ||
		      osVersion >= 6.0) &&
		     (mtpUtilStrncmp(s_hostVersion.platformSubtype, "Mac", 16) != 0 ||
		      osVersion > 10.5)))
			return (int)0x80540026;
		return 0;
	default:
		return (int)0x8054001E;
	}
}

int mtpStatusIsSupportedHost(void)
{
	MtpNetHost host;
	double osVersion;
	int supported;

	switch (s_status.platformClass) {
	case MTP_PLATFORM_CLASS_PS:
		return (SceUInt32)s_status.protocolVersion >= MTP_PROTOCOL_VERSION_PS;
	case MTP_PLATFORM_CLASS_PC:
		if (mtpCoreGetIfType() == MTP_IF_TYPE_NET) {
			mtpUtilMemset(&host, 0, sizeof(host));
			if (mtpCoreGetHostInfo(&host) == 0 && host.type == MTP_NET_HOST_TYPE_NASNE)
				return (SceUInt32)s_status.protocolVersion >= MTP_PROTOCOL_VERSION_PS;
		}
		supported = 0;
		if (s_status.protocolVersion == MTP_PROTOCOL_VERSION ||
		    (osVersion = sce_paf_atof(s_status.osVersion),
		     (SceUInt32)s_status.protocolVersion >= MTP_PROTOCOL_VERSION_PC &&
		     ((mtpUtilStrncmp(s_hostVersion.platformSubtype, "Win", 16) == 0 &&
		       osVersion < 6.0) ||
		      (mtpUtilStrncmp(s_hostVersion.platformSubtype, "Mac", 16) == 0 &&
		       osVersion <= 10.5))))
			supported = 1;
		return supported;
	default:
		return s_status.protocolVersion == MTP_PROTOCOL_VERSION;
	}
}

int mtpStatusGetPlatformClass(void)
{
	return s_status.platformClass;
}

/* what the shell's background copy is told the host is */
static inline int mtpStatusHostKind(int platformType)
{
	switch (platformType) {
	case 1:
	case 10:
		return 1;
	case 8:
		return 3;
	case 9:
		return 2;
	case 11:
		return 5;
	default:
		if (platformType < 2 || platformType > 7)
			return -1;
		return 2;
	}
}

void mtpStatusSetState(int state, int reason, int quiet)
{
	int kind;

	if (!(state == 0 && s_status.reason != reason && reason != -1 && s_status.reason != 0 &&
	      s_status.reason != 2 && s_status.reason != 8 && s_status.reason != 9 &&
	      s_status.reason != 3 && s_status.reason != 15) &&
	    s_status.state == state)
		return;
	if (reason != -1)
		s_status.reason = reason;
	if (mtpCoreGetIfType() != MTP_IF_TYPE_NET && (state == 5 || state == 2) &&
	    (s_status.state == 4 || s_status.state == 3)) {
		mtpCallbackNotify(0x200);
		if (mtpMarlinIsWaiting())
			mtpMarlinCancelWaitResult();
	}
	if (s_status.state == 4 && state != 4) {
		if (mtpCoreGetIfType() == MTP_IF_TYPE_USB) {
			mtpCallbackInvokeEvent(2);
			mtpBgCopyCancelAll();
		} else if (reason == 8 || reason == 3 || reason == 0 || reason == 9) {
			mtpCallbackInvokeEvent(2);
			mtpBgCopyCancelAll();
		}
	}
	if (state == 0) {
		if (s_status.platformType != 0)
			s_lastPlatformType = s_status.platformType;
		s_status.osVersion[0] = '\0';
		s_status.name[0] = '\0';
		s_status.state = 0;
		s_status.platformType = 0;
		s_status.protocolVersion = 0;
		s_status.applicationType = 0;
		s_status.flags = 0;
		s_status.platformClass = 0;
		s_status.arg = 0;
		mtpCallbackNotify(4);
		return;
	}
	if (state == 1)
		s_status.reason = -1;
	if (state > 0 && state <= 2 && s_status.platformType != 0)
		s_lastPlatformType = s_status.platformType;
	if (state != 4)
		s_status.arg = 0;
	s_status.state = state;
	if (quiet)
		return;
	if (state == 5) {
		if (s_status.platformClass == MTP_PLATFORM_CLASS_PS &&
		    sceRegMgrSrvGetRegion() == MTP_REGION_CHINA)
			state = 0;
		else
			state = mtpStatusIsSupportedHost() ? 4 : 3;
	}
	switch (state) {
	case 3:
		mtpCallbackNotify(0x20);
		kind = mtpStatusHostKind(s_status.platformType);
		if (kind < 0)
			return;
		consoleInfoWrite1(0, kind, CONSOLE_INFO_WRITE_ONCE);
		break;
	case 4:
		setConnected();
		mtpCallbackNotify(1);
		kind = mtpStatusHostKind(s_status.platformType);
		if (kind < 0)
			return;
		consoleInfoWrite1(0, kind, CONSOLE_INFO_WRITE_ONCE);
		break;
	case 1:
		mtpCallbackNotify(2);
		break;
	case 2:
		mtpCallbackNotify(8);
		break;
	case 7:
		mtpCallbackNotify(4);
		break;
	}
}

void mtpStatusReset(void)
{
	mtpUtilMemset(&s_status, 0, sizeof(s_status));
	mtpStatusSetState(7, -1, 0);
}

void mtpStatusSetFlags(SceUInt32 value, SceUInt32 mask)
{
	s_status.flags = (value & mask) | (s_status.flags & ~mask);
}

void mtpStatusGet(MtpStatus *status)
{
	mtpUtilMemcpy(status, &s_status, sizeof(s_status));
}

void mtpStatusGetHostVersion(MtpHostVersion *version)
{
	mtpUtilMemcpy(version, &s_hostVersion, sizeof(s_hostVersion));
}

int mtpStatusGetState(void)
{
	return s_status.state;
}

int mtpStatusIsApplicationType1(void)
{
	return s_status.applicationType == 1;
}

/* whether an operation may run now: 0, or why not */
int mtpStatusCheck(int checkVersion)
{
	int platformClass;

	mtpCoreSetCancelRequested(0);
	if ((s_status.state == 0 || mtpCoreGetPendingOperation() == 0) &&
	    mtpCoreWake(1) == SCE_MTP_ERROR_CANCELED)
		return SCE_MTP_ERROR_CANCELED;
	switch (s_status.state) {
	case 3:
		if (checkVersion)
			return mtpStatusCheckProtocolVersion();
		return 0;
	case 4:
		return 0;
	case 5:
		return (int)0x80540021;
	case 6:
		return (int)0x80540022;
	}
	platformClass = mtpPlatformClass(s_lastPlatformType);
	if (s_status.state == 2) {
		if (platformClass == MTP_PLATFORM_CLASS_PS)
			return (int)0x8054002B;
		if (platformClass == MTP_PLATFORM_CLASS_PC) {
			if (s_lastPlatformType == 11)
				return (int)0x80540043;
			return (int)0x8054002A;
		}
	}
	if (platformClass == MTP_PLATFORM_CLASS_PS)
		return (int)0x80540020;
	if (platformClass == MTP_PLATFORM_CLASS_PC) {
		if (s_lastPlatformType == 11)
			return (int)0x80540044;
		return (int)0x8054001F;
	}
	if (s_status.state != 0)
		return SCE_MTP_ERROR_NO_HOST;
	return (int)0x80540034;
}

int mtpStatusSetArg(int arg)
{
	s_status.arg = arg;
	return mtpQueueKick();
}

/* the initiatorInfo element's handler */
class InitiatorInfoHandler : public bXCe::Sax::DocumentHandler {
public:
	virtual ~InitiatorInfoHandler();
	virtual int startDocument(void *userData);
	virtual int startElement(void *userData, const char *name, int length,
				 const bXCe::AttributeList *attrList);
	virtual int characters(void *userData, const char *ch, int length);
	virtual int endElement(void *userData, const char *name, int length);
	virtual int endDocument(void *userData);
	virtual int skippedText(void *userData, int tokenType, const char *text, int length);
	virtual int entityData(void *userData, int entityType, const char *name, int length);
	virtual void fatalError(void *userData, int errCode);
};

InitiatorInfoHandler::~InitiatorInfoHandler()
{
}

int InitiatorInfoHandler::startDocument(void *userData)
{
	return 0;
}

static inline void setHostApplicationType(const char *value)
{
	int type;

	if (value == NULL || *value == '\0')
		return;
	type = sce_paf_atoi(value);
	if (type < 0 || type > 7) {
		MTP_ERR("INIT:", 169, "unknown application type: %s\n", value);
		type = 0;
	}
	s_status.applicationType = type;
}

int InitiatorInfoHandler::startElement(void *userData, const char *name, int length,
	const bXCe::AttributeList *attrList)
{
	const bXCe::Attr *attr;
	const char *attrName;
	const char *value;
	MtpPlatformNames names;
	MtpNetHost host;
	int num;
	int i;
	int j;
	int nameLength;

	if (sce_paf_strncmp("initiatorInfo", name, length + 1) == 0) {
		num = attrList->getLength();
		for (i = 0; i < num; i++) {
			attr = (const bXCe::Attr *)attrList->getAttribute(i);
			if (attr == NULL)
				continue;
			attrName = attr->getName();
			nameLength = attr->getNameLength() + 1;
			if (sce_paf_strncmp("platformType", attrName, nameLength) == 0) {
				value = attr->getValue();
				memcpy(&names, &s_platformNames, sizeof(names));
				j = 0;
				while (mtpUtilStrcmp(value, names.entry[j].name) != 0) {
					if (++j >= 11)
						goto next;
				}
				s_status.platformType = names.entry[j].type;
				if (s_status.platformType == MTP_PLATFORM_TYPE_PC &&
				    mtpCoreGetIfType() == MTP_IF_TYPE_NET) {
					mtpUtilMemset(&host, 0, sizeof(host));
					if (mtpCoreGetHostInfo(&host) == 0 && host.type == MTP_NET_HOST_TYPE_NASNE)
						s_status.platformType = MTP_PLATFORM_TYPE_NASNE;
				}
				s_status.platformClass = mtpPlatformClass(s_status.platformType);
			} else if (sce_paf_strncmp("platformSubtype", attrName, nameLength) == 0) {
				mtpUtilStrncpy(s_hostVersion.platformSubtype, attr->getValue(),
					sizeof(s_hostVersion.platformSubtype));
			} else if (sce_paf_strncmp("osVersion", attrName, nameLength) == 0) {
				mtpUtilStrncpy(s_status.osVersion, attr->getValue(),
					sizeof(s_status.osVersion));
			} else if (sce_paf_strncmp("version", attrName, nameLength) == 0) {
				mtpUtilStrncpy(s_hostVersion.version, attr->getValue(),
					sizeof(s_hostVersion.version));
			} else if (sce_paf_strncmp("protocolVersion", attrName, nameLength) == 0) {
				value = attr->getValue();
				if (value != NULL && *value != '\0') {
					s_status.protocolVersion = sce_paf_atoi(value);
					s_hostVersion.check = mtpStatusCheckProtocolVersion();
				}
			} else if (sce_paf_strncmp("name", attrName, nameLength) == 0) {
				mtpUtilMemset(s_status.name, 0, sizeof(s_status.name));
				mtpUtilStrncpy(s_status.name, attr->getValue(),
					sizeof(s_status.name) - 1);
			} else if (sce_paf_strncmp("applicationType", attrName, nameLength) == 0) {
				setHostApplicationType(attr->getValue());
			}
next:
			;
		}
	}
	if (s_status.platformType == MTP_PLATFORM_TYPE_PC &&
	    mtpUtilStrncmp(s_hostVersion.platformSubtype, "Mac", 16) == 0)
		s_status.platformType = MTP_PLATFORM_TYPE_MAC;
	return 0;
}

int InitiatorInfoHandler::characters(void *userData, const char *ch, int length)
{
	return 0;
}

int InitiatorInfoHandler::endElement(void *userData, const char *name, int length)
{
	sce_paf_strncmp("initiatorInfo", name, length + 1);
	return 0;
}

int InitiatorInfoHandler::endDocument(void *userData)
{
	return 0;
}

int InitiatorInfoHandler::skippedText(void *userData, int tokenType, const char *text,
	int length)
{
	return 0;
}

int InitiatorInfoHandler::entityData(void *userData, int entityType, const char *name,
	int length)
{
	return 0;
}

void InitiatorInfoHandler::fatalError(void *userData, int errCode)
{
	MTP_ERR("INIT:", 364, "Error: 0x%x\n", errCode);
}

InitiatorInfoParser::~InitiatorInfoParser()
{
	if (m_handler != NULL)
		delete m_handler;
	if (m_parser != NULL) {
		m_parser->terminate();
		delete m_parser;
	}
}

int InitiatorInfoParser::Initialize()
{
	bXCe::Sax::Parser *parser;
	bXCe::Sax::DocumentHandler *handler;
	int ret;

	parser = new bXCe::Sax::Parser;
	if (parser == NULL) {
		MTP_ERR("INIT:", 383, "no memory\n");
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	ret = parser->initialize(NULL);
	if (ret < 0) {
		MTP_ERR("INIT:", 389, "Parser Initialize failed: 0x%x\n", ret);
		delete parser;
		return ret;
	}
	parser->setResolveEntity(true);
	parser->setResolveAttributeEntity(true);
	parser->setSkipIgnorableWhiteSpace(true);
	handler = new InitiatorInfoHandler;
	if (handler == NULL) {
		MTP_ERR("INIT:", 400, "no memory\n");
		delete parser;
		return SCE_MTP_ERROR_NO_MEMORY;
	}
	parser->setDocumentHandler(handler);
	m_parser = parser;
	m_handler = handler;
	return 0;
}

int InitiatorInfoParser::Terminate()
{
	int ret = 0;

	if (m_handler != NULL) {
		delete m_handler;
		m_handler = NULL;
	}
	if (m_parser != NULL) {
		ret = m_parser->terminate();
		delete m_parser;
		m_parser = NULL;
	}
	return ret;
}

int InitiatorInfoParser::Parse(const char *data, int length, bool isFinal)
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
