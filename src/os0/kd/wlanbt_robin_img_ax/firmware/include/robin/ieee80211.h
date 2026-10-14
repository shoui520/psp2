/* 802.11 frame control bits as the firmware tests them. */
#ifndef ROBIN_IEEE80211_H
#define ROBIN_IEEE80211_H

/* WlanWirelessFrame.frameControl[0]: version (bits 0-1), type (2-3), subtype (4-7). */
#define IEEE80211_FC0_TYPE_MASK		0x0c
#define IEEE80211_FC0_TYPE_MGMT		0x00
#define IEEE80211_FC0_TYPE_CTL		0x04
#define IEEE80211_FC0_TYPE_DATA		0x08
#define IEEE80211_FC0_SUBTYPE_SHIFT	4
#define IEEE80211_FC0_QOS		0x80	/* data subtype with the QoS bit */

/* WlanWirelessFrame.frameControl[1]. */
#define IEEE80211_FC1_TODS		0x01
#define IEEE80211_FC1_FROMDS		0x02
#define IEEE80211_FC1_MORE_FRAG		0x04
#define IEEE80211_FC1_RETRY		0x08
#define IEEE80211_FC1_PWR_MGT		0x10
#define IEEE80211_FC1_MORE_DATA		0x20
#define IEEE80211_FC1_PROTECTED		0x40
#define IEEE80211_FC1_ORDER		0x80

#define IEEE80211_SUBTYPE_NULL		4
#define IEEE80211_SUBTYPE_QOS_NULL	12

static inline int ieee80211IsData(unsigned int fc0)
{
	return (fc0 & IEEE80211_FC0_TYPE_MASK) == IEEE80211_FC0_TYPE_DATA;
}

static inline int ieee80211IsQosData(unsigned int fc0)
{
	return ieee80211IsData(fc0) && (fc0 & IEEE80211_FC0_QOS);
}

/* Null function frames, with or without QoS. */
static inline int ieee80211IsNullData(unsigned int fc0)
{
	unsigned int subtype = fc0 >> IEEE80211_FC0_SUBTYPE_SHIFT;

	return ieee80211IsData(fc0) &&
	       (subtype == IEEE80211_SUBTYPE_NULL || subtype == IEEE80211_SUBTYPE_QOS_NULL);
}

#endif /* ROBIN_IEEE80211_H */
