#ifndef BBMC_QMI_GPS_REPORT_H
#define BBMC_QMI_GPS_REPORT_H

#include <stddef.h>
#include <scetypes.h>

/* PDS event-report wire values are byte aligned and little endian. */
typedef struct QmiPdsPositionWire {
	SceUInt8 validFields[4];
	SceUInt8 year[2];
	SceUInt8 month, dayOfWeek, dayOfMonth, hour, minute, second;
	SceUInt8 millisecond[2];
	SceUInt8 leapSeconds;
	SceUInt8 utcTimestamp[8];
	SceUInt8 timeUncertainty[4];
	SceUInt8 latitude[8], longitude[8];
	SceUInt8 altitudeEllipsoid[4], altitudeSeaLevel[4];
	SceUInt8 horizontalSpeed[4], verticalSpeed[4], heading[4];
	SceUInt8 horizontalUncertaintyCircular[4];
	SceUInt8 horizontalUncertaintyEllipseMajor[4];
	SceUInt8 horizontalUncertaintyEllipseMinor[4];
	SceUInt8 horizontalUncertaintyEllipseAzimuth[4];
	SceUInt8 verticalUncertainty[4];
	SceUInt8 horizontalVelocityUncertainty[4], verticalVelocityUncertainty[4];
	SceUInt8 horizontalConfidence;
	SceUInt8 positionDop[4], horizontalDop[4], verticalDop[4];
	SceUInt8 operationMode;
} QmiPdsPositionWire;

typedef struct QmiPdsSatelliteWire {
	SceUInt8 validFields[4];
	SceUInt8 system, prn, health, processingStatus;
	SceUInt8 ephemerisState, almanacState;
	SceUInt8 elevation[4];
	SceUInt8 azimuth[2], carrierToNoise[2];
} QmiPdsSatelliteWire;

typedef struct QmiPdsVxPromptWire {
	SceUInt8 validFields[4];
	SceUInt8 privacy, qualityOfService;
	SceUInt8 positionCount[4], intervalSeconds[4];
	SceUInt8 mode, requestorCodingScheme, requestorLength;
	SceUInt8 requestor[];
} QmiPdsVxPromptWire;

typedef struct QmiPdsSuplPromptWire {
	SceUInt8 validFields[4];
	SceUInt8 privacy;
	SceUInt8 initHash[8];
	SceUInt8 mode;
	SceUInt8 sessionId[4];
	SceUInt8 ipv4Port[4], ipv6Port[4], ipv4Address[4], ipv6Address[16];
	SceUInt8 reservedServerUrl;
	SceUInt8 serverUrlLength;
	char serverUrl[];
} QmiPdsSuplPromptWire;

typedef struct QmiPdsUmtsCpPromptWire {
	SceUInt8 validFields[4];
	SceUInt8 privacy, invokeId, notificationCodingScheme, notificationLength;
	SceUInt8 notification[];
} QmiPdsUmtsCpPromptWire;

typedef struct QmiPdsCountedStringWire {
	SceUInt8 codingScheme, length;
	SceUInt8 text[];
} QmiPdsCountedStringWire;

_Static_assert(sizeof(QmiPdsPositionWire) == 105, "PDS wire position");
_Static_assert(sizeof(QmiPdsSatelliteWire) == 18, "PDS wire satellite");
_Static_assert(sizeof(QmiPdsVxPromptWire) == 17, "PDS wire VX prefix");
_Static_assert(sizeof(QmiPdsSuplPromptWire) == 48, "PDS wire SUPL prefix");
_Static_assert(sizeof(QmiPdsUmtsCpPromptWire) == 8, "PDS wire UMTS CP prefix");

typedef struct QmiPdsPosition {
	SceUInt32 validFields;
	SceUInt16 year;
	SceUInt8 month, dayOfWeek, dayOfMonth, hour, minute, second;
	SceUInt16 millisecond;
	SceUInt8 leapSeconds;
	SceUInt8 utcTimestamp[8];
	SceUInt32 timeUncertainty;
	float latitude, longitude;
	float altitudeEllipsoid, altitudeSeaLevel;
	float horizontalSpeed, verticalSpeed, heading;
	float horizontalUncertaintyCircular;
	float horizontalUncertaintyEllipseMajor;
	float horizontalUncertaintyEllipseMinor;
	float horizontalUncertaintyEllipseAzimuth;
	float verticalUncertainty;
	float horizontalVelocityUncertainty, verticalVelocityUncertainty;
	SceUInt8 horizontalConfidence;
	float positionDop, horizontalDop, verticalDop;
	SceUInt32 operationMode;
} QmiPdsPosition;

typedef struct QmiPdsNmeaSentence {
	SceUInt8 length;
	char text[128];
} QmiPdsNmeaSentence;

typedef struct QmiPdsNmeaReport {
	SceUInt16 operationMode;
	SceUInt8 count;
	QmiPdsNmeaSentence sentences[16];
} QmiPdsNmeaReport;

typedef struct QmiPdsWifiRequest {
	SceUInt8 requestType;
	SceUInt16 intervalMilliseconds;
} QmiPdsWifiRequest;

typedef struct QmiPdsSatellite {
	SceUInt32 validFields;
	SceUInt8 system, prn, health, processingStatus;
	SceUInt8 ephemerisState, almanacState;
	float elevation;
	SceUInt16 azimuth, carrierToNoise;
} QmiPdsSatellite;

typedef struct QmiPdsSatelliteReport {
	SceUInt32 validFields;
	SceUInt8 ionosphericCorrections, count;
	QmiPdsSatellite satellites[32];
} QmiPdsSatelliteReport;

typedef struct QmiPdsVxPrompt {
	SceUInt32 validFields;
	SceUInt8 privacy, qualityOfService, mode, requestorCodingScheme;
	SceUInt32 positionCount, intervalSeconds;
	SceUInt8 requestorLength;
	SceUInt8 requestor[255];
} QmiPdsVxPrompt;

typedef struct QmiPdsSuplPrompt {
	SceUInt32 validFields;
	SceUInt8 privacy;
	SceUInt8 initHash[8];
	SceUInt8 mode;
	SceUInt32 ipv4Port;
	SceUInt32 ipv6Port;
	SceUInt8 ipv4Address[4];
	SceUInt8 ipv6Address[16];
	SceUInt32 sessionId;
	SceUInt8 serverUrlLength;
	char serverUrl[255];
	SceUInt8 requestCodingScheme, requestorCodingScheme, requestorLength;
	SceUInt8 requestor[32];
	SceUInt8 clientCodingScheme, clientLength;
	SceUInt8 client[32];
} QmiPdsSuplPrompt;

typedef struct QmiPdsUmtsCpPrompt {
	SceUInt32 validFields;
	SceUInt8 privacy, invokeId;
	SceUInt8 reservedNotificationCodingScheme;
	SceUInt8 notificationLength;
	SceUInt8 notification[128];
	SceUInt8 clientAddressLength;
	SceUInt8 clientAddress[128];
	SceUInt8 locationType, requestorCodingScheme, requestorLength;
	SceUInt8 requestor[128];
	/* The decoder also writes notification coding here before codeword coding. */
	SceUInt8 codewordCodingScheme, codewordLength;
	SceUInt8 codeword[128];
	SceUInt8 serviceType;
} QmiPdsUmtsCpPrompt;

/* Whole allocation cleared by the configured-service callback and freed after decode.
 * Reserved intervals have no semantic access in the decoder or its readers.
 */
typedef struct QmiPdsEventReport {
	QmiPdsPosition position;
	SceUInt8 sessionStatus;
	SceUInt8 reservedSession[3];
	QmiPdsNmeaReport nmea;
	SceUInt8 reservedNmeaGap[2];
	QmiPdsNmeaReport extendedNmea;
	SceUInt8 reservedExtendedNmeaGap[2];
	QmiPdsWifiRequest wifiRequest;
	QmiPdsSatelliteReport satelliteReport;
	QmiPdsVxPrompt vxPrompt;
	SceUInt8 reservedVxGap[4];
	QmiPdsSuplPrompt suplPrompt;
	SceUInt8 reservedSuplGap[29];
	SceUInt8 suplHorizontalAccuracy, suplVerticalAccuracy;
	SceUInt8 reservedPromptGap;
	QmiPdsUmtsCpPrompt umtsCpPrompt;
	SceUInt8 communicationEvent, communicationState;
} QmiPdsEventReport;

_Static_assert(sizeof(QmiPdsPosition) == 104, "PDS decoded position");
_Static_assert(sizeof(QmiPdsNmeaReport) == 2068, "PDS decoded NMEA");
_Static_assert(sizeof(QmiPdsSatellite) == 20, "PDS decoded satellite");
_Static_assert(sizeof(QmiPdsSatelliteReport) == 648, "PDS decoded satellite report");
_Static_assert(sizeof(QmiPdsVxPrompt) == 272, "PDS VX prompt");
_Static_assert(sizeof(QmiPdsSuplPrompt) == 376, "PDS SUPL prompt");
_Static_assert(sizeof(QmiPdsUmtsCpPrompt) == 528, "PDS UMTS CP prompt");
_Static_assert(sizeof(QmiPdsEventReport) == 6116, "PDS event report allocation");
_Static_assert(offsetof(QmiPdsEventReport, nmea) == 108, "PDS NMEA placement");
_Static_assert(offsetof(QmiPdsEventReport, extendedNmea) == 2178, "PDS extended NMEA placement");
_Static_assert(offsetof(QmiPdsEventReport, satelliteReport) == 4252, "PDS satellite placement");
_Static_assert(offsetof(QmiPdsEventReport, vxPrompt) == 4900, "PDS VX placement");
_Static_assert(offsetof(QmiPdsEventReport, suplPrompt) == 5176, "PDS SUPL placement");
_Static_assert(offsetof(QmiPdsEventReport, suplHorizontalAccuracy) == 5581, "PDS SUPL accuracy placement");
_Static_assert(offsetof(QmiPdsEventReport, umtsCpPrompt) == 5584, "PDS UMTS placement");
_Static_assert(offsetof(QmiPdsSuplPrompt, ipv4Port) == 16, "PDS SUPL IPv4 port");
_Static_assert(offsetof(QmiPdsSuplPrompt, ipv6Port) == 20, "PDS SUPL IPv6 port");
_Static_assert(offsetof(QmiPdsSuplPrompt, ipv4Address) == 24, "PDS SUPL IPv4 address");
_Static_assert(offsetof(QmiPdsSuplPrompt, ipv6Address) == 28, "PDS SUPL IPv6 address");
_Static_assert(offsetof(QmiPdsSuplPrompt, sessionId) == 44, "PDS SUPL session");

#endif
