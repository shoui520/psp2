/*
 * np/np_auth.h: NP authentication (tickets and entitlements), under Sony's names and with
 * Sony's definitions (SDK Release 03.570.011, target/include/np/np_auth.h).
 */
#ifndef _SCE_NP_AUTH_H_
#define _SCE_NP_AUTH_H_

#include <rtc.h>
#include <np/np_common.h>

typedef int SceNpAuthRequestId;

#define SCE_NP_TICKET_MAX_SIZE			(65536)
#define SCE_NP_TICKET_SERIAL_ID_SIZE		(20)
#define SCE_NP_SUBJECT_REGION_SIZE		(4)
#define SCE_NP_SUBJECT_DOMAIN_SIZE		(4)
#define SCE_NP_SERVICE_ID_SIZE			(24)
#define SCE_NP_ENTITLEMENT_ID_SIZE		(32)
#define SCE_NP_COOKIE_MAX_SIZE			(1024)

#define SCE_NP_TICKET_PARAM_DATA_LEN		(256)

typedef SceUInt64 SceNpTime;

typedef struct SceNpDate {
	SceUShort16 year;
	SceUChar8 month;
	SceUChar8 day;
} SceNpDate;

typedef union SceNpTicketParam {
	SceInt32	i32;
	SceInt64	i64;
	SceUInt32	u32;
	SceUInt64	u64;
	SceNpDate	date;
	SceUChar8	data[SCE_NP_TICKET_PARAM_DATA_LEN];
} SceNpTicketParam;

#define SCE_NP_TICKET_PARAM_SERIAL_ID			0	/* Binary */
#define SCE_NP_TICKET_PARAM_ISSUER_ID			1	/* SceUInt32 */
#define SCE_NP_TICKET_PARAM_ISSUED_DATE			2	/* SceNpTime */
#define SCE_NP_TICKET_PARAM_EXPIRE_DATE			3	/* SceNpTime */
#define SCE_NP_TICKET_PARAM_SUBJECT_ACCOUNT_ID		4	/* SceUInt64 */
#define SCE_NP_TICKET_PARAM_SUBJECT_ONLINE_ID		5	/* String */
#define SCE_NP_TICKET_PARAM_SUBJECT_REGION		6	/* Binary */
#define SCE_NP_TICKET_PARAM_SUBJECT_DOMAIN		7	/* String */
#define SCE_NP_TICKET_PARAM_SERVICE_ID			8	/* String */
#define SCE_NP_TICKET_PARAM_SUBJECT_STATUS		9	/* SceUInt32 */
#define SCE_NP_TICKET_PARAM_STATUS_DURATION		10	/* SceNpTime */
#define SCE_NP_TICKET_PARAM_SUBJECT_DOB			11	/* SceNpDate */
#define SCE_NP_TICKET_PARAM_MAX				12

#define SCE_NP_AUTH_LATEST_TICKET_VERSION_MAJOR		3
#define SCE_NP_AUTH_LATEST_TICKET_VERSION_MINOR		0

typedef struct SceNpTicketVersion {
	unsigned short major;
	unsigned short minor;
} SceNpTicketVersion;

typedef int (*SceNpAuthCallback)(
	SceNpAuthRequestId id,
	int result,
	void *arg	/* user argument */
	);

typedef struct SceNpAuthRequestParameter {
	SceSize size;
	SceNpTicketVersion version;
	const char *serviceId;
	const void *cookie;
	SceSize cookieSize;
	const char *entitlementId;
	SceUInt32 consumedCount;
	SceNpAuthCallback ticketCb;
	void *cbArg;
} SceNpAuthRequestParameter;

typedef struct SceNpEntitlementId {
	unsigned char data[SCE_NP_ENTITLEMENT_ID_SIZE];
} SceNpEntitlementId;

#define SCE_NP_ENTITLEMENT_TYPE_NON_CONSUMABLE	0
#define SCE_NP_ENTITLEMENT_TYPE_CONSUMABLE	1

typedef struct SceNpEntitlement {
	SceNpEntitlementId id;
	SceNpTime createdDate;
	SceNpTime expireDate;
	SceUInt32 type;
	SceInt32 remainingCount;	/* may be negative */
	SceUInt32 consumedCount;
	char padding[4];
} SceNpEntitlement;

#ifdef __cplusplus
extern "C" {
#endif

int sceNpAuthInit(void);
int sceNpAuthTerm(void);
int sceNpAuthCreateStartRequest(const SceNpAuthRequestParameter *param);
int sceNpAuthDestroyRequest(SceNpAuthRequestId id);
int sceNpAuthAbortRequest(SceNpAuthRequestId id);
int sceNpAuthGetTicket(SceNpAuthRequestId id, void *buf, SceSize len);
int sceNpAuthGetTicketParam(const unsigned char *ticket, SceSize ticketSize, int paramId,
			    union SceNpTicketParam *param);
int sceNpAuthGetEntitlementIdList(const unsigned char *ticket, SceSize ticketSize,
				  SceNpEntitlementId *entIdList, SceSize entIdListNum);
int sceNpAuthGetEntitlementById(const unsigned char *ticket, SceSize ticketSize,
				const char *entId, SceNpEntitlement *ent);

#define sceNpAuthCheckEntitlementById(ticket, ticketSize, entId) \
	sceNpAuthGetEntitlementById((ticket), (ticketSize), (entId), NULL)

#ifdef __cplusplus
}
#endif

#endif /* _SCE_NP_AUTH_H_ */
