/*
 * tel_sms_sender.cpp: loads an outgoing SMS from SQLite, encodes a Submit PDU
 * for each pending/failed recipient peer, transmits via BBMC, and updates
 * per-peer delivery status (3.65-CEX: 0x81254878..0x81254ABF).
 */
#include "../tel_internal.h"

namespace telephony {

static const int kSmsPeerStatusSent = 2;
static const int kSmsPeerStatusPending = 3;
static const int kSmsPeerStatusFailed = 5;

/* 0x81254878 */
TelSmsSender::TelSmsSender(TelBbmcAdapterInterface *bbmcAdapter,
			   TelSmsDbManager *dbManager,
			   TelSmsPduGenerator *pduGenerator)
	: TelObservable("SceTelSmsSenderIf"),
	  m_bbmcAdapter(bbmcAdapter),
	  m_dbManager(dbManager),
	  m_pduGenerator(pduGenerator)
{
}

/* 0x812548E0 */
TelephonyResult TelSmsSender::SendMessage(const TelSmsMessageId &messageId)
{
	TelSmsMessage message;
	TelSmsProtocolDataUnit submitPdu;

	TelephonyResult res = m_dbManager->GetMessageById(messageId, &message);
	if (!res.IsSuccess())
		return res;

	paf::vector<TelSmsPeerInfo> peers = message.GetPeers();
	for (SceSize idx = 0; idx < peers.size(); ++idx) {
		int peerStatus = peers[idx].GetStatus();
		if (peerStatus != kSmsPeerStatusFailed && peerStatus != kSmsPeerStatusPending)
			continue;

		message.SetSinglePeer(peers[idx]);
		res = m_pduGenerator->Generate(message, &submitPdu);

		int updatedStatus = kSmsPeerStatusFailed;
		if (res.IsSuccess()) {
			res = m_bbmcAdapter->SendSms(submitPdu);
			if (res.IsSuccess())
				updatedStatus = kSmsPeerStatusSent;
		}

		res = m_dbManager->UpdateMessageStatusForPeer(
			message.GetMessageId(),
			message.GetFirstPeer().GetAddress().GetNumber(),
			&updatedStatus);
	}

	return res;
}

} /* namespace telephony */
