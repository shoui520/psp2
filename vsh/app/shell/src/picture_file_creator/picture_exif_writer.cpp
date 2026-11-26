/*
 * picture_exif_writer.cpp: EXIF 2.21 APP1/TIFF/GPS/Interop writer, PlayStation MakerNote
 * builder, and default EXIF/MakerNote initializers for ScePictureFileCreator.
 * 3.65-CEX: 0x81366678 - 0x8136695F, 0x8136731C - 0x8136866F, 0x8136C28C - 0x8136D04F
 */
#include <stddef.h>
#include <stdint.h>
#include <scetypes.h>
#include <std/string.h>
#include <std/stdio.h>
#include <rtc/rtc.h>

#include "picture_file_creator_internal.h"

using namespace paf;

/* --- Public EXIF/MakerNote metadata helpers (0x81366678 - 0x8136695F) --- */

/* 0x81366678 */
int pictureExifInitDefaults(PictureExifData *exif)
{
	if (exif == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	PictureExifRational defaultDpi;
	defaultDpi.numerator = 72;
	defaultDpi.denominator = 1;

	exif->compression.Set(6);
	exif->xResolution.Set(defaultDpi);
	exif->yResolution.Set(defaultDpi);
	exif->resolutionUnit.Set(2);
	exif->yCbCrPositioning.Set(1);

	sce_paf_memcpy(exif->exifVersion.value, "0221", 4);
	exif->exifVersion.present = true;

	exif->componentsConfiguration.value[0] = 1;
	exif->componentsConfiguration.value[1] = 2;
	exif->componentsConfiguration.value[2] = 3;
	exif->componentsConfiguration.value[3] = 0;
	exif->componentsConfiguration.present = true;

	exif->flash.Set(32);

	sce_paf_memcpy(exif->flashpixVersion.value, "0100", 4);
	exif->flashpixVersion.present = true;

	exif->colorSpace.Set(1);
	exif->customRendered.Set(0);
	exif->exposureMode.Set(0);
	exif->whiteBalance.Set(0);
	exif->sceneCaptureType.Set(0);

	exif->interopIndex.Set(paf::string("R98"));

	sce_paf_memcpy(exif->interopVersion.value, "0100", 4);
	exif->interopVersion.present = true;

	return 0;
}

/* 0x813667FC */
int pictureMakerNoteInit(PictureMakerNote *makerNote)
{
	if (makerNote == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	makerNote->formatVersion.Set(paf::string("1.0"));
	return 0;
}

/* 0x81366876 */
int pictureExifSetDateTime(PictureExifData *exif, paf::rtc::Tick tick)
{
	if (exif == NULL)
		return PIC_CREATOR_ERROR_INVALID_ARGUMENT;

	paf::rtc::DateTime dateTime;
	int ret = paf::rtc::ConvertTickToDateTime(&tick, &dateTime, NULL);
	if (ret == 0) {
		char formatted[20];
		sce_paf_snprintf(formatted, sizeof(formatted), "%04d:%02d:%02d %02d:%02d:%02d",
				 dateTime.year, dateTime.month, dateTime.day, dateTime.hour,
				 dateTime.minute, dateTime.second);
		formatted[19] = '\0';

		paf::string stamp(formatted);
		exif->dateTime.Set(stamp);
		exif->dateTimeOriginal.Set(stamp);
		exif->dateTimeDigitized.Set(stamp);
	}
	return ret;
}

/* --- PictureByteWriter --- */

PictureByteWriter::PictureByteWriter()
	: m_base(NULL), m_capacity(0), m_cursor(0), m_bigEndian(true)
{
}

bool PictureByteWriter::Attach(void *buffer, uint32_t capacity)
{
	if (m_base != NULL)
		return false;
	if (buffer == NULL || capacity == 0) {
		Reset();
		return false;
	}
	m_base = static_cast<uint8_t *>(buffer);
	m_capacity = capacity;
	m_cursor = 0;
	return true;
}

void PictureByteWriter::Reset()
{
	m_base = NULL;
	m_capacity = 0;
	m_cursor = 0;
}

bool PictureByteWriter::WriteRaw(const void *src, uint32_t len)
{
	if (m_base == NULL || m_cursor + len > m_capacity)
		return false;
	sce_paf_memcpy(m_base + m_cursor, src, len);
	m_cursor += len;
	return true;
}

bool PictureByteWriter::WriteValue(const void *src, uint32_t len)
{
	if (!m_bigEndian)
		return WriteRaw(src, len);
	if (len > 8 || m_base == NULL || m_cursor + len > m_capacity)
		return false;

	uint8_t swapped[8];
	const uint8_t *bytes = static_cast<const uint8_t *>(src);
	for (uint32_t i = 0; i < len; ++i)
		swapped[i] = bytes[len - 1 - i];
	return WriteRaw(swapped, len);
}

bool PictureByteWriter::ReserveZero(uint32_t len, uint32_t *outOffset)
{
	if (m_base == NULL || m_cursor + len > m_capacity)
		return false;
	if (outOffset != NULL)
		*outOffset = m_cursor;
	sce_paf_memset(m_base + m_cursor, 0, len);
	m_cursor += len;
	return true;
}

uint32_t PictureByteWriter::Tell() const
{
	return m_cursor;
}

bool PictureByteWriter::Seek(uint32_t pos)
{
	if (pos > m_capacity)
		return false;
	m_cursor = pos;
	return true;
}

void PictureByteWriter::GetResult(void **outPtr, uint32_t *outLen) const
{
	if (outPtr != NULL)
		*outPtr = m_base;
	if (outLen != NULL)
		*outLen = m_cursor;
}

/* --- PictureExifApp1Writer (0x8136C28C - 0x8136D04F) --- */

struct ExifTagTypeLayout {
	uint32_t unitSize;
	uint32_t swapSize;
};

static const ExifTagTypeLayout kExifTypeTable[11] = {
	{ 0, 0 }, { 1, 1 }, { 1, 1 }, { 2, 2 }, { 4, 4 }, { 8, 4 },
	{ 0, 0 }, { 1, 1 }, { 0, 0 }, { 4, 4 }, { 8, 4 }
};

PictureExifApp1Writer::PictureExifApp1Writer()
	: m_ifdCapacity(0),
	  m_ifdWritten(0),
	  m_inApp1(false),
	  m_inTiff(false),
	  m_inIfd(false),
	  m_buffer(NULL),
	  m_bufferSize(4096),
	  m_tiffBase(0),
	  m_app1LenOffset(0),
	  m_nextIfdOffset(0),
	  m_externalPayloadSize(0),
	  m_ifdEntryBase(0)
{
	m_buffer = static_cast<uint8_t *>(
		ScePafStdc_3CF19248(m_bufferSize, pictureWorkHeapGet()));
}

PictureExifApp1Writer::~PictureExifApp1Writer()
{
	if (m_buffer != NULL) {
		operator delete(m_buffer);
		m_buffer = NULL;
	}
}

int PictureExifApp1Writer::BeginApp1()
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (m_inApp1)
		return PIC_EXIF_ERROR_INVALID_STATE;

	m_externalPayloadSize = 0;
	bool ok = m_stream.Attach(m_buffer, m_bufferSize);
	static const uint8_t app1Marker[2] = { 0xFF, 0xE1 };
	static const uint8_t exifHeader[6] = { 'E', 'x', 'i', 'f', '\0', '\0' };

	ok = ok && m_stream.WriteRaw(app1Marker, 2);
	ok = ok && m_stream.ReserveZero(2, &m_app1LenOffset);
	ok = ok && m_stream.WriteRaw(exifHeader, 6);
	m_inApp1 = true;
	return ok ? 0 : PIC_EXIF_ERROR_GENERIC;
}

int PictureExifApp1Writer::BeginTiff(uint32_t *outMainIfdPatch)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (!m_inApp1 || m_inTiff)
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (outMainIfdPatch == NULL)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;

	m_tiffBase = m_stream.Tell();
	static const uint8_t byteOrder[2] = { 'M', 'M' };
	uint16_t tiffMagic = 42;
	bool ok = m_stream.WriteRaw(byteOrder, 2);
	ok = ok && m_stream.WriteValue(&tiffMagic, 2);
	ok = ok && m_stream.ReserveZero(4, outMainIfdPatch);
	m_inTiff = true;
	return ok ? 0 : PIC_EXIF_ERROR_GENERIC;
}

int PictureExifApp1Writer::BeginIfd(uint16_t entryCount, uint32_t patchOffset)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (!m_inTiff || m_inIfd || !m_inApp1)
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (entryCount == 0 || patchOffset == 0)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;

	uint32_t curPos = m_stream.Tell();
	uint32_t relOffset = curPos - m_tiffBase;
	bool ok = m_stream.Seek(patchOffset);
	ok = ok && m_stream.WriteValue(&relOffset, 4);
	ok = ok && m_stream.Seek(curPos);
	ok = ok && m_stream.WriteValue(&entryCount, 2);
	ok = ok && m_stream.ReserveZero(12u * entryCount, &m_ifdEntryBase);
	ok = ok && m_ifdWriter.Attach(m_buffer + m_ifdEntryBase, 12u * entryCount);
	ok = ok && m_stream.ReserveZero(4, &m_nextIfdOffset);
	m_ifdCapacity = entryCount;
	m_ifdWritten = 0;
	m_inIfd = true;
	return ok ? 0 : PIC_EXIF_ERROR_GENERIC;
}

int PictureExifApp1Writer::WriteTag(uint16_t tag, uint16_t tagType, uint32_t count,
				    const void *value)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (!m_inIfd || m_ifdWritten >= m_ifdCapacity)
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (count == 0 || value == NULL || tagType >= 11 ||
	    kExifTypeTable[tagType].unitSize == 0)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;

	const ExifTagTypeLayout &layout = kExifTypeTable[tagType];
	bool ok = m_ifdWriter.WriteValue(&tag, 2);
	ok = ok && m_ifdWriter.WriteValue(&tagType, 2);
	ok = ok && m_ifdWriter.WriteValue(&count, 4);

	uint32_t byteLen = layout.unitSize * count;
	PictureByteWriter *target = &m_ifdWriter;
	uint32_t paddedLen = 4;

	if (byteLen > 4) {
		uint32_t dataRelOffset = m_stream.Tell() - m_tiffBase;
		ok = ok && m_ifdWriter.WriteValue(&dataRelOffset, 4);
		target = &m_stream;
		paddedLen = (byteLen + 1u) & ~1u;
	}

	const uint8_t *srcBytes = static_cast<const uint8_t *>(value);
	for (uint32_t cursor = 0; cursor < byteLen; cursor += layout.swapSize)
		ok = ok && target->WriteValue(srcBytes + cursor, layout.swapSize);
	ok = ok && target->ReserveZero(paddedLen - byteLen, NULL);
	++m_ifdWritten;
	return ok ? 0 : PIC_EXIF_ERROR_GENERIC;
}

int PictureExifApp1Writer::WriteDeferredOffsetTag(uint16_t tag, uint16_t tagType, uint32_t count,
						  uint32_t *outPatchPos)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (!m_inIfd || m_ifdWritten >= m_ifdCapacity)
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (count == 0 || outPatchPos == NULL)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;

	bool ok = m_ifdWriter.WriteValue(&tag, 2);
	ok = ok && m_ifdWriter.WriteValue(&tagType, 2);
	ok = ok && m_ifdWriter.WriteValue(&count, 4);
	ok = ok && m_ifdWriter.ReserveZero(4, outPatchPos);
	*outPatchPos += m_ifdEntryBase;
	++m_ifdWritten;
	return ok ? 0 : PIC_EXIF_ERROR_GENERIC;
}

int PictureExifApp1Writer::PatchExternalPayload(uint32_t payloadSize, uint32_t patchPos)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (!m_inTiff || !m_inIfd || !m_inApp1)
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (payloadSize == 0 || patchPos == 0)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;

	uint32_t curPos = m_stream.Tell();
	uint32_t relOffset = curPos - m_tiffBase;
	bool ok = m_stream.Seek(patchPos);
	ok = ok && m_stream.WriteValue(&relOffset, 4);
	ok = ok && m_stream.Seek(curPos);
	if (ok)
		m_externalPayloadSize += payloadSize;
	return ok ? 0 : PIC_EXIF_ERROR_GENERIC;
}

int PictureExifApp1Writer::EndIfd(uint32_t *outNextIfdPatchPos)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (!m_inIfd || m_ifdWritten != m_ifdCapacity)
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (outNextIfdPatchPos != NULL)
		*outNextIfdPatchPos = m_nextIfdOffset;
	m_ifdWriter.Reset();
	m_inIfd = false;
	return 0;
}

int PictureExifApp1Writer::EndApp1(void **outPtr, uint32_t *outLen)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (outPtr == NULL || outLen == NULL)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;
	if (!m_inTiff || m_inIfd || !m_inApp1)
		return PIC_EXIF_ERROR_INVALID_STATE;

	uint32_t totalBytes = m_stream.Tell();
	uint16_t segmentLen = static_cast<uint16_t>(totalBytes + m_externalPayloadSize - 2);
	bool ok = m_stream.Seek(m_app1LenOffset);
	ok = ok && m_stream.WriteValue(&segmentLen, 2);
	ok = ok && m_stream.Seek(totalBytes);
	m_stream.GetResult(outPtr, outLen);
	m_stream.Reset();
	m_inApp1 = false;
	m_inTiff = false;
	m_inIfd = false;
	return ok ? 0 : PIC_EXIF_ERROR_GENERIC;
}

/* --- PictureMakerNoteWriter (0x81367F78 - 0x8136866F) --- */

PictureMakerNoteWriter::PictureMakerNoteWriter()
	: m_buffer(NULL),
	  m_capacity(2048),
	  m_sizePatchOffset(0),
	  m_countPatchOffset(0),
	  m_entryCount(0),
	  m_active(false)
{
	m_buffer = static_cast<uint8_t *>(
		ScePafStdc_3CF19248(m_capacity, pictureWorkHeapGet()));
}

PictureMakerNoteWriter::~PictureMakerNoteWriter()
{
	if (m_buffer != NULL) {
		operator delete(m_buffer);
		m_buffer = NULL;
	}
}

int PictureMakerNoteWriter::Begin()
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (m_active)
		return PIC_EXIF_ERROR_INVALID_STATE;

	static const char headerMagic[12] = "SONY PS";
	static const char vendorId[3] = { 'S', 'I', 'E' };
	uint8_t verMajor = 1;
	uint8_t verMinor = 0;
	uint8_t headerFlags = 0;

	bool ok = m_stream.Attach(m_buffer, m_capacity);
	ok = ok && m_stream.WriteRaw(headerMagic, 12);
	ok = ok && m_stream.WriteRaw(vendorId, 3);
	ok = ok && m_stream.WriteValue(&verMajor, 1);
	ok = ok && m_stream.WriteValue(&verMinor, 1);
	ok = ok && m_stream.WriteValue(&headerFlags, 1);
	ok = ok && m_stream.ReserveZero(4, &m_sizePatchOffset);
	ok = ok && m_stream.ReserveZero(2, &m_countPatchOffset);
	m_entryCount = 0;
	m_active = true;
	return ok ? 0 : PIC_EXIF_ERROR_OUT_OF_MEMORY;
}

int PictureMakerNoteWriter::AddStringTag(const char *tagName, uint16_t tagFlags, const char *str,
					 uint32_t len)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (!m_active)
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (tagName == NULL || str == NULL || len == 0)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;
	if (m_entryCount >= 10)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;

	++m_entryCount;
	static const uint8_t tagPrefix[4] = { 'T', 'X', 'X', 'X' };
	static const uint8_t charsetPad[3] = { 'X', 'X', 'X' };
	uint32_t recordLen = len + 9;
	uint8_t encoding = 3;

	bool ok = m_stream.WriteRaw(tagPrefix, 4);
	ok = ok && m_stream.WriteValue(&recordLen, 4);
	ok = ok && m_stream.WriteValue(&tagFlags, 2);
	ok = ok && m_stream.WriteValue(&encoding, 1);
	ok = ok && m_stream.WriteRaw(charsetPad, 3);
	ok = ok && m_stream.WriteRaw(tagName, 5);
	ok = ok && m_stream.WriteRaw(str, len);
	return ok ? 0 : PIC_EXIF_ERROR_OUT_OF_MEMORY;
}

int PictureMakerNoteWriter::AddGeoPairTag(const char *tagName, uint16_t tagFlags, int32_t firstVal,
					  int32_t secondVal)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (!m_active)
		return PIC_EXIF_ERROR_INVALID_STATE;
	if (tagName == NULL)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;
	if (m_entryCount >= 10)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;

	++m_entryCount;
	static const uint8_t geoPrefix[4] = { 'G', 'E', 'O', 'B' };
	uint32_t recordLen = 16;
	uint8_t zeroByte = 0;

	bool ok = m_stream.WriteRaw(geoPrefix, 4);
	ok = ok && m_stream.WriteValue(&recordLen, 4);
	ok = ok && m_stream.WriteValue(&tagFlags, 2);
	ok = ok && m_stream.WriteValue(&zeroByte, 1);
	ok = ok && m_stream.WriteValue(&zeroByte, 1);
	ok = ok && m_stream.WriteValue(&zeroByte, 1);
	ok = ok && m_stream.WriteRaw(tagName, 5);
	ok = ok && m_stream.WriteValue(&firstVal, 4);
	ok = ok && m_stream.WriteValue(&secondVal, 4);
	return ok ? 0 : PIC_EXIF_ERROR_OUT_OF_MEMORY;
}

int PictureMakerNoteWriter::End(void **outPtr, uint32_t *outLen)
{
	if (m_buffer == NULL)
		return PIC_EXIF_ERROR_OUT_OF_MEMORY;
	if (outPtr == NULL || outLen == NULL)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;
	if (!m_active)
		return PIC_EXIF_ERROR_INVALID_STATE;

	uint32_t endPos = m_stream.Tell();
	uint32_t payloadLen = endPos - 22;
	uint16_t entryCountShort = static_cast<uint16_t>(m_entryCount);

	bool ok = m_stream.Seek(m_sizePatchOffset);
	ok = ok && m_stream.WriteValue(&payloadLen, 4);
	ok = ok && m_stream.Seek(m_countPatchOffset);
	ok = ok && m_stream.WriteValue(&entryCountShort, 2);
	ok = ok && m_stream.Seek(endPos);
	m_stream.GetResult(outPtr, outLen);
	m_stream.Reset();
	m_active = false;
	return ok ? 0 : PIC_EXIF_ERROR_GENERIC;
}

static inline int mergeUnsignedErr(int currentErr, int stepErr)
{
	return (static_cast<uint32_t>(stepErr) > static_cast<uint32_t>(currentErr)) ? stepErr
										    : currentErr;
}

/* 0x81367FD0 */
int PictureMakerNoteWriter::Build(const PictureMakerNote *makerNote, void **outPtr,
				  uint32_t *outLen)
{
	if (makerNote == NULL || outPtr == NULL || outLen == NULL)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;

	int err = Begin();

	struct MakerStringSpec {
		const char *tagName;
		const PictureExifField<paf::string> *member;
		uint32_t maxLen;
	};
	const MakerStringSpec strSpecs[7] = {
		{ "PEFV", &makerNote->formatVersion, 4 },
		{ "GCID", &makerNote->gameContentId, 49 },
		{ "GPNM", &makerNote->gameProductName, 13 },
		{ "GDSC", &makerNote->gameDescription, 512 },
		{ "ALBM", &makerNote->albumTitle, 512 },
		{ "PSIS", &makerNote->photoSourceInfo, 2 },
		{ "PTYP", &makerNote->photoType, 2 }
	};

	for (int i = 0; i < 7; ++i) {
		if (strSpecs[i].member->present) {
			uint32_t len = static_cast<uint32_t>(strSpecs[i].member->value.size()) + 1;
			int tagErr = (len <= strSpecs[i].maxLen)
					     ? AddStringTag(strSpecs[i].tagName, 0,
							    strSpecs[i].member->value.c_str(), len)
					     : PIC_EXIF_ERROR_STRING_TOO_LONG;
			err = mergeUnsignedErr(err, tagErr);
		}
	}

	struct MakerGeoSpec {
		const char *tagName;
		const PictureExifField<PictureMakerNoteGeoPair> *member;
	};
	const MakerGeoSpec geoSpecs[4] = {
		{ "LLAC", &makerNote->locationAccuracy },
		{ "PHFR", &makerNote->panHorizontalRate },
		{ "PVFR", &makerNote->panVerticalRate },
		{ "PYPR", &makerNote->panYawPitchRoll }
	};

	for (int i = 0; i < 4; ++i) {
		if (geoSpecs[i].member->present) {
			int tagErr = AddGeoPairTag(geoSpecs[i].tagName, 0,
						   geoSpecs[i].member->value.first,
						   geoSpecs[i].member->value.second);
			err = mergeUnsignedErr(err, tagErr);
		}
	}

	int endErr = End(outPtr, outLen);
	return mergeUnsignedErr(err, endErr);
}

/* --- Complete EXIF APP1 segment serializer (0x8136752E) --- */

static uint16_t countGpsIfdTags(const PictureExifGpsData *gps)
{
	if (gps == NULL)
		return 0;
	return static_cast<uint16_t>(
		(gps->version.present ? 1 : 0) + (gps->latRef.present ? 1 : 0) +
		(gps->latitude.present ? 1 : 0) + (gps->lonRef.present ? 1 : 0) +
		(gps->longitude.present ? 1 : 0) + (gps->altitudeRef.present ? 1 : 0) +
		(gps->altitude.present ? 1 : 0) + (gps->timeStamp.present ? 1 : 0) +
		(gps->measureMode.present ? 1 : 0) + (gps->dop.present ? 1 : 0) +
		(gps->speedRef.present ? 1 : 0) + (gps->speed.present ? 1 : 0) +
		(gps->trackRef.present ? 1 : 0) + (gps->track.present ? 1 : 0) +
		(gps->imgDirectionRef.present ? 1 : 0) + (gps->imgDirection.present ? 1 : 0) +
		(gps->mapDatum.present ? 1 : 0) + (gps->dateStamp.present ? 1 : 0));
}

int pictureBuildExifApp1(PictureExifApp1Writer &writer, const PictureExifData *exif,
			 uint32_t thumbSize, const void *makerNotePtr, uint32_t makerNoteSize,
			 void **outPtr, uint32_t *outLen)
{
	if (exif == NULL || outPtr == NULL || outLen == NULL)
		return PIC_EXIF_ERROR_INVALID_ARGUMENT;

	const PictureExifGpsData *gps =
		static_cast<const PictureExifGpsData *>(exif->gpsData);

	uint16_t interopCount = static_cast<uint16_t>(
		(exif->interopIndex.present ? 1 : 0) + (exif->interopVersion.present ? 1 : 0));

	bool hasMakerNote = (makerNotePtr != NULL && makerNoteSize != 0);
	uint16_t exifCount = static_cast<uint16_t>(
		(exif->exifVersion.present ? 1 : 0) + (exif->dateTimeOriginal.present ? 1 : 0) +
		(exif->dateTimeDigitized.present ? 1 : 0) +
		(exif->componentsConfiguration.present ? 1 : 0) + (exif->flash.present ? 1 : 0) +
		(hasMakerNote ? 1 : 0) + (exif->flashpixVersion.present ? 1 : 0) +
		(exif->colorSpace.present ? 1 : 0) + (exif->pixelXDimension.present ? 1 : 0) +
		(exif->pixelYDimension.present ? 1 : 0) + (interopCount != 0 ? 1 : 0) +
		(exif->customRendered.present ? 1 : 0) + (exif->exposureMode.present ? 1 : 0) +
		(exif->whiteBalance.present ? 1 : 0) + (exif->sceneCaptureType.present ? 1 : 0));

	uint16_t gpsCount = countGpsIfdTags(gps);

	uint16_t mainIfdCount = static_cast<uint16_t>(
		(exif->make.present ? 1 : 0) + (exif->model.present ? 1 : 0) +
		(exif->orientation.present ? 1 : 0) + (exif->xResolution.present ? 1 : 0) +
		(exif->yResolution.present ? 1 : 0) + (exif->resolutionUnit.present ? 1 : 0) +
		(exif->software.present ? 1 : 0) + (exif->dateTime.present ? 1 : 0) +
		(exif->yCbCrPositioning.present ? 1 : 0) + (exifCount != 0 ? 1 : 0) +
		(gpsCount != 0 ? 1 : 0));

	uint16_t thumbIfdCount = 0;
	if (thumbSize != 0) {
		thumbIfdCount = static_cast<uint16_t>(
			(exif->compression.present ? 1 : 0) + (exif->orientation.present ? 1 : 0) +
			(exif->xResolution.present ? 1 : 0) + (exif->yResolution.present ? 1 : 0) +
			(exif->resolutionUnit.present ? 1 : 0) + 2);
	}

	uint32_t mainIfdPatch = 0;
	uint32_t exifIfdPatch = 0;
	uint32_t gpsIfdPatch = 0;
	uint32_t interopIfdPatch = 0;
	uint32_t thumbIfdPatch = 0;

	int err = writer.BeginApp1();
	err = mergeUnsignedErr(err, writer.BeginTiff(&mainIfdPatch));

	if (mainIfdCount != 0) {
		err = mergeUnsignedErr(err, writer.BeginIfd(mainIfdCount, mainIfdPatch));
		if (exif->make.present)
			err = mergeUnsignedErr(err, writer.WriteTag(271, 2,
				static_cast<uint32_t>(exif->make.value.size()) + 1,
				exif->make.value.c_str()));
		if (exif->model.present)
			err = mergeUnsignedErr(err, writer.WriteTag(272, 2,
				static_cast<uint32_t>(exif->model.value.size()) + 1,
				exif->model.value.c_str()));
		if (exif->orientation.present)
			err = mergeUnsignedErr(err, writer.WriteTag(274, 3, 1,
				&exif->orientation.value));
		if (exif->xResolution.present)
			err = mergeUnsignedErr(err, writer.WriteTag(282, 5, 1,
				&exif->xResolution.value));
		if (exif->yResolution.present)
			err = mergeUnsignedErr(err, writer.WriteTag(283, 5, 1,
				&exif->yResolution.value));
		if (exif->resolutionUnit.present)
			err = mergeUnsignedErr(err, writer.WriteTag(296, 3, 1,
				&exif->resolutionUnit.value));
		if (exif->software.present)
			err = mergeUnsignedErr(err, writer.WriteTag(305, 2,
				static_cast<uint32_t>(exif->software.value.size()) + 1,
				exif->software.value.c_str()));
		if (exif->dateTime.present)
			err = mergeUnsignedErr(err, writer.WriteTag(306, 2,
				static_cast<uint32_t>(exif->dateTime.value.size()) + 1,
				exif->dateTime.value.c_str()));
		if (exif->yCbCrPositioning.present)
			err = mergeUnsignedErr(err, writer.WriteTag(531, 3, 1,
				&exif->yCbCrPositioning.value));
		if (exifCount != 0)
			err = mergeUnsignedErr(err, writer.WriteDeferredOffsetTag(34665, 4, 1,
				&exifIfdPatch));
		if (gpsCount != 0)
			err = mergeUnsignedErr(err, writer.WriteDeferredOffsetTag(34853, 4, 1,
				&gpsIfdPatch));
		err = mergeUnsignedErr(err, writer.EndIfd(&thumbIfdPatch));
	}

	if (exifCount != 0) {
		err = mergeUnsignedErr(err, writer.BeginIfd(exifCount, exifIfdPatch));
		if (exif->exifVersion.present)
			err = mergeUnsignedErr(err, writer.WriteTag(36864, 7, 4,
				exif->exifVersion.value));
		if (exif->dateTimeOriginal.present)
			err = mergeUnsignedErr(err, writer.WriteTag(36867, 2,
				static_cast<uint32_t>(exif->dateTimeOriginal.value.size()) + 1,
				exif->dateTimeOriginal.value.c_str()));
		if (exif->dateTimeDigitized.present)
			err = mergeUnsignedErr(err, writer.WriteTag(36868, 2,
				static_cast<uint32_t>(exif->dateTimeDigitized.value.size()) + 1,
				exif->dateTimeDigitized.value.c_str()));
		if (exif->componentsConfiguration.present)
			err = mergeUnsignedErr(err, writer.WriteTag(37121, 7, 4,
				exif->componentsConfiguration.value));
		if (exif->flash.present)
			err = mergeUnsignedErr(err, writer.WriteTag(37385, 3, 1,
				&exif->flash.value));
		if (hasMakerNote)
			err = mergeUnsignedErr(err, writer.WriteTag(37500, 7, makerNoteSize,
				makerNotePtr));
		if (exif->flashpixVersion.present)
			err = mergeUnsignedErr(err, writer.WriteTag(40960, 7, 4,
				exif->flashpixVersion.value));
		if (exif->colorSpace.present)
			err = mergeUnsignedErr(err, writer.WriteTag(40961, 3, 1,
				&exif->colorSpace.value));
		if (exif->pixelXDimension.present)
			err = mergeUnsignedErr(err, writer.WriteTag(40962, 4, 1,
				&exif->pixelXDimension.value));
		if (exif->pixelYDimension.present)
			err = mergeUnsignedErr(err, writer.WriteTag(40963, 4, 1,
				&exif->pixelYDimension.value));
		if (interopCount != 0)
			err = mergeUnsignedErr(err, writer.WriteDeferredOffsetTag(40965, 4, 1,
				&interopIfdPatch));
		if (exif->customRendered.present)
			err = mergeUnsignedErr(err, writer.WriteTag(41985, 3, 1,
				&exif->customRendered.value));
		if (exif->exposureMode.present)
			err = mergeUnsignedErr(err, writer.WriteTag(41986, 3, 1,
				&exif->exposureMode.value));
		if (exif->whiteBalance.present)
			err = mergeUnsignedErr(err, writer.WriteTag(41987, 3, 1,
				&exif->whiteBalance.value));
		if (exif->sceneCaptureType.present)
			err = mergeUnsignedErr(err, writer.WriteTag(41990, 3, 1,
				&exif->sceneCaptureType.value));
		err = mergeUnsignedErr(err, writer.EndIfd(NULL));
	}

	if (gpsCount != 0 && gps != NULL) {
		err = mergeUnsignedErr(err, writer.BeginIfd(gpsCount, gpsIfdPatch));
		if (gps->version.present)
			err = mergeUnsignedErr(err, writer.WriteTag(0, 1, 4, gps->version.value));
		if (gps->latRef.present)
			err = mergeUnsignedErr(err, writer.WriteTag(1, 2,
				static_cast<uint32_t>(gps->latRef.value.size()) + 1,
				gps->latRef.value.c_str()));
		if (gps->latitude.present)
			err = mergeUnsignedErr(err, writer.WriteTag(2, 5, 3, gps->latitude.value));
		if (gps->lonRef.present)
			err = mergeUnsignedErr(err, writer.WriteTag(3, 2,
				static_cast<uint32_t>(gps->lonRef.value.size()) + 1,
				gps->lonRef.value.c_str()));
		if (gps->longitude.present)
			err = mergeUnsignedErr(err, writer.WriteTag(4, 5, 3, gps->longitude.value));
		if (gps->altitudeRef.present)
			err = mergeUnsignedErr(err, writer.WriteTag(5, 1, 1,
				&gps->altitudeRef.value));
		if (gps->altitude.present)
			err = mergeUnsignedErr(err, writer.WriteTag(6, 5, 1, &gps->altitude.value));
		if (gps->timeStamp.present)
			err = mergeUnsignedErr(err, writer.WriteTag(7, 5, 3, gps->timeStamp.value));
		if (gps->measureMode.present)
			err = mergeUnsignedErr(err, writer.WriteTag(10, 2,
				static_cast<uint32_t>(gps->measureMode.value.size()) + 1,
				gps->measureMode.value.c_str()));
		if (gps->dop.present)
			err = mergeUnsignedErr(err, writer.WriteTag(11, 5, 1, &gps->dop.value));
		if (gps->speedRef.present)
			err = mergeUnsignedErr(err, writer.WriteTag(12, 2,
				static_cast<uint32_t>(gps->speedRef.value.size()) + 1,
				gps->speedRef.value.c_str()));
		if (gps->speed.present)
			err = mergeUnsignedErr(err, writer.WriteTag(13, 5, 1, &gps->speed.value));
		if (gps->trackRef.present)
			err = mergeUnsignedErr(err, writer.WriteTag(14, 2,
				static_cast<uint32_t>(gps->trackRef.value.size()) + 1,
				gps->trackRef.value.c_str()));
		if (gps->track.present)
			err = mergeUnsignedErr(err, writer.WriteTag(15, 5, 1, &gps->track.value));
		if (gps->imgDirectionRef.present)
			err = mergeUnsignedErr(err, writer.WriteTag(16, 2,
				static_cast<uint32_t>(gps->imgDirectionRef.value.size()) + 1,
				gps->imgDirectionRef.value.c_str()));
		if (gps->imgDirection.present)
			err = mergeUnsignedErr(err, writer.WriteTag(17, 5, 1,
				&gps->imgDirection.value));
		if (gps->mapDatum.present)
			err = mergeUnsignedErr(err, writer.WriteTag(18, 2,
				static_cast<uint32_t>(gps->mapDatum.value.size()) + 1,
				gps->mapDatum.value.c_str()));
		if (gps->dateStamp.present)
			err = mergeUnsignedErr(err, writer.WriteTag(29, 2,
				static_cast<uint32_t>(gps->dateStamp.value.size()) + 1,
				gps->dateStamp.value.c_str()));
		err = mergeUnsignedErr(err, writer.EndIfd(NULL));
	}

	if (interopCount != 0) {
		err = mergeUnsignedErr(err, writer.BeginIfd(interopCount, interopIfdPatch));
		if (exif->interopIndex.present)
			err = mergeUnsignedErr(err, writer.WriteTag(1, 2,
				static_cast<uint32_t>(exif->interopIndex.value.size()) + 1,
				exif->interopIndex.value.c_str()));
		if (exif->interopVersion.present)
			err = mergeUnsignedErr(err, writer.WriteTag(2, 7, 4,
				exif->interopVersion.value));
		err = mergeUnsignedErr(err, writer.EndIfd(NULL));
	}

	if (thumbIfdCount != 0) {
		uint32_t thumbOffsetPatch = 0;
		err = mergeUnsignedErr(err, writer.BeginIfd(thumbIfdCount, thumbIfdPatch));
		if (exif->compression.present)
			err = mergeUnsignedErr(err, writer.WriteTag(259, 3, 1,
				&exif->compression.value));
		if (exif->orientation.present)
			err = mergeUnsignedErr(err, writer.WriteTag(274, 3, 1,
				&exif->orientation.value));
		if (exif->xResolution.present)
			err = mergeUnsignedErr(err, writer.WriteTag(282, 5, 1,
				&exif->xResolution.value));
		if (exif->yResolution.present)
			err = mergeUnsignedErr(err, writer.WriteTag(283, 5, 1,
				&exif->yResolution.value));
		if (exif->resolutionUnit.present)
			err = mergeUnsignedErr(err, writer.WriteTag(296, 3, 1,
				&exif->resolutionUnit.value));
		err = mergeUnsignedErr(err, writer.WriteDeferredOffsetTag(513, 4, 1,
			&thumbOffsetPatch));
		err = mergeUnsignedErr(err, writer.WriteTag(514, 4, 1, &thumbSize));
		err = mergeUnsignedErr(err, writer.PatchExternalPayload(thumbSize,
			thumbOffsetPatch));
		err = mergeUnsignedErr(err, writer.EndIfd(NULL));
	}

	return mergeUnsignedErr(err, writer.EndApp1(outPtr, outLen));
}
