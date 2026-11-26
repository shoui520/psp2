/*
 * teleport_http_protocol.cpp: HTTP/1.1 protocol handlers (GET /teleport.xml, POST /,
 * DELETE /, OPTIONS, disconnect cleanup, and IPMI launch response dispatch) for
 * SceShellTeleport (0x813A1E34..0x813A6123).
 */
#include "teleport_internal.h"
#include "../../console_info/console_info.h"
#include <bXCe/bxce_internal.h>
#include <kernel/iofilemgr.h>
#include <std/ctype.h>
#include <std/stdio.h>
#include <std/string.h>
#include <power/power.h>
#include <string.h>

extern "C" {
int sceAVConfigHdmiCecCmdOneTouchPlay(int mode);
int sceAppMgrGetStatusByName(const char *appName, void *appStatus);
int sceAppMgrDestroyAppByName(const char *appName);
int sceAppMgrGetPidListForShell(SceUID *pidList, unsigned int count);
}

#define TELEPORT_PROTOCOL_VERSION		"02.00"
#define TELEPORT_MAX_XML_REQUEST_BYTES		0x2B10u
#define TELEPORT_MAX_POST_PARAM_BYTES		6144u
#define TELEPORT_MAX_VIDEO_PARAM_BYTES		0x2800u
#define TELEPORT_MAX_APP_PARAM_BYTES		0x3FFu
#define TELEPORT_MAX_DELETE_URI_BYTES		0x200u
#define TELEPORT_LAUNCH_FLAGS			0x30000
#define TELEPORT_NET_ERR_WOULDBLOCK		35
#define TELEPORT_VIDEO_ARG_FILE			"ur0:temp/videostreaming/video.txt"
#define TELEPORT_VIDEO_LAUNCH_URI		"videostreaming:play?arg_file='ur0:temp/videostreaming/video.txt'"
#define TELEPORT_CONSOLE_INFO_VIDEO_METRIC	226
#define TELEPORT_APPMGR_SUSPENDED_APP_ID	(-6)

TeleportHttpProtocol::TeleportHttpProtocol()
	: m_mutex("SceTeleportHttpProtocol", 0),
	  m_responseBuffer(new char[TELEPORT_HTTP_BUFFER_SIZE])
{
}

TeleportHttpProtocol::~TeleportHttpProtocol()
{
	delete[] m_responseBuffer;
	m_responseBuffer = NULL;
}

TeleportHttpProtocol *TeleportHttpProtocol::GetInstance(void)
{
	static TeleportHttpProtocol s_instance;
	return &s_instance;
}

/* 0x813A1E34: handle HTTP GET /teleport.xml device & app capability descriptor */
SceInt32 TeleportHttpProtocol::HandleGet(TeleportHttpServerThread *thread, SceNetId sock,
					 const char *request, SceSize length,
					 TeleportHttpSendFunc sendFn,
					 TeleportHttpRequestInfo *reqInfo)
{
	(void)length;
	TeleportHttpProtocol *self = GetInstance();
	paf::string requestStr(request != NULL ? request : "");
	paf::string expectedUri("/teleport.xml");

	static const char kHeaderXmlFmt[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<teleport><device><name>%s</name><type>0x00000001</type>"
		"<version>%s</version></device>"
		"<protocol><version>%s</version><auth_salt>%s</auth_salt></protocol>";
	static const char kClosingTag[] = "</teleport>";

	if (sce_paf_strncmp(reqInfo->uri.c_str(), expectedUri.c_str(), reqInfo->uri.size()) != 0)
		return -3;

	TeleportAppInfoHelper helper;
	char deviceName[64];
	char swVersion[28];
	char authSalt[36];
	sce_paf_memset(deviceName, 0, sizeof(deviceName));
	sce_paf_memset(swVersion, 0, sizeof(swVersion));
	sce_paf_memset(authSalt, 0, 33u);

	helper.GetDeviceInfo(deviceName, swVersion);

	SceNetSockInfo sockInfo;
	sceNetGetSockInfo(sock, &sockInfo, 1, 0);

	paf::map<SceNetId, paf::string>::iterator saltIt = self->m_socketSalts.find(sockInfo.s);
	if (saltIt == self->m_socketSalts.end()) {
		char macAddress[20];
		TeleportNetMonitor::GetInstance()->GetMacAddress(macAddress);
		helper.GenerateAuthSalt(authSalt, macAddress);
		self->m_socketSalts[sockInfo.s] = paf::string(authSalt);
	} else {
		sce_paf_snprintf(authSalt, 33u, "%s", saltIt->second.c_str());
	}

	SceSize closingLen = sce_paf_strlen(kClosingTag);
	sce_paf_memset(self->m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);
	SceInt32 prefixLen = sce_paf_snprintf(self->m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
					      kHeaderXmlFmt, deviceName, swVersion,
					      TELEPORT_PROTOCOL_VERSION, authSalt);
	SceInt32 appsLen = helper.BuildAppsListXml(
		self->m_responseBuffer,
		TELEPORT_HTTP_BUFFER_SIZE - ((SceSize)prefixLen + closingLen));

	sce_paf_memset(self->m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);
	SceInt32 totalBodyLen = sce_paf_snprintf(self->m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
						 kHeaderXmlFmt, deviceName, swVersion,
						 TELEPORT_PROTOCOL_VERSION, authSalt) +
				appsLen + (SceInt32)closingLen;

	sce_paf_memset(self->m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);
	SceSize offset = (SceSize)sce_paf_snprintf(
		self->m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
		"HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", totalBodyLen);
	offset += (SceSize)sce_paf_snprintf(
		self->m_responseBuffer + offset, TELEPORT_HTTP_BUFFER_SIZE - offset,
		kHeaderXmlFmt, deviceName, swVersion, TELEPORT_PROTOCOL_VERSION, authSalt);
	offset += (SceSize)helper.BuildAppsListXml(
		self->m_responseBuffer + offset, TELEPORT_HTTP_BUFFER_SIZE - offset);
	sce_paf_snprintf(self->m_responseBuffer + offset, TELEPORT_HTTP_BUFFER_SIZE - offset,
			 "%s", kClosingTag);

	SceSize responseLen = sce_paf_strlen(self->m_responseBuffer);
	sendFn(thread, sock, self->m_responseBuffer, responseLen + 1, 0);
	return 0;
}

/* 0x813A2334: format and send HTTP response for launch success (201/200) or error (401/403) */
SceInt32 TeleportHttpProtocol::SendLaunchResponse(TeleportHttpServerThread *thread,
						  TeleportHttpSendFunc sendFn, SceNetId sock,
						  const paf::string &appFieldsXml,
						  const paf::string &errorXml,
						  SceInt32 statusCode, SceInt32 timeoutUs,
						  SceUInt16 port, const char *extText)
{
	sce_paf_memset(m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);

	char extXml[160];
	memset(extXml, 0, sizeof(extXml));
	if (extText != NULL)
		sce_paf_snprintf(extXml, sizeof(extXml), "<ext>%s</ext>", extText);
	else
		sce_paf_snprintf(extXml, sizeof(extXml), "<ext></ext>");

	static const char kSuccessBodyFmt[] =
		"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<teleport><app>%s<port>%d</port>%s</app></teleport>";

	if (statusCode == 0) {
		SceInt32 bodyLen = sce_paf_snprintf(m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
						    kSuccessBodyFmt, appFieldsXml.c_str(),
						    (int)port, extXml);
		sce_paf_memset(m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);
		SceInt32 hdrLen = sce_paf_snprintf(
			m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
			"HTTP/1.1 201 CREATED\r\nContent-Length: %d\r\n\r\n", bodyLen);
		sce_paf_snprintf(m_responseBuffer + hdrLen, TELEPORT_HTTP_BUFFER_SIZE - (SceSize)hdrLen,
				 kSuccessBodyFmt, appFieldsXml.c_str(), (int)port, extXml);
	} else if (statusCode == 1) {
		SceInt32 bodyLen = sce_paf_snprintf(m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
						    kSuccessBodyFmt, appFieldsXml.c_str(),
						    (int)port, extXml);
		sce_paf_memset(m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);
		SceInt32 hdrLen = sce_paf_snprintf(
			m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
			"HTTP/1.1 200 OK\r\nContent-Length: %d\r\n\r\n", bodyLen);
		sce_paf_snprintf(m_responseBuffer + hdrLen, TELEPORT_HTTP_BUFFER_SIZE - (SceSize)hdrLen,
				 kSuccessBodyFmt, appFieldsXml.c_str(), (int)port, extXml);
	} else if (statusCode == -2) {
		sce_paf_snprintf(
			m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
			"HTTP/1.1 401 UNAUTHORIZED\r\n"
			"Content-Length: 0\r\n"
			"WWW-Authenticate: basic realm=\"Teleport authenticate\"\r\n"
			"\r\n");
	} else if (statusCode == -3) {
		sce_paf_snprintf(m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
				 "HTTP/1.1 403 FORBIDDEN\r\nContent-Length: 0\r\n\r\n");
	} else {
		SceInt32 bodyLen = sce_paf_snprintf(
			m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
			"<?xml version=\"1.0\" encoding=\"UTF-8\"?>%s", errorXml.c_str());
		sce_paf_memset(m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);

		paf::string reasonPhrase;
		SceSize codeTagPos = errorXml.find("<code>");
		if (codeTagPos == paf::string::npos)
			reasonPhrase = "FORBIDDEN";
		else
			reasonPhrase = errorXml.substr(codeTagPos + sce_paf_strlen("<code>"), 10);

		SceInt32 hdrLen = sce_paf_snprintf(
			m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
			"HTTP/1.1 403 %s\r\nContent-Length: %d\r\n\r\n",
			reasonPhrase.c_str(), bodyLen);
		sce_paf_snprintf(m_responseBuffer + hdrLen, TELEPORT_HTTP_BUFFER_SIZE - (SceSize)hdrLen,
				 "<?xml version=\"1.0\" encoding=\"UTF-8\"?>%s", errorXml.c_str());
	}

	SceInt32 sendTimeout = (timeoutUs > 0) ? timeoutUs : TELEPORT_DEFAULT_SEND_TIMEOUT_US;
	SceSize responseLen = sce_paf_strlen(m_responseBuffer);
	SceInt32 sendRet = sendFn(thread, sock, m_responseBuffer, responseLen + 1, sendTimeout);
	if (sendRet == TELEPORT_NET_ERR_WOULDBLOCK)
		return TELEPORT_ERR_SEND_TIMEOUT;
	if (sendRet >= 0)
		return 0;
	return TELEPORT_ERR_SEND_FAILED;
}

/* 0x813A2670: build <teleport><error><code>0x...</code><hint></hint></error>...</teleport> */
void TeleportHttpProtocol::BuildErrorXml(paf::string *outXml, const paf::string &appXml,
					 SceInt32 errorCode)
{
	char errCodeBuf[16];
	memset(errCodeBuf, 0, sizeof(errCodeBuf));
	sce_paf_snprintf(errCodeBuf, sizeof(errCodeBuf), "0x%08X", (unsigned int)errorCode);

	*outXml = "<teleport>";
	*outXml += "<error>";
	outXml->append((paf::string("<code>") + errCodeBuf + "</code>").c_str());
	*outXml += "<hint></hint>";
	*outXml += "</error>";
	outXml->append(appXml.c_str(), appXml.size());
	*outXml += "</teleport>";
}

/* 0x813A2B46: append empty <app><name></name><version></version><uri></uri></app> element */
void TeleportHttpProtocol::BuildEmptyAppXml(paf::string *outXml)
{
	*outXml += "<app>";
	*outXml += "<name></name>";
	*outXml += "<version></version>";
	*outXml += "<uri></uri>";
	*outXml += "</app>";
}

/* 0x813A2D30: store active pending launch state awaiting IPMI 0x001A0000 notification */
SceInt32 TeleportHttpProtocol::SetPendingLaunch(const TeleportPendingLaunch &pending)
{
	TeleportHttpProtocol *self = GetInstance();
	self->m_pendingLaunch = pending;
	return 0;
}

/* 0x813A2E4E: launch NPXS10085 video streaming app via temporary argument file */
SceInt32 TeleportHttpProtocol::LaunchVideoStreaming(const char *launchParam)
{
	SceInt32 ret = 0;
	AppmgrLaunchAppByNameOption launchOpt;
	sce_paf_memset(&launchOpt, 0, sizeof(launchOpt));

	if (launchParam != NULL) {
		launchOpt.launchMode = TELEPORT_LAUNCH_FLAGS;
		launchOpt.paramCallbackId = -1;
		launchOpt.creatorAppId = -1;
		teleportEnsureVideoTempDir();

		SceSize pathLen = sce_paf_strlen(TELEPORT_VIDEO_ARG_FILE);
		char *filePath = new char[pathLen + 1];
		sce_paf_snprintf(filePath, pathLen + 1, "%s", TELEPORT_VIDEO_ARG_FILE);

		SceUID fd = sceIoOpen(filePath,
				      SCE_O_WRONLY | SCE_O_CREAT | SCE_O_TRUNC, 06);
		if (fd != 0) {
			sceIoWrite(fd, launchParam, sce_paf_strlen(launchParam) + 1);
			sceIoClose(fd);
		}

		ret = sceAppMgrLaunchAppByUri2(TELEPORT_VIDEO_LAUNCH_URI, &launchOpt);
		consoleInfoFileAddInt(TELEPORT_CONSOLE_INFO_VIDEO_METRIC, 1, 0);
		delete[] filePath;
	}
	return ret;
}

/* 0x813A2F16: check whether a launch request is currently awaiting IPMI completion */
bool TeleportHttpProtocol::HasPendingLaunch(void)
{
	TeleportHttpProtocol *self = GetInstance();
	return self->m_pendingLaunch.clientSocket != 0;
}

/* 0x813A3022: verify string contains only uppercase hex digits (optionally skipping "0X" prefix) */
bool TeleportHttpProtocol::IsUpperHexString(const paf::string &text, bool hasHexPrefix)
{
	SceSize startPos = hasHexPrefix ? 2u : 0u;
	if (text.empty() || (hasHexPrefix && text.size() < 2u))
		return false;

	for (SceSize idx = startPos; idx < text.size(); ++idx) {
		char ch = text[idx];
		if (!((ch >= '0' && ch <= '9') || (ch >= 'A' && ch <= 'F')))
			return false;
	}
	return true;
}

/* 0x813A30B2: replace every occurrence of fromStr with toStr in target */
void TeleportHttpProtocol::ReplaceAllSubstrings(paf::string *target, const paf::string &fromStr,
						const paf::string &toStr)
{
	SceSize pos = 0;
	while ((pos = target->find(fromStr, pos)) != paf::string::npos) {
		target->replace(pos, fromStr.size(), toStr);
		pos += toStr.size();
	}
}

/* 0x813A314E: unescape the five standard XML character entities in place */
void TeleportHttpProtocol::UnescapeXmlEntities(paf::string *text)
{
	ReplaceAllSubstrings(text, paf::string("&lt;"), paf::string("<"));
	ReplaceAllSubstrings(text, paf::string("&gt;"), paf::string(">"));
	ReplaceAllSubstrings(text, paf::string("&amp;"), paf::string("&"));
	ReplaceAllSubstrings(text, paf::string("&quot;"), paf::string("\""));
	ReplaceAllSubstrings(text, paf::string("&apos;"), paf::string("'"));
}

/* 0x813A33E0: parse <teleport><app><uri>...</uri><option>...</option></app></teleport> via bXCe */
SceInt32 TeleportHttpProtocol::ParseLaunchRequestXml(const paf::string &httpMessage,
						     paf::string *outUri,
						     paf::string *outOption)
{
	SceSize xmlPos = httpMessage.find("<?xml ");
	paf::string xmlPayload = httpMessage.substr(xmlPos, httpMessage.size());

	if (httpMessage.size() > TELEPORT_MAX_XML_REQUEST_BYTES)
		return TELEPORT_ERR_XML_PARSE;

	bXCe::Dom::DocumentBuilder *builder = new bXCe::Dom::DocumentBuilder();
	if (builder == NULL)
		return TELEPORT_ERR_XML_PARSE;

	SceInt32 ret = builder->initialize();
	if (ret == 0) {
		builder->setSkipIgnorableText(true);
		builder->setSkipIgnorableWhiteSpace(true);
		ret = builder->parse(xmlPayload.c_str(), (int)xmlPayload.size(), true);
		if (ret == 0) {
			bXCe::Dom::Document doc(builder->getDocument());
			BxceNodeId rootNode = doc.getRoot();
			bXCe::Dom::NodeList *teleportList =
				doc.getElementsByTagName(rootNode, "teleport");

			if (teleportList != NULL && teleportList->getLength() != 0) {
				BxceNodeId teleportNode = teleportList->item(0);
				BxceNodeId appFirstChild = doc.getFirstChild(teleportNode);
				if (appFirstChild != 0 &&
				    sce_paf_strcmp("app", doc.getNodeName(appFirstChild)) == 0) {
					bXCe::Dom::NodeList *appList =
						doc.getElementsByTagName(appFirstChild, "app");
					if (appList != NULL) {
						int appCount = appList->getLength();
						for (int idx = 0; idx < appCount; ++idx) {
							paf::string nodeText;
							BxceNodeId appNode = appList->item((SceUInt32)idx);
							bXCe::Dom::NodeList *uriList =
								doc.getElementsByTagName(appNode, "uri");
							if (uriList->getLength() != 0) {
								BxceNodeId textNode = doc.getFirstChild(uriList->item(0));
								const char *text = doc.getText(textNode);
								nodeText = (text != NULL) ? text : "";
							}
							if (nodeText.empty()) {
								ret = TELEPORT_ERR_XML_PARSE;
								break;
							}
							*outUri = nodeText;
							nodeText.clear();

							bXCe::Dom::NodeList *optList =
								doc.getElementsByTagName(appNode, "option");
							if (optList->getLength() != 0) {
								BxceNodeId textNode = doc.getFirstChild(optList->item(0));
								const char *text = doc.getText(textNode);
								nodeText = (text != NULL) ? text : "";
							}
							*outOption = nodeText;
						}
					} else {
						ret = TELEPORT_ERR_XML_PARSE;
					}
				} else {
					ret = TELEPORT_ERR_XML_PARSE;
				}
			} else {
				ret = TELEPORT_ERR_XML_PARSE;
			}
		}
	}

	builder->terminate();
	delete builder;
	return ret;
}

/* 0x813A3774: handle HTTP POST / remote application launch request */
SceInt32 TeleportHttpProtocol::HandlePost(TeleportHttpServerThread *thread, SceNetId sock,
					  const char *request, SceSize length,
					  TeleportHttpSendFunc sendFn,
					  TeleportHttpRequestInfo *reqInfo)
{
	(void)length;
	TeleportHttpProtocol *self = GetInstance();
	paf::string rawRequest(request != NULL ? request : "");
	paf::string trimmedRequest;
	paf::string authDigest;
	paf::string sessionSalt;
	paf::string rootUri("/");

	self->m_mutex.Lock();

	if (reqInfo->contentLength == -1) {
		self->m_mutex.Unlock();
		return -2;
	}

	if (reqInfo->totalLength == (SceSize)-1 || rawRequest.size() <= reqInfo->totalLength)
		trimmedRequest = rawRequest;
	else
		trimmedRequest = rawRequest.substr(0, reqInfo->totalLength);

	if (sce_paf_strncmp(reqInfo->uri.c_str(), rootUri.c_str(), reqInfo->uri.size()) != 0) {
		self->m_mutex.Unlock();
		return -1;
	}

	if (!reqInfo->authToken.empty()) {
		SceNetSockInfo sockInfo;
		sceNetGetSockInfo(sock, &sockInfo, 1, 0);
		paf::map<SceNetId, paf::string>::iterator saltIt =
			self->m_socketSalts.find(sockInfo.s);
		if (saltIt != self->m_socketSalts.end())
			sessionSalt = saltIt->second;

		authDigest = reqInfo->authToken;
		if (!self->IsUpperHexString(authDigest, false)) {
			paf::string emptyXml;
			self->SendLaunchResponse(thread, sendFn, sock, emptyXml, emptyXml,
						 -2, 0, 0, NULL);
		}
	}

	paf::string launchUri;
	paf::string optionStr;
	if (ParseLaunchRequestXml(trimmedRequest, &launchUri, &optionStr) != 0) {
		self->m_mutex.Unlock();
		return -1;
	}

	UnescapeXmlEntities(&launchUri);

	SceInt32 launchOption = 0;
	if (!optionStr.empty()) {
		paf::string upperOpt = optionStr;
		for (SceSize idx = 0; idx < upperOpt.size(); ++idx)
			upperOpt[idx] = (char)sce_paf_toupper(upperOpt[idx]);

		if (!self->IsUpperHexString(upperOpt, true)) {
			self->m_mutex.Unlock();
			return -1;
		}

		char hexBuf[11];
		memset(hexBuf, 0, sizeof(hexBuf));
		for (SceSize idx = 0; idx < 11u && idx < upperOpt.size(); ++idx)
			hexBuf[idx] = (char)sce_paf_toupper(upperOpt[idx]);

		for (SceSize idx = 0; idx < 11u; ++idx) {
			char ch = hexBuf[idx];
			if (ch >= '0' && ch <= '9')
				launchOption = (launchOption * 16) + (ch - '0');
			else if (ch >= 'A' && ch <= 'F')
				launchOption = (launchOption * 16) + (ch - 'A' + 10);
			else if (ch != 'x' && ch != 'X')
				break;
		}
	}

	if (!launchUri.empty()) {
		static const char kParamPrefix[] =
			"type=LAUNCH_APP_BY_URI&factor=TELEPORT&uri=";
		SceSize maxUriLen = TELEPORT_MAX_POST_PARAM_BYTES - sce_paf_strlen(kParamPrefix);
		if (launchUri.size() > maxUriLen) {
			self->m_mutex.Unlock();
			return -1;
		}

		TeleportAppInfoHelper helper;
		paf::string errorXml;
		paf::string conflictAppXml;
		SceInt32 validRet = helper.ValidateLaunchRequest(launchUri, authDigest, sessionSalt,
								 launchOption, &errorXml,
								 &conflictAppXml);

		bool busyWithPending = (validRet == 1 && HasPendingLaunch());
		if (busyWithPending || (validRet != 0 && validRet != 1)) {
			SceInt32 errStatus = busyWithPending ? -1 : validRet;
			paf::string emptyAppXml;
			if (errorXml.empty()) {
				self->BuildEmptyAppXml(&emptyAppXml);
				if (errStatus == -4)
					self->BuildErrorXml(&errorXml, emptyAppXml,
							    TELEPORT_ERR_NOT_ALLOWED);
				else if (errStatus == -5)
					self->BuildErrorXml(&errorXml, emptyAppXml,
							    TELEPORT_ERR_UNSUPPORTED_LIST);
				else
					self->BuildErrorXml(&errorXml, emptyAppXml,
							    TELEPORT_ERR_LAUNCH_FAILED);
			}
			self->SendLaunchResponse(thread, sendFn, sock, emptyAppXml, errorXml,
						 errStatus, 0, 0, NULL);
		} else {
			paf::string appFieldsXml;
			paf::string titleId;
			paf::string appVersion;
			helper.ResolveAppXmlAndTitle(launchUri, &appFieldsXml, &titleId,
						     &appVersion);

			paf::string launchParam = paf::string(kParamPrefix) + launchUri;
			SceInt32 launchRet = TELEPORT_ERR_LAUNCH_FAILED;
			if (sce_paf_strcmp(titleId.c_str(), "NPXS10085") == 0) {
				if (launchParam.size() <= TELEPORT_MAX_VIDEO_PARAM_BYTES)
					launchRet = self->LaunchVideoStreaming(launchParam.c_str());
			} else {
				if (launchParam.size() <= TELEPORT_MAX_APP_PARAM_BYTES)
					launchRet = sceAppMgrLaunchAppByName(TELEPORT_LAUNCH_FLAGS,
									     titleId.c_str(),
									     launchParam.c_str());
			}

			if (launchRet != 0) {
				if (appFieldsXml.empty())
					self->BuildEmptyAppXml(&appFieldsXml);
				self->BuildErrorXml(&errorXml, appFieldsXml,
						    TELEPORT_ERR_LAUNCH_FAILED);
				self->SendLaunchResponse(thread, sendFn, sock, appFieldsXml,
							 errorXml, -1, 0, 0, NULL);
			} else {
				scePowerRequestDisplayOn();
				sceAVConfigHdmiCecCmdOneTouchPlay(2);

				TeleportPendingLaunch pending;
				pending.clientSocket = sock;
				pending.launchMode = validRet;
				pending.appXml = appFieldsXml;
				SetPendingLaunch(pending);
			}
		}
	}

	self->m_mutex.Unlock();
	return 0;
}

/* 0x813A4754: handle HTTP DELETE / to destroy an app previously launched by the same peer IP */
SceInt32 TeleportHttpProtocol::HandleDelete(TeleportHttpServerThread *thread, SceNetId sock,
					    const char *request, SceSize length,
					    TeleportHttpSendFunc sendFn,
					    TeleportHttpRequestInfo *reqInfo)
{
	(void)length;
	TeleportHttpProtocol *self = GetInstance();
	paf::string rootUri("/");
	paf::string rawRequest(request != NULL ? request : "");
	paf::string trimmedRequest;

	if (reqInfo->contentLength == -1 || reqInfo->totalLength == (SceSize)-1 ||
	    rawRequest.size() <= reqInfo->totalLength)
		trimmedRequest = rawRequest;
	else
		trimmedRequest = rawRequest.substr(0, reqInfo->totalLength);

	if (sce_paf_strncmp(reqInfo->uri.c_str(), rootUri.c_str(), reqInfo->uri.size()) != 0)
		return -1;

	paf::string targetUri;
	paf::string unusedOption;
	if (ParseLaunchRequestXml(trimmedRequest, &targetUri, &unusedOption) != 0)
		return -1;

	UnescapeXmlEntities(&targetUri);

	if (targetUri.empty() || targetUri.size() > TELEPORT_MAX_DELETE_URI_BYTES) {
		char forbiddenBuf[128];
		memset(forbiddenBuf, 0, sizeof(forbiddenBuf));
		sce_paf_snprintf(forbiddenBuf, sizeof(forbiddenBuf),
				 "HTTP/1.1 403 FORBIDDEN\r\nContent-Length: 0\r\n\r\n");
		sendFn(thread, sock, forbiddenBuf, sce_paf_strlen(forbiddenBuf) + 1, 0);
		return -1;
	}

	TeleportAppInfoHelper helper;
	if (!helper.ValidateUtf8NoBom(targetUri))
		return -1;

	paf::string titleId;
	SceInt32 destroyRet = -1;
	if (helper.ResolveTitleIdByUri(targetUri, &titleId) == 0 && !titleId.empty()) {
		SceNetSockInfo sockInfo;
		sceNetGetSockInfo(sock, &sockInfo, 1, 0);

		SceAppMgrAppStatus appStatus;
		sceAppMgrGetStatusByName(titleId.c_str(), &appStatus);

		paf::map<SceUInt32, SceUID>::iterator pidIt =
			self->m_remoteLaunchedPids.find(sockInfo.remote_adr.s_addr);
		if (pidIt != self->m_remoteLaunchedPids.end() &&
		    pidIt->second == appStatus.processId) {
			sceAppMgrDestroyAppByName(titleId.c_str());
			self->m_remoteLaunchedPids.erase(pidIt);
			destroyRet = 0;
		}
	}

	char replyBuf[128];
	memset(replyBuf, 0, sizeof(replyBuf));
	if (destroyRet != 0)
		sce_paf_snprintf(replyBuf, sizeof(replyBuf),
				 "HTTP/1.1 403 FORBIDDEN\r\nContent-Length: 0\r\n\r\n");
	else
		sce_paf_snprintf(replyBuf, sizeof(replyBuf), "HTTP/1.1 200 OK\r\n\r\n");

	sendFn(thread, sock, replyBuf, sce_paf_strlen(replyBuf) + 1, 0);
	return 0;
}

/* 0x813A4E56: handle HTTP OPTIONS * and OPTIONS /teleport.xml */
SceInt32 TeleportHttpProtocol::HandleOptions(TeleportHttpServerThread *thread, SceNetId sock,
					     const char *request, SceSize length,
					     TeleportHttpSendFunc sendFn, void *userData)
{
	(void)userData;
	TeleportHttpProtocol *self = GetInstance();

	char requestLineBuf[128];
	memset(requestLineBuf, 0, sizeof(requestLineBuf));
	SceSize copyLimit = (length >= sizeof(requestLineBuf)) ? 127u : length;
	SceSize outPos = 0;
	for (SceSize idx = 0; idx < copyLimit; ++idx) {
		char ch = request[idx];
		if (ch == '\n')
			break;
		if (ch != '\r')
			requestLineBuf[outPos++] = ch;
	}

	paf::string requestLine(requestLineBuf);
	SceSize firstSpace = requestLine.find(" ");
	if (firstSpace == paf::string::npos)
		return -1;
	SceSize secondSpace = requestLine.find(" ", firstSpace + 1);
	if (secondSpace == paf::string::npos || firstSpace > secondSpace ||
	    requestLine.size() < secondSpace)
		return -1;

	paf::string reqUri = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
	paf::string httpVer = requestLine.substr(secondSpace + 1, requestLine.size() - secondSpace);
	if (sce_paf_strcmp(httpVer.c_str(), "HTTP/1.1") != 0)
		return -1;

	if (sce_paf_strcmp(reqUri.c_str(), "*") == 0) {
		sce_paf_memset(self->m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);
		sce_paf_snprintf(self->m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
				 "HTTP/1.1 200 OK\r\nAllow: GET, OPTIONS, POST, DELETE\r\n\r\n");
		sendFn(thread, sock, self->m_responseBuffer,
		       sce_paf_strlen(self->m_responseBuffer) + 1, 0);
		return 0;
	}

	SceSize teleportSuffixLen = sce_paf_strlen("/teleport.xml");
	SceSize suffixPos = reqUri.rfind("/teleport.xml", paf::string::npos, teleportSuffixLen);
	if (suffixPos == paf::string::npos || suffixPos != reqUri.size() - teleportSuffixLen)
		return -1;

	sce_paf_memset(self->m_responseBuffer, 0, TELEPORT_HTTP_BUFFER_SIZE);
	sce_paf_snprintf(self->m_responseBuffer, TELEPORT_HTTP_BUFFER_SIZE,
			 "HTTP/1.1 200 OK\r\nAllow: GET\r\n\r\n");
	sendFn(thread, sock, self->m_responseBuffer,
	       sce_paf_strlen(self->m_responseBuffer) + 1, 0);
	return 0;
}

/* 0x813A54F6: prune peer-IP -> PID entries for processes that have exited */
void TeleportHttpProtocol::PruneExitedApps(void)
{
	SceUID activePids[20];
	SceInt32 pidCount = sceAppMgrGetPidListForShell(activePids, 20);

	paf::vector<SceUInt32> staleIps;
	for (paf::map<SceUInt32, SceUID>::iterator it = m_remoteLaunchedPids.begin();
	     it != m_remoteLaunchedPids.end(); ++it) {
		SceInt32 idx = 0;
		for (; idx < pidCount; ++idx) {
			if (it->second == activePids[idx])
				break;
		}
		if (idx == pidCount)
			staleIps.push_back(it->first);
	}

	for (SceSize idx = 0; idx < staleIps.size(); ++idx) {
		paf::map<SceUInt32, SceUID>::iterator it =
			m_remoteLaunchedPids.find(staleIps[idx]);
		if (it != m_remoteLaunchedPids.end())
			m_remoteLaunchedPids.erase(it);
	}
}

/* 0x813A5666: erase cached authentication salt associated with a closing TCP client socket */
void TeleportHttpProtocol::EraseSocketSalt(SceNetId sock)
{
	SceNetSockInfo sockInfo;
	sceNetGetSockInfo(sock, &sockInfo, 1, 0);

	paf::map<SceNetId, paf::string>::iterator it = m_socketSalts.find(sockInfo.s);
	if (it != m_socketSalts.end())
		m_socketSalts.erase(it);
}

/* 0x813A56DC: socket disconnect callback */
SceInt32 TeleportHttpProtocol::HandleDisconnect(TeleportHttpServerThread *thread, SceNetId sock,
						void *userData)
{
	(void)thread;
	(void)userData;
	TeleportHttpProtocol *self = GetInstance();
	self->EraseSocketSalt(sock);
	self->PruneExitedApps();
	return 0;
}

/* 0x813A57F0: atomically pop and reset the pending launch descriptor */
SceInt32 TeleportHttpProtocol::TakePendingLaunch(TeleportPendingLaunch *outPending)
{
	TeleportHttpProtocol *self = GetInstance();
	if (self->m_pendingLaunch.clientSocket == 0)
		return -1;

	*outPending = self->m_pendingLaunch;
	self->m_pendingLaunch.clientSocket = 0;
	self->m_pendingLaunch.launchMode = 0;
	self->m_pendingLaunch.appXml = "";
	return 0;
}

/* 0x813A5942: send delayed HTTP launch response once launched app reports via IPMI 0x001A0000 */
SceInt32 TeleportHttpProtocol::HandleIpmiResponse(TeleportHttpServerThread *thread,
						  const TeleportIpmiLaunchResponse *response,
						  SceInt32 timeoutUs,
						  TeleportHttpSendFunc sendFn, void *userData)
{
	(void)userData;
	TeleportHttpProtocol *self = GetInstance();
	self->m_mutex.Lock();

	paf::string errorXml;
	TeleportPendingLaunch pending;
	TakePendingLaunch(&pending);

	SceInt32 result;
	if (response->rejected == 1) {
		char errCodeBuf[16];
		memset(errCodeBuf, 0, sizeof(errCodeBuf));
		sce_paf_snprintf(errCodeBuf, sizeof(errCodeBuf), "0x%08X",
				 (unsigned int)TELEPORT_ERR_APP_LAUNCH_REJECTED);

		errorXml = "<teleport>";
		errorXml += "<error>";
		errorXml.append((paf::string("<code>") + errCodeBuf + "</code>").c_str());
		errorXml += "<hint></hint>";
		errorXml += "</error>";
		errorXml += "<app>";
		errorXml.append(pending.appXml.c_str(), pending.appXml.size());
		errorXml += "</app>";
		errorXml += "</teleport>";

		result = self->SendLaunchResponse(thread, sendFn, pending.clientSocket,
						  pending.appXml, errorXml, -1, timeoutUs, 0,
						  NULL);
	} else {
		result = self->SendLaunchResponse(thread, sendFn, pending.clientSocket,
						  pending.appXml, errorXml, pending.launchMode,
						  timeoutUs, (SceUInt16)response->port,
						  response->extText);
		if (pending.launchMode == 0) {
			SceNetSockInfo sockInfo;
			sceNetGetSockInfo(pending.clientSocket, &sockInfo, 1, 0);

			SceAppMgrAppStatus appStatus;
			sceAppMgrGetStatusByAppId(TELEPORT_APPMGR_SUSPENDED_APP_ID, &appStatus);
			self->m_remoteLaunchedPids[sockInfo.remote_adr.s_addr] =
				appStatus.processId;
		}
	}

	self->m_mutex.Unlock();
	return result;
}
