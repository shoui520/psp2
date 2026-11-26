/*
 * Extended ISO 14496-12 / DASH / CENC box subclasses (0x81000000-0x81001A63).
 */

#include "be_isobmf_internal.h"

/* ItemLocationBox (iloc) */

void ItemLocationBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("Struct_ItemLocationBoxHeader", BE_FIELD_BYTES, 2);
		addField("item_count", BE_FIELD_UINT16, 2);
	}

	if (findField("item_count") != NULL && findField("item_count")->isIncomplete() == 0) {
		uint16_t itemCount = *static_cast<const uint16_t *>(findField("item_count")->getData());
		if (itemCount != 0) {
			uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
				static_cast<uint32_t>(getFullHeaderSize()) - 4;
			addField("item_array", BE_FIELD_BYTES, remain);
		}
	}
}

ItemLocationBox::ItemLocationBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_ILOC;
}

/* LevelAssignmentBox (leva) */

void LevelAssignmentBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("level_count", BE_FIELD_UINT8, 1);
	}

	if (findField("level_count") != NULL && findField("level_count")->isIncomplete() == 0) {
		uint32_t levelCount = *static_cast<const uint32_t *>(findField("level_count")->getData());
		if (levelCount > m_levelIndex) {
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "track_id", m_levelIndex);
			addField(m_nameBuf, BE_FIELD_UINT32, 4);
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "padding_flag_assignment_type", m_levelIndex);
			addField(m_nameBuf, BE_FIELD_UINT8, 1);

			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "padding_flag_assignment_type", m_levelIndex);
			if (findField(m_nameBuf) != NULL) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "padding_flag_assignment_type", m_levelIndex);
				if (findField(m_nameBuf)->isIncomplete() == 0) {
					beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "padding_flag_assignment_type", m_levelIndex);
					uint8_t assignType = *static_cast<const uint8_t *>(findField(m_nameBuf)->getData()) & 0x7F;
					if (assignType == 0) {
						beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "grouping_type", m_levelIndex);
						addField(m_nameBuf, BE_FIELD_BYTES, 4);
					} else if (assignType == 1) {
						beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "grouping_type", m_levelIndex);
						addField(m_nameBuf, BE_FIELD_BYTES, 4);
						beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "grouping_type_parameter", m_levelIndex);
						addField(m_nameBuf, BE_FIELD_BYTES, 4);
					} else if (assignType == 4) {
						beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sub" "_track_id", m_levelIndex);
						addField(m_nameBuf, BE_FIELD_UINT32, 4);
					}
					m_levelIndex++;
				}
			}
		}
	}
}

LevelAssignmentBox::LevelAssignmentBox()
{
	m_levelIndex = 0;
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_LEVA;
}

int LevelAssignmentBox::isContainer()
{
	return 0;
}

/* MetaBox (meta) */

MetaBox::MetaBox()
{
	m_boxKindId = ISO_BOX_META;
	m_hasExtendedHeader = 1;
}

/* MovieExtendsHeaderBox (mehd) */

void MovieExtendsHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		uint8_t version = *static_cast<const uint8_t *>(findField("version")->getData());
		if (version == 1)
			addField("fragment_duration", BE_FIELD_UINT64, 8);
		else
			addField("fragment_duration", BE_FIELD_UINT32, 4);
	}
}

MovieExtendsHeaderBox::MovieExtendsHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_MEHD;
}

/* OriginalFormatBox (frma) */

void OriginalFormatBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	m_bodyAdded = 1;
	addField("data_format", BE_FIELD_BYTES, 4);
}

OriginalFormatBox::OriginalFormatBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_FRMA;
}

/* ProtectionSchemeInfoBox (sinf) */

ProtectionSchemeInfoBox::ProtectionSchemeInfoBox()
{
	m_boxKindId = ISO_BOX_SINF;
}

/* ProtectionSystemSpecificHeaderBox (pssh) */

void ProtectionSystemSpecificHeaderBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("SystemID", BE_FIELD_BYTES, 16);
		addField("DataSize", BE_FIELD_UINT32, 4);
	}

	if (findField("DataSize") != NULL && findField("DataSize")->isIncomplete() == 0) {
		uint32_t dataSize = *static_cast<const uint32_t *>(findField("DataSize")->getData());
		addField("Data", BE_FIELD_BYTES, dataSize);
	}
}

ProtectionSystemSpecificHeaderBox::ProtectionSystemSpecificHeaderBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_PSSH;
}

/* SampleAuxInfoOffsetsBox (saio) */

void SampleAuxInfoOffsetsBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
	}

	if (findField("flags") != NULL && findField("flags")->isIncomplete() == 0) {
		if ((isoBoxReadFlags(this) & 1) != 0) {
			addField("aux_info_type", BE_FIELD_UINT32, 4);
			addField("aux_info_type_parameter", BE_FIELD_UINT32, 4);
		}
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("version") != NULL && findField("version")->isIncomplete() == 0 &&
	    findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		if (entryCount != 0) {
			uint32_t version = *static_cast<const uint32_t *>(findField("version")->getData());
			if (version != 0)
				addField("offset", BE_FIELD_UINT64_ARRAY, 8 * entryCount);
			else
				addField("offset", BE_FIELD_UINT32_ARRAY, 4 * entryCount);
		}
	}
}

SampleAuxInfoOffsetsBox::SampleAuxInfoOffsetsBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SAIO;
}

/* SampleAuxInfoSizesBox (saiz) */

void SampleAuxInfoSizesBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
	}

	if (findField("flags") != NULL && findField("flags")->isIncomplete() == 0) {
		if ((isoBoxReadFlags(this) & 1) != 0) {
			addField("aux_info_type", BE_FIELD_UINT32, 4);
			addField("aux_info_type_parameter", BE_FIELD_UINT32, 4);
		}
		addField("default_sample_info_size", BE_FIELD_UINT8, 1);
		addField("sample_count", BE_FIELD_UINT32, 4);
	}

	if (findField("default_sample_info_size") != NULL &&
	    findField("default_sample_info_size")->isIncomplete() == 0 &&
	    findField("sample_count") != NULL &&
	    findField("sample_count")->isIncomplete() == 0) {
		uint32_t sampleCount = *static_cast<const uint32_t *>(findField("sample_count")->getData());
		if (sampleCount != 0)
			addField("sample_info_size", BE_FIELD_BYTES, sampleCount);
	}
}

SampleAuxInfoSizesBox::SampleAuxInfoSizesBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SAIZ;
}

/* SampleGroupDescriptionBox (sgpd) */

void SampleGroupDescriptionBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("grouping_type", BE_FIELD_BYTES, 4);
	}

	if (m_bodyAdded == 1 &&
	    findField("version") != NULL &&
	    findField("version")->isIncomplete() == 0 &&
	    findField("entry_count") == NULL) {
		if (*static_cast<const uint8_t *>(findField("version")->getData()) == 1)
			addField("default_length", BE_FIELD_UINT32, 4);
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (m_bodyAdded == 1 &&
	    findField("version") != NULL &&
	    findField("version")->isIncomplete() == 0 &&
	    findField("entry_count") != NULL &&
	    findField("entry_count")->isIncomplete() == 0) {
		if (*static_cast<const uint8_t *>(findField("version")->getData()) == 0 &&
		    findField("sample_entry_block") == NULL) {
			uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
				static_cast<uint32_t>(getFullHeaderSize()) - 8;
			addField("sample_entry_block", BE_FIELD_BYTES, remain);
		}
	}

	if (m_bodyAdded == 1 &&
	    findField("entry_count") != NULL &&
	    findField("entry_count")->isIncomplete() == 0 &&
	    findField("default_length") != NULL &&
	    findField("default_length")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		if (m_entryIndex < entryCount) {
			uint8_t version = *static_cast<const uint8_t *>(findField("version")->getData());
			uint32_t defaultLen = *static_cast<const uint32_t *>(findField("default_length")->getData());

			if (version == 1 && defaultLen == 0) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "description_length", m_entryIndex);
				if (findField(m_nameBuf) == NULL) {
					beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "description_length", m_entryIndex);
					addField(m_nameBuf, BE_FIELD_UINT32, 4);
				}
			}

			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "description_length", m_entryIndex);
			if (findField(m_nameBuf) != NULL) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "description_length", m_entryIndex);
				if (findField(m_nameBuf)->isIncomplete() == 0) {
					uint32_t descLen = *static_cast<const uint32_t *>(findField("description_length")->getData());
					beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sample_group_entry", m_entryIndex);
					addField(m_nameBuf, BE_FIELD_BYTES, descLen);
					m_entryIndex++;
					return;
				}
			}

			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "description_length", m_entryIndex);
			if (findField(m_nameBuf) == NULL) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sample_group_entry", m_entryIndex);
				addField(m_nameBuf, BE_FIELD_BYTES, defaultLen);
				m_entryIndex++;
			}
		}
	}
}

SampleGroupDescriptionBox::SampleGroupDescriptionBox()
{
	m_isContainer = 0;
	m_entryIndex = 0;
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SGPD;
}

int SampleGroupDescriptionBox::isContainer()
{
	return m_isContainer;
}

/* SampleToGroupBox (sbgp) */

void SampleToGroupBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("grouping_type", BE_FIELD_UINT32, 4);
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		uint32_t entryCount = *static_cast<const uint32_t *>(findField("entry_count")->getData());
		addField("Struct_sample_count", BE_FIELD_UINT32_ARRAY, 8 * entryCount);
	}
}

SampleToGroupBox::SampleToGroupBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SBGP;
}

/* SchemeInformationBox (schi) */

SchemeInformationBox::SchemeInformationBox()
{
	m_boxKindId = ISO_BOX_SCHI;
}

/* SchemeTypeBox (schm) */

void SchemeTypeBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("scheme_type", BE_FIELD_BYTES, 4);
		addField("scheme_version", BE_FIELD_UINT32, 4);
	}

	if (findField("flags") != NULL && findField("flags")->isIncomplete() == 0) {
		if ((isoBoxReadFlags(this) & 1) != 0) {
			int uriLen = static_cast<int>(getBoxSize()) -
				static_cast<int>(getFullHeaderSize()) - 8;
			if (uriLen > 0) {
				uint32_t allocLen = static_cast<uint32_t>(getBoxSize()) -
					static_cast<uint32_t>(getFullHeaderSize()) - 8;
				addField("scheme_uri", BE_FIELD_BYTES, allocLen);
			}
		}
	}
}

SchemeTypeBox::SchemeTypeBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SCHM;
}

/* SegmentIndexBox (sidx) */

void SegmentIndexBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
	}

	if (findField("version") != NULL && findField("version")->isIncomplete() == 0) {
		addField("reference_ID", BE_FIELD_UINT32, 4);
		addField("timescale", BE_FIELD_UINT32, 4);
		if (*static_cast<const uint8_t *>(findField("version")->getData()) != 0) {
			addField("earliest_presentation_time", BE_FIELD_UINT64, 8);
			addField("first_offset", BE_FIELD_UINT64, 8);
		} else {
			addField("earliest_presentation_time", BE_FIELD_UINT32, 4);
			addField("first_offset", BE_FIELD_UINT32, 4);
		}
		addField("reserved ", BE_FIELD_UINT16, 2);
		addField("reference_count", BE_FIELD_UINT16, 2);
	}

	if (findField("reference_count") != NULL && findField("reference_count")->isIncomplete() == 0) {
		uint16_t refCount = *static_cast<const uint16_t *>(findField("reference_count")->getData());
		addField("Struct_reference_data", BE_FIELD_UINT32_ARRAY, 12 * refCount);
	}
}

SegmentIndexBox::SegmentIndexBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SIDX;
}

/* SegmentTypeBox (styp) */

void SegmentTypeBox::onFieldParsed()
{
	IsoBox::onFieldParsed();
	if (m_bodyAdded == 0 && getBytesRead() >= getHeaderSize()) {
		m_bodyAdded = 1;
		addField("major_brand", BE_FIELD_BYTES, 4);
		addField("minor_version", BE_FIELD_UINT32, 4);
		uint64_t brandCount = (getBoxSize() - (getBytesRead() + 8)) / 4;
		for (uint64_t i = 0; i < brandCount; i++) {
			char brandName[64];
			beSnprintf(brandName, sizeof(brandName), "compatible_brands_%d", static_cast<int>(i));
			addField(brandName, BE_FIELD_BYTES, 4);
		}
	}
}

SegmentTypeBox::SegmentTypeBox()
{
	m_bodyAdded = 0;
	m_boxKindId = ISO_BOX_STYP;
	m_hasExtendedHeader = 1;
}

/* SubSampleInformationBox (subs) */

void SubSampleInformationBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("entry_count", BE_FIELD_UINT32, 4);
	}

	if (findField("entry_count") != NULL && findField("entry_count")->isIncomplete() == 0) {
		if (m_entryIndex < *static_cast<const uint32_t *>(findField("entry_count")->getData())) {
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sample_delta", m_entryIndex);
			if (findField(m_nameBuf) == NULL) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "sample_delta", m_entryIndex);
				addField(m_nameBuf, BE_FIELD_UINT32, 4);
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "subsample_count", m_entryIndex);
				addField(m_nameBuf, BE_FIELD_UINT16, 2);
			}
		}
	}

	beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "subsample_count", m_entryIndex);
	if (findField(m_nameBuf) != NULL) {
		beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "subsample_count", m_entryIndex);
		if (findField(m_nameBuf)->isIncomplete() == 0) {
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "subsample_count", m_entryIndex);
			uint16_t subCount = *static_cast<const uint16_t *>(findField(m_nameBuf)->getData());
			if (subCount != 0) {
				if (*static_cast<const uint8_t *>(findField("version")->getData()) == 1) {
					beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "Struct_subsample_entry", m_entryIndex);
					addField(m_nameBuf, BE_FIELD_BYTES, 10 * subCount);
				} else {
					beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "Struct_subsample_entry", m_entryIndex);
					addField(m_nameBuf, BE_FIELD_BYTES, 8 * subCount);
				}
			}
			m_entryIndex++;
		}
	}
}

void SubSampleInformationBox::onBoxComplete()
{
	if (findField("entry_count") != NULL) {
		int entryCount = *static_cast<const int *>(findField("entry_count")->getData());
		for (int i = 0; i < entryCount; i++) {
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "subsample_count", i);
			if (findField(m_nameBuf) != NULL) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "subsample_count", i);
				uint16_t subCount = *static_cast<const uint16_t *>(findField(m_nameBuf)->getData());
				for (int j = 0; j < subCount; j++) {
					if (*static_cast<const uint8_t *>(findField("version")->getData()) == 1) {
						beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "Struct_subsample_entry", i);
						uint8_t *entryBytes = static_cast<uint8_t *>(findField(m_nameBuf)->getData());
						beSwapByteOrder(&entryBytes[j * 10], 4, 4);
					} else {
						beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "Struct_subsample_entry", i);
						uint8_t *entryBytes = static_cast<uint8_t *>(findField(m_nameBuf)->getData());
						beSwapByteOrder(&entryBytes[j * 8], 2, 2);
					}
				}
			}
		}
	}
}

SubSampleInformationBox::SubSampleInformationBox()
{
	m_entryIndex = 0;
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SUBS;
}

/* SubsegmentIndexBox (ssix) */

void SubsegmentIndexBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("subsegment_count", BE_FIELD_UINT32, 4);
		m_subsegmentIndex = 0;
	}

	if (findField("subsegment_count") != NULL && findField("subsegment_count")->isIncomplete() == 0) {
		if (m_subsegmentIndex < *static_cast<const uint32_t *>(findField("subsegment_count")->getData())) {
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "ranges_count", m_subsegmentIndex);
			addField(m_nameBuf, BE_FIELD_UINT32, 4);

			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "ranges_count", m_subsegmentIndex);
			if (findField(m_nameBuf) != NULL) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "ranges_count", m_subsegmentIndex);
				if (findField(m_nameBuf)->isIncomplete() == 0) {
					beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "level8_range_size24", m_subsegmentIndex);
					beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "ranges_count", m_subsegmentIndex);
					uint32_t rangeCount = *static_cast<const uint32_t *>(findField(m_nameBuf)->getData());
					addField(m_nameBuf, BE_FIELD_BYTES, 4 * rangeCount);
					m_subsegmentIndex++;
				}
			}
		}
	}
}

void SubsegmentIndexBox::onBoxComplete()
{
	if (findField("subsegment_count") != NULL) {
		uint32_t subsegCount = *static_cast<const uint32_t *>(findField("subsegment_count")->getData());
		for (uint32_t i = 0; i < subsegCount; i++) {
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "ranges_count", i);
			uint32_t rangeCount = *static_cast<const uint32_t *>(findField(m_nameBuf)->getData());
			beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "level8_range_size24", i);
			if (findField(m_nameBuf) != NULL) {
				beSnprintf(m_nameBuf, sizeof(m_nameBuf), "%s_%d", "level8_range_size24", i);
				uint8_t *rangeBytes = static_cast<uint8_t *>(findField(m_nameBuf)->getData());
				for (uint32_t j = 0; j < rangeCount; j++) {
					uint8_t swapped = rangeBytes[1];
					rangeBytes[1] = rangeBytes[3];
					rangeBytes[3] = swapped;
					rangeBytes += 4;
				}
			}
		}
	}
}

SubsegmentIndexBox::SubsegmentIndexBox()
{
	m_subsegmentIndex = 0;
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_SSIX;
}

/* SubtitleMediaHeaderBox (sthd) */

SubtitleMediaHeaderBox::SubtitleMediaHeaderBox()
{
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_STHD;
}

/* TrackEncryptionBox (tenc) */

void TrackEncryptionBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		addField("default_AlgorithmID", BE_FIELD_UINT24, 3);
		addField("default_IV_size", BE_FIELD_UINT8, 1);
		addField("default_KID", BE_FIELD_BYTES, 16);
	}
}

TrackEncryptionBox::TrackEncryptionBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_TENC;
}

/* TrackFragmentDecodeTimeBox (tfdt) */

void TrackFragmentDecodeTimeBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
	}

	if (findField("version") != NULL && findField("version")->isIncomplete() == 0) {
		if (*static_cast<const uint8_t *>(findField("version")->getData()) == 1)
			addField("base_media_decode_time", BE_FIELD_UINT64, 8);
		else
			addField("base_media_decode_time", BE_FIELD_UINT32, 4);
	}
}

TrackFragmentDecodeTimeBox::TrackFragmentDecodeTimeBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_TFDT;
}

/* XmlBox (xml ) */

void XmlBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getFullHeaderSize());
		addField("xml", BE_FIELD_BYTES, remain);
	}
}

XmlBox::XmlBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_XML;
}

/* BinaryXmlBox (bxml) */

void BinaryXmlBox::onFieldParsed()
{
	IsoFullBox::onFieldParsed();
	if (m_bodyAdded == 0 &&
	    getBytesRead() >= getFullHeaderSize() &&
	    findField("version") != NULL &&
	    findField("flags") != NULL) {
		m_bodyAdded = 1;
		uint32_t remain = static_cast<uint32_t>(getBoxSize()) -
			static_cast<uint32_t>(getFullHeaderSize());
		addField("data", BE_FIELD_BYTES, remain);
	}
}

BinaryXmlBox::BinaryXmlBox()
{
	m_bodyAdded = 0;
	m_hasExtendedHeader = 1;
	m_boxKindId = ISO_BOX_BXML;
}
