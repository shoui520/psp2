/*
 * tel_apn_info_list.cpp: TelApnInfoList, TelApnInfoListFactory, TelApnInfoListUtil,
 * and TSS APN list downloader (3.65-CEX: 0x813DC710..0x813DCC1F, 0x813E6264..0x813E75BF).
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <buffer/buffer.h>
#include <common/shared_ptr.h>
#include <file/localfile.h>
#include <http/http.h>
#include <kernel/threadmgr.h>
#include <libhmac.h>
#include <plugin.h>
#include "../tel_common.h"

extern "C" {
extern int vshSblUtMgrHasComTestFlag(void);
extern int sceRegMgrGetKeyStr(const char *category, const char *name, char *buf,
				     SceSize size);
extern char *sceClibStrncpy(char *dst, const char *src, SceSize count);
extern "C" int sceAesScheduleKey(void *aesCtx, SceSize blockBits, SceSize keyBits, const void *key);
extern "C" int sceAesDecrypt(const void *aesCtx, const void *inBlock, void *outBlock);
}

namespace telephony {

namespace {

static const SceUInt8 kMaskedApnAesKey[16] = {
	0xA8, 0xF1, 0x91, 0x28, 0x90, 0xA0, 0xF8, 0xD4,
	0xB6, 0xF4, 0xDE, 0x38, 0xCF, 0xFC, 0x16, 0x9E
};

static const SceUInt8 kMaskedApnHmacKey[16] = {
	0x13, 0x65, 0xCC, 0x72, 0x72, 0x07, 0x2B, 0x7A,
	0x03, 0x7E, 0xD9, 0x44, 0x47, 0x0A, 0x1F, 0x72
};

struct ApnAesContext {
	SceUInt32 headerWords[2];
	SceUInt32 blockWordCount;
	SceUInt8 keySchedule[996];
};

static SceUID g_apnDownloaderThreadId = -1;

/* 0x813E62D2 */
static int decryptApnListAesCbc(const SceUInt8 *iv, SceUInt8 *payload, SceSize length)
{
	SceUInt8 key[16];
	for (int idx = 0; idx < 16; ++idx)
		key[idx] = kMaskedApnAesKey[idx] ^ 0xA2U;

	ApnAesContext aesCtx;
	sce_paf_memset(&aesCtx, 0, sizeof(aesCtx));
	int ret = sceAesScheduleKey(&aesCtx, 128, 128, key);
	if (ret < 0)
		return ret;

	SceUInt32 blockBytes = 4U * aesCtx.blockWordCount;
	SceUInt8 chainBlock[16];
	if (blockBytes > sizeof(chainBlock))
		blockBytes = sizeof(chainBlock);
	sce_paf_memcpy(chainBlock, iv, blockBytes);

	SceUInt8 decryptedBlock[16];
	for (SceSize offset = 0; offset < length; offset += 16) {
		sceAesDecrypt(&aesCtx, &payload[offset], decryptedBlock);
		for (SceUInt32 byteIdx = 0; byteIdx < blockBytes; ++byteIdx) {
			SceUInt8 nextChain = payload[offset + byteIdx];
			payload[offset + byteIdx] = chainBlock[byteIdx] ^ decryptedBlock[byteIdx];
			chainBlock[byteIdx] = nextChain;
		}
	}
	return 0;
}

/* 0x813E63D0 */
static int downloadAndInstallApnLists(void)
{
	paf::common::SharedPtr<paf::Buffer> responseBuf;
	paf::http::Client::Param clientParam;
	paf::http::Client client(clientParam);
	paf::http::Transaction tx(client);

	paf::string lastModified;
	paf::string prevModified;
	TelApnIniFileAccessor updateIni;
	TelephonyResult iniRes = updateIni.Open(
		paf::string("ur0:shell/telephony/tel_apn_list_update.ini"), true);
	if (iniRes.GetCode() != TEL_OK)
		return iniRes.GetCode();

	updateIni.GetString(paf::string("LastModifiedDate"), &prevModified);

	char npEnv[17];
	sce_paf_memset(npEnv, 0, sizeof(npEnv));
	sce_paf_strlcpy(npEnv, "np", sizeof(npEnv));
	if (vshSblUtMgrHasComTestFlag() != 0)
		sceClibStrncpy(npEnv, "prod-qa", 16);
	else
		sceRegMgrGetKeyStr("/CONFIG/NP", "env", npEnv, sizeof(npEnv));

	paf::string url = telFormatString(
		"https://a0.ww.%s.dl.playstation.net/tss/%s/NPWR02951_00/NPWR02951_00-%d.tss",
		npEnv, npEnv, 0);

	int err = ScePafMisc_A86A5D0E(&tx, url.c_str(), "GET", NULL);
	if (err < 0)
		return err;

	if (prevModified.length() != 0)
		tx.SetRequestHeader("If-Modified-Since", prevModified.c_str());

	err = tx.SendRequest(NULL, 0);
	if (err < 0)
		return err;

	paf::Plugin *telPlugin = paf::Plugin::Find("telephony_plugin");
	if (telPlugin == NULL)
		return -2136014814;

	responseBuf = paf::HeapAllocBuffer::Allocate(196608, telPlugin->m_private_heap);
	if (responseBuf.get() == NULL)
		return -2136014814;

	SceUInt32 receivedSize = 0;
	err = tx.ReceiveResponse(responseBuf, &receivedSize);
	if (err < 0)
		return err;

	int statusCode = 0;
	tx.GetStatusCode(statusCode);
	if (statusCode != 200)
		return err;

	err = tx.GetResponseHeader("Last-Modified", lastModified);
	if (err < 0)
		return err;

	if (receivedSize < 0x4000U)
		return -2135994367;

	SceUInt8 *rawBytes = (SceUInt8 *)responseBuf->GetAddr();
	if (rawBytes[0] != 1)
		return -2135994367;

	SceUInt8 hmacKey[16];
	for (int idx = 0; idx < 16; ++idx)
		hmacKey[idx] = kMaskedApnHmacKey[idx] ^ 0x39U;

	SceUInt8 computedDigest[20];
	sce_paf_memset(computedDigest, 0, sizeof(computedDigest));
	SceSize signedLen = receivedSize - 20U;
	if (sceHmacSha1Digest(hmacKey, sizeof(hmacKey), rawBytes, signedLen, computedDigest) != 0 ||
	    sce_paf_memcmp(computedDigest, rawBytes + signedLen, sizeof(computedDigest)) != 0)
		return -2135994367;

	SceSize encryptedLen = receivedSize - 52U;
	err = decryptApnListAesCbc(rawBytes + 16, rawBytes + 32, encryptedLen);
	if (err < 0)
		return err;

	SceUInt16 formatVersion = (SceUInt16)(rawBytes[32] | ((SceUInt16)rawBytes[33] << 8));
	SceUInt16 fileCount = (SceUInt16)(rawBytes[34] | ((SceUInt16)rawBytes[35] << 8));
	if (formatVersion != 1 || fileCount != 20)
		return -2135994367;

	const SceUInt8 *cursor = rawBytes + 36;
	const SceUInt8 *payloadEnd = rawBytes + signedLen;
	for (unsigned int fileIdx = 0; fileIdx < 20U; ++fileIdx) {
		if (cursor + 2 >= payloadEnd)
			return -2135994367;

		SceUInt16 chunkLen = (SceUInt16)(cursor[0] | ((SceUInt16)cursor[1] << 8));
		cursor += 2;
		if (cursor + chunkLen >= payloadEnd)
			return -2135994367;

		char halfLetter = ((fileIdx & 1U) != 0) ? 'b' : 'a';
		paf::string outPath = telFormatString("ur0:shell/telephony/tel_apn_list%d%c.ini",
						      (int)(fileIdx >> 1), (int)halfLetter);
		int openErr = 0;
		paf::common::SharedPtr<paf::File> outFile =
			paf::LocalFile::Open(outPath.c_str(), 0x603U, 0x106U, &openErr);
		if (outFile.get() == NULL)
			return openErr;

		int written = outFile->Write(cursor, chunkLen);
		if (written < 0)
			return written;

		outFile->Close();
		cursor += chunkLen;
	}

	if (lastModified.length() != 0)
		updateIni.SetString(paf::string("LastModifiedDate"), lastModified);

	return 0;
}

/* 0x813E759E */
static int apnDownloaderThreadEntry(SceSize args, void *argp)
{
	(void)args;
	(void)argp;
	int result = downloadAndInstallApnLists();
	g_apnDownloaderThreadId = -1;
	sceKernelExitDeleteThread(result);
	return result;
}

/* 0x813E6264 */
static int startApnDownloaderThread(void)
{
	int res = g_apnDownloaderThreadId;
	if (res < 0) {
		res = sceKernelCreateThread("sceTelApnInfoListDownloader",
					    apnDownloaderThreadEntry,
					    0x1000010A, 0x4000U, 0, 0, NULL);
		g_apnDownloaderThreadId = res;
		if (res >= 0) {
			res = sceKernelStartThread(res, 0, NULL);
			if (res < 0) {
				sceKernelDeleteThread(g_apnDownloaderThreadId);
				g_apnDownloaderThreadId = -1;
				return -1;
			}
		}
	}
	return res;
}

} /* namespace */

/* 0x813DCB8A */
bool TelApnInfoList::ContainsApnName(const paf::string &apnName) const
{
	for (SceSize idx = 0; idx < m_entries.size(); ++idx) {
		if (m_entries[idx].GetName() == apnName)
			return true;
	}
	return false;
}

/* 0x813DC710 */
TelApnInfoListFactory::~TelApnInfoListFactory()
{
}

/* 0x813DC734 */
TelephonyResult TelApnInfoListFactory::Create(const TelephonyPlmn &plmn, TelApnInfoList *outList)
{
	TelApnInfoListUtil util;
	return util.LoadForPlmn(plmn, outList);
}

/* 0x813DC790 */
TelApnInfoListUtil::~TelApnInfoListUtil()
{
}

/* 0x813DC7B4 */
TelephonyResult TelApnInfoListUtil::LoadForPlmn(const TelephonyPlmn &plmn, TelApnInfoList *outList)
{
	paf::string plmnCode = plmn.GetPlmnString();
	TelApnIniFileAccessor accessor;
	TelephonyResult res = accessor.OpenForMcc((SceUInt16)plmn.GetMcc());
	if (res.GetCode() != TEL_OK)
		return res;

	int loadedCount = 0;
	for (int seq = 1; ; ++seq) {
		paf::string apnName;
		paf::string contractId;
		paf::string userId;
		paf::string password;
		paf::string billingPlanStr;
		int autoFlag = 0;

		paf::string key = telFormatString("%s_%d_NAME", plmnCode.c_str(), seq);
		res = accessor.GetString(key, &apnName);
		if (res.GetCode() != TEL_OK)
			break;

		key = telFormatString("%s_%d_CONTRACT_STRING_ID", plmnCode.c_str(), seq);
		accessor.GetString(key, &contractId);

		key = telFormatString("%s_%d_ID", plmnCode.c_str(), seq);
		accessor.GetString(key, &userId);

		key = telFormatString("%s_%d_PW", plmnCode.c_str(), seq);
		accessor.GetString(key, &password);

		key = telFormatString("%s_%d_BILLING_PLAN", plmnCode.c_str(), seq);
		accessor.GetString(key, &billingPlanStr);

		key = telFormatString("%s_%d_AUTO", plmnCode.c_str(), seq);
		accessor.GetInt(key, &autoFlag);

		TelOperatorApnInfo apnInfo;
		apnInfo.SetBillingPlan(sce_paf_atoi(billingPlanStr.c_str()));
		apnInfo.SetContractStringId(contractId);
		if (autoFlag == 1)
			apnInfo.SetAutoSelectable();

		TelephonyOperator credentials;
		credentials.SetCredentials(apnName, userId, password);
		apnInfo.SetApnCredentials(credentials);

		outList->GetEntries().push_back(apnInfo);
		loadedCount = seq;
	}

	if (loadedCount != 0)
		res = TelephonyResult(TEL_OK);
	return res;
}

/* 0x813DCBEA */
TelephonyResult telStartApnInfoListDownloader(void)
{
	startApnDownloaderThread();
	return TelephonyResult(TEL_OK);
}

} /* namespace telephony */
