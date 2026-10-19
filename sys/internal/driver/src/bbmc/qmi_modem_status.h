#ifndef BBMC_QMI_MODEM_STATUS_H
#define BBMC_QMI_MODEM_STATUS_H

#include "qmi_backend.h"

typedef struct BbmcGeranNeighbor {
	SceUInt32 cellId;
	SceUInt16 lac;
	SceUInt16 arfcn;
	SceUInt8 plmn[3];
	SceUInt8 bsic;
	SceUInt8 rxLevel;
	SceUInt8 reserved[3];
} BbmcGeranNeighbor;

typedef struct BbmcGeranCellInfo {
	SceUInt32 cellId;
	SceUInt16 lac;
	SceUInt16 arfcn;
	SceUInt8 plmn[3];
	SceUInt8 bsic;
	SceUInt32 timingAdvance;
	SceUInt16 rxLevel;
	SceUInt8 neighborCount;
	SceUInt8 reserved;
	BbmcGeranNeighbor neighbors[16];
} BbmcGeranCellInfo;

typedef struct BbmcUmtsNeighbor {
	SceUInt16 uarfcn;
	SceUInt16 primaryScramblingCode;
	SceUInt16 rscp;
	SceUInt16 ecio;
} BbmcUmtsNeighbor;

typedef struct BbmcUmtsGeranNeighbor {
	SceUInt16 arfcn;
	SceUInt8 networkColorCode;
	SceUInt8 baseStationColorCode;
	SceUInt16 rssi;
} BbmcUmtsGeranNeighbor;

typedef struct BbmcUmtsCellInfo {
	SceUInt16 cellId;
	SceUInt16 lac;
	SceUInt16 uarfcn;
	SceUInt16 primaryScramblingCode;
	SceUInt16 rscp;
	SceUInt16 ecio;
	SceUInt8 plmn[3];
	SceUInt8 neighborCount;
	BbmcUmtsNeighbor neighbors[16];
	SceUInt8 geranNeighborCount;
	SceUInt8 reserved[3];
	BbmcUmtsGeranNeighbor geranNeighbors[16];
} BbmcUmtsCellInfo;

typedef struct BbmcCdmaCellInfo {
	SceUInt16 systemId;
	SceUInt16 networkId;
	SceUInt16 reserved;
	SceUInt16 referencePn;
	SceUInt32 latitude;
	SceUInt32 longitude;
} BbmcCdmaCellInfo;

typedef struct BbmcCellLocationInfo {
	BbmcGeranCellInfo geran;
	BbmcUmtsCellInfo umts;
	BbmcCdmaCellInfo cdma;
} BbmcCellLocationInfo;

typedef struct QmiGeranNeighborValue {
	SceUInt8 cellId[4];
	SceUInt8 plmn[3];
	SceUInt8 lac[2];
	SceUInt8 arfcn[2];
	SceUInt8 bsic;
	SceUInt8 rxLevel;
} QmiGeranNeighborValue;

typedef struct QmiGeranCellInfoValue {
	SceUInt8 cellId[4];
	SceUInt8 plmn[3];
	SceUInt8 lac[2];
	SceUInt8 arfcn[2];
	SceUInt8 bsic;
	SceUInt8 timingAdvance[4];
	SceUInt8 rxLevel[2];
	SceUInt8 neighborCount;
	QmiGeranNeighborValue neighbors[];
} QmiGeranCellInfoValue;

typedef struct QmiUmtsNeighborValue {
	SceUInt8 uarfcn[2];
	SceUInt8 primaryScramblingCode[2];
	SceUInt8 rscp[2];
	SceUInt8 ecio[2];
} QmiUmtsNeighborValue;

typedef struct QmiUmtsCellInfoValue {
	SceUInt8 cellId[2];
	SceUInt8 plmn[3];
	SceUInt8 lac[2];
	SceUInt8 uarfcn[2];
	SceUInt8 primaryScramblingCode[2];
	SceUInt8 rscp[2];
	SceUInt8 ecio[2];
	SceUInt8 neighborCount;
	QmiUmtsNeighborValue neighbors[];
} QmiUmtsCellInfoValue;

typedef struct QmiUmtsGeranNeighborValue {
	SceUInt8 arfcn[2];
	SceUInt8 networkColorCode;
	SceUInt8 baseStationColorCode;
	SceUInt8 rssi[2];
} QmiUmtsGeranNeighborValue;

typedef struct QmiUmtsGeranNeighborsValue {
	SceUInt8 count;
	QmiUmtsGeranNeighborValue neighbors[];
} QmiUmtsGeranNeighborsValue;

typedef struct QmiCdmaCellInfoValue {
	SceUInt8 systemId[2];
	SceUInt8 networkId[2];
	SceUInt8 baseStationId[2];
	SceUInt8 referencePn[2];
	SceUInt8 latitude[4];
	SceUInt8 longitude[4];
} QmiCdmaCellInfoValue;

typedef struct BbmcScannedNetwork {
	SceUInt16 mcc;
	SceUInt16 mnc;
	SceUInt8 descriptionLength;
	SceUInt8 reserved[3];
	char description[68];
	SceUInt32 forbiddenStatus;
	SceUInt32 preferredStatus;
	SceUInt32 availabilityStatus;
	SceUInt32 currentStatus;
	SceUInt32 radioInterface;
} BbmcScannedNetwork;

typedef struct BbmcNetworkScanInfo {
	SceUInt16 count;
	SceUInt16 reserved;
	BbmcScannedNetwork networks[10];
} BbmcNetworkScanInfo;

typedef struct BbmcNetworkScanContext {
	SceUInt32 attempts;
} BbmcNetworkScanContext;

typedef struct QmiScannedNetworkValue {
	SceUInt8 mcc[2];
	SceUInt8 mnc[2];
	SceUInt8 status;
	SceUInt8 descriptionLength;
	char description[];
} QmiScannedNetworkValue;

typedef struct QmiNetworkRadioInterfaceValue {
	SceUInt8 mcc[2];
	SceUInt8 mnc[2];
	SceUInt8 radioInterface;
} QmiNetworkRadioInterfaceValue;

typedef struct QmiNetworkListValue {
	SceUInt8 count[2];
	SceUInt8 networks[];
} QmiNetworkListValue;

_Static_assert(sizeof(BbmcGeranNeighbor) == 16, "GERAN neighbor size");
_Static_assert(sizeof(BbmcGeranCellInfo) == 276, "GERAN information size");
_Static_assert(sizeof(BbmcUmtsNeighbor) == 8, "UMTS neighbor size");
_Static_assert(sizeof(BbmcUmtsGeranNeighbor) == 6, "UMTS GERAN neighbor size");
_Static_assert(sizeof(BbmcUmtsCellInfo) == 244, "UMTS information size");
_Static_assert(sizeof(BbmcCdmaCellInfo) == 16, "CDMA information size");
_Static_assert(sizeof(BbmcCellLocationInfo) == 536, "cell location information size");
_Static_assert(offsetof(BbmcCellLocationInfo, umts) == 276, "UMTS information");
_Static_assert(offsetof(BbmcCellLocationInfo, cdma) == 520, "CDMA information");
_Static_assert(offsetof(BbmcUmtsCellInfo, neighbors) == 16, "UMTS neighbors");
_Static_assert(offsetof(BbmcUmtsCellInfo, geranNeighborCount) == 144,
	"neighboring GERAN count");
_Static_assert(offsetof(BbmcUmtsCellInfo, geranNeighbors) == 148,
	"neighboring GERAN cells");
_Static_assert(sizeof(QmiGeranNeighborValue) == 13, "GERAN wire neighbor size");
_Static_assert(sizeof(QmiGeranCellInfoValue) == 19, "GERAN wire prefix size");
_Static_assert(sizeof(QmiUmtsNeighborValue) == 8, "UMTS wire neighbor size");
_Static_assert(sizeof(QmiUmtsCellInfoValue) == 16, "UMTS wire prefix size");
_Static_assert(sizeof(QmiUmtsGeranNeighborValue) == 6, "GERAN wire neighbor size");
_Static_assert(sizeof(QmiCdmaCellInfoValue) == 16, "CDMA wire information size");
_Static_assert(sizeof(BbmcScannedNetwork) == 96, "scanned network size");
_Static_assert(offsetof(BbmcScannedNetwork, forbiddenStatus) == 76,
	"scanned network status");
_Static_assert(offsetof(BbmcScannedNetwork, radioInterface) == 92,
	"scanned radio interface");
_Static_assert(sizeof(BbmcNetworkScanInfo) == 964, "network scan information size");
_Static_assert(sizeof(BbmcNetworkScanContext) == 4, "network scan context size");
_Static_assert(sizeof(QmiScannedNetworkValue) == 6, "scanned network wire prefix");
_Static_assert(sizeof(QmiNetworkRadioInterfaceValue) == 5, "network radio wire entry");

/* qmi_modem_status.c */
int bbcmdGetCellLocationInfo(BbcmdRequest *request);
int bbcmdNetworkScan(BbcmdRequest *request);

#endif
