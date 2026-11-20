/*
 * The activity service's static resource (/np/resource/activity/activity.xml), as
 * CommandGetStaticResource reads it: the service's state, the host the requests go to and the
 * trophy icon URLs, with the signature (<!--Sce-Np-Signature:...-->) the utility checks it
 * against. No binary names the struct or its fields; the names are ours, from the XML's
 * elements and attributes (activity status, activity-fqdn, trophy-icon).
 */
#ifndef NP_ACTIVITY_COMMAND_STATIC_RESOURCE_H
#define NP_ACTIVITY_COMMAND_STATIC_RESOURCE_H

#include <scetypes.h>

/* StaticResource::status: <activity status="..."> */
#define STATIC_RESOURCE_STATUS_ALIVE		0
#define STATIC_RESOURCE_STATUS_MAINTENANCE	1
#define STATIC_RESOURCE_STATUS_END		2
#define STATIC_RESOURCE_STATUS_BEFORE		3

#define STATIC_RESOURCE_FQDN_SIZE	0x80
#define STATIC_RESOURCE_SIGNATURE_SIZE	0x100
#define STATIC_RESOURCE_DATA_SIZE	0x800
#define STATIC_RESOURCE_URL_SIZE	0x101

typedef struct StaticResource {
	int status;					/* STATIC_RESOURCE_STATUS_* */
	char activityFqdn[STATIC_RESOURCE_FQDN_SIZE];
	char signature[STATIC_RESOURCE_SIGNATURE_SIZE];
	char data[STATIC_RESOURCE_DATA_SIZE];		/* what the signature covers */
	SceSize dataSize;
	char trophyIconBaseUrl[STATIC_RESOURCE_URL_SIZE];
	char trophyIconBronze[STATIC_RESOURCE_URL_SIZE];
	char trophyIconSilver[STATIC_RESOURCE_URL_SIZE];
	char trophyIconGold[STATIC_RESOURCE_URL_SIZE];
	char trophyIconPlatinum[STATIC_RESOURCE_URL_SIZE];
	char trophyIconHidden[STATIC_RESOURCE_URL_SIZE];
} StaticResource;

#endif
