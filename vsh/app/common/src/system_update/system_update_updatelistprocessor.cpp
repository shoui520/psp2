/*
 * system_update_updatelistprocessor.cpp: bXCe's handler that reads the update list
 * (0x811331B4-0x81134E5C). The list nests its elements as
 *
 *	<update_data_list>
 *	  <region id="...">			one per CDN region; the console's is read
 *	    <version system_version="..." label="...">
 *	      <update_data update_type="full|patch|multiple_patch" current_system_version="...">
 *	        <image size="...">url</image>
 *	      </update_data>
 *	    </version>
 *	    <recovery spkg_type="...">		a package m_pRcvInfo_array asks for
 *	      <image spkg_version="..." size="...">url</image>
 *	    </recovery>
 *	  </region>
 *	</update_data_list>
 *
 * and each region's versions and their images become the SystemUpdateList's entries.
 */
#include <stddef.h>
#include <scetypes.h>
#include <std/stdlib.h>
#include <std/string.h>
#include <std/string>

#include "system_update_updatelistprocessor.h"

using namespace paf;

/* the update list's errors (PLACEHOLDER names) */
#define UPDATE_LIST_ERROR_VERSION		((int)0x80182200)
#define UPDATE_LIST_ERROR_FROM_VERSION		((int)0x80182201)
#define UPDATE_LIST_ERROR_UPDATE_TYPE		((int)0x80182202)
#define UPDATE_LIST_ERROR_VERSION_COUNT		((int)0x80182203)
#define UPDATE_LIST_ERROR_DATA_COUNT		((int)0x80182204)
#define UPDATE_LIST_ERROR_TEXT_LENGTH		((int)0x80182205)
#define UPDATE_LIST_ERROR_NO_REGION_ID		((int)0x80182206)
#define UPDATE_LIST_ERROR_NO_SYSTEM_VERSION	((int)0x80182207)
#define UPDATE_LIST_ERROR_NO_FROM_VERSION	((int)0x80182208)
#define UPDATE_LIST_ERROR_NO_ROOT		((int)0x80182209)
#define UPDATE_LIST_ERROR_NO_REGION		((int)0x8018220A)
#define UPDATE_LIST_ERROR_NO_VERSION		((int)0x8018220B)
#define UPDATE_LIST_ERROR_NO_DATA		((int)0x8018220C)
#define UPDATE_LIST_ERROR_STATE			((int)0x8018220D)
#define UPDATE_LIST_ERROR_NO_SPKG_TYPE		((int)0x8018220E)

#define UPDATE_LIST_TEXT_MAX		1024	/* PLACEHOLDER names */
#define UPDATE_LIST_VERSION_MAX		20
#define UPDATE_LIST_DATA_MAX		256

/* Sony's log names the file by its build path */
#define UPLIST_FILE \
	"W:\\Build\\J01509657\\vsh\\app\\common\\src\\system_update\\system_update_updatelistprocessor.cpp"
#define UPLIST_LOG(...)		Bgdl_811363E4(0, UPLIST_FILE, 0, __VA_ARGS__)
#define UPLIST_ERROR(e)							\
	do {								\
		m_result = (e);						\
		UPLIST_LOG("[APP][UPLIST] UpdateList error %x\n", (e));	\
		stop = 1;						\
	} while (0)

UpdateListProcessor::UpdateListProcessor(SystemUpdateList *list)
	: m_recoveryUrl((const char *)NULL, 0), m_recoverySize(-1)
{
	m_list = list;
}

/* Initialize: every element and text forgotten */
int UpdateListProcessor::Bgdl_81133210(void)
{
	UPLIST_LOG("[UPDATE][UPLIST] Initialize\n");
	m_inRecoveryImage = false;
	m_pRcvInfo_array[4] = NULL;
	m_rcvIndex = 0;
	m_pRcvInfo_array[3] = NULL;
	m_pRcvInfo_array[2] = NULL;
	m_pRcvInfo_array[1] = NULL;
	m_pRcvInfo_array[0] = NULL;
	m_state = UPDATE_LIST_STATE_NONE;
	m_result = 0;
	m_regionDone = false;
	m_inText = false;
	m_inImage = false;
	m_inData = false;
	m_inVersion = false;
	m_inRegion = false;
	m_inRoot = false;
	m_list_data = NULL;
	m_list_updata = NULL;
	m_rcvType.clear();
	m_rcvValue = 0;
	m_url.erase(0, string::npos);
	m_text.erase(0, string::npos);
	return 0;
}

int UpdateListProcessor::Bgdl_8113329E(void)
{
	UPLIST_LOG("[UPDATE][UPLIST] Result %d\n", m_result);
	return m_result;
}

int UpdateListProcessor::startElement(void *userData, const char *name, int length,
				      const bXCe::AttributeList *attrList)
{
	int stop = 0;
	string element(name, length);

	if (m_state == UPDATE_LIST_STATE_NONE) {
		bool match = sce_paf_strcmp(element.c_str(), string("update_data_list").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] <root>\n");
			m_inRoot = true;
			m_state = UPDATE_LIST_STATE_ROOT;
		} else {
			UPLIST_ERROR(UPDATE_LIST_ERROR_NO_ROOT);
		}
	} else switch (m_state) {
	case UPDATE_LIST_STATE_ROOT: {
		bool match = sce_paf_strcmp(element.c_str(), string("region").c_str()) == 0 &&
			     m_regionDone != true;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] <region>\n");

			string id("");
			string att("id");
			const bXCe::Attr *attr =
				(const bXCe::Attr *)attrList->getAttribute("id", att.length());

			if (attr != NULL) {
				string value(attr->getValue(), attr->getValueLength());

				id = value;
				UPLIST_LOG("region %s %s\n", id.c_str(), m_region.c_str());
				if (sce_paf_strcmp(id.c_str(), m_region.c_str()) == 0) {
					UPLIST_LOG("[APP][UPLIST] Detect Region %s\n", m_region.c_str());
					m_inRegion = true;
					m_state = UPDATE_LIST_STATE_REGION;
				}
			} else {
				UPLIST_ERROR(UPDATE_LIST_ERROR_NO_REGION_ID);
			}
		}
		break;
	}
	case UPDATE_LIST_STATE_REGION: {
		bool match = sce_paf_strcmp(element.c_str(), string("version").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] <version>\n");

			string version("");
			string att("system_version");
			const bXCe::Attr *attr =
				(const bXCe::Attr *)attrList->getAttribute("system_version",
									  att.length());

			if (attr != NULL) {
				string value(attr->getValue(), attr->getValueLength());

				version = value;

				string labelAtt("label");
				const bXCe::Attr *label =
					(const bXCe::Attr *)attrList->getAttribute("label",
										  labelAtt.length());

				if (label != NULL) {
					UPLIST_LOG("[APP][UPLIST] UPDATE_LIST_ATT_LABEL\n");
					version = string(label->getValue(), label->getValueLength());
				}

				int number = Bgdl_81135E8C(value);

				if (number == -1) {
					UPLIST_LOG("[APP][UPLIST] Unknown Version fomat\n");
					UPLIST_ERROR(UPDATE_LIST_ERROR_VERSION);
				} else if (m_list_data != NULL) {
					UPLIST_LOG("[APP][UPLIST] m_list_data error\n");
					UPLIST_ERROR(UPDATE_LIST_ERROR_STATE);
				} else {
					m_list_data = new SystemUpdateData;
					m_list_data->type = number;
					m_list_data->version = version;
					m_inVersion = true;
					m_state = UPDATE_LIST_STATE_VERSION;
				}
			} else {
				UPLIST_ERROR(UPDATE_LIST_ERROR_NO_SYSTEM_VERSION);
			}
		} else {
			bool recovery = sce_paf_strcmp(element.c_str(), string("recovery").c_str()) == 0;

			if (recovery) {
				string att("spkg_type");
				const bXCe::Attr *attr =
					(const bXCe::Attr *)attrList->getAttribute("spkg_type",
										  att.length());

				if (attr != NULL) {
					string type(attr->getValue(), attr->getValueLength());

					for (int i = 0; i < UPDATE_LIST_RECOVERY_MAX; i++) {
						if (m_pRcvInfo_array[i] == NULL)
							continue;
						UPLIST_LOG("[APP][UPLIST] m_pRcvInfo_array[%d]\n", i);
						m_rcvType = m_pRcvInfo_array[i]->type;
						UPLIST_LOG("[APP][UPLIST] att=%s in=%s\n", type.c_str(),
							   m_rcvType.c_str());
						if (sce_paf_strcmp(type.c_str(), m_rcvType.c_str()) == 0) {
							m_rcvIndex = i;
							m_state = UPDATE_LIST_STATE_RECOVERY;
						}
					}
				} else {
					UPLIST_ERROR(UPDATE_LIST_ERROR_NO_SPKG_TYPE);
				}
			}
		}
		break;
	}
	case UPDATE_LIST_STATE_VERSION: {
		bool match = sce_paf_strcmp(element.c_str(), string("update_data").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] <data>\n");

			string version("");
			string att("update_type");
			const bXCe::Attr *attr =
				(const bXCe::Attr *)attrList->getAttribute("update_type", att.length());

			if (attr == NULL) {
				/* no type: a full image for any version */
				if (m_list_updata != NULL) {
					UPLIST_LOG("[APP][UPLIST] m_list_updata error\n");
					UPLIST_ERROR(UPDATE_LIST_ERROR_STATE);
				} else {
					m_list_updata = new SystemUpdateImage;
					m_list_updata->type = -1;
					m_list_updata->kind = 0;
					m_inData = true;
					m_state = UPDATE_LIST_STATE_DATA;
				}
			} else {
				string type(attr->getValue(), attr->getValueLength());
				bool known =
					sce_paf_strcmp(type.c_str(), string("full").c_str()) == 0 ||
					sce_paf_strcmp(type.c_str(), string("patch").c_str()) == 0 ||
					sce_paf_strcmp(type.c_str(), string("multiple_patch").c_str()) == 0;

				if (known) {
					int kind = 0;
					bool full = sce_paf_strcmp(type.c_str(), string("full").c_str()) == 0;

					if (!full) {
						bool patch = sce_paf_strcmp(type.c_str(),
									    string("patch").c_str()) == 0;

						kind = patch ? 1 : 2;
					}

					bool ok = false;
					int from;

					if (kind != 0) {
						string fromAtt("current_system_version");
						const bXCe::Attr *fromAttr = (const bXCe::Attr *)
							attrList->getAttribute("current_system_version",
									       fromAtt.length());

						if (fromAttr != NULL) {
							string value(fromAttr->getValue(),
								     fromAttr->getValueLength());

							version = value;
							from = Bgdl_81135E8C(version);
							if (from == -1) {
								UPLIST_LOG("[APP][UPLIST] Unknown Version fomat\n");
								UPLIST_ERROR(UPDATE_LIST_ERROR_FROM_VERSION);
							} else {
								ok = true;
							}
						} else {
							UPLIST_ERROR(UPDATE_LIST_ERROR_NO_FROM_VERSION);
							from = -1;
						}
						if (ok != true)
							goto done;
					} else {
						from = -1;
					}
					if (m_list_updata != NULL) {
						UPLIST_LOG("[APP][UPLIST] m_list_updata error\n");
						UPLIST_ERROR(UPDATE_LIST_ERROR_STATE);
					} else {
						m_list_updata = new SystemUpdateImage;
						m_list_updata->type = from;
						m_list_updata->kind = kind;
						m_inData = true;
						m_state = UPDATE_LIST_STATE_DATA;
					}
				} else {
					UPLIST_ERROR(UPDATE_LIST_ERROR_UPDATE_TYPE);
				}
done:
				;
			}
		}
		break;
	}
	case UPDATE_LIST_STATE_DATA: {
		bool match = sce_paf_strcmp(element.c_str(), string("image").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] <image>\n");

			string att("size");
			const bXCe::Attr *attr =
				(const bXCe::Attr *)attrList->getAttribute("size", att.length());

			if (attr != NULL) {
				string value(attr->getValue(), attr->getValueLength());
				int size = sce_paf_atoi(value.c_str());

				m_list_updata->size = size;
				UPLIST_LOG("[APP][UPLIST] size=%d\n", size);
			}
			m_inImage = true;
			m_url.erase(0, string::npos);
		}
		break;
	}
	case UPDATE_LIST_STATE_RECOVERY: {
		bool match = sce_paf_strcmp(element.c_str(), string("image").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] r<image>\n");

			string att("spkg_version");
			const bXCe::Attr *attr =
				(const bXCe::Attr *)attrList->getAttribute("spkg_version",
									  att.length());

			if (attr != NULL) {
				string version(attr->getValue(), attr->getValueLength());
				string sizeAtt("size");
				const bXCe::Attr *sizeAttr =
					(const bXCe::Attr *)attrList->getAttribute("size",
										  sizeAtt.length());

				if (sizeAttr != NULL) {
					string size(sizeAttr->getValue(), sizeAttr->getValueLength());
					int number = Bgdl_81135E8C(version);

					if (number == -1) {
						UPLIST_LOG("[APP][UPLIST] Unknown Version fomat\n");
						UPLIST_ERROR(UPDATE_LIST_ERROR_FROM_VERSION);
					} else {
						UPLIST_LOG("[APP][UPLIST] ver=%x in=%x\n", number,
							   m_pRcvInfo_array[m_rcvIndex]->version);
						if (number == m_pRcvInfo_array[m_rcvIndex]->version) {
							m_recoveryUrl.clear();

							int value = sce_paf_atoi(size.c_str());

							m_recoverySize = value;
							UPLIST_LOG("[APP][UPLIST] size=%lld\n", (SceInt64)value);
							m_inRecoveryImage = true;
						}
					}
				}
			}
		}
		break;
	}
	default:
		UPLIST_ERROR(UPDATE_LIST_ERROR_STATE);
		break;
	}
	return stop;
}

int UpdateListProcessor::characters(void *userData, const char *ch, int length)
{
	int stop = 0;

	if (m_inImage == true) {
		string text(ch, length);

		if (m_url.length() + length > UPDATE_LIST_TEXT_MAX)
			UPLIST_ERROR(UPDATE_LIST_ERROR_TEXT_LENGTH);
		else
			m_url.append(text);
	}
	if (m_inRecoveryImage == true) {
		string text(ch, length);

		if (m_recoveryUrl.length() + length > UPDATE_LIST_TEXT_MAX)
			UPLIST_ERROR(UPDATE_LIST_ERROR_TEXT_LENGTH);
		else
			m_recoveryUrl.append(text);
	}
	if (m_inText == true) {
		string text(ch, length);

		if (m_text.length() + length > UPDATE_LIST_TEXT_MAX)
			UPLIST_ERROR(UPDATE_LIST_ERROR_TEXT_LENGTH);
		else
			m_text.append(text);
	}
	return stop;
}

int UpdateListProcessor::endElement(void *userData, const char *name, int length)
{
	int stop = 0;
	string element(name, length);

	if (m_state == UPDATE_LIST_STATE_NONE) {
		sce_paf_strcmp(element.c_str(), string("update_data_list").c_str());
	} else switch (m_state) {
	case UPDATE_LIST_STATE_ROOT: {
		bool match = sce_paf_strcmp(element.c_str(), string("update_data_list").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] </root>\n");
			m_state = UPDATE_LIST_STATE_NONE;
			m_inRoot = false;
			if (m_list->url.length() == 0)
				UPLIST_ERROR(UPDATE_LIST_ERROR_NO_REGION);
		}
		break;
	}
	case UPDATE_LIST_STATE_REGION: {
		bool match = sce_paf_strcmp(element.c_str(), string("region").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] </region>\n");
			if (m_inRegion == true)
				m_list->url = m_region;
			m_inRegion = false;
			m_state = UPDATE_LIST_STATE_ROOT;
			if (m_list->list.size() != 0)
				m_regionDone = true;
			else
				UPLIST_ERROR(UPDATE_LIST_ERROR_NO_VERSION);
		}
		break;
	}
	case UPDATE_LIST_STATE_VERSION: {
		bool match = sce_paf_strcmp(element.c_str(), string("version").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] </version>\n");
			if (m_inVersion == true) {
				if (m_list_data != NULL) {
					if (m_list_data->images.size() != 0) {
						if (m_list->list.size() >= UPDATE_LIST_VERSION_MAX) {
							UPLIST_LOG("[APP][UPLIST] Version num error\n");
							UPLIST_ERROR(UPDATE_LIST_ERROR_VERSION_COUNT);
						} else {
							m_list->list.push_back(*m_list_data);
							UPLIST_LOG("[APP][UPLIST] push_back version %#x\n",
								   m_list_data->type);
						}
					} else {
						UPLIST_ERROR(UPDATE_LIST_ERROR_NO_DATA);
					}
					if (m_list_data != NULL)
						delete m_list_data;
					m_list_data = NULL;
				} else {
					UPLIST_LOG("[APP][UPLIST] critical error\n");
					UPLIST_ERROR(UPDATE_LIST_ERROR_STATE);
				}
			}
			m_inVersion = false;
			m_state = UPDATE_LIST_STATE_REGION;
		}
		break;
	}
	case UPDATE_LIST_STATE_DATA: {
		bool match = sce_paf_strcmp(element.c_str(), string("image").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] </image>\n");
			if (m_inImage == true) {
				/* the url without its line breaks */
				string url(m_url);
				string::size_type urlLength = url.length();
				string::size_type at;

				do {
					at = url.find_first_of("\n\r");
					if (urlLength > at && at != string::npos) {
						url.erase(at, 1);
						UPLIST_LOG("[APP][UPLIST] url in cr code.(adjust:%s)\n",
							   url.c_str());
					}
				} while (at != string::npos);
				m_list_updata->url = url;
			}
			m_url.erase(0, string::npos);
			m_inImage = false;
		} else {
			bool data = sce_paf_strcmp(element.c_str(), string("update_data").c_str()) == 0;

			if (data) {
				UPLIST_LOG("[APP][UPLIST] </data>\n");
				if (m_inData == true) {
					if (m_list_updata != NULL) {
						if (m_list_data->images.size() >= UPDATE_LIST_DATA_MAX) {
							UPLIST_LOG("[APP][UPLIST] Data num error\n");
							UPLIST_ERROR(UPDATE_LIST_ERROR_DATA_COUNT);
						} else {
							m_list_data->images.push_back(*m_list_updata);
							UPLIST_LOG("[APP][UPLIST]LIST push_back data\n");
						}
						if (m_list_updata != NULL)
							delete m_list_updata;
						m_list_updata = NULL;
					} else {
						UPLIST_ERROR(UPDATE_LIST_ERROR_STATE);
					}
				}
				m_inData = false;
				m_state = UPDATE_LIST_STATE_VERSION;
			}
		}
		break;
	}
	case UPDATE_LIST_STATE_RECOVERY: {
		bool match = sce_paf_strcmp(element.c_str(), string("image").c_str()) == 0;

		if (match) {
			UPLIST_LOG("[APP][UPLIST] r</image>\n");
			if (m_inRecoveryImage == true) {
				UPLIST_LOG("[APP][UPLIST] *recovery url\n");
				m_pRcvInfo_array[m_rcvIndex]->url = m_recoveryUrl;
				m_pRcvInfo_array[m_rcvIndex]->size = m_recoverySize;
			}
			m_recoveryUrl.erase(0, string::npos);
			m_inRecoveryImage = false;
		} else {
			bool recovery = sce_paf_strcmp(element.c_str(), string("recovery").c_str()) == 0;

			if (recovery) {
				UPLIST_LOG("[APP][UPLIST] </recovery>\n");
				m_state = UPDATE_LIST_STATE_REGION;
			}
		}
		break;
	}
	default:
		UPLIST_ERROR(UPDATE_LIST_ERROR_STATE);
		break;
	}
	return stop;
}

/* an entity's text read as the element's text */
int UpdateListProcessor::entityData(void *userData, int entityType, const char *name, int length)
{
	characters(userData, name, length);
	return 0;
}

UpdateListReader::UpdateListReader(SystemUpdateList *list, SceSize max) : UpdateListProcessor(list)
{
}

/* the region whose entries are read */
inline void UpdateListReader::setRegion(string region)
{
	UPLIST_LOG("[APP][UPLIST] setRegion %s\n", region.c_str());
	m_region.clear();
	m_region = region;
}

/* Initialize, and the console's CDN region set as the one read */
int UpdateListReader::Bgdl_81134D70(void)
{
	int ret = Bgdl_81133210();

	if (ret == 0) {
		string cdn;

		ret = Bgdl_811361D4(&cdn);
		if (ret >= 0) {
			UPLIST_LOG("[UPDATE][UPLIST] cdn region = %s\n", cdn.c_str());

			setRegion(cdn);
		}
	}
	return ret;
}
