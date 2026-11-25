/*
 * SceNpGameCustomDataDlgImpl (vs0:sys/external/libcdlg_game_custom_data_impl.suprx):
 * WebAPI core client (SceGcdCoreImpl) for Game Custom Data items, messages,
 * thumbnails, and multipart uploads.
 */
#ifndef GCD_IMPL_GCD_CORE_H
#define GCD_IMPL_GCD_CORE_H

#include <scetypes.h>
#include <std/string.h>
#include <std/vector>
#include <thread/thread.h>

struct GcdItemSummary {
	paf::string itemId;
	paf::string fromOnlineId;
	paf::string dataName;
	paf::string dataDescription;
	paf::string messageSubject;
	paf::string receivedDate;
	paf::string expiredDate;
	paf::string npTitleId;
	paf::string npTitleName;
	paf::string npTitleIconUrl;
	paf::string npCommunicationId;
	paf::string contentId;
	paf::string attachmentUrl;
	bool transferredFromPs3;
	bool seenFlag;
	bool dataUsedFlag;
};

struct GcdSendRequest {
	paf::vector<paf::string> recipients;
	paf::string message;
	paf::string dataName;
	paf::string dataDescription;
	paf::string attachmentUrl;
	paf::string token;
	SceInt32 expireMinutes;
	bool forward;
	bool withItemId;
	const SceUInt8 *binaryData;
	SceSize binarySize;
	const SceUInt8 *thumbnailData;
	SceSize thumbnailSize;
	bool thumbnailIsPng;
};

class SceGcdCoreImpl {
public:
	SceGcdCoreImpl();
	~SceGcdCoreImpl();

	SceInt32 Initialize(SceInt32 libHttpCtxId, const char *npLanguage);
	void Terminate();

	SceInt32 FetchItemList(const char *onlineId, SceInt32 limit, SceInt32 offset,
			       paf::vector<GcdItemSummary> &outItems);
	SceInt32 FetchItemMessage(const char *onlineId, const char *itemId,
				  GcdItemSummary &outDetail);
	SceInt32 FetchBinaryPart(const char *onlineId, const char *itemId,
				 const char *partName, paf::vector<SceUInt8> &outBytes);
	SceInt32 SendItem(const GcdSendRequest &request, paf::string &outItemId);
	SceInt32 UpdateItemFlag(const char *onlineId, const char *itemId, const char *flagName);
	SceInt32 DeleteItem(const char *onlineId, const char *itemId);

	static paf::string BuildTmdbUrl(const char *envPrefix, const char *titleId,
					const char *hashPart);
	static const char *NormalizeNpLanguage(SceInt32 systemLang);

private:
	SceInt32 m_webApiCtxId;
	SceInt32 m_userCtxId;
	paf::string m_npLanguage;
	paf::thread::RMutex m_sendSizeLock;
	bool m_initialized;
};

#endif /* GCD_IMPL_GCD_CORE_H */
